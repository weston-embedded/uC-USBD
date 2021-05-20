/*
*********************************************************************************************************
*                                            uC/USB-Device
*                                    The Embedded USB Device Stack
*
*                    Copyright 2004-2021 Silicon Laboratories Inc. www.silabs.com
*
*                                 SPDX-License-Identifier: APACHE-2.0
*
*               This software is subject to an open source license and is distributed by
*                Silicon Laboratories Inc. pursuant to the terms of the Apache License,
*                    Version 2.0 available at www.apache.org/licenses/LICENSE-2.0.
*
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                   USB DEVICE ENDPOINT OPERATIONS
*
* Filename : usbd_ep.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)  : (1) High-speed isochronous transfer not supported.
*
*            (2) 'goto' statements were used in this software module. Their usage is restricted to
*                 cleanup purposes in exceptional program flow (e.g. error handling), in compliance
*                 with CERT MEM12-C and MISRA C:2012 rules 15.2, 15.3 and 15.4.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#include  "usbd_core.h"
#include  "usbd_internal.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  USBD_EP_ADDR_CTRL_OUT                          0x00u
#define  USBD_EP_ADDR_CTRL_IN                           0x80u

#define  USBD_URB_MAX_NBR                      (USBD_CFG_MAX_NBR_URB_EXTRA + \
                                                USBD_CFG_MAX_NBR_EP_OPEN)

#define  USBD_URB_FLAG_XFER_END                 DEF_BIT_00      /* Flag indicating if xfer requires a ZLP to complete.  */
#define  USBD_URB_FLAG_EXTRA_URB                DEF_BIT_01      /* Flag indicating if the URB is an 'extra' URB.        */


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           ENDPOINT STATES
*********************************************************************************************************
*/

typedef  enum  usbd_ep_state {
    USBD_EP_STATE_CLOSE = 0,
    USBD_EP_STATE_OPEN,
    USBD_EP_STATE_STALL
} USBD_EP_STATE;


/*
*********************************************************************************************************
*                                           TRANSFER STATES
*
* Note(s): (1) If an asynchronous transfer cannot be fully queued in the driver, no more transfer can be
*              queued, to respect the transfers sequence.
*              For example, if a driver can queue only 512 bytes at once and the class/application needs
*              to queue 518 bytes, the first 512 bytes will be queued and it will be impossible to queue
*              another transaction. The remaining 6 bytes will only be queued when the previous (512
*              bytes) transaction completes. The state of the endpoint will be changed to
*              USBD_EP_XFER_TYPE_ASYNC_PARTIAL and other transfers could be queued after this one.
*********************************************************************************************************
*/

typedef  enum  usbd_xfer_state {
    USBD_XFER_STATE_NONE = 0,                                   /* No            xfer    in progress.                   */
    USBD_XFER_STATE_SYNC,                                       /* Sync          xfer    in progress.                   */
    USBD_XFER_STATE_ASYNC,                                      /* Async         xfer(s) in progress.                   */
    USBD_XFER_STATE_ASYNC_PARTIAL                               /* Partial async xfer(s) in progress (see Note #1).     */
} USBD_XFER_STATE;


/*
*********************************************************************************************************
*                                              URB STATES
*********************************************************************************************************
*/

typedef  enum  usbd_urb_state {
    USBD_URB_STATE_IDLE = 0,                                    /* URB is in the memory pool, not used by any EP.       */
    USBD_URB_STATE_XFER_SYNC,                                   /* URB is used for a   sync xfer.                       */
    USBD_URB_STATE_XFER_ASYNC                                   /* URB is used for an async xfer.                       */
} USBD_URB_STATE;


/*
*********************************************************************************************************
*                                ENDPOINT USB REQUEST BLOCK DATA TYPE
*
* Note(s): (1) The 'Flags' field is used as a bitmap. The following bits are used:
*
*                   D7..2 Reserved (reset to zero)
*                   D1    End-of-transfer:
*                               If this bit is set and transfer length is multiple of maximum packet
*                               size, a zero-length packet is transferred to indicate a short transfer to
*                               the host.
*                   D0    Extra URB:
*                               If this bit is set, it indicates that this URB is considered an 'extra'
*                               URB, that is shared amongst all endpoints. If this bit is cleared, it
*                               indicates that this URB is 'reserved' to allow every endpoint to have at
*                               least one URB available at any time.
*
*********************************************************************************************************
*/

typedef  struct  usbd_urb {
    CPU_INT08U        *BufPtr;                                  /* Pointer to buffer.                                   */
    CPU_INT32U         BufLen;                                  /* Buffer length.                                       */
    CPU_INT32U         XferLen;                                 /* Length that has been transferred.                    */
    CPU_INT32U         NextXferLen;                             /* Length of the next transfer.                         */
    CPU_INT08U         Flags;                                   /* Flags (see Note #1).                                 */
    USBD_URB_STATE     State;                                   /* State of the transaction.                            */
    USBD_ASYNC_FNCT    AsyncFnct;                               /* Asynchronous notification function.                  */
    void              *AsyncFnctArg;                            /* Asynchronous function argument.                      */
    USBD_ERR           Err;                                     /* Error passed to callback, if any.                    */
    struct  usbd_urb  *NextPtr;                                 /* Pointer to next     URB in list.                     */
} USBD_URB;


/*
*********************************************************************************************************
*                                         ENDPOINT DATA TYPE
*********************************************************************************************************
*/

typedef  struct  usbd_ep {
    USBD_EP_STATE     State;                                    /* EP   state.                                          */
    USBD_XFER_STATE   XferState;                                /* Xfer state.                                          */
    CPU_INT08U        Addr;                                     /* Address.                                             */
    CPU_INT08U        Attrib;                                   /* Attributes.                                          */
    CPU_INT16U        MaxPktSize;                               /* Maximum packet size.                                 */
    CPU_INT08U        Interval;                                 /* Interval.                                            */
    CPU_INT08U        TransPerFrame;                            /* Transaction per microframe (HS only).                */
    CPU_INT08U        Ix;                                       /* Allocation index.                                    */
#if (USBD_CFG_MAX_NBR_URB_EXTRA > 0u)
    CPU_BOOLEAN       URB_MainAvail;                            /* Flag indicating if main URB associated to EP avail.  */
#endif
    USBD_URB         *URB_HeadPtr;                              /* USB request block head of the list.                  */
    USBD_URB         *URB_TailPtr;                              /* USB request block tail of the list.                  */
} USBD_EP;


/*
*********************************************************************************************************
*                                            LOCAL MACROS
*********************************************************************************************************
*/


#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
#define  USBD_DBG_EP(msg, ep_addr)                   USBD_DBG_GENERIC((msg),                                                \
                                                                      (ep_addr),                                            \
                                                                       USBD_IF_NBR_NONE)

#define  USBD_ERR_LINE_0(x)                          #x
#define  USBD_ERR_LINE(x)                            USBD_ERR_LINE_0(x)
#define  USBD_DBG_EP_ERR(msg, ep_addr, err)          USBD_DBG_GENERIC_ERR( msg " line: "USBD_ERR_LINE(__LINE__)" error: ", \
                                                                           ep_addr,                                         \
                                                                           USBD_IF_NBR_NONE,                                \
                                                                          (err))

#define  USBD_DBG_EP_ARG(msg, ep_addr, arg)          USBD_DBG_GENERIC_ARG((msg),                                            \
                                                                           ep_addr,                                         \
                                                                           USBD_IF_NBR_NONE,                                \
                                                                          (arg))


#else
#define  USBD_DBG_EP(msg, ep_addr)
#define  USBD_DBG_EP_ERR(msg, ep_addr, err)
#define  USBD_DBG_EP_ARG(msg, ep_addr, arg)
#endif


/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

                                                                /* Endpoint structures table.                           */
static  USBD_EP             USBD_EP_Tbl[USBD_CFG_MAX_NBR_DEV][USBD_CFG_MAX_NBR_EP_OPEN];
static  USBD_EP            *USBD_EP_TblPtrs[USBD_CFG_MAX_NBR_DEV][USBD_EP_MAX_NBR];
static  CPU_INT08U          USBD_EP_OpenCtr[USBD_CFG_MAX_NBR_DEV];
static  CPU_INT32U          USBD_EP_OpenBitMap[USBD_CFG_MAX_NBR_DEV];
static  USBD_URB            USBD_URB_Tbl[USBD_CFG_MAX_NBR_DEV][USBD_URB_MAX_NBR];
static  USBD_URB           *USBD_URB_TblPtr[USBD_CFG_MAX_NBR_DEV];
#if (USBD_CFG_MAX_NBR_URB_EXTRA > 0u)
static  CPU_INT08U          USBD_URB_ExtraCtr[USBD_CFG_MAX_NBR_DEV];   /* Nbr of extra URB currently used.                     */
#endif
#if (USBD_CFG_DBG_STATS_EN == DEF_ENABLED)
        USBD_DBG_STATS_EP   USBD_DbgStatsEP_Tbl[USBD_CFG_MAX_NBR_DEV][USBD_CFG_MAX_NBR_EP_OPEN];
#endif


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void          USBD_EP_RxStartAsyncProcess(USBD_DRV         *p_drv,
                                                  USBD_EP          *p_ep,
                                                  USBD_URB         *p_urb,
                                                  CPU_INT08U       *p_buf_cur,
                                                  CPU_INT32U        len,
                                                  USBD_ERR         *p_err);

static  void          USBD_EP_TxAsyncProcess     (USBD_DRV         *p_drv,
                                                  USBD_EP          *p_ep,
                                                  USBD_URB         *p_urb,
                                                  CPU_INT08U       *p_buf_cur,
                                                  CPU_INT32U        len,
                                                  USBD_ERR         *p_err);

static  CPU_INT32U    USBD_EP_Rx                 (USBD_DRV         *p_drv,
                                                  USBD_EP          *p_ep,
                                                  void             *p_buf,
                                                  CPU_INT32U        buf_len,
                                                  USBD_ASYNC_FNCT   async_fnct,
                                                  void             *p_async_arg,
                                                  CPU_INT16U        timeout_ms,
                                                  USBD_ERR         *p_err);

static  CPU_INT32U    USBD_EP_Tx                 (USBD_DRV         *p_drv,
                                                  USBD_EP          *p_ep,
                                                  void             *p_buf,
                                                  CPU_INT32U        buf_len,
                                                  USBD_ASYNC_FNCT   async_fnct,
                                                  void             *p_async_arg,
                                                  CPU_INT16U        timeout_ms,
                                                  CPU_BOOLEAN       end,
                                                  USBD_ERR         *p_err);

static  USBD_URB     *USBD_EP_URB_Abort          (USBD_DRV         *p_drv,
                                                  USBD_EP          *p_ep,
                                                  USBD_ERR         *p_err);

static  USBD_URB     *USBD_URB_AsyncCmpl         (USBD_EP          *p_ep,
                                                  USBD_ERR          err);

static  void          USBD_URB_AsyncEnd          (CPU_INT08U        dev_nbr,
                                                  USBD_EP          *p_ep,
                                                  USBD_URB         *p_urb_head);

static  void          USBD_URB_Free              (CPU_INT08U        dev_nbr,
                                                  USBD_EP          *p_ep,
                                                  USBD_URB         *p_urb);

static  USBD_URB     *USBD_URB_Get               (CPU_INT08U        dev_nbr,
                                                  USBD_EP          *p_ep,
                                                  USBD_ERR         *p_err);

static  void          USBD_URB_Queue             (USBD_EP          *p_ep,
                                                  USBD_URB         *p_urb);

static  void          USBD_URB_Dequeue           (USBD_EP          *p_ep);


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                       BULK TRANSFER FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            USBD_BulkRx()
*
* Description : Receive data on Bulk OUT endpoint.
*
* Argument(s) : dev_nbr     Device number.
*
*               ep_addr     Endpoint address.
*
*               p_buf       Pointer to destination buffer to receive data (see Note #1).
*
*               buf_len     Number of octets to receive.
*
*               timeout_ms  Timeout in milliseconds.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE               Data successfully received.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE  Transfer type only available if device is in
*                                                           configured state.
*                               USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                               USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                               USBD_ERR_EP_INVALID_TYPE    Invalid endpoint type.
*
*                               - RETURNED BY USBD_OS_EP_LockAcquire() -
*                               See USBD_OS_EP_LockAcquire() for additional return error codes.
*
*                               - RETURNED BY USBD_EP_Rx() -
*                               See USBD_EP_Rx() for additional return error codes.
*
* Return(s)   : Number of octets received, if NO error(s).
*
*               0,                         otherwise.
*
* Note(s)     : (1) Receive buffer must be at least aligned on a word.
*********************************************************************************************************
*/

CPU_INT32U  USBD_BulkRx (CPU_INT08U   dev_nbr,
                         CPU_INT08U   ep_addr,
                         void        *p_buf,
                         CPU_INT32U   buf_len,
                         CPU_INT16U   timeout_ms,
                         USBD_ERR    *p_err)
{
    USBD_EP         *p_ep;
    USBD_DRV        *p_drv;
    USBD_DEV_STATE   state;
    CPU_INT32U       xfer_len;
    CPU_INT08U       ep_phy_nbr;


    USBD_DBG_STATS_DEV_INC(dev_nbr, BulkRxSyncExecNbr);

#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return (0u);
    }

    state = USBD_DevStateGet(dev_nbr, p_err);
    if (state != USBD_DEV_STATE_CONFIGURED) {                   /* EP transfers are ONLY allowed in cfg'd state.        */
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return (0u);
    }

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
    p_ep       = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];

    if (p_ep == (USBD_EP *)0) {
       *p_err = USBD_ERR_EP_INVALID_ADDR;
        return (0u);
    }

    USBD_OS_EP_LockAcquire(p_drv->DevNbr,
                           p_ep->Ix,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (0u);
    }

    if (p_ep->State != USBD_EP_STATE_OPEN) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_STATE;
        return (0u);
    }

                                                                /* Chk EP attrib.                                       */
    if (((p_ep->Attrib & USBD_EP_TYPE_MASK) != USBD_EP_TYPE_BULK) ||
        ((ep_addr      & USBD_EP_DIR_MASK)  != USBD_EP_DIR_OUT)) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_TYPE;
        return (0u);
    }

    xfer_len = USBD_EP_Rx(                 p_drv,               /* Call generic EP rx fnct.                             */
                                           p_ep,
                                           p_buf,
                                           buf_len,
                          (USBD_ASYNC_FNCT)0,
                          (void          *)0,
                                           timeout_ms,
                                           p_err);

    USBD_OS_EP_LockRelease(p_drv->DevNbr,
                           p_ep->Ix);

    USBD_DBG_STATS_DEV_INC_IF_TRUE(dev_nbr, BulkRxSyncSuccessNbr, (*p_err == USBD_ERR_NONE));

    return (xfer_len);
}


/*
*********************************************************************************************************
*                                         USBD_BulkRxAsync()
*
* Description : Receive data on Bulk OUT endpoint asynchronously.
*
* Argument(s) : dev_nbr         Device number.
*
*               ep_addr         Endpoint address.
*
*               p_buf           Pointer to destination buffer to receive data (see Note #1).
*
*               buf_len         Number of octets to receive.
*
*               async_fnct      Function that will be invoked upon completion of receive operation.
*
*               p_async_arg     Pointer to argument that will be passed as parameter of 'async_fnct'.
*
*               p_err           Pointer to variable that will receive return error code from this function :
*
*                                   USBD_ERR_NONE               Data successfully received.
*                                   USBD_ERR_NULL_PTR           Parameter 'async_fnct' is a null pointer.
*                                   USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                                   USBD_ERR_DEV_INVALID_STATE  Transfer type only available if device is in
*                                                               configured state.
*                                   USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                                   USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                                   USBD_ERR_EP_INVALID_TYPE    Invalid endpoint type.
*
*                                   - RETURNED BY USBD_OS_EP_LockAcquire() -
*                                   See USBD_OS_EP_LockAcquire() for additional return error codes.
*
*                                   - RETURNED BY USBD_EP_Rx() -
*                                   See USBD_EP_Rx() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Receive buffer must be at least aligned on a word.
*********************************************************************************************************
*/

void  USBD_BulkRxAsync (CPU_INT08U        dev_nbr,
                        CPU_INT08U        ep_addr,
                        void             *p_buf,
                        CPU_INT32U        buf_len,
                        USBD_ASYNC_FNCT   async_fnct,
                        void             *p_async_arg,
                        USBD_ERR         *p_err)
{
    USBD_EP         *p_ep;
    USBD_DRV        *p_drv;
    USBD_DEV_STATE   state;
    CPU_INT08U       ep_phy_nbr;


    USBD_DBG_STATS_DEV_INC(dev_nbr, BulkRxAsyncExecNbr);

#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }

    if (async_fnct == (USBD_ASYNC_FNCT)0) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }
