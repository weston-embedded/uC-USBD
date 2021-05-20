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
*                                USB COMMUNICATIONS DEVICE CLASS (CDC)
*                                   ETHERNET EMULATION MODEL (EEM)
*
* Filename : usbd_cdc_eem.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)       : (1) This implementation is compliant with the CDC-EEM specification "Universal Serial
*                     Bus Communications Class Subclass Specification for Ethernet Emulation Model
*                     Devices" revision 1.0. February 2, 2005.
*
*                 (2) This class implementation does NOT use the CDC base class implementation as
*                     CDC-EEM specification does not follow the CDC specification revision 1.2
*                     December 6, 2012.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    USBD_CDC_EEM_MODULE
#include  <lib_mem.h>
#include  <usbd_cfg.h>
#include  <KAL/kal.h>
#include  "usbd_cdc_eem.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

                                                                /* Dflt Rx buffer qty is 1.                             */
#ifndef  USBD_CDC_EEM_CFG_RX_BUF_QTY_PER_DEV
#define  USBD_CDC_EEM_CFG_RX_BUF_QTY_PER_DEV              1u
#endif

                                                                /* Max nbr of comm struct.                              */
#define  USBD_CDC_EEM_COMM_NBR_MAX                 (USBD_CDC_EEM_CFG_MAX_NBR_DEV * \
                                                    USBD_CDC_EEM_CFG_MAX_NBR_CFG)


#define  USBD_CDC_EEM_MAX_RETRY_CNT                       3u    /* Max nbr of retry when Rx buf submission fails.*/


#define  USBD_CDC_EEM_SUBCLASS_CODE                     0x0C
#define  USBD_CDC_EEM_PROTOCOL_CODE                     0x07


/*
*********************************************************************************************************
*                                ZERO LENGTH EEM (ZLE) SPECIAL PACKET
*
* Note(s) : (1) ZLE packet is defined in "Universal Serial Bus Communications Class Subclass
*               Specification for Ethernet Emulation Model Devices" revision 1.0. February 2, 2005,
*               section 5.1.2.3.1.
*********************************************************************************************************
*/

#define  USBD_CDC_EEM_HDR_ZLE                         0x0000


/*
*********************************************************************************************************
*                                            PACKET TYPES
*
* Note(s) : (1) Packet types are defined in "Universal Serial Bus Communications Class Subclass
*               Specification for Ethernet Emulation Model Devices" revision 1.0. February 2, 2005,
*               section 5.1.2.
*********************************************************************************************************
*/

#define  USBD_CDC_EEM_PKT_TYPE_MASK               DEF_BIT_15
#define  USBD_CDC_EEM_PKT_TYPE_PAYLOAD                    0u
#define  USBD_CDC_EEM_PKT_TYPE_CMD                        1u


/*
*********************************************************************************************************
*                                EEM PAYLOAD PACKET HEADER DEFINITIONS
*
* Note(s) : (1) Payload header format is defined in "Universal Serial Bus Communications Class Subclass
*               Specification for Ethernet Emulation Model Devices" revision 1.0. February 2, 2005,
*               section 5.1.2.1.
*
*               (a) Payload packets header have the following format:
*
*                   +---------+----------------------------------+
*                   | Bit     |   15   |  14   | 13..0           |
*                   +---------+----------------------------------+
*                   | Content | bmType | bmCRC | Length of       |
*                   |         |  (0)   |       | Ethernet frame  |
*                   +---------+----------------------------------+
*
*                   (1) bmCRC indicates whether the Ethernet frame contains a valid CRC or a CRC set
*                       to 0xDEADBEEF.
*********************************************************************************************************
*/

                                                                /* ----------------- PAYLOAD PKT CRC ------------------ */
#define  USBD_CDC_EEM_PAYLOAD_CRC_MASK            DEF_BIT_14
#define  USBD_CDC_EEM_PAYLOAD_CRC_NOT_CALC                0u
#define  USBD_CDC_EEM_PAYLOAD_CRC_CALC                    1u

                                                                /* ----------------- PAYLOAD PKT LEN ------------------ */
#define  USBD_CDC_EEM_PAYLOAD_LEN_MASK         DEF_BIT_FIELD(14u, 0u)


/*
*********************************************************************************************************
*                                EEM COMMAND PACKET HEADER DEFINITIONS
*
* Note(s) : (1) Command header format is defined in "Universal Serial Bus Communications Class Subclass
*               Specification for Ethernet Emulation Model Devices" revision 1.0. February 2, 2005,
*               section 5.1.2.2.
*
*               (a) Command packets header have the following format:
*
*                   +---------+------------------------------------------------+
*                   | Bit     | 15     | 14         | 13..11   | 10..0         |
*                   +---------+------------------------------------------------+
*                   | Content | bmType | bmReserved | bmEEMCmd | bmEEMCmdParam |
*                   |         |  (1)   |    (0)     |          |               |
*                   +---------+------------------------------------------------+
*
*                   (1) bmEEMCmd gives the code of the command to execute.
*
*                   (2) bmEEMCmdParam gives extra information for the command execution. Format depends
*                       on the command code.
*********************************************************************************************************
*/

                                                                /* ------------------- CMD RSVD BIT ------------------- */
#define  USBD_CDC_EEM_CMD_RSVD_MASK               DEF_BIT_14
#define  USBD_CDC_EEM_CMD_RSVD_OK                         0u

                                                                /* ------------------- CMD PKT CODE ------------------- */
#define  USBD_CDC_EEM_CMD_CODE_MASK            DEF_BIT_FIELD(3u, 11u)
#define  USBD_CDC_EEM_CMD_CODE_ECHO                       0u
#define  USBD_CDC_EEM_CMD_CODE_ECHO_RESP                  1u
#define  USBD_CDC_EEM_CMD_CODE_SUSP_HINT                  2u
#define  USBD_CDC_EEM_CMD_CODE_RESP_HINT                  3u
#define  USBD_CDC_EEM_CMD_CODE_RESP_CMPL_HINT             4u
#define  USBD_CDC_EEM_CMD_CODE_TICKLE                     5u

                                                                /* ------------------- CMD PKT PARAM ------------------- */
#define  USBD_CDC_EEM_CMD_PARAM_MASK           DEF_BIT_FIELD(11u, 0u)

                                                                /* ------------------- ECHO CMD LEN ------------------- */
#define  USBD_CDC_EEM_CMD_PARAM_ECHO_LEN_MASK  DEF_BIT_FIELD(10u, 0u)


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/

                                                                /* Dflt CRC value.                                      */
static  const  CPU_INT08U  USBD_CDC_EEM_PayloadCRC[] = {0xDE, 0xAD, 0xBE, 0xEF};


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                        FORWARD DECLARATIONS
*********************************************************************************************************
*/

typedef  struct  usbd_cdc_eem_ctrl  USBD_CDC_EEM_CTRL;          /* Forward declaration.                                 */


/*
*********************************************************************************************************
*                                         CDC-EEM CLASS STATES
*********************************************************************************************************
*/

typedef  enum  usbd_cdc_eem_state {
    USBD_CDC_EEM_STATE_NONE = 0,
    USBD_CDC_EEM_STATE_INIT,
    USBD_CDC_EEM_STATE_CFG
} USBD_CDC_EEM_STATE;


/*
*********************************************************************************************************
*                                  CDC-EEM CLASS BUFFER QUEUE ENTRY
*********************************************************************************************************
*/

typedef  struct  usbd_cdc_eem_buf_entry {
    CPU_INT08U   *BufPtr;                                       /* Pointer to buffer.                                   */
    CPU_INT16U    BufLen;                                       /* Buffer length in bytes.                              */
    CPU_BOOLEAN   CrcComputed;                                  /* Flag indicates if CRC computed for this buf.         */
} USBD_CDC_EEM_BUF_ENTRY;


/*
*********************************************************************************************************
*                                       CDC-EEM CLASS COMM INFO
*********************************************************************************************************
*/

typedef  struct  usbd_cdc_eem_comm {
    USBD_CDC_EEM_CTRL  *CtrlPtr;                                /* Ptr to ctrl info.                                    */
                                                                /* Avail EP for comm: Bulk (and Intr)                   */
    CPU_INT08U          DataInEpAddr;                           /* Address of Bulk IN EP.                               */
    CPU_INT08U          DataOutEpAddr;                          /* Address of Bulk OUT EP.                              */
} USBD_CDC_EEM_COMM;


