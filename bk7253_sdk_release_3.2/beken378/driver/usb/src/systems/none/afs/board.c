/******************************************************************
 *                                                                *
 *      Copyright Mentor Graphics Corporation 2003-2004           *
 *                                                                *
 *                All Rights Reserved.                            *
 *                                                                *
 *    THIS WORK CONTAINS TRADE SECRET AND PROPRIETARY INFORMATION *
 *  WHICH IS THE PROPERTY OF MENTOR GRAPHICS CORPORATION OR ITS   *
 *  LICENSORS AND IS SUBJECT TO LICENSE TERMS.                    *
 *                                                                *
 ******************************************************************/
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "include.h"
#include "mu_diag.h"
#include "mu_mem.h"
#include "mu_none.h"
#include "brd_cnf.h"
#include "board.h"
#include "drv_model_pub.h"
#include "mem_pub.h"
#include "str_pub.h"
#include "fake_clock_pub.h"

#if CFG_USB
/**************************** GLOBALS *****************************/
MUSB_NoneController MUSB_aNoneController[] =
{
#ifdef MUSB_MHDRC
    { MUSB_CONTROLLER_MHDRC, (void *)0x00804000, 0x0010, FALSE },
#endif

#ifdef MUSB_HSFC
    { MUSB_CONTROLLER_HSFC, (void *)0x14ab, 0x0010, FALSE },
#endif

#ifdef MUSB_FDRC
    { MUSB_CONTROLLER_FDRC, (void *)0x00804000, 9, FALSE },
#endif
};

static MGC_AfsUds *MGC_apAfsUds[sizeof(MUSB_aNoneController) / sizeof(MUSB_NoneController)];
static uint8_t MGC_bAfsUdsCount   = 0;
static uint8_t MGC_bBoardInitDone = FALSE;

/*************************** FUNCTIONS ****************************/
char MUSB_ReadConsole(void)
{
    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    return 0;
}

void MUSB_WriteConsole(const char bChar)
{
    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
}

void *MGC_AfsMemRealloc(void *pBlock, uint32_t iSize)
{
    void *pNewBlock = MUSB_MemAlloc(iSize);

    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    if (pNewBlock)
    {
        MUSB_MemCopy(pNewBlock, pBlock, iSize);
        MUSB_MemFree(pBlock);
    }
    else
    {
		MUSB_ERR_PRINTF("MGC_AfsMemRealloc: MUSB_MemAlloc failed\r\n");
    }
    return (pNewBlock);
}

uint8_t MUSB_BoardMessageString(char *pMsg, uint16_t wBufSize, const char *pString)
{
    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    if ((strlen(pMsg) + strlen(pString)) >= wBufSize)
    {
        return FALSE;
    }
    strcat(pMsg, pString);
    return TRUE;
}

uint8_t MUSB_BoardMessageNumber(char *pMsg, uint16_t wBufSize, uint32_t dwNumber,
                                uint8_t bBase, uint8_t bJustification)
{
    char type;
    char format[8];
    char fmt[16];
    char number[32];

    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    switch (bBase)
    {
    case 8:
        type = 'i';
        break;
    case 10:
        type = 'd';
        break;
    case 16:
        type = 'x';
        break;
    default:
        return FALSE;
    }
    if (bJustification)
    {
        sprintf(format, "0%d%c", bJustification, type);
    }
    else
    {
        sprintf(format, "%c", type);
    }
    fmt[0] = '%';
    fmt[1] = (char)0;
    strcat(fmt, format);
    sprintf(number, fmt, dwNumber);

    return MUSB_BoardMessageString(pMsg, wBufSize, number);
}

uint32_t MUSB_BoardGetTime(void)
{
    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    return fclk_get_tick();
}

void MGC_AfsUdsIsr(void)
{
    uint8_t bIndex;
    MGC_AfsUds *pUds;

    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    MUSB_NPRT("\r\n[MGC] AfsUdsIsr\r\n");
    MUSB_DPRINTF("MGC_AfsUdsIsr: MGC_bAfsUdsCount = %x\r\n",MGC_bAfsUdsCount);
    for (bIndex = 0; bIndex < MGC_bAfsUdsCount; bIndex++)
    {
        pUds = MGC_apAfsUds[bIndex];
        if (pUds)
        {
            if (pUds->pfNoneIsr)
            {
                //			MGC_NoneControllerIsr()
                pUds->pfNoneIsr(pUds->pNonePrivateData);
                if (pUds->pPciAck)
                {
                    *((uint32_t *)pUds->pPciAck) = 3;
                }
            }
        }
    }
}

