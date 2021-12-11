#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <rtthread.h>
#include <finsh.h>
#include <cJSON.h>
#include "webclient.h"
#include "wantong_httpclient.h"
#include "vt_types.h"
#include <fal.h>
#include "BkDriverFlash.h"
#include "drv_flash.h"
#include "include.h"
#include <string.h>
#include "typedef.h"
#include "arm_arch.h"
#include "uart_pub.h"
#include "rtos_pub.h"
#include "error.h"
#include "msh.h"
#include "video_transfer.h"
#include "str_pub.h"
#ifdef RT_USING_DFS 
#include <dfs_posix.h>
#endif

#define LOGIN_URI "http://client-api1.51wanxue.com/api-client/login/license?sdkVersion=v1.0.0&license=14418E32D4B363D6DE5BC30ABB5E2B02C82BB95BFFDE086146A2C2C5B90522733E61EB2D6B2440C63D23A6DA81337C07C76D97C5892E39CF6118D59627B55091953BC1EE5188945BF6E46A1B6AB959FA&appCode=17DE2E56-C3C3-4C95-A77F-64162880D875"
static LoginResult login_result;
static int last_bookid=4872;
static int last_physicalIndex=-1;
extern int webclient_connect(struct webclient_session *session, const char *URI);
extern int webclient_handle_response(struct webclient_session *session);
extern int webclient_send_header(struct webclient_session *session, int method);
int vt_httpRequest_cb(str_vt_networkPara *param, str_vt_networkRespond *res) 
{
    int ret = 0;
    int size;
    unsigned char *buffer = NULL;
    buffer = (unsigned char *) web_malloc(1024);
    printf("url:%s\r\n",param->url);
    size=wantong_http_login(param->url,&login_result,buffer);
    res->data = buffer;
    res->size = size;
    return ret;
}

void wantong_authentication(void)
{
    unsigned char *buffer = NULL;
    buffer = (unsigned char *) web_malloc(1024);
    wantong_http_login(LOGIN_URI,&login_result,buffer);
    free(buffer);
}

int wantong_http_login(const char *uri,LoginResult *result, unsigned char *buffer)
{
    struct webclient_session* session = RT_NULL;
    int index;
    int bytes_read, resp_status;
    int content_length = -1;

 	cJSON *root = RT_NULL,*item = RT_NULL,*data = RT_NULL;
    int content_pos = 0;
	
    if(NULL != result->token)
    {
        rt_free(result->token);
        result->token = NULL;
    }
    if(NULL != result->verification)
    {
        rt_free(result->verification);
        result->verification = NULL;
    }

    if(NULL != result->Cookie)
    {
        rt_free(result->Cookie);
        result->Cookie = NULL;
    }
	
    //buffer = (unsigned char *) web_malloc(WEBCLIENT_RESPONSE_BUFSZ);
    if (RT_NULL == buffer)
    {
        rt_kprintf("no memory for receive buffer.\r\n");
        goto __exit;

    }

    /* create webclient session and set header response size */
    session = webclient_session_create(WEBCLIENT_HEADER_BUFSZ);
    if (RT_NULL == session )
    {
        goto __exit;
    }

    /* send GET request by default header */
    if ((resp_status = webclient_get(session, uri)) != 200)
    {
        rt_kprintf("webclient GET request failed, response(%d) error.\r\n", resp_status);
        goto __exit;
    }
    
 	if(NULL!=session->Cookie)
    {
        result->Cookie= rt_malloc(strlen(session->Cookie)+1);
        if(result->Cookie== NULL)
        {
          rt_kprintf("malloc cookie failed\r\n"); 
           goto __exit; 
        }
        memset(result->Cookie,0,strlen(session->Cookie)+1);
        memcpy(result->Cookie,session->Cookie,strlen(session->Cookie));
    }
	rt_kprintf("webclient get response data:\r\n");
    content_length = webclient_content_length_get(session);
    if (content_length < 0)
    {
        rt_kprintf("webclient GET request type is chunked.\r\n");
        do
        {
            bytes_read = webclient_read(session, buffer, WEBCLIENT_RESPONSE_BUFSZ);
            if (bytes_read <= 0)
            {
                break;
            }
            for (index = 0; index < bytes_read; index++)
            {
                rt_kprintf("%c", buffer[index]);
            }
            content_pos += bytes_read;
        } while (1);
    }
    else
    {
        do
        {
            bytes_read = webclient_read(session, buffer, 
                    content_length - content_pos > WEBCLIENT_RESPONSE_BUFSZ ?
                            WEBCLIENT_RESPONSE_BUFSZ : content_length - content_pos);
            if (bytes_read <= 0)
            {
                break;
            }
            for (index = 0; index < bytes_read; index++)
            {
                rt_kprintf("%c", buffer[index]);
            }
            content_pos += bytes_read;
        } while (content_pos < content_length);
    }
    rt_kprintf("\r\n");
    root = cJSON_Parse((const char *)buffer);
    if (!root)
    {
        rt_kprintf("No memory for cJSON root!\n");
        goto _json_exit;
    }

    item = cJSON_GetObjectItem(root, "code");
    result->code=item->valueint;
    if(item->valueint==0)
    {
        data = cJSON_GetObjectItem(root, "data");
        item=  cJSON_GetObjectItem(data, "token");
        result->token=web_strdup(item->valuestring);
        item=  cJSON_GetObjectItem(data, "verification");
        result->verification=web_strdup(item->valuestring);
    }

_json_exit:
    if (root != RT_NULL)
        cJSON_Delete(root);
__exit:
    if (session)
    {
        webclient_close(session);
    }

    return content_pos;
}