/*
*********************************************************************************************************
*                                     CDC-EEM CLASS BUFFER QUEUE
*********************************************************************************************************
*/

typedef  struct  usbd_cdc_eem_buf_q {
    USBD_CDC_EEM_BUF_ENTRY  *Tbl;                               /* Ptr to table of buffer entry.                        */
    CPU_INT08U               InIdx;                             /* In  Q index.                                         */
    CPU_INT08U               OutIdx;                            /* Out Q index.                                         */
    CPU_INT08U               Cnt;                               /* Nbr of elements in Q.                                */
    CPU_INT08U               Size;                              /* Size of Q.                                           */
} USBD_CDC_EEM_BUF_Q;


/*
*********************************************************************************************************
*                                       CDC-EEM CLASS CTRL INFO
*********************************************************************************************************
*/

struct  usbd_cdc_eem_ctrl {
    CPU_INT08U           DevNbr;                                /* Dev nbr to which this class instance is associated.  */
    CPU_INT08U           ClassNbr;                              /* Class instacne number.                               */
    USBD_CDC_EEM_COMM   *CommPtr;                               /* Ptr to comm.                                         */

    USBD_CDC_EEM_STATE   State;                                 /* Class instance current state.                        */
    CPU_INT08U           StartCnt;                              /* Start cnt.                                           */
    KAL_LOCK_HANDLE      StateLockHandle;                       /* Handle on lock for class instance state.             */

    CPU_INT08U           RxErrCnt;                              /* Cnt of Rx error.                                     */

                                                                /* Ptr to Rx buffer table.                              */
    CPU_INT08U          *RxBufPtrTbl[USBD_CDC_EEM_CFG_RX_BUF_QTY_PER_DEV];

    CPU_INT08U          *BufEchoPtr;                            /* Ptr to buffer that contains echo data.               */

    USBD_CDC_EEM_DRV    *DrvPtr;                                /* Ptr to network driver.                               */
    void                *DrvArgPtr;                             /* Arg of network driver.                               */

                                                                /* ----------------- RX STATE MACHINE ----------------- */
    CPU_INT16U           CurHdr;                                /* Hdr of current buffer.                               */
    CPU_INT32U           CurBufLenRem;                          /* Remaining length of current buffer.                  */
    CPU_INT08U          *CurBufPtr;                             /* Pointer to current network buffer.                   */
    CPU_INT16U           CurBufIx;                              /* Index of network buffer to write.                    */
    CPU_BOOLEAN          CurBufCrcComputed;                     /* Flag indicates if CRC is computed on current buf.    */

                                                                /* ---------------- NETWORK BUFFER QS ----------------- */
    USBD_CDC_EEM_BUF_Q   RxBufQ;                                /* Rx buffer Q.                                         */
    USBD_CDC_EEM_BUF_Q   TxBufQ;                                /* Tx buffer Q.                                         */
    CPU_BOOLEAN          TxInProgress;                          /* Flag that indicates if a Tx is in progress.          */
};


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
                                                                /* CDC EEM class instances array.                        */
static  USBD_CDC_EEM_CTRL  USBD_CDC_EEM_CtrlTbl[USBD_CDC_EEM_CFG_MAX_NBR_DEV];
static  CPU_INT08U         USBD_CDC_EEM_CtrlNbrNext;
                                                                /* CDC EEM class comm array.                             */
static  USBD_CDC_EEM_COMM  USBD_CDC_EEM_CommTbl[USBD_CDC_EEM_COMM_NBR_MAX];
static  CPU_INT08U         USBD_CDC_EEM_CommNbrNext;


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void         USBD_CDC_EEM_Conn       (CPU_INT08U           dev_nbr,
                                              CPU_INT08U           cfg_nbr,
                                              void                *p_if_class_arg);

static  void         USBD_CDC_EEM_Disconn    (CPU_INT08U           dev_nbr,
                                              CPU_INT08U           cfg_nbr,
                                              void                *p_if_class_arg);

static  void         USBD_CDC_EEM_CommStart  (USBD_CDC_EEM_CTRL   *p_ctrl,
                                              USBD_ERR            *p_err);

static  void         USBD_CDC_EEM_RxCmpl     (CPU_INT08U           dev_nbr,
                                              CPU_INT08U           ep_addr,
                                              void                *p_buf,
                                              CPU_INT32U           buf_len,
                                              CPU_INT32U           xfer_len,
                                              void                *p_arg,
                                              USBD_ERR             err);

static  void         USBD_CDC_EEM_TxCmpl     (CPU_INT08U           dev_nbr,
                                              CPU_INT08U           ep_addr,
                                              void                *p_buf,
                                              CPU_INT32U           buf_len,
                                              CPU_INT32U           xfer_len,
                                              void                *p_arg,
                                              USBD_ERR             err);

static  void         USBD_CDC_EEM_TxBufSubmit(USBD_CDC_EEM_CTRL   *p_ctrl,
                                              CPU_INT08U          *p_buf,
                                              CPU_INT16U           buf_len,
                                              USBD_ERR            *p_err);

static  void         USBD_CDC_EEM_BufQ_Add   (USBD_CDC_EEM_BUF_Q  *p_buf_q,
                                              CPU_INT08U          *p_buf,
                                              CPU_INT16U           buf_len,
                                              CPU_BOOLEAN          crc_computed);

static  CPU_INT08U  *USBD_CDC_EEM_BufQ_Get   (USBD_CDC_EEM_BUF_Q  *p_buf_q,
                                              CPU_INT16U          *p_buf_len,
                                              CPU_BOOLEAN         *p_crc_computed);

static  void         USBD_CDC_EEM_StateLock  (USBD_CDC_EEM_CTRL   *p_ctrl,
                                              USBD_ERR            *p_err);

static  void         USBD_CDC_EEM_StateUnlock(USBD_CDC_EEM_CTRL   *p_ctrl,
                                              USBD_ERR            *p_err);


/*
*********************************************************************************************************
*                                         CDC-EEM CLASS DRIVER
*********************************************************************************************************
*/

static  USBD_CLASS_DRV  USBD_CDC_EEM_Drv = {
    USBD_CDC_EEM_Conn,
    USBD_CDC_EEM_Disconn,
    DEF_NULL,
    DEF_NULL,
    DEF_NULL,
    DEF_NULL,
    DEF_NULL,
    DEF_NULL,
    DEF_NULL,
    DEF_NULL,
    DEF_NULL,

#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
    DEF_NULL,
    DEF_NULL,
#endif
};


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                          GLOBAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         USBD_CDC_EEM_Init()
*
* Description : Initializes internal structures and variables used by the CDC EEM class.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Operation was successful.
*                               USBD_ERR_OS_SIGNAL_CREATE   Unable to create state lock.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_CDC_EEM_Init (USBD_ERR  *p_err)
{
    CPU_INT08U          ix;
    USBD_CDC_EEM_CTRL  *p_ctrl;
    USBD_CDC_EEM_COMM  *p_comm;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == DEF_NULL) {
        CPU_SW_EXCEPTION(;);
    }
#endif

    for (ix = 0u; ix < USBD_CDC_EEM_CFG_MAX_NBR_DEV; ix++) {    /* Init ctrl struct.                                    */
        KAL_ERR  err_kal;


        p_ctrl            = &USBD_CDC_EEM_CtrlTbl[ix];
        p_ctrl->DevNbr    =  USBD_DEV_NBR_NONE;
        p_ctrl->State     =  USBD_CDC_EEM_STATE_NONE;
        p_ctrl->ClassNbr  =  USBD_CLASS_NBR_NONE;
        p_ctrl->CommPtr   =  DEF_NULL;
        p_ctrl->DrvPtr    =  DEF_NULL;
        p_ctrl->DrvArgPtr =  DEF_NULL;
        p_ctrl->StartCnt  =  0u;

        p_ctrl->StateLockHandle = KAL_LockCreate("USBD - CDC EEM State lock",
                                                  DEF_NULL,
                                                 &err_kal);
        if (err_kal != KAL_ERR_NONE) {
           *p_err = USBD_ERR_OS_SIGNAL_CREATE;
            return;
        }
    }

    for (ix = 0u; ix < USBD_CDC_EEM_COMM_NBR_MAX; ix++) {       /* Init comm struct.                                    */
        p_comm                    = &USBD_CDC_EEM_CommTbl[ix];
        p_comm->CtrlPtr           =  DEF_NULL;
        p_comm->DataInEpAddr      =  USBD_EP_ADDR_NONE;
        p_comm->DataOutEpAddr     =  USBD_EP_ADDR_NONE;
    }

    USBD_CDC_EEM_CtrlNbrNext = 0u;
    USBD_CDC_EEM_CommNbrNext = 0u;

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          USBD_CDC_EEM_Add()
*
* Description : Adds a new instance of the CDC EEM class.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE       Operation was successful.
*                               USBD_ERR_ALLOC      No more class instance structure available.
*
*
* Return(s)   : Class instance number, if NO error(s).
*
*               USBD_CLASS_NBR_NONE,   otherwise.
*
* Note(s)     : None.
*********************************************************************************************************
*/

