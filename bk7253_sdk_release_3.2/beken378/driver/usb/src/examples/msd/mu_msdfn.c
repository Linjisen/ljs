/******************************************************************
*                                                                *
*        Copyright Mentor Graphics Corporation 2005              *
*                                                                *
*                All Rights Reserved.                            *
*                                                                *
*    THIS WORK CONTAINS TRADE SECRET AND PROPRIETARY INFORMATION *
*  WHICH IS THE PROPERTY OF MENTOR GRAPHICS CORPORATION OR ITS   *
*  LICENSORS AND IS SUBJECT TO LICENSE TERMS.                    *
*                                                                *
******************************************************************/
#include "include.h"

#include "mu_arch.h"
#include "mu_cdi.h"
#include "mu_diag.h"
#include "mu_impl.h"
#include "mu_mem.h"
#include "class/mu_msd.h"
#include "class/mu_bot.h"
#include "class/mu_scsi.h"
#include "sdcard.h"
/**
 * You may define this if your core is configured for a bidirectional endpoint 1
 */
//#define MGC_MSD_BIDIR_ENDPOINTS

/**
 * MGC_MsdCommandUsage.
 * For table lookup of command usage data
 * @field bCommand command
 * @field bDataSize size of buffer
 * @field pData buffer
 */
typedef struct
{
    uint8_t bCommand;
    uint8_t bDataSize;
    const uint8_t *pData;
} MGC_MsdCommandUsage;

#include "mu_pkon.h"
/**
 * MGC_MsdRbcParams.
 * For static declaration of RBC mode parameters page
 */
typedef struct
{
    const MGC_MsdModeParamHeader6 Header6;
    MGC_MsdRbcDeviceParams RbcParams;
} MGC_MsdRbcParams;

#if CFG_SUPPORT_MSD

#include "mu_pkoff.h"
/******************************************************************
Forwards
******************************************************************/
static uint8_t MGC_MsdDeviceRequest(void *hClient, MUSB_BusHandle hBus,
                                    uint32_t dwSequenceNumber,
                                    const uint8_t *pSetup,
                                    uint16_t wRequestLength);
static uint8_t MGC_MsdDeviceConfigSelected(void *hClient,
        MUSB_BusHandle hBus,
        uint8_t bConfigurationValue,
        MUSB_PipePtr *ahPipe);
static void MGC_MsdNewUsbState(void *hClient, MUSB_BusHandle hBus,
                               MUSB_State State);

static uint32_t MGC_MsdRxCbwComplete(void *pCompleteParam, MUSB_Irp *pIrp);
static uint32_t MGC_MsdRxDataComplete(void *pCompleteParam, MUSB_Irp *pIrp);
static uint32_t MGC_MsdTxDataComplete(void *pCompleteParam, MUSB_Irp *pIrp);
static uint32_t MGC_MsdTxCswComplete(void *pCompleteParam, MUSB_Irp *pIrp);
static uint32_t MGC_MsdReadData(void);
static uint32_t Multi_Block_MGC_MsdReadData(void);
extern void MGC_Start_Csw_Tx(void);
extern void MGC_Start_Data_Rx(uint8_t *data);
extern void Multi_Block_MGC_Start_Data_Rx(uint8_t *data);
extern int sdcard_get_size( void );
extern int sdcard_get_block_size( void );
extern uint8_t usb_get_buffer_address(void);

extern UINT32 sdcard_read_new(int first_block, int block_num, uint8 *dest);
extern UINT32 sdcard_write_new(int first_block, int block_num, uint8_t *data);
/******************************************************************
Globals
******************************************************************/

/* UCDI variables */
static uint8_t MGC_bMsdSelfPowered = TRUE;
static MUSB_State MGC_eMsdUsbState = MUSB_POWER_OFF;
static uint8_t MGC_aControlData[64];//[256];
static uint8_t first_read=0;
static const uint8_t MGC_aDescriptorData[] =
{
    /* Device Descriptor */
    0x12,                      /* bLength              */
    MUSB_DT_DEVICE,            /* DEVICE               */
    0x10,0x01,                 /* USB 1.1              */
    0x00,                      /* CLASS                */
    0x00,                      /* Subclass             */
    0x00,                      /* Protocol             */
    0x40,                      /* bMaxPktSize0         */
    0xD1, 0x11,                /* idVendor             */
    0x31, 0x31,                /* idProduct            */
    0x00, 0x02,                /* bcdDevice            */
    0x01,                      /* iManufacturer        */
    0x02,                      /* iProduct             */
    0x03,                      /* iSerial Number       */
    0x01,                      /* One configuration    */

    /* strings */
    2 + 4,
    MUSB_DT_STRING,
    0x09, 0x04,			/* English (U.S.) */
    0x09, 0x0c,			/* English (Australia) */

    /* TODO: make tool to generate strings and eventually whole descriptor! */
    /* English (U.S.) strings */
    2+30,            /* Manufacturer: Beken Corporation */
    MUSB_DT_STRING,
    'B', 0, 'e', 0, 'k', 0, 'e', 0, 'n', 0, ' ', 0, ' ', 0,
    'C', 0, 'o', 0, 'r', 0, 'p', 0, 'o', 0, 'r', 0, 'a', 0, '.', 0,

    2 + 8,			/* Product ID: Demo */
    MUSB_DT_STRING,
    'D', 0, 'e', 0, 'm', 0, 'o', 0,

    2 + 24,			/* Serial #: 123412341234 */
    MUSB_DT_STRING,
    '1', 0, '2', 0, '3', 0, '4', 0,
    '1', 0, '2', 0, '3', 0, '4', 0,
    '1', 0, '2', 0, '3', 0, '4', 0,

    /* English (Australia) strings */
    2 + 30,         /* Manufacturer: Beken Corporation */
    MUSB_DT_STRING,
    'B', 0, 'e', 0, 'k', 0, 'e', 0, 'n', 0, ' ', 0, ' ', 0,
    'C', 0, 'o', 0, 'r', 0, 'p', 0, 'o', 0, 'r', 0, 'a', 0, '.', 0,

    2+22,			/* Product ID: Card Reader*/
    MUSB_DT_STRING,
    'C', 0, 'a', 0, 'r', 0, 'd', 0, ' ', 0, 'R', 0, 'e', 0, 'a', 0, 'd', 0, 'e', 0, 'r', 0,

    2 + 24,			/* Serial #: 123412341234 */
    MUSB_DT_STRING,
    '1', 0, '2', 0, '3', 0, '4', 0,
    '1', 0, '2', 0, '3', 0, '4', 0,
    '1', 0, '2', 0, '3', 0, '4', 0,

    /* configuration */
    0x09,                                   /* bLength              */
    MUSB_DT_CONFIG,                         /* CONFIGURATION        */
    0x20,                                   /* length               */
    0x00,                                   /* length               */
    0x01,                                   /* bNumInterfaces       */
    0x01,                                   /* bConfigurationValue  */
    0x00,                                   /* iConfiguration       */
    0xC0,                                   /* bmAttributes (required + self-powered) */
    0x32,                                   /* power                */

    /* interface */
    0x09,                                   /* bLength              */
    MUSB_DT_INTERFACE,                      /* INTERFACE            */
    0x00,                                   /* bInterfaceNumber     */
    0x00,                                   /* bAlternateSetting    */
    0x02,                                   /* bNumEndpoints        */
    0x08,                                   /* bInterfaceClass      */
    0x06,                                   /* bInterfaceSubClass (1=RBC, 6=SCSI) */
    0x50,                                   /* bInterfaceProtocol (BOT) */
    0x00,                                   /* iInterface           */

    /* Endpoint Descriptor  : Bulk-In */
    0x07,                                   /* bLength              */
    MUSB_DT_ENDPOINT,                       /* ENDPOINT             */
    0x81,                                   /* bEndpointAddress     */
    0x02,                                   /* bmAttributes         */
    0x40, 0x00,                             /* wMaxPacketSize       */
    0x00,                                   /* bInterval            */

    /* Endpoint Descriptor  : Bulk-Out */
    0x07,                                   /* bLength              */
    MUSB_DT_ENDPOINT,                       /* ENDPOINT             */
//#ifdef MGC_MSD_BIDIR_ENDPOINTS
//    0x01,                                   /* bEndpointAddress     */
//#else
    0x02,                                   /* bEndpointAddress     */
//#endif
    0x02,                                   /* bmAttributes         */
    0x40, 0x00,                             /* wMaxPacketSize       */
    0x00,                                   /* bInterval            */
};

