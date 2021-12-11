#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <rtthread.h>
#include <rtthread.h>
#include <wlan_dev.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include "webclient.h"
#include "video_transfer.h"
#include "rtp.h"
#include "samples_config.h"

#ifdef RTSP_CAMERA_SAMPLE

#define SERVER_RTSP_PORT        7070
#define SERVER_RTP_PORT         55532
#define SERVER_RTCP_PORT        55533
#define RTSP_RECIVE_BUFSZ       1024
#define CAMERA_FPS              10
#define CAMERA_MAX_BUF_SIZE     40*1024
#define IMG_WIDTH               640
#define IMG_HEIGHT              480

static rt_thread_t rtsp_tid = RT_NULL;
static CAMERA_PARAMTER_T *camera_t=NULL;
static RTP_PARAMTER_T *rtp_t=NULL;
extern int video_buffer_open (void);
extern int video_buffer_close (void);

static void softap_start(char * ssid,char* paasword)
{
    struct rt_wlan_info info;
    struct rt_wlan_device *wlan;
    wlan = (struct rt_wlan_device *)rt_device_find(WIFI_DEVICE_AP_NAME);
    rt_err_t result = RT_EOK;

    if(!paasword)
        rt_wlan_info_init(&info, WIFI_AP, SECURITY_OPEN, ssid);
    else
        rt_wlan_info_init(&info, WIFI_AP, SECURITY_WPA2_AES_PSK, ssid);
      
    info.channel = 11;
    rt_wlan_init(wlan, WIFI_AP);
    /* start soft ap */
    rt_wlan_softap(wlan, &info, paasword);
    rt_wlan_info_deinit(&info);
}

static void rtp_camera_start()
{
    video_buffer_open();
}

static void rtp_camera_stop()
{
    video_buffer_close();   
}

static char const *DateHeader()
{
    static char buf[200];
    time_t tt = time(NULL);
    strftime(buf, sizeof buf, "Date: %a, %b %d %Y %H:%M:%S GMT", gmtime(&tt));
    return buf;
}

static int rtsp_strncasecmp(const char *a, const char *b, size_t n)
{
    uint8_t c1, c2;
    if (n <= 0)
        return 0;
    do {
        c1 = tolower(*a++);
        c2 = tolower(*b++);
    } while (--n && c1 && c1 == c2);
    return c1 - c2;
}

static const char *rtsp_strstri(const char* str, const char* subStr)
{
    int len = strlen(subStr);

    if(len == 0)
    {
        return RT_NULL;
    }

    while(*str)
    {
        if(rtsp_strncasecmp(str, subStr, len) == 0)
        {
            return str;
        }
        ++str;
    }
    return RT_NULL;
}
static char* getLineFromBuf(char* buf, char* line)
{
    while(*buf != '\n')
    {
        *line = *buf;
        line++;
        buf++;
    }

    *line = '\n';
    ++line;
    *line = '\0';

    ++buf;
    return buf; 
}

static void rtsp_header_fields_get(char *resp_buf,const char *fields,char *outline)
{
    size_t resp_buf_len = strlen(resp_buf);

    char *mime_ptr = RT_NULL;
    mime_ptr=(char *)rtsp_strstri(resp_buf, fields);
    if (mime_ptr)
    {
        while(*mime_ptr != '\n')
        {
           *outline = *mime_ptr;
           outline++; 
           mime_ptr++;
        }
        *outline = '\n';
        ++outline;
        *outline = '\0';
    }

}
static int handleCmd_OPTIONS(char* result, int cseq)
{
    sprintf(result, "RTSP/1.0 200 OK\r\n"
                    "CSeq: %d\r\n"
                    "Public: OPTIONS, DESCRIBE, SETUP, PLAY,PAUSE,TEARDOWN\r\n"
                    "\r\n",
                    cseq);
                
    return 0;
}

static int handleCmd_DESCRIBE(char* result, int cseq, char* url)
{
    char sdp[256];
    char localIp[32];

    sscanf(url, "rtsp://%[^:]:", localIp);

    sprintf(sdp, "v=0\r\n"
                 "o=- 9%ld 1 IN IP4 %s\r\n"
                 "s=\r\n"
                 "t=0 0\r\n"
                 "m=video 0 RTP/AVP 26\r\n"
                 "c=IN IP4 0.0.0.0\r\n",
                 time(NULL), localIp);
    
    sprintf(result, "RTSP/1.0 200 OK\r\n"
                    "CSeq: %d\r\n"
                    "%s\r\n"
                    "Content-Base: %s\r\n"
                    "Content-type: application/sdp\r\n"
                    "Content-length: %d\r\n\r\n"
                    "%s",
                    cseq,
                    DateHeader(),
                    url,
                    strlen(sdp),
                    sdp);
    
    return 0;
}