CPU_INT08U  USBD_CDC_EEM_Add (USBD_ERR  *p_err)
{
    CPU_INT08U          class_nbr;
    CPU_INT08U          buf_cnt;
    LIB_ERR             err_lib;
    USBD_CDC_EEM_CTRL  *p_ctrl;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == DEF_NULL) {
        CPU_SW_EXCEPTION(USBD_CLASS_NBR_NONE);
    }
#endif

    CPU_CRITICAL_ENTER();
    class_nbr = USBD_CDC_EEM_CtrlNbrNext;
    if (class_nbr >= USBD_CDC_EEM_CFG_MAX_NBR_DEV) {
        CPU_CRITICAL_EXIT();

       *p_err = USBD_ERR_ALLOC;
        return (USBD_CLASS_NBR_NONE);
    }
    USBD_CDC_EEM_CtrlNbrNext++;
    CPU_CRITICAL_EXIT();

    p_ctrl = &USBD_CDC_EEM_CtrlTbl[class_nbr];

                                                                /* Alloc buffer used by echo response command.          */
    p_ctrl->BufEchoPtr = (CPU_INT08U *)Mem_HeapAlloc(USBD_CDC_EEM_CFG_ECHO_BUF_LEN,
                                                     USBD_CFG_BUF_ALIGN_OCTETS,
                                                     DEF_NULL,
                                                    &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = USBD_ERR_ALLOC;
        return (USBD_CLASS_NBR_NONE);
    }

                                                                /* Allocate Rx buffers.                                 */
    for (buf_cnt = 0u; buf_cnt < USBD_CDC_EEM_CFG_RX_BUF_QTY_PER_DEV; buf_cnt++) {
        p_ctrl->RxBufPtrTbl[buf_cnt] = (CPU_INT08U *)Mem_HeapAlloc(USBD_CDC_EEM_CFG_RX_BUF_LEN,
                                                                   USBD_CFG_BUF_ALIGN_OCTETS,
                                                                   DEF_NULL,
                                                                  &err_lib);
        if (err_lib != LIB_MEM_ERR_NONE) {
           *p_err = USBD_ERR_ALLOC;
            return (USBD_CLASS_NBR_NONE);
        }
    }

   *p_err = USBD_ERR_NONE;

    return (class_nbr);
}