static const uint8_t MGC_aHighSpeedDescriptorData[] =
{
    /* device qualifier */
    0x0a,                      /* bLength              */
    MUSB_DT_DEVICE_QUALIFIER,  /* DEVICE Qualifier     */
    0x01,0x01,                 /* USB 1.1              */
    0,                         /* CLASS                */
    0,                         /* Subclass             */
    0x00,                      /* Protocol             */
    0x40,                      /* bMaxPacketSize0      */
    0x01,                      /* One configuration    */
    0x00,                      /* bReserved            */

    /* configuration */
    0x09,                                   /* bLength              */
    MUSB_DT_OTHER_SPEED,                    /* CONFIGURATION        */
    0x20,                                   /* length               */
    0x0,                                    /* length               */
    0x01,                                   /* bNumInterfaces       */
    0x01,                                   /* bConfigurationValue  */
    0x00,                                   /* iConfiguration       */
    0xC0,                                   /* bmAttributes (required + self-powered) */
    0x32,                                   /* power                */

    /* interface */
    0x09,                                   /* bLength              */
    MUSB_DT_INTERFACE,                      /* INTERFACE            */
    0x0,                                    /* bInterfaceNumber     */
    0x0,                                    /* bAlternateSetting    */
    0x02,                                   /* bNumEndpoints        */
    0x08,                                   /* bInterfaceClass      */
    0x06,                                   /* bInterfaceSubClass (1=RBC, 6=SCSI) */
    0x50,                                   /* bInterfaceProtocol (BOT) */
    0x00,                                   /* iInterface           */

    /* Endpoint Descriptor  : Bulk-In */
    0x07,                                   /* bLength              */
    MUSB_DT_ENDPOINT,                       /* ENDPOINT             */
    0x81,                                   /* bEndpointAddress     */
    0x02,                                   /* bmAttributes         */
    0x40, 0x00,                             /* wMaxPacketSize       */
    0x01,                                   /* bInterval            */

    /* Endpoint Descriptor  : Bulk-Out */
    0x07,                                   /* bLength              */
    MUSB_DT_ENDPOINT,                       /* ENDPOINT             */
//#ifdef MGC_MSD_BIDIR_ENDPOINTS
//    0x01,                                   /* bEndpointAddress     */
//#else
    0x02,                                   /* bEndpointAddress     */
//#endif
    0x02,                                   /* bmAttributes         */
    0x40, 0x00,                             /* wMaxPacketSize       */
    0x01,                                   /* bInterval            */
};

static const MGC_MsdStandardInquiryData MGC_MsdRbcInquiryData =
{
    MGC_SCSI_DEVICE_TYPE_DIRECT,
    0x80,   /* removable (though this doesn't seem so) */
    0,	    /* 0=no comformance to any standard */
    0,	    /* 2=required response data format */
    0x1f,   /* extra length */
    {0, 0, 0},   
    { 'B', 'e', 'k', 'e', 'n', ' ', ' ', ' ' },
    { 'D' ,'e', 'm', 'o', ' ', 'D', 'i', 's', 'k', ' ', ' ', ' ', ' ', ' ', ' ', ' ' },
    { '1', ' ', ' ', ' ' }
};

static const uint8_t MGC_MsdSupportedVpdPagesData[] =
{
    MGC_SCSI_DEVICE_TYPE_DIRECT,   /* device type */
    MGC_SCSI_PAGE_SUPPORTED_VPD,    /* page code */
    0,	    /* reserved */
    3,	    /* length of: */
    MGC_SCSI_PAGE_SUPPORTED_VPD,
    MGC_SCSI_PAGE_UNIT_SERIAL_NUM,
    MGC_SCSI_PAGE_DEVICE_ID
};

static const uint8_t MGC_MsdUnitSerialNumberPageData[] =
{
    MGC_SCSI_DEVICE_TYPE_DIRECT,   /* device type */
    MGC_SCSI_PAGE_UNIT_SERIAL_NUM,  /* page code */
    0,	    /* reserved */
    4,	    /* length of: */
    '1', '2', 'f', 'e'
};

static const uint8_t MGC_MsdDeviceIdPageData[] =
{
    MGC_SCSI_DEVICE_TYPE_DIRECT,   /* device type */
    MGC_SCSI_PAGE_DEVICE_ID,	    /* page code */
    0,	    /* reserved */
    0,	    /* length of: */
};

static const uint8_t MGC_aUnsupportedCommandData[] =
{
    MGC_SCSI_DEVICE_TYPE_DIRECT,   /* device type */
    1,	    /* unsupported */
    0,
    0,
    0,
    0
};

static const uint8_t MGC_aRead10Support[] =
{
    MGC_SCSI_DEVICE_TYPE_DIRECT,   /* device type */
    5,	    /* supported, vendor-specific */
    0,	    /* no compliance claimed */
    0, 0,
    10,
    MGC_SCSI_READ10,
    0,
    0xff, 0xff, 0xff, 0xff,
    0,
    0xff, 0xff,
    0
};

