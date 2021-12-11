#ifndef _SDIO_INTF_PUB_H_
#define _SDIO_INTF_PUB_H_

#include "sdio_pub.h"
#include "ke_msg.h"
#include "tx_swdesc.h"
#include "rwnx.h"

#define SDIO_INTF_FAILURE        ((UINT32)-1)
#define SDIO_INTF_SUCCESS        (0)

/*******************************************************************************
* Function Declarations
*******************************************************************************/
extern UINT32 sdio_intf_init(void);
extern void sdio_emb_rxed_evt(int dummy);
UINT32 outbound_upload_data(RW_RXIFO_PTR rx_info);
extern void inbound_cfm(struct tx_cfm_tag *cfm);
extern UINT32 sdio_emb_kmsg_fwd(struct ke_msg *msg);
extern void sdio_trans_evt(int dummy);
extern int sdio_trans_init(void);

#endif // _SDIO_INTF_PUB_H_