/*
*********************************************************************************************************
*                                        USBD_CDC_EEM_CfgAdd()
*
* Description : Add CDC-EEM class instance into the specified configuration.
*
* Argument(s) : class_nbr               Class instance number.
*
*               dev_nbr                 Device number.
*
*               cfg_nbr                 Configuration index to add CDC-EEM class instance to.
*
*               p_if_name               Pointer to string that contains name of the CDC-EEM interface.
*                                       Can be DEF_NULL.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Operation was successful.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'.
*                               USBD_ERR_ALLOC                  No more class communication structure available.
*
*                               -RETURNED BY USBD_IF_Add()-
*                               See USBD_IF_Add() for additional return error codes.
*
*                               -RETURNED BY USBD_BulkAdd()-
*                               See USBD_BulkAdd() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_CDC_EEM_CfgAdd (       CPU_INT08U   class_nbr,
                                  CPU_INT08U   dev_nbr,
                                  CPU_INT08U   cfg_nbr,
                           const  CPU_CHAR    *p_if_name,
                                  USBD_ERR    *p_err)
{
    USBD_CDC_EEM_CTRL  *p_ctrl;
    USBD_CDC_EEM_COMM  *p_comm;
    CPU_INT08U         if_nbr;
    CPU_INT08U         ep_addr;
    CPU_INT16U         comm_nbr;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == DEF_NULL) {
        CPU_SW_EXCEPTION(;);
    }

    if (class_nbr >= USBD_CDC_EEM_CtrlNbrNext) {
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }
#endif

    p_ctrl = &USBD_CDC_EEM_CtrlTbl[class_nbr];
    CPU_CRITICAL_ENTER();
    if ((p_ctrl->DevNbr != USBD_DEV_NBR_NONE) &&                /* Chk if class is associated with a different dev.     */
        (p_ctrl->DevNbr != dev_nbr)) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }

    p_ctrl->DevNbr = dev_nbr;

    comm_nbr = USBD_CDC_EEM_CommNbrNext;
    if (comm_nbr >= USBD_CDC_EEM_COMM_NBR_MAX) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_ALLOC;
        return;
    }
    USBD_CDC_EEM_CommNbrNext++;
    CPU_CRITICAL_EXIT();

    p_comm = &USBD_CDC_EEM_CommTbl[comm_nbr];

                                                                /* ------------------ BUILD USB FNCT ------------------ */
    if_nbr = USBD_IF_Add(        dev_nbr,
                                 cfg_nbr,
                                &USBD_CDC_EEM_Drv,
                         (void *)p_comm,
                                 DEF_NULL,
                                 USBD_CLASS_CODE_CDC_CONTROL,
                                 USBD_CDC_EEM_SUBCLASS_CODE,
                                 USBD_CDC_EEM_PROTOCOL_CODE,
                                 p_if_name,
                                 p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    ep_addr = USBD_BulkAdd(dev_nbr,
                           cfg_nbr,
                           if_nbr,
                           0u,
                           DEF_YES,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    p_comm->DataInEpAddr = ep_addr;


    ep_addr = USBD_BulkAdd(dev_nbr,
                           cfg_nbr,
                           if_nbr,
                           0u,
                           DEF_NO,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    p_comm->DataOutEpAddr = ep_addr;


    CPU_CRITICAL_ENTER();
    p_ctrl->State    = USBD_CDC_EEM_STATE_INIT;
    p_ctrl->ClassNbr = class_nbr;
    p_ctrl->CommPtr  = DEF_NULL;
    CPU_CRITICAL_EXIT();

    p_comm->CtrlPtr = p_ctrl;

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        USBD_CDC_EEM_IsConn()
*
* Description : Gets the CDC-EEM class instance connection state.
*
* Argument(s) : class_nbr   Class instance number.
*
* Return(s)   : DEF_YES, if class instance is connected.
*
*               DEF_NO,  otherwise.
*
* Note(s)     : None.
*********************************************************************************************************
*/

CPU_BOOLEAN  USBD_CDC_EEM_IsConn (CPU_INT08U  class_nbr)
{
    USBD_DEV_STATE       dev_state;
    USBD_CDC_EEM_CTRL   *p_ctrl;
    USBD_CDC_EEM_STATE   class_state;
    USBD_ERR             err;


                                                                /* ------------------- VALIDATE ARG ------------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    {
        CPU_SR_ALLOC();


        CPU_CRITICAL_ENTER();
        if (class_nbr >= USBD_CDC_EEM_CtrlNbrNext) {
            CPU_CRITICAL_EXIT();

            return (DEF_NO);
        }
        CPU_CRITICAL_EXIT();
    }
#endif

    p_ctrl    = &USBD_CDC_EEM_CtrlTbl[class_nbr];
    dev_state =  USBD_DevStateGet(p_ctrl->DevNbr, &err);
    if (err != USBD_ERR_NONE) {
        return (DEF_NO);
    }

    USBD_CDC_EEM_StateLock(p_ctrl, &err);
    if (err != USBD_ERR_NONE) {
        return (DEF_NO);
    }

    class_state = p_ctrl->State;
    USBD_CDC_EEM_StateUnlock(p_ctrl, &err);

    if ((err         == USBD_ERR_NONE            ) &&
        (dev_state   == USBD_DEV_STATE_CONFIGURED) &&
        (class_state == USBD_CDC_EEM_STATE_CFG   )) {
        return (DEF_YES);
    } else {
        return (DEF_NO);
    }
}


/*
*********************************************************************************************************
*                                     USBD_CDC_EEM_InstanceInit()
*
* Description : Initializes CDC-EEM class instance according to network driver needs.
*
* Argument(s) : class_nbr       Class instance number.
*
*               p_cfg           Pointer to CDC-EEM configuration structure.
*
*               p_cdc_eem_drv   Pointer to CDC-EEM driver structure.
*
*               p_arg           Pointer to CDC-EEM driver argument.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Operation was successful.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'
*                                                               /'p_cfg'/'p_cdc_eem_drv'.
*                               USBD_ERR_NULL_PTR               Invalid null pointer passed to 'p_cfg'.
*                               USBD_ERR_ALLOC                  Unable to allocate Rx and or Tx queue(s).
*                               USBD_ERR_INVALID_CLASS_STATE    Class instance laready configured.
*
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_CDC_EEM_InstanceInit (CPU_INT08U         class_nbr,
                                 USBD_CDC_EEM_CFG  *p_cfg,
                                 USBD_CDC_EEM_DRV  *p_cdc_eem_drv,
                                 void              *p_arg,
                                 USBD_ERR          *p_err)
{
    LIB_ERR             err_lib;
    USBD_CDC_EEM_CTRL  *p_ctrl;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    {
        CPU_SR_ALLOC();


        if (p_err == DEF_NULL) {
            CPU_SW_EXCEPTION(;);
        }

        CPU_CRITICAL_ENTER();
        if (class_nbr >= USBD_CDC_EEM_CtrlNbrNext) {
            CPU_CRITICAL_EXIT();

           *p_err = USBD_ERR_INVALID_ARG;
            return;
        }
        CPU_CRITICAL_EXIT();

        if (p_cfg == DEF_NULL) {
           *p_err = USBD_ERR_NULL_PTR;
            return;
        }

        if ((p_cfg->RxBufQSize < 1u) ||
            (p_cfg->TxBufQSize < 2u)) {
           *p_err = USBD_ERR_INVALID_ARG;
            return;
        }

        if (p_cdc_eem_drv == DEF_NULL) {
           *p_err = USBD_ERR_INVALID_ARG;
            return;
        }
    }
#endif

    p_ctrl = &USBD_CDC_EEM_CtrlTbl[class_nbr];

#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_ctrl->DrvPtr != DEF_NULL) {                           /* Ensure class instance is not already configured.     */
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return;
    }
#endif

    p_ctrl->DrvPtr    = p_cdc_eem_drv;
    p_ctrl->DrvArgPtr = p_arg;

                                                                /* ---------------- ALLOC TX AND RX QS ---------------- */
    p_ctrl->TxBufQ.Size =  p_cfg->TxBufQSize + 1u;              /* Add 1 entry for echo buf.                            */
    p_ctrl->TxBufQ.Tbl  = (USBD_CDC_EEM_BUF_ENTRY *)Mem_HeapAlloc(sizeof(USBD_CDC_EEM_BUF_ENTRY) * p_ctrl->TxBufQ.Size,
                                                                  sizeof(CPU_ALIGN),
                                                                  DEF_NULL,
                                                                 &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

    p_ctrl->RxBufQ.Size =  p_cfg->RxBufQSize;
    p_ctrl->RxBufQ.Tbl  = (USBD_CDC_EEM_BUF_ENTRY *)Mem_HeapAlloc(sizeof(USBD_CDC_EEM_BUF_ENTRY) * p_ctrl->RxBufQ.Size,
                                                                  sizeof(CPU_ALIGN),
                                                                  DEF_NULL,
                                                                 &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        USBD_CDC_EEM_Start()
*
* Description : Starts communication on given CDC-EEM class instance.
*
* Argument(s) : class_nbr       Class instance number.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Operation was successful.
*                               USBD_ERR_INVALID_ARG        Invalid argument(s) passed to 'class_nbr'.
*
*                               -RETURNED BY USBD_CDC_EEM_StateLock()-
*                               See USBD_CDC_EEM_StateLock() for additional return error codes.
*
*                               -RETURNED BY USBD_CDC_EEM_CommStart()-
*                               See USBD_CDC_EEM_CommStart() for additional return error codes.
*
* Return(s)   : None.
*
* Note(s)     : (1) This function will start communication only if class instance is connected.
*********************************************************************************************************
*/

void  USBD_CDC_EEM_Start (CPU_INT08U   class_nbr,
                          USBD_ERR    *p_err)
{
    USBD_CDC_EEM_CTRL  *p_ctrl;
    USBD_ERR            err_unlock;



                                                                /* ------------------- VALIDATE ARG ------------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    {
        CPU_SR_ALLOC();


        if (p_err == DEF_NULL) {
            CPU_SW_EXCEPTION(;);
        }

        CPU_CRITICAL_ENTER();
        if (class_nbr >= USBD_CDC_EEM_CtrlNbrNext) {
            CPU_CRITICAL_EXIT();
           *p_err = USBD_ERR_INVALID_ARG;

            return;
        }
        CPU_CRITICAL_EXIT();
    }
#endif

    p_ctrl = &USBD_CDC_EEM_CtrlTbl[class_nbr];

    USBD_CDC_EEM_StateLock(p_ctrl, p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    p_ctrl->StartCnt++;

    if ((p_ctrl->StartCnt == 1u) &&                             /* If net drv started and class connected, start comm.  */
        (p_ctrl->State    == USBD_CDC_EEM_STATE_CFG)) {

        USBD_CDC_EEM_CommStart(p_ctrl, p_err);
    } else {
       *p_err = USBD_ERR_NONE;
    }

    USBD_CDC_EEM_StateUnlock(p_ctrl, &err_unlock);
    (void)err_unlock;
}


/*
*********************************************************************************************************
*                                        USBD_CDC_EEM_Stop()
*
* Description : Stops communication on given CDC-EEM class instance.
*
* Argument(s) : class_nbr       Class instance number.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Operation was successful.
*                               USBD_ERR_INVALID_ARG        Invalid argument(s) passed to 'class_nbr'.
*
*                               -RETURNED BY USBD_CDC_EEM_StateLock()-
*                               See USBD_CDC_EEM_StateLock() for additional return error codes.
*
*                               -RETURNED BY USBD_CDC_EEM_StateUnlock()-
*                               See USBD_CDC_EEM_StateUnlock() for additional return error codes.
*
* Return(s)   : None.
*
* Note(s)     : (1) This function will stop communication only if class instance is connected.
*********************************************************************************************************
*/

void  USBD_CDC_EEM_Stop (CPU_INT08U   class_nbr,
                         USBD_ERR    *p_err)
{
    USBD_CDC_EEM_CTRL  *p_ctrl;


                                                                /* ------------------- VALIDATE ARG ------------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    {
        CPU_SR_ALLOC();


        if (p_err == DEF_NULL) {
            CPU_SW_EXCEPTION(;);
        }

        CPU_CRITICAL_ENTER();
        if (class_nbr >= USBD_CDC_EEM_CtrlNbrNext) {
            CPU_CRITICAL_EXIT();
           *p_err = USBD_ERR_INVALID_ARG;

            return;
        }
        CPU_CRITICAL_EXIT();
    }
#endif

    p_ctrl = &USBD_CDC_EEM_CtrlTbl[class_nbr];

    USBD_CDC_EEM_StateLock(p_ctrl, p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    if (p_ctrl->StartCnt > 0u) {
        p_ctrl->StartCnt--;
    }

    if ((p_ctrl->StartCnt == 0u) &&                             /* If class conn still connected, stop comm.            */
        (p_ctrl->State    == USBD_CDC_EEM_STATE_CFG)) {
        USBD_ERR  err_abort;


        USBD_EP_Abort(p_ctrl->DevNbr,
                      p_ctrl->CommPtr->DataInEpAddr,
                     &err_abort);

        USBD_EP_Abort(p_ctrl->DevNbr,
                      p_ctrl->CommPtr->DataOutEpAddr,
                     &err_abort);

        (void)err_abort;
    }

    USBD_CDC_EEM_StateUnlock(p_ctrl, p_err);
}


/*
*********************************************************************************************************
*                                      USBD_CDC_EEM_DevNbrGet()
*
* Description : Gets device number associated to this CDC-EEM class instance.
*
* Argument(s) : class_nbr       Class instance number.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Operation was successful.
*                               USBD_ERR_INVALID_ARG        Invalid argument(s) passed to 'class_nbr'.
*
* Return(s)   : Device number.
*
* Note(s)     : None.
*********************************************************************************************************
*/

CPU_INT08U  USBD_CDC_EEM_DevNbrGet (CPU_INT08U   class_nbr,
                                    USBD_ERR    *p_err)
{
    USBD_CDC_EEM_CTRL  *p_ctrl;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    {
        CPU_SR_ALLOC();


        if (p_err == DEF_NULL) {                               /* Validate error ptr.                                  */
            CPU_SW_EXCEPTION(;);
        }

        CPU_CRITICAL_ENTER();
        if (class_nbr >= USBD_CDC_EEM_CtrlNbrNext) {
            CPU_CRITICAL_EXIT();
           *p_err = USBD_ERR_INVALID_ARG;

            return (USBD_DEV_NBR_NONE);
        }
        CPU_CRITICAL_EXIT();
    }
#endif

    p_ctrl = &USBD_CDC_EEM_CtrlTbl[class_nbr];
   *p_err  =  USBD_ERR_NONE;

    return (p_ctrl->DevNbr);
}


/*
*********************************************************************************************************
*                                     USBD_CDC_EEM_RxDataPktGet()
*
* Description : Gets first received data packet from received queue.
*
* Argument(s) : class_nbr           Class instance number.
*
*               p_rx_len            Pointer to variable that will receive the length of the received
*                                   buffer in bytes.
*
*               p_crc_computed      Pointer to variable that will receive the status of the ethernet CRC
*                                   in buffer. Can be DEF_NULL.
*
*                                   DEF_YES     CRC is computed.
*                                   DEF_NO      CRC is not computed. Last 4 bytes of buffer are set to
*                                               0xDEADBEEF.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Operation was successful.
*                               USBD_ERR_INVALID_ARG        Invalid argument(s) passed to 'class_nbr'.
*                               USBD_ERR_NULL_PTR           Invalid null pointer passed to 'p_rx_len'.
*                               USBD_ERR_RX                 No rx buffer available.
*
* Return(s)   : Pointer to received buffer, if successful.
*
*               DEF_NULL,                   otherwise.
*
* Note(s)     : None.
*********************************************************************************************************
*/

CPU_INT08U  *USBD_CDC_EEM_RxDataPktGet (CPU_INT08U    class_nbr,
                                        CPU_INT16U   *p_rx_len,
                                        CPU_BOOLEAN  *p_crc_computed,
                                        USBD_ERR     *p_err)
{
    CPU_INT08U         *p_buf;
    USBD_CDC_EEM_CTRL  *p_ctrl;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == DEF_NULL) {
        CPU_SW_EXCEPTION(;);
    }

    CPU_CRITICAL_ENTER();
    if (class_nbr >= USBD_CDC_EEM_CtrlNbrNext) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_DEV_INVALID_NBR;

        return (DEF_NULL);
    }
    CPU_CRITICAL_EXIT();

    if (p_rx_len == DEF_NULL) {
       *p_err = USBD_ERR_NULL_PTR;
        return (DEF_NULL);
    }
#endif

    p_ctrl = &USBD_CDC_EEM_CtrlTbl[class_nbr];

    CPU_CRITICAL_ENTER();                                       /* Get next buffer if any.                              */
    p_buf = USBD_CDC_EEM_BufQ_Get(&p_ctrl->RxBufQ,
                                   p_rx_len,
                                   p_crc_computed);
    CPU_CRITICAL_EXIT();

   *p_err = (p_buf != DEF_NULL) ? USBD_ERR_NONE : USBD_ERR_RX;

    return (p_buf);
}


/*
*********************************************************************************************************
*                                     USBD_CDC_EEM_TxDataPktSubmit()
*
* Description : Submits a Tx buffer.
*
* Argument(s) : class_nbr       Class instance number.
*
*               p_buf           Pointer to buffer to submit.
*
*               buf_len         Buffer length in bytes.
*
*               crc_computed    Flag that indicates if submitted buffer contains a computed ethernet CRC.
*
*                                   DEF_YES     Buffer contains a valid ethernet CRC in its last 4 bytes.
*                                   DEF_NO      Buffer does not contain a valid ethernet CRC.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Operation was successful.
*                               USBD_ERR_INVALID_ARG        Invalid argument(s) passed to 'class_nbr'.
*                               USBD_ERR_NULL_PTR           Invalid null pointer passed to 'p_buf'.
*
*                                   -RETURNED BY USBD_CDC_EEM_TxBufSubmit()-
*                                   See USBD_CDC_EEM_TxBufSubmit() for additional return error codes.
*
* Return(s)   : None.
*
* Note(s)     : (1) Buffers submitted using this function MUST have a padding of 2 bytes at the
*                   beginning.
*********************************************************************************************************
*/

void  USBD_CDC_EEM_TxDataPktSubmit (CPU_INT08U    class_nbr,
                                    CPU_INT08U   *p_buf,
                                    CPU_INT32U    buf_len,
                                    CPU_BOOLEAN   crc_computed,
                                    USBD_ERR     *p_err)
{
    CPU_INT16U          hdr;
    USBD_CDC_EEM_CTRL  *p_ctrl;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    {
        CPU_SR_ALLOC();


        if (p_err == DEF_NULL) {
            CPU_SW_EXCEPTION(;);
        }

        CPU_CRITICAL_ENTER();
        if (class_nbr >= USBD_CDC_EEM_CtrlNbrNext) {
            CPU_CRITICAL_EXIT();

           *p_err = USBD_ERR_INVALID_ARG;
            return;
        }
        CPU_CRITICAL_EXIT();

        if ((p_buf   == DEF_NULL) &&
            (buf_len != 0u)) {
           *p_err = USBD_ERR_NULL_PTR;
            return;
        }
    }
#endif

    p_ctrl = &USBD_CDC_EEM_CtrlTbl[class_nbr];

                                                                /* Prepare header.                                      */
    hdr = DEF_BIT_FIELD_ENC(USBD_CDC_EEM_PKT_TYPE_PAYLOAD, USBD_CDC_EEM_PKT_TYPE_MASK);

    if (crc_computed == DEF_NO) {
                                                                /* Set CRC to 0xDEADBEEF as per CDC-EEM specification.  */
        Mem_Copy((void *)&p_buf[buf_len + USBD_CDC_EEM_HDR_LEN],
                 (void *) USBD_CDC_EEM_PayloadCRC,
                          4u);

        hdr |= DEF_BIT_FIELD_ENC(USBD_CDC_EEM_PAYLOAD_CRC_NOT_CALC, USBD_CDC_EEM_PAYLOAD_CRC_MASK) |
               DEF_BIT_FIELD_ENC(buf_len + 4u,                      USBD_CDC_EEM_PAYLOAD_LEN_MASK);
    } else {
        hdr |= DEF_BIT_FIELD_ENC(USBD_CDC_EEM_PAYLOAD_CRC_CALC, USBD_CDC_EEM_PAYLOAD_CRC_MASK) |
               DEF_BIT_FIELD_ENC(buf_len,                       USBD_CDC_EEM_PAYLOAD_LEN_MASK);
    }

   ((CPU_INT16U *)p_buf)[0u] = MEM_VAL_GET_INT16U_LITTLE(&hdr); /* Copy hdr to buffer.                                  */

    USBD_CDC_EEM_TxBufSubmit(p_ctrl,                            /* Send buffer to host.                                 */
                             p_buf,
                             buf_len + USBD_CDC_EEM_HDR_LEN + 4u,
                             p_err);
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
*                                         USBD_CDC_EEM_Conn()
*
* Description : Notify class that configuration is active.
*
* Argument(s) : dev_nbr         Device number.
*
*               cfg_nbr         Configuration index to add the interface to.
*
*               p_if_class_arg  Pointer to class argument.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_CDC_EEM_Conn (CPU_INT08U   dev_nbr,
                                 CPU_INT08U   cfg_nbr,
                                 void        *p_if_class_arg)
{
    USBD_CDC_EEM_COMM  *p_comm = (USBD_CDC_EEM_COMM *)p_if_class_arg;
    USBD_ERR            err;


    (void)dev_nbr;
    (void)cfg_nbr;

    USBD_CDC_EEM_StateLock(p_comm->CtrlPtr, &err);

    p_comm->CtrlPtr->State   = USBD_CDC_EEM_STATE_CFG;
    p_comm->CtrlPtr->CommPtr = p_comm;

    if (p_comm->CtrlPtr->StartCnt > 0u) {                       /* If class instance is started by net drv, start comm. */
        USBD_CDC_EEM_CommStart(p_comm->CtrlPtr, &err);
    }

    USBD_CDC_EEM_StateUnlock(p_comm->CtrlPtr, &err);

    (void)err;
}


/*
*********************************************************************************************************
*                                        USBD_CDC_EEM_Disconn()
*
* Description : Notify class that configuration is not active.
*
* Argument(s) : dev_nbr         Device number.
*
*               cfg_nbr         Configuration index to add the interface.
*
*               p_if_class_arg  Pointer to class argument.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_CDC_EEM_Disconn (CPU_INT08U   dev_nbr,
                                    CPU_INT08U   cfg_nbr,
                                    void        *p_if_class_arg)
{
    USBD_CDC_EEM_COMM  *p_comm = (USBD_CDC_EEM_COMM *)p_if_class_arg;
    USBD_ERR            err_lock;


    (void)dev_nbr;
    (void)cfg_nbr;

    USBD_CDC_EEM_StateLock(p_comm->CtrlPtr, &err_lock);
    p_comm->CtrlPtr->State   = USBD_CDC_EEM_STATE_INIT;
    p_comm->CtrlPtr->CommPtr = DEF_NULL;
    USBD_CDC_EEM_StateUnlock(p_comm->CtrlPtr, &err_lock);

    (void)err_lock;
}


/*
*********************************************************************************************************
*                                      USBD_CDC_EEM_CommStart()
*
* Description : Starts communication on given class instance.
*
* Argument(s) : p_ctrl      Pointer to class instance control structure.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Operation was successful.
*
*                           -RETURNED BY USBD_BulkRxAsync()-
*                           See USBD_BulkRxAsync() for additional return error codes.
*
* Return(s)   : None.
*
* Note(s)     : (1) State of CDC-EEM must be locked by caller function.
*********************************************************************************************************
*/

static  void  USBD_CDC_EEM_CommStart (USBD_CDC_EEM_CTRL  *p_ctrl,
                                      USBD_ERR           *p_err)
{
    CPU_INT08U          buf_cnt;
    USBD_CDC_EEM_COMM  *p_comm;


    p_comm = p_ctrl->CommPtr;

                                                                /* Init Rx state machine.                               */
    p_ctrl->CurBufLenRem      = 0u;
    p_ctrl->CurBufPtr         = DEF_NULL;
    p_ctrl->CurBufIx          = 0u;
    p_ctrl->CurBufCrcComputed = DEF_NO;
    p_ctrl->RxErrCnt          = 0u;

                                                                /* Init Tx and Rx Qs.                                   */
    p_ctrl->TxBufQ.InIdx  = 0u;
    p_ctrl->TxBufQ.OutIdx = 0u;
    p_ctrl->TxBufQ.Cnt    = 0u;
    p_ctrl->TxInProgress  = DEF_NO;

    p_ctrl->RxBufQ.InIdx  = 0u;
    p_ctrl->RxBufQ.OutIdx = 0u;
    p_ctrl->RxBufQ.Cnt    = 0u;

                                                                /* Submit all avail Rx buffers.                         */
    for (buf_cnt = 0u; buf_cnt < USBD_CDC_EEM_CFG_RX_BUF_QTY_PER_DEV; buf_cnt++) {
        USBD_BulkRxAsync(        p_ctrl->DevNbr,
                                 p_comm->DataOutEpAddr,
                                 p_ctrl->RxBufPtrTbl[buf_cnt],
                                 USBD_CDC_EEM_CFG_RX_BUF_LEN,
                                 USBD_CDC_EEM_RxCmpl,
                         (void *)p_ctrl,
                                 p_err);
        if (*p_err != USBD_ERR_NONE) {
            break;
        }
    }
}


/*
*********************************************************************************************************
*                                      USBD_CDC_EEM_RxCmpl()
*
* Description : Inform the application about the Bulk OUT transfer completion.
*
* Argument(s) : dev_nbr          Device number
*
*               ep_addr          Endpoint address.
*
*               p_buf            Pointer to the receive buffer.
*
*               buf_len          Receive buffer length.
*
*               xfer_len         Number of octets received.
*
*               p_arg            Additional argument provided by application.
*
*               err              Transfer status: success or error.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  USBD_CDC_EEM_RxCmpl (CPU_INT08U   dev_nbr,
                                   CPU_INT08U   ep_addr,
                                   void        *p_buf,
                                   CPU_INT32U   buf_len,
                                   CPU_INT32U   xfer_len,
                                   void        *p_arg,
                                   USBD_ERR     err)
{
    CPU_INT16U          buf_ix_cur =  0u;
    USBD_CDC_EEM_CTRL  *p_ctrl     = (USBD_CDC_EEM_CTRL *)p_arg;
    USBD_ERR            err_usbd;
    CPU_SR_ALLOC();


    (void)dev_nbr;
    (void)ep_addr;
    (void)buf_len;

    switch (err) {                                              /* Chk errors.                                          */
        case USBD_ERR_NONE:
             p_ctrl->RxErrCnt = 0u;
             break;


        case USBD_ERR_EP_ABORT:
             return;


        default:
             p_ctrl->RxErrCnt++;                                /* Retry a few times.                                   */
             if (p_ctrl->RxErrCnt > USBD_CDC_EEM_MAX_RETRY_CNT) {
                 return;
             }

             goto resubmit;
    }

    if (p_ctrl->CurBufLenRem > 0u) {                            /* If previous received buffer was incomplete.          */
        CPU_INT16U  len_to_copy = DEF_MIN(xfer_len, p_ctrl->CurBufLenRem);


        if (p_ctrl->CurBufPtr != DEF_NULL) {
            Mem_Copy((void *)&p_ctrl->CurBufPtr[p_ctrl->CurBufIx],
                              p_buf,
                              len_to_copy);
        }

        p_ctrl->CurBufIx     += len_to_copy;
        p_ctrl->CurBufLenRem -= len_to_copy;
        buf_ix_cur           += len_to_copy;

        if (p_ctrl->CurBufLenRem != 0u) {
            goto resubmit;
        }
    }

    do {
        CPU_INT08U  cmd;
        CPU_INT16U  cmd_param;


        if (p_ctrl->CurBufIx == 0u) {                           /* Get hdr only if not in data copy phase.              */
            p_ctrl->CurHdr = MEM_VAL_GET_INT16U_LITTLE(&((CPU_INT08U *)p_buf)[buf_ix_cur]);
        }

                                                                /* Ensure not a Zero-Length EEM (ZLE) pkt.              */
        if (p_ctrl->CurHdr == USBD_CDC_EEM_HDR_ZLE) {
            buf_ix_cur += USBD_CDC_EEM_HDR_LEN;
            continue;
        }

        switch (DEF_BIT_FIELD_RD(p_ctrl->CurHdr, USBD_CDC_EEM_PKT_TYPE_MASK)) {
            case USBD_CDC_EEM_PKT_TYPE_CMD:                     /* --------------------- EEM CMD ---------------------- */
                                                                /* Ensure rsvd bit is 0.                                */
                 if (DEF_BIT_FIELD_RD(p_ctrl->CurHdr, USBD_CDC_EEM_CMD_RSVD_MASK) != USBD_CDC_EEM_CMD_RSVD_OK) {
                     buf_ix_cur += USBD_CDC_EEM_HDR_LEN;
                     continue;
                 }

                 cmd       = DEF_BIT_FIELD_RD(p_ctrl->CurHdr, USBD_CDC_EEM_CMD_CODE_MASK);
                 cmd_param = DEF_BIT_FIELD_RD(p_ctrl->CurHdr, USBD_CDC_EEM_CMD_PARAM_MASK);

                 switch (cmd) {
                     case USBD_CDC_EEM_CMD_CODE_ECHO:           /* --------------------- ECHO CMD --------------------- */
                          if (p_ctrl->CurBufIx == 0u) {
                              CPU_INT16U  len_to_copy;


                              buf_ix_cur       += USBD_CDC_EEM_HDR_LEN;
                              p_ctrl->CurBufIx += USBD_CDC_EEM_HDR_LEN;

                                                                /* Copy echo buffer.                                    */
                              p_ctrl->CurBufPtr    = p_ctrl->BufEchoPtr;
                              p_ctrl->CurBufLenRem = DEF_MIN(DEF_BIT_FIELD_RD(cmd_param, USBD_CDC_EEM_CMD_PARAM_ECHO_LEN_MASK),
                                                             USBD_CDC_EEM_CFG_ECHO_BUF_LEN - USBD_CDC_EEM_HDR_LEN);

                              len_to_copy = DEF_MIN(p_ctrl->CurBufLenRem, (xfer_len - buf_ix_cur));
                              Mem_Copy((void *)&p_ctrl->CurBufPtr[p_ctrl->CurBufIx],
                                               &((CPU_INT08U *)p_buf)[buf_ix_cur],
                                                len_to_copy);
                              p_ctrl->CurBufIx     += len_to_copy;
                              p_ctrl->CurBufLenRem -= len_to_copy;

                              buf_ix_cur += len_to_copy;
                          }

                          if (p_ctrl->CurBufLenRem == 0u) {     /* If buf copy done, prepare echo resp reply.           */
                              CPU_INT16U  *p_hdr = &((CPU_INT16U *)p_ctrl->CurBufPtr)[0u];


                                                                /* Prepare hdr.                                         */
                             *p_hdr = DEF_BIT_FIELD_ENC(USBD_CDC_EEM_PKT_TYPE_CMD,               USBD_CDC_EEM_PKT_TYPE_MASK) |
                                      DEF_BIT_FIELD_ENC(USBD_CDC_EEM_CMD_CODE_ECHO_RESP,         USBD_CDC_EEM_CMD_CODE_MASK) |
                                      DEF_BIT_FIELD_ENC(p_ctrl->CurBufIx - USBD_CDC_EEM_HDR_LEN, USBD_CDC_EEM_CMD_PARAM_ECHO_LEN_MASK);

                              USBD_CDC_EEM_TxBufSubmit(p_ctrl,
                                                       p_ctrl->CurBufPtr,
                                                       p_ctrl->CurBufIx,
                                                      &err_usbd);

                              p_ctrl->CurBufIx = 0u;
                          }
                          break;


                     case USBD_CDC_EEM_CMD_CODE_ECHO_RESP:      /* ----------------- UNSUPPORTED CMDS ----------------- */
                     case USBD_CDC_EEM_CMD_CODE_TICKLE:
                     case USBD_CDC_EEM_CMD_CODE_SUSP_HINT:
                     case USBD_CDC_EEM_CMD_CODE_RESP_HINT:
                     case USBD_CDC_EEM_CMD_CODE_RESP_CMPL_HINT:
                     default:
                          buf_ix_cur+= USBD_CDC_EEM_HDR_LEN;
                          break;
                 }
                 break;


            case USBD_CDC_EEM_PKT_TYPE_PAYLOAD:                 /* ------------------- EEM PAYLOADS ------------------- */
                 if (p_ctrl->CurBufIx == 0u) {
                     CPU_INT16U  net_buf_len;
                     CPU_INT16U  len_to_copy;


                     buf_ix_cur       += USBD_CDC_EEM_HDR_LEN;
                     p_ctrl->CurBufPtr = p_ctrl->DrvPtr->RxBufGet(p_ctrl->ClassNbr,
                                                                  p_ctrl->DrvArgPtr,
                                                                 &net_buf_len);

                     p_ctrl->CurBufLenRem = DEF_BIT_FIELD_RD(p_ctrl->CurHdr, USBD_CDC_EEM_PAYLOAD_LEN_MASK);

                     if (DEF_BIT_FIELD_RD(p_ctrl->CurHdr, USBD_CDC_EEM_PAYLOAD_CRC_MASK) == USBD_CDC_EEM_PAYLOAD_CRC_CALC) {
                         p_ctrl->CurBufCrcComputed = DEF_YES;
                     } else {
                         p_ctrl->CurBufCrcComputed = DEF_NO;
                     }

                                                                /* Copy payload content to network drv's buf.           */
                     len_to_copy = DEF_MIN(p_ctrl->CurBufLenRem, (xfer_len - buf_ix_cur));
                     if (p_ctrl->CurBufPtr != DEF_NULL) {
                         Mem_Copy((void *)&p_ctrl->CurBufPtr[p_ctrl->CurBufIx],
                                          &((CPU_INT08U *)p_buf)[buf_ix_cur],
                                           len_to_copy);
                     }
                     p_ctrl->CurBufIx     += len_to_copy;
                     p_ctrl->CurBufLenRem -= len_to_copy;

                     buf_ix_cur += len_to_copy;
                 }

                 if (p_ctrl->CurBufLenRem == 0u) {              /* If reception complete, submit buf to network drv.    */
                     if (p_ctrl->CurBufPtr != DEF_NULL) {
                         CPU_CRITICAL_ENTER();
                         USBD_CDC_EEM_BufQ_Add(&p_ctrl->RxBufQ,
                                                p_ctrl->CurBufPtr,
                                                p_ctrl->CurBufIx,
                                                p_ctrl->CurBufCrcComputed);
                         CPU_CRITICAL_EXIT();

                         p_ctrl->DrvPtr->RxBufRdy(p_ctrl->ClassNbr,
                                                  p_ctrl->DrvArgPtr);
                     }

                     p_ctrl->CurBufIx = 0u;
                 }
                 break;


            default:
                 break;
        }
    } while (buf_ix_cur <= (xfer_len - USBD_CDC_EEM_HDR_LEN));


resubmit:
    USBD_CDC_EEM_StateLock(p_ctrl, &err_usbd);

    if ((p_ctrl->StartCnt >  0u) &&                             /* Re-submit buf if still connected.                    */
        (p_ctrl->State    == USBD_CDC_EEM_STATE_CFG)) {
        USBD_BulkRxAsync(        p_ctrl->DevNbr,
                                 p_ctrl->CommPtr->DataOutEpAddr,
                                 p_buf,
                                 USBD_CDC_EEM_CFG_RX_BUF_LEN,
                                 USBD_CDC_EEM_RxCmpl,
                         (void *)p_ctrl,
                                &err_usbd);
    }

    USBD_CDC_EEM_StateUnlock(p_ctrl, &err_usbd);

    (void)err_usbd;
}


