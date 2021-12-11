/******************************************************************
*                                                                *
*         Copyright Mentor Graphics Corporation 2004             *
*                                                                *
*                All Rights Reserved.                            *
*                                                                *
*    THIS WORK CONTAINS TRADE SECRET AND PROPRIETARY INFORMATION *
*  WHICH IS THE PROPERTY OF MENTOR GRAPHICS CORPORATION OR ITS   *
*  LICENSORS AND IS SUBJECT TO LICENSE TERMS.                    *
*                                                                *
******************************************************************/

/*
 * MUSBMicroSW array/list implementation.
 * $Revision: 1.4 $
 */

#include "mu_impl.h"
#include "mu_list.h"
#include "mu_mem.h"

/* Call this (portable) or initialize struct (non-portable) */
MUSB_Array *MUSB_ArrayInit(MUSB_Array *pArray,
                           uint_fast16_t wItemSize,
                           uint_fast16_t wStaticItemCount,
                           void *pStaticBuffer)
{
    MUSB_Array *pResult = pArray;

    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    if (!pArray)
    {
        pResult = (MUSB_Array *)MUSB_MemAlloc(sizeof(MUSB_Array));
    }
    if (pResult)
    {
        MUSB_MemSet(pResult, 0, sizeof(MUSB_Array));
        pResult->wItemSize = wItemSize;
        if (pStaticBuffer)
        {
            pResult->wStaticItemCount = wStaticItemCount;
        }
        else
        {
            pResult->wStaticItemCount = 0;
        }
        pResult->pStaticBuffer = pStaticBuffer;
        pResult->wItemCount = pResult->wDynamicItemCount = 0;
        pResult->pDynamicBuffer = NULL;
    }
    else
    {
		MUSB_ERR_PRINTF("MUSB_ArrayInit: MUSB_MemAlloc failed\r\n");
    }
    return pResult;
}

/* How many items are in array? */
uint_fast16_t MUSB_ArrayLength(MUSB_Array *pArray)
{
    uint_fast16_t result = 0;

    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    if (pArray)
    {
        result = pArray->wItemCount;
    }
    return result;
}

/* Get the item at the given index */
void *MUSB_ArrayFetch(MUSB_Array *pArray, uint_fast16_t wIndex)
{
    void *pResult = NULL;

    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    MUSB_DPRINTF("MUSB_ArrayFetch: pArray = 0x%p, wIndex = 0x%x\r\n", pArray, wIndex);
    if (pArray)
    {
        MUSB_DPRINTF("MUSB_ArrayFetch: pArray->wItemCount = 0x%x\r\n", pArray->wItemCount);
        MUSB_DPRINTF("MUSB_ArrayFetch: pArray->wStaticItemCount = 0x%x\r\n", pArray->wStaticItemCount);
        if (wIndex < pArray->wItemCount)
        {
            if (wIndex < pArray->wStaticItemCount)
            {
                pResult = (uint8_t*)pArray->pStaticBuffer + 
                            (wIndex * pArray->wItemSize);
            }
            else
            {
                pResult = (uint8_t*)pArray->pDynamicBuffer + 
                            ((wIndex - pArray->wStaticItemCount) * pArray->wItemSize);
            }
        }
    }
    return pResult;
}