static const uint8_t MGC_aWrite10Support[] =
{
    MGC_SCSI_DEVICE_TYPE_DIRECT,   /* device type */
    5,	    /* supported, vendor-specific */
    0,	    /* no compliance claimed */
    0, 0,
    10,
    MGC_SCSI_WRITE10,
    0,
    0xff, 0xff, 0xff, 0xff,
    0,
    0xff, 0xff,
    0
};

static const uint8_t MGC_aVerifySupport[] =
{
    MGC_SCSI_DEVICE_TYPE_DIRECT,   /* device type */
    5,	    /* supported, vendor-specific */
    0,	    /* no compliance claimed */
    0, 0,
    10,
    MGC_SCSI_VERIFY,
    0,
    0xff, 0xff, 0xff, 0xff,
    0,
    0xff, 0xff,
    0
};

static const uint8_t MGC_aWriteBufferSupport[] =
{
    MGC_SCSI_DEVICE_TYPE_DIRECT,   /* device type */
    5,	    /* supported, vendor-specific */
    0,	    /* no compliance claimed */
    0, 0,
    10,
    MGC_SCSI_WRITE_BUFFER,
    7,
    0xff, 0xff, 0xff, 0xff,
    0,
    0xff, 0xff,
    0
};

static const uint8_t MGC_aReadCapacitySupport[] =
{
    MGC_SCSI_DEVICE_TYPE_DIRECT,   /* device type */
    5,	    /* supported, vendor-specific */
    0,	    /* no compliance claimed */
    0, 0,
    10,
    MGC_SCSI_READ_CAPACITY,
    0,
    0xff, 0xff, 0xff, 0xff,
    0,
    0xff, 0xff,
    0
};

static const uint8_t MGC_aTestUnitReadySupport[] =
{
    MGC_SCSI_DEVICE_TYPE_DIRECT,   /* device type */
    5,	    /* supported, vendor-specific */
    0,	    /* no compliance claimed */
    0, 0,
    6,
    MGC_SCSI_TEST_UNIT_READY,
    0,
    0, 0,
    0xff,
    0
};

static const uint8_t MGC_aRequestSenseSupport[] =
{
    MGC_SCSI_DEVICE_TYPE_DIRECT,   /* device type */
    5,	    /* supported, vendor-specific */
    0,	    /* no compliance claimed */
    0, 0,
    6,
    MGC_SCSI_REQUEST_SENSE,
    0,
    0, 0,
    0xff,
    0
};

static const uint8_t MGC_aInquirySupport[] =
{
    MGC_SCSI_DEVICE_TYPE_DIRECT,   /* device type */
    5,	    /* supported, vendor-specific */
    0,	    /* no compliance claimed */
    0, 0,
    6,
    MGC_SCSI_INQUIRY,
    3,
    0xff,
    0,
    0xff,
    0
};

static const uint8_t MGC_aModeSenseSupport[] =
{
    MGC_SCSI_DEVICE_TYPE_DIRECT,   /* device type */
    5,	    /* supported, vendor-specific */
    0,	    /* no compliance claimed */
    0, 0,
    6,
    MGC_SCSI_MODE_SENSE,
    8,
    0xff,
    0,
    0xff,
    0
};

static const uint8_t MGC_aModeSelectSupport[] =
{
    MGC_SCSI_DEVICE_TYPE_DIRECT,   /* device type */
    5,	    /* supported, vendor-specific */
    0,	    /* no compliance claimed */
    0, 0,
    6,
    MGC_SCSI_MODE_SELECT,
    0x11,
    0,
    0,
    0xff,
    0
};

static const uint8_t MGC_aStartStopUnitSupport[] =
{
    MGC_SCSI_DEVICE_TYPE_DIRECT,   /* device type */
    5,	    /* supported, vendor-specific */
    0,	    /* no compliance claimed */
    0, 0,
    6,
    MGC_SCSI_START_STOP_UNIT,
    0x01,
    0,
    0,
    0xf3,
    0
};

static const MGC_MsdCommandUsage MGC_aCommandUsageData[] =
{
    { MGC_SCSI_READ10, (uint8_t)sizeof(MGC_aRead10Support), MGC_aRead10Support },
    { MGC_SCSI_WRITE10, (uint8_t)sizeof(MGC_aWrite10Support), MGC_aWrite10Support },
    { MGC_SCSI_VERIFY, (uint8_t)sizeof(MGC_aVerifySupport), MGC_aVerifySupport },
    { MGC_SCSI_WRITE_BUFFER, (uint8_t)sizeof(MGC_aWriteBufferSupport), MGC_aWriteBufferSupport },
    { MGC_SCSI_READ_CAPACITY, (uint8_t)sizeof(MGC_aReadCapacitySupport), MGC_aReadCapacitySupport },
    { MGC_SCSI_TEST_UNIT_READY, (uint8_t)sizeof(MGC_aTestUnitReadySupport), MGC_aTestUnitReadySupport },
    { MGC_SCSI_REQUEST_SENSE, (uint8_t)sizeof(MGC_aRequestSenseSupport), MGC_aRequestSenseSupport },
    { MGC_SCSI_INQUIRY, (uint8_t)sizeof(MGC_aInquirySupport), MGC_aInquirySupport },
    { MGC_SCSI_MODE_SENSE, (uint8_t)sizeof(MGC_aModeSenseSupport), MGC_aModeSenseSupport },
    { MGC_SCSI_MODE_SELECT, (uint8_t)sizeof(MGC_aModeSelectSupport), MGC_aModeSelectSupport },
    { MGC_SCSI_START_STOP_UNIT, (uint8_t)sizeof(MGC_aStartStopUnitSupport), MGC_aStartStopUnitSupport },

};

static uint8_t MGC_bIsReady = TRUE;
static uint8_t MGC_bMsdMaxLun = 0;
static uint8_t MGC_bMsdInterface = 0;
static uint16_t MGC_wBlockSize = 512;
static uint32_t MGC_dwRxCount;
static uint32_t MGC_dwGoodCmdCount = 0;
static uint8_t MGC_bMsdLastCommand;

/** Current sense data */
static MGC_MsdScsiSenseData MGC_MsdSenseData;

/** mode sense response data */
static MGC_MsdScsiModeSenseData MGC_MsdModeSenseData;

/** Capacity response */
static MGC_MsdCapacityResponse MGC_MsdCapacity;
static MGC_MsdFormatCapacityResponse MGC_MsdFormatCapacity;

/** Current CBW */
static MGC_MsdCbw MGC_Cbw;

/** Current CSW */
static MGC_MsdCsw MGC_Csw;

static uint8_t MGC_aJunk[512];//[512];

/** IRP for CBW reception */
static MUSB_Irp MGC_MsdRxCbwIrp =
{
    NULL,
    (uint8_t *) &MGC_Cbw,
    sizeof(MGC_Cbw),
    0,
    0,
    MGC_MsdRxCbwComplete,
    NULL,
    FALSE,	/* bAllowShortTransfer */
    TRUE,	/* bIsrCallback */
    FALSE	/* bAllowDma */
};