#endif

    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return;
    }

    state = USBD_DevStateGet(dev_nbr, p_err);
    if (state != USBD_DEV_STATE_CONFIGURED) {                   /* EP transfers are ONLY allowed in cfg'd state.        */
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return;
    }

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
    p_ep       = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];

    if (p_ep == (USBD_EP *)0) {
       *p_err = USBD_ERR_EP_INVALID_ADDR;
        return;
    }

    USBD_OS_EP_LockAcquire(p_drv->DevNbr,
                           p_ep->Ix,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    if (p_ep->State != USBD_EP_STATE_OPEN) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_STATE;
        return;
    }

                                                                /* Chk EP attrib.                                       */
    if (((p_ep->Attrib & USBD_EP_TYPE_MASK) != USBD_EP_TYPE_BULK) ||
        ((ep_addr      & USBD_EP_DIR_MASK)  != USBD_EP_DIR_OUT)) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_TYPE;
        return;
    }

    (void)USBD_EP_Rx(p_drv,                                     /* Call generic EP rx fnct.                             */
                     p_ep,
                     p_buf,
                     buf_len,
                     async_fnct,
                     p_async_arg,
                     0u,
                     p_err);

    USBD_OS_EP_LockRelease(p_drv->DevNbr,
                           p_ep->Ix);

    USBD_DBG_STATS_DEV_INC_IF_TRUE(dev_nbr, BulkRxAsyncSuccessNbr, (*p_err == USBD_ERR_NONE));
}


/*
*********************************************************************************************************
*                                            USBD_BulkTx()
*
* Description : Send data on Bulk IN endpoint.
*
* Argument(s) : dev_nbr     Device number.
*
*               ep_addr     Endpoint address.
*
*               p_buf       Pointer to buffer of data that will be transmitted (see Note #2).
*
*               buf_len     Number of octets to transmit.
*
*               timeout_ms  Timeout in milliseconds.
*
*               end         End-of-transfer flag (see Note #3).
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE               Data successfully transmitted.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE  Transfer type only available if device is in
*                                                           configured state.
*                               USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                               USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                               USBD_ERR_EP_INVALID_TYPE    Invalid endpoint type.
*
*                               - RETURNED BY USBD_OS_EP_LockAcquire() -
*                               See USBD_OS_EP_LockAcquire() for additional return error codes.
*
*                               - RETURNED BY USBD_EP_Tx() -
*                               See USBD_EP_Tx() for additional return error codes.
*
* Return(s)   : Number of octets transmitted, if NO error(s).
*
*               0,                            otherwise.
*
* Note(s)     : (1) This function SHOULD NOT be called from interrupt service routine (ISR).
*
*               (2) Transmit buffer must be at least aligned on a word.
*
*               (3) If end-of-transfer is set and transfer length is multiple of maximum packet size,
*                   a zero-length packet is transferred to indicate a short transfer to the host.
*********************************************************************************************************
*/

CPU_INT32U  USBD_BulkTx (CPU_INT08U    dev_nbr,
                         CPU_INT08U    ep_addr,
                         void         *p_buf,
                         CPU_INT32U    buf_len,
                         CPU_INT16U    timeout_ms,
                         CPU_BOOLEAN   end,
                         USBD_ERR     *p_err)
{
    USBD_EP         *p_ep;
    USBD_DRV        *p_drv;
    USBD_DEV_STATE   state;
    CPU_INT32U       xfer_len;
    CPU_INT08U       ep_phy_nbr;


    USBD_DBG_STATS_DEV_INC(dev_nbr, BulkTxSyncExecNbr);

#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return (0u);
    }

    state = USBD_DevStateGet(dev_nbr, p_err);
    if (state != USBD_DEV_STATE_CONFIGURED) {                   /* EP transfers are ONLY allowed in cfg'd state.        */
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return (0u);
    }

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
    p_ep       = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];

    if (p_ep == (USBD_EP *)0) {
       *p_err = USBD_ERR_EP_INVALID_ADDR;
        return (0u);
    }

    USBD_OS_EP_LockAcquire(p_drv->DevNbr,
                           p_ep->Ix,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (0u);
    }

    if (p_ep->State != USBD_EP_STATE_OPEN) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_STATE;
        return (0u);
    }
                                                                /* Chk EP attrib.                                       */
    if (((p_ep->Attrib & USBD_EP_TYPE_MASK) != USBD_EP_TYPE_BULK) ||
        ((ep_addr      & USBD_EP_DIR_MASK)  != USBD_EP_DIR_IN)) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_TYPE;
        return (0u);
    }

    xfer_len = USBD_EP_Tx(                 p_drv,               /* Call generic EP tx fnct.                             */
                                           p_ep,
                                           p_buf,
                                           buf_len,
                          (USBD_ASYNC_FNCT)0,
                          (void          *)0,
                                           timeout_ms,
                                           end,
                                           p_err);

    USBD_OS_EP_LockRelease(p_drv->DevNbr,
                           p_ep->Ix);

    USBD_DBG_STATS_DEV_INC_IF_TRUE(dev_nbr, BulkTxSyncSuccessNbr, (*p_err == USBD_ERR_NONE));

    return (xfer_len);
}


/*
*********************************************************************************************************
*                                         USBD_BulkTxAsync()
*
* Description : Send data on Bulk IN endpoint asynchronously.
*
* Argument(s) : dev_nbr         Device number.
*
*               ep_addr         Endpoint address.
*
*               p_buf           Pointer to buffer of data that will be transmitted (see Note #1).
*
*               buf_len         Number of octets to transmit.
*
*               async_fnct      Function that will be invoked upon completion of transmit operation.
*
*               p_async_arg     Pointer to argument that will be passed as parameter of 'async_fnct'.
*
*               end             End-of-transfer flag (see Note #2).
*
*               p_err           Pointer to variable that will receive return error code from this function :
*
*                                   USBD_ERR_NONE               Data successfully transmitted.
*                                   USBD_ERR_NULL_PTR           Parameter 'async_fnct' is a null pointer.
*                                   USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                                   USBD_ERR_DEV_INVALID_STATE  Transfer type only available if device is in
*                                                               configured state.
*                                   USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                                   USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                                   USBD_ERR_EP_INVALID_TYPE    Invalid endpoint type.
*
*                                   - RETURNED BY USBD_OS_EP_LockAcquire() -
*                                   See USBD_OS_EP_LockAcquire() for additional return error codes.
*
*                                   - RETURNED BY USBD_EP_Tx() -
*                                   See USBD_EP_Tx() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Transmit buffer must be at least aligned on a word.
*
*               (2) If end-of-transfer is set and transfer length is multiple of maximum packet size,
*                   a zero-length packet is transferred to indicate a short transfer to the host.
*********************************************************************************************************
*/

void  USBD_BulkTxAsync (CPU_INT08U        dev_nbr,
                        CPU_INT08U        ep_addr,
                        void             *p_buf,
                        CPU_INT32U        buf_len,
                        USBD_ASYNC_FNCT   async_fnct,
                        void             *p_async_arg,
                        CPU_BOOLEAN       end,
                        USBD_ERR         *p_err)
{
    USBD_EP         *p_ep;
    USBD_DRV        *p_drv;
    USBD_DEV_STATE   state;
    CPU_INT08U       ep_phy_nbr;


    USBD_DBG_STATS_DEV_INC(dev_nbr, BulkTxAsyncExecNbr);

#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }

    if (async_fnct == (USBD_ASYNC_FNCT)0) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }
#endif

    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return;
    }

    state = USBD_DevStateGet(dev_nbr, p_err);
    if (state != USBD_DEV_STATE_CONFIGURED) {                   /* EP transfers are ONLY allowed in cfg'd state.        */
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return;
    }

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
    p_ep       = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];

    if (p_ep == (USBD_EP *)0) {
       *p_err = USBD_ERR_EP_INVALID_ADDR;
        return;
    }

    USBD_OS_EP_LockAcquire(p_drv->DevNbr,
                           p_ep->Ix,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    if (p_ep->State != USBD_EP_STATE_OPEN) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_STATE;
        return;
    }

                                                                /* Chk EP attrib.                                       */
    if (((p_ep->Attrib & USBD_EP_TYPE_MASK) != USBD_EP_TYPE_BULK) ||
        ((ep_addr      & USBD_EP_DIR_MASK)  != USBD_EP_DIR_IN)) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_TYPE;
        return;
    }

   (void)USBD_EP_Tx(p_drv,
                    p_ep,
                    p_buf,
                    buf_len,
                    async_fnct,
                    p_async_arg,
                    0u,
                    end,
                    p_err);

   USBD_OS_EP_LockRelease(p_drv->DevNbr,
                          p_ep->Ix);

   USBD_DBG_STATS_DEV_INC_IF_TRUE(dev_nbr, BulkTxAsyncSuccessNbr, (*p_err == USBD_ERR_NONE));
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                    INTERRUPT TRANSFER FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            USBD_IntrRx()
*
* Description : Receive data on Interrupt OUT endpoint.
*
* Argument(s) : dev_nbr     Device number.
*
*               ep_addr     Endpoint address.
*
*               p_buf       Pointer to destination buffer to receive data (see Note #2).
*
*               buf_len     Number of octets to receive.
*
*               timeout_ms  Timeout in milliseconds.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE               Data successfully received.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE  Transfer type only available if device is in
*                                                           configured state.
*                               USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                               USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                               USBD_ERR_EP_INVALID_TYPE    Invalid endpoint type.
*
*                               - RETURNED BY USBD_OS_EP_LockAcquire() -
*                               See USBD_OS_EP_LockAcquire() for additional return error codes.
*
*                               - RETURNED BY USBD_EP_Rx() -
*                               See USBD_EP_Rx() for additional return error codes.
*
* Return(s)   : Number of octets received, if NO error(s).
*
*               0,                         otherwise.
*
* Note(s)     : (1) This function SHOULD NOT be called from interrupt service routine (ISR).
*
*               (2) Receive buffer must be at least aligned on a word.
*********************************************************************************************************
*/

CPU_INT32U  USBD_IntrRx (CPU_INT08U   dev_nbr,
                         CPU_INT08U   ep_addr,
                         void        *p_buf,
                         CPU_INT32U   buf_len,
                         CPU_INT16U   timeout_ms,
                         USBD_ERR    *p_err)
{
    USBD_EP         *p_ep;
    USBD_DRV        *p_drv;
    USBD_DEV_STATE   state;
    CPU_INT32U       xfer_len;
    CPU_INT08U       ep_phy_nbr;


    USBD_DBG_STATS_DEV_INC(dev_nbr, IntrRxSyncExecNbr);

#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return (0u);
    }

    state = USBD_DevStateGet(dev_nbr, p_err);
    if (state != USBD_DEV_STATE_CONFIGURED) {                   /* EP transfers are ONLY allowed in cfg'd state.        */
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return (0u);
    }

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
    p_ep       = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];

    if (p_ep == (USBD_EP *)0) {
       *p_err = USBD_ERR_EP_INVALID_ADDR;
        return (0u);
    }

    USBD_OS_EP_LockAcquire(p_drv->DevNbr,
                           p_ep->Ix,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (0u);
    }

    if (p_ep->State != USBD_EP_STATE_OPEN) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_STATE;
        return (0u);
    }
                                                                /* Chk EP attrib.                                       */
    if (((p_ep->Attrib & USBD_EP_TYPE_MASK) != USBD_EP_TYPE_INTR) ||
        ((ep_addr      & USBD_EP_DIR_MASK)  != USBD_EP_DIR_OUT)) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_TYPE;
        return (0u);
    }

    xfer_len = USBD_EP_Rx(                 p_drv,
                                           p_ep,
                                           p_buf,
                                           buf_len,
                          (USBD_ASYNC_FNCT)0,
                          (void          *)0,
                                           timeout_ms,
                                           p_err);

    USBD_OS_EP_LockRelease(p_drv->DevNbr,
                           p_ep->Ix);

    USBD_DBG_STATS_DEV_INC_IF_TRUE(dev_nbr, IntrRxSyncSuccessNbr, (*p_err == USBD_ERR_NONE));

    return (xfer_len);
}


/*
*********************************************************************************************************
*                                         USBD_IntrRxAsync()
*
* Description : Receive data on Interrupt OUT endpoint asynchronously.
*
* Argument(s) : dev_nbr         Device number.
*
*               ep_addr         Endpoint address.
*
*               p_buf           Pointer to destination buffer to receive data (see Note #1).
*
*               buf_len         Number of octets to receive.
*
*               async_fnct      Function that will be invoked upon completion of receive operation.
*
*               p_async_arg     Pointer to argument that will be passed as parameter of 'async_fnct'.
*
*               p_err           Pointer to variable that will receive return error code from this function :
*
*                                   USBD_ERR_NONE               Data successfully received.
*                                   USBD_ERR_NULL_PTR           Parameter 'async_fnct' is a null pointer.
*                                   USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                                   USBD_ERR_DEV_INVALID_STATE  Transfer type only available if device is in
*                                                               configured state.
*                                   USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                                   USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                                   USBD_ERR_EP_INVALID_TYPE    Invalid endpoint type.
*
*                                   - RETURNED BY USBD_OS_EP_LockAcquire() -
*                                   See USBD_OS_EP_LockAcquire() for additional return error codes.
*
*                                   - RETURNED BY USBD_EP_Rx() -
*                                   See USBD_EP_Rx() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Receive buffer must be at least aligned on a word.
*********************************************************************************************************
*/

void  USBD_IntrRxAsync (CPU_INT08U        dev_nbr,
                        CPU_INT08U        ep_addr,
                        void             *p_buf,
                        CPU_INT32U        buf_len,
                        USBD_ASYNC_FNCT   async_fnct,
                        void             *p_async_arg,
                        USBD_ERR         *p_err)
{
    USBD_EP         *p_ep;
    USBD_DRV        *p_drv;
    CPU_INT08U       ep_phy_nbr;
    USBD_DEV_STATE   state;


    USBD_DBG_STATS_DEV_INC(dev_nbr, IntrRxAsyncExecNbr);

#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }

    if (async_fnct == (USBD_ASYNC_FNCT)0) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }
#endif

    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return;
    }

    state = USBD_DevStateGet(dev_nbr, p_err);
    if (state != USBD_DEV_STATE_CONFIGURED) {                   /* EP transfers are ONLY allowed in cfg'd state.        */
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return;
    }

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
    p_ep       = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];

    if (p_ep == (USBD_EP *)0) {
       *p_err = USBD_ERR_EP_INVALID_ADDR;
        return;
    }

    USBD_OS_EP_LockAcquire(p_drv->DevNbr,
                           p_ep->Ix,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    if (p_ep->State != USBD_EP_STATE_OPEN) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_STATE;
        return;
    }
                                                                /* Chk EP attrib.                                       */
    if (((p_ep->Attrib & USBD_EP_TYPE_MASK) != USBD_EP_TYPE_INTR) ||
        ((ep_addr      & USBD_EP_DIR_MASK)  != USBD_EP_DIR_OUT)) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_TYPE;
        return;
    }

    (void)USBD_EP_Rx(p_drv,                                     /* Call generic EP rx fnct.                             */
                     p_ep,
                     p_buf,
                     buf_len,
                     async_fnct,
                     p_async_arg,
                     0u,
                     p_err);

    USBD_OS_EP_LockRelease(p_drv->DevNbr,
                           p_ep->Ix);

    USBD_DBG_STATS_DEV_INC_IF_TRUE(dev_nbr, IntrRxAsyncSuccessNbr, (*p_err == USBD_ERR_NONE));
}


/*
*********************************************************************************************************
*                                          USBD_EP_IntrTx()
*
* Description : Send data on Interrupt IN endpoint.
*
* Argument(s) : dev_nbr     Device number.
*
*               ep_addr     Endpoint address.
*
*               p_buf       Pointer to buffer of data that will be transmitted (see Note #2).
*
*               buf_len     Number of octets to transmit.
*
*               timeout_ms  Timeout in milliseconds.
*
*               end         End-of-transfer flag (see Note #3).
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE               Data successfully transmitted.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE  Transfer type only available if device is in
*                                                           configured state.
*                               USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                               USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                               USBD_ERR_EP_INVALID_TYPE    Invalid endpoint type.
*
*                               - RETURNED BY USBD_OS_EP_LockAcquire() -
*                               See USBD_OS_EP_LockAcquire() for additional return error codes.
*
*                               - RETURNED BY USBD_EP_Tx() -
*                               See USBD_EP_Tx() for additional return error codes.
*
* Return(s)   : Number of octets transmitted, if NO error(s).
*
*               0,                            otherwise.
*
* Note(s)     : (1) This function SHOULD NOT be called from interrupt service routine (ISR).
*
*               (2) Transmit buffer must be at least aligned on a word.
*
*               (3) If end-of-transfer is set and transfer length is multiple of maximum packet size,
*                   a zero-length packet is transferred to indicate a short transfer to the host.
*********************************************************************************************************
*/

