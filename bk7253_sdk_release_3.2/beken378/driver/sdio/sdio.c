#include "include.h"
#include "arm_arch.h"

#include "sdio_pub.h"
#include "sdio.h"
#include "sutil.h"
#include "sdma_pub.h"
#include "intc_pub.h"

#include "drv_model_pub.h"
#include "uart_pub.h"
#include "icu_pub.h"
#include "mem_pub.h"

#include "co_math.h"
#include "gpio_pub.h"

#if CFG_CI_SDIO || CFG_SDIO || CFG_SDIO_TRANS

STATIC SDIO_S sdio;

STATIC DD_OPERATIONS sdio_op =
{
    sdio_open,
    sdio_close,
    sdio_read,
    sdio_write,
    sdio_ctrl
};


SDIO_NODE_PTR sdio_alloc_valid_node(UINT32 buf_size)
{
    UINT8 *buff_ptr;
    SDIO_NODE_PTR temp_node_ptr;
    SDIO_NODE_PTR mem_node_ptr = 0;

    if(0 == buf_size)
    {
        goto av_exit;
    }

    temp_node_ptr = su_pop_node(&sdio.free_nodes);
    if(temp_node_ptr)
    {
#ifdef SDIO_PREALLOC_BUF
		ASSERT(buf_size <= BLOCK_LEN);
		temp_node_ptr->addr   = (UINT8 *)((UINT32)temp_node_ptr->orig_addr + CFG_MSDU_RESV_HEAD_LEN);
		temp_node_ptr->length = buf_size;
		mem_node_ptr = temp_node_ptr;
#else /* SDIO_PREALLOC_BUF */
        buff_ptr = (UINT8 *)dtcm_malloc(CFG_MSDU_RESV_HEAD_LEN + buf_size
                                      + CFG_MSDU_RESV_TAIL_LEN
                                      + MALLOC_MAGIC_LEN);
        if(buff_ptr)
        {
            temp_node_ptr->orig_addr = buff_ptr;
            temp_node_ptr->addr   = (UINT8 *)((UINT32)buff_ptr + CFG_MSDU_RESV_HEAD_LEN);
            temp_node_ptr->length = buf_size;
            mem_node_ptr = temp_node_ptr;
        }
        else
        {
            fatal_prf("alloc_null:%d\r\n", buf_size);
            su_push_node(&sdio.free_nodes, temp_node_ptr);
        }
#endif /* SDIO_PREALLOC_BUF */
    }
    else
    {
        fatal_prf("NoNode\r\n");
    }

av_exit:
    return mem_node_ptr;
}

void sdio_free_valid_node(SDIO_NODE_PTR node_ptr)
{
#ifndef SDIO_PREALLOC_BUF
    os_free(node_ptr->orig_addr);
    node_ptr->orig_addr   = 0;
#endif
    node_ptr->addr   = 0;
    node_ptr->length = 0;

    su_push_node(&sdio.free_nodes, node_ptr);
}


static UINT8 null_data[8] = {0};

static void print_hex(const char *title, const unsigned char buf[], size_t len)
{
    SDIO_PRT("%s: ", title);

    for (size_t i = 0; i < len; i++)
        SDIO_PRT("%02X ", buf[i]);

    SDIO_PRT("\r\n");
}


volatile int sdio_debug_cnt = 0;

