#include "include.h"
#include "wlan_ui_pub.h"
#include "rw_pub.h"
#include "vif_mgmt.h"
#include "sa_station.h"
#include "param_config.h"
#include "common/ieee802_11_defs.h"
#include "driver_beken.h"
#include "mac_ie.h"
#include "sa_ap.h"
#include "main_none.h"
#include "sm.h"
#include "mac.h"
#include "sm_task.h"
#include "scan_task.h"
#include "hal_machw.h"
#include "error.h"
#include "lwip_netif_address.h"
#include "lwip/inet.h"
#include <stdbool.h>
#include "rw_pub.h"
#include "power_save_pub.h"
#include "rwnx.h"
#include "net.h"
#include "mm_bcn.h"
#include "phy_trident.h"
#include "mcu_ps_pub.h"
#include "manual_ps_pub.h"
#include "gpio_pub.h"
#include "phy_trident.h"
#include "wdt_pub.h"
#include "wlan_dev.h"
#include "rtdevice.h"
#include "sys_ctrl_pub.h"
#include "wpa_psk_cache.h"
#if CFG_ROLE_LAUNCH
#include "role_launch.h"
#endif

monitor_cb_t g_monitor_cb = 0;
unsigned char g_monitor_is_not_filter = 0;
static monitor_cb_t g_bcn_cb = 0;

int g_set_channel_postpone_num = 0; 
#ifdef CONFIG_AOS_MESH
monitor_cb_t g_mesh_monitor_cb = 0;
uint8_t g_mesh_bssid[6];
#endif
FUNC_1PARAM_PTR connection_status_cb = 0;

extern void mm_hw_ap_disable(void);
extern int hostapd_main_exit(void);
extern int supplicant_main_exit(void);
extern void net_wlan_add_netif(void *mac);
extern void wpa_hostapd_release_scan_rst(void);
extern int mm_bcn_get_tx_cfm(void);
extern void wlan_ui_bcn_callback(uint8_t *data, int len, wifi_link_info_t *info);
extern void power_save_bcn_callback(uint8_t *data, int len, wifi_link_info_t *info);
extern void bk_wlan_register_bcn_cb(monitor_cb_t fn);
extern void mcu_ps_bcn_callback(uint8_t *data, int len, wifi_link_info_t *info);
extern void rwnx_cal_set_max_twper(FP32 max_tx_pwr);
extern int bmsg_tx_raw_cb_sender(uint8_t *buffer, int length, void *cb, void *param);
static int bk_wlan_unconditional_rf_powerup(void);
static void rwnx_remove_added_interface(void)
{
    int ret;
    u8 test_mac[6];
    struct mm_add_if_cfm *cfm;
    struct apm_start_cfm *apm_cfm = 0;

	wifi_get_mac_address(test_mac, CONFIG_ROLE_STA);
	
    cfm = (struct mm_add_if_cfm *)os_malloc(sizeof(struct mm_add_if_cfm));
	ASSERT(cfm);

    ret = rw_msg_send_add_if((const unsigned char *)&test_mac, 3, 0, cfm);
    if(ret || (cfm->status != CO_OK))
    {
        os_printf("[saap]MM_ADD_IF_REQ failed!\r\n");
        goto ERR_RETURN;
    }

    apm_cfm = (struct apm_start_cfm *)os_malloc(sizeof(struct apm_start_cfm));
    ret = rw_msg_send_apm_start_req(cfm->inst_nbr, 1, apm_cfm);

    if(ret || (apm_cfm->status != CO_OK))
    {
        os_printf("[saap]APM_START_REQ failed!\r\n");
        goto ERR_RETURN;
    }
    
    rw_msg_send_remove_if(cfm->inst_nbr);

ERR_RETURN:
    if(cfm)
    {
        os_free(cfm);
    }

    if(apm_cfm)
    {
        os_free(apm_cfm);
    }
}

int bk_wlan_enter_powersave(struct rt_wlan_device *device, int level)
{
    int result = 0;

    if (device == RT_NULL) return -RT_EIO;

    result = rt_device_control(RT_DEVICE(device), WIFI_ENTER_POWERSAVE, (void *)&level);

    return result;
}

void bk_wlan_connection_loss(void)
{
    struct vif_info_tag *p_vif_entry = vif_mgmt_first_used();

    while (p_vif_entry != NULL)
    {
        if(p_vif_entry->type == VIF_STA)
        {
            os_printf("bk_wlan_connection_loss vif:%d\r\n", p_vif_entry->index);
            sta_ip_down();
            rw_msg_send_connection_loss_ind(p_vif_entry->index);
        }
        p_vif_entry = vif_mgmt_next(p_vif_entry);
    }
}

uint32_t bk_sta_cipher_is_open(void)
{
    ASSERT(g_sta_param_ptr);
    return (BK_SECURITY_TYPE_NONE == g_sta_param_ptr->cipher_suite);
}

uint32_t bk_sta_cipher_is_wep(void)
{
    ASSERT(g_sta_param_ptr);
    return (BK_SECURITY_TYPE_WEP == g_sta_param_ptr->cipher_suite);
}

int bk_sta_cipher_type(void)
{
    if(!sta_ip_is_start()) 
    {
        return -1;
    }
    
    return g_sta_param_ptr->cipher_suite;
}

OSStatus bk_wlan_set_country(const wifi_country_t *country)
{
    return rw_ieee80211_set_country(country);
}

OSStatus bk_wlan_get_country(wifi_country_t *country)
{
    return rw_ieee80211_get_country(country);
}

uint32_t bk_wlan_ap_get_frequency(void)
{
    uint8_t channel = bk_wlan_ap_get_channel_config();
    
    return rw_ieee80211_get_centre_frequency(channel);
}

uint8_t bk_wlan_ap_get_channel_config(void)
{
    return g_ap_param_ptr->chann;
}

void bk_wlan_ap_set_channel_config(uint8_t channel)
{
    g_ap_param_ptr->chann = channel;
}

uint8_t bk_wlan_has_role(uint8_t role)
{
    VIF_INF_PTR vif_entry;
    uint32_t role_count = 0;

    vif_entry = (VIF_INF_PTR)rwm_mgmt_is_vif_first_used();
    while(vif_entry)
    {
        if(vif_entry->type == role)
        {
            role_count ++ ;
        }

        vif_entry = (VIF_INF_PTR)rwm_mgmt_next(vif_entry);
    }

    return role_count;
}

void bk_wlan_set_coexist_at_init_phase(uint8_t current_role)
{
    uint32_t coexit = 0;
    
    switch(current_role)
    {
        case CONFIG_ROLE_AP:
            if(bk_wlan_has_role(VIF_STA))
            {
                coexit = 1;
            }
            break;
            
        case CONFIG_ROLE_STA:
            if(bk_wlan_has_role(VIF_AP))
            {
                coexit = 1;
            }
            break;
            
        case CONFIG_ROLE_NULL:
            if(bk_wlan_has_role(VIF_STA)
                && bk_wlan_has_role(VIF_AP))
            {
                coexit = 1;
            }
            break;
            
        case CONFIG_ROLE_COEXIST:
            coexit = 1;
            ASSERT(CONFIG_ROLE_COEXIST == g_wlan_general_param->role);
            break;
            
        default:
            break;
    }

    if(coexit)
    {
        g_wlan_general_param->role = CONFIG_ROLE_COEXIST;
    }
}