/** IRP for data reception */
static MUSB_Irp MGC_MsdRxDataIrp =
{
    NULL,
    NULL,
    0,
    0,
    0,
    MGC_MsdRxDataComplete,
    NULL,
    FALSE,	/* bAllowShortTransfer */
    TRUE,	/* bIsrCallback */
    FALSE	/* bAllowDma */
};

/** IRP for data transmission */
static MUSB_Irp MGC_MsdTxDataIrp =
{
    NULL,
    NULL,
    0,
    0,
    0,
    MGC_MsdTxDataComplete,
    NULL,
    FALSE,	/* bAllowShortTransfer */
    TRUE,	/* bIsrCallback */
    FALSE	/* bAllowDma */
};

/** IRP for CSW transmission */
static MUSB_Irp MGC_MsdTxCswIrp =
{
    NULL,
    (uint8_t *) &MGC_Csw,
    13,
    0,
    0,
    MGC_MsdTxCswComplete,
    NULL,
    FALSE,	/* bAllowShortTransfer */
    TRUE,	/* bIsrCallback */
    FALSE	/* bAllowDma */
};

/*
* registration
*/
MUSB_FunctionClient MGC_xxxFunctionClient =
{
    NULL,   /* no instance data; we are singleton */
    MGC_aDescriptorData,
    sizeof(MGC_aDescriptorData),
    3,      /* strings per language */
    MGC_aHighSpeedDescriptorData,
    sizeof(MGC_aHighSpeedDescriptorData),
    sizeof(MGC_aControlData),
    MGC_aControlData,
    &MGC_bMsdSelfPowered,
    MGC_MsdDeviceRequest,
    MGC_MsdDeviceConfigSelected,
    NULL,
    MGC_MsdNewUsbState
};

static uint8_t MGC_aMsdData[1];//[1024+512];
#define MGC_dwMsdBlockCount         1024 * 8192 / 512
//static uint32_t MGC_dwMsdBlockCount = 1024 * 8192 / 512;
uint32_t read_block = 0;
uint32_t bcmdread = 0;
uint32_t write_block = 0;
static uint32_t dwLength=0;
uint32_t bcmdwrite = 0;

/******************************************************************
CDI callbacks
******************************************************************/

/* handle an INQUIRY command */
static void MGC_MsdInquiry(uint8_t bmFlags, uint32_t dwLength, uint8_t bPage,
                           uint8_t *pbStartTx, uint8_t *pbStatus)
{
    uint16_t wIndex, wCount;

    MUSB_DPRINTF1("MGC_MsdInquiry: bmFlags = 0x%x\r\n", bmFlags);

    if ((bmFlags & (MGC_M_MSD_INQUIRY_CMD_DT | MGC_M_MSD_INQUIRY_EVPD)) ==
            (MGC_M_MSD_INQUIRY_CMD_DT | MGC_M_MSD_INQUIRY_EVPD))
    {
        /* set sense codes */
        MGC_MsdSenseData.bResponseCode = MGC_M_MSD_SCSI_SENSE_DATA_VALID | 0x70;
        MGC_MsdSenseData.bSenseKey = (uint8_t)MGC_SCSI_SC_ILLEGAL_REQUEST;
        MGC_MsdSenseData.bAsc = MGC_SCSI_ASC_INVALID_CDB_FIELD;
        MGC_MsdSenseData.bAdditionalLength = 5;
        *pbStatus = 1;
    }
    else if (bmFlags & MGC_M_MSD_INQUIRY_CMD_DT)
    {
        /* command support data */
        wCount = sizeof(MGC_aCommandUsageData) / sizeof(MGC_MsdCommandUsage);
        for (wIndex = 0; wIndex < wCount; wIndex++)
        {
            if (MGC_aCommandUsageData[wIndex].bCommand == bPage)
            {
                break;
            }
        }
        if (wIndex < wCount)
        {
            MGC_MsdTxDataIrp.pBuffer = (uint8_t *)MGC_aCommandUsageData[wIndex].pData;
            MGC_MsdTxDataIrp.dwLength = MUSB_MIN(dwLength, MGC_aCommandUsageData[wIndex].bDataSize);
        }
        else
        {
            MGC_MsdTxDataIrp.pBuffer = (uint8_t *)MGC_aUnsupportedCommandData;
            MGC_MsdTxDataIrp.dwLength = MUSB_MIN(dwLength, sizeof(MGC_aUnsupportedCommandData));
        }
        *pbStartTx = TRUE;
        *pbStatus = 0;
    }
    else if (bmFlags & MGC_M_MSD_INQUIRY_EVPD)
    {
        /* vital product data pages */
        switch (bPage)
        {
        case MGC_SCSI_PAGE_SUPPORTED_VPD:
            MGC_MsdTxDataIrp.pBuffer = (uint8_t *)&MGC_MsdSupportedVpdPagesData;
            MGC_MsdTxDataIrp.dwLength = MUSB_MIN(dwLength, sizeof(MGC_MsdSupportedVpdPagesData));
            *pbStartTx = TRUE;
            *pbStatus = 0;
            break;
        case MGC_SCSI_PAGE_UNIT_SERIAL_NUM:
            MGC_MsdTxDataIrp.pBuffer = (uint8_t *)&MGC_MsdUnitSerialNumberPageData;
            MGC_MsdTxDataIrp.dwLength = MUSB_MIN(dwLength, sizeof(MGC_MsdUnitSerialNumberPageData));
            *pbStartTx = TRUE;
            *pbStatus = 0;
            break;
        case MGC_SCSI_PAGE_DEVICE_ID:
            MGC_MsdTxDataIrp.pBuffer = (uint8_t *)&MGC_MsdDeviceIdPageData;
            MGC_MsdTxDataIrp.dwLength = MUSB_MIN(dwLength, sizeof(MGC_MsdDeviceIdPageData));
            *pbStartTx = TRUE;
            *pbStatus = 0;
            break;
        default:
            break;
        }
    }
    else
    {
        /* standard data */
        MGC_MsdTxDataIrp.pBuffer = (uint8_t *)&MGC_MsdRbcInquiryData;
        MGC_MsdTxDataIrp.dwLength = MUSB_MIN(dwLength, sizeof(MGC_MsdRbcInquiryData));
        *pbStartTx = TRUE;
        *pbStatus = 0;
    }
}

uint8_t MGC_MsdisReady(void)
{
    return MGC_bIsReady;
}