CPU_INT32U  USBD_IntrTx (CPU_INT08U    dev_nbr,
                         CPU_INT08U    ep_addr,
                         void         *p_buf,
                         CPU_INT32U    buf_len,
                         CPU_INT16U    timeout_ms,
                         CPU_BOOLEAN   end,
                         USBD_ERR     *p_err)
{
    USBD_EP         *p_ep;
    USBD_DRV        *p_drv;
    CPU_INT08U       ep_phy_nbr;
    CPU_INT32U       xfer_len;
    USBD_DEV_STATE   state;


    USBD_DBG_STATS_DEV_INC(dev_nbr, IntrTxSyncExecNbr);

#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return (0u);
    }

    state = USBD_DevStateGet(dev_nbr, p_err);
    if (state != USBD_DEV_STATE_CONFIGURED) {                   /* EP transfers are ONLY allowed in cfg'd state.        */
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return (0u);
    }

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
    p_ep       = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];

    if (p_ep == (USBD_EP *)0) {
       *p_err = USBD_ERR_EP_INVALID_ADDR;
        return (0u);
    }

    USBD_OS_EP_LockAcquire(p_drv->DevNbr,
                           p_ep->Ix,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (0u);
    }

    if (p_ep->State != USBD_EP_STATE_OPEN) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_STATE;
        return (0u);
    }
                                                                /* Chk EP attrib.                                       */
    if (((p_ep->Attrib & USBD_EP_TYPE_MASK) != USBD_EP_TYPE_INTR) ||
        ((ep_addr      & USBD_EP_DIR_MASK)  != USBD_EP_DIR_IN)) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_TYPE;
        return (0u);
    }

    xfer_len = USBD_EP_Tx(                 p_drv,
                                           p_ep,
                                           p_buf,
                                           buf_len,
                          (USBD_ASYNC_FNCT)0,
                          (void          *)0,
                                           timeout_ms,
                                           end,
                                           p_err);

    USBD_OS_EP_LockRelease(p_drv->DevNbr,
                           p_ep->Ix);

    USBD_DBG_STATS_DEV_INC_IF_TRUE(dev_nbr, IntrTxSyncSuccessNbr, (*p_err == USBD_ERR_NONE));

    return (xfer_len);
}


/*
*********************************************************************************************************
*                                         USBD_IntrTxAsync()
*
* Description : Send data on Interrupt IN endpoint asynchronously.
*
* Argument(s) : dev_nbr         Device number.
*
*               ep_addr         Endpoint address.
*
*               p_buf           Pointer to buffer of data that will be transmitted (see Note #1).
*
*               buf_len         Number of octets to transmit.
*
*               async_fnct      Function that will be invoked upon completion of transmit operation.
*
*               p_async_arg     Pointer to argument that will be passed as parameter of 'async_fnct'.
*
*               end             End-of-transfer flag (see Note #2).
*
*               p_err           Pointer to variable that will receive return error code from this function :
*
*                                   USBD_ERR_NONE               Data successfully transmitted.
*                                   USBD_ERR_NULL_PTR           Parameter 'async_fnct' is a null pointer.
*                                   USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                                   USBD_ERR_DEV_INVALID_STATE  Transfer type only available if device is in
*                                                               configured state.
*                                   USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                                   USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                                   USBD_ERR_EP_INVALID_TYPE    Invalid endpoint type.
*
*                                   - RETURNED BY USBD_OS_EP_LockAcquire() -
*                                   See USBD_OS_EP_LockAcquire() for additional return error codes.
*
*                                   - RETURNED BY USBD_EP_Tx() -
*                                   See USBD_EP_Tx() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Transmit buffer must be at least aligned on a word.
*
*               (2) If end-of-transfer is set and transfer length is multiple of maximum packet size,
*                   a zero-length packet is transferred to indicate a short transfer to the host.
*********************************************************************************************************
*/

void  USBD_IntrTxAsync (CPU_INT08U        dev_nbr,
                        CPU_INT08U        ep_addr,
                        void             *p_buf,
                        CPU_INT32U        buf_len,
                        USBD_ASYNC_FNCT   async_fnct,
                        void             *p_async_arg,
                        CPU_BOOLEAN       end,
                        USBD_ERR         *p_err)
{
    USBD_EP         *p_ep;
    USBD_DRV        *p_drv;
    CPU_INT08U       ep_phy_nbr;
    USBD_DEV_STATE   state;


    USBD_DBG_STATS_DEV_INC(dev_nbr, IntrTxAsyncExecNbr);

#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }

    if (async_fnct == (USBD_ASYNC_FNCT)0) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }
#endif

    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return;
    }

    state = USBD_DevStateGet(dev_nbr, p_err);
    if (state != USBD_DEV_STATE_CONFIGURED) {                   /* EP transfers are ONLY allowed in cfg'd state.        */
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return;
    }

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
    p_ep       = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];

    if (p_ep == (USBD_EP *)0) {
       *p_err = USBD_ERR_EP_INVALID_ADDR;
        return;
    }

    USBD_OS_EP_LockAcquire(p_drv->DevNbr,
                           p_ep->Ix,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    if (p_ep->State != USBD_EP_STATE_OPEN) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_STATE;
        return;
    }
                                                                /* Chk EP attrib.                                       */
    if (((p_ep->Attrib & USBD_EP_TYPE_MASK) != USBD_EP_TYPE_INTR) ||
        ((ep_addr      & USBD_EP_DIR_MASK)  != USBD_EP_DIR_IN)) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_TYPE;
        return;
    }

    (void)USBD_EP_Tx(p_drv,
                     p_ep,
                     p_buf,
                     buf_len,
                     async_fnct,
                     p_async_arg,
                     0u,
                     end,
                     p_err);

    USBD_OS_EP_LockRelease(p_drv->DevNbr,
                           p_ep->Ix);

    USBD_DBG_STATS_DEV_INC_IF_TRUE(dev_nbr, IntrTxAsyncSuccessNbr, (*p_err == USBD_ERR_NONE));
}


/*
*********************************************************************************************************
*                                         USBD_IsocRxAsync()
*
* Description : Receive data on isochronous OUT endpoint asynchronously.
*
* Argument(s) : dev_nbr         Device number.
*
*               ep_addr         Endpoint address.
*
*               p_buf           Pointer to destination buffer to receive data (see Note #1).
*
*               buf_len         Number of octets to receive.
*
*               async_fnct      Function that will be invoked upon completion of receive operation.
*
*               p_async_arg     Pointer to argument that will be passed as parameter of 'async_fnct'.
*
*               p_err           Pointer to variable that will receive return error code from this function :
*
*                                   USBD_ERR_NONE               Data successfully received.
*                                   USBD_ERR_NULL_PTR           Parameter 'async_fnct' is a null pointer.
*                                   USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                                   USBD_ERR_DEV_INVALID_STATE  Transfer type only available if device is in
*                                                               configured state.
*                                   USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                                   USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                                   USBD_ERR_EP_INVALID_TYPE    Invalid endpoint type.
*
*                                   - RETURNED BY USBD_OS_EP_LockAcquire() -
*                                   See USBD_OS_EP_LockAcquire() for additional return error codes.
*
*                                   - RETURNED BY USBD_EP_Rx() -
*                                   See USBD_EP_Rx() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Receive buffer must be at least aligned on a word.
*********************************************************************************************************
*/

#if (USBD_CFG_EP_ISOC_EN == DEF_ENABLED)
void  USBD_IsocRxAsync (CPU_INT08U        dev_nbr,
                        CPU_INT08U        ep_addr,
                        void             *p_buf,
                        CPU_INT32U        buf_len,
                        USBD_ASYNC_FNCT   async_fnct,
                        void             *p_async_arg,
                        USBD_ERR         *p_err)
{
    USBD_EP         *p_ep;
    USBD_DRV        *p_drv;
    USBD_DEV_STATE   state;
    CPU_INT08U       ep_phy_nbr;


    USBD_DBG_STATS_DEV_INC(dev_nbr, IsocRxAsyncExecNbr);

#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }

    if (async_fnct == (USBD_ASYNC_FNCT)0) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }
#endif

    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return;
    }

    state = USBD_DevStateGet(dev_nbr, p_err);
    if (state != USBD_DEV_STATE_CONFIGURED) {                   /* EP transfers are ONLY allowed in cfg'd state.        */
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return;
    }

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
    p_ep       = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];

    if (p_ep == (USBD_EP *)0) {
       *p_err = USBD_ERR_EP_INVALID_ADDR;
        return;
    }

    USBD_OS_EP_LockAcquire(p_drv->DevNbr,
                           p_ep->Ix,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    if (p_ep->State != USBD_EP_STATE_OPEN) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_STATE;
        return;
    }
                                                                /* Chk EP attrib.                                       */
    if (((p_ep->Attrib & USBD_EP_TYPE_MASK) != USBD_EP_TYPE_ISOC) ||
        ((ep_addr      & USBD_EP_DIR_MASK)  != USBD_EP_DIR_OUT)) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_TYPE;
        return;
    }

    (void)USBD_EP_Rx(p_drv,                                     /* Call generic EP rx fnct.                             */
                     p_ep,
                     p_buf,
                     buf_len,
                     async_fnct,
                     p_async_arg,
                     0u,
                     p_err);

    USBD_OS_EP_LockRelease(p_drv->DevNbr,
                           p_ep->Ix);

    USBD_DBG_STATS_DEV_INC_IF_TRUE(dev_nbr, IsocRxAsyncSuccessNbr, (*p_err == USBD_ERR_NONE));
}
#endif


/*
*********************************************************************************************************
*                                         USBD_IsocTxAsync()
*
* Description : Send data on isochronous IN endpoint asynchronously.
*
* Argument(s) : dev_nbr         Device number.
*
*               ep_addr         Endpoint address.
*
*               p_buf           Pointer to buffer of data that will be transmitted (see Note #1).
*
*               buf_len         Number of octets to transmit.
*
*               async_fnct      Function that will be invoked upon completion of transmit operation.
*
*               p_async_arg     Pointer to argument that will be passed as parameter of 'async_fnct'.
**
*               p_err           Pointer to variable that will receive return error code from this function :
*
*                                   USBD_ERR_NONE               Data successfully transmitted.
*                                   USBD_ERR_NULL_PTR           Parameter 'async_fnct' is a null pointer.
*                                   USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                                   USBD_ERR_DEV_INVALID_STATE  Transfer type only available if device is in
*                                                               configured state.
*                                   USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                                   USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                                   USBD_ERR_EP_INVALID_TYPE    Invalid endpoint type.
*
*                                   - RETURNED BY USBD_OS_EP_LockAcquire() -
*                                   See USBD_OS_EP_LockAcquire() for additional return error codes.
*
*                                   - RETURNED BY USBD_EP_Tx() -
*                                   See USBD_EP_Tx() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Transmit buffer must be at least aligned on a word.
*********************************************************************************************************
*/

#if (USBD_CFG_EP_ISOC_EN == DEF_ENABLED)
void  USBD_IsocTxAsync (CPU_INT08U        dev_nbr,
                        CPU_INT08U        ep_addr,
                        void             *p_buf,
                        CPU_INT32U        buf_len,
                        USBD_ASYNC_FNCT   async_fnct,
                        void             *p_async_arg,
                        USBD_ERR         *p_err)
{
    USBD_EP         *p_ep;
    USBD_DRV        *p_drv;
    USBD_DEV_STATE   state;
    CPU_INT08U       ep_phy_nbr;


    USBD_DBG_STATS_DEV_INC(dev_nbr, IsocTxAsyncExecNbr);

#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }

    if (async_fnct == (USBD_ASYNC_FNCT)0) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }
#endif

    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return;
    }

    state = USBD_DevStateGet(dev_nbr, p_err);
    if (state != USBD_DEV_STATE_CONFIGURED) {                   /* EP transfers are ONLY allowed in cfg'd state.        */
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return;
    }

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
    p_ep       = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];

    if (p_ep == (USBD_EP *)0) {
       *p_err = USBD_ERR_EP_INVALID_ADDR;
        return;
    }

    USBD_OS_EP_LockAcquire(p_drv->DevNbr,
                           p_ep->Ix,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    if (p_ep->State != USBD_EP_STATE_OPEN) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_STATE;
        return;
    }
                                                                /* Chk EP attrib.                                       */
    if (((p_ep->Attrib & USBD_EP_TYPE_MASK) != USBD_EP_TYPE_ISOC) ||
        ((ep_addr      & USBD_EP_DIR_MASK)  != USBD_EP_DIR_IN)) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_TYPE;
        return;
    }

    (void)USBD_EP_Tx(p_drv,
                     p_ep,
                     p_buf,
                     buf_len,
                     async_fnct,
                     p_async_arg,
                     0u,
                     DEF_NO,
                     p_err);

    USBD_OS_EP_LockRelease(p_drv->DevNbr,
                           p_ep->Ix);

    USBD_DBG_STATS_DEV_INC_IF_TRUE(dev_nbr, IsocTxAsyncSuccessNbr, (*p_err == USBD_ERR_NONE));
}
#endif


/*
*********************************************************************************************************
*                                         USBD_CtrlTxStatus()
*
* Description : Handle status stage from host on control (EP0) IN endpoint.
*
* Argument(s) : dev_nbr     Device number.
*
*               timeout_ms  Timeout in milliseconds.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE               Status stage successfully completed.
*                               USBD_ERR_DEV_INVALID_STATE  Transfer type only available if device is in
*                                                           default/addressed/configured state.
*
*                               - RETURNED BY USBD_DevStateGet() -
*                               See USBD_DevStateGet() for additional return error codes.
*
*                               - RETURNED BY USBD_EP_TxZLP() -
*                               See USBD_EP_TxZLP() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_CtrlTxStatus (CPU_INT08U   dev_nbr,
                         CPU_INT16U   timeout_ms,
                         USBD_ERR    *p_err)
{
    USBD_DEV_STATE  state;


    USBD_DBG_STATS_DEV_INC(dev_nbr, CtrlTxStatusExecNbr);

    state = USBD_DevStateGet(dev_nbr, p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    if ((state != USBD_DEV_STATE_DEFAULT)    &&
        (state != USBD_DEV_STATE_ADDRESSED)  &&
        (state != USBD_DEV_STATE_CONFIGURED)) {
       *p_err  = USBD_ERR_DEV_INVALID_STATE;
        return;
    }

    USBD_EP_TxZLP(dev_nbr,
                  USBD_EP_ADDR_CTRL_IN,
                  timeout_ms,
                  p_err);
    USBD_DBG_STATS_DEV_INC_IF_TRUE(dev_nbr, CtrlTxStatusSuccessNbr, (*p_err == USBD_ERR_NONE));
}


/*
*********************************************************************************************************
*                                            USBD_CtrlRx()
*
* Description : Receive data on Control OUT endpoint.
*
* Argument(s) : dev_nbr     Device number.
*
*               p_buf       Pointer to destination buffer to receive data.
*
*               buf_len     Number of octets to receive.
*
*               timeout_ms  Timeout in milliseconds.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE               Data successfully received.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE  Transfer type only available if device is in
*                                                           default/addressed/configured state.
*                               USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                               USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                               USBD_ERR_EP_INVALID_TYPE    Invalid endpoint type.
*
*                               - RETURNED BY USBD_OS_EP_LockAcquire() -
*                               See USBD_OS_EP_LockAcquire() for additional return error codes.
*
*                               - RETURNED BY USBD_EP_Rx() -
*                               See USBD_EP_Rx() for additional return error codes.
*
* Return(s)   : Number of octets received, if NO error(s).
*
*               0,                         otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT32U  USBD_CtrlRx (CPU_INT08U    dev_nbr,
                         void         *p_buf,
                         CPU_INT32U    buf_len,
                         CPU_INT16U    timeout_ms,
                         USBD_ERR     *p_err)
{
    USBD_DRV        *p_drv;
    USBD_EP         *p_ep;
    CPU_INT08U       ep_phy_nbr;
    USBD_DEV_STATE   state;
    CPU_INT32U       xfer_len;


    USBD_DBG_STATS_DEV_INC(dev_nbr, CtrlRxSyncExecNbr);

#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return (0u);
    }

    state = USBD_DevStateGet(dev_nbr, p_err);
    if ((state != USBD_DEV_STATE_DEFAULT)   &&
        (state != USBD_DEV_STATE_ADDRESSED) &&
        (state != USBD_DEV_STATE_CONFIGURED)) {
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return (0u);
    }

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(USBD_EP_ADDR_CTRL_OUT);
    p_ep       = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];

    if (p_ep == (USBD_EP *)0) {
       *p_err = USBD_ERR_EP_INVALID_ADDR;
        return (0u);
    }

    USBD_OS_EP_LockAcquire(p_drv->DevNbr,
                           p_ep->Ix,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (0u);
    }

    if (p_ep->State != USBD_EP_STATE_OPEN) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_STATE;
        return (0u);
    }
                                                                /* Chk EP attrib.                                       */
    if ((p_ep->Attrib & USBD_EP_TYPE_MASK) != USBD_EP_TYPE_CTRL) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_TYPE;
        return (0u);
    }

    xfer_len = USBD_EP_Rx(                 p_drv,
                                           p_ep,
                                           p_buf,
                                           buf_len,
                          (USBD_ASYNC_FNCT)0,
                          (void          *)0,
                                           timeout_ms,
                                           p_err);

    USBD_OS_EP_LockRelease(p_drv->DevNbr,
                           p_ep->Ix);

    USBD_DBG_STATS_DEV_INC_IF_TRUE(dev_nbr, CtrlRxSyncSuccessNbr, (*p_err == USBD_ERR_NONE));

    return (xfer_len);
}


/*
*********************************************************************************************************
*                                            USBD_CtrlTx()
*
* Description : Send data on Control IN endpoint.
*
* Argument(s) : dev_nbr     Device number.
*
*               p_buf       Pointer to buffer of data that will be sent.
*
*               buf_len     Number of octets to transmit.
*
*               timeout_ms  Timeout in milliseconds.
*
*               end         End-of-transfer flag (see Note #1).
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE               Data successfully transmitted.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE  Transfer type only available if device is in
*                                                           configured state.
*                               USBD_ERR_INVALID_ARG        Invalid argument(s): 'buf_len'.
*                               USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                               USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                               USBD_ERR_EP_INVALID_TYPE    Invalid endpoint type.
*
*                               - RETURNED BY USBD_OS_EP_LockAcquire() -
*                               See USBD_OS_EP_LockAcquire() for additional return error codes.
*
*                               - RETURNED BY USBD_EP_Tx() -
*                               See USBD_EP_Tx() for additional return error codes.
*
* Return(s)   : Number of octets transmitted, if NO error(s).
*
*               0,                            otherwise.
*
* Note(s)     : (1) If end-of-transfer is set and transfer length is multiple of maximum packet size,
*                   a zero-length packet is transferred to indicate a short transfer to the host.
*********************************************************************************************************
*/

