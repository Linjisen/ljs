#ifndef _WANTONG_HTTPCLIENT_H_
#define _WANTONG_HTTPCLIENT_H_
#define MAX_VOICE_URL		8

//图片识别结果
typedef struct _json_result {
	int  code;
    int  bookId; 
    int  physicalIndex;        
    char *voice_url[MAX_VOICE_URL];    
    char *effectSound_url;
    char *bgMusic_url;
} RecognitionResult;


//玩瞳SDK登录结果
typedef struct _login_result {
    int code;              //服务器返回的登录结果状态码
    char *verification;    //服务器返回的授信字符串
    char *token;
    char *Cookie;
} LoginResult;


void customLoginRequest(char *requestStr, LoginResult *result);
int wantong_http_login(const char *uri,LoginResult *result,unsigned char *buffer);
int wantong_post_resquest(RecognitionResult *result,char *imag_buf,int image_size,void (*fn)());
void wantong_authentication(void);
void wantong_server_deinit(void);
#endif
