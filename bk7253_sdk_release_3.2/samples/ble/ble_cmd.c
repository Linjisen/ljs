#include <rtthread.h>
#include <finsh.h>
#include "common.h"
#include "ble_api.h"
#include "ble_pub.h"
#include "app_sdp.h"
#include "param_config.h"
#include "app_task.h"
#include "ble_cmd.h"
#include "application.h"
#include "samples_config.h"
#include "uart_pub.h"

#ifdef USING_BLE_TEST

#define BK_ATT_DECL_PRIMARY_SERVICE_128     {0x00,0x28,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
#define BK_ATT_DECL_CHARACTERISTIC_128      {0x03,0x28,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
#define BK_ATT_DESC_CLIENT_CHAR_CFG_128     {0x02,0x29,0,0,0,0,0,0,0,0,0,0,0,0,0,0}

#define WRITE_REQ_CHARACTERISTIC_AF6A_128   {0x6F,0xF1,0xD8,0x54,0xF8,0x00,0xE5,0x94,0x35,0x4E,0x71,0x2B,0x6A,0xAF,0x80,0x1F}
#define WRITE_REQ_CHARACTERISTIC_AF6B_128   {0x6F,0xF1,0xD8,0x54,0xF8,0x00,0xE5,0x94,0x35,0x4E,0x71,0x2B,0x6B,0xAF,0x80,0x1F}
#define NOTIFY_CHARACTERISTIC_AF6C_128      {0x6F,0xF1,0xD8,0x54,0xF8,0x00,0xE5,0x94,0x35,0x4E,0x71,0x2B,0x6C,0xAF,0x80,0x1F}
#define WRITE_REQ_CHARACTERISTIC_AF6C_128   {0x6F,0xF1,0xD8,0x54,0xF8,0x00,0xE5,0x94,0x35,0x4E,0x71,0x2B,0x6D,0xAF,0x80,0x1F}
#define NOTIFYCHARACTERISTIC_AF6C_128       {0x6F,0xF1,0xD8,0x54,0xF8,0x00,0xE5,0x94,0x35,0x4E,0x71,0x2B,0x6E,0xAF,0x80,0x1F}

typedef adv_info_t ble_adv_param_t;
//static const uint8_t test_svc_uuid[16] =  {0x96,0x4B,0x12,0xAB,0x87,0xE2,0xFC,0xA9,0x6E,0x4D,0xCF,0x3E,0xE7,0xFE,0x00,0x00}; 
static const uint8_t test_svc_uuid[16] =  {0x96,0x4B,0x12,0xAB,0x87,0xE2,0xFC,0xA9,0x6E,0x4D,0xCF,0x3E,0x66,0xFE,0xF0,0x1C};

enum
{
    TEST_IDX_SVC,    
    TEST_IDX_AF6A_VAL_CHAR,
    TEST_IDX_AF6A_VAL_VALUE,
    TEST_IDX_AF6B_VAL_CHAR,
    TEST_IDX_AF6B_VAL_VALUE,
    TEST_IDX_AF6C_VAL_CHAR,
    TEST_IDX_AF6C_VAL_VALUE,
    TEST_IDX_AF6C_VAL_NTF_CFG,
    TEST_IDX_AF6D_VAL_CHAR,
    TEST_IDX_AF6D_VAL_VALUE,
    TEST_IDX_AF6E_VAL_CHAR,
    TEST_IDX_AF6E_VAL_VALUE,
    TEST_IDX_AF6E_VAL_NTF_CFG,
    TEST_IDX_NB,
};