static int handleCmd_SETUP(char* result, int cseq, int clientRtpPort)
{
    sprintf(result, "RTSP/1.0 200 OK\r\n"
                    "CSeq: %d\r\n"
                    "%s\r\n"
                    "Transport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d\r\n"
                    "Session: 66334873\r\n"
                    "\r\n",
                    cseq,
                    DateHeader(),
                    clientRtpPort,
                    clientRtpPort+1,
                    SERVER_RTP_PORT,
                    SERVER_RTCP_PORT);
    
    return 0;
}

static int handleCmd_PLAY(char* result, int cseq)
{
    sprintf(result, "RTSP/1.0 200 OK\r\n"
                    "CSeq: %d\r\n"
                    "%s\r\n"
                    "Range: npt=0.000-\r\n"
                    "Session: 66334873\r\n\r\n",
                    cseq,
                    DateHeader());
    
    return 0;
}

static int handleCmd_PAUSE_OR_TEARDOWN(char* result, int cseq)
{
    sprintf(result, "RTSP/1.0 200 OK\r\n"
                    "CSeq: %d\r\n"
                    "Session: 66334873\r\n\r\n",
                    cseq);
    
    return 0;
}

static void parse_rtsp_request(int clientSockfd, const char* clientIP)
{
    char method[16];
    char url[32];
    char version[16];
    char line[128];
    int cseq;
    char *bufPtr;
    char* rBuf = malloc(RTSP_RECIVE_BUFSZ);
    char* sBuf = malloc(RTSP_RECIVE_BUFSZ);
    int clientRtpPort,clientRtcpPort;
    while(1)
    {
        int recvLen;
        recvLen = recv(clientSockfd, rBuf, RTSP_RECIVE_BUFSZ, 0);
        if(recvLen <= 0)
            goto out;

        rBuf[recvLen] = '\0';
        rt_kprintf("---------------C->S--------------\n");
        printf("%s", rBuf);

        /* 解析方法 */
        bufPtr = getLineFromBuf(rBuf, line);
        if(sscanf(line, "%s %s %s\r\n", method, url, version) != 3)
        {
            rt_kprintf("cmd parse err\n");
            goto out;
        }

        rtsp_header_fields_get(rBuf,"CSeq",line);
        /* 解析序列号 */
        if(sscanf(line, "CSeq: %d\r\n", &cseq) != 1)
        {
            goto out;
        }

        /* 如果是SETUP，那么就再解析client_port */
        if(!strcmp(method, "SETUP"))
        {
            rtsp_header_fields_get(rBuf,"client_port=",line);
            sscanf(line, "client_port=%d-%d\r\n",&clientRtpPort,&clientRtcpPort);
        }

        if(!strcmp(method, "OPTIONS"))
        {
            if(handleCmd_OPTIONS(sBuf, cseq))
            {
                rt_kprintf("failed to handle options\n");
                goto out;
            }
        }
        else if(!strcmp(method, "DESCRIBE"))
        {
            if(handleCmd_DESCRIBE(sBuf, cseq, url))
            {
                rt_kprintf("failed to handle describe\n");
                goto out;
            }
        }
        else if(!strcmp(method, "SETUP"))
        {
            if(handleCmd_SETUP(sBuf, cseq, clientRtpPort))
            {
                rt_kprintf("failed to handle setup\n");
                goto out;
            }
        }
        else if(!strcmp(method, "PLAY"))
        {
            if(handleCmd_PLAY(sBuf, cseq))
            {
                rt_kprintf("failed to handle play\n");
                goto out;
            }
        }
        else if(!strcmp(method, "PAUSE"))
        {
            if(handleCmd_PAUSE_OR_TEARDOWN(sBuf, cseq))
            {
                rt_kprintf("failed to handle pause\n");
                goto out;
            }
            rtp_server_pause(); 
        }
        else if(!strcmp(method, "TEARDOWN"))
        {
            if(handleCmd_PAUSE_OR_TEARDOWN(sBuf, cseq))
            {
                rt_kprintf("failed to handle teardown\n");
            }
            goto out;
        }
        else
        {
            rt_kprintf("=======unkown cmd===========\n");
            goto out;
        }

        rt_kprintf("---------------S->C--------------\n");
        printf("%s", sBuf);
        send(clientSockfd, sBuf, strlen(sBuf), 0);

        /* 开始播放，发送RTP线程 */
        if(!strcmp(method, "PLAY"))
        {
            rtp_server_start(clientIP,clientRtpPort);
        }

    }
out:
    rt_kprintf("rtsp server finish\n");
    rtp_server_stop();
    closesocket(clientSockfd);
    if(rtp_t->clientIP)
    {
        free(rtp_t->clientIP);
        rtp_t->clientIP=NULL;
    }
    free(rBuf);
    free(sBuf);
}