CPU_INT32U  USBD_CtrlTx (CPU_INT08U    dev_nbr,
                         void         *p_buf,
                         CPU_INT32U    buf_len,
                         CPU_INT16U    timeout_ms,
                         CPU_BOOLEAN   end,
                         USBD_ERR     *p_err)
{
    USBD_DRV        *p_drv;
    USBD_EP         *p_ep;
    CPU_INT08U       ep_phy_nbr;
    USBD_DEV_STATE   state;
    CPU_INT32U       xfer_len;


    USBD_DBG_STATS_DEV_INC(dev_nbr, CtrlTxSyncExecNbr);

#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return (0u);
    }

    state = USBD_DevStateGet(dev_nbr, p_err);
    if ((state != USBD_DEV_STATE_DEFAULT)   &&
        (state != USBD_DEV_STATE_ADDRESSED) &&
        (state != USBD_DEV_STATE_CONFIGURED)) {
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return (0u);
    }

    if (buf_len == 0u) {
       *p_err = USBD_ERR_INVALID_ARG;
        return (0u);
    }

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(USBD_EP_ADDR_CTRL_IN);
    p_ep       = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];

    if (p_ep == (USBD_EP *)0) {
       *p_err = USBD_ERR_EP_INVALID_ADDR;
        return (0u);
    }

    USBD_OS_EP_LockAcquire(p_drv->DevNbr,
                           p_ep->Ix,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (0u);
    }

    if (p_ep->State != USBD_EP_STATE_OPEN) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_STATE;
        return (0u);
    }
                                                                /* Chk EP attrib.                                       */
    if ((p_ep->Attrib & USBD_EP_TYPE_MASK) != USBD_EP_TYPE_CTRL) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_TYPE;
        return (0u);
    }

    xfer_len = USBD_EP_Tx(                 p_drv,
                                           p_ep,
                                           p_buf,
                                           buf_len,
                          (USBD_ASYNC_FNCT)0,
                          (void          *)0,
                                           timeout_ms,
                                           end,
                                           p_err);

    USBD_OS_EP_LockRelease(p_drv->DevNbr,
                           p_ep->Ix);

    USBD_DBG_STATS_DEV_INC_IF_TRUE(dev_nbr, CtrlTxSyncSuccessNbr, (*p_err == USBD_ERR_NONE));

    return (xfer_len);
}


/*
*********************************************************************************************************
*                                         USBD_CtrlRxStatus()
*
* Description : Handle status stage from host on control (EP0) OUT endpoint.
*
* Argument(s) : dev_nbr     Device number.
*
*               timeout_ms  Timeout in milliseconds.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE               Status stage successfully completed.
*                               USBD_ERR_DEV_INVALID_STATE  Transfer type only available if device is in
*                                                           default/addressed/configured state.
*
*                               - RETURNED BY USBD_DevStateGet() -
*                               See USBD_DevStateGet() for additional return error codes.
*
*                               - RETURNED BY USBD_EP_RxZLP() -
*                               See USBD_EP_RxZLP() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_CtrlRxStatus (CPU_INT08U   dev_nbr,
                         CPU_INT16U   timeout_ms,
                         USBD_ERR    *p_err)
{
    USBD_DEV_STATE  state;


    USBD_DBG_STATS_DEV_INC(dev_nbr, CtrlRxStatusExecNbr);

    state = USBD_DevStateGet(dev_nbr, p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    if ((state != USBD_DEV_STATE_DEFAULT)   &&
        (state != USBD_DEV_STATE_ADDRESSED) &&
        (state != USBD_DEV_STATE_CONFIGURED)) {
       *p_err  = USBD_ERR_DEV_INVALID_STATE;
        return;
    }

    USBD_EP_RxZLP(dev_nbr,
                  USBD_EP_ADDR_CTRL_OUT,
                  timeout_ms,
                  p_err);

    USBD_DBG_STATS_DEV_INC_IF_TRUE(dev_nbr, CtrlRxStatusSuccessNbr, (*p_err == USBD_ERR_NONE));
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                     CONTROL ENDPOINT FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                           USBD_CtrlOpen()
*
* Description : Open control endpoints.
*
* Argument(s) : dev_nbr         Device number.
*
*               max_pkt_size    Maximum packet size.
*
*               p_err           Pointer to variable that will receive return error code from this function :
*
*                                   USBD_ERR_NONE               Control endpoint successfully opened.
*                                   USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*
*                                   - RETURNED BY USBD_EP_Open() -
*                                   See USBD_EP_Open() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_CtrlOpen (CPU_INT08U   dev_nbr,
                     CPU_INT16U   max_pkt_size,
                     USBD_ERR    *p_err)
{
    USBD_DRV  *p_drv;
    USBD_ERR   local_err;


    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return;
    }

    USBD_EP_Open(p_drv,
                 USBD_EP_ADDR_CTRL_IN,
                 max_pkt_size,
                 USBD_EP_TYPE_CTRL,
                 0u,
                 p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    USBD_EP_Open(p_drv,
                 USBD_EP_ADDR_CTRL_OUT,
                 max_pkt_size,
                 USBD_EP_TYPE_CTRL,
                 0u,
                 p_err);
    if (*p_err != USBD_ERR_NONE) {
        USBD_EP_Close(p_drv, USBD_EP_ADDR_CTRL_IN,  &local_err);
        return;
    }

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          USBD_CtrlClose()
*
* Description : Close control endpoint.
*
* Argument(s) : dev_nbr     Device number.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE               Control endpoint successfully closed.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*
*                               - RETURNED BY USBD_EP_Close() -
*                               See USBD_EP_Close() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_CtrlClose (CPU_INT08U   dev_nbr,
                      USBD_ERR    *p_err)
{
    USBD_DRV  *p_drv;
    USBD_ERR   err_in;
    USBD_ERR   err_out;


    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return;
    }

    USBD_EP_Close(p_drv, USBD_EP_ADDR_CTRL_IN,  &err_in);
    USBD_EP_Close(p_drv, USBD_EP_ADDR_CTRL_OUT, &err_out);

    if (err_in != USBD_ERR_NONE) {
       *p_err = err_in;
    } else {
       *p_err = err_out;
    }
}


/*
*********************************************************************************************************
*                                          USBD_CtrlStall()
*
* Description : Stall control endpoint.
*
* Argument(s) : dev_nbr     Device number.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE               Control endpoint successfully stalled.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                               USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                               USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                               USBD_ERR_EP_STALL           Device driver stall endpoint failed.
*
*                               - RETURNED BY USBD_OS_EP_LockAcquire() -
*                               See USBD_OS_EP_LockAcquire() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_CtrlStall (CPU_INT08U   dev_nbr,
                      USBD_ERR    *p_err)
{
    USBD_EP       *p_ep_out;
    USBD_EP       *p_ep_in;
    USBD_DRV      *p_drv;
    USBD_DRV_API  *p_drv_api;
    CPU_INT08U     ep_phy_nbr;
    CPU_BOOLEAN    stall_in;
    CPU_BOOLEAN    stall_out;


    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return;
    }

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(USBD_EP_ADDR_CTRL_OUT);
    p_ep_out   = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];
    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(USBD_EP_ADDR_CTRL_IN);
    p_ep_in    = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];

    if ((p_ep_out == (USBD_EP *)0) ||
        (p_ep_in  == (USBD_EP *)0)) {
       *p_err = USBD_ERR_EP_INVALID_ADDR;
        return;
    }

    USBD_OS_EP_LockAcquire(p_drv->DevNbr,
                           p_ep_out->Ix,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    USBD_OS_EP_LockAcquire(p_drv->DevNbr,
                           p_ep_in->Ix,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep_out->Ix);
        return;
    }

    if ((p_ep_out->State == USBD_EP_STATE_OPEN) &&
        (p_ep_in->State  == USBD_EP_STATE_OPEN)) {

        p_drv_api = p_drv->API_Ptr;                             /* Get dev drv API struct.                              */
        stall_in = p_drv_api->EP_Stall(p_drv,
                                       USBD_EP_ADDR_CTRL_IN,
                                       DEF_SET);

        stall_out = p_drv_api->EP_Stall(p_drv,
                                        USBD_EP_ADDR_CTRL_OUT,
                                        DEF_SET);

        if ((stall_in  == DEF_FAIL) ||
            (stall_out == DEF_FAIL)) {
           *p_err = USBD_ERR_EP_STALL;
        } else {
           *p_err = USBD_ERR_NONE;
        }

    } else {
       *p_err = USBD_ERR_EP_INVALID_STATE;
    }

    USBD_OS_EP_LockRelease(p_drv->DevNbr,
                           p_ep_in->Ix);

    USBD_OS_EP_LockRelease(p_drv->DevNbr,
                           p_ep_out->Ix);
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                     GENERAL ENDPOINT FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                           USBD_EP_Init()
*
* Description : Initialize endpoint structures.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_EP_Init (void)
{
    USBD_EP     *p_ep;
    USBD_URB    *p_urb;
    CPU_INT08U   ep_ix;
    CPU_INT08U   dev_nbr;
    CPU_INT16U   urb_ix;


    for (dev_nbr = 0u; dev_nbr < USBD_CFG_MAX_NBR_DEV; dev_nbr++) {
        for (ep_ix = 0u; ep_ix < USBD_EP_MAX_NBR; ep_ix++) {
            if (ep_ix < USBD_CFG_MAX_NBR_EP_OPEN) {
                p_ep                = &USBD_EP_Tbl[dev_nbr][ep_ix];
                p_ep->Addr          =  USBD_EP_ADDR_NONE;
                p_ep->Attrib        =  DEF_BIT_NONE;
                p_ep->MaxPktSize    =  0u;
                p_ep->Interval      =  0u;
                p_ep->Ix            =  0u;
#if (USBD_CFG_MAX_NBR_URB_EXTRA > 0u)
                p_ep->URB_MainAvail =  DEF_YES;
#endif
                p_ep->URB_HeadPtr   = (USBD_URB *)0;
                p_ep->URB_TailPtr   = (USBD_URB *)0;

                USBD_DBG_STATS_EP_RESET(dev_nbr, ep_ix);
            }
            USBD_EP_TblPtrs[dev_nbr][ep_ix] = (USBD_EP *)0;
        }

        USBD_URB_TblPtr[dev_nbr] = &USBD_URB_Tbl[dev_nbr][0];

        for (urb_ix = 0u; urb_ix < USBD_URB_MAX_NBR; urb_ix++) {
            p_urb               = &USBD_URB_Tbl[dev_nbr][urb_ix];
            p_urb->BufPtr       = (CPU_INT08U    *)0;
            p_urb->BufLen       =  0u;
            p_urb->XferLen      =  0u;
            p_urb->NextXferLen  =  0u;
            p_urb->Flags        =  0u;
            p_urb->State        =  USBD_URB_STATE_IDLE;
            p_urb->AsyncFnct    = (USBD_ASYNC_FNCT)0;
            p_urb->AsyncFnctArg = (void          *)0;
            p_urb->Err          =  USBD_ERR_NONE;
            if (urb_ix < (USBD_URB_MAX_NBR - 1)) {
                p_urb->NextPtr  = &USBD_URB_Tbl[dev_nbr][urb_ix + 1];
            } else {
                p_urb->NextPtr  = (USBD_URB      *)0;
            }
        }

        USBD_EP_OpenCtr[dev_nbr]    = 0u;
        USBD_EP_OpenBitMap[dev_nbr] = DEF_INT_32_MASK;
#if (USBD_CFG_MAX_NBR_URB_EXTRA > 0u)
        USBD_URB_ExtraCtr[dev_nbr]  = 0u;
#endif
    }
}


/*
*********************************************************************************************************
*                                      USBD_EP_XferAsyncProcess()
*
* Description : Read/write data asynchronously from/to non-control endpoints.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*               ----        Argument checked by caller.
*
*               ep_addr     Endpoint address.
*
*               xfer_err    Error code returned by the USB device driver.
*
* Return(s)   : none.
*
* Note(s)     : (1) A USB device driver can notify the core about the Tx transfer completion using
*                   USBD_EP_TxCmpl() or USBD_EP_TxCmplExt(). The latter function allows to report a
*                   specific error code whereas USBD_EP_TxCmpl() reports only a successful transfer.
*                   In the case of an asynchronous transfer, the error code reported by the USB device
*                   driver must be tested. In case of an error condition, the asynchronous transfer
*                   is marked as completed and the associated callback is called by the core task.
*
*               (2) This condition covers also the case where the transfer length is multiple of the
*                   maximum packet size. In that case, host sends a zero-length packet considered as
*                   a short packet for the condition.
*********************************************************************************************************
*/

void  USBD_EP_XferAsyncProcess (USBD_DRV    *p_drv,
                                CPU_INT08U   ep_addr,
                                USBD_ERR     xfer_err)
{
    CPU_INT08U     ep_phy_nbr;
    USBD_EP       *p_ep;
    USBD_DRV_API  *p_drv_api;
    CPU_BOOLEAN    ep_dir_in;
    USBD_ERR       local_err;
    USBD_URB      *p_urb;
    USBD_URB      *p_urb_cmpl;
    CPU_INT08U    *p_buf_cur;
    CPU_INT32U     xfer_len;
    CPU_INT32U     xfer_rem;


    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
    p_ep       = USBD_EP_TblPtrs[p_drv->DevNbr][ep_phy_nbr];
    if (p_ep == (USBD_EP *)0) {
        return;
    }

    p_drv_api = p_drv->API_Ptr;
    ep_dir_in = USBD_EP_IS_IN(p_ep->Addr);

    USBD_OS_EP_LockAcquire(p_drv->DevNbr,
                           p_ep->Ix,
                           0u,
                          &local_err);
    if (local_err != USBD_ERR_NONE) {
        return;
    }

    if (p_ep->XferState == USBD_XFER_STATE_NONE) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
        return;
    }

    p_urb = p_ep->URB_HeadPtr;
    if (p_urb == (USBD_URB *)0) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
        USBD_DBG_EP("USBD_EP_Process(): no URB to process", ep_addr);
        return;
    }

    if ((p_urb->State == USBD_URB_STATE_IDLE) ||
        (p_urb->State == USBD_URB_STATE_XFER_SYNC)) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
        USBD_DBG_EP("USBD_EP_Process(): incorrect URB state", ep_addr);
        return;
    }

    p_urb_cmpl = (USBD_URB *)0;
    if (xfer_err == USBD_ERR_NONE) {                            /* See Note #1.                                         */
        xfer_rem   =  p_urb->BufLen - p_urb->XferLen;
        p_buf_cur  = &p_urb->BufPtr[p_urb->XferLen];

        if (ep_dir_in == DEF_YES) {                             /* ------------------- IN TRANSFER -------------------- */
            if (xfer_rem > 0u) {                                /* Another transaction must be done.                    */
                USBD_EP_TxAsyncProcess(p_drv,
                                       p_ep,
                                       p_urb,
                                       p_buf_cur,
                                       xfer_rem,
                                      &local_err);
                if (local_err != USBD_ERR_NONE) {
                    p_urb_cmpl = USBD_URB_AsyncCmpl(p_ep, local_err);
                }
            } else if ((DEF_BIT_IS_SET(p_urb->Flags, USBD_URB_FLAG_XFER_END) == DEF_YES) &&
                       (p_urb->XferLen % p_ep->MaxPktSize                    == 0u)      &&
                       (p_urb->XferLen                                       != 0u)) {
                                                                /* $$$$ This case should be tested more thoroughly.     */
                                                                /* Send ZLP if needed, at end of xfer.                  */
                DEF_BIT_CLR(p_urb->Flags, USBD_URB_FLAG_XFER_END);

                USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, DrvTxZLP_Nbr);

                p_drv_api->EP_TxZLP(p_drv, p_ep->Addr, &local_err);
                if (local_err != USBD_ERR_NONE) {
                    p_urb_cmpl = USBD_URB_AsyncCmpl(p_ep, local_err);
                }
                USBD_DBG_STATS_EP_INC_IF_TRUE(p_drv->DevNbr, p_ep->Ix, DrvTxZLP_Nbr, (local_err == USBD_ERR_NONE));
            } else {                                            /* Xfer is completed.                                   */
                p_urb_cmpl = USBD_URB_AsyncCmpl(p_ep, USBD_ERR_NONE);
            }
        } else {                                                /* ------------------- OUT TRANSFER ------------------- */
            USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, DrvRxNbr);

            xfer_len = p_drv_api->EP_Rx(p_drv,
                                        p_ep->Addr,
                                        p_buf_cur,
                                        p_urb->NextXferLen,
                                       &local_err);
            if (local_err != USBD_ERR_NONE) {
                p_urb_cmpl = USBD_URB_AsyncCmpl(p_ep, local_err);
            } else {
                USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, DrvRxSuccessNbr);

                p_urb->XferLen += xfer_len;

                if ((xfer_len       == 0u)                 ||   /* Rx'd a ZLP.                                          */
                    (xfer_len       <  p_urb->NextXferLen) ||   /* Rx'd a short pkt (see Note #2).                      */
                    (p_urb->XferLen == p_urb->BufLen)) {        /* All bytes rx'd.                                      */
                                                                /* Xfer finished.                                       */
                    p_urb_cmpl = USBD_URB_AsyncCmpl(p_ep, USBD_ERR_NONE);
                } else {
                    p_buf_cur = &p_urb->BufPtr[p_urb->XferLen]; /* Xfer not finished.                                   */
                    xfer_len  =  p_urb->BufLen - p_urb->XferLen;

                    USBD_EP_RxStartAsyncProcess(p_drv,
                                                p_ep,
                                                p_urb,
                                                p_buf_cur,
                                                xfer_len,
                                               &local_err);
                    if (local_err != USBD_ERR_NONE) {
                        p_urb_cmpl = USBD_URB_AsyncCmpl(p_ep, local_err);
                    }
                }
            }
        }
    } else {
        p_urb_cmpl = USBD_URB_AsyncCmpl(p_ep, xfer_err);
    }

    USBD_OS_EP_LockRelease(p_drv->DevNbr,
                           p_ep->Ix);

    if (p_urb_cmpl != (USBD_URB *)0) {
        USBD_URB_AsyncEnd(p_drv->DevNbr, p_ep, p_urb_cmpl);     /* Execute callback and free aborted URB(s), if any.    */
    }
}