/*
*********************************************************************************************************
*                                      USBD_CDC_EEM_TxCmpl()
*
* Description : Inform the application about the Bulk IN transfer completion.
*
* Argument(s) : dev_nbr          Device number
*
*               ep_addr          Endpoint address.
*
*               p_buf            Pointer to the receive buffer.
*
*               buf_len          Receive buffer length.
*
*               xfer_len         Number of octets sent.
*
*               p_arg            Additional argument provided by application.
*
*               err              Transfer status: success or error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_CDC_EEM_TxCmpl (CPU_INT08U   dev_nbr,
                                   CPU_INT08U   ep_addr,
                                   void        *p_buf,
                                   CPU_INT32U   buf_len,
                                   CPU_INT32U   xfer_len,
                                   void        *p_arg,
                                   USBD_ERR     err)
{
    CPU_INT08U         *p_next_buf;
    CPU_INT16U          next_buf_len;
    USBD_CDC_EEM_CTRL  *p_ctrl = (USBD_CDC_EEM_CTRL *)p_arg;
    USBD_ERR            err_lock;
    CPU_SR_ALLOC();


    (void)dev_nbr;
    (void)ep_addr;
    (void)xfer_len;
    (void)err;

    if ((p_buf != p_ctrl->BufEchoPtr) &&
        (p_buf != DEF_NULL)) {                                  /* Free Tx buf to network drv.                          */
        p_ctrl->DrvPtr->TxBufFree(              p_ctrl->ClassNbr,
                                                p_ctrl->DrvArgPtr,
                                  (CPU_INT08U *)p_buf,
                                                buf_len);
    }

    USBD_CDC_EEM_StateLock(p_ctrl, &err_lock);

    if ((p_ctrl->State    != USBD_CDC_EEM_STATE_CFG) ||
        (p_ctrl->StartCnt == 0u)) {

        USBD_CDC_EEM_StateUnlock(p_ctrl, &err_lock);
        return;
    }

    CPU_CRITICAL_ENTER();                                       /* Submit next buffer if any.                           */
    p_next_buf = USBD_CDC_EEM_BufQ_Get(&p_ctrl->TxBufQ,
                                       &next_buf_len,
                                        DEF_NULL);
    if (p_next_buf == DEF_NULL) {
        p_ctrl->TxInProgress = DEF_NO;
    }
    CPU_CRITICAL_EXIT();

    if (p_next_buf != DEF_NULL) {
        USBD_ERR  err_submit;


        USBD_BulkTxAsync(        p_ctrl->DevNbr,
                                 p_ctrl->CommPtr->DataInEpAddr,
                         (void *)p_next_buf,
                                 next_buf_len,
                                 USBD_CDC_EEM_TxCmpl,
                         (void *)p_ctrl,
                                 DEF_YES,
                                &err_submit);

        (void)err_submit;
    }

    USBD_CDC_EEM_StateUnlock(p_ctrl, &err_lock);
    (void)err_lock;
}