bk_attm_desc_t test_att_db[TEST_IDX_NB] =
{
    //  Service Declaration
    [TEST_IDX_SVC]              = {BK_ATT_DECL_PRIMARY_SERVICE_128,   BK_PERM_SET(RD, ENABLE), 0, 0},
    
    [TEST_IDX_AF6A_VAL_CHAR]    = {BK_ATT_DECL_CHARACTERISTIC_128,    BK_PERM_SET(RD, ENABLE), 0, 0},
    [TEST_IDX_AF6A_VAL_VALUE]   = {WRITE_REQ_CHARACTERISTIC_AF6A_128, BK_PERM_SET(WRITE_REQ, ENABLE), BK_PERM_SET(RI, ENABLE)|BK_PERM_SET(UUID_LEN, UUID_128), 128},
    
    [TEST_IDX_AF6B_VAL_CHAR]    = {BK_ATT_DECL_CHARACTERISTIC_128,    BK_PERM_SET(RD, ENABLE), 0, 0},
    [TEST_IDX_AF6B_VAL_VALUE]   = {WRITE_REQ_CHARACTERISTIC_AF6B_128, BK_PERM_SET(WRITE_REQ, ENABLE), BK_PERM_SET(RI, ENABLE)|BK_PERM_SET(UUID_LEN, UUID_128), 128},
    

    [TEST_IDX_AF6C_VAL_CHAR]    = {BK_ATT_DECL_CHARACTERISTIC_128,    BK_PERM_SET(RD, ENABLE), 0, 0},
    [TEST_IDX_AF6C_VAL_VALUE]   = {NOTIFY_CHARACTERISTIC_AF6C_128,    BK_PERM_SET(RD, ENABLE)|BK_PERM_SET(NTF, ENABLE), BK_PERM_SET(RI, ENABLE)|BK_PERM_SET(UUID_LEN, UUID_128), 128},
    [TEST_IDX_AF6C_VAL_NTF_CFG] = {BK_ATT_DESC_CLIENT_CHAR_CFG_128,   BK_PERM_SET(RD, ENABLE)|BK_PERM_SET(WRITE_REQ, ENABLE), 0, 0},
    
    [TEST_IDX_AF6D_VAL_CHAR]    = {BK_ATT_DECL_CHARACTERISTIC_128,    BK_PERM_SET(RD, ENABLE), 0, 0},
    [TEST_IDX_AF6D_VAL_VALUE]   = {WRITE_REQ_CHARACTERISTIC_AF6C_128, BK_PERM_SET(WRITE_REQ, ENABLE), BK_PERM_SET(RI, ENABLE)|BK_PERM_SET(UUID_LEN, UUID_128), 128},
    

    [TEST_IDX_AF6E_VAL_CHAR]    = {BK_ATT_DECL_CHARACTERISTIC_128,    BK_PERM_SET(RD, ENABLE), 0, 0},
    [TEST_IDX_AF6E_VAL_VALUE]   = {NOTIFYCHARACTERISTIC_AF6C_128,     BK_PERM_SET(RD, ENABLE)|BK_PERM_SET(WRITE_REQ, ENABLE)|BK_PERM_SET(NTF, ENABLE), BK_PERM_SET(RI, ENABLE)|BK_PERM_SET(UUID_LEN, UUID_128), 128},
    [TEST_IDX_AF6E_VAL_NTF_CFG] = {BK_ATT_DESC_CLIENT_CHAR_CFG_128,   BK_PERM_SET(RD, ENABLE)|BK_PERM_SET(WRITE_REQ, ENABLE), 0, 0},
};

ble_err_t bk_ble_init(void)
{
    ble_err_t status;
    struct bk_ble_db_cfg ble_db_cfg;

    ble_db_cfg.att_db = test_att_db;
    ble_db_cfg.att_db_nb = TEST_IDX_NB;
    ble_db_cfg.prf_task_id = 0;
    ble_db_cfg.start_hdl = 0;
    ble_db_cfg.svc_perm = BK_PERM_SET(SVC_UUID_LEN, UUID_128);
    memcpy(&(ble_db_cfg.uuid[0]), &test_svc_uuid[0], 16);

    status = bk_ble_create_db(&ble_db_cfg);

    return status;
}


void appm_adv_data_decode(uint8_t len, const uint8_t *data)
{
    uint8_t index;
    uint8_t i;
    for(index = 0; index < len;)
    {
        switch(data[index + 1])
        {
            case 0x01:
            {
                bk_printf("AD_TYPE : ");
                for(i = 0; i < data[index] - 1; i++)
                {
                    bk_printf("%02x ",data[index + 2 + i]);
                }
                bk_printf("\r\n");
                index +=(data[index] + 1);
            }
            break;
            case 0x08:
            case 0x09:
            {
                bk_printf("ADV_NAME : ");
                for(i = 0; i < data[index] - 1; i++)
                {
                    bk_printf("%c",data[index + 2 + i]);
                }
                bk_printf("\r\n");
                index +=(data[index] + 1);
            }
            break;
            case 0x02:
            {
                bk_printf("UUID : ");
                for(i = 0; i < data[index] - 1;)
                {
                    bk_printf("%02x%02x  ",data[index + 2 + i],data[index + 3 + i]);
                    i+=2;
                }
                bk_printf("\r\n");
                index +=(data[index] + 1);
            }
            break;
            default:
            {
                index +=(data[index] + 1);
            }
            break;
        }
    }
    return ;
}