uint16_t bk_wlan_sta_get_frequency(void)
{  
    uint16_t frequency = 0;
    uint32_t sta_flag = 0;
    VIF_INF_PTR vif_entry;
    
    vif_entry = (VIF_INF_PTR)rwm_mgmt_is_vif_first_used();
    while(vif_entry)

    {
        if(vif_entry->type == VIF_STA)
        {
            sta_flag = 1;
            break;
        }

        vif_entry = (VIF_INF_PTR)rwm_mgmt_next(vif_entry);
    }

    if(0 == sta_flag)
    {
        goto get_exit;
    }

    frequency = chan_get_vif_frequency(vif_entry);
    
get_exit:    
    return frequency;
}

uint8_t bk_wlan_sta_get_channel(void)
{  
    uint8_t channel = 0;
    uint16_t frequency;

    frequency = bk_wlan_sta_get_frequency();
    if(frequency)
    {
        channel = rw_ieee80211_get_chan_id(frequency);
    }
    
    return channel;
}

uint8_t sys_channel = DEFAULT_CHANNEL_AP;
uint8_t bk_wlan_ap_get_default_channel(void)
{
    uint8_t channel;
    
    /* if ap and sta are coexist, ap channel shall match sta channel firstly*/
    channel = bk_wlan_sta_get_channel();
    if(0 == channel)
    {
        if(sys_channel == DEFAULT_CHANNEL_AP)
            channel = DEFAULT_CHANNEL_AP;
        else
            channel = sys_channel;
    }

    return channel;
}

void bk_wlan_ap_set_default_channel(uint8_t channel)
{
    sys_channel = channel;
}

void bk_wlan_ap_csa_coexist_mode(void *arg, uint8_t dummy)
{
    int ret = 0;
    uint16_t frequency;
    
    if(0 == bk_wlan_has_role(VIF_AP))
    {        
        return;
    }

    frequency = bk_wlan_sta_get_frequency();
    if(frequency)
    {
        os_printf("\r\nhostapd_channel_switch\r\n");
#if CFG_ROLE_LAUNCH
        if(!fl_get_pre_sta_cancel_status())
#endif
        {
        	ret = hostapd_channel_switch(frequency);
        }

        if(ret)
        {
            os_printf("csa_failed:%x\r\n", ret);
        }
    }
}

void bk_wlan_reg_csa_cb_coexist_mode(void)
{
    /* the callback routine will be invoked at the timepoint of associating at station mode*/
    mhdr_connect_user_cb(bk_wlan_ap_csa_coexist_mode, 0);
}

void bk_wlan_phy_open_cca(void)
{
	phy_open_cca();
	os_printf("bk_wlan cca opened\r\n");
}

void bk_wlan_phy_close_cca(void)
{
	phy_close_cca();
	os_printf("bk_wlan cca closed\r\n");
}

void bk_wlan_phy_show_cca(void)
{
	phy_show_cca();
}

void bk_reboot(void)
{
    FUNCPTR reboot = 0;
    UINT32 wdt_val = 1;

#if CFG_USE_STA_PS
    GLOBAL_INT_DECLARATION();

    GLOBAL_INT_DISABLE();
    if(power_save_if_ps_rf_dtim_enabled()
        && power_save_if_rf_sleep())
    {
        power_save_wkup_event_set(NEED_DISABLE_BIT | NEED_REBOOT_BIT);
    }
    else
    {
#endif

    os_printf("wdt reboot\r\n");
    sddev_control(WDT_DEV_NAME, WCMD_SET_PERIOD, &wdt_val);
    sddev_control(WDT_DEV_NAME, WCMD_POWER_UP, NULL);
#if CFG_USE_STA_PS
    }
    GLOBAL_INT_RESTORE();
#endif
}

void bk_wlan_ap_init(network_InitTypeDef_st *inNetworkInitPara)
{
    os_printf("Soft_AP_start\r\n");
#if CFG_ROLE_LAUNCH
	rl_init_csa_status();
#endif

    if(!g_ap_param_ptr)
    {
        g_ap_param_ptr = (ap_param_t *)os_zalloc(sizeof(ap_param_t));
        ASSERT(g_ap_param_ptr);
    }

    os_memset(g_ap_param_ptr, 0x00, sizeof(*g_ap_param_ptr));

    if(MAC_ADDR_NULL((u8 *)&g_ap_param_ptr->bssid))
    {
        wifi_get_mac_address((u8 *)&g_ap_param_ptr->bssid, CONFIG_ROLE_AP);
    }

    bk_wlan_ap_set_channel_config(bk_wlan_ap_get_default_channel());

    if(!g_wlan_general_param)
    {
        g_wlan_general_param = (general_param_t *)os_zalloc(sizeof(general_param_t));
        ASSERT(g_wlan_general_param);
    }
    g_wlan_general_param->role = CONFIG_ROLE_AP;
    bk_wlan_set_coexist_at_init_phase(CONFIG_ROLE_AP);

    if(inNetworkInitPara)
    {
        g_ap_param_ptr->ssid.length = MIN(SSID_MAX_LEN, os_strlen(inNetworkInitPara->wifi_ssid));
        os_memcpy(g_ap_param_ptr->ssid.array, inNetworkInitPara->wifi_ssid, g_ap_param_ptr->ssid.length);
        g_ap_param_ptr->key_len = os_strlen(inNetworkInitPara->wifi_key);
        if(g_ap_param_ptr->key_len < 8)
        {
            g_ap_param_ptr->cipher_suite = BK_SECURITY_TYPE_NONE;
        }
        else
        {
            g_ap_param_ptr->cipher_suite = BK_SECURITY_TYPE_WPA2_AES;
            os_memcpy(g_ap_param_ptr->key, inNetworkInitPara->wifi_key, g_ap_param_ptr->key_len);
        }

        if(inNetworkInitPara->dhcp_mode == DHCP_SERVER)
        {
            g_wlan_general_param->dhcp_enable = 1;
        }
        else
        {
            g_wlan_general_param->dhcp_enable = 0;
        }
        inet_aton(inNetworkInitPara->local_ip_addr, &(g_wlan_general_param->ip_addr));
        inet_aton(inNetworkInitPara->net_mask, &(g_wlan_general_param->ip_mask));
        inet_aton(inNetworkInitPara->gateway_ip_addr, &(g_wlan_general_param->ip_gw));
			
#if CFG_ROLE_LAUNCH
		if(rl_pre_ap_set_status(RL_STATUS_AP_INITING))
		{
			return;
		}
#endif
    }
	
    sa_ap_init();
}

OSStatus bk_wlan_start_ap(network_InitTypeDef_st *inNetworkInitParaAP)
{
    int ret, flag ,empty;
    GLOBAL_INT_DECLARATION();

	while( 1 )
	{
		GLOBAL_INT_DISABLE();
		flag = mm_bcn_get_tx_cfm();
        empty = is_apm_bss_config_empty();
		if ( flag == 0 && empty == 1)
		{
			GLOBAL_INT_RESTORE();
			break;
		}
		else
		{
			GLOBAL_INT_RESTORE();
			rtos_delay_milliseconds(100);
		}
	}

	bk_wlan_stop(BK_SOFT_AP);
    
    bk_wlan_ap_init(inNetworkInitParaAP);
    
    ret = hostapd_main_entry(2, 0);
    if(ret)
    {
        os_printf("bk_wlan_start softap failed!!\r\n");
        bk_wlan_stop(BK_SOFT_AP);
        return -1;
    }

    net_wlan_add_netif(&g_ap_param_ptr->bssid);

    ip_address_set(BK_SOFT_AP, 
                   inNetworkInitParaAP->dhcp_mode,
                   inNetworkInitParaAP->local_ip_addr,
                   inNetworkInitParaAP->net_mask,
                   inNetworkInitParaAP->gateway_ip_addr,
                   inNetworkInitParaAP->dns_server_ip_addr);
    uap_ip_start();

    sm_build_broadcast_deauthenticate();

    return kNoErr;
}