static void MGC_MsdCommand(const MGC_MsdCbw *pCbw)
{
    const MGC_MsdCommand6 *pCommand6;
    const MGC_MsdInquiryCommand *pInquiryCommand;
	static uint8_t test_unit_cnt = 0;
	static uint8_t usb_wr_flag = 0;
	
    uint8_t bmFlags, bPage;
    //uint32_t dwLength;
    uint8_t bStartTx = FALSE;
    uint8_t bStartRx = FALSE;
    uint8_t bStatus = 1;
    uint32_t dwBlock = 0;
    uint32_t dwDataLength = MUSB_SWAP32(pCbw->dCbwDataTransferLength);
    MGC_MsdCsw *pCsw = &MGC_Csw;
    uint8_t bOpcode = pCbw->aCbwCb[0];

    MUSB_DPRINTF1("MGC_MsdCommand: bOpcode = 0x%x\r\n", bOpcode);

    MGC_Cbw.dCbwDataTransferLength = dwDataLength;
    MGC_bMsdLastCommand = bOpcode;

    /* seed failed sense data */
    MGC_MsdSenseData.bResponseCode = MGC_M_MSD_SCSI_SENSE_DATA_VALID | 0x70;
    MGC_MsdSenseData.bSenseKey = (uint8_t)MGC_SCSI_SC_ILLEGAL_REQUEST;
    MGC_MsdSenseData.bAsc = MGC_SCSI_ASC_INVALID_CDB_FIELD;
    MGC_MsdSenseData.bAdditionalLength = 5;

    /* parse command */
    switch (bOpcode)
    {
    case MGC_SCSI_START_STOP_UNIT:
        if (pCbw->aCbwCb[4] & 1)
        {
            MGC_bIsReady = TRUE;
        }
        else
        {
            MGC_bIsReady = FALSE;
        }
        MGC_MsdSenseData.bResponseCode = MGC_M_MSD_SCSI_SENSE_DATA_VALID | 0x70;
        MGC_MsdSenseData.bSenseKey = (uint8_t)MGC_SCSI_SC_NONE;
        MGC_MsdSenseData.bAsc = 0;
        MGC_MsdSenseData.bAscq = 0;
        bStatus = 0;
        break;

    case MGC_SCSI_REQUEST_SENSE:
        dwLength = pCbw->aCbwCb[4];
        MGC_MsdTxDataIrp.pBuffer = (uint8_t *)&MGC_MsdSenseData;
        MGC_MsdTxDataIrp.dwLength = MUSB_MIN(dwLength,
                                             (uint32_t)(8 + MGC_MsdSenseData.bAdditionalLength));
        pCsw->dCswDataResidue = dwLength - MGC_MsdTxDataIrp.dwLength;
        bStartTx = TRUE;
        MGC_MsdSenseData.bResponseCode = 0x70;
    	if (MGC_bIsReady)
    	{
    	}
    	else
    	{
    	    MGC_MsdSenseData.bSenseKey = MGC_SCSI_SC_NOT_READY;
//            pCsw->dCswDataResidue = 0;
    	}
        bStatus = 0;
        break;

    case MGC_SCSI_TEST_UNIT_READY:
		if(test_unit_cnt ++ > 10)
		{
			rt_kprintf("---test unit:%d---\r\n",usb_wr_flag);
			test_unit_cnt = 0;
			sdcard_stop_handle();
		}
        pCsw->dCswDataResidue = 0L;
        MGC_MsdSenseData.bAdditionalLength = 5;
        if (MGC_bIsReady)
        {
            MGC_MsdSenseData.bResponseCode = MGC_M_MSD_SCSI_SENSE_DATA_VALID | 0x70;
            MGC_MsdSenseData.bSenseKey = (uint8_t)MGC_SCSI_SC_NONE;
            MGC_MsdSenseData.bAsc = 0;
            MGC_MsdSenseData.bAscq = 0;
            bStatus = 0;
        }
        else
        {
            MGC_MsdSenseData.bResponseCode =
                MGC_M_MSD_SCSI_SENSE_DATA_VALID | MGC_SCSI_STATUS_CHECK_CONDITION;
            MGC_MsdSenseData.bSenseKey = (uint8_t)MGC_SCSI_SC_NOT_READY;
            MGC_MsdSenseData.bAsc = 4;
            MGC_MsdSenseData.bAscq = 2;
            bStatus = 1;
        }
        break;

    case MGC_SCSI_INQUIRY:
        pCommand6 = (const MGC_MsdCommand6*)&(pCbw->aCbwCb[0]);
        pInquiryCommand = (const MGC_MsdInquiryCommand *)pCommand6;
        bmFlags = pInquiryCommand->bmFlags;
        dwLength = pCbw->aCbwCb[4];
        bPage = pCbw->aCbwCb[2];
        MGC_MsdInquiry(bmFlags, dwLength, bPage, &bStartTx, &bStatus);
        pCsw->dCswDataResidue = dwLength - MGC_MsdTxDataIrp.dwLength;
        break;

    case MGC_SCSI_READ_CAPACITY:
        if (MGC_bIsReady == FALSE)
        {
            bStatus = 1;
            bStartTx = FALSE;
            break;
        }
        dwLength = pCbw->aCbwCb[4];
    	MGC_MsdCapacity.dwLastBlock = MGC_SCSI_SWAP32(sdcard_get_size() - 1);
    	MGC_MsdCapacity.dwBytesPerBlock = MGC_SCSI_SWAP32(sdcard_get_block_size());
        MGC_MsdTxDataIrp.pBuffer = (uint8_t *)&MGC_MsdCapacity;
        MGC_MsdTxDataIrp.dwLength = sizeof(MGC_MsdCapacity);
        pCsw->dCswDataResidue = dwLength ? (dwLength - MGC_MsdTxDataIrp.dwLength) : 0;
        MGC_MsdSenseData.bResponseCode = 0x70;
        bStartTx = TRUE;
        bStatus = 0;
        break;

    case MGC_SCSI_RD_FMT_CAPC:
        dwLength = pCbw->aCbwCb[4];
        MGC_MsdFormatCapacity.dwListLength = MGC_SCSI_SWAP32(8);
        MGC_MsdFormatCapacity.dwBlockCount = MGC_SCSI_SWAP32(sdcard_get_size());
        MGC_MsdFormatCapacity.dwBytesPerBlock = MGC_SCSI_SWAP32(sdcard_get_block_size());
        MGC_MsdTxDataIrp.pBuffer = (uint8_t *)&MGC_MsdFormatCapacity;
        MGC_MsdTxDataIrp.dwLength = sizeof(MGC_MsdFormatCapacity);
        pCsw->dCswDataResidue = dwLength ? (dwLength - MGC_MsdTxDataIrp.dwLength) : 0;
        MGC_MsdSenseData.bResponseCode = 0x70;
        bStartTx = TRUE;
        bStatus = 0;
        break;

    case MGC_SCSI_PREVENT_ALLOW_MED_REMOVE:
        pCsw->dCswDataResidue = 0L;
        MGC_MsdSenseData.bResponseCode = 0x70;
        bStatus = 0;
        break;

    case MGC_SCSI_MODE_SELECT:
        dwLength = pCbw->aCbwCb[4];
        MGC_MsdRxDataIrp.pBuffer = MGC_aJunk;
        MGC_MsdRxDataIrp.dwLength = MUSB_MIN(dwLength, sizeof(MGC_aJunk));
        MGC_MsdSenseData.bResponseCode = 0x70;
        bStartRx = TRUE;
        bStatus = 0;
        break;

    case MGC_SCSI_READ6:
        pCsw->dCswDataResidue = 0L;
        dwLength = pCbw->aCbwCb[4];
        dwBlock = (pCbw->aCbwCb[2] << 8) | pCbw->aCbwCb[3];
        MGC_MsdTxDataIrp.pBuffer = &(MGC_aMsdData[dwBlock * MGC_wBlockSize]);
        MGC_MsdTxDataIrp.dwLength = dwDataLength;
        MUSB_DIAG2(3, "READ(6) @", dwBlock, ", count=", dwLength, 16, 0);
        /* TODO: check for overrun */
        MGC_MsdSenseData.bResponseCode = 0x70;
        bStartTx = TRUE;
        bStatus = 0;
        break;

    case MGC_SCSI_READ10:
		usb_wr_flag = 0;
        MUSB_DPRINTF("MGC_SCSI_READ10: MGC_bIsReady = %d\r\n", MGC_bIsReady);
        if (MGC_bIsReady == FALSE)
        {
            bcmdread = 0;
            bStatus = 1;
            bStartTx = FALSE;
            break;
        }
        bcmdread = 1;
        pCsw->dCswDataResidue = dwDataLength;
        dwLength = (pCbw->aCbwCb[7] << 8) | pCbw->aCbwCb[8];
        read_block = (pCbw->aCbwCb[2] << 24) | (pCbw->aCbwCb[3] << 16) |
                  (pCbw->aCbwCb[4] << 8) | pCbw->aCbwCb[5];
        MUSB_DIAG2(3, "READ(10) @", read_block, ", count=", dwLength, 16, 0);
		rt_kprintf("---usb read:%x,%x---\r\n",read_block,dwLength);
        //MUSB_DPRINTF("read block %p\r\n", read_block);
        if (MGC_MsdReadData())
        {
            MUSB_DPRINTF("read cmd failed\r\n");
            bStatus = 1;
        }
        else
        {
            MGC_MsdSenseData.bResponseCode = 0x70;
            bStartTx = TRUE;
            bStatus = 0;
        }
        break;

    case MGC_SCSI_WRITE6:
        MGC_dwRxCount = 0L;
        pCsw->dCswDataResidue = 0L;
        dwLength = pCbw->aCbwCb[4];
        dwBlock = (pCbw->aCbwCb[2] << 8) | pCbw->aCbwCb[3];
        MGC_MsdRxDataIrp.pBuffer = &(MGC_aMsdData[dwBlock * MGC_wBlockSize]);
        MGC_MsdRxDataIrp.dwLength = dwDataLength;
        MUSB_DIAG2(3, "WRITE(6) @", dwBlock, ", count=", dwLength, 16, 0);
        /* check for overrun */
        if ((dwBlock + dwLength) > MGC_dwMsdBlockCount)
        {
            bStatus = 1;
            MUSB_SetPipeHalt(MGC_MsdRxDataIrp.hPipe, TRUE);
        }
        else
        {
            MGC_MsdSenseData.bResponseCode = 0x70;
            bStartRx = TRUE;
            bStatus = 0;
        }
        break;

    case MGC_SCSI_WRITE10:
		usb_wr_flag = 1;
/*        if (MGC_bIsReady == FALSE)
        {
            bcmdwrite = 0;
            bStatus = 1;
            bStartRx = FALSE;
            break;
        }*/
        bcmdwrite = 1;
        MUSB_DPRINTF("dwDataLength:%d\r\n", dwDataLength);
        pCsw->dCswDataResidue = dwDataLength;
        dwLength = (pCbw->aCbwCb[7] << 8) | pCbw->aCbwCb[8];
        write_block = (pCbw->aCbwCb[2] << 24) | (pCbw->aCbwCb[3] << 16) |
            (pCbw->aCbwCb[4] << 8) | pCbw->aCbwCb[5];
        rt_kprintf("---usb write:%x,%x---\r\n",write_block,dwLength);   
        /* check for overrun */
        if ((write_block + dwLength) > sdcard_get_size())
        {
            bStatus = 1;
            MUSB_SetPipeHalt(MGC_MsdRxDataIrp.hPipe, TRUE);
        }
        else
        {
            MGC_MsdSenseData.bResponseCode = 0x70;
            bStartRx = TRUE;
            bStatus = 0;
        }
        break;

    case MGC_SCSI_VERIFY:
        bcmdwrite = 1;
        pCsw->dCswDataResidue = dwDataLength;
        dwLength = (pCbw->aCbwCb[7] << 8) | pCbw->aCbwCb[8];
        if (pCbw->aCbwCb[1] & 2)
        {
            write_block = (pCbw->aCbwCb[2] << 24) | (pCbw->aCbwCb[3] << 16) |
                (pCbw->aCbwCb[4] << 8) | pCbw->aCbwCb[5];
                        
            if ((write_block + dwLength) > sdcard_get_size())
            {
                bStatus = 1;
                MUSB_SetPipeHalt(MGC_MsdRxDataIrp.hPipe, TRUE);
            }
            else
            {
                MGC_MsdSenseData.bResponseCode = 0x70;
                bStartRx = TRUE;
                bStatus = 0;
            }
        }
        else
        {
            bStatus = 0;
        }
        break;

    case MGC_SCSI_MODE_SENSE:
        if (MGC_bIsReady)
        {
            MUSB_MemSet(&MGC_MsdModeSenseData, 0, sizeof(MGC_MsdModeSenseData));
            switch (pCbw->aCbwCb[2])    // page code
            {
                case 0x08:
                    MGC_MsdModeSenseData.sModePage.bPage = 0x08;
                    MGC_MsdModeSenseData.sModePage.bLength = 0x12;
                    break;

                case 0x1C:
                    MGC_MsdModeSenseData.sModePage.bPage = 0x1C;
                    MGC_MsdModeSenseData.sModePage.bLength = 0x0A;
                    break;

                case 0x3F:
                    MGC_MsdModeSenseData.sModePage.bPage = 0x08;
                    MGC_MsdModeSenseData.sModePage.bLength = 0x12;
                    break;

                default:
                    break;
            }
            MGC_MsdModeSenseData.sModeParam.bDataLength = 5 + MGC_MsdModeSenseData.sModePage.bLength;
    	    MGC_MsdTxDataIrp.pBuffer = (uint8_t*)&MGC_MsdModeSenseData;
    	    MGC_MsdTxDataIrp.dwLength = sizeof(MGC_MsdModeSenseData);
    	    pCsw->dCswDataResidue = 0;
    	    bStartTx = TRUE;
    	    bStatus = 0;
        }
        else
        {
    	    bStartTx = FALSE;
    	    bStatus = 0;
        }
        break;

    case MGC_SCSI_WRITE_BUFFER:
        /* TODO */
        dwLength = (pCbw->aCbwCb[7] << 8) | pCbw->aCbwCb[8];
        MGC_MsdSenseData.bResponseCode &= ~MGC_M_MSD_SCSI_SENSE_DATA_VALID;
        bStatus = 0;
        break;

    default:
        MUSB_DIAG1(2, "MSD: unknown SCSI command ", bOpcode, 16, 02);
        bStatus = 1;
        break;
    }

    /* if bad status and host expecting data, stall IN pipe */
    if (bStatus && (pCbw->bmCbwFlags & MGC_MSD_BOT_DIR_IN))
    {
        MUSB_SetPipeHalt(MGC_MsdTxDataIrp.hPipe, TRUE);
    }

    /* start data transfer or CSW transfer */
    pCsw->dCswSignature = MGC_MSD_BOT_CSW_SIGNATURE;
    pCsw->dCswTag = pCbw->dCbwTag;
    pCsw->bCswStatus = bStatus;

    if (bStartTx)
    {
        MUSB_DIAG_STRING(3, "MSD: starting Data Tx");
        MGC_MsdTxDataIrp.dwActualLength = 0L;
        MUSB_StartTransfer(&MGC_MsdTxDataIrp);
    }
    else if (bStartRx)
    {
        MUSB_DIAG_STRING(3, "MSD: starting Data Rx");
//        MGC_MsdRxDataIrp.dwActualLength = 0L;
//        MUSB_StartTransfer(&MGC_MsdRxDataIrp);
	    MGC_Start_Data_Rx((uint8_t *)MGC_aJunk);
    }
    else
    {
        MUSB_DIAG_STRING(3, "MSD: starting CSW Tx");
	    MGC_Start_Csw_Tx();
    }
}
static uint32_t MGC_MsdReadData(void)
{
    MGC_MsdCsw* pCsw = &MGC_Csw;

    //os_printf("MGC_MsdReadData start\r\n");
#if 1   //#ifdef CONFIG_APP_SDCARD
    if (sdcard_read_new(read_block, 1, (uint8 *)MGC_aJunk))
    {
        MUSB_DPRINTF("sdcard read failed:0x%lx\r\n", read_block);
    }
    else
    {
       // MUSB_DPRINTF("sdcard read ok:0x%lx\r\n", read_block);
    }
#endif
    //MUSB_DPRINTF("#");

    MGC_MsdTxDataIrp.pBuffer = (uint8 *)MGC_aJunk;
    
    read_block ++;
    
    if (pCsw->dCswDataResidue > 512)
    {
        pCsw->dCswDataResidue -= 512;
        MGC_MsdTxDataIrp.dwLength = 512;
    }
    else
    {
        bcmdread = 0;
        MGC_MsdTxDataIrp.dwLength = pCsw->dCswDataResidue;
        pCsw->dCswDataResidue = 0;
    }

    return 0;
}