void ble_write_callback(write_req_t *write_req)
{
    bk_printf("write_cb[prf_id:%d, att_idx:%d, len:%d]\r\n", write_req->prf_id, write_req->att_idx, write_req->len);
    for(int i = 0; i < write_req->len; i++)
    {
        bk_printf("0x%x ", write_req->value[i]);
    }
    bk_printf("\r\n");
}

uint8_t ble_read_callback(read_req_t *read_req)
{
    bk_printf("read_cb[prf_id:%d, att_idx:%d]\r\n", read_req->prf_id, read_req->att_idx);
    read_req->value[0] = 0x10;
    read_req->value[1] = 0x20;
    read_req->value[2] = 0x30;
    return 3;
}

void ble_event_callback(ble_event_t event, void *param)
{
    switch(event)
    {
        case BLE_STACK_OK:
        {
            os_printf("STACK INIT OK\r\n");
            bk_ble_init();
        }
        break;
        case BLE_STACK_FAIL:
        {
            os_printf("STACK INIT FAIL\r\n");
        }
        break;
        case BLE_CONNECT:
        {
            os_printf("BLE CONNECT\r\n");
        }
        break;
        case BLE_DISCONNECT:
        {
            os_printf("BLE DISCONNECT\r\n");
        }
        break;
        case BLE_MTU_CHANGE:
        {
            os_printf("BLE_MTU_CHANGE:%d\r\n", *(uint16_t *)param);
        }
        break;
        case BLE_TX_DONE:
        {
            os_printf("BLE_TX_DONE\r\n");
        }
        break;
        case BLE_GEN_DH_KEY:
        {
            os_printf("BLE_GEN_DH_KEY\r\n");
            os_printf("key_len:%d\r\n", ((struct ble_gen_dh_key_ind *)param)->len);
            for(int i = 0; i < ((struct ble_gen_dh_key_ind *)param)->len; i++)
            {
                os_printf("%02x ", ((struct ble_gen_dh_key_ind *)param)->result[i]);
            }
            os_printf("\r\n");
        }    
        break;
        case BLE_GET_KEY:
        {
            os_printf("BLE_GET_KEY\r\n");
            os_printf("pri_len:%d\r\n", ((struct ble_get_key_ind *)param)->pri_len);
            for(int i = 0; i < ((struct ble_get_key_ind *)param)->pri_len; i++)
            {
                os_printf("%02x ", ((struct ble_get_key_ind *)param)->pri_key[i]);
            }
            os_printf("\r\n");
        }    
        break;
        case BLE_CREATE_DB_OK:
        {
            os_printf("CREATE DB SUCCESS\r\n");
        }
        break;
        default:
            os_printf("UNKNOW EVENT\r\n");
        break;
    }
}

static void ble_command_usage(void)
{
    os_printf("ble help           - Help information\r\n");
    os_printf("ble active         - Active ble to with BK7231BTxxx\r\n");    
    os_printf("ble start_adv      - Start advertising as a slave device\r\n");
    os_printf("ble stop_adv       - Stop advertising as a slave device\r\n");
    os_printf("ble notify prf_id att_id value\r\n");
    os_printf("                   - Send ntf value to master\r\n");
    os_printf("ble indicate prf_id att_id value\r\n");
    os_printf("                   - Send ind value to master\r\n");

    os_printf("ble disc           - Disconnect\r\n");
}

static void ble_get_info_handler(void)
{
    UINT8 *ble_mac;
    os_printf("\r\n****** ble information ************\r\n");

    if (ble_is_start() == 0) {
        os_printf("no ble startup          \r\n");
        return;
    }
    ble_mac = ble_get_mac_addr();    
    os_printf("* name: %s             \r\n", ble_get_name());
    os_printf("* mac:%02x-%02x-%02x-%02x-%02x-%02x\r\n", ble_mac[0], ble_mac[1],ble_mac[2],ble_mac[3],ble_mac[4],ble_mac[5]);  
    os_printf("***********  end  *****************\r\n");     
}