/* Append an item */
uint8_t MUSB_ArrayAppend(MUSB_Array *pArray, const void *pItem)
{
    uint8_t bResult = FALSE;
    void *pDest = NULL;
    uint_fast16_t wCount;
    uint_fast16_t wIncrement;
    uint_fast16_t wNewCount;

    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    MUSB_DPRINTF("MUSB_ArrayAppend: pArray = 0x%p, pItem = 0x%p, pArray->wItemCount = %d\r\n",
              pArray, pItem, pArray->wItemCount);
    MUSB_DPRINTF("MUSB_ArrayAppend: pEnd->wMaxPacketSizeTx = 0x%x, pEnd->wMaxPacketSizeRx = 0x%x\r\n",
                 ((MGC_EndpointResource *)pItem)->wMaxPacketSizeTx, ((MGC_EndpointResource *)pItem)->wMaxPacketSizeRx);
    MUSB_DPRINTF("MUSB_ArrayAppend: pEnd->bTrafficType = 0x%x, pEnd->bRxTrafficType = 0x%x\r\n",
                 ((MGC_EndpointResource *)pItem)->bTrafficType, ((MGC_EndpointResource *)pItem)->bRxTrafficType);
    MUSB_DPRINTF("MUSB_ArrayAppend: pEnd->bIsClaimed = 0x%x, pEnd->bRxClaimed = 0x%x\r\n",
                 ((MGC_EndpointResource *)pItem)->bIsClaimed, ((MGC_EndpointResource *)pItem)->bRxClaimed);
    if (!pArray || !pItem)
    {
        return FALSE;
    }
    wCount = pArray->wItemCount;
    if (wCount < pArray->wStaticItemCount)
    {
        /* simplest case: static space */
        bResult = TRUE;
        pDest = (uint8_t *)pArray->pStaticBuffer +
                (wCount * pArray->wItemSize);
    }
    else if (wCount < (pArray->wStaticItemCount + pArray->wDynamicItemCount))
    {
        /* dynamic buffer has space */
        bResult = TRUE;
        pDest = (uint8_t *)pArray->pDynamicBuffer +
                ((wCount - pArray->wStaticItemCount) * pArray->wItemSize);
    }
    else
    {
        /* need to grow dynamic buffer */
        wIncrement = MUSB_MAX(pArray->wStaticItemCount, 2);
        wNewCount = wCount + wIncrement;
        MUSB_ERR_PRINTF("MUSB_ArrayAppend: MUSB_MemRealloc: pDest = 0x%p, "
        		"pArray->pDynamicBuffer = 0x%p, wNewCount = 0x%x, "
        		"pArray->wItemSize = 0x%x\r\n",
        		pDest, pArray->pDynamicBuffer, wNewCount, pArray->wItemSize);
        pDest = MUSB_MemRealloc(pArray->pDynamicBuffer,
                                wNewCount * pArray->wItemSize);
        if (pDest)
        {
            bResult = TRUE;
            pArray->pDynamicBuffer = pDest;
            pArray->wDynamicItemCount += wIncrement;
            pDest = (uint8_t *)pArray->pDynamicBuffer +
                    ((wCount - pArray->wStaticItemCount) * pArray->wItemSize);
        }
    }
    if (bResult)
    {
        MUSB_MemCopy(pDest, pItem, pArray->wItemSize);
        pArray->wItemCount++;
    }
    return bResult;
}

/* Remove all array items */
void MUSB_ArrayClear(MUSB_Array *pArray)
{
    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    if (pArray)
    {
        pArray->wItemCount = 0;
        if (pArray->pDynamicBuffer)
        {
            MUSB_MemFree(pArray->pDynamicBuffer);
            pArray->pDynamicBuffer = NULL;
            pArray->wDynamicItemCount = 0;
        }
    }
}

/* Initialize the given list */
void MUSB_ListInit(MUSB_LinkedList *pList)
{
    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    if (pList)
    {
        pList->pNext = NULL;
        pList->dwCount = 0L;
        pList->pItem = NULL;
    }
}

/* Get the number of items in the given list */
uint_fast16_t MUSB_ListLength(MUSB_LinkedList *pList)
{
    MUSB_LinkedList *pNext;
    uint_fast16_t wResult = 0;

    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    if (pList)
    {
    	if (pList->pItem)
    	{
			pNext = pList->pNext;
			wResult++;
			while (pNext)
			{
				wResult++;
				pNext = pNext->pNext;
			}
    	}
    }
    return wResult;
}

/* Find the index'th item (counting from 0) in the given list */
void *MUSB_ListFindItem(MUSB_LinkedList *pList, uint_fast16_t wIndex)
{
    void *pItem = NULL;
    MUSB_LinkedList *pRecord = MUSB_ListFindRecord(pList, wIndex);

    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    if (pRecord)
    {
        pItem = pRecord->pItem;
    }
    return pItem;
}

/* Find the index'th record (counting from 0) in the given list */
MUSB_LinkedList *MUSB_ListFindRecord(MUSB_LinkedList *pList, uint_fast16_t wIndex)
{
    uint_fast16_t wCount = 0;
    MUSB_LinkedList *pResult = NULL;
    MUSB_LinkedList *pNext = pList;

    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    while ((wCount < wIndex) && pNext)
    {
        wCount++;
        pNext = pNext->pNext;
    }
    if (wCount == wIndex)
    {
        pResult = pNext;
    }
    return pResult;
}

