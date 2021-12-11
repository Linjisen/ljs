#include <rtthread.h>
#include "codec_helixmp3.h" 
#include "player_init.h"
#include "player.h"
#include "test_config.h"
#include "drivers/board.h"
#include <finsh.h>
#include <stdio.h>
#include <stdlib.h>
#include "dfs_posix.h"
#include "player.h"
#include "gpio_pub.h"
#include "rtos_pub.h"
#include "sys_ctrl_pub.h"
#include "saradc_intf.h"

#ifdef CHECK_SD_PLAY_TEST

enum{
	OFFLINE_PLAYER_PLAY = (0x1 << (PLYAER_STATE_CHANGED+1)),
	OFFLINE_PLAYER_PAUSE = (0x1 << (PLYAER_STATE_CHANGED+2)),
	OFFLINE_PLAYER_STOP = (0x1 << (PLYAER_STATE_CHANGED+3)),
	OFFLINE_PLAYER_SONG_NEXT = (0x1 << (PLYAER_STATE_CHANGED+4)),
	OFFLINE_PLAYER_SONG_PREV = (0x1 << (PLYAER_STATE_CHANGED+5)),
	OFFLINE_PLAYER_SD_INSTER_EVENT = (0x1 << (PLYAER_STATE_CHANGED+6)),
	OFFLINE_PLAYER_SD_REMOVE_EVENT = (0x1 << (PLYAER_STATE_CHANGED+7)),
	OFFLINE_PLAYER_POSITION = (0x1 << (PLYAER_STATE_CHANGED+8)),
	OFFLINE_PLAYER_ALL_EVENT = 0x1FFFF,
};


#define	SD_ROOT "/sd"
/*max song directory*/
#define	MAX_SONG_DIR	64
/*max directory level*/
#define	MAX_DIR_LEVEL	8

#define MAX_LFN_LEN	256

typedef enum {
	GET_PREV,
	GET_NEXT
}PLAY_DIRECTION;


typedef struct _SONG_DIR
{
	char *path;
	unsigned short dir_music_num;
}SONG_DIR;


typedef struct _PLAY_INFO_
{
	SONG_DIR dir_info[MAX_SONG_DIR];
	char cur_file_path[MAX_LFN_LEN];
	unsigned short music_num;
	unsigned short music_index;
	unsigned short dir_num;
	unsigned short dir_index;
	
	unsigned char init_done : 1;
	unsigned char player_status : 1;
	unsigned char player_pause_flag : 1;
	unsigned char vol_step : 5;
	unsigned char vol;
}PLAY_INFO;


PLAY_INFO _sd_player = {0};
static 	struct rt_event offlineplay_evt;

////////////SD check//////////////////
typedef enum{
	OFFLINE_STATUS = 0x00,
	ONLINE_STATUS,
}CHECK_STATUS_E;

#define SD_STATUS_CHECK_PIN				12 
#define SD_DEBOUNCE_COUNT 			    10
#define SD_INSTER_STATUS_CHECK_PIN_LEVEL 0
#define SD_CHECK_SCAN_INTERVAL_MS		 20
CHECK_STATUS_E sd_status = OFFLINE_STATUS;
static beken_timer_t sd_check_handle_timer = {0};


/*
idx: song index in current dir ,start from 0
path: current dir full path name
fullname:the searched song's full path name(path name + filename)
*/
static int  read_file_in_dir(unsigned int idx,char *path,char * fullname)
{
	struct dirent  *ent  = NULL;
	DIR *pDir = NULL;
	unsigned short cur_idx = 0;
	int ret = -1;
	
	pDir = opendir(path);
	if(NULL != pDir)
	{
		while(1)
		{
			ent = readdir(pDir);
			if(NULL == ent)
				break;
			if(ent->d_type & DT_REG)
			{
				if( (0 == strncasecmp(strchr(ent->d_name,'.'),".mp3",4)) ||
				    (0 == strncasecmp(strchr(ent->d_name,'.'),".wav",4)) )
				{
					if( cur_idx == idx )
					{
						ret = 0;
						snprintf(fullname, MAX_LFN_LEN,"%s%s%s",path,"/",ent->d_name);						
						break;
					}
					cur_idx ++;
				}
			}
		}
		closedir(pDir);
	}
	return ret;
}