/*
*********************************************************************************************************
*                                     USBD_CDC_EEM_TxBufSubmit()
*
* Description : Submits a transmit buffer.
*
* Argument(s) : p_ctrl      Pointer to class instance control structure.
*
*               p_buf       Pointer to buffer that contains data to send.
*
*               buf_len     Length of buffer in bytes.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Operation was successful.
*
*                               -RETURNED BY USBD_CDC_EEM_StateLock()-
*                               See USBD_CDC_EEM_StateLock() for additional return error codes.
*
*                               -RETURNED BY USBD_BulkTxAsync()-
*                               See USBD_BulkTxAsync() for additional return error codes.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  USBD_CDC_EEM_TxBufSubmit (USBD_CDC_EEM_CTRL  *p_ctrl,
                                        CPU_INT08U         *p_buf,
                                        CPU_INT16U          buf_len,
                                        USBD_ERR           *p_err)
{
    CPU_BOOLEAN  tx_in_progress;
    USBD_ERR     err_unlock;
    CPU_SR_ALLOC();


    USBD_CDC_EEM_StateLock(p_ctrl, p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    if ((p_ctrl->State    != USBD_CDC_EEM_STATE_CFG) ||
        (p_ctrl->StartCnt == 0u)) {
        USBD_CDC_EEM_StateUnlock(p_ctrl, &err_unlock);

       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return;
    }


    CPU_CRITICAL_ENTER();
    tx_in_progress = p_ctrl->TxInProgress;
    if (tx_in_progress == DEF_NO) {
        p_ctrl->TxInProgress = DEF_YES;                         /* Submit buffer if no buffer in Q.                     */
    } else {
        USBD_CDC_EEM_BufQ_Add(&p_ctrl->TxBufQ,                  /* Add buffer to Q.                                     */
                               p_buf,
                               buf_len,
                               DEF_NO);
    }
    CPU_CRITICAL_EXIT();

    if (tx_in_progress == DEF_NO) {
        USBD_BulkTxAsync(        p_ctrl->DevNbr,
                                 p_ctrl->CommPtr->DataInEpAddr,
                         (void *)p_buf,
                                 buf_len,
                                 USBD_CDC_EEM_TxCmpl,
                         (void *)p_ctrl,
                                 DEF_YES,
                                 p_err);
    }

    USBD_CDC_EEM_StateUnlock(p_ctrl, &err_unlock);
    (void)err_unlock;
}