static void MGC_BoardInit()
{
    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    MUSB_MemSet(MGC_apAfsUds, 0, sizeof(MGC_apAfsUds));

    MGC_bBoardInitDone = TRUE;
}

/*��̬����ṹ��MGC_AfsUds�������г�ʼ��
��ʼ��isr,��̬�ṹ��ָ������MGC_apAfsUds[]ָ��˽ṹ��
*/
void *MUSB_BoardInitController(void *pPrivateData,
                               MUSB_NoneIsr pFuncIsr,
                               const MUSB_NoneController *pControllerInfo,
                               uint8_t **ppBaseIsr,
                               uint8_t **ppBaseBsr)
{
    static MGC_AfsUds pUds;
    MGC_bAfsUdsCount = 0;

    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    if (!MGC_bBoardInitDone)
    {
        MGC_BoardInit();
    }

    MUSB_MemSet(&pUds, 0, sizeof(MGC_AfsUds));

    pUds.dwIrq = pControllerInfo->dwInfo;
    pUds.pNonePrivateData = pPrivateData;
    pUds.pfNoneIsr = pFuncIsr;

    /* assign the interrupt */
    os_strcpy(pUds.aIsrName, "MUSB-");
    pUds.aIsrName[5] = '0' + MGC_bAfsUdsCount;
    pUds.aIsrName[6] = (char)0;

    pUds.bIndex = MGC_bAfsUdsCount;
    MGC_apAfsUds[MGC_bAfsUdsCount++] = &pUds;
    MUSB_DPRINTF("MUSB_BoardInitController: pUds.aIsrName = \"%s\", &pUds = 0x%p\r\n", pUds.aIsrName, &pUds);
    MUSB_DPRINTF("MUSB_BoardInitController: MGC_bAfsUdsCount = %d, MGC_apAfsUds[] = 0x%p\r\n", MGC_bAfsUdsCount, MGC_apAfsUds[MGC_bAfsUdsCount-1]);
    return &pUds;
}

uint8_t MUSB_BoardInitTimers(void *pPrivateData, uint16_t wTimerCount,
                             const uint32_t *adwTimerResolutions)
{
    unsigned int iTimerCount;
    unsigned int iTimerIndex;
    int iIndex;
    unsigned int iTimerAvail = 0;
    static MGC_AfsTimerWrapper Timerwrapper;
    MGC_AfsUds *pUds = (MGC_AfsUds *)pPrivateData;

    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    iTimerCount = 1;
    if (iTimerCount < wTimerCount)
    {
        return FALSE;
    }
    pUds->aTimerWrapper = &Timerwrapper;

    /* ensure enough free timers */
    for (iTimerIndex = 0;
            (iTimerAvail < wTimerCount) && (iTimerIndex < iTimerCount);
            iTimerIndex++)
    {
        pUds->aTimerWrapper[iTimerAvail++].iIndex = iTimerIndex;
    }

    if (iTimerAvail < wTimerCount)
    {
        /* insufficient good timers */
//		MUSB_MemFree(pUds->aTimerWrapper);
        return FALSE;
    }

    /* allocate timers now */
    for (iTimerIndex = 0; iTimerIndex < wTimerCount; iTimerIndex++)
    {
		iIndex = 0;
		if (iIndex >= 0)
		{
		    pUds->aTimerWrapper[iTimerIndex].iIndex = iIndex;
		    pUds->aTimerWrapper[iTimerIndex].pfExpired = NULL;
			pUds->aTimerWrapper[iTimerIndex].bTimerStart = 0;
		}
    }

    pUds->wTimerCount = wTimerCount;

    return TRUE;
}

void MUSB_BoardDestroyController(void *pPrivateData)
{
    MGC_AfsUds *pUds = (MGC_AfsUds *)pPrivateData;

    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    MGC_apAfsUds[pUds->bIndex] = NULL;
//    MUSB_MemFree(pPrivateData);   // by gwf
}