void MGC_Start_Data_Rx(uint8_t *data)
{
    MUSB_DPRINTF1("MGC_Start_Data_Rx\r\n");
    MGC_MsdRxDataIrp.pBuffer = data;
    MGC_MsdRxDataIrp.dwActualLength = 0;
    if (MGC_Csw.dCswDataResidue > 512)
    {
        MGC_MsdRxDataIrp.dwLength = 512;
        MGC_Csw.dCswDataResidue -= 512;
    }
    else
    {
        MGC_MsdRxDataIrp.dwLength = MGC_Csw.dCswDataResidue;
        MGC_Csw.dCswDataResidue = 0;
    }
    
    MUSB_StartTransfer(&MGC_MsdRxDataIrp);
}

void Multi_Block_MGC_Start_Data_Rx(uint8_t *data)
{
    MUSB_DPRINTF1("Multi_Block_MGC_Start_Data_Rx\r\n");
    MGC_MsdRxDataIrp.pBuffer = data;
    MGC_MsdRxDataIrp.dwActualLength = 0;

    MGC_MsdRxDataIrp.dwLength = MGC_Csw.dCswDataResidue;
    MGC_Csw.dCswDataResidue = 0;

    
    MUSB_StartTransfer(&MGC_MsdRxDataIrp);
}