/*
*********************************************************************************************************
*                                       USBD_CDC_EEM_BufQ_Add()
*
* Description : Adds a buffer to the tail of the Q.
*
* Argument(s) : p_buf_q         Pointer to buffer Q.
*
*               p_buf           Pointer to buffer to add to Q.
*
*               buf_len         Length of buffer in bytes.
*
*               crc_computed    Flag that indicates if ethernet CRC is computed in submitted buffer.
*
* Return(s)   : None.
*
* Note(s)     : (1) This function must be called from within a critical section.
*********************************************************************************************************
*/

static  void  USBD_CDC_EEM_BufQ_Add (USBD_CDC_EEM_BUF_Q  *p_buf_q,
                                     CPU_INT08U          *p_buf,
                                     CPU_INT16U           buf_len,
                                     CPU_BOOLEAN          crc_computed)
{
    if (p_buf_q->Cnt >= p_buf_q->Size) {
        return;
    }

    p_buf_q->Tbl[p_buf_q->InIdx].BufPtr      = p_buf;
    p_buf_q->Tbl[p_buf_q->InIdx].BufLen      = buf_len;
    p_buf_q->Tbl[p_buf_q->InIdx].CrcComputed = crc_computed;

    p_buf_q->InIdx++;
    if (p_buf_q->InIdx >= p_buf_q->Size) {
        p_buf_q->InIdx = 0u;
    }

    p_buf_q->Cnt++;
}