static int get_song(PLAY_DIRECTION prev_next)
{
	int i = 0;
	int ret = -1;
	unsigned short total = 0;
	if(_sd_player.music_num == 0)
		return ret;
	
	if(GET_PREV == prev_next)
	{//prev
		if(0 == _sd_player.music_index)
			_sd_player.music_index = _sd_player.music_num - 1;
		else
			_sd_player.music_index --;
	}
	else
	{//next
		if(_sd_player.music_index >= _sd_player.music_num - 1)
			_sd_player.music_index = 0;
		else
			_sd_player.music_index ++;
	}

	if(_sd_player.music_index != 0)
	{
		for(i=0;i<sizeof(_sd_player.dir_info)/sizeof(SONG_DIR);i++)
		{
			total += _sd_player.dir_info[i].dir_music_num;
			if(_sd_player.music_index <= total-1)
				break;		
		}
		total -= _sd_player.dir_info[i].dir_music_num;
	}
	memset(_sd_player.cur_file_path,0,sizeof(_sd_player.cur_file_path));
	ret = read_file_in_dir(_sd_player.music_index-total,_sd_player.dir_info[i].path,_sd_player.cur_file_path);
	printf("@@@@@ full name:%s @@@@@\r\n",_sd_player.cur_file_path);
	
	return ret;
}

/*
for directory play mode
find directory and open the first mp3 file 
*/
static int get_dir(PLAY_DIRECTION prev_next)
{
	int i = 0;
	int ret = -1;
	
	if(_sd_player.dir_num == 0)
		return ret;

	if(GET_PREV == prev_next)
	{
		if(0 == _sd_player.dir_index)
			_sd_player.dir_index = _sd_player.dir_num - 1;
		else
			_sd_player.dir_index --;
	}
	else
	{
		if(_sd_player.dir_index >= _sd_player.dir_num -1)
			_sd_player.dir_index = 0;
		else
			_sd_player.dir_index ++;
	}
	_sd_player.music_index = 0;
	
	for(i=0;i<_sd_player.dir_index;i++)
		_sd_player.music_index +=  _sd_player.dir_info[_sd_player.dir_index].dir_music_num;

	memset(_sd_player.cur_file_path,0,sizeof(_sd_player.cur_file_path));
	if(_sd_player.music_index != 0)
	{
		_sd_player.music_index -= 1;
		ret = read_file_in_dir(0,_sd_player.dir_info[_sd_player.dir_index].path,_sd_player.cur_file_path);
	}
	return ret;
}

/*
full disk scan: get mp3 total number and DIRs' pathname which have .mp3 files
*/
static void scan_files(char *path,rt_uint8_t recu_level)
{
    struct dirent  *ent  = NULL;
	DIR *pDir = NULL;
	short filecnt = 0;
	rt_uint8_t tmp;

	pDir = opendir(path);
	if(NULL != pDir)
	{
		tmp = strlen(path);
		while(1)
		{
			ent = readdir(pDir);
			if(NULL == ent)
				break;

			if( (0 == strcmp(ent->d_name,".")) || (0 == strcmp(ent->d_name,"..")) )
                continue;

			if(ent->d_type & DT_DIR)
			{
				if(recu_level < MAX_DIR_LEVEL)
				{
					snprintf(&path[tmp],strlen(ent->d_name)+1+1,"/%s",ent->d_name);
					recu_level++;
					scan_files(path,recu_level);
					path[tmp] = 0;
				}
				else
					break;
			}
			else
			{
				if((0 == strncasecmp(strchr(ent->d_name,'.'),".mp3",4))||
				   (0 == strncasecmp(strchr(ent->d_name,'.'),".wav",4)))
				{
					filecnt ++;
				}
			}
		}
		if(filecnt > 0)
		{
			if(_sd_player.dir_num < MAX_SONG_DIR -1)
			{
				_sd_player.dir_info[_sd_player.dir_num].path = rt_malloc(strlen(path)+1);
				memset(_sd_player.dir_info[_sd_player.dir_num].path,0,strlen(path)+1);
				snprintf(_sd_player.dir_info[_sd_player.dir_num].path,strlen(path)+1,"%s",path);
			
				_sd_player.dir_info[_sd_player.dir_num].dir_music_num = filecnt;
				_sd_player.music_num += filecnt;
				_sd_player.dir_num++;
			} 
		}
		closedir(pDir);
	}
}