void sdio_cmd_handler(void *buf, UINT32 len)
{
    UINT32 count;
    UINT32 remain;
    SDIO_PTR sdio_ptr;
    SDIO_DCMD_PTR cmd_ptr;
    SDIO_CMD_PTR reg_cmd_ptr;
    SDIO_NODE_PTR mem_node_ptr = NULL;

    ASSERT(NULL != buf);
    ASSERT(buf == &sdio.cmd);
    cmd_ptr = (SDIO_DCMD_PTR)buf;
    reg_cmd_ptr = (SDIO_CMD_PTR)buf;

    sdma_fake_stop_dma();

    print_hex("sdio_cmd_handler: ", buf, len);

    sdio_ptr = &sdio;
    switch(cmd_ptr->op_code)
    {
    	/* Host -> Slave */
        case OPC_WR_DTCM :
        {
            sdio_ptr->rx_len = cmd_ptr->data_len;
            sdio_ptr->r_hdl_len = 0;

            count = su_get_node_count(&sdio.rxing_list);

            if(count)
            {
                mem_node_ptr = su_pop_node(&sdio.rxing_list);
            }
            else
            {
                mem_node_ptr = sdio_alloc_valid_node(cmd_ptr->data_len);
            }

            if(mem_node_ptr)
            {
                sdio_ptr->rx_status = RX_NODE_OK;
                remain = sdio_ptr->rx_len - sdio_ptr->r_hdl_len;
                sdio_ptr->rx_transaction_len = MIN(BLOCK_LEN, remain);
                su_push_node(&sdio.rxing_list, mem_node_ptr);
                sdma_start_rx(mem_node_ptr->addr, sdio_ptr->rx_transaction_len);
            }
            else
            {
                sdio_ptr->rx_status = RX_NO_NODE;
            }
            break;
        }


		/* Slave -> Host */
        case OPC_RD_DTCM :
        {
            if(su_get_node_count(&sdio.tx_dat) == 0)
                sdio_ctrl(SDIO_CMD_CLEAR_TX_VALID, 0);

            sdio_ptr->tx_len = cmd_ptr->data_len;
            sdio_ptr->t_hdl_len = 0;

            count = su_get_node_count(&sdio.txing_list);

            if(count)
            {
                mem_node_ptr = su_pop_node(&sdio.txing_list);
            }
            else
            {
                mem_node_ptr = su_pop_node(&sdio.tx_dat);
            }

            if(mem_node_ptr)
            {
                int _txlen;
                UINT8 *pp;
                sdio_ptr->tx_status = TX_NODE_OK;
                remain = sdio_ptr->tx_len - sdio_ptr->t_hdl_len;

                sdio_ptr->tx_transaction_len = MIN(BLOCK_LEN, remain);
                su_push_node(&sdio.txing_list, mem_node_ptr);

                sdma_start_tx(mem_node_ptr->addr, sdio_ptr->tx_transaction_len);
            }
            else
            {
                sdma_start_tx(null_data, sizeof(null_data));
                print_hex("TX2: ", null_data, sizeof(null_data));
                sdio_ptr->tx_status = TX_NO_NODE;
            }

            break;
        }

		/* Host -> Slave */
        case OPC_WR_REG :
        {
            UINT8 reg_numb = (reg_cmd_ptr->len - 4) >> 3;
            UINT8 i;
            UINT32 reg_addr, reg_val;
            for(i = 0; i < reg_numb; i++)
            {
                reg_addr = reg_cmd_ptr->content[i];
                reg_val = reg_cmd_ptr->content[i + reg_numb];
                *((UINT32 *)reg_addr) = reg_val;
            }

            break;
        }

		/* Slave -> Host */
        case OPC_RD_REG :
        {
            UINT8 reg_numb = (reg_cmd_ptr->len - 4) >> 2;
            UINT8 i;
            UINT32 reg_addr, reg_val[16];
            for(i = 0; i < reg_numb; i++)
            {
                reg_addr = reg_cmd_ptr->content[i];
                reg_val[i] = *((UINT32 *)reg_addr);
            }

            sdio_ptr->transaction_len = MIN(64, reg_numb << 2);
            sdma_start_tx((UINT8 *)reg_val, sdio_ptr->transaction_len);

            break;
        }

        default:
        {
            SDIO_WPRT("Exceptional cmd of sdio\r\n");
            break;
        }
    }
}

void sdio_tx_cb(void)
{
    UINT32 remain;
    SDIO_PTR sdio_ptr;
    SDIO_NODE_PTR mem_node_ptr;

    sdio_ptr = &sdio;
    SDIO_PRT("%s %d\r\n", __func__, __LINE__);
    sdio_ptr->t_hdl_len += sdio_ptr->tx_transaction_len;
    mem_node_ptr = su_pop_node(&sdio.txing_list);
    if((0 == mem_node_ptr)
            && (TX_NO_NODE == sdio_ptr->tx_status))
    {
        SDIO_PRT("tx_cb_no_node\r\n");
        sdio_ptr->tx_status = TX_NODE_OK;
        return;
    }

    ASSERT(mem_node_ptr);

    if(mem_node_ptr->callback)
    {
        (mem_node_ptr->callback)(mem_node_ptr->Lparam, mem_node_ptr->Rparam);
    }

    mem_node_ptr->callback = NULLPTR;
    mem_node_ptr->Lparam   = NULL;
    mem_node_ptr->Rparam   = NULL;

    sdio_free_valid_node(mem_node_ptr);

    remain = sdio_ptr->tx_len - sdio_ptr->t_hdl_len;
    if(remain)
    {
        ASSERT(0);
#if 0
        mem_node_ptr = su_pop_node(&sdio.tx_dat);
        if(mem_node_ptr)
        {
            sdio_ptr->tx_transaction_len = MIN(BLOCK_LEN, remain);
            su_push_node(&sdio.txing_list, mem_node_ptr);
            SDIO_PRT("%s %d\r\n", __func__, __LINE__);
            sdma_start_tx(mem_node_ptr->addr, sdio_ptr->tx_transaction_len);
            print_hex_dump("TX0: ", mem_node_ptr->addr,
                    sdio_ptr->tx_transaction_len < 16 ? sdio_ptr->tx_transaction_len : 16);
        }
        else
        {
        }
#endif
    }
    else
    {
        /*sdma_start_cmd*/
    }

}