/*
*********************************************************************************************************
*                                       USBD_CDC_EEM_BufQ_Get()
*
* Description : Gets a buffer from the head of the Q.
*
* Argument(s) : p_buf_q             Pointer to buffer Q.
*
*               p_buf_len           Pointer to variable that will receive length of buffer in bytes.
*
*               p_crc_computed      Pointer to variable that will receive the CRC computes state of the
*                                   buffer. Can be DEF_NULL if ignored.
*
*                                   DEF_YES     Buffer contains a valid/computed ethernet CRC.
*                                   DEF_NO      Buffer does not contains a valid/computed ethernet CRC.
*
* Return(s)   : Pointer to buffer.
*
* Note(s)     : (1) This function must be called from within a critical section.
*********************************************************************************************************
*/

static  CPU_INT08U  *USBD_CDC_EEM_BufQ_Get (USBD_CDC_EEM_BUF_Q  *p_buf_q,
                                            CPU_INT16U          *p_buf_len,
                                            CPU_BOOLEAN         *p_crc_computed)
{
    CPU_INT08U  *p_buf;


    if (p_buf_q->Cnt == 0u) {
        return (DEF_NULL);
    }

    p_buf     = p_buf_q->Tbl[p_buf_q->OutIdx].BufPtr;
   *p_buf_len = p_buf_q->Tbl[p_buf_q->OutIdx].BufLen;

    if (p_crc_computed != DEF_NULL) {
       *p_crc_computed = p_buf_q->Tbl[p_buf_q->OutIdx].CrcComputed;
    }

    p_buf_q->OutIdx++;
    if (p_buf_q->OutIdx >= p_buf_q->Size) {
        p_buf_q->OutIdx = 0u;
    }

    p_buf_q->Cnt--;

    return (p_buf);
}


/*
*********************************************************************************************************
*                                      USBD_CDC_EEM_StateLock()
*
* Description : Locks CDC-EEM class instance state.
*
* Argument(s) : p_ctrl      Pointer to class instance control structure.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Operation was successful.
*                               USBD_ERR_OS_FAIL            Lock failed.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  USBD_CDC_EEM_StateLock (USBD_CDC_EEM_CTRL  *p_ctrl,
                                      USBD_ERR           *p_err)
{
    KAL_ERR  err_kal;


    KAL_LockAcquire(p_ctrl->StateLockHandle,
                    KAL_OPT_PEND_NONE,
                    0u,
                   &err_kal);
   *p_err = (err_kal == KAL_ERR_NONE) ? USBD_ERR_NONE : USBD_ERR_OS_FAIL;
}


/*
*********************************************************************************************************
*                                     USBD_CDC_EEM_StateUnlock()
*
* Description : Unlocks CDC-EEM class instance state.
*
* Argument(s) : p_ctrl      Pointer to class instance control structure.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Operation was successful.
*                               USBD_ERR_OS_FAIL            Unlock failed.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  USBD_CDC_EEM_StateUnlock (USBD_CDC_EEM_CTRL  *p_ctrl,
                                        USBD_ERR           *p_err)
{
    KAL_ERR  err_kal;


    KAL_LockRelease(p_ctrl->StateLockHandle,
                   &err_kal);
   *p_err = (err_kal == KAL_ERR_NONE) ? USBD_ERR_NONE : USBD_ERR_OS_FAIL;
}