void bk_wlan_sta_init(network_InitTypeDef_st *inNetworkInitPara)
{
	uint8_t passphrase[65];
	
#if CFG_ROLE_LAUNCH
	rl_init_csa_status();
#endif
	
    if(!g_sta_param_ptr)
    {
        g_sta_param_ptr = (sta_param_t *)os_zalloc(sizeof(sta_param_t));
        ASSERT(g_sta_param_ptr);
    }

    wifi_get_mac_address((u8 *)&g_sta_param_ptr->own_mac, CONFIG_ROLE_STA);
    if(!g_wlan_general_param)
    {
        g_wlan_general_param = (general_param_t *)os_zalloc(sizeof(general_param_t));
        ASSERT(g_wlan_general_param);
    }
    g_wlan_general_param->role = CONFIG_ROLE_STA;
    bk_wlan_set_coexist_at_init_phase(CONFIG_ROLE_STA);

    if(inNetworkInitPara)
    {
        g_sta_param_ptr->ssid.length = MIN(SSID_MAX_LEN, os_strlen(inNetworkInitPara->wifi_ssid));
        os_memcpy(g_sta_param_ptr->ssid.array,
                  inNetworkInitPara->wifi_ssid,
                  g_sta_param_ptr->ssid.length);

#if CFG_SUPPOET_BSSID_CONNECT
        os_memcpy(g_sta_param_ptr->fast_connect.bssid, inNetworkInitPara->wifi_bssid, sizeof(inNetworkInitPara->wifi_bssid));
#endif

        g_sta_param_ptr->key_len = os_strlen(inNetworkInitPara->wifi_key);
        os_memcpy(g_sta_param_ptr->key, inNetworkInitPara->wifi_key, g_sta_param_ptr->key_len);

		os_memcpy(passphrase, g_sta_param_ptr->key, g_sta_param_ptr->key_len);
		passphrase[g_sta_param_ptr->key_len] = 0;
		wpa_psk_request(g_sta_param_ptr->ssid.array, g_sta_param_ptr->ssid.length,
				passphrase, NULL, 0);

        if(inNetworkInitPara->dhcp_mode == DHCP_CLIENT)
        {
            g_wlan_general_param->dhcp_enable = 1;
        }
        else
        {
            g_wlan_general_param->dhcp_enable = 0;
            inet_aton(inNetworkInitPara->local_ip_addr, &(g_wlan_general_param->ip_addr));
            inet_aton(inNetworkInitPara->net_mask, &(g_wlan_general_param->ip_mask));
            inet_aton(inNetworkInitPara->gateway_ip_addr, &(g_wlan_general_param->ip_gw));
        }
    }
		
#if CFG_ROLE_LAUNCH
	    if(rl_pre_sta_set_status(RL_STATUS_STA_INITING))
	    {
	        return;
	    }
#endif

    bk_wlan_reg_csa_cb_coexist_mode();
    sa_station_init();
    
    bk_wlan_register_bcn_cb(wlan_ui_bcn_callback);
}

OSStatus bk_wlan_start_sta(network_InitTypeDef_st *inNetworkInitPara)
{
    bk_wlan_stop(BK_STATION);
    
    bk_wlan_sta_init(inNetworkInitPara);
    
    supplicant_main_entry(inNetworkInitPara->wifi_ssid);
    
    net_wlan_add_netif(&g_sta_param_ptr->own_mac);
    
    ip_address_set(inNetworkInitPara->wifi_mode,
                   inNetworkInitPara->dhcp_mode,
                   inNetworkInitPara->local_ip_addr,
                   inNetworkInitPara->net_mask,
                   inNetworkInitPara->gateway_ip_addr,
                   inNetworkInitPara->dns_server_ip_addr);
    
    return kNoErr;
}

OSStatus bk_wlan_start(network_InitTypeDef_st *inNetworkInitPara)
{
    int ret = 0;
#if CFG_ROLE_LAUNCH
    LAUNCH_REQ lreq;
#endif

    bk_disable_unconditional_sleep();
    bk_wlan_unconditional_rf_powerup();

    if(bk_wlan_is_monitor_mode()) 
    {
        os_printf("monitor (ie.airkiss) is not finish yet, stop it or waiting it finish!\r\n");
        return ret;
    }

    if(inNetworkInitPara->wifi_mode == BK_SOFT_AP)
    {
        #if CFG_ROLE_LAUNCH
        lreq.req_type = LAUNCH_REQ_AP;
        lreq.descr = *inNetworkInitPara;
        
        rl_ap_request_enter(&lreq, 0);
        #else
        bk_wlan_start_ap(inNetworkInitPara);
        #endif
    }
    else if(inNetworkInitPara->wifi_mode == BK_STATION)
    {
        #if CFG_ROLE_LAUNCH
		rl_cnt_init();
        lreq.req_type = LAUNCH_REQ_STA;
        lreq.descr = *inNetworkInitPara;
        
        rl_sta_request_enter(&lreq, 0);
        #else
        bk_wlan_start_sta(inNetworkInitPara);
        #endif
    }

    return 0;
}

void bk_wlan_start_scan(void)
{
    SCAN_PARAM_T scan_param = {0};
	
#if CFG_USE_STA_PS    
    bk_wlan_dtim_rf_ps_disable_send_msg();
#endif

    if(bk_wlan_is_monitor_mode()) 
    {
        os_printf("monitor (ie.airkiss) is not finish yet, stop it or waiting it finish!\r\n");
		
		#if CFG_ROLE_LAUNCH
		rl_pre_sta_set_status(RL_STATUS_STA_LAUNCH_FAILED);
		#endif
		
        return;
    }

    bk_wlan_sta_init(0);

    os_memset(&scan_param.bssid, 0xff, ETH_ALEN);
	scan_param.vif_idx = INVALID_VIF_IDX;
	scan_param.num_ssids = 1;
	
    rw_msg_send_scanu_req(&scan_param);
}

void bk_wlan_scan_ap_reg_cb(FUNC_2PARAM_PTR ind_cb)
{
    mhdr_scanu_reg_cb(ind_cb,0);
}

unsigned char bk_wlan_get_scan_ap_result_numbers(void)
{
    struct scanu_rst_upload *scan_rst;
    unsigned char scanu_num = 0;

    scan_rst = sr_get_scan_results();
    if(scan_rst)
    {
        scanu_num = scan_rst->scanu_num;
    }

    return scanu_num;
}

void bk_wlan_get_scan_ap_result(SCAN_RST_ITEM_PTR scan_rst_table,unsigned char get_scanu_num)
{
    struct scanu_rst_upload *scan_rst;
    unsigned char scanu_num = 0,i;
    
    scan_rst = sr_get_scan_results();
    if(scan_rst)
    {
        scanu_num = (scan_rst->scanu_num) > get_scanu_num ? (get_scanu_num):(scan_rst->scanu_num);
        
        for(i = 0;i<scanu_num;i++)
        {
            scan_rst_table[i] = *(scan_rst->res[i]);
        }
    }   

    sr_release_scan_results(scan_rst);
}

void bk_wlan_start_assign_scan(UINT8 **ssid_ary, UINT8 ssid_num)
{
    SCAN_PARAM_T scan_param = {0};

    bk_wlan_sta_init(0);

    os_memset(&scan_param.bssid, 0xff, ETH_ALEN);
	scan_param.vif_idx = INVALID_VIF_IDX;
    scan_param.num_ssids = ssid_num;
    for (int i = 0 ; i < ssid_num ; i++ )
    {
        scan_param.ssids[i].length = MIN(SSID_MAX_LEN, os_strlen(ssid_ary[i]));
        os_memcpy(scan_param.ssids[i].array, ssid_ary[i], scan_param.ssids[i].length);
    }
	
    rw_msg_send_scanu_req(&scan_param);
}