/*
*********************************************************************************************************
*                                           USBD_EP_Open()
*
* Description : Open non-control endpoint.
*
* Argument(s) : p_drv           Pointer to device driver structure.
*
*               ep_addr         Endpoint address.
*
*               max_pkt_size    Maximum packet size.
*
*               attrib          Endpoint attributes.
*
*               interval        Endpoint polling interval.
*
*               p_err           Pointer to variable that will receive return error code from this function :
*
*                                   USBD_ERR_NONE               Endpoint successfully opened.
*                                   USBD_ERR_NULL_PTR           Argument 'p_drv' passed a NULL pointer.
*                                   USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                                   USBD_ERR_EP_NONE_AVAIL      Physical endpoint NOT available.
*
*                                   - RETURNED BY USBD_OS_EP_SignalCreate() -
*                                   See USBD_OS_EP_SignalCreate() for additional return error codes.
*
*                                   - RETURNED BY USBD_OS_EP_LockAcquire() -
*                                   See USBD_OS_EP_LockAcquire() for additional return error codes.
*
*                                   - RETURNED BY 'p_drv_api->EP_Open()' -
*                                   See specific driver(s) 'p_drv_api->EP_Open()' for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_EP_Open (USBD_DRV    *p_drv,
                    CPU_INT08U   ep_addr,
                    CPU_INT16U   max_pkt_size,
                    CPU_INT08U   attrib,
                    CPU_INT08U   interval,
                    USBD_ERR    *p_err)
{
    USBD_DRV_API  *p_drv_api;
    USBD_EP       *p_ep;
    CPU_INT08U     ep_bit;
    CPU_INT08U     ep_ix;
    CPU_INT08U     ep_phy_nbr;
    CPU_INT08U     dev_nbr;
    CPU_INT08U     transaction_frame;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }
#endif

    dev_nbr    = p_drv->DevNbr;
    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
    p_ep       = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];

    if (p_ep != (USBD_EP *)0) {
       *p_err =  USBD_ERR_EP_INVALID_ADDR;
        return;
    }

    CPU_CRITICAL_ENTER();
    if (USBD_EP_OpenCtr[dev_nbr] == USBD_CFG_MAX_NBR_EP_OPEN) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_EP_NONE_AVAIL;
        return;
    }

    ep_bit = (CPU_INT08U)(USBD_EP_MAX_NBR - 1u - CPU_CntLeadZeros32(USBD_EP_OpenBitMap[dev_nbr]));
    DEF_BIT_CLR(USBD_EP_OpenBitMap[dev_nbr], DEF_BIT32(ep_bit));
    USBD_EP_OpenCtr[dev_nbr]++;
    CPU_CRITICAL_EXIT();

    ep_ix = USBD_EP_MAX_NBR - 1u - ep_bit;

    USBD_OS_EP_SignalCreate(p_drv->DevNbr, ep_ix, p_err);
    if (*p_err != USBD_ERR_NONE) {
        goto end_clean;
    }

    USBD_OS_EP_LockCreate(p_drv->DevNbr, ep_ix, p_err);
    if (*p_err != USBD_ERR_NONE) {
        goto end_signal_clean;
    }

    transaction_frame  = (max_pkt_size >> 11u) & 0x3;
    transaction_frame +=  1u;

    p_drv_api = p_drv->API_Ptr;
    p_drv_api->EP_Open(p_drv,                                   /* Open EP in dev drv.                                  */
                       ep_addr,
                       attrib & USBD_EP_TYPE_MASK,
                       max_pkt_size & 0x7FF,                    /* Mask out transactions per microframe.                */
                       transaction_frame,
                       p_err);
    if (*p_err != USBD_ERR_NONE) {
        goto end_lock_signal_clean;
    }

    p_ep = &USBD_EP_Tbl[dev_nbr][ep_ix];

    CPU_CRITICAL_ENTER();
    p_ep->Addr       = ep_addr;
    p_ep->Attrib     = attrib;
    p_ep->MaxPktSize = max_pkt_size;
    p_ep->Interval   = interval;
    p_ep->State      = USBD_EP_STATE_OPEN;
    p_ep->XferState  = USBD_XFER_STATE_NONE;
    p_ep->Ix         = ep_ix;

    USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr] = p_ep;
    CPU_CRITICAL_EXIT();

#if (USBD_CFG_DBG_STATS_EN == DEF_ENABLED)
                                                                /* Reset stats only if EP address changed.              */
    if (ep_addr != USBD_DBG_STATS_EP_GET(dev_nbr, ep_ix, Addr)) {
        USBD_DBG_STATS_EP_RESET(dev_nbr, ep_ix);
        USBD_DBG_STATS_EP_SET_ADDR(dev_nbr, ep_ix, ep_addr);
    }
#endif
    USBD_DBG_STATS_EP_INC(dev_nbr, ep_ix, EP_OpenNbr);

    USBD_DBG_EP("EP Open", ep_addr);

   *p_err = USBD_ERR_NONE;
    return;

end_lock_signal_clean:
    USBD_OS_EP_LockDel(p_drv->DevNbr, ep_ix);

end_signal_clean:
    USBD_OS_EP_SignalDel(p_drv->DevNbr, ep_ix);

end_clean:
    CPU_CRITICAL_ENTER();
    DEF_BIT_SET(USBD_EP_OpenBitMap[dev_nbr], DEF_BIT32(ep_bit));
    USBD_EP_OpenCtr[dev_nbr] -= 1u;
    CPU_CRITICAL_EXIT();

    USBD_DBG_EP_ERR("EP Open", ep_addr, *p_err);

    return;
}


/*
*********************************************************************************************************
*                                       USBD_EP_MaxPktSizeGet()
*
* Description : Retrieve endpoint maximum packet size.
*
* Argument(s) : dev_nbr     Device number.
*
*               ep_addr     Endpoint address.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE               Endpoint maximum packet size successfully retrieved.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                               USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*
* Return(s)   : Maximum packet size, if NO error(s).
*
*               0,                   otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16U  USBD_EP_MaxPktSizeGet (CPU_INT08U   dev_nbr,
                                   CPU_INT08U   ep_addr,
                                   USBD_ERR    *p_err)
{
    USBD_EP     *p_ep;
    USBD_DRV    *p_drv;
    CPU_INT08U   ep_phy_nbr;
    CPU_INT16U   max_pkt_len;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return (0u);
    }

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);

    p_ep = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];
    if (p_ep == (USBD_EP *)0) {
       *p_err = USBD_ERR_EP_INVALID_ADDR;
        return (0u);
    }

    max_pkt_len = p_ep->MaxPktSize & 0x7FF;                     /* Mask out transactions per microframe.                */

    return (max_pkt_len);
}

/*
*********************************************************************************************************
*                                       USBD_EP_MaxNbrOpenGet()
*
* Description : Retrieve maximum number of opened endpoints.
*
* Argument(s) : dev_nbr     Device number.
*
* Return(s)   : Maximum number of opened endpoints, if NO error(s).
*
*               0,                                  otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  USBD_EP_MaxNbrOpenGet (CPU_INT08U  dev_nbr)
{
    USBD_DRV    *p_drv;
    CPU_INT08U   nbr_open;


    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get drv struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
        return (0u);
    }

    nbr_open = USBD_EP_OpenCtr[dev_nbr];

    return (nbr_open);
}


/*
*********************************************************************************************************
*                                           USBD_EP_Abort()
*
* Description : Abort I/O transfer on endpoint.
*
* Argument(s) : dev_nbr     Device number.
*
*               ep_addr     Endpoint address.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE               Transfer successfully aborted.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                               USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                               USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*
*                               - RETURNED BY USBD_OS_EP_LockAcquire() -
*                               See USBD_OS_EP_LockAcquire() for additional return error codes.
*
*                               - RETURNED BY USBD_EP_URB_Abort() -
*                               See USBD_EP_URB_Abort() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_EP_Abort (CPU_INT08U   dev_nbr,
                     CPU_INT08U   ep_addr,
                     USBD_ERR    *p_err)
{
    USBD_EP     *p_ep;
    USBD_DRV    *p_drv;
    CPU_INT08U   ep_phy_nbr;
    USBD_URB    *p_urb_head_aborted;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return;
    }

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
    p_ep       = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];

    if (p_ep == (USBD_EP *)0) {
       *p_err = USBD_ERR_EP_INVALID_ADDR;
        return;
    }

    USBD_OS_EP_LockAcquire(p_drv->DevNbr,
                           p_ep->Ix,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    USBD_DBG_STATS_EP_INC(dev_nbr, p_ep->Ix, EP_AbortExecNbr);

    if ((p_ep->State != USBD_EP_STATE_OPEN) &&
        (p_ep->State != USBD_EP_STATE_STALL)) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_EP_INVALID_STATE;
        return;
    }

    p_urb_head_aborted = USBD_EP_URB_Abort(p_drv,               /* Abort xfers in progress, keep ptr to head of list.   */
                                           p_ep,
                                           p_err);

    USBD_DBG_STATS_EP_INC_IF_TRUE(dev_nbr, p_ep->Ix, EP_AbortSuccessNbr, (*p_err == USBD_ERR_NONE));

    USBD_OS_EP_LockRelease(p_drv->DevNbr,
                           p_ep->Ix);

    if (p_urb_head_aborted != (USBD_URB *)0) {
        USBD_URB_AsyncEnd(dev_nbr, p_ep, p_urb_head_aborted);   /* Execute callback and free aborted URB(s), if any.    */
    }
}


/*
*********************************************************************************************************
*                                           USBD_EP_Close()
*
* Description : Close non-control endpoint.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_addr     Endpoint address.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE               Endpoint successfully closed.
*                               USBD_ERR_NULL_PTR           Argument 'p_drv' passed a NULL pointer.
*                               USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*
*                               - RETURNED BY USBD_OS_EP_LockAcquire() -
*                               See USBD_OS_EP_LockAcquire() for additional return error codes.
*
*                               - RETURNED BY USBD_EP_URB_Abort() -
*                               See USBD_EP_URB_Abort() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_EP_Close (USBD_DRV    *p_drv,
                     CPU_INT08U   ep_addr,
                     USBD_ERR    *p_err)
{
    USBD_EP     *p_ep;
    CPU_INT08U   dev_nbr;
    CPU_INT08U   ep_bit;
    CPU_INT08U   ep_phy_nbr;
    USBD_URB    *p_urb_head_aborted;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }
#endif

    dev_nbr    = p_drv->DevNbr;
    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
    p_ep       = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];

    if (p_ep == (USBD_EP *)0) {
       *p_err =  USBD_ERR_EP_INVALID_ADDR;
        return;
    }

    USBD_OS_EP_LockAcquire(p_drv->DevNbr,
                           p_ep->Ix,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    if (p_ep->State == USBD_EP_STATE_CLOSE) {
        USBD_OS_EP_LockRelease(p_drv->DevNbr,
                               p_ep->Ix);
       *p_err = USBD_ERR_NONE;
        return;
    }

    USBD_DBG_STATS_EP_INC(dev_nbr, p_ep->Ix, EP_CloseExecNbr);

    ep_bit = USBD_EP_MAX_NBR - 1u - p_ep->Ix;

    p_urb_head_aborted = USBD_EP_URB_Abort(p_drv,               /* Abort xfers in progress, keep ptr to head of list.   */
                                           p_ep,
                                           p_err);

    p_ep->State = USBD_EP_STATE_CLOSE;

    CPU_CRITICAL_ENTER();
    DEF_BIT_SET(USBD_EP_OpenBitMap[dev_nbr], DEF_BIT32(ep_bit));
    USBD_EP_OpenCtr[dev_nbr]--;
    CPU_CRITICAL_EXIT();

    p_drv->API_Ptr->EP_Close(p_drv, ep_addr);

    p_ep->XferState = USBD_XFER_STATE_NONE;
    USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr] = (USBD_EP *)0;

    USBD_DBG_STATS_EP_INC_IF_TRUE(dev_nbr, p_ep->Ix, EP_CloseSuccessNbr, (*p_err == USBD_ERR_NONE));

    USBD_OS_EP_LockRelease(p_drv->DevNbr,
                           p_ep->Ix);

    USBD_OS_EP_SignalDel(p_drv->DevNbr, p_ep->Ix);
    USBD_OS_EP_LockDel  (p_drv->DevNbr, p_ep->Ix);

    if (p_urb_head_aborted != (USBD_URB *)0) {
        USBD_URB_AsyncEnd(dev_nbr, p_ep, p_urb_head_aborted);   /* Execute callback and free aborted URB(s), if any.    */
    }
}


/*
*********************************************************************************************************
*                                           USBD_EP_Stall()
*
* Description : Stall non-control endpoint.
*
* Argument(s) : dev_nbr     Device number.
*
*               ep_addr     Endpoint address.
*
*               state       Endpoint stall state.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE               Endpoint successfully stalled.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                               USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                               USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                               USBD_ERR_EP_STALL           Device driver endpoint stall failed.
*
*                               - RETURNED BY USBD_OS_EP_LockAcquire() -
*                               See USBD_OS_EP_LockAcquire() for additional return error codes.
*
*                               - RETURNED BY USBD_EP_URB_Abort() -
*                               See USBD_EP_URB_Abort() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_EP_Stall (CPU_INT08U    dev_nbr,
                     CPU_INT08U    ep_addr,
                     CPU_BOOLEAN   state,
                     USBD_ERR     *p_err)
{
    USBD_DRV     *p_drv;
    CPU_INT08U    ep_phy_nbr;
    USBD_EP      *p_ep;
    USBD_URB     *p_urb_head_aborted;
    CPU_BOOLEAN   valid;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return;
    }

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
    p_ep       = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];

    if (p_ep == (USBD_EP *)0) {
       *p_err = USBD_ERR_EP_INVALID_ADDR;
        return;
    }

    USBD_OS_EP_LockAcquire(p_drv->DevNbr,
                           p_ep->Ix,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    p_urb_head_aborted = (USBD_URB *)0;

    switch (p_ep->State) {
        case USBD_EP_STATE_OPEN:
             if (state == DEF_SET) {
                 p_urb_head_aborted = USBD_EP_URB_Abort(p_drv,  /* Abort xfers in progress, keep ptr to head of list.   */
                                                        p_ep,
                                                        p_err);
                 if (*p_err != USBD_ERR_NONE) {
                     break;
                 }
                 p_ep->State = USBD_EP_STATE_STALL;
             }

             valid = p_drv->API_Ptr->EP_Stall(p_drv, p_ep->Addr, state);
             if (valid == DEF_FAIL) {
                *p_err = USBD_ERR_EP_STALL;
             } else {
                *p_err = USBD_ERR_NONE;
             }
             break;


        case USBD_EP_STATE_STALL:
             if (state == DEF_CLR) {

                 valid = p_drv->API_Ptr->EP_Stall(p_drv, p_ep->Addr, DEF_CLR);
                 if (valid == DEF_FAIL) {
                    *p_err = USBD_ERR_EP_STALL;
                 } else {
                     p_ep->State = USBD_EP_STATE_OPEN;
                    *p_err = USBD_ERR_NONE;
                 }
             }
             break;


        case USBD_EP_STATE_CLOSE:
        default:
            *p_err = USBD_ERR_EP_INVALID_STATE;
             break;
    }

    USBD_OS_EP_LockRelease(p_drv->DevNbr,
                           p_ep->Ix);

    if (p_urb_head_aborted != (USBD_URB *)0) {
        USBD_URB_AsyncEnd(dev_nbr, p_ep, p_urb_head_aborted);   /* Execute callback and free aborted URB(s), if any.    */
    }
}