/* send HTTP POST request by common request interface, it used to receive longer data */
int wantong_post_resquest(RecognitionResult *result,char *imag_buf,int image_size,void (*fn)())
{
    struct webclient_session* session = RT_NULL;
    char *buffer = RT_NULL;
    int index, ret = RT_ERROR;
    int bytes_read = 0, resp_status;
	
    char boundary[60];
    char buffer_ptr2[128];
	char *buffer_ptr;
	char bookid[6];
	int length = image_size;
	int  rc = WEBCLIENT_OK;
	int content_pos = 0;
	const char* url="http://brs-api1.51wanxue.com/api-brs/recognize";
	sprintf(bookid,"%d",last_bookid);

    buffer = (unsigned char *) web_malloc(WEBCLIENT_RESPONSE_BUFSZ);
    if (buffer == RT_NULL)
    {
        rt_kprintf("no memory for receive response buffer.\n");
        ret = -RT_ENOMEM;
        goto __exit;
    }
	memset(buffer,0,WEBCLIENT_RESPONSE_BUFSZ);
    /* create webclient session and set header response size */
    session = webclient_session_create(WEBCLIENT_HEADER_BUFSZ);
    if (session == RT_NULL)
    {
        ret = -RT_ENOMEM;
        goto __exit;
    }

	/* build boundary */
    rt_snprintf(boundary, sizeof(boundary), "----------------------------%012d",rt_tick_get());
    /* build encapsulated mime_multipart information*/
    buffer_ptr = buffer;
    /* first boundary */
    buffer_ptr += rt_snprintf((char*) buffer_ptr,
            WEBCLIENT_RESPONSE_BUFSZ - (buffer_ptr - buffer), "--%s\r\n", boundary);
    buffer_ptr += rt_snprintf((char*) buffer_ptr,WEBCLIENT_RESPONSE_BUFSZ - (buffer_ptr - buffer),
            "Content-Disposition: form-data; name=\"image\";filename=\"%s\"\r\n","image.jpg");
    buffer_ptr += rt_snprintf((char*) buffer_ptr,WEBCLIENT_RESPONSE_BUFSZ - (buffer_ptr - buffer),
			"Content-Type: multipart/form-data\r\n\r\n");
    /* calculate content-length */
    if(bookid!=RT_NULL)
    {
        length +=rt_snprintf(buffer_ptr2, sizeof(buffer_ptr2),"\r\n--%s\r\nContent-Disposition: form-data; name=\"bookId\"\r\n\r\n", boundary);
        length +=strlen(bookid);
    }
    
    length += buffer_ptr - buffer;
    length += strlen(boundary) + 6; /* add the last boundary */
	
	/* build header for upload */
	webclient_header_fields_add(session,"token:%s\r\n", login_result.token);
	webclient_header_fields_add(session,"Cookie:%s\r\n",login_result.Cookie);
	webclient_header_fields_add(session,"Content-Length: %d\r\n", length);
	webclient_header_fields_add(session,"Accept-Encoding: identity\r\n");
	webclient_header_fields_add(session,"Content-Type: multipart/form-data; boundary=%s\r\n", boundary);

   	rc = webclient_connect(session, url);
	if(rc <0)
		goto __exit;
	
    rc = webclient_send_header(session, WEBCLIENT_POST);
    if (rc < 0)
        goto __exit;

    webclient_write(session, buffer, buffer_ptr - buffer);
    webclient_write(session, imag_buf, image_size);
    if(bookid!=RT_NULL)
    {
        webclient_write(session, buffer_ptr2, strlen(buffer_ptr2));
        webclient_write(session, bookid, strlen(bookid));
    }
    /* send last boundary */
    rt_snprintf((char*) buffer, WEBCLIENT_RESPONSE_BUFSZ, "\r\n--%s--\r\n", boundary);
    webclient_write(session, buffer, strlen(boundary) + 8);
    fn();
    webclient_handle_response(session);

#if 1
    memset(buffer,0x0,WEBCLIENT_RESPONSE_BUFSZ);
    do
    {
        bytes_read = webclient_read(session, buffer+bytes_read, WEBCLIENT_RESPONSE_BUFSZ-content_pos); 
        if (bytes_read <= 0)
        {
            break;
        }

        content_pos += bytes_read;
    } while (content_pos < WEBCLIENT_RESPONSE_BUFSZ);
	printf("buffer:%s\r\n", buffer);
#else
	if(session->content_remainder == -1/*0xffffffff*/)
	{
		length = WEBCLIENT_RESPONSE_BUFSZ;
	}
	else
	{
		length = session->content_remainder;
	}
	memset(buffer,0x0,WEBCLIENT_RESPONSE_BUFSZ);
	bytes_read = 0;
    do
    {
        bytes_read = webclient_read(session, buffer + content_pos, length-content_pos); 
		rt_kprintf("--bytes_read:%d\r\n",bytes_read);
        if (bytes_read <= 0)
        {
            break;
        }
        content_pos += bytes_read;
    } while (content_pos < length);
    rt_kprintf("\r\n-----len:%d---\r\n",content_pos);
#endif

    cJSON *root = RT_NULL,*code = RT_NULL,*data = RT_NULL,*brs = RT_NULL,*book = RT_NULL,*physicalIndex=RT_NULL,
    *bookId = RT_NULL,*page = RT_NULL,*audios = RT_NULL,*voice = RT_NULL,*fileName = RT_NULL,
    *loop = RT_NULL,*startAt = RT_NULL,*bgMusic = RT_NULL,*effectSound = RT_NULL,*msg = RT_NULL;  

    root = cJSON_Parse((const char *)buffer);
    if (!root)
    {
        rt_kprintf("No memory for cJSON root!\r\n");
		msh_exec("free",4);
        goto json_exit;
    }

    code = cJSON_GetObjectItem(root, "code");
	if(0 == code->valueint)
    {     
        data = cJSON_GetObjectItem(root, "data");
        brs  = cJSON_GetObjectItem(data, "brs");
        code = cJSON_GetObjectItem(brs, "code");
        result->code=code->valueint;
        if(0 == code->valueint)
        {
            ret  = RT_EOK;
            data = cJSON_GetObjectItem(brs, "data");
            book = cJSON_GetObjectItem(data, "book");
                    
            if(NULL!=book)
            {
                bookId = cJSON_GetObjectItem(book, "bookId");
            }
            page = cJSON_GetObjectItem(data, "page");
            bookId = cJSON_GetObjectItem(page, "bookId");
            physicalIndex=cJSON_GetObjectItem(page, "physicalIndex");

            if(last_physicalIndex==physicalIndex->valueint && last_bookid==bookId->valueint)
            {
                ret = 2;
                goto json_exit;
            }

            last_physicalIndex=physicalIndex->valueint;
            last_bookid=bookId->valueint;
            rt_kprintf("\r\nbookId:%d\r\n",bookId->valueint);
            rt_kprintf("\r\nphysicalIndex:%d\r\n",physicalIndex->valueint);

            audios = cJSON_GetObjectItem(page, "audios");
            if(NULL!=audios)
            {
                voice = cJSON_GetObjectItem(audios, "voice");
                if(NULL!=voice)
                {
                    cJSON *url_list  = voice->child;
                    int url_num=0;
                    while(NULL!=url_list)
                    {
                        fileName = cJSON_GetObjectItem(url_list, "fileName");
                        loop =  cJSON_GetObjectItem(url_list, "loop");
                        startAt = cJSON_GetObjectItem(url_list, "startAt");
                        result->voice_url[url_num++]=rt_strdup(fileName->valuestring);
						//printf("&& index:%d,url:%s\r\n\r\n",url_num,fileName->valuestring);
                        url_list=url_list->next;
                    }
                }

                effectSound = cJSON_GetObjectItem(audios, "effectSound");
                if(NULL!=effectSound)
                {
                    fileName = cJSON_GetObjectItem(effectSound->child, "fileName");
                    loop =  cJSON_GetObjectItem(effectSound->child, "loop");
                    startAt = cJSON_GetObjectItem(effectSound->child, "startAt");
                    result->effectSound_url=rt_strdup(fileName->valuestring);
                }
       
                bgMusic = cJSON_GetObjectItem(audios, "bgMusic");
                if(NULL!=bgMusic)
                {
                    fileName = cJSON_GetObjectItem(bgMusic->child, "fileName");
                    loop =  cJSON_GetObjectItem(bgMusic->child, "loop");
                    startAt = cJSON_GetObjectItem(bgMusic->child, "startAt");
                    result->bgMusic_url=rt_strdup(fileName->valuestring);
                }
                              
            }
        }
    }
    else
    {
        msg = cJSON_GetObjectItem(root, "msg");
        rt_kprintf("post failed msg:%s\r\n",msg->valuestring);
    }

json_exit:   
    if (RT_NULL != root)  
		cJSON_Delete(root);  

__exit:
    if (NULL!= session) 
		webclient_close(session);
    if (NULL!= buffer ) 
		web_free(buffer);
    return  ret;
}

void wantong_server_deinit(void)
{
    if(NULL != login_result.token)
    {
        rt_free(login_result.token);
        login_result.token = NULL;
    }
    if(NULL != login_result.verification)
    {
        rt_free(login_result.verification);
        login_result.verification = NULL;
    }

    if(NULL != login_result.Cookie)
    {
        rt_free(login_result.Cookie);
        login_result.Cookie = NULL;
    }
    last_physicalIndex=-1;
}