void bk_wlan_sta_init_adv(network_InitTypeDef_adv_st *inNetworkInitParaAdv)
{
    if(!g_sta_param_ptr)
    {
        g_sta_param_ptr = (sta_param_t *)os_zalloc(sizeof(sta_param_t));
        ASSERT(g_sta_param_ptr);
    }

    if(MAC_ADDR_NULL((u8 *)&g_sta_param_ptr->own_mac))
    {
        wifi_get_mac_address((char *)&g_sta_param_ptr->own_mac, CONFIG_ROLE_STA);
    }

    g_sta_param_ptr->ssid.length = MIN(SSID_MAX_LEN, os_strlen(inNetworkInitParaAdv->ap_info.ssid));
    os_memcpy(g_sta_param_ptr->ssid.array, inNetworkInitParaAdv->ap_info.ssid, g_sta_param_ptr->ssid.length);

	g_sta_param_ptr->cipher_suite = inNetworkInitParaAdv->ap_info.security;
    g_sta_param_ptr->fast_connect_set = 1;
    g_sta_param_ptr->fast_connect.chann = inNetworkInitParaAdv->ap_info.channel;
    os_memcpy(g_sta_param_ptr->fast_connect.bssid, inNetworkInitParaAdv->ap_info.bssid, ETH_ALEN);
    g_sta_param_ptr->key_len = inNetworkInitParaAdv->key_len;
    os_memcpy((uint8_t *)g_sta_param_ptr->key, inNetworkInitParaAdv->key, inNetworkInitParaAdv->key_len);

    if(!g_wlan_general_param)
    {
        g_wlan_general_param = (general_param_t *)os_malloc(sizeof(general_param_t));
    }
    g_wlan_general_param->role = CONFIG_ROLE_STA;
    bk_wlan_set_coexist_at_init_phase(CONFIG_ROLE_STA);

    if(inNetworkInitParaAdv->dhcp_mode == DHCP_CLIENT)
    {
        g_wlan_general_param->dhcp_enable = 1;
    }
    else
    {
        g_wlan_general_param->dhcp_enable = 0;
        inet_aton(inNetworkInitParaAdv->local_ip_addr, &(g_wlan_general_param->ip_addr));
        inet_aton(inNetworkInitParaAdv->net_mask, &(g_wlan_general_param->ip_mask));
        inet_aton(inNetworkInitParaAdv->gateway_ip_addr, &(g_wlan_general_param->ip_gw));
    }
    
    bk_wlan_reg_csa_cb_coexist_mode();
    sa_station_init();
}

void bk_wlan_ap_init_adv(network_InitTypeDef_ap_st *inNetworkInitParaAP)
{
    if(!g_ap_param_ptr)
    {
        g_ap_param_ptr = (ap_param_t *)os_zalloc(sizeof(ap_param_t));
        ASSERT(g_ap_param_ptr);
    }

    if(MAC_ADDR_NULL((u8 *)&g_ap_param_ptr->bssid))
    {
        wifi_get_mac_address((u8 *)&g_ap_param_ptr->bssid, CONFIG_ROLE_AP);
    }

    if(!g_wlan_general_param)
    {
        g_wlan_general_param = (general_param_t *)os_zalloc(sizeof(general_param_t));
        ASSERT(g_wlan_general_param);
    }
    g_wlan_general_param->role = CONFIG_ROLE_AP;
    bk_wlan_set_coexist_at_init_phase(CONFIG_ROLE_AP);

    if(inNetworkInitParaAP)
    {
        if(0 == inNetworkInitParaAP->channel)
        {
            g_ap_param_ptr->chann = bk_wlan_ap_get_default_channel();
        }
        else
        {
            g_ap_param_ptr->chann = inNetworkInitParaAP->channel;
        }
        
        g_ap_param_ptr->ssid.length = MIN(SSID_MAX_LEN, os_strlen(inNetworkInitParaAP->wifi_ssid));
        os_memcpy(g_ap_param_ptr->ssid.array, inNetworkInitParaAP->wifi_ssid, g_ap_param_ptr->ssid.length);
        g_ap_param_ptr->key_len = os_strlen(inNetworkInitParaAP->wifi_key);
        os_memcpy(g_ap_param_ptr->key, inNetworkInitParaAP->wifi_key, g_ap_param_ptr->key_len);

        g_ap_param_ptr->cipher_suite = inNetworkInitParaAP->security;        

        if(inNetworkInitParaAP->dhcp_mode == DHCP_SERVER)
        {
            g_wlan_general_param->dhcp_enable = 1;
        }
        else
        {
            g_wlan_general_param->dhcp_enable = 0;
        }
        inet_aton(inNetworkInitParaAP->local_ip_addr, &(g_wlan_general_param->ip_addr));
        inet_aton(inNetworkInitParaAP->net_mask, &(g_wlan_general_param->ip_mask));
        inet_aton(inNetworkInitParaAP->gateway_ip_addr, &(g_wlan_general_param->ip_gw));
    }

    sa_ap_init();
}

OSStatus bk_wlan_start_sta_adv(network_InitTypeDef_adv_st *inNetworkInitParaAdv)
{
    if(bk_wlan_is_monitor_mode()) 
    {
        os_printf("airkiss is not finish yet, stop airkiss or waiting it finish!\r\n");
        return 0;
    }

    bk_wlan_stop(BK_STATION);

#if CFG_ROLE_LAUNCH
    if(rl_pre_sta_set_status(RL_STATUS_STA_INITING))
    {
        return -1;
    }
#endif
    
    bk_wlan_sta_init_adv(inNetworkInitParaAdv);

    supplicant_main_entry(inNetworkInitParaAdv->ap_info.ssid);

    net_wlan_add_netif(&g_sta_param_ptr->own_mac);
    ip_address_set(BK_STATION, inNetworkInitParaAdv->dhcp_mode,
                   inNetworkInitParaAdv->local_ip_addr,
                   inNetworkInitParaAdv->net_mask,
                   inNetworkInitParaAdv->gateway_ip_addr,
                   inNetworkInitParaAdv->dns_server_ip_addr);

    return 0;
}

OSStatus bk_wlan_start_ap_adv(network_InitTypeDef_ap_st *inNetworkInitParaAP)
{
    int ret = 0;

    if(bk_wlan_is_monitor_mode()) 
    {
        os_printf("monitor (ie.airkiss) is not finish yet, stop it or waiting it finish!\r\n");
        return ret;
    }

    bk_wlan_stop(BK_SOFT_AP);

#if CFG_ROLE_LAUNCH
    if(rl_pre_ap_set_status(RL_STATUS_AP_INITING))
    {
        return 0;
    }
#endif
    
    bk_wlan_ap_init_adv(inNetworkInitParaAP);

    ret = hostapd_main_entry(2, 0);
    if(ret)
    {
        os_printf("bk_wlan_start_ap_adv failed!!\r\n");
        bk_wlan_stop(BK_SOFT_AP);
        return -1;
    }

    net_wlan_add_netif(&g_ap_param_ptr->bssid);

    ip_address_set(BK_SOFT_AP, inNetworkInitParaAP->dhcp_mode,
                   inNetworkInitParaAP->local_ip_addr,
                   inNetworkInitParaAP->net_mask,
                   inNetworkInitParaAP->gateway_ip_addr,
                   inNetworkInitParaAP->dns_server_ip_addr);
    uap_ip_start();

    sm_build_broadcast_deauthenticate();

    return kNoErr;
}