typedef adv_info_t ble_adv_param_t;

static void ble_advertise(void)
{
    UINT8 mac[6];
    char ble_name[20];
    UINT8 adv_idx, adv_name_len;

    wifi_get_mac_address((char *)mac, CONFIG_ROLE_STA);
    adv_name_len = snprintf(ble_name, sizeof(ble_name), "bk72xx-%02x%02x", mac[4], mac[5]);

    memset(&adv_info, 0x00, sizeof(adv_info));

    adv_info.channel_map = 7;
    adv_info.interval_min = 160;
    adv_info.interval_max = 160;

    adv_idx = 0;
    adv_info.advData[adv_idx] = 0x02; adv_idx++;
    adv_info.advData[adv_idx] = 0x01; adv_idx++;
    adv_info.advData[adv_idx] = 0x06; adv_idx++;

    adv_info.advData[adv_idx] = adv_name_len + 1; adv_idx +=1;
    adv_info.advData[adv_idx] = 0x09; adv_idx +=1; //name
    memcpy(&adv_info.advData[adv_idx], ble_name, adv_name_len); adv_idx +=adv_name_len;

    adv_info.advDataLen = adv_idx;

    adv_idx = 0;

    adv_info.respData[adv_idx] = adv_name_len + 1; adv_idx +=1;
    adv_info.respData[adv_idx] = 0x08; adv_idx +=1; //name
    memcpy(&adv_info.respData[adv_idx], ble_name, adv_name_len); adv_idx +=adv_name_len;
    adv_info.respDataLen = adv_idx;

    if (ERR_SUCCESS != appm_start_advertising())
    {
        os_printf("ERROR\r\n");
    }
}

static void ble_command(int argc, char **argv)
{
    ble_adv_param_t adv_param;

    if ((argc < 2) || (os_strcmp(argv[1], "help") == 0))
    {
        ble_command_usage();
        return;
    }

    if (os_strcmp(argv[1], "active") == 0)
    {
        
        ble_set_write_cb(ble_write_callback);
        ble_set_read_cb(ble_read_callback);
        ble_set_event_cb(ble_event_callback);
        ble_activate(NULL);
    }
    else if(os_strcmp(argv[1], "start_adv") == 0)
    { 
        ble_advertise();
    }
    else if(os_strcmp(argv[1], "stop_adv") == 0)
    {
        if(ERR_SUCCESS != appm_stop_advertising())
        {
            os_printf("ERROR\r\n");
        }
    }
    else if(os_strcmp(argv[1], "notify") == 0)
    {
        uint8 len;
        uint16 prf_id;
        uint16 att_id;
        uint8 write_buffer[20];
        
        if(argc != 5)
        {
            ble_command_usage();
            return;
        }

        len = os_strlen(argv[4]);
        if(len % 2 != 0)
        {
            os_printf("ERROR\r\n");
            return;
        }
        hexstr2bin(argv[4], write_buffer, len/2);

        prf_id = atoi(argv[2]);
        att_id = atoi(argv[3]);

        if(ERR_SUCCESS != bk_ble_send_ntf_value(len / 2, write_buffer, prf_id, att_id))
        {
            os_printf("ERROR\r\n");
        }
    }
    else if(os_strcmp(argv[1], "indicate") == 0)
    {
        uint8 len;
        uint16 prf_id;
        uint16 att_id;
        uint8 write_buffer[20];
        
        if(argc != 5)
        {
            ble_command_usage();
            return;
        }

        len = os_strlen(argv[4]);
        if(len % 2 != 0)
        {
            os_printf("ERROR\r\n");
            return;
        }
        hexstr2bin(argv[4], write_buffer, len/2);

        prf_id = atoi(argv[2]);
        att_id = atoi(argv[3]);

        if(ERR_SUCCESS != bk_ble_send_ind_value(len / 2, write_buffer, prf_id, att_id))
        {
            os_printf("ERROR\r\n");
        }
    }
    else if(os_strcmp(argv[1], "disc") == 0)
    {
        appm_disconnect();
    }
}
MSH_CMD_EXPORT(ble_command,ble_command);

#endif