void MGC_Start_Csw_Tx(void)
{
   // os_printf("MGC_Start_Csw_Tx\r\n");
    if (bcmdwrite == 1)
    {
        bcmdwrite = 0;
    }
    MGC_MsdTxCswIrp.dwActualLength = 0L;
    MUSB_StartTransfer(&MGC_MsdTxCswIrp);
}

/** CBW reception callback */
static uint32_t MGC_MsdRxCbwComplete(void *pCompleteParam, MUSB_Irp *pIrp)
{
    const MGC_MsdCbw *pCbw;

    MUSB_DPRINTF1("MGC_MsdRxCbwComplete\r\n");

    if ((MUSB_STATUS_OK == pIrp->dwStatus)
            && (31 == pIrp->dwActualLength))
    {
        pCbw = (const MGC_MsdCbw *)pIrp->pBuffer;
        if (MGC_MSD_BOT_CBW_SIGNATURE == pCbw->dCbwSignature)
        {
            /* process the valid CBW */
            MGC_MsdCommand(pCbw);
            MGC_dwGoodCmdCount ++;

            return (uint32_t) - 1;
        }
    }

    /* invalid CBW: stall pipes */
    MUSB_SetPipeHalt(MGC_MsdRxCbwIrp.hPipe, TRUE);
    MUSB_SetPipeHalt(MGC_MsdTxCswIrp.hPipe, TRUE);

    /* prepare for post-stall command */
    MGC_MsdRxCbwIrp.dwActualLength = 0L;
    MUSB_StartTransfer(&MGC_MsdRxCbwIrp);
    return 0;
}

/** data reception callback */
static uint32_t MGC_MsdRxDataComplete(void *pCompleteParam, MUSB_Irp *pIrp)
{
    MUSB_DPRINTF1("MGC_MsdRxDataComplete\r\n");
    /* data recvd; so send status */
    if (bcmdwrite == 1)
    {
        //msg_put(MSG_MGC_WRITE);
        if (sdcard_write_new(write_block, 1, (uint8_t *)MGC_aJunk))
        {
            MUSB_DPRINTF("sdcard write failed:0x%lx\r\n", write_block);
        }
        else
        {
            MUSB_DPRINTF("sdcard write ok:0x%lx\r\n", write_block);
        }
        write_block ++;
    }
    //MUSB_DPRINTF("dC:%d\r\n", MGC_Csw.dCswDataResidue);
    if (MGC_Csw.dCswDataResidue != 0)
    {
        MGC_Start_Data_Rx((uint8_t *)MGC_aJunk);
        return 0;
    }    /* TODO: check for data recv error */

    /* send CSW */
    MGC_Start_Csw_Tx();
    return 0;
}