/*
*********************************************************************************************************
*                                         USBD_EP_IsStalled()
*
* Description : Get stall status of non-control endpoint.
*
* Argument(s) : dev_nbr     Device number.
*
*               ep_addr     Endpoint address.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE               Stall state successfully retrieved.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                               USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*
* Return(s)   : DEF_TRUE,  if endpoint is stalled.
*
*               DEF_FALSE, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  USBD_EP_IsStalled (CPU_INT08U   dev_nbr,
                                CPU_INT08U   ep_addr,
                                USBD_ERR    *p_err)
{
    USBD_DRV    *p_drv;
    USBD_EP     *p_ep;
    CPU_INT08U   ep_phy_nbr;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(DEF_NO);
    }
#endif

    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return (DEF_NO);
    }

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
    p_ep       = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];

    if (p_ep == (USBD_EP *)0) {
       *p_err = USBD_ERR_EP_INVALID_ADDR;
        return (DEF_NO);
    }

   *p_err = USBD_ERR_NONE;

    if (p_ep->State == USBD_EP_STATE_STALL) {
        return (DEF_YES);
    } else {
        return (DEF_NO);
    }
}


/*
*********************************************************************************************************
*                                          USBD_EP_RxCmpl()
*
* Description : Notify USB stack that packet receive has completed.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_log_nbr  Endpoint logical number.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_EP_RxCmpl (USBD_DRV    *p_drv,
                      CPU_INT08U   ep_log_nbr)
{
    USBD_EP     *p_ep;
    CPU_INT08U   ep_phy_nbr;
    USBD_ERR     err;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_drv == (USBD_DRV *)0) {
        return;
    }
#endif

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(USBD_EP_LOG_TO_ADDR_OUT(ep_log_nbr));
    p_ep       = USBD_EP_TblPtrs[p_drv->DevNbr][ep_phy_nbr];

    if (p_ep == (USBD_EP *)0) {
        USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, RxCmplErrNbr);
        return;
    }

    USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, RxCmplNbr);

    if (p_ep->XferState == USBD_XFER_STATE_SYNC) {
        USBD_OS_EP_SignalPost(p_drv->DevNbr, p_ep->Ix, &err);
    } else if ((p_ep->XferState == USBD_XFER_STATE_ASYNC) ||
               (p_ep->XferState == USBD_XFER_STATE_ASYNC_PARTIAL)) {
        USBD_EventEP(p_drv, p_ep->Addr, USBD_ERR_NONE);
    } else {
        USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, RxCmplErrNbr);
        USBD_DBG_EP("USBD_EP_RxCmpl(): incorrect XferState", p_ep->Addr);
    }
}


/*
*********************************************************************************************************
*                                          USBD_EP_TxCmpl()
*
* Description : Notify USB stack that packet transmit has completed.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_log_nbr  Endpoint logical number.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_EP_TxCmpl (USBD_DRV    *p_drv,
                      CPU_INT08U   ep_log_nbr)
{
    USBD_EP     *p_ep;
    CPU_INT08U   ep_phy_nbr;
    USBD_ERR     err;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_drv == (USBD_DRV *)0) {
        return;
    }
#endif

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(USBD_EP_LOG_TO_ADDR_IN(ep_log_nbr));
    p_ep       = USBD_EP_TblPtrs[p_drv->DevNbr][ep_phy_nbr];

    if (p_ep == (USBD_EP *)0) {
        USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, TxCmplErrNbr);
        return;
    }

    USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, TxCmplNbr);

    if (p_ep->XferState == USBD_XFER_STATE_SYNC) {
        USBD_OS_EP_SignalPost(p_drv->DevNbr, p_ep->Ix, &err);
    } else if ((p_ep->XferState == USBD_XFER_STATE_ASYNC) ||
               (p_ep->XferState == USBD_XFER_STATE_ASYNC_PARTIAL)) {
        USBD_EventEP(p_drv, p_ep->Addr, USBD_ERR_NONE);
    } else {
        USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, TxCmplErrNbr);
        USBD_DBG_EP("USBD_EP_TxCmpl(): incorrect XferState", p_ep->Addr);
    }
}


/*
*********************************************************************************************************
*                                         USBD_EP_TxCmplExt()
*
* Description : Notify USB stack that packet transmit has completed (see Note #1).
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_log_nbr  Endpoint logical number.
*
*               err         Error code returned by the USB driver.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function is an alternative to the function USBD_EP_TxCmpl() so that a USB device
*                   driver can return to the core an error code upon the Tx transfer completion.
*********************************************************************************************************
*/

void  USBD_EP_TxCmplExt (USBD_DRV    *p_drv,
                         CPU_INT08U   ep_log_nbr,
                         USBD_ERR     xfer_err)
{
    USBD_EP     *p_ep;
    CPU_INT08U   ep_phy_nbr;
    USBD_ERR     local_err;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_drv == (USBD_DRV *)0) {
        return;
    }
#endif

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(USBD_EP_LOG_TO_ADDR_IN(ep_log_nbr));
    p_ep       = USBD_EP_TblPtrs[p_drv->DevNbr][ep_phy_nbr];

    if (p_ep == (USBD_EP *)0) {
        USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, TxCmplErrNbr);
        return;
    }

    USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, TxCmplNbr);

    if (p_ep->XferState == USBD_XFER_STATE_SYNC) {
        USBD_OS_EP_SignalAbort(p_drv->DevNbr, p_ep->Ix, &local_err);
        if (local_err != USBD_ERR_NONE) {
            USBD_DBG_EP_ERR("USBD_EP_TxCmplExt()", p_ep->Addr, local_err);
        }
    } else if ((p_ep->XferState == USBD_XFER_STATE_ASYNC) ||
               (p_ep->XferState == USBD_XFER_STATE_ASYNC_PARTIAL)) {
        USBD_EventEP(p_drv, p_ep->Addr, xfer_err);
    } else {
        USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, TxCmplErrNbr);
        USBD_DBG_EP("USBD_EP_TxCmplExt(): incorrect XferState", p_ep->Addr);
    }
}


/*
*********************************************************************************************************
*                                           USBD_EP_TxZLP()
*
* Description : Send zero-length packet to the host.
*
* Argument(s) : dev_nbr     Device number.
*
*               ep_addr     Endpoint address.
*
*               timeout_ms  Timeout in milliseconds.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE               Data successfully transferred.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                               USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                               USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                               USBD_ERR_EP_INVALID_TYPE    Invalid endpoint type.
*                               USBD_ERR_EP_IO_PENDING      Data already queued on this endpoint.
*
*                               - RETURNED BY USBD_OS_EP_LockAcquire() -
*                               See USBD_OS_EP_LockAcquire() for additional return error codes.
*
*                               - RETURNED BY USBD_URB_Get() -
*                               See USBD_URB_Get() for additional return error codes.
*
*                               - RETURNED BY 'p_drv_api->EP_TxZLP()' -
*                               See specific driver(s) 'p_drv_api->EP_TxZLP()' for additional return error codes.
*
*                               - RETURNED BY USBD_OS_EP_SignalPend() -
*                               See USBD_OS_EP_SignalPend() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function should only be called during a synchronous transfer.
*********************************************************************************************************
*/

void  USBD_EP_TxZLP (CPU_INT08U   dev_nbr,
                     CPU_INT08U   ep_addr,
                     CPU_INT16U   timeout_ms,
                     USBD_ERR    *p_err)
{
    USBD_EP       *p_ep;
    USBD_URB      *p_urb;
    USBD_DRV      *p_drv;
    USBD_DRV_API  *p_drv_api;
    CPU_INT08U     ep_phy_nbr;
    USBD_ERR       local_err;


    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return;
    }

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
    p_ep       = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];

    if (p_ep == (USBD_EP *)0) {
       *p_err = USBD_ERR_EP_INVALID_ADDR;
        return;
    }

    USBD_OS_EP_LockAcquire(p_drv->DevNbr,
                           p_ep->Ix,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    USBD_DBG_STATS_EP_INC(dev_nbr, p_ep->Ix, TxZLP_ExecNbr);

    if (p_ep->State != USBD_EP_STATE_OPEN) {
       *p_err = USBD_ERR_EP_INVALID_STATE;
        goto lock_release;
    }
                                                                /* Chk EP attrib.                                       */
    if ((p_ep->Attrib & USBD_EP_TYPE_MASK) == USBD_EP_TYPE_ISOC) {
       *p_err = USBD_ERR_EP_INVALID_TYPE;
        goto lock_release;
    }

    if (p_ep->XferState != USBD_XFER_STATE_NONE) {
       *p_err = USBD_ERR_EP_IO_PENDING;
        goto lock_release;
    }

    p_urb = USBD_URB_Get(dev_nbr, p_ep, p_err);
    if (*p_err != USBD_ERR_NONE) {
        goto lock_release;
    }

    p_urb->State    = USBD_URB_STATE_XFER_SYNC;                 /* Only State needs to be set to indicate sync xfer.    */
    p_ep->XferState = USBD_XFER_STATE_SYNC;                     /* Set XferState before submitting xfer.                */

    USBD_URB_Queue(p_ep, p_urb);

    p_drv_api = p_drv->API_Ptr;                                 /* Get dev drv API struct.                              */

    USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, DrvTxZLP_Nbr);

    p_drv_api->EP_TxZLP(p_drv, ep_addr, p_err);
    if (*p_err == USBD_ERR_NONE) {
        USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, DrvTxZLP_SuccessNbr);

        USBD_OS_EP_LockRelease(p_drv->DevNbr,                   /* Unlock before pending on completion.                 */
                               p_ep->Ix);

        USBD_OS_EP_SignalPend(dev_nbr, p_ep->Ix, timeout_ms, p_err);

        USBD_OS_EP_LockAcquire(p_drv->DevNbr,                   /* Re-lock EP after xfer completion.                    */
                               p_ep->Ix,
                               0u,
                              &local_err);
        if (local_err != USBD_ERR_NONE) {
           *p_err = USBD_ERR_OS_FAIL;

        } else if (*p_err == USBD_ERR_OS_TIMEOUT) {
            p_drv_api->EP_Abort(p_drv, ep_addr);
            USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, TxSyncTimeoutErrNbr);
        }
    }

    USBD_URB_Dequeue(p_ep);

    USBD_URB_Free(dev_nbr, p_ep, p_urb);

    USBD_DBG_STATS_EP_INC_IF_TRUE(dev_nbr, p_ep->Ix, TxZLP_SuccessNbr, (*p_err == USBD_ERR_NONE));

lock_release:
    USBD_OS_EP_LockRelease(p_drv->DevNbr,
                           p_ep->Ix);
}


/*
*********************************************************************************************************
*                                           USBD_EP_RxZLP()
*
* Description : Receive zero-length packet from the host.
*
* Argument(s) : dev_nbr     Device number.
*
*               ep_addr     Endpoint address.
*
*               timeout_ms  Timeout in milliseconds.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE               Data successfully transferred.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                               USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                               USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                               USBD_ERR_EP_INVALID_TYPE    Invalid endpoint type.
*                               USBD_ERR_EP_IO_PENDING      Data already queued on this endpoint.
*
*                               - RETURNED BY USBD_OS_EP_LockAcquire() -
*                               See USBD_OS_EP_LockAcquire() for additional return error codes.
*
*                               - RETURNED BY USBD_URB_Get() -
*                               See USBD_URB_Get() for additional return error codes.
*
*                               - RETURNED BY USBD_OS_EP_SignalPend() -
*                               See USBD_OS_EP_SignalPend() for additional return error codes.
*
*                               - RETURNED BY 'p_drv_api->EP_RxStart()' -
*                               See specific driver(s) 'p_drv_api->EP_RxStart()' for additional return error codes.
*
*                               - RETURNED BY 'p_drv_api->EP_RxZLP()' -
*                               See specific driver(s) 'p_drv_api->EP_RxZLP()' for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function should only be called during a synchronous transfer.
*********************************************************************************************************
*/

void  USBD_EP_RxZLP (CPU_INT08U   dev_nbr,
                     CPU_INT08U   ep_addr,
                     CPU_INT16U   timeout_ms,
                     USBD_ERR    *p_err)
{
    USBD_EP       *p_ep;
    USBD_URB      *p_urb;
    USBD_DRV      *p_drv;
    USBD_DRV_API  *p_drv_api;
    CPU_INT08U     ep_phy_nbr;
    USBD_ERR       local_err;


    p_drv = USBD_DrvRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return;
    }

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
    p_ep       = USBD_EP_TblPtrs[dev_nbr][ep_phy_nbr];

    if (p_ep == (USBD_EP *)0) {
       *p_err = USBD_ERR_EP_INVALID_ADDR;
        return;
    }

    USBD_OS_EP_LockAcquire(p_drv->DevNbr,
                           p_ep->Ix,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    USBD_DBG_STATS_EP_INC(dev_nbr, p_ep->Ix, RxZLP_ExecNbr);

    if (p_ep->State != USBD_EP_STATE_OPEN) {
       *p_err = USBD_ERR_EP_INVALID_STATE;
        goto lock_release;
    }

    if ((p_ep->Attrib & USBD_EP_TYPE_MASK) == USBD_EP_TYPE_ISOC) {
       *p_err = USBD_ERR_EP_INVALID_TYPE;
        goto lock_release;
    }

    if (p_ep->XferState != USBD_XFER_STATE_NONE) {
       *p_err = USBD_ERR_EP_IO_PENDING;
        goto lock_release;
    }

    p_urb = USBD_URB_Get(dev_nbr, p_ep, p_err);
    if (*p_err != USBD_ERR_NONE) {
        goto lock_release;
    }

    p_urb->State    = USBD_URB_STATE_XFER_SYNC;                 /* Only State needs to be set to indicate sync xfer.    */
    p_ep->XferState = USBD_XFER_STATE_SYNC;                     /* Set XferState before submitting xfer.                */

    USBD_URB_Queue(p_ep, p_urb);

    p_drv_api = p_drv->API_Ptr;                                 /* Get dev drv API struct.                              */
    USBD_DBG_STATS_EP_INC(dev_nbr, p_ep->Ix, DrvRxStartNbr);
    (void)p_drv_api->EP_RxStart(              p_drv,
                                              ep_addr,
                                (CPU_INT08U *)0u,
                                              0u,
                                              p_err);
    if (*p_err == USBD_ERR_NONE) {
        USBD_DBG_STATS_EP_INC(dev_nbr, p_ep->Ix, DrvRxStartSuccessNbr);

        USBD_OS_EP_LockRelease(p_drv->DevNbr,                   /* Unlock before pending on completion.                 */
                               p_ep->Ix);

        USBD_OS_EP_SignalPend(dev_nbr, p_ep->Ix, timeout_ms, p_err);

        USBD_OS_EP_LockAcquire(p_drv->DevNbr,                   /* Re-lock EP after xfer completion.                    */
                               p_ep->Ix,
                               0u,
                              &local_err);
        if ((*p_err    == USBD_ERR_NONE) &&
            (local_err == USBD_ERR_NONE)) {
            USBD_DBG_STATS_EP_INC(dev_nbr, p_ep->Ix, DrvRxZLP_Nbr);

            p_drv_api->EP_RxZLP(p_drv,
                                ep_addr,
                                p_err);
            USBD_DBG_STATS_EP_INC_IF_TRUE(dev_nbr, p_ep->Ix, DrvRxZLP_SuccessNbr, (*p_err == USBD_ERR_NONE));

        } else if (*p_err == USBD_ERR_OS_TIMEOUT) {

            p_drv_api->EP_Abort(p_drv, ep_addr);
            USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, RxSyncTimeoutErrNbr);

        } else if (local_err != USBD_ERR_NONE) {
           *p_err = USBD_ERR_OS_FAIL;
        }
    }

    USBD_URB_Dequeue(p_ep);

    USBD_URB_Free(dev_nbr, p_ep, p_urb);

    USBD_DBG_STATS_EP_INC_IF_TRUE(dev_nbr, p_ep->Ix, RxZLP_SuccessNbr, (*p_err == USBD_ERR_NONE));

lock_release:
    USBD_OS_EP_LockRelease(p_drv->DevNbr,
                           p_ep->Ix);
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                     USBD_EP_RxStartAsyncProcess()
*
* Description : Process driver's asynchronous RxStart operation.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               p_ep        Pointer to endpoint on which data will be received.
*
*               p_urb       Pointer to USB request block.
*
*               p_buf_cur   Pointer to source buffer to receive data.
*
*               len         Number of octets to receive.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE               Receive successfully configured.
*
*                               - RETURNED BY 'p_drv_api->EP_RxStart()' -
*                               See specific driver(s) 'p_drv_api->EP_RxStart()' for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Endpoint must be locked when calling this function.
*********************************************************************************************************
*/

static  void  USBD_EP_RxStartAsyncProcess (USBD_DRV    *p_drv,
                                           USBD_EP     *p_ep,
                                           USBD_URB    *p_urb,
                                           CPU_INT08U  *p_buf_cur,
                                           CPU_INT32U   len,
                                           USBD_ERR    *p_err)
{
    USBD_DRV_API  *p_drv_api;
    CPU_SR_ALLOC();


    p_drv_api = p_drv->API_Ptr;                                 /* Get dev drv API struct.                              */

    USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, DrvRxStartNbr);
    p_urb->NextXferLen = p_drv_api->EP_RxStart(p_drv,
                                               p_ep->Addr,
                                               p_buf_cur,
                                               len,
                                               p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, DrvRxStartSuccessNbr);

    if (p_urb->NextXferLen != len) {
        CPU_CRITICAL_ENTER();
        p_ep->XferState = USBD_XFER_STATE_ASYNC_PARTIAL;        /* Xfer will have to be done in many transactions.      */
        CPU_CRITICAL_EXIT();
    }
}