static int scan_sd(void)
{
	int i;
	char *path = sdram_calloc(1,256);

	if(NULL == path)
	{
		rt_kprintf("scan malloc error\r\n");
		return -1;
	}
	snprintf(path,strlen(SD_ROOT)+1,"%s",SD_ROOT);
	scan_files(path,0);
	for(i=0;i<_sd_player.dir_num;i++)
	{
		rt_kprintf("====path[%d]:%s===\r\n",i,_sd_player.dir_info[i].path);
	}
	sdram_free(path);
	if(_sd_player.dir_num > 0)
		return 0;
	else
		return -1;
}
static int mount_sd(void)
{
	int ret;
	/* mount sd card fat partition 1 as root directory */
	ret = dfs_mount("sd0", SD_ROOT, "elm", 0, 0);
	if(ret == 0)
		rt_kprintf("SD Card initialized!\n");
	else
		rt_kprintf("SD Card initialzation failed!\n");
	return ret;
}

static void unmount_sd(void)
{
	int ret,i;
	ret = dfs_unmount(SD_ROOT);
	rt_kprintf("unmount sd :ret =%d\r\n",ret);
	for(i=0;i<_sd_player.dir_num;i++)
	{
		if(_sd_player.dir_info[i].path != NULL)
		{
			rt_free(_sd_player.dir_info[i].path);
			_sd_player.dir_info[i].path = NULL;
			_sd_player.dir_info[i].dir_music_num = 0;
		}
	}
	_sd_player.music_num = 0;
	_sd_player.music_index = 0;
	_sd_player.dir_num = 0;
	_sd_player.dir_index = 0;
}


static int offline_player_send_event(int event)
{	
	int ret;
	if(_sd_player.init_done)
	{
		ret=rt_event_send(&offlineplay_evt,event);
		if(ret==RT_EOK)
			rt_kprintf("rt_event_send OK\r\n");	
	}
	return ret;
}	

static void play_music_callback(int event, void *user_data)
{
/*
/////////////////////////////////////////////////////////////////////////////////
NOTE: any function in player.h is forbidden to be called in the callback funtion
\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\ 
*/
    switch(event)
    {
	    case PLAYER_AUDIO_CLOSED:
	       	offline_player_send_event(1 << PLAYER_AUDIO_CLOSED);
	        break;
	    case PLAYER_PLAYBACK_FAILED:
	        offline_player_send_event(1 << PLAYER_PLAYBACK_FAILED);
	        break;
	    case PLAYER_PLAYBACK_BREAK:
	      	offline_player_send_event(1 << PLAYER_PLAYBACK_BREAK);
	        break;   
		default:
			rt_kprintf("recv event :%d\r\n",event);
			break;
    }
}

static void sd_play_song(PLAY_DIRECTION prev_next)
{
	player_stop();
	get_song(prev_next);
	player_set_uri(_sd_player.cur_file_path);
	player_play();
}
static void sd_play_thread(void *param)
{	
	uint32_t recv_evt;
	int ret;
    rt_err_t result;
    int event;
	rt_thread_delay(500);
    while(1)
    {
		ret = rt_event_recv(&offlineplay_evt,OFFLINE_PLAYER_ALL_EVENT,
								RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,RT_WAITING_FOREVER, &recv_evt);

		if(ret == RT_EOK)
		{
			rt_kprintf("recv_evt:%x\n",recv_evt);
			
			if(recv_evt & OFFLINE_PLAYER_SD_INSTER_EVENT)
			{
				
				if(0 == mount_sd())
				{
					rt_kprintf("---sd mount ok---\r\n");
					if(0 == scan_sd())
					{
					//first song
						_sd_player.music_index = _sd_player.music_num;
						sd_play_song(GET_NEXT);
					}
					else
					{
						rt_kprintf("---sd no song---\r\n");
					}
						
				}
				else
					rt_kprintf("---sd mount fail---\r\n");
			}
			if( recv_evt & OFFLINE_PLAYER_SD_REMOVE_EVENT )
			{
				rt_kprintf("---sd unmount---\r\n");
				player_stop();
				unmount_sd();
			}
			if( recv_evt &( 1 << PLAYER_AUDIO_CLOSED) )
			{ 
				sd_play_song(GET_NEXT);
				
			}
			if(recv_evt & OFFLINE_PLAYER_PLAY)
			{
				player_play();
			}
			if(recv_evt & OFFLINE_PLAYER_PAUSE)
			{
				player_pause();
			}
			if(recv_evt & OFFLINE_PLAYER_STOP)
			{
				player_stop();
			}
			if(recv_evt & OFFLINE_PLAYER_SONG_NEXT)
			{
				sd_play_song(GET_NEXT);
			}
			if(recv_evt & OFFLINE_PLAYER_SONG_PREV)
			{
				sd_play_song(GET_PREV);
			}
			if(recv_evt & OFFLINE_PLAYER_POSITION)
			{
				rt_kprintf("total time:%d s,cur pos:%d ms\r\n",player_get_duration(),player_get_position());	
			}
		}
		else
			rt_kprintf("recv_evt failed\r\n");
    }
}