void sdio_rx_cb(UINT32 count)
{
    UINT32 remain;
    SDIO_PTR sdio_ptr;
    SDIO_NODE_PTR mem_node_ptr;

    sdio_ptr = &sdio;

	SDIO_PRT("%s: count %d\r\n", __func__, count);
    sdio_ptr->r_hdl_len += sdio_ptr->rx_transaction_len;
    mem_node_ptr = su_pop_node(&sdio.rxing_list);
    if((0 == mem_node_ptr)
            && (RX_NO_NODE == sdio_ptr->rx_status))
    {
        SDIO_WPRT("rx_cb_no_node\r\n");
        sdio_ptr->rx_status = RX_NODE_OK;
        return;
    }

    SDIO_PRT("rx_cb\r\n");
    ASSERT(mem_node_ptr);


    su_push_node(&sdio.rx_dat, mem_node_ptr);
    print_hex("RX: ", mem_node_ptr->addr, mem_node_ptr->length > 16 ? 16 : mem_node_ptr->length);

    if(sdio_ptr->rx_cb)
    {
        (*(sdio_ptr->rx_cb))();
    }

    remain = sdio_ptr->rx_len - sdio_ptr->r_hdl_len;
    if(remain)
    {
        ASSERT(0);
#if 0
        sdio_ptr->rx_transaction_len = MIN(BLOCK_LEN, remain);
        mem_node_ptr = sdio_alloc_valid_node(sdio_ptr->rx_transaction_len);
        if(mem_node_ptr)
        {
            su_push_node(&sdio.rxing_list, mem_node_ptr);
            sdma_start_rx(mem_node_ptr->addr, sdio_ptr->rx_transaction_len);
        }
        else
        {
        }
#endif
    }
    else
    {
        /*sdma_start_cmd*/

    }
}

void sdio_register_rx_cb(FUNCPTR func)
{
    sdio.rx_cb = func;
}

static void sdio_intfer_gpio_config(void)
{
	UINT32 param;

	//param = GFUNC_MODE_SD_DMA;
	param = GFUNC_MODE_SD1_DMA;

	sddev_control(GPIO_DEV_NAME, CMD_GPIO_ENABLE_SECOND, &param);
}


void sdio_init(void)
{
    sdio_intfer_gpio_config();

	os_memset(&sdio, 0, sizeof(sdio));
    INIT_LIST_HEAD(&sdio.tx_dat);
    INIT_LIST_HEAD(&sdio.rx_dat);
    INIT_LIST_HEAD(&sdio.txing_list);
    INIT_LIST_HEAD(&sdio.rxing_list);

    su_init(&sdio);

    sdma_register_handler(sdio_tx_cb, sdio_rx_cb, sdio_cmd_handler);
    sdma_init();

    ddev_register_dev(SDIO_DEV_NAME, &sdio_op);
}


void sdio_exit(void)
{
    sdma_uninit();

    ddev_unregister_dev(SDIO_DEV_NAME);
}

UINT32 sdio_open(UINT32 op_flag)
{
    UINT32 reg;

    sdma_open();
    sdma_start_cmd((UINT8 *)&sdio.cmd, sizeof(sdio.cmd));

    reg = *((volatile UINT32 *)((0x00802000 + 16 * 4)));
    if(reg & (1 << FIQ_SDIO_DMA))
    {
        ASSERT(0);
    }

    intc_enable(FIQ_SDIO_DMA);
    SDIO_PRT("XXXXXXXXXXx sdio_open XXXXXXXX \r\n");

    return SDIO_SUCCESS;
}

UINT32 sdio_close(void)
{
    sdma_close();

    return SDIO_SUCCESS;
}

UINT32 sdio_read(char *user_buf, UINT32 count, UINT32 op_flag)
{
    UINT32 ret;
    UINT32 len;
    SDIO_NODE_PTR mem_node_ptr;

    ret = SDIO_FAILURE;

    peri_busy_count_add();

    if(S2H_RD_SYNC == op_flag)
    {
        if((NULLPTR == user_buf)
                || (0 == count))
        {
            goto rd_exit;
        }

        mem_node_ptr = su_pop_node(&sdio.tx_dat);

        if(mem_node_ptr)
        {
            len = MIN(count, mem_node_ptr->length);
            os_memcpy(user_buf, mem_node_ptr->addr, len);
            ret = mem_node_ptr->length;

            sdio_free_valid_node(mem_node_ptr);
        }
        else
        {
            ret = SDIO_FAILURE;
        }
    }

	/* pop node from rx_dat, and copy to @user_buf */
    else if(H2S_RD_SYNC == op_flag)
    {
        if((NULLPTR == user_buf)
                || (0 == count))
        {
            goto rd_exit;
        }

        mem_node_ptr = su_pop_node(&sdio.rx_dat);
        if(mem_node_ptr)
        {
            len = MIN(count, mem_node_ptr->length);
            os_memcpy(user_buf, mem_node_ptr->addr, len);
            ret = mem_node_ptr->length;

            sdio_free_valid_node(mem_node_ptr);
        }
        else
        {
            ret = SDIO_FAILURE;
        }
    }

	/* pop node from rx_dat */
    else if(H2S_RD_SPECIAL == op_flag)
    {
        mem_node_ptr = su_pop_node(&sdio.rx_dat);
        if(mem_node_ptr)
        {
            mem_node_ptr->Lparam = &sdio.free_nodes;
            mem_node_ptr->Rparam = mem_node_ptr;

            ret = (UINT32)mem_node_ptr;
        }
        else
        {
            ret = SDIO_FAILURE;
        }
    }

    peri_busy_count_dec();

rd_exit:
    return ret;
}