static uint8_t MGC_ListInsertItem(MUSB_LinkedList *pPos, void *pItem,
                                  uint32_t dwCount)
{
    uint8_t bOK = FALSE;
    MUSB_LinkedList *pNext = NULL;

    pNext = (MUSB_LinkedList *)MUSB_MemAlloc(sizeof(MUSB_LinkedList));

    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    if (pNext == NULL)
    {
		MUSB_ERR_PRINTF("MGC_ListInsertItem: MUSB_MemAlloc failed\r\n");
    }
    if (pPos && pNext)
    {
        MUSB_MemSet(pNext, 0, sizeof(MUSB_LinkedList));
        bOK = TRUE;
        pNext->pNext = pPos->pNext;
        pPos->pNext = pNext;
        pNext->pItem = pItem;
        pNext->dwCount = dwCount;
    }
    return bOK;
}

/* Append an item to the end of the given list */
uint8_t MUSB_ListAppendItem(MUSB_LinkedList *pList, void *pItem,
                            unsigned long dwCount)
{
    uint8_t bOK = FALSE;
    MUSB_LinkedList *pNext = pList;

    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    MUSB_DPRINTF("MUSB_ListAppendItem: pList = 0x%p, pList->pItem = 0x%p, pList->pNext = 0x%p, pList->dwCount = 0x%lx\r\n",
            pList, pList->pItem, pList->pNext, pList->dwCount);
    /* sanity */
    if (!pList)
    {
        return FALSE;
    }

    /* check if empty */
    if (!pList->pItem)
    {
        bOK = TRUE;
        pList->pItem = pItem;
        pList->dwCount = dwCount;
        pList->pNext = NULL;
    }
    else
    {
        /* general case */
        while (pNext->pNext)
        {
            pNext = pNext->pNext;
        }
        bOK = MGC_ListInsertItem(pNext, pItem, dwCount);
    }
    return bOK;
}

/* Insert an item at the given position in the given list */
uint8_t MUSB_ListInsertItem(MUSB_LinkedList *pList, uint_fast16_t wItemIndex,
                            void *pItem, unsigned long dwCount)
{
    uint8_t bOK = FALSE;
    MUSB_LinkedList *pNext;

    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    /* sanity */
    if (!pList)
    {
        return FALSE;
    }

    /* check index */
    if (!wItemIndex)
    {
        /* special case for index 0 */
        pNext = (MUSB_LinkedList *)MUSB_MemAlloc(sizeof(MUSB_LinkedList));
        if (pNext)
        {
            MUSB_MemSet(pNext, 0, sizeof(MUSB_LinkedList));
            bOK = TRUE;
            pNext->dwCount = pList->dwCount;
            pNext->pItem = pList->pItem;
            pNext->pNext = pList->pNext;
            pList->dwCount = dwCount;
            pList->pItem = pItem;
            pList->pNext = pNext;
        }
	else
	{
		MUSB_ERR_PRINTF("MUSB_ListInsertItem: MUSB_MemAlloc failed\r\n");
	}
    }
    else
    {
        /* general case */
        pNext = MUSB_ListFindRecord(pList, wItemIndex);
        if (pNext)
        {
            bOK = MGC_ListInsertItem(pNext, pItem, dwCount);
        }
    }
    return bOK;
}

/* Remove an item from the given list */
void MUSB_ListRemoveItem(MUSB_LinkedList *pList, void *pItem)
{
    MUSB_LinkedList *pNext = pList;
    MUSB_LinkedList *pPos;

    MUSB_DPRINTF1("%s\r\n", __FUNCTION__);
    /* sanity */
    if (!pList)
    {
        return;
    }

    /* special case for head */
    if (pList->pItem == pItem)
    {
        /* check if this will result in an empty list (since we never free the head) */
        pNext = pList->pNext;
        if (pNext)
        {
            /* non-empty; copy next record and free its storage */
            pList->pItem = pNext->pItem;
            pList->dwCount = pNext->dwCount;
            pList->pNext = pNext->pNext;
            MUSB_MemFree(pNext);
        }
        else
        {
            /* now the list is empty (head item pointer is NULL) */
            pList->pItem = NULL;
            pList->dwCount = 0L;
            pList->pNext = NULL;
        }
        return;
    }

    /* general case */
    while (pNext->pNext && (pNext->pNext->pItem != pItem))
    {
        pNext = pNext->pNext;
    }
    if (pNext->pNext->pItem == pItem)
    {
        /* found it; unlink and free storage */
        pPos = pNext->pNext;
        /* check if we are removing last element */
        if (pPos->pNext)
        {
            /* not last element; link */
            pNext->pNext->pItem = pPos->pNext->pItem;
            pNext->pNext->dwCount = pPos->pNext->dwCount;
            pNext->pNext = pPos->pNext;
        }
        else
        {
            /* last element; clear link */
            pNext->pNext = NULL;
        }
        MUSB_MemFree(pPos);
    }
}