static uint8_t get_sd_check_pin_status(void)
{
	return gpio_input(SD_STATUS_CHECK_PIN);
}

static void sd_check_handle_timer_callback( void * arg )  
{
	static uint16 cnt_online = 0;	
	if(get_sd_check_pin_status() == SD_INSTER_STATUS_CHECK_PIN_LEVEL)
	{				
		if(OFFLINE_STATUS == sd_status)
		{ 
			rt_kprintf("sd_status=%d\r\n",sd_status);
			if (cnt_online < SD_DEBOUNCE_COUNT)
	        {
	            cnt_online ++;			
	        }		 
			else
			{
				sd_status = ONLINE_STATUS;
				rt_kprintf(" sd_status = ONLINE_STATUS\r\n");
				offline_player_send_event(OFFLINE_PLAYER_SD_INSTER_EVENT);
	 		}
		}
	}
	else 
	{
		if(ONLINE_STATUS == sd_status)
		{ 
			cnt_online = 0;
			sd_status = OFFLINE_STATUS;
			rt_kprintf(" sd_status = OFFLINE_STATUS\r\n");
			offline_player_send_event(OFFLINE_PLAYER_SD_REMOVE_EVENT);		
		}
	}
}

static int sd_is_online(void)
{
	return (sd_status == ONLINE_STATUS)?1:0;
}

static void sd_check_init(void)
{
	int  err;	
	gpio_config(SD_STATUS_CHECK_PIN, GMODE_INPUT_PULLUP);
	err = rtos_init_timer(&sd_check_handle_timer, 
							SD_CHECK_SCAN_INTERVAL_MS, 
							sd_check_handle_timer_callback, 
							NULL);
	err = rtos_start_timer(&sd_check_handle_timer);
}

static void sd_play_test(int argc, char **argv)
{
    rt_thread_t tid;
	int event;
	
	if(argc == 2)
	{
		if(0 == strcmp(argv[1],"start"))
		{
			if(_sd_player.init_done)
				return;
			saradc_config_vddram_voltage(PSRAM_VDD_3_3V);
			sd_check_init();
			memset(&_sd_player,0,sizeof(_sd_player));
		    player_set_volume(50);
		    player_set_event_callback(play_music_callback, RT_NULL); 
			rt_event_init(&offlineplay_evt, "evt", RT_IPC_FLAG_FIFO);

		    tid = rt_thread_create("sd_play", sd_play_thread, RT_NULL, 1024 * 4, 10, 10);
		    if (RT_NULL != tid)
		    {
		        rt_thread_startup(tid);
				_sd_player.init_done= 1;
		    }
		}
		else
		{
			if(_sd_player.init_done)
			{
				if(0 == strcmp(argv[1],"next"))
				{
					event =OFFLINE_PLAYER_SONG_NEXT;
				}
				else if(0 == strcmp(argv[1],"prev"))
				{
					event =OFFLINE_PLAYER_SONG_PREV;
				}
				else if(0 == strcmp(argv[1],"play"))
				{
					event =OFFLINE_PLAYER_PLAY;
				}
				else if(0 == strcmp(argv[1],"pause"))
				{
					event = OFFLINE_PLAYER_PAUSE;
				}
				else if(0 == strcmp(argv[1],"stop"))
				{
					event = OFFLINE_PLAYER_STOP;
				}
				else if(0 == strcmp(argv[1],"position"))
				{
					event = OFFLINE_PLAYER_POSITION;
				}
				else
					event = OFFLINE_PLAYER_STOP;
				
				offline_player_send_event(event);
			}
		}
	}
}

MSH_CMD_EXPORT(sd_play_test, sd_play_test);
#endif