static uint8_t bk_wlan_stop_state=0;
uint8_t get_bk_wlan_stop_state(void)
{
    return bk_wlan_stop_state;
}

int bk_wlan_stop(char mode)
{
    int ret = kNoErr;
    bk_wlan_stop_state=1;
    mhdr_set_station_status(RW_EVT_STA_IDLE);
#if CFG_USE_STA_PS
    bk_wlan_dtim_rf_ps_disable_send_msg();
#endif

    switch(mode)
    {
    case BK_SOFT_AP:
	    sm_build_broadcast_deauthenticate();
		
        mm_hw_ap_disable();

        uap_ip_down();
        net_wlan_remove_netif(&g_ap_param_ptr->bssid);
        hostapd_main_exit();
        if(bk_wlan_has_role(VIF_STA))
        {
            g_wlan_general_param->role = CONFIG_ROLE_STA;
        }    
		
#if CFG_ROLE_LAUNCH
        rl_pre_ap_set_status(RL_STATUS_AP_LAUNCHED);
#endif
        break;

    case BK_STATION:
        sta_ip_down();  		
     
#if CFG_ROLE_LAUNCH
		rl_pre_sta_clear_cancel();
#endif

        net_wlan_remove_netif(&g_sta_param_ptr->own_mac);
        supplicant_main_exit();
        wpa_hostapd_release_scan_rst();
        if(bk_wlan_has_role(VIF_AP))
        {
            g_wlan_general_param->role = CONFIG_ROLE_AP;
        }   
		
#if CFG_ROLE_LAUNCH
        rl_pre_sta_set_status(RL_STATUS_STA_LAUNCHED);
#endif
        break;

    default:
        ret = kGeneralErr;
        break;
    }
    bk_wlan_stop_state=0;
    return ret;
}

OSStatus bk_wlan_set_ip_status(IPStatusTypedef *inNetpara, WiFi_Interface inInterface)
{
    OSStatus ret = kNoErr;

    switch ( inInterface )
    {
    case BK_SOFT_AP :
        if ( uap_ip_is_start() )
        {
            uap_ip_down();
        }
        else
        {
            ret = kGeneralErr;
        }
        break;

    case BK_STATION :
        if ( sta_ip_is_start() )
        {
            sta_ip_down();
        }
        else
        {
            ret = kGeneralErr;
        }
        break;

    default:
        ret = kGeneralErr;
        break;
    }

    if ( ret == kNoErr )
    {
        ip_address_set(inInterface, inNetpara->dhcp, inNetpara->ip,
                       inNetpara->mask, inNetpara->gate, inNetpara->dns);
        if ( inInterface == BK_SOFT_AP )
        {
            uap_ip_is_start();
        }
        else if ( inInterface == BK_STATION )
        {
            sta_ip_start();
        }
    }

    return ret;
}

OSStatus bk_wlan_get_ip_status(IPStatusTypedef *outNetpara, WiFi_Interface inInterface)
{
    OSStatus ret = kNoErr;
    struct wlan_ip_config addr;

    os_memset(&addr, 0, sizeof(struct wlan_ip_config));    
    
    switch ( inInterface )
    {
    case BK_SOFT_AP :
        net_get_if_addr(&addr, net_get_uap_handle());
        net_get_if_macaddr(outNetpara->mac, net_get_uap_handle());
        break;

    case BK_STATION :
        net_get_if_addr(&addr, net_get_sta_handle());
        net_get_if_macaddr(outNetpara->mac, net_get_sta_handle());
        break;

    default:
        ret = kGeneralErr;
        break;
    }

    if ( ret == kNoErr )
    {
        outNetpara->dhcp = addr.ipv4.addr_type;
        os_strcpy(outNetpara->ip, inet_ntoa(addr.ipv4.address));
        os_strcpy(outNetpara->mask, inet_ntoa(addr.ipv4.netmask));
        os_strcpy(outNetpara->gate, inet_ntoa(addr.ipv4.gw));
        os_strcpy(outNetpara->dns, inet_ntoa(addr.ipv4.dns1));
    }

    return ret;
}

OSStatus bk_wlan_get_link_status(LinkStatusTypeDef *outStatus)
{
    struct sm_get_bss_info_cfm *cfm;
    int ret;
    u8 vif_idx = 0, ssid_len;

    if( !sta_ip_is_start() )
    {
        return kGeneralErr;
    }
    
    outStatus->conn_state = mhdr_get_station_status();

    cfm = os_malloc(sizeof(struct sm_get_bss_info_cfm));
    if(!cfm)
    {
        return kNoMemoryErr;
    }

    vif_idx = rwm_mgmt_vif_mac2idx((void *)&g_sta_param_ptr->own_mac);
    ret = rw_msg_get_bss_info(vif_idx, (void *)cfm);
    if(ret)
    {
        os_free(cfm);
        return kGeneralErr;
    }
    
    outStatus->wifi_strength = cfm->rssi;
    outStatus->channel = rw_ieee80211_get_chan_id(cfm->freq);
    outStatus->security = g_sta_param_ptr->cipher_suite;
    os_memcpy(outStatus->bssid, cfm->bssid, 6);
    ssid_len = MIN(SSID_MAX_LEN, os_strlen(cfm->ssid));
    os_memcpy(outStatus->ssid, cfm->ssid, ssid_len);

    os_free(cfm);

    return kNoErr;
}

void bk_wlan_ap_para_info_get(network_InitTypeDef_ap_st *ap_info)
{
    IPStatusTypedef ap_ips;
    
    if((!ap_info)||(!uap_ip_is_start()))
    {
        return;
    }
    
    memcpy(ap_info->wifi_ssid,g_ap_param_ptr->ssid.array,g_ap_param_ptr->ssid.length);
    memcpy(ap_info->wifi_key,g_ap_param_ptr->key,g_ap_param_ptr->key_len);
    ap_info->channel = g_ap_param_ptr->chann;
    ap_info->security = g_ap_param_ptr->cipher_suite;
     
    bk_wlan_get_ip_status(&ap_ips,BK_SOFT_AP);
    memcpy(ap_info->local_ip_addr,ap_ips.ip,16);
    memcpy(ap_info->gateway_ip_addr,ap_ips.gate,16);
    memcpy(ap_info->net_mask,ap_ips.mask,16);
    memcpy(ap_info->dns_server_ip_addr,ap_ips.dns,16);

    ap_info->dhcp_mode = g_wlan_general_param->dhcp_enable;
}

uint32_t bk_wlan_get_INT_status(void)
{
    return platform_is_in_interrupt_context();
}

void bk_wlan_enable_lsig(void)
{
    hal_machw_allow_rx_rts_cts();
    phy_enable_lsig_intr();
}

void bk_wlan_disable_lsig(void)
{
    hal_machw_disallow_rx_rts_cts();
    phy_disable_lsig_intr();
}
/**
 * bit0: 1:mulicast_brdcast is not filte
 * bit1: 1:duplicate frame is not filte
**/
void bk_wlan_set_monitor_filter(unsigned char filter)
{
	g_monitor_is_not_filter = filter;
}

/** @brief  Start wifi monitor mode
 *
 *  @detail This function disconnect wifi station and softAP.
 *
 */
int bk_wlan_start_monitor(void)
{
	uint32_t ret;
	
	os_printf("bk_wlan_stop-ap\r\n");
    bk_wlan_stop(BK_SOFT_AP);
	
	os_printf("bk_wlan_stop-sta\r\n");
    bk_wlan_stop(BK_STATION);

#if (CFG_SUPPORT_ALIOS)
    lsig_init();
#endif

    bk_wlan_ap_init(0);
    rwnx_remove_added_interface();
	
	os_printf("mm_enter_monitor_mode\r\n");
	ret = mm_enter_monitor_mode();
	ASSERT(0 == ret);
	
    g_wlan_general_param->role = CONFIG_ROLE_NULL;

    return 0;
}