/*
*********************************************************************************************************
*                                       USBD_EP_TxAsyncProcess()
*
* Description : Process driver's asynchronous Tx operation.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               p_ep        Pointer to endpoint on which data will be transmitted.
*
*               p_urb       Pointer to USB request block.
*
*               p_buf_cur   Pointer to source buffer to transmit data.
*
*               len         Number of octets to transmit.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE               Transmit successfully configured.
*                               USBD_ERR_TX                 Generic Tx error.
*
*                               - RETURNED BY 'p_drv_api->EP_Tx()' -
*                               See specific driver(s) 'p_drv_api->EP_Tx()' for additional return error codes.
*
*                               - RETURNED BY 'p_drv_api->EP_TxStart()' -
*                               See specific driver(s) 'p_drv_api->EP_TxStart()' for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Endpoint must be locked when calling this function.
*********************************************************************************************************
*/

static  void  USBD_EP_TxAsyncProcess (USBD_DRV    *p_drv,
                                      USBD_EP     *p_ep,
                                      USBD_URB    *p_urb,
                                      CPU_INT08U  *p_buf_cur,
                                      CPU_INT32U   len,
                                      USBD_ERR    *p_err)
{
    USBD_DRV_API  *p_drv_api;


    p_drv_api = p_drv->API_Ptr;                                 /* Get dev drv API struct.                              */

    USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, DrvTxNbr);

    p_urb->NextXferLen = p_drv_api->EP_Tx(p_drv,
                                          p_ep->Addr,
                                          p_buf_cur,
                                          len,
                                          p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    if (p_urb->NextXferLen == len) {                            /* Xfer can be done is a single transaction.            */
        p_ep->XferState = USBD_XFER_STATE_ASYNC;
    } else if ((p_ep->Attrib & USBD_EP_TYPE_MASK) != USBD_EP_TYPE_ISOC) {
        p_ep->XferState = USBD_XFER_STATE_ASYNC_PARTIAL;        /* Xfer will have to be done in many transactions.      */
    } else {
       *p_err = USBD_ERR_TX;                                    /* Cannot split xfer on isoc EP.                        */
        return;
    }

    USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, DrvTxSuccessNbr);
    USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, DrvTxStartNbr);

    p_drv_api->EP_TxStart(p_drv,
                          p_ep->Addr,
                          p_buf_cur,
                          p_urb->NextXferLen,
                          p_err);
    if (*p_err == USBD_ERR_NONE) {
        p_urb->XferLen     += p_urb->NextXferLen;               /* Error not accounted on total xfer len.               */
        p_urb->NextXferLen  = 0u;
        USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, DrvTxStartSuccessNbr);
    }

    return;
}


/*
*********************************************************************************************************
*                                            USBD_EP_Rx()
*
* Description : Receive data on OUT endpoint.This function should not be called from Interrupt Context.
*
* Argument(s) : p_drv           Pointer to device driver structure.
*               -----           Argument checked by caller.
*
*               p_ep            Pointer to endpoint on which data will be received.
*               ----            Argument checked by caller.
*
*               p_buf           Pointer to destination buffer to receive data.
*
*               buf_len         Number of octets to receive.
*
*               async_fnct      Function that will be invoked upon completion of receive operation.
*
*               p_async_arg     Pointer to argument that will be passed as parameter of 'async_fnct'.
*
*               timeout_ms      Timeout in milliseconds.
*
*               p_err           Pointer to variable that will receive return error code from this function :
*               -----           Argument checked by caller.

*                                   USBD_ERR_NONE               Data successfully received.
*                                   USBD_ERR_NULL_PTR           Null pointer passed as argument.
*                                   USBD_ERR_EP_IO_PENDING      Transfer already in progress on endpoint.
*                                   USBD_ERR_OS_FAIL            OS operation failed.
*                                   USBD_ERR_RX                 Generic Rx error.
*
*                                   - RETURNED BY USBD_URB_Get() -
*                                   See USBD_URB_Get() for additional return error codes.
*
*                                   - RETURNED BY USBD_EP_RxStartAsyncProcess() -
*                                   See USBD_EP_RxStartAsyncProcess() for additional return error codes.
*
*                                   - RETURNED BY USBD_OS_EP_SignalPend() -
*                                   See USBD_OS_EP_SignalPend() for additional return error codes.
*
*                                   - RETURNED BY 'p_drv_api->EP_RxStart()' -
*                                   See specific driver(s) 'p_drv_api->EP_RxStart()' for additional return error codes.
*
*                                   - RETURNED BY 'p_drv_api->EP_Rx()' -
*                                   See specific driver(s) 'p_drv_api->EP_Rx()' for additional return error codes.
*
* Return(s)   : Number of octets received, if NO error(s).
*
*               0,                         otherwise.
*
* Note(s)     : (1) This function SHOULD NOT be called from interrupt service routine (ISR).
*
*               (2) Endpoint must be locked when calling this function.
*
*               (3) During a synchronous transfer, endpoint is unlocked before pending on transfer
*                   completion to be able to abort. Since the endpoint is already locked when this
*                   function is called (see callers functions), it releases the lock before pending and
*                   re-locks once the transfer completes.
*
*               (4) This condition covers also the case where the transfer length is multiple of the
*                   maximum packet size. In that case, host sends a zero-length packet considered as
*                   a short packet for the condition.
*********************************************************************************************************
*/

static  CPU_INT32U  USBD_EP_Rx (USBD_DRV         *p_drv,
                                USBD_EP          *p_ep,
                                void             *p_buf,
                                CPU_INT32U        buf_len,
                                USBD_ASYNC_FNCT   async_fnct,
                                void             *p_async_arg,
                                CPU_INT16U        timeout_ms,
                                USBD_ERR         *p_err)
{
    USBD_URB         *p_urb;
    USBD_DRV_API     *p_drv_api;
    USBD_XFER_STATE   prev_xfer_state;
    CPU_INT08U       *p_buf_cur;
    CPU_INT32U        xfer_len;
    CPU_INT32U        xfer_tot;
    CPU_INT32U        prev_xfer_len;
    USBD_ERR          local_err;


    if ((buf_len !=         0u) &&
        (p_buf   == (void *)0)) {
       *p_err = USBD_ERR_NULL_PTR;
        return (0u);
    }

    if (async_fnct == (USBD_ASYNC_FNCT)0) {
        USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, RxSyncExecNbr);
        if (p_ep->XferState != USBD_XFER_STATE_NONE) {
           *p_err = USBD_ERR_EP_IO_PENDING;
            return (0u);
        }
    } else {
        USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, RxAsyncExecNbr);
        if ((p_ep->XferState != USBD_XFER_STATE_NONE) &&
            (p_ep->XferState != USBD_XFER_STATE_ASYNC)) {
           *p_err = USBD_ERR_EP_IO_PENDING;
            return (0u);
        }
    }

    p_urb = USBD_URB_Get(p_drv->DevNbr, p_ep, p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (0u);
    }

    p_urb->BufPtr       = (CPU_INT08U *)p_buf;                  /* Init 'p_urb' fields.                                 */
    p_urb->BufLen       =  buf_len;
    p_urb->XferLen      =  0u;
    p_urb->NextXferLen  =  0u;
    p_urb->AsyncFnct    =  async_fnct;
    p_urb->AsyncFnctArg =  p_async_arg;
    p_urb->Err          =  USBD_ERR_NONE;
    p_urb->NextPtr      = (USBD_URB *)0;

    if (async_fnct != (USBD_ASYNC_FNCT)0) {                     /* -------------------- ASYNC XFER -------------------- */
        p_urb->State    = USBD_URB_STATE_XFER_ASYNC;
        prev_xfer_state = p_ep->XferState;                      /* Keep prev XferState, to restore in case of err.      */
        p_ep->XferState = USBD_XFER_STATE_ASYNC;                /* Set XferState before submitting the xfer.            */

        USBD_EP_RxStartAsyncProcess(p_drv,
                                    p_ep,
                                    p_urb,
                                    p_urb->BufPtr,
                                    p_urb->BufLen,
                                    p_err);
        if (*p_err == USBD_ERR_NONE) {
            USBD_URB_Queue(p_ep, p_urb);                        /* If no err, queue URB.                                */
            USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, RxAsyncSuccessNbr);
        } else {
            p_ep->XferState = prev_xfer_state;                  /* If an err occured, restore prev XferState.           */
            USBD_URB_Free(p_drv->DevNbr, p_ep, p_urb);          /* Free URB.                                            */
        }

        return (0u);
    }

    p_ep->XferState = USBD_XFER_STATE_SYNC;                     /* -------------------- SYNC XFER --------------------- */
    p_urb->State    = USBD_URB_STATE_XFER_SYNC;

    USBD_URB_Queue(p_ep, p_urb);

    p_drv_api          = p_drv->API_Ptr;                        /* Get dev drv API struct.                              */
    p_urb->NextXferLen = p_urb->BufLen;
   *p_err              = USBD_ERR_NONE;

    while ((*p_err              == USBD_ERR_NONE) &&
           ( p_urb->NextXferLen >  0u)) {

        p_buf_cur = &p_urb->BufPtr[p_urb->XferLen];

        USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, DrvRxStartNbr);
        p_urb->NextXferLen = p_drv_api->EP_RxStart(p_drv,
                                                   p_ep->Addr,
                                                   p_buf_cur,
                                                   p_urb->NextXferLen,
                                                   p_err);
        if (*p_err != USBD_ERR_NONE) {
            break;
        }
        USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, DrvRxStartSuccessNbr);

        USBD_OS_EP_LockRelease(p_drv->DevNbr,                   /* Unlock before pending on completion. See Note #3.    */
                               p_ep->Ix);

        USBD_OS_EP_SignalPend(p_drv->DevNbr, p_ep->Ix, timeout_ms, p_err);

        USBD_OS_EP_LockAcquire(p_drv->DevNbr,                   /* Re-lock EP after xfer completion. See Note #3.       */
                               p_ep->Ix,
                               0u,
                              &local_err);

        if (*p_err == USBD_ERR_OS_TIMEOUT) {
            p_drv_api->EP_Abort(p_drv, p_ep->Addr);
            USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, RxSyncTimeoutErrNbr);
            break;
        } else if (*p_err != USBD_ERR_NONE) {
            break;
        } else if (local_err != USBD_ERR_NONE) {
           *p_err = USBD_ERR_OS_FAIL;
            break;
        }

        USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, DrvRxNbr);
        xfer_len = p_drv_api->EP_Rx(p_drv,
                                    p_ep->Addr,
                                    p_buf_cur,
                                    p_urb->NextXferLen,
                                    p_err);
        if (*p_err != USBD_ERR_NONE) {
            break;
        }
        USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, DrvRxSuccessNbr);

        if (xfer_len > p_urb->NextXferLen) {                    /* Rx'd more data than what was expected.               */
            p_urb->XferLen += p_urb->NextXferLen;
           *p_err = USBD_ERR_RX;
        } else {
            p_urb->XferLen     += xfer_len;
            prev_xfer_len       = p_urb->NextXferLen;
            p_urb->NextXferLen  = p_urb->BufLen - p_urb->XferLen;
            if ((xfer_len == 0u) ||                             /* Rx'd a ZLP.                                          */
                (xfer_len  <  prev_xfer_len)) {                 /* Rx'd a short pkt (see Note #4).                      */
                p_urb->NextXferLen = 0u;
            }
        }
    }

    xfer_tot = p_urb->XferLen;

    USBD_URB_Dequeue(p_ep);

    USBD_URB_Free(p_drv->DevNbr, p_ep, p_urb);

    USBD_DBG_STATS_EP_INC_IF_TRUE(p_drv->DevNbr, p_ep->Ix, RxSyncSuccessNbr, (*p_err == USBD_ERR_NONE));

    return (xfer_tot);
}


/*
*********************************************************************************************************
*                                            USBD_EP_Tx()
*
* Description : Send data on IN endpoints.
*
* Argument(s) : p_drv           Pointer to device driver structure.
*               -----           Argument checked by caller.
*
*               p_ep            Pointer to endpoint on which data will be sent.
*               ----            Argument checked by caller.
*
*               p_buf           Pointer to buffer of data that will be sent.
*
*               buf_len         Number of octets to transmit.
*
*               async_fnct      Function that will be invoked upon completion of transmit operation.
*
*               p_async_arg     Pointer to argument that will be passed as parameter of 'async_fnct'.
*
*               timeout_ms      Timeout in milliseconds.
*
*               end             End-of-transfer flag (see Note #2).
*
*               p_err           Pointer to variable that will receive return error code from this function :
*               -----           Argument checked by caller.
*
*                                   USBD_ERR_NONE               Data successfully transmitted.
*                                   USBD_ERR_NULL_PTR           Null pointer passed as argument.
*                                   USBD_ERR_EP_IO_PENDING      Transfer already in progress on endpoint.
*                                   USBD_ERR_OS_FAIL            OS operation failed.
*
*                                   - RETURNED BY USBD_URB_Get() -
*                                   See USBD_URB_Get() for additional return error codes.
*
*                                   - RETURNED BY USBD_EP_TxAsyncProcess() -
*                                   See USBD_EP_TxAsyncProcess() for additional return error codes.
*
*                                   - RETURNED BY USBD_OS_EP_SignalPend() -
*                                   See USBD_OS_EP_SignalPend() for additional return error codes.
*
*                                   - RETURNED BY 'p_drv_api->EP_Tx()' -
*                                   See specific driver(s) 'p_drv_api->EP_Tx()' for additional return error codes.
*
*                                   - RETURNED BY 'p_drv_api->EP_TxStart()' -
*                                   See specific driver(s) 'p_drv_api->EP_TxStart()' for additional return error codes.
*
*                                   - RETURNED BY 'p_drv_api->EP_TxZLP()' -
*                                   See specific driver(s) 'p_drv_api->EP_TxZLP()' for additional return error codes.
*
* Return(s)   : Number of octets transmitted, if NO error(s).
*
*               0,                            otherwise.
*
* Note(s)     : (1) This function SHOULD NOT be called from interrupt service routine (ISR).
*
*               (2) If end-of-transfer is set and transfer length is multiple of maximum packet size,
*                   a zero-length packet is transferred to indicate a short transfer to the host.
*
*               (3) Endpoint must be locked when calling this function.
*
*               (4) During a synchronous transfer, endpoint is unlocked before pending on transfer
*                   completion to be able to abort. Since the endpoint is already locked when this
*                   function is called (see callers functions), it releases the lock before pending and
*                   re-locks once the transfer completes.
*********************************************************************************************************
*/