void MUSB_BoardRunBackground(void* pPrivateData)
{
    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    /* nothing to do */
}

void MGC_AfsTimerExpired(void)
{
    volatile MGC_AfsUds *pUds;
    MGC_AfsTimerWrapper *pWrapper;

    pUds = MGC_apAfsUds[0];
    if (pUds == NULL)
    {
        MUSB_DPRINTF("MGC_AfsTimerExpired: pUds = null\r\n");
        return;
    }

    pWrapper = &(pUds->aTimerWrapper[0]);
	if (pWrapper != NULL)
	{
        if (pWrapper->bTimerStart == 1)
        {
            if ((--pWrapper->dwTime) == 0)
            {
                pWrapper->bTimerStart = 0; // stop count
                if (pWrapper->pfExpired != NULL)
                {
                    pWrapper->pfExpired(pWrapper->pParam);
                }
            }
        }
        else
        {
//            MUSB_DPRINTF("pWrapper->bTimerStart = %d\r\n", pWrapper->bTimerStart);
        }
	}
    else
    {
        MUSB_DPRINTF("pWrapper == NULL\r\n");
    }
}

uint8_t MUSB_BoardArmTimer(void *pPrivateData, uint16_t wIndex,
                           uint32_t dwTime,
                           uint8_t bPeriodic,
                           MUSB_pfTimerExpired pfExpireCallback,
                           void *pParam)
{
    OSStatus err = kNoErr;
    MGC_AfsUds *pUds = (MGC_AfsUds *)pPrivateData;
    MGC_AfsTimerWrapper *pWrapper = &(pUds->aTimerWrapper[wIndex]);

    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
	MUSB_DPRINTF("MUSB_BoardArmTimer: wIndex=%d, dwTime=%ld, bPeriodic=%d\r\n", wIndex, dwTime, bPeriodic);

    pWrapper->pParam = pParam;
    pWrapper->pfExpired = pfExpireCallback;
    pWrapper->dwTime = dwTime;
    pWrapper->bPeriodic = bPeriodic;
    pWrapper->bTimerStart = 1;

    if (FALSE == bPeriodic)
    {
        if (rtos_is_oneshot_timer_running(&(pWrapper->timer)))
        {
            MUSB_PRT("[MGC] Timer %d, stop it;\r\n", wIndex);
            rtos_stop_oneshot_timer(&(pWrapper->timer));
        }
        if (rtos_is_oneshot_timer_init(&(pWrapper->timer)))
        {
            MUSB_PRT("[MGC] Timer %d, deinit it;\r\n", wIndex);
            rtos_deinit_oneshot_timer(&(pWrapper->timer));
        }

        MUSB_PRT("[MGC] Timer %d, arm it;\r\n", wIndex);
        err = rtos_init_oneshot_timer(&(pWrapper->timer),
                                      dwTime,
                                      (timer_2handler_t)pfExpireCallback,
                                      pParam,
                                      NULL);
        ASSERT(kNoErr == err);

        err = rtos_start_oneshot_timer(&(pWrapper->timer));
        ASSERT(kNoErr == err);
    }
    else
    {
        MUSB_PRT("[MGC] BoardArmTimer will be periodic\r\n");
        ASSERT(FALSE == bPeriodic);
    }

    return TRUE;
}

uint8_t MUSB_BoardCancelTimer(void *pPrivate, uint16_t wIndex)
{
    MGC_AfsUds *pUds = (MGC_AfsUds *)pPrivate;
    MGC_AfsTimerWrapper *pWrapper = &(pUds->aTimerWrapper[wIndex]);

    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    GLOBAL_INT_DECLARATION();

    GLOBAL_INT_DISABLE();
    pWrapper->pfExpired = NULL;
    pWrapper->bTimerStart = 0;
    GLOBAL_INT_RESTORE();

    return TRUE;
}

/*
* Controller calls this to print a diagnostic message
*/
uint8_t MUSB_BoardPrintDiag(void *pPrivate, const char *pMessage)
{
    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    return TRUE;
}

/*
* Controller calls this to get a bus address (for DMA) from a system address
*/
void *MUSB_BoardSystemToBusAddress(void *pPrivate, const void *pSysAddr)
{
    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    return NULL;
}
#endif
// eof