/** @brief  Stop wifi monitor mode **/
int bk_wlan_stop_monitor(void)
{
    if(g_monitor_cb)
    {
        g_monitor_cb = 0;
        hal_machw_exit_monitor_mode();
    }

    return 0;
}

int bk_wlan_get_channel(void)
{
    int channel, freq;

    freq = rw_msg_get_channel(NULL);
    channel = rw_ieee80211_get_chan_id(freq);

    return channel;
}

/** @brief  Set the monitor channel
 *
 *  @detail This function change the monitor channel (from 1~13).
 *       it can change the channel dynamically, don't need restart monitor mode.
 */
int bk_wlan_set_channel_sync(int channel)
{
    rwnxl_reset_evt(0);
    rw_msg_set_channel(channel, PHY_CHNL_BW_20, NULL);

    return 0;
}

int bk_wlan_get_channel_with_band_width(int *channel, int *band_width)
{
    struct phy_channel_info info;

    phy_get_channel(&info, 0);

    *channel = rw_ieee80211_get_chan_id(info.info2);
    *band_width = (info.info1 >> 8) & 0xFF;

    return 0;
}

int bk_wlan_set_channel_with_band_width(int channel, int band_width)
{
    rwnxl_reset_evt(0);
    rw_msg_set_channel((uint32_t)channel, (uint32_t)band_width, NULL);

    return 0;
}

/** @brief  Set channel at the asynchronous method
 *
 *  @detail This function change the monitor channel (from 1~13).
 *       it can change the channel dynamically, don't need restart monitor mode.
 */
int bk_wlan_set_channel(int channel)
{
	wifi_country_t country;
	GLOBAL_INT_DECLARATION();
	
    if(0 == channel)
    {
        channel = 1;
    }    
	
	rw_ieee80211_get_country(&country);
	if(channel < country.schan || channel > country.nchan)
	{
		return channel;
	}
	
	GLOBAL_INT_DISABLE();
	g_set_channel_postpone_num = channel;
	GLOBAL_INT_RESTORE();

    CHECK_NEED_WAKE_IF_STA_IN_SLEEP();
	ke_evt_set(KE_EVT_RESET_BIT);
    CHECK_NEED_WAKE_IF_STA_IN_SLEEP_END();
    
    return 0;
}

/** @brief  Register the monitor callback function
 *        Once received a 802.11 packet call the registered function to return the packet.
 */
void bk_wlan_register_monitor_cb(monitor_cb_t fn)
{
	if(fn)
	{
	    g_monitor_cb = fn;
	}
}

monitor_cb_t bk_wlan_get_monitor_cb(void)
{
    if (g_monitor_cb)
    {
#if (CFG_SUPPORT_ALIOS)
        return bk_monitor_callback;
#else
    	return g_monitor_cb;
#endif
    }
    else 
	{
        return NULL;
    }
}

int bk_wlan_monitor_filter(unsigned char filter_type)
{
	return ((g_monitor_is_not_filter&filter_type) == filter_type);
}

int bk_wlan_is_monitor_mode(void)
{
    return (0 == g_monitor_cb) ? false : true;
}

void wlan_ui_bcn_callback(uint8_t *data, int len, wifi_link_info_t *info)
{
#if CFG_USE_STA_PS
    if(power_save_if_ps_rf_dtim_enabled())
    {
        power_save_bcn_callback(data,len,info);
    }
#endif
#if CFG_USE_MCU_PS
    mcu_ps_bcn_callback(data,len,info);
#endif
}

void bk_wlan_register_bcn_cb(monitor_cb_t fn)
{
	g_bcn_cb = fn;
}

monitor_cb_t bk_wlan_get_bcn_cb(void)
{
    return g_bcn_cb;
}

extern void bmsg_ps_sender(uint8_t ioctl);

/** @brief  Request deep sleep,and wakeup by gpio.
 *
 *  @param  gpio_index_map:The gpio bitmap which set 1 enable wakeup deep sleep.
 *              gpio_index_map is hex and every bits is map to gpio0-gpio31.
 *          gpio_edge_map:The gpio edge bitmap for wakeup gpios,
 *              gpio_edge_map is hex and every bits is map to gpio0-gpio31.
 *              0:rising,1:falling.
 */
#if CFG_USE_DEEP_PS
void bk_enter_deep_sleep(UINT32 gpio_index_map,UINT32 gpio_edge_map)
{
    UINT32 param;
    UINT32 i;
    
    for (i = 0; i < GPIONUM; i++)
    {
        if (gpio_index_map & (0x01UL << i))
        {
            if(gpio_index_map & gpio_edge_map & (0x01UL << i))
            {
            	param = GPIO_CFG_PARAM(i, GMODE_INPUT_PULLUP);
            	sddev_control(GPIO_DEV_NAME, CMD_GPIO_CFG, &param); 
            }
            else
            {
                param = GPIO_CFG_PARAM(i, GMODE_INPUT_PULLDOWN);
                sddev_control(GPIO_DEV_NAME, CMD_GPIO_CFG, &param);  
            }
        }
    }

    deep_sleep_wakeup_with_gpio(gpio_index_map,gpio_edge_map);
}
#endif

#if CFG_USE_STA_PS
static uint32_t rf_ps_enabled = 0;
/** @brief  Enable dtim power save,close rf,and wakeup by ieee dtim dynamical
 *
 */
int bk_wlan_dtim_rf_ps_mode_enable(void )
{
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    rf_ps_enabled = 1;
    GLOBAL_INT_RESTORE();
    bmsg_ps_sender(PS_BMSG_IOCTL_RF_ENABLE);

    return 0;
}


/** @brief  When in dtim rf off mode,user can manual wakeup before dtim wakeup time.
 *          this function can not be called in "core_thread" context
 */
int bk_wlan_dtim_rf_ps_mode_do_wakeup()
{
    if(power_save_if_ps_rf_dtim_enabled()
            && power_save_if_rf_sleep())
    {
        power_save_rf_ps_wkup_semlist_wait();
    }

    return 0;
}

int bk_wlan_dtim_rf_ps_disable_send_msg(void)
{
    GLOBAL_INT_DECLARATION();
    PRINT_LR_REGISTER();
    os_printf(" %d: ",__LINE__);

    GLOBAL_INT_DISABLE();
    if(power_save_if_ps_rf_dtim_enabled()
            && power_save_if_rf_sleep())
    {
        power_save_wkup_event_set(NEED_DISABLE_BIT);
    }
    else
    {
        power_save_dtim_rf_ps_disable_send_msg();
    }
    
    GLOBAL_INT_RESTORE();
    return 0;
}

/** @brief  Request exit dtim dynamical ps mode 
 */
int bk_wlan_dtim_rf_ps_mode_disable(void)
{
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    rf_ps_enabled = 0;
    GLOBAL_INT_RESTORE();
    PRINT_LR_REGISTER();
    os_printf(" %d: ",__LINE__);

    bk_wlan_dtim_rf_ps_disable_send_msg();
    bk_wlan_dtim_rf_ps_timer_pause();
    return 0;
}

int bk_wlan_dtim_rf_ps_timer_start(void)
{
    bmsg_ps_sender(PS_BMSG_IOCTL_RF_PS_TIMER_INIT);
    return 0;
}

int bk_wlan_dtim_rf_ps_timer_pause(void)
{
    bmsg_ps_sender(PS_BMSG_IOCTL_RF_PS_TIMER_DEINIT);
    return 0;
}