static  CPU_INT32U  USBD_EP_Tx (USBD_DRV         *p_drv,
                                USBD_EP          *p_ep,
                                void             *p_buf,
                                CPU_INT32U        buf_len,
                                USBD_ASYNC_FNCT   async_fnct,
                                void             *p_async_arg,
                                CPU_INT16U        timeout_ms,
                                CPU_BOOLEAN       end,
                                USBD_ERR         *p_err)
{
    USBD_URB         *p_urb;
    USBD_DRV_API     *p_drv_api;
    USBD_XFER_STATE   prev_xfer_state;
    CPU_INT08U       *p_buf_cur;
    CPU_INT32U        xfer_rem;
    CPU_INT32U        xfer_tot;
    USBD_ERR          local_err;
    CPU_BOOLEAN       zlp_flag = DEF_NO;


    if ((buf_len !=         0u) &&
        (p_buf   == (void *)0)) {
       *p_err = USBD_ERR_NULL_PTR;
        return (0u);
    }

    if (async_fnct == (USBD_ASYNC_FNCT)0) {
        USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, TxSyncExecNbr);
        if (p_ep->XferState != USBD_XFER_STATE_NONE) {
           *p_err = USBD_ERR_EP_IO_PENDING;
            return (0u);
        }
    } else {
        USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, TxAsyncExecNbr);
        if ((p_ep->XferState != USBD_XFER_STATE_NONE) &&
            (p_ep->XferState != USBD_XFER_STATE_ASYNC)) {
           *p_err = USBD_ERR_EP_IO_PENDING;
            return (0u);
        }
    }

    if (buf_len == 0u) {
        zlp_flag = DEF_YES;
    }

    p_urb = USBD_URB_Get(p_drv->DevNbr, p_ep, p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (0u);
    }

    p_urb->BufPtr       = (CPU_INT08U *)p_buf;                  /* Init 'p_urb' fields.                                 */
    p_urb->BufLen       =  buf_len;
    p_urb->XferLen      =  0u;
    p_urb->NextXferLen  =  0u;
    p_urb->AsyncFnct    =  async_fnct;
    p_urb->AsyncFnctArg =  p_async_arg;
    p_urb->Err          =  USBD_ERR_NONE;
    p_urb->NextPtr      = (USBD_URB *)0;
    if (end == DEF_YES) {
        DEF_BIT_SET(p_urb->Flags, USBD_URB_FLAG_XFER_END);
    }

    if (async_fnct != (USBD_ASYNC_FNCT)0) {                     /* -------------------- ASYNC XFER -------------------- */
        p_urb->State    = USBD_URB_STATE_XFER_ASYNC;
        prev_xfer_state = p_ep->XferState;                      /* Keep prev XferState, to restore in case of err.      */
        p_ep->XferState = USBD_XFER_STATE_ASYNC;                /* Set XferState before submitting the xfer.            */

        USBD_EP_TxAsyncProcess(p_drv,
                               p_ep,
                               p_urb,
                               p_urb->BufPtr,
                               p_urb->BufLen,
                               p_err);
        if (*p_err == USBD_ERR_NONE) {
            USBD_URB_Queue(p_ep, p_urb);                        /* If no err, queue URB.                                */
            USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, TxAsyncSuccessNbr);
        } else {
            p_ep->XferState = prev_xfer_state;                  /* If an err occured, restore prev XferState.           */
            USBD_URB_Free(p_drv->DevNbr, p_ep, p_urb);          /* Free URB.                                            */
        }

        return (0u);
    }

    p_ep->XferState = USBD_XFER_STATE_SYNC;                     /* -------------------- SYNC XFER --------------------- */
    p_urb->State    = USBD_URB_STATE_XFER_SYNC;

    USBD_URB_Queue(p_ep, p_urb);

    p_drv_api = p_drv->API_Ptr;                                 /* Get dev drv API struct.                              */
    xfer_rem  = p_urb->BufLen;
   *p_err     = USBD_ERR_NONE;

    while ((*p_err     == USBD_ERR_NONE) &&
           ((xfer_rem  >  0u)            ||
            (zlp_flag  == DEF_YES))) {

        if (zlp_flag == DEF_NO) {
            p_buf_cur = &p_urb->BufPtr[p_urb->XferLen];
        } else {
            p_buf_cur = (CPU_INT08U *)0;
            zlp_flag  = DEF_NO;                                 /* If Tx ZLP, loop done only once.                      */
        }

        USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, DrvTxNbr);
        p_urb->NextXferLen = p_drv_api->EP_Tx(p_drv,
                                              p_ep->Addr,
                                              p_buf_cur,
                                              xfer_rem,
                                              p_err);
        if (*p_err != USBD_ERR_NONE) {
            break;
        }

        USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, DrvTxSuccessNbr);
        USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, DrvTxStartNbr);

        p_drv_api->EP_TxStart(p_drv,
                              p_ep->Addr,
                              p_buf_cur,
                              p_urb->NextXferLen,
                              p_err);
        if (*p_err != USBD_ERR_NONE) {
            break;
        }
        USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, DrvTxStartSuccessNbr);

        USBD_OS_EP_LockRelease(p_drv->DevNbr,                   /* Unlock before pending on completion. See Note #4.    */
                               p_ep->Ix);

        USBD_OS_EP_SignalPend(p_drv->DevNbr, p_ep->Ix, timeout_ms, p_err);

        USBD_OS_EP_LockAcquire(p_drv->DevNbr,                   /* Re-lock EP after xfer completion. See Note #4.       */
                               p_ep->Ix,
                               0u,
                              &local_err);

        if (*p_err == USBD_ERR_OS_TIMEOUT) {
            p_drv_api->EP_Abort(p_drv, p_ep->Addr);
            USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, TxSyncTimeoutErrNbr);
            break;
        } else if (*p_err != USBD_ERR_NONE) {
            break;
        } else if (local_err != USBD_ERR_NONE) {
           *p_err = USBD_ERR_OS_FAIL;
            break;
        }

        p_urb->XferLen += p_urb->NextXferLen;
        xfer_rem       -= p_urb->NextXferLen;
    }

    xfer_tot = p_urb->XferLen;

    if (( end                               == DEF_YES)       &&
        (*p_err                             == USBD_ERR_NONE) &&
        ((p_urb->BufLen % p_ep->MaxPktSize) == 0u)            &&
        ( p_urb->BufLen                     != 0u)) {
                                                                /* $$$$ This case should be tested more thoroughly.     */
        USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, DrvTxZLP_Nbr);

        p_drv_api->EP_TxZLP(p_drv, p_ep->Addr, p_err);

        if (*p_err == USBD_ERR_NONE) {
            USBD_DBG_STATS_EP_INC(p_drv->DevNbr, p_ep->Ix, DrvTxZLP_SuccessNbr);

            USBD_OS_EP_LockRelease(p_drv->DevNbr,               /* Unlock before pending on completion. See Note #4.    */
                                   p_ep->Ix);

            USBD_OS_EP_SignalPend(p_drv->DevNbr, p_ep->Ix, timeout_ms, p_err);

            USBD_OS_EP_LockAcquire(p_drv->DevNbr,               /* Re-lock EP after xfer completion. See Note #4.       */
                                   p_ep->Ix,
                                   0u,
                                  &local_err);
            if (local_err != USBD_ERR_NONE) {
               *p_err = USBD_ERR_OS_FAIL;
            }
        }
    }

    USBD_URB_Dequeue(p_ep);

    USBD_URB_Free(p_drv->DevNbr, p_ep, p_urb);

    USBD_DBG_STATS_EP_INC_IF_TRUE(p_drv->DevNbr, p_ep->Ix, TxSyncSuccessNbr, (*p_err == USBD_ERR_NONE));

    return (xfer_tot);
}


/*
*********************************************************************************************************
*                                        USBD_EP_URB_Abort()
*
* Description : Aborts endpoint's URB(s). Does not free the URB(s), see Note #1.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*               ----        Argument checked by caller.
*
*               p_ep        Pointer to endpoint on which data will be sent.
*               ----        Argument checked by caller.

*               p_err       Pointer to variable that will receive return error code from this function :
*               ----        Argument checked by caller.
*
*                               USBD_ERR_NONE               URB successfully aborted.
*                               USBD_ERR_EP_ABORT           Device driver abort endpoint failed.
*
*                               - RETURNED BY USBD_OS_EP_SignalAbort() -
*                               See USBD_OS_EP_SignalAbort() for additional return error codes.
*
* Return(s)   : Pointer to head of completed URB list, if asynchronous transfer in progress.
*
*               Pointer to NULL,                       otherwise.
*
* Note(s)     : (1) If a synchronous transfer was in progress, a single URB was aborted and must be
*                   freed by calling  USBD_URB_Free(). If an asynchronous transfer was in progress, it
*                   is possible that multiple URBs were aborted and must be freed by calling
*                   USBD_URB_AsyncEnd() with the pointer to the head of the aborted URB(s) list returned
*                   by this function as a parameter.
*
*               (2) Endpoint must be locked when calling this function.
*********************************************************************************************************
*/

static  USBD_URB  *USBD_EP_URB_Abort (USBD_DRV  *p_drv,
                                      USBD_EP   *p_ep,
                                      USBD_ERR  *p_err)
{
    USBD_DRV_API  *p_drv_api;
    CPU_BOOLEAN    abort_ok;
    USBD_URB      *p_urb_head;
    USBD_URB      *p_urb;
    USBD_URB      *p_urb_cur;


    p_drv_api  =  p_drv->API_Ptr;                               /* Get dev drv API struct.                              */
    p_urb_head = (USBD_URB *)0;
    p_urb_cur  = (USBD_URB *)0;

    switch (p_ep->XferState) {
        case USBD_XFER_STATE_NONE:
             abort_ok = DEF_OK;
             break;


        case USBD_XFER_STATE_ASYNC:
        case USBD_XFER_STATE_ASYNC_PARTIAL:
             p_urb = p_ep->URB_HeadPtr;
             while(p_urb != (USBD_URB *)0) {
                 p_urb = USBD_URB_AsyncCmpl(p_ep, USBD_ERR_EP_ABORT);
                 if (p_urb_head == (USBD_URB *)0) {
                     p_urb_head = p_urb;
                 } else {
                     p_urb_cur->NextPtr = p_urb;
                 }
                 p_urb_cur = p_urb;
             }
             abort_ok = p_drv->API_Ptr->EP_Abort(p_drv, p_ep->Addr); /* Call drv's abort fnct.                               */
             break;


        case USBD_XFER_STATE_SYNC:
             USBD_OS_EP_SignalAbort(p_drv->DevNbr, p_ep->Ix, p_err);

             abort_ok = p_drv_api->EP_Abort(p_drv, p_ep->Addr);
             break;


        default:
             abort_ok = DEF_FAIL;
             break;
    }

    if (abort_ok == DEF_OK) {
       *p_err = USBD_ERR_NONE;
    } else {
       *p_err = USBD_ERR_EP_ABORT;
    }

    return (p_urb_head);                                        /* See Note #1.                                         */
}


/*
*********************************************************************************************************
*                                         USBD_URB_AsyncCmpl()
*
* Description : Notify URB completion to asynchronous callback.
*
* Argument(s) : p_ep        Pointer to endpoint on which transfer has completed.
*               ----        Argument checked by caller.
*
*               err         Error associated with transfer.
*
* Return(s)   : Pointer to completed URB, if any.
*
*               Pointer to NULL,          otherwise.
*
* Note(s)     : (1) Endpoint must be locked when calling this function.
*
*               (2) Endpoint must have an asynchronous transfer in progress.
*********************************************************************************************************
*/

static  USBD_URB  *USBD_URB_AsyncCmpl (USBD_EP   *p_ep,
                                       USBD_ERR   err)
{
    USBD_URB  *p_urb;


    p_urb = p_ep->URB_HeadPtr;                                  /* Get head URB for EP.                                 */
    if (p_urb == (USBD_URB *)0) {
        return (p_urb);
    }

    USBD_URB_Dequeue(p_ep);                                     /* Dequeue first URB from EP.                           */

    p_urb->Err     =  err;                                      /* Set err for curr URB.                                */
    p_urb->NextPtr = (USBD_URB *)0;                             /* Remove links with 'p_ep' URB linked list.            */

    return (p_urb);
}


/*
*********************************************************************************************************
*                                          USBD_URB_AsyncEnd()
*
* Description : Execute callback associated with each USB request block in the linked list and free them.
*
* Argument(s) : dev_nbr     Device number.
*               -------     Argument checked by caller.
*
*               p_ep        Pointer to endpoint structure.
*               ----        Argument checked by caller.
*
*               p_urb_head  Pointer to head of USB request block linked list.
*               -----       Argument checked by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) Endpoint must NOT be locked when calling this function.
*********************************************************************************************************
*/

static  void  USBD_URB_AsyncEnd (CPU_INT08U   dev_nbr,
                                 USBD_EP     *p_ep,
                                 USBD_URB    *p_urb_head)
{
    USBD_URB         *p_urb_cur;
    USBD_URB         *p_urb_next;
    void             *p_buf;
    CPU_INT32U        buf_len;
    CPU_INT32U        xfer_len;
    USBD_ASYNC_FNCT   async_fnct;
    void             *p_async_arg;
    USBD_ERR          err;


    p_urb_cur = p_urb_head;
    while (p_urb_cur != (USBD_URB *)0) {                        /* Iterate through linked list.                         */
        p_buf       = (void *)p_urb_cur->BufPtr;
        buf_len     =  p_urb_cur->BufLen;
        xfer_len    =  p_urb_cur->XferLen;
        async_fnct  =  p_urb_cur->AsyncFnct;
        p_async_arg =  p_urb_cur->AsyncFnctArg;
        err         =  p_urb_cur->Err;
        p_urb_next  =  p_urb_cur->NextPtr;

        USBD_URB_Free(dev_nbr, p_ep, p_urb_cur);                /* Free URB to pool.                                    */

        async_fnct(dev_nbr,                                     /* Execute callback fnct.                               */
                   p_ep->Addr,
                   p_buf,
                   buf_len,
                   xfer_len,
                   p_async_arg,
                   err);

        p_urb_cur = p_urb_next;
    }
}


/*
*********************************************************************************************************
*                                           USBD_URB_Free()
*
* Description : Free URB to URB pool.
*
* Argument(s) : dev_nbr     Device number.
*               -------     Argument checked by caller.
*
*               p_ep        Pointer to endpoint structure.
*               ----        Argument checked by caller.
*
*               p_urb       Pointer to USB request block.
*               -----       Argument checked by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_URB_Free (CPU_INT08U   dev_nbr,
                             USBD_EP     *p_ep,
                             USBD_URB    *p_urb)
{
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    p_urb->State             = USBD_URB_STATE_IDLE;
    p_urb->NextPtr           = USBD_URB_TblPtr[dev_nbr];
    USBD_URB_TblPtr[dev_nbr] = p_urb;

#if (USBD_CFG_MAX_NBR_URB_EXTRA > 0u)
    if (DEF_BIT_IS_SET(p_urb->Flags, USBD_URB_FLAG_EXTRA_URB)) {
                                                                /* If the URB freed is an 'extra' URB, dec ctr.         */
        USBD_URB_ExtraCtr[dev_nbr]--;
    } else {
        p_ep->URB_MainAvail = DEF_YES;
    }
#else
    (void)p_ep;
#endif
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                           USBD_URB_Get()
*
* Description : Get URB from URB pool.
*
* Argument(s) : dev_nbr     Device number.
*               -------     Argument checked by caller.
*
*               p_ep        Pointer to endpoint structure.
*               ----        Argument checked by caller.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*               ----        Argument checked by caller.
*
*                               USBD_ERR_NONE               URB successfully returned.
*                               USBD_ERR_EP_QUEUING         No URB available in the pool.
*
* Return(s)   : Pointer to USB request block, if NO error(s).
*
*               Pointer to NULL,              otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  USBD_URB  *USBD_URB_Get (CPU_INT08U   dev_nbr,
                                 USBD_EP     *p_ep,
                                 USBD_ERR    *p_err)
{
    CPU_BOOLEAN  ep_empty;
    USBD_URB  *p_urb;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    ep_empty = ((p_ep->URB_HeadPtr == (USBD_URB *)0) && (p_ep->URB_TailPtr == (USBD_URB *)0)) ? DEF_YES : DEF_NO;

#if (USBD_CFG_MAX_NBR_URB_EXTRA > 0u)
                                                                /* Check if EP is empty, if enough URB rem or if main...*/
                                                                /* ...URB avail.                                        */
    if ((ep_empty                   == DEF_YES) ||
        (USBD_URB_ExtraCtr[dev_nbr] <  USBD_CFG_MAX_NBR_URB_EXTRA) ||
        (p_ep->URB_MainAvail        == DEF_YES)) {
#else
    if (ep_empty == DEF_YES) {                                  /* Check if EP is empty.                                */
#endif

        p_urb = USBD_URB_TblPtr[dev_nbr];
        if (p_urb != (USBD_URB *)0) {
            USBD_URB_TblPtr[dev_nbr] = p_urb->NextPtr;

            p_urb->NextPtr = (USBD_URB *)0;
            p_urb->Flags   =  0u;
#if (USBD_CFG_MAX_NBR_URB_EXTRA > 0u)
            if ((ep_empty            == DEF_NO) &&
                (p_ep->URB_MainAvail == DEF_NO)) {
                                                                /* If the EP already has an URB in progress, inc ...    */
                USBD_URB_ExtraCtr[dev_nbr]++;                   /* ... ctr and mark the URB as an 'extra' URB.          */
                DEF_BIT_SET(p_urb->Flags, USBD_URB_FLAG_EXTRA_URB);
            } else if (p_ep->URB_MainAvail == DEF_YES) {
                p_ep->URB_MainAvail = DEF_NO;
            }
#endif
           *p_err = USBD_ERR_NONE;
        } else {
           *p_err =  USBD_ERR_EP_QUEUING;
            p_urb = (USBD_URB *)0;
        }
    } else {

       *p_err =  USBD_ERR_EP_QUEUING;
        p_urb = (USBD_URB *)0;
    }
    CPU_CRITICAL_EXIT();

    return (p_urb);
}


/*
*********************************************************************************************************
*                                          USBD_URB_Queue()
*
* Description : Queue USB request block into endpoint.
*
* Argument(s) : p_ep        Pointer to endpoint structure.
*               ----        Argument checked by caller.
*
*               p_urb       Pointer to USB request block.
*               -----       Argument checked by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) Endpoint must be locked when calling this function.
*********************************************************************************************************
*/

static  void  USBD_URB_Queue (USBD_EP   *p_ep,
                              USBD_URB  *p_urb)
{
    p_urb->NextPtr = (USBD_URB *)0;


    if (p_ep->URB_TailPtr == (USBD_URB *)0) {                   /* Q is empty.                                          */
        p_ep->URB_HeadPtr = p_urb;
        p_ep->URB_TailPtr = p_urb;
    } else {                                                    /* Q is not empty.                                      */
        p_ep->URB_TailPtr->NextPtr = p_urb;
        p_ep->URB_TailPtr          = p_urb;
    }

    return;
}


/*
*********************************************************************************************************
*                                         USBD_URB_Dequeue()
*
* Description : Dequeue head USB request block from endpoint.
*
* Argument(s) : p_ep        Pointer to endpoint structure.
*               ----        Argument checked by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) Endpoint must be locked when calling this function.
*********************************************************************************************************
*/

static  void  USBD_URB_Dequeue (USBD_EP  *p_ep)
{
    USBD_URB  *p_urb;


    p_urb = p_ep->URB_HeadPtr;
    if (p_urb == (USBD_URB *)0) {
        return;
    }

    if (p_urb->NextPtr == (USBD_URB *)0) {                      /* Only one URB is queued.                              */
        p_ep->URB_HeadPtr = (USBD_URB *)0;
        p_ep->URB_TailPtr = (USBD_URB *)0;
        p_ep->XferState   =  USBD_XFER_STATE_NONE;
    } else {
        p_ep->URB_HeadPtr = p_urb->NextPtr;
    }
}