static void rtsp_server_main_thread(void *param)
{
    int rtsp_serverSockfd;
    struct sockaddr_in server_addr;

    rtsp_serverSockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(rtsp_serverSockfd < 0)
    {
        rt_kprintf("failed to create tcp socket\n");
        return;
    }

    /* 初始化服务端地址 */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_RTSP_PORT); /* 服务端工作的端口 */
    server_addr.sin_addr.s_addr = INADDR_ANY;
    rt_memset(&(server_addr.sin_zero), 0, sizeof(server_addr.sin_zero));

    if (bind(rtsp_serverSockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)
    {
        rt_kprintf("Unable to bind\n");
        goto exit;
    }

    if (listen(rtsp_serverSockfd, 5) == -1)
    {
        rt_kprintf("Listen error\n");
        goto exit;
    }

    rt_kprintf("rtsp://%s:%d\n", DHCPD_SERVER_IP,SERVER_RTSP_PORT);

    while(1)
    {
        int clientSockfd;
        struct sockaddr_in client_addr;
        socklen_t sin_size;
        sin_size=sizeof(struct sockaddr_in);;

        clientSockfd  = accept(rtsp_serverSockfd, (struct sockaddr *)&client_addr, &sin_size);
        if(clientSockfd < 0 || RTP_START==get_rtp_server_state())
        {
            rt_kprintf("failed to accept client\n");
            continue;
        }

        rt_kprintf("I got a connection from (%s,%d)\n",inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        parse_rtsp_request(clientSockfd, inet_ntoa(client_addr.sin_addr));

    }

exit:
    rtp_camera_stop();
    rtp_server_deinit();
    closesocket(rtsp_serverSockfd);

    if(camera_t->img_buf)
    {
        free(camera_t->img_buf);
        camera_t->img_buf=NULL;
    }
    if(camera_t)
    {
        free(camera_t);
        camera_t=NULL;
    }
    if(rtp_t->clientIP)
    {
        free(rtp_t->clientIP);
        rtp_t->clientIP=NULL;  
    }
    if(rtp_t)
    {
        free(rtp_t);
        rtp_t=NULL; 
    }

    rtsp_tid = RT_NULL;
}

static void rtsp_server_main()
{

    if (rtsp_tid)
    {
        rt_kprintf("rtsp server thread already init.\n");
        return;
    }

    camera_t=(CAMERA_PARAMTER_T *)malloc(sizeof(CAMERA_PARAMTER_T));
    rtp_t=(RTP_PARAMTER_T *)malloc(sizeof(RTP_PARAMTER_T));

    camera_t->img_max_size=CAMERA_MAX_BUF_SIZE;
    camera_t->img_width=IMG_WIDTH;
    camera_t->img_hight =IMG_HEIGHT;
    camera_t->camera_fps =CAMERA_FPS;
    camera_t->img_buf = (uint8_t *) malloc (camera_t->img_max_size); 

    rtp_t->state=RTP_IDLE;
    rtp_t->serverRtpPort=SERVER_RTP_PORT;

    softap_start("BK_RTSP_CAMERA",NULL);
    rtp_server_init(camera_t,rtp_t);
    rtp_camera_start();
    rtsp_tid = rt_thread_create("rtsp_server_thread",
                           rtsp_server_main_thread,
                           RT_NULL,
                           1024*2,
                           20,
                           10);

    if (rtsp_tid != NULL)
    {
        rt_thread_startup(rtsp_tid);
    }

}

MSH_CMD_EXPORT(rtsp_server_main,rtsp_server_main);

#endif