UINT32 bk_wlan_dtim_rf_ps_get_sleep_time(void)
{
    return  power_save_get_rf_ps_dtim_time();
}

static void enter_ps_mode(void *parameter)
{
	rt_kprintf("------start enter ps mode---\r\n");
	bmsg_ps_sender(PS_BMSG_IOCTL_RF_ENABLE);
    bk_wlan_dtim_rf_ps_timer_start();
}
int auto_check_dtim_rf_ps_mode(void )
{
#if 0
	static rt_timer_t check_ps_timer = NULL;
    rt_kprintf("rf_ps_enabled:%d,check_ps_timer:%x\r\n",rf_ps_enabled,check_ps_timer);
    if(1 == rf_ps_enabled)
    {

    	if(check_ps_timer == NULL)
    	{
			check_ps_timer = rt_timer_create("ps_timer",enter_ps_mode, RT_NULL,2000,
                                     RT_TIMER_FLAG_SOFT_TIMER | RT_TIMER_FLAG_ONE_SHOT);
			if(check_ps_timer == NULL)
				rt_kprintf("create timer error!!\r\n");
    	}
		rt_timer_start(check_ps_timer);
    }
#else
	if(1 == rf_ps_enabled)
	{
		enter_ps_mode(NULL);
	}
#endif
    return 0;
}
#endif

int bk_wlan_mcu_suppress_and_sleep(UINT32 sleep_ticks )
{
#if CFG_USE_MCU_PS
	#if (CFG_OS_FREERTOS)
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    TickType_t missed_ticks = 0;  
	
    missed_ticks = mcu_power_save( sleep_ticks );    
    fclk_update_tick( missed_ticks );
    GLOBAL_INT_RESTORE();
	#endif
#endif
    return 0;
}

#if CFG_USE_MCU_PS
/** @brief  Enable mcu power save,close mcu ,and wakeup by irq
 */
int bk_wlan_mcu_ps_mode_enable(void)
{
    bmsg_ps_sender(PS_BMSG_IOCTL_MCU_ENABLE);

    return 0;
}

/** @brief  Disable mcu power save mode
 */
int bk_wlan_mcu_ps_mode_disable(void)
{
    bmsg_ps_sender(PS_BMSG_IOCTL_MCU_DISANABLE);

    return 0;
}
#endif

BK_PS_LEVEL global_ps_level = 0;
int bk_wlan_power_save_set_level(BK_PS_LEVEL level)
{
	PS_DEEP_CTRL_PARAM deep_sleep_param;
	deep_sleep_param.gpio_index_map = 0xc000;
	deep_sleep_param.gpio_edge_map  = 0x0;
	deep_sleep_param.wake_up_way = PS_DEEP_WAKEUP_GPIO;

    if(level & PS_DEEP_SLEEP_BIT)
    {
#if CFG_USE_STA_PS
        if(global_ps_level & PS_RF_SLEEP_BIT)
        {
            bk_wlan_dtim_rf_ps_mode_disable();
        }
#endif
#if CFG_USE_MCU_PS
        if(global_ps_level & PS_MCU_SLEEP_BIT)
        {
            bk_wlan_mcu_ps_mode_disable();
        }
#endif
        rtos_delay_milliseconds(100);
#if CFG_USE_DEEP_PS
        bk_enter_deep_sleep_mode(&deep_sleep_param);
#endif
    }

    if((global_ps_level & PS_RF_SLEEP_BIT) ^ (level & PS_RF_SLEEP_BIT))
    {
#if CFG_USE_STA_PS
        if(global_ps_level & PS_RF_SLEEP_BIT)
        {
            bk_wlan_dtim_rf_ps_mode_disable();
        }
        else
        {
            bk_wlan_dtim_rf_ps_mode_enable();
        }
#endif
    }

    if((global_ps_level & PS_MCU_SLEEP_BIT) ^ (level & PS_MCU_SLEEP_BIT))
    {
#if CFG_USE_MCU_PS
        if(global_ps_level & PS_MCU_SLEEP_BIT)
        {
            bk_wlan_mcu_ps_mode_disable();
        }
        else
        {
            bk_wlan_mcu_ps_mode_enable();
        }
#endif
    }

    global_ps_level = level;

    return 0;
}

void bk_wlan_status_register_cb(FUNC_1PARAM_PTR cb)
{
	connection_status_cb = cb;
}

FUNC_1PARAM_PTR bk_wlan_get_status_cb(void)
{
	return connection_status_cb;
}

OSStatus bk_wlan_ap_is_up(void)
{
#if CFG_ROLE_LAUNCH
    if(RL_STATUS_AP_INITING < rl_pre_ap_get_status())
    {
        return 1;
    }
#else
    ip_addr_t ip_addr;
    int result = 0;
    struct netif * netif = NULL;

    /* TODO: replace hardcoding with flexible way */
    netif = netif_find("ap1");
    if (!netif)
    {
        return -1;
    }

    ip_addr_set_zero(&ip_addr);
    if (ip_addr_cmp(&(netif->ip_addr), &ip_addr))
    {
        result = 0;
    }
    else
    {
        result = 1;
    }

    return result;
#endif

    return 0;
}

OSStatus bk_wlan_sta_is_connected(void)
{
#if CFG_ROLE_LAUNCH
    if(RL_STATUS_STA_LAUNCHED <= rl_pre_sta_get_status())
    {
        return 1;
    }
#else
    ip_addr_t ip_addr;
    int result = 0;
    struct netif * netif = NULL;

    /* TODO: replace hardcoding with flexible way */
    netif = netif_find("w00");
    if (!netif)
    {
        return -1;
    }

    ip_addr_set_zero(&ip_addr);
    if (ip_addr_cmp(&(netif->ip_addr), &ip_addr))
    {
        result = 0;
    }
    else
    {
        result = 1;
    }

    return result;
#endif

    return 0;
}

UINT32 if_other_mode_rf_sleep(void)
{
    if(!bk_wlan_has_role(VIF_AP)
        &&!bk_wlan_has_role(VIF_MESH_POINT)
        &&!bk_wlan_has_role(VIF_IBSS)
        &&!bk_wlan_is_monitor_mode())
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void del_vif()
{
    bk_wlan_stop(BK_SOFT_AP);
    bk_wlan_stop(BK_STATION);
}


UINT32 bk_wifi_has_up(void)
{
    if(!bk_wlan_has_role(VIF_AP)
        &&!bk_wlan_has_role(VIF_MESH_POINT)
        &&!bk_wlan_has_role(VIF_IBSS)
        &&!bk_wlan_is_monitor_mode()
        &&!bk_wlan_has_role(VIF_STA))
    {
        return 0;
    }
    else
        {
        os_printf("vif:%d %d %d %d %d\r\n",bk_wlan_has_role(VIF_AP),
            bk_wlan_has_role(VIF_MESH_POINT),bk_wlan_has_role(VIF_IBSS),
            bk_wlan_is_monitor_mode(),bk_wlan_has_role(VIF_STA));
        return 1;
    }
}

int bk_wlan_unconditional_rf_powerup(void)
{
    sddev_control(SCTRL_DEV_NAME, CMD_SCTRL_UNCONDITIONAL_RF_UP, 0);
    sddev_control(SCTRL_DEV_NAME, CMD_SCTRL_UNCONDITIONAL_MAC_UP, 0);
	return 0;
}

int bk_wlan_unconditional_rf_powerdown(void)
{
    if(if_other_mode_rf_sleep()
        &&(!bk_wlan_has_role(VIF_STA)))
    {
        sddev_control(SCTRL_DEV_NAME, CMD_SCTRL_UNCONDITIONAL_RF_DOWN, 0);
        sddev_control(SCTRL_DEV_NAME, CMD_SCTRL_UNCONDITIONAL_MAC_DOWN, 0);
    	return 0;
    }
    else
        {
        return -1;
    }
}

#if CFG_SUPPORT_ALIOS
/**********************for alios*******************************/
void bk_wifi_get_mac_address(char *mac)
{
	wifi_get_mac_address(mac, CONFIG_ROLE_STA);
}

void bk_wifi_set_mac_address(char *mac)
{
	wifi_set_mac_address(mac);
}

static int get_cipher_info(uint8_t *frame, int frame_len,
		uint8_t *pairwise_cipher_type)
{
	uint8_t cap = frame[24+10]; // 24 is mac header; 8 timestamp, 2 beacon interval; 
	u8 is_privacy = !!(cap & 0x10); // bit 4 = privacy
	const u8 *ie = frame + 36; // 24 + 12
	u16 ielen = frame_len - 36;
	const u8 *tmp;
	int ret = 0;

	tmp = (uint8_t *)mac_ie_find((uint32_t)ie, ielen, WLAN_EID_RSN);
	if (tmp && tmp[1]) {
		*pairwise_cipher_type = WLAN_ENC_CCMP;
	} else {
		tmp = mac_vendor_ie_find(OUI_MICROSOFT, OUI_TYPE_MICROSOFT_WPA, ie, ielen);
		if (tmp) {
			*pairwise_cipher_type = WLAN_ENC_TKIP;
		} else {
			if (is_privacy) {
				*pairwise_cipher_type = WLAN_ENC_WEP;
			} else {
				*pairwise_cipher_type = WLAN_ENC_OPEN;
			}
		}
	}

	return ret;
}

static void bk_monitor_callback(uint8_t *data, int len, wifi_link_info_t *info)
{
    uint8_t enc_type;

    /* check the RTS packet */
    if ((data[0] == 0xB4) && (len == 16)) { // RTS
        rts_update(data, info->rssi, rtos_get_time());
        return;
    }
    /* check beacon/probe reponse packet */
    if ((data[0] == 0x80) || (data[0] == 0x50)) {
        get_cipher_info(data, len, &enc_type);
        beacon_update(&data[16], enc_type);
    }
    
    if (g_monitor_cb)
    {
        (*g_monitor_cb)(data, len, info);
    }
}

static monitor_cb_t g_mgnt_cb = 0;
static  uint32_t umac_rx_filter = 0;

void bk_wlan_register_mgnt_monitor_cb(monitor_cb_t fn)
{
    g_mgnt_cb = fn;

	if (fn != NULL) {
		umac_rx_filter = mm_rx_filter_umac_get();
		mm_rx_filter_umac_set(umac_rx_filter | NXMAC_ACCEPT_PROBE_REQ_BIT);
	} else {
		if (umac_rx_filter != 0) {
			mm_rx_filter_umac_set(umac_rx_filter);
		}
	}
}

monitor_cb_t bk_wlan_get_mgnt_monitor_cb(void)
{
    return g_mgnt_cb;
}

int bk_wlan_send_80211_raw_frame(uint8_t *buffer, int len)
{
	uint8_t *pkt;
	int ret;

	pkt = os_malloc(len);
	if (pkt == NULL) 
	{
		return -1;
	}

	os_memcpy(pkt, buffer, len);
	ret = bmsg_tx_raw_sender(pkt, len);
	
	return ret;
}

int bk_wlan_suspend(void)
{
	bk_wlan_stop(BK_SOFT_AP);
	bk_wlan_stop(BK_STATION);
	
	return 0;
}

int bk_wlan_suspend_station(void)
{
	bk_wlan_stop(BK_STATION);
	
	return 0;
}

int bk_wlan_suspend_softap(void)
{
    bk_wlan_stop(BK_SOFT_AP);
	
	return 0;
}

uint32_t bk_wlan_max_power_level_get(void)
{
	return nxmac_max_power_level_get();
}

OSStatus bk_wlan_get_bssid_info(apinfo_adv_t *ap, uint8_t **key, int *key_len)
{
	LinkStatusTypeDef link_stat;

    if( uap_ip_is_start() )
    {
        return kGeneralErr;
    }

	ap->security = g_sta_param_ptr->cipher_suite;
	bk_wlan_get_link_status(&link_stat);
	ap->channel = link_stat.channel;
	os_memcpy(ap->bssid, link_stat.bssid, 6);
	os_strcpy(ap->ssid, link_stat.ssid);

	*key = g_sta_param_ptr->key;
	*key_len = g_sta_param_ptr->key_len;
	
	return 0;
}

#ifndef CONFIG_AOS_MESH
int wlan_is_mesh_monitor_mode(void)
{
    return false;
}

bool rxu_mesh_monitor(struct rx_swdesc *swdesc)
{
    return false;
}

monitor_cb_t wlan_get_mesh_monitor_cb(void)
{
    return NULL;
}
#else
void wlan_register_mesh_monitor_cb(monitor_cb_t fn)
{
    g_mesh_monitor_cb = fn;
}

monitor_cb_t wlan_get_mesh_monitor_cb(void)
{
    return g_mesh_monitor_cb;
}

int wlan_is_mesh_monitor_mode(void)
{
    if (g_mesh_monitor_cb) {
        return true;
    }
    return false;
}

int wlan_set_mesh_bssid(uint8_t *bssid)
{
    if (bssid == NULL) {
        return -1;
    }
    os_memcpy(g_mesh_bssid, bssid, 6);
    return 0;
}

uint8_t *wlan_get_mesh_bssid(void)
{
    return g_mesh_bssid;
}

bool rxu_mesh_monitor(struct rx_swdesc *swdesc)
{
    struct rx_dmadesc *dma_hdrdesc = swdesc->dma_hdrdesc;
    struct rx_hd *rhd = &dma_hdrdesc->hd;
    struct rx_payloaddesc *payl_d = HW2CPU(rhd->first_pbd_ptr);
    struct rx_cntrl_rx_status *rx_status = &rxu_cntrl_env.rx_status;
    uint32_t *frame = payl_d->buffer;
    struct mac_hdr *hdr = (struct mac_hdr *)frame;
    uint8_t *local_bssid;
    uint8_t *bssid;

    if (wlan_is_mesh_monitor_mode() == false) 
	{
        return false;
    }

    if(MAC_FCTRL_DATA_T == (hdr->fctl & MAC_FCTRL_TYPE_MASK)) 
	{
        local_bssid = wlan_get_mesh_bssid();
        bssid = (uint8_t *)hdr->addr3.array;
        if (os_memcmp(local_bssid, bssid, 6) == 0) {
            return true;
        }
    } 
	else if (MAC_FCTRL_ACK == (hdr->fctl & MAC_FCTRL_TYPESUBTYPE_MASK)) 
	{
        uint16_t local_addr[3];
        uint16_t *addr;
        uint32_t addr_low;
        uint16_t addr_high;

        addr = (uint16_t *)hdr->addr1.array;
        addr_low = nxmac_mac_addr_low_get();
        local_addr[0] = addr_low;
        local_addr[1] = (addr_low & 0xffff0000) >> 16;
        local_addr[2] = nxmac_mac_addr_high_getf();

        if (os_memcmp(local_addr, addr, 6) == 0) 
		{
            return true;
        }
    }

    return false;
}

int wlan_get_mesh_condition(void)
{
	int ret = 0;
	
	if(bk_wlan_has_role(VIF_STA) || bk_wlan_has_role(VIF_AP))
	{
		ret = 1;
	}
	
	return ret;
}
#endif
#endif
// eof