//volatile int write_debug = 0;

UINT32 sdio_write(char *user_buf, UINT32 count, UINT32 op_flag)
{
    UINT32 ret;
    SDIO_NODE_PTR mem_node_ptr;

    ret = SDIO_FAILURE;

	//if (write_debug)
	//	return 0;

    peri_busy_count_add();

	/* allocate a SDIO_NODE_T, and copy @user_buf to SDIO_NODE_T's internal buffer, then enqueue this node */
    if(S2H_WR_SYNC == op_flag)
    {
        if((NULLPTR == user_buf)
                || (0 == count))
        {
            goto exit_wr;
        }

        mem_node_ptr = sdio_alloc_valid_node(count);
        if(mem_node_ptr)
        {
            os_memcpy(mem_node_ptr->addr, user_buf, count);
            su_push_node(&sdio.tx_dat, mem_node_ptr);
            sdio_ctrl(SDIO_CMD_SET_TX_VALID, 0);

            ret = SDIO_SUCCESS;
        }
        else
        {
            ret = SDIO_FAILURE;
        }
    }

	/* @user_buf is a SDIO_NODE_T{}, just enqueue it */
    else  if(S2H_WR_SPECIAL == op_flag)
    {
        if((NULLPTR == user_buf)
                || (0 == count))
        {
            goto exit_wr;
        }

        mem_node_ptr = (SDIO_NODE_PTR)user_buf;
        su_push_node(&sdio.tx_dat, mem_node_ptr);

        sdio_ctrl(SDIO_CMD_SET_TX_VALID, 0);
        ret = SDIO_SUCCESS;
    }

	/*  */
    else if(H2S_WR_SYNC == op_flag)
    {
        if((NULLPTR == user_buf)
                || (0 == count))
        {
            goto exit_wr;
        }

        mem_node_ptr = sdio_alloc_valid_node(count);
        if(mem_node_ptr)
        {
            os_memcpy(mem_node_ptr->addr, user_buf, count);
            su_push_node(&sdio.rx_dat, mem_node_ptr);

            ret = SDIO_SUCCESS;
        }
        else
        {
            ret = SDIO_FAILURE;
        }
    }

    peri_busy_count_dec();

exit_wr:
    return ret;
}

UINT32 sdio_ctrl(UINT32 cmd, void *param)
{
    UINT32 ret;
    SDIO_NODE_PTR mem_node_ptr;

    ret = SDIO_SUCCESS;

    peri_busy_count_add();

    switch(cmd)
    {
    case SDIO_CMD_PUSH_FREE_NODE:
        mem_node_ptr = (SDIO_NODE_PTR)param;

        sdio_free_valid_node(mem_node_ptr);
        break;

    case SDIO_CMD_GET_FREE_NODE:
        mem_node_ptr = sdio_alloc_valid_node(*(UINT32 *)param);

        ret = (UINT32)mem_node_ptr;
        break;

    case SDIO_CMD_REG_RX_CALLBACK:
        sdio_register_rx_cb((FUNCPTR)param);
        break;

    case SDIO_CMD_GET_CNT_FREE_NODE:
        *((UINT32 *)param) = su_get_node_count(&sdio.free_nodes);
        break;

    case SDIO_CMD_CLEAR_TX_VALID:
		SDIO_PRT("CLEAR_TX_VALID\r\n");
        sdma_clr_tx_valid();
        break;

    case SDIO_CMD_PEEK_S2H_COUNT:
        ret = su_get_node_count(&sdio.tx_dat);
        break;

    case SDIO_CMD_PEEK_H2S_COUNT:
        ret = su_get_node_count(&sdio.rx_dat);
        break;

    case SDIO_CMD_SET_TX_VALID:
		SDIO_PRT("SET_TX_VALID\r\n");
        sdma_set_tx_valid();
        break;

    default:
        break;
    }

    peri_busy_count_dec();

    return ret;
}
#endif
