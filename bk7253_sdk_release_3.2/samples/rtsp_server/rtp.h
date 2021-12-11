#ifndef RTP_H_
#define RTP_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    RTP_IDLE = 0,
    RTP_START = 1,
    RTP_PAUSE = 2,
    RTP_STOP = 3
} RTP_SERVER_T;

typedef struct
{
	uint8_t  state;
	uint8_t  *clientIP;
	int 	clientRtpPort;
	int 	clientRtcpPort;
	uint16_t serverRtpSockfd;
	uint16_t serverRtcpSockfd;
    int     serverRtpPort;

} RTP_PARAMTER_T;

typedef struct
{
    uint8_t  *img_buf;
    uint16_t  img_max_size;
    uint16_t  img_width;
    uint16_t  img_hight;
    uint8_t   camera_fps;

} CAMERA_PARAMTER_T;

void rtp_server_init(CAMERA_PARAMTER_T* cam,RTP_PARAMTER_T *rtp);
void rtp_server_reinit(CAMERA_PARAMTER_T* cam,RTP_PARAMTER_T *rtp);
int rtp_server_deinit(void);
int rtp_server_start(const char* clientIP,int clientRtpPort);
int rtp_server_stop(void);
int rtp_server_pause(void);
RTP_SERVER_T get_rtp_server_state(void);

#endif /* RTP_H_ */