/** data transmit complete callback */
static uint32_t MGC_MsdTxDataComplete(void *pCompleteParam, MUSB_Irp *pIrp)
{
    //os_printf("MGC_MsdTxDataComplete\r\n");
    /* data sent; send status */
    /* TODO: check for data send error */

    if ((bcmdread == 1) && (MGC_Csw.dCswDataResidue > 0))
    {
        if (MGC_MsdReadData())
        {
            MGC_Csw.bCswStatus = 1;
        }
        else
        {
            MGC_MsdTxDataIrp.dwActualLength = 0L;
	        MUSB_StartTransfer(&MGC_MsdTxDataIrp);
            MGC_Csw.bCswStatus = 0;
            return 0;
        }
    }

    /* send CSW */
    MGC_MsdTxCswIrp.dwActualLength = 0L;
    MUSB_DIAG_STRING(3, "MSD: Starting CSW Tx");
    MUSB_StartTransfer(&MGC_MsdTxCswIrp);

	return 0;
}

/** CSW transmit complete callback */
static uint32_t MGC_MsdTxCswComplete(void *pCompleteParam, MUSB_Irp *pIrp)
{
    MUSB_DPRINTF("MGC_MsdTxCswComplete\r\n");
    /* prepare for next command */
    MGC_MsdRxCbwIrp.dwActualLength = 0L;
    MUSB_DIAG_STRING(3, "MSD: Starting CBW Rx");
    MUSB_StartTransfer(&MGC_MsdRxCbwIrp);
    return 0;
}



static void MGC_MsdNewUsbState(void* hClient, MUSB_BusHandle hBus,
			       MUSB_State State)
{
	MUSB_DPRINTF1("MGC_MsdNewUsbState: state = %x\r\n",State); // MUSB_DPRINTF
    MGC_eMsdUsbState = State;
#ifdef CONFIG_APP_USB_CARD_READER
    if(4 == State) //pc link
    {
//        msg_put(MSG_USB_DEVICE_ENUM_OVER);
//       app_card_reader_play_pause(0);
    }
#endif
}

uint8_t UsbDeviceGetStatus(void)
{
    return MGC_eMsdUsbState;
}

void UsbSetbReady(uint8_t bReady)
{
    MUSB_DPRINTF1("UsbSetbReady: bReady = %d\r\n", bReady);
    MGC_bIsReady = bReady;
}

static uint8_t MGC_MsdDeviceRequest(void *hClient, MUSB_BusHandle hBus,
                                    uint32_t dwSequence, const uint8_t *pSetup,
                                    uint16_t wLength)
{
    uint32_t dwStatus;
    MUSB_DeviceRequest *pRequest = (MUSB_DeviceRequest *)pSetup;
    uint8_t bOk = FALSE;

    MUSB_DPRINTF1("MGC_MsdDeviceRequest\r\n");

    if (MUSB_TYPE_STANDARD == (pRequest->bmRequestType & MUSB_TYPE_MASK))
    {
        switch (pRequest->bRequest)
        {
        case MUSB_REQ_GET_INTERFACE:
            MUSB_DeviceResponse(hBus, dwSequence, &MGC_bMsdInterface, 1, FALSE);
            bOk = TRUE;
            break;
        case MUSB_REQ_SET_INTERFACE:
            MUSB_DeviceResponse(hBus, dwSequence, NULL, 0, FALSE);
            bOk = TRUE;
            break;
        }
    }
    else if ((pRequest->bmRequestType & MUSB_TYPE_CLASS)
            && (pRequest->bmRequestType & MUSB_RECIP_INTERFACE))
    {
        switch (pRequest->bRequest)
        {
        case MGC_MSD_BOT_RESET:
            /* reset */
            MUSB_CancelTransfer(&MGC_MsdTxCswIrp);
            MUSB_CancelTransfer(&MGC_MsdTxDataIrp);
            MUSB_CancelTransfer(&MGC_MsdRxDataIrp);
            MUSB_CancelTransfer(&MGC_MsdRxCbwIrp);
            MUSB_FlushPipe(MGC_MsdTxDataIrp.hPipe);
            MUSB_FlushPipe(MGC_MsdRxDataIrp.hPipe);
            dwStatus = MUSB_StartTransfer(&MGC_MsdRxCbwIrp);
            MUSB_DeviceResponse(hBus, dwSequence, NULL, 0, FALSE);
            bOk = TRUE;
            break;

        case MGC_MSD_BOT_GET_MAXLUN:
            /* get max lun */
            MUSB_DeviceResponse(hBus, dwSequence, &MGC_bMsdMaxLun, 1, FALSE);
            bOk = TRUE;
            break;
        }
    }
    dwStatus++;
    return bOk;
}

static uint8_t MGC_MsdDeviceConfigSelected(void *hClient, MUSB_BusHandle hBus,
        uint8_t bConfigurationValue,
        MUSB_PipePtr *ahPipe)
{
    uint32_t dwStatus;

    MUSB_DPRINTF1("MGC_MsdDeviceConfigSelected\r\n");

    MGC_MsdTxDataIrp.hPipe = ahPipe[0];
    MGC_MsdTxCswIrp.hPipe = ahPipe[0];
    MGC_MsdRxCbwIrp.hPipe = ahPipe[1];
    MGC_MsdRxDataIrp.hPipe = ahPipe[1];
    dwStatus = MUSB_StartTransfer(&MGC_MsdRxCbwIrp);
    MUSB_DPRINTF("MGC_MsdDeviceConfigSelected: dwStatus = 0x%lx\r\n", dwStatus);
    if (MUSB_STATUS_OK == dwStatus)
    {
        MGC_bIsReady = TRUE;
        MGC_MsdSenseData.bResponseCode = MGC_M_MSD_SCSI_SENSE_DATA_VALID | 0x70;
        MGC_MsdSenseData.bSenseKey = (uint8_t)MGC_SCSI_SC_NONE;
        MGC_MsdSenseData.bAsc = 0;
        MGC_MsdSenseData.bAscq = 0;
//        uint16 wIntrRxEnable = REG_USB_INTRRX1E | (REG_USB_INTRRX2E << 8 );
        return TRUE;
    }
    /* TODO: log error? */
    return FALSE;
}

// eof
#endif // CFG_SUPPORT_MSD