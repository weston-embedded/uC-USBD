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
*                                   USB COMMUNICATIONS DEVICE CLASS (CDC)
*                                        ABSTRACT CONTROL MODEL (ACM)
*                                             SERIAL EMULATION
*
* Filename : usbd_acm_serial.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)       : (1) This implementation is compliant with the PSTN subclass specification revision 1.2
*                     February 9, 2007.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#include  "usbd_acm_serial.h"


/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  USBD_ACM_CTRL_REQ_TIMEOUT_mS                  5000u


/*
*********************************************************************************************************
*                              ACM FUNCTIONAL DESCRIPTORS SIZES DEFINES
*
* Note(s) : (1) Table 3 and 4 from the PSTN specification version 1.2 defines the Call management
*               and ACM management functional descriptors.
*********************************************************************************************************
*/

#define  USBD_ACM_DESC_CALL_MGMT_SIZE                     5u
#define  USBD_ACM_DESC_SIZE                               4u
#define  USBD_ACM_DESC_TOT_SIZE                (USBD_ACM_DESC_CALL_MGMT_SIZE + \
                                                USBD_ACM_DESC_SIZE)


/*
*********************************************************************************************************
*                                  ACM SERIAL NOTIFICATIONS DEFINES
*********************************************************************************************************
*/

#define  USBD_ACM_SERIAL_REQ_STATE                     0x20u    /* Serial state notification code.                      */
#define  USBD_ACM_SERIAL_REQ_STATE_SIZE                   2u    /* Serial state notification data size.                 */

#define  USBD_ACM_SERIAL_STATE_BUF_SIZE        (USBD_CDC_NOTIFICATION_HEADER + \
                                                USBD_ACM_SERIAL_REQ_STATE_SIZE)


/*
*********************************************************************************************************
*                                   ABSTRACT STATE FEATURE DEFINES
*********************************************************************************************************
*/

#define  USBD_ACM_SERIAL_ABSTRACT_DATA_MUX      DEF_BIT_01
#define  USBD_ACM_SERIAL_ABSTRACT_IDLE          DEF_BIT_00


/*
*********************************************************************************************************
*                                      LINE STATE SIGNAL DEFINES
*********************************************************************************************************
*/
                                                                /* Consistent signals.                                  */
#define  USBD_ACM_SERIAL_EVENTS_CONS           (USBD_ACM_SERIAL_STATE_DCD | \
                                                USBD_ACM_SERIAL_STATE_DSR)

                                                                /* Irregular signals.                                   */
#define  USBD_ACM_SERIAL_EVENTS_IRRE           (USBD_ACM_SERIAL_STATE_BREAK   | \
                                                USBD_ACM_SERIAL_STATE_RING    | \
                                                USBD_ACM_SERIAL_STATE_PARITY  | \
                                                USBD_ACM_SERIAL_STATE_FRAMING | \
                                                USBD_ACM_SERIAL_STATE_OVERUN)


/*
*********************************************************************************************************
*                                   SET CONTROL LINE STATE DEFINES
*
* Note(s): (1) The PSTN specification version 1.2 defines the 'SetControlLineState' request as:
*
*                +---------------+-------------------+------------------+-----------+---------+------+
*                | bmRequestType |    bRequestCode   |      wValue      |  wIndex   | wLength | Data |
*                +---------------+-------------------+------------------+-----------+---------+------+
*                | 00100001B     | SET_CONTROL_LINE_ |  Control Signal  | Interface |  Zero   | None |
*                |               |       STATE       |     Bitmap       |           |         |      |
*                +---------------+-------------------+------------------+-----------+---------+------+
*
*
*               (a) Table 18 from the PSTN specification defines the control signal bitmap values for
*                   the SET_CONTROL_LINE_STATE
*
*                   Bit Position
*                  -------------
*                       D1          Carrier control for half duplex modems. This signal correspond to
*                                   V.24 signal 105 and RS-232 signal RTS.
*
*                       D0          Indicates to DCE if DTE is present or not. This signal corresponds to
*                                   V.24 signal 108/2 and RS-232 signal DTR.
*********************************************************************************************************
*/

#define USBD_ACM_SERIAL_REQ_DTR                 DEF_BIT_00
#define USBD_ACM_SERIAL_REQ_RTS                 DEF_BIT_01


/*
*********************************************************************************************************
*                                 COMMUNICATION FEATURE SELECTOR DEFINES
*
** Note(s): (1) The PSTN specification version 1.2 defines the 'GetCommFeature' request as:
*
*                +---------------+-------------------+------------------+-----------+-------------+--------+
*                | bmRequestType |    bRequestCode   |      wValue      |  wIndex   |  wLength    |  Data  |
*                +---------------+-------------------+------------------+-----------+-------------+--------+
*                | 00100001B     | GET_COMM_FEATURE  | Feature Selector | Interface | Length of   | Status |
*                |               |                   |     Bitmap       |           | Status Data |        |
*                +---------------+-------------------+------------------+-----------+-------------+--------+
*
*
*               (a) Table 14 from the PSTN specification defines the communication feature selector codes:
*
*                    Feature             Code     Description
*                    Selector
*                   ----------------   --------   ----------------------------------------------------
*                    ABSTRACT_STATE     01h       Two bytes of data describing multiplexed state and idle
*                                                 state for this Abstract Model communications device.
*
*                    COUNTRY_SETTING    02h       Country code in hexadecimal format as defined in
*                                                 [ISO3166], release date as specified in offset 3 of
*                                                 the Country Selection Functional Descriptor. This
*                                                 selector is only valid for devices that provide a
*                                                 Country Selection Functional Descriptor, and the value
*                                                 supplied shall appear as supported country in the
*                                                 Country Selection Functional Descriptor.
*********************************************************************************************************
*/

#define  USBD_ACM_SERIAL_ABSTRACT_STATE                0x01u
#define  USBD_ACM_SERIAL_COUNTRY_SETTING               0x02u


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
*                                        FORWARD DECLARATIONS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                    CDC ACM SERIAL CTRL DATA TYPE
*********************************************************************************************************
*/


typedef  struct  usbd_acm_serial_ctrl {                         /* --------- ACM SUBCLASS CONTROL INFORMATION --------- */
    CPU_INT08U                          Nbr;                    /* CDC dev nbr.                                         */
    CPU_BOOLEAN                         Idle;
    USBD_ACM_SERIAL_LINE_CODING_CHNGD   LineCodingChngdFnct;
    void                               *LineCodingChngdArgPtr;
    USBD_ACM_SERIAL_LINE_CODING         LineCoding;
    USBD_ACM_SERIAL_LINE_CTRL_CHNGD     LineCtrlChngdFnct;
    void                               *LineCtrlChngdArgPtr;
    CPU_INT08U                          LineCtrl;
    CPU_INT08U                          LineState;
    CPU_INT16U                          LineStateInterval;
    CPU_INT08U                         *LineStateBufPtr;
    CPU_BOOLEAN                         LineStateSent;
    CPU_INT08U                          CallMgmtCapabilities;
    CPU_INT08U                         *ReqBufPtr;
} USBD_ACM_SERIAL_CTRL;


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

static  USBD_ACM_SERIAL_CTRL  USBD_ACM_SerialCtrlTbl[USBD_ACM_SERIAL_CFG_MAX_NBR_DEV];
static  CPU_INT08U            USBD_ACM_SerialCtrlNbrNext;


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_ACM_SerialMgmtReq        (       CPU_INT08U       dev_nbr,
                                                    const  USBD_SETUP_REQ  *p_setup_req,
                                                           void            *p_subclass_arg);

static  void         USBD_ACM_SerialNotifyCmpl     (       CPU_INT08U       dev_nbr,
                                                           void            *p_subclass_arg);

static  void         USBD_ACM_SerialFnctDesc       (       CPU_INT08U       dev_nbr,
                                                           void            *p_subclass_arg,
                                                           CPU_INT08U       first_dci_if_nbr);

static  CPU_INT16U   USBD_ACM_SerialFnctDescSizeGet(       CPU_INT08U       dev_nbr,
                                                           void            *p_subclass_arg);


/*
*********************************************************************************************************
*                                        CDC ACM CLASS DRIVER
*********************************************************************************************************
*/

static  USBD_CDC_SUBCLASS_DRV  USBD_ACM_SerialDrv = {
    USBD_ACM_SerialMgmtReq,
    USBD_ACM_SerialNotifyCmpl,
    USBD_ACM_SerialFnctDesc,
    USBD_ACM_SerialFnctDescSizeGet,
};


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          INTERFACE DRIVER
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                        APPLICATION FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                        USBD_ACM_SerialInit()
*
* Description : Initialize CDC ACM serial emulation subclass.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE   CDC ACM serial emulation subclass initialized successfully.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_ACM_SerialInit (USBD_ERR  *p_err)
{
    CPU_INT08U             ix;
    USBD_ACM_SERIAL_CTRL  *p_ctrl;
    LIB_ERR                err_lib;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

                                                                /* Init ACM serial ctrl.                                */
    for (ix = 0u; ix < USBD_ACM_SERIAL_CFG_MAX_NBR_DEV; ix++) {
        p_ctrl                        = &USBD_ACM_SerialCtrlTbl[ix];
        p_ctrl->Nbr                   =  USBD_CDC_NBR_NONE;
        p_ctrl->Idle                  =  DEF_NO;
        p_ctrl->LineCodingChngdFnct   = (USBD_ACM_SERIAL_LINE_CODING_CHNGD)0;
        p_ctrl->LineCodingChngdArgPtr = (void                            *)0;
        p_ctrl->LineCoding.BaudRate   =  9600u;
        p_ctrl->LineCoding.Parity     =  USBD_ACM_SERIAL_PARITY_NONE;
        p_ctrl->LineCoding.StopBits   =  USBD_ACM_SERIAL_STOP_BIT_1;
        p_ctrl->LineCoding.DataBits   =  8u;

        p_ctrl->LineCtrlChngdFnct     = (USBD_ACM_SERIAL_LINE_CTRL_CHNGD)0;
        p_ctrl->LineCtrlChngdArgPtr   = (void                          *)0;
        p_ctrl->LineCtrl              =  DEF_BIT_NONE;

        p_ctrl->LineStateSent         =  DEF_NO;
        p_ctrl->LineStateInterval     =  0u;
        p_ctrl->LineState             =  0u;
        p_ctrl->CallMgmtCapabilities  =  0u;

        p_ctrl->ReqBufPtr = (CPU_INT08U *)Mem_HeapAlloc(              7u,
                                                                      USBD_CFG_BUF_ALIGN_OCTETS,
                                                        (CPU_SIZE_T *)DEF_NULL,
                                                                     &err_lib);
        if (err_lib != LIB_MEM_ERR_NONE) {
           *p_err = USBD_ERR_ALLOC;
            return;
        }

        p_ctrl->LineStateBufPtr = (CPU_INT08U *)Mem_HeapAlloc(              USBD_ACM_SERIAL_STATE_BUF_SIZE,
                                                                            USBD_CFG_BUF_ALIGN_OCTETS,
                                                              (CPU_SIZE_T *)DEF_NULL,
                                                                           &err_lib);
        if (err_lib != LIB_MEM_ERR_NONE) {
           *p_err = USBD_ERR_ALLOC;
            return;
        }

        Mem_Clr((void *)&p_ctrl->LineStateBufPtr[0],
                         USBD_ACM_SERIAL_STATE_BUF_SIZE);
    }

    USBD_ACM_SerialCtrlNbrNext = 0u;

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        USBD_ACM_SerialAdd()
*
* Description : Add a new instance of the CDC ACM serial emulation subclass.
*
* Argument(s) : line_state_interval     Line state notification interval in milliseconds. The value must
*                                       be a power of 2.
*
*               call_mgmt_capabilities  Call Management Capabilities bitmap. OR'ed of the following
*                                       flags (see Note #1):
*
*                           USBD_ACM_SERIAL_CALL_MGMT_DEV           Device handles call management itself.
*                           USBD_ACM_SERIAL_CALL_MGMT_DATA_CCI_DCI  Device can send/receive call management
*                                                                   information over a Data Class interface.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           CDC ACM serial emulation subclass instance
*                                                       successfully added.
*                               USBD_ERR_ALLOC          CDC ACM serial emulation subclass instance NOT available.
*
*                                                               ---------- RETURNED BY USBD_CDC_Add() : ----------
*                               USBD_ERR_ALLOC          CDC class instance NOT available.
*
*                                                               ------ RETURNED BY USBD_CDC_DataIF_Add() : -------
*                               USBD_ERR_ALLOC          Data interface instance NOT available.
*                               USBD_ERR_INVALID_ARG    Invalid argument(s) passed to 'class_nbr/'isoc_en'.
*
* Return(s)   : CDC ACM serial emulation subclass instance number.
*
* Note(s)     : (1) See Note #2 of the function USBD_ACM_SerialFnctDesc() for more details about the
*                   Call Management capabilities.
*
*               (2) Depending on the operating system (Windows, Linux or Mac OS X), not all the possible
*                   flags combinations are supported. Windows and Linux supports all combinations whereas
*                   Mac OS X supports all except the combination (USBD_ACM_SERIAL_CALL_MGMT_DEV).
*********************************************************************************************************
*/

CPU_INT08U  USBD_ACM_SerialAdd (CPU_INT16U   line_state_interval,
                                CPU_INT16U   call_mgmt_capabilities,
                                USBD_ERR    *p_err)

{
    USBD_ACM_SERIAL_CTRL  *p_ctrl;
    CPU_INT08U             subclass_nbr;
    CPU_INT08U             class_nbr;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    CPU_CRITICAL_ENTER();
    subclass_nbr = USBD_ACM_SerialCtrlNbrNext;                  /* Alloc new CDC ACM serial emulation subclass.         */

    if (subclass_nbr >= USBD_ACM_SERIAL_CFG_MAX_NBR_DEV) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_CDC_SUBCLASS_INSTANCE_ALLOC;
        return (USBD_ACM_SERIAL_NBR_NONE);
    }

    USBD_ACM_SerialCtrlNbrNext++;
    CPU_CRITICAL_EXIT();
                                                                /* Init control struct.                                 */
    p_ctrl = &USBD_ACM_SerialCtrlTbl[subclass_nbr];
                                                                /* Create new CDC device.                               */
    class_nbr = USBD_CDC_Add(        USBD_CDC_SUBCLASS_ACM,
                                    &USBD_ACM_SerialDrv,
                             (void *)p_ctrl,
                                     USBD_CDC_COMM_PROTOCOL_AT_V250,
                                     DEF_ENABLED,
                                     line_state_interval,
                                     p_err);

    if (*p_err != USBD_ERR_NONE) {
        return (USBD_ACM_SERIAL_NBR_NONE);
    }
                                                                /* Add data IF class to CDC device.                     */
    (void)USBD_CDC_DataIF_Add(class_nbr,
                              DEF_DISABLED,
                              USBD_CDC_DATA_PROTOCOL_NONE,
                              p_err);

    if (*p_err != USBD_ERR_NONE) {
        return (USBD_ACM_SERIAL_NBR_NONE);
    }

    p_ctrl->LineStateInterval    = line_state_interval;
    p_ctrl->CallMgmtCapabilities = call_mgmt_capabilities;      /* See Note #2.                                         */
    p_ctrl->Nbr               = class_nbr;

    *p_err = USBD_ERR_NONE;

    return (subclass_nbr);
}


/*
*********************************************************************************************************
*                                       USBD_ACM_SerialCfgAdd()
*
* Description : Add CDC ACM subclass class instance into USB device configuration.
*
* Argument(s) : subclass_nbr    CDC ACM serial emulation subclass instance number.
*
*               dev_nbr         Device number.
*
*               cfg_nbr         Configuration index to add new test class interface to.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   CDC ACM serial subclass configuration
*                                                               successfully added.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'subclass_nbr'.
*
*                                                               -------- RETURNED BY USBD_CDC_CfgAdd() : --------
*                               USBD_ERR_ALLOC                  CDC class communication instances NOT available.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'interval'.
*
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_INVALID_CLASS_STATE    Invalid class state.
*                               USBD_ERR_CFG_INVALID_NBR        Invalid configuration number.
*                               USBD_ERR_IF_ALLOC               Interfaces                   NOT available.
*                               USBD_ERR_IF_ALT_ALLOC           Interface alternate settings NOT available.
*                               USBD_ERR_EP_NONE_AVAIL          Physical endpoint NOT available.
*                               USBD_ERR_EP_ALLOC               Endpoints NOT available.
*
* Return(s)   : DEF_YES, if CDC ACM serial subclass instance added to USB device configuration
*                        successfully.
*
*               DEF_NO,  otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  USBD_ACM_SerialCfgAdd (CPU_INT08U   subclass_nbr,
                                    CPU_INT08U   dev_nbr,
                                    CPU_INT08U   cfg_nbr,
                                    USBD_ERR    *p_err)
{
    USBD_ACM_SERIAL_CTRL  *p_ctrl;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(DEF_NO);
    }
#endif

    if (subclass_nbr >= USBD_ACM_SerialCtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (DEF_NO);
    }

    p_ctrl = &USBD_ACM_SerialCtrlTbl[subclass_nbr];

    (void)USBD_CDC_CfgAdd(p_ctrl->Nbr,
                          dev_nbr,
                          cfg_nbr,
                          p_err);

    if (*p_err != USBD_ERR_NONE) {
        return (DEF_NO);
    }

    return (DEF_YES);
}


/*
*********************************************************************************************************
*                                       USBD_ACM_SerialIsConn()
*
* Description : Get CDC ACM serial emulation subclass connection state.
*
* Argument(s) : subclass_nbr    CDC ACM serial emulation subclass instance number.
*
* Return(s)   : DEF_YES, CDC ACM serial emulation is connected.
*
*               DEF_NO,  otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  USBD_ACM_SerialIsConn (CPU_INT08U  subclass_nbr)

{
    USBD_ACM_SERIAL_CTRL  *p_ctrl;
    CPU_BOOLEAN            conn;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (subclass_nbr >= USBD_ACM_SerialCtrlNbrNext) {
        return (DEF_NO);
    }
#endif

    p_ctrl = &USBD_ACM_SerialCtrlTbl[subclass_nbr];

    conn = USBD_CDC_IsConn(p_ctrl->Nbr);

    return (conn);
}


/*
*********************************************************************************************************
*                                         USBD_ACM_SerialRx()
*
* Description : Receive data on CDC ACM serial emulation subclass.
*
* Argument(s) : subclass_nbr    CDC ACM serial emulation subclass instance number.
*
*               p_buf           Pointer to destination buffer to receive data.
*
*               buf_len         Number of octets to receive.
*
*               timeout_ms      Timeout in milliseconds.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE                   Data successfully received.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'subclass_nbr'.
*                               USBD_ERR_INVALID_CLASS_STATE    Invalid subclass state or subclass is in
*                                                                   idle mode.
*
*                                                               ------- RETURNED BY  USBD_CDC_DataRx() : -------
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'/
*                                                                   'data_if_nbr'.
*                               USBD_ERR_INVALID_CLASS_STATE    Invalid subclass state.
*                               USBD_ERR_EP_INVALID_ADDR        Invalid endpoint address.
*                               USBD_ERR_EP_INVALID_STATE       Invalid endpoint state.
*                               USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*
*                               USBD_ERR_OS_TIMEOUT             OS signal NOT successfully acquired in the time
*                                                                   specified by 'timeout_ms'.
*                               USBD_ERR_OS_ABORT               OS signal aborted.
*                               USBD_ERR_OS_FAIL                OS signal not acquired because another error.
*
*                                                               See specific device driver(s) 'EP_RxStart()' for
*                                                                   additional return error codes.
*
*                                                               See specific device driver(s) 'EP_Rx()' for
*                                                                   additional return error codes.
*
* Return(s)   : Number of octets received, if NO error(s).
*
*               0,                         otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT32U  USBD_ACM_SerialRx (CPU_INT08U   subclass_nbr,
                               CPU_INT08U  *p_buf,
                               CPU_INT32U   buf_len,
                               CPU_INT16U   timeout,
                               USBD_ERR    *p_err)
{
    USBD_ACM_SERIAL_CTRL  *p_ctrl;
    CPU_BOOLEAN            conn;
    CPU_INT32U             xfer_len;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    if (subclass_nbr >= USBD_ACM_SerialCtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (0u);
    }

    p_ctrl = &USBD_ACM_SerialCtrlTbl[subclass_nbr];
    conn   =  USBD_CDC_IsConn(p_ctrl->Nbr);

    if ((conn         == DEF_NO  ) ||
        (p_ctrl->Idle == DEF_TRUE)) {
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return (0u);
    }

    xfer_len = USBD_CDC_DataRx(p_ctrl->Nbr,
                               0u,
                               p_buf,
                               buf_len,
                               timeout,
                               p_err);

    return (xfer_len);
}


/*
*********************************************************************************************************
*                                         USBD_ACM_SerialTx()
*
* Description : Send data on CDC ACM serial emulation subclass.
*
* Argument(s) : subclass_nbr   CDC ACM serial emulation subclass instance number.
*
*               p_buf          Pointer to buffer of data that will be transmitted.
*
*               buf_len        Number of octets to transmit.
*
*               timeout_ms     Timeout in milliseconds.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE                   Data successfully received.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'subclass_nbr'.
*                               USBD_ERR_INVALID_CLASS_STATE    Invalid subclass state or subclass is in
*                                                                   idle mode.
*
*                                                               ------- RETURNED BY  USBD_CDC_DataTx() : -------
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'/
*                                                                   'data_if_nbr'.
*                               USBD_ERR_DEV_INVALID_STATE      Transfer type only available if CDC device is in
*                                                                   configured state.
*                               USBD_ERR_EP_INVALID_ADDR        Invalid endpoint address.
*                               USBD_ERR_EP_INVALID_STATE       Invalid endpoint state.
*                               USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*
*                               USBD_ERR_OS_TIMEOUT             OS signal NOT successfully acquired in the time
*                                                                    specified by 'timeout_ms'.
*                               USBD_ERR_OS_ABORT               OS signal aborted.
*                               USBD_ERR_OS_FAIL                OS signal not acquired because another error.
*
*                                                               See specific device driver(s) 'EP_Tx()' for
*                                                                   additional return error codes.
*
*                                                               See specific device driver(s) 'EP_TxStart()' for
*                                                                   additional return error codes.
*
*                                                               See specific device driver(s) 'EP_TxZLP()' for
*                                                                   additional return error codes.
* Return(s)   : Number of octets transmitted, if NO error(s).
*
*               0,                            otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT32U  USBD_ACM_SerialTx (CPU_INT08U   subclass_nbr,
                               CPU_INT08U  *p_buf,
                               CPU_INT32U   buf_len,
                               CPU_INT16U   timeout,
                               USBD_ERR    *p_err)
{
    USBD_ACM_SERIAL_CTRL  *p_ctrl;
    CPU_BOOLEAN            conn;
    CPU_INT32U             xfer_len;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    if (subclass_nbr >= USBD_ACM_SerialCtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (0u);
    }

    p_ctrl = &USBD_ACM_SerialCtrlTbl[subclass_nbr];
    conn   =  USBD_CDC_IsConn(p_ctrl->Nbr);

    if ((conn         == DEF_NO  ) ||
        (p_ctrl->Idle == DEF_TRUE)) {
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return (0u);
    }

    xfer_len = USBD_CDC_DataTx(p_ctrl->Nbr,
                               0u,
                               p_buf,
                               buf_len,
                               timeout,
                               p_err);

    return (xfer_len);
}


/*
*********************************************************************************************************
*                                    USBD_ACM_SerialLineCtrlGet()
*
* Description : Return the state of control lines.
*
* Argument(s) : subclass_nbr    CDC ACM serial emulation subclass instance number.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE           Data successfully received.
*                               USBD_ERR_INVALID_ARG    Invalid argument(s) passed to 'subclass_nbr'.
*
* Return(s)   : Bit-field with the state of the control line.
*
*                   USBD_ACM_SERIAL_CTRL_BREAK
*                   USBD_ACM_SERIAL_CTRL_RTS
*                   USBD_ACM_SERIAL_CTRL_DTR
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  USBD_ACM_SerialLineCtrlGet (CPU_INT08U   subclass_nbr,
                                        USBD_ERR    *p_err)
{
    USBD_ACM_SERIAL_CTRL  *p_ctrl;
    CPU_INT08U             line_ctrl;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    if (subclass_nbr >= USBD_ACM_SerialCtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (DEF_BIT_NONE);
    }

    p_ctrl    = &USBD_ACM_SerialCtrlTbl[subclass_nbr];
    line_ctrl =  p_ctrl->LineCtrl;
   *p_err     =  USBD_ERR_NONE;

    return (line_ctrl);
}


/*
*********************************************************************************************************
*                                    USBD_ACM_SerialLineCtrlReg()
*
* Description : Set line control change notification callback.
*
* Argument(s) : subclass_nbr       CDC ACM serial emulation subclass instance number.
*
*               line_ctrl_chngd    Line control change notification callback (see Note #1).
*
*               p_arg              Pointer to callback argument.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           Line control change notification callback successfully
*                                                           registered.
*
*                               USBD_ERR_INVALID_ARG    Invalid argument(s) passed to 'subclass_nbr'.
*
* Return(s)   : ACM serial emulation subclass device number.
*
* Note(s)     : (1) The callback specified by 'line_ctrl_chngd' argument is used to notify changes in the
*                   control signals to the application.
*
*                   The line control notification function has the following prototype:
*
*                       void  AppLineCtrlChngd  (CPU_INT08U   subclass_nbr,
*                                                CPU_INT08U   events,
*                                                CPU_INT08U   events_chngd,
*                                                void        *p_arg);
*
*                       Argument(s) : subclass_nbr     CDC ACM serial emulation subclass instance number.
*
*                                     events           Current line state. The line state is a OR'ed of
*                                                          the following flags:
*
*                                                          USBD_ACM_SERIAL_CTRL_BREAK
*                                                          USBD_ACM_SERIAL_CTRL_RTS
*                                                          USBD_ACM_SERIAL_CTRL_DTR
*
*                                     events_chngd     Line state flags that have changed.
*
*                                     p_arg            Pointer to callback argument.
*
*********************************************************************************************************
*/

void  USBD_ACM_SerialLineCtrlReg (CPU_INT08U                        subclass_nbr,
                                  USBD_ACM_SERIAL_LINE_CTRL_CHNGD   line_ctrl_chngd,
                                  void                             *p_arg,
                                  USBD_ERR                         *p_err)
{
    USBD_ACM_SERIAL_CTRL  *p_ctrl;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

    if (subclass_nbr >= USBD_ACM_SerialCtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    p_ctrl = &USBD_ACM_SerialCtrlTbl[subclass_nbr];

    CPU_CRITICAL_ENTER();
    p_ctrl->LineCtrlChngdFnct   = line_ctrl_chngd;
    p_ctrl->LineCtrlChngdArgPtr = p_arg;
    CPU_CRITICAL_EXIT();

    *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                   USBD_ACM_SerialLineCodingGet()
*
* Description : Get the current state of the line coding.
*
* Argument(s) : subclass_nbr     CDC ACM serial emulation subclass instance number.
*
*               p_line_coding    Pointer to the structure where the current line coding will be stored.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           Line coding successfully retrieved.
*                               USBD_ERR_INVALID_ARG    Invalid argument(s) to 'subclass_nbr'.
*                               USBD_ERR_NULL_PTR       Argument 'p_line_coding' passed a NULL pointer.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_ACM_SerialLineCodingGet (CPU_INT08U                    subclass_nbr,
                                    USBD_ACM_SERIAL_LINE_CODING  *p_line_coding,
                                    USBD_ERR                     *p_err)
{
    USBD_ACM_SERIAL_CTRL  *p_ctrl;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }

    if (p_line_coding == (USBD_ACM_SERIAL_LINE_CODING *)0) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }
#endif

    if (subclass_nbr >= USBD_ACM_SerialCtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    p_ctrl = &USBD_ACM_SerialCtrlTbl[subclass_nbr];

    CPU_CRITICAL_ENTER();
    p_line_coding->BaudRate = p_ctrl->LineCoding.BaudRate;
    p_line_coding->DataBits = p_ctrl->LineCoding.DataBits;
    p_line_coding->StopBits = p_ctrl->LineCoding.StopBits;
    p_line_coding->Parity   = p_ctrl->LineCoding.Parity;
    CPU_CRITICAL_EXIT();

    *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                   USBD_ACM_SerialLineCodingSet()
*
* Description : Set a new line coding.
*
* Argument(s) : subclass_nbr     CDC ACM serial emulation subclass instance number.
*
*               p_line_coding    Pointer to the structure that contains the new line coding.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           Line coding successfully set.
*                               USBD_ERR_INVALID_ARG    Invalid argument(s) to 'subclass_nbr'.
*                               USBD_ERR_NULL_PTR       Argument 'p_line_coding' passed a NULL pointer.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_ACM_SerialLineCodingSet (CPU_INT08U                    subclass_nbr,
                                    USBD_ACM_SERIAL_LINE_CODING  *p_line_coding,
                                    USBD_ERR                     *p_err)
{
    USBD_ACM_SERIAL_CTRL  *p_ctrl;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }

    if (p_line_coding == (USBD_ACM_SERIAL_LINE_CODING *)0) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }
#endif

    if (subclass_nbr >= USBD_ACM_SerialCtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    if ((p_line_coding->DataBits !=  5u) &&
        (p_line_coding->DataBits !=  6u) &&
        (p_line_coding->DataBits !=  7u) &&
        (p_line_coding->DataBits !=  8u) &&
        (p_line_coding->DataBits != 16u)) {
        *p_err = USBD_ERR_INVALID_ARG;
         return;
    }

    if ((p_line_coding->StopBits != USBD_ACM_SERIAL_STOP_BIT_1  ) &&
        (p_line_coding->StopBits != USBD_ACM_SERIAL_STOP_BIT_1_5) &&
        (p_line_coding->StopBits != USBD_ACM_SERIAL_STOP_BIT_2  )) {
        *p_err = USBD_ERR_INVALID_ARG;
         return;
    }

    if ((p_line_coding->Parity != USBD_ACM_SERIAL_PARITY_NONE ) &&
        (p_line_coding->Parity != USBD_ACM_SERIAL_PARITY_ODD  ) &&
        (p_line_coding->Parity != USBD_ACM_SERIAL_PARITY_EVEN ) &&
        (p_line_coding->Parity != USBD_ACM_SERIAL_PARITY_MARK ) &&
        (p_line_coding->Parity != USBD_ACM_SERIAL_PARITY_SPACE)) {
        *p_err = USBD_ERR_INVALID_ARG;
         return;
    }

    p_ctrl = &USBD_ACM_SerialCtrlTbl[subclass_nbr];

    CPU_CRITICAL_ENTER();
    p_ctrl->LineCoding.BaudRate = p_line_coding->BaudRate;
    p_ctrl->LineCoding.DataBits = p_line_coding->DataBits;
    p_ctrl->LineCoding.StopBits = p_line_coding->StopBits;
    p_ctrl->LineCoding.Parity   = p_line_coding->Parity;
    CPU_CRITICAL_EXIT();

    *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                   USBD_ACM_SerialLineCodingReg()
*
* Description : Set line coding change notification callback.
*
* Argument(s) : subclass_nbr         CDC ACM serial emulation subclass instance number.
*
*               line_coding_chngd    Line coding change notification callback (see Note #1).
*
*               p_arg              Pointer to callback argument.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           Line control change notification callback successfully
*                                                           registered.
*                               USBD_ERR_INVALID_ARG    Invalid argument(s) passed to 'subclass_nbr'.
*
* Return(s)   : None.
*
* Note(s)     : (1) This callback is used to notify changes in the line coding to the application.
*
*                   The line coding change notification function has the following prototype:
*
*                       CPU_BOOLEAN  AppLineCodingChngd (CPU_INT08U                    subclass_nbr,
*                                                        USBD_ACM_SERIAL_LINE_CODING  *p_line_coding,
*                                                        void                         *p_arg);
*
*                       Argument(s) : subclass_nbr     CDC ACM serial emulation subclass instance number.
*
*                                     p_line_coding    Pointer to line coding structure.
*
*                                     p_arg            Pointer to callback argument.
*
*                       Return(s)  :  DEF_OK,   if line coding is supported by the app.
*
*                                     DEF_FAIL, otherwise.
*********************************************************************************************************
*/

void  USBD_ACM_SerialLineCodingReg(CPU_INT08U                          subclass_nbr,
                                   USBD_ACM_SERIAL_LINE_CODING_CHNGD   line_coding_chngd,
                                   void                               *p_arg,
                                   USBD_ERR                           *p_err)
{
    USBD_ACM_SERIAL_CTRL  *p_ctrl;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

    if (subclass_nbr >= USBD_ACM_SerialCtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    p_ctrl = &USBD_ACM_SerialCtrlTbl[subclass_nbr];

    CPU_CRITICAL_ENTER();
    p_ctrl->LineCodingChngdFnct   = line_coding_chngd;
    p_ctrl->LineCodingChngdArgPtr = p_arg;
    CPU_CRITICAL_EXIT();

    *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                    USBD_ACM_SerialLineStateSet()
*
* Description : Set a line state event(s).
*
* Argument(s) : subclass_nbr    CDC ACM serial emulation subclass instance number.
*
*               events          Line state event(s) to set.  OR'ed of the following flags:
*
*                                   USBD_ACM_SERIAL_STATE_DCD        Set DCD signal (Rx carrier).
*                                   USBD_ACM_SERIAL_STATE_DSR        Set DSR signal (Tx carrier).
*                                   USBD_ACM_SERIAL_STATE_BREAK      Set break.
*                                   USBD_ACM_SERIAL_STATE_RING       Set ring.
*                                   USBD_ACM_SERIAL_STATE_FRAMING    Set framing error.
*                                   USBD_ACM_SERIAL_STATE_PARITY     Set parity  error.
*                                   USBD_ACM_SERIAL_STATE_OVERUN     Set overrun.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE                   State events successfully set.
*                               USBD_ERR_CLASS_INVALID_NBR      Invalid number passed to 'subclass_nbr'.
*                               USBD_ERR_INVALID_CLASS_STATE    Invalid subclass state or subclass is in
*                                                               idle mode.
*
*                                                               See 'USBD_CDC_Notify()' for additional return
*                                                               error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_ACM_SerialLineStateSet (CPU_INT08U   subclass_nbr,
                                   CPU_INT08U   events,
                                   USBD_ERR    *p_err)

{
    USBD_ACM_SERIAL_CTRL  *p_ctrl;
    CPU_INT08U             line_state_chngd;
    CPU_BOOLEAN            conn;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

    if (subclass_nbr >= USBD_ACM_SerialCtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    p_ctrl = &USBD_ACM_SerialCtrlTbl[subclass_nbr];
    conn   =  USBD_CDC_IsConn(p_ctrl->Nbr);

    if (conn == DEF_NO) {
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return;
    }

    events &= (USBD_ACM_SERIAL_EVENTS_CONS | USBD_ACM_SERIAL_EVENTS_IRRE);

    CPU_CRITICAL_ENTER();
    line_state_chngd = events ^ p_ctrl->LineState;

    if (line_state_chngd != DEF_BIT_NONE) {
        DEF_BIT_SET(p_ctrl->LineState, events & USBD_ACM_SERIAL_EVENTS_CONS);

        if (p_ctrl->LineStateSent == DEF_NO) {
            p_ctrl->LineStateSent  = DEF_YES;
            p_ctrl->LineStateBufPtr[USBD_CDC_NOTIFICATION_HEADER] = p_ctrl->LineState
                                                                    | (events & USBD_ACM_SERIAL_EVENTS_IRRE);
            CPU_CRITICAL_EXIT();

            (void)USBD_CDC_Notify( p_ctrl->Nbr,
                                   USBD_ACM_SERIAL_REQ_STATE,
                                   0u,
                                  &p_ctrl->LineStateBufPtr[0],
                                   USBD_ACM_SERIAL_REQ_STATE_SIZE,
                                   p_err);
            if (*p_err != USBD_ERR_NONE) {
                return;
            }
        } else {
            DEF_BIT_SET(p_ctrl->LineState, events & USBD_ACM_SERIAL_EVENTS_IRRE);
            CPU_CRITICAL_EXIT();
        }
    } else {
        CPU_CRITICAL_EXIT();
    }

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                    USBD_ACM_SerialLineStateClr()
*
* Description : Clear a line state event(s).
*
* Argument(s) : subclass_nbr    CDC ACM serial emulation subclass instance number.
*
*               events          Line state event(s) set to be cleared. OR'ed of the following
                                flags (see Note #1) :
*
*                                   USBD_ACM_SERIAL_STATE_DCD    Set DCD signal (Rx carrier).
*                                   USBD_ACM_SERIAL_STATE_DSR    Set DSR signal (Tx carrier).
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                                   USBD_ERR_NONE                   State events successfully cleared.
*                                   USBD_ERR_CLASS_INVALID_NBR      Invalid number passed to 'subclass_nbr'.
*                                   USBD_ERR_INVALID_CLASS_STATE    Invalid subclass state or subclass is in
*                                                                   idle mode.
*
*                                                                   See 'USBD_CDC_Notify()' for additional return
*                                                                   error codes.
*
* Return(s)   : none.
*
* Note(s)     :  (1) USB PSTN spec ver 1.20 states: "For the irregular signals like break, the
*                    incoming ring signal, or the overrun error state, this will reset their values
*                    to zero and again will not send another notification until their state changes"
*
*                    The irregular events are self-clear by the ACM serial emulation subclass.
*********************************************************************************************************
*/

void  USBD_ACM_SerialLineStateClr (CPU_INT08U   subclass_nbr,
                                   CPU_INT08U   events,
                                   USBD_ERR    *p_err)
{
    USBD_ACM_SERIAL_CTRL  *p_ctrl;
    CPU_INT08U             line_state_clr;
    CPU_BOOLEAN            conn;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

    if (subclass_nbr >= USBD_ACM_SerialCtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    p_ctrl = &USBD_ACM_SerialCtrlTbl[subclass_nbr];
    conn   =  USBD_CDC_IsConn(p_ctrl->Nbr);

    if (conn == DEF_NO) {
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return;
    }

    events &= USBD_ACM_SERIAL_EVENTS_CONS;
    CPU_CRITICAL_ENTER();
    line_state_clr = events & p_ctrl->LineState;

    if (line_state_clr != DEF_BIT_NONE) {
        DEF_BIT_CLR(p_ctrl->LineState, (CPU_INT32U)events);

        if (p_ctrl->LineStateSent == DEF_NO) {
            p_ctrl->LineStateSent  = DEF_YES;
            p_ctrl->LineStateBufPtr[USBD_CDC_NOTIFICATION_HEADER] = p_ctrl->LineState;
            CPU_CRITICAL_EXIT();

            (void)USBD_CDC_Notify( p_ctrl->Nbr,
                                   USBD_ACM_SERIAL_REQ_STATE,
                                   0u,
                                  &p_ctrl->LineStateBufPtr[0],
                                   USBD_ACM_SERIAL_REQ_STATE_SIZE,
                                   p_err);
            if (*p_err != USBD_ERR_NONE) {
                return;
            }
        } else {
            CPU_CRITICAL_EXIT();
        }
    } else {
        CPU_CRITICAL_EXIT();
    }

    *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                      USBD_ACM_SerialMgmtReq()
*
* Description : CDC ACM serial emulation class management request.
*
* Argument(s) : dev_nbr         Device number.
*
*               p_setup_req     Pointer to setup request structure.
*
*               p_subclass_arg  Pointer to subclass argument.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) CDC ACM defines the following requests :
*
*                   (a) SET_COMM_FEATURE        This request controls the settings for a particular communications
*                                               feature of a particular target.
*
*                   (b) GET_COMM_FEATURE        This request returns the current settings for the communications
*                                               feature as selected
*
*                   (c) CLEAR_COMM_FEATURE      This request controls the settings for a particular communications
*                                               feature of a particular target, setting the selected feature
*                                               to its default state.
*
*                   (d) SET_LINE_CODING         This request allows the host to specify typical asynchronous
*                                               line-character formatting properties, which may be required by
*                                               some applications.
*
*                   (e) GET_LINE_CODING         This request allows the host to find out the currently configured
*                                               line coding.
*
*                   (f) SET_CONTROL_LINE_STATE  This request generates RS-232/V.24 style control signals.
*
*                   (g) SEND_BREAK              This request sends special carrier modulation that generates an
*                                               RS-232 style break.
*
*                   See 'Universal Serial Bus Communications Class Subclass Specification for PSTN Devices
*                   02/09/2007, Version 1.2', section 6.2.2 for more details about ACM requests.
*
*               (2) $$$$ 'SEND_BREAK' with variable length is not implemented in most USB host stacks.
*                   This feature may be implemented in the feature.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_ACM_SerialMgmtReq (       CPU_INT08U       dev_nbr,
                                             const  USBD_SETUP_REQ  *p_setup_req,
                                                    void            *p_subclass_arg)
{
    USBD_ACM_SERIAL_CTRL         *p_ctrl;
    CPU_INT08U                    request_code;
    USBD_ACM_SERIAL_LINE_CODING   line_coding;
    CPU_INT08U                    event_chngd;
    CPU_INT08U                    event_state;
    CPU_BOOLEAN                   valid;
    USBD_ERR                      err;
    CPU_SR_ALLOC();


    p_ctrl       = (USBD_ACM_SERIAL_CTRL *)p_subclass_arg;
    request_code =  p_setup_req->bRequest;
    valid        =  DEF_FAIL;

    switch (request_code) {
        case USBD_CDC_REQ_SET_COMM_FEATURE:                     /* ---------- SET_COMM_FEATURE (see Note #1a) --------- */
                                                                /* Only 'ABSTRACT_STATE' feature is supported.          */
             if (p_setup_req->wValue == USBD_ACM_SERIAL_ABSTRACT_STATE) {
                 (void)USBD_CtrlRx(         dev_nbr,
                                   (void *)&p_ctrl->ReqBufPtr[0],
                                            2u,
                                            USBD_ACM_CTRL_REQ_TIMEOUT_mS,
                                           &err);
                                                                /* Multiplexing call management command on data is ...  */
                                                                /* ... not supported                                    */
                 if ((err == USBD_ERR_NONE) &&
                     (DEF_BIT_IS_CLR(p_ctrl->ReqBufPtr[0], USBD_ACM_SERIAL_ABSTRACT_DATA_MUX))) {
                     CPU_CRITICAL_ENTER();
                     p_ctrl->Idle = DEF_BIT_IS_SET(p_ctrl->ReqBufPtr[0], USBD_ACM_SERIAL_ABSTRACT_IDLE);
                     CPU_CRITICAL_EXIT();
                     valid = DEF_OK;
                 }
             }
             break;


        case USBD_CDC_REQ_GET_COMM_FEATURE:                     /* ---------- GET_COMM_FEATURE (see Note #1b) --------- */
             if (p_setup_req->wValue == USBD_ACM_SERIAL_ABSTRACT_STATE) {
                 p_ctrl->ReqBufPtr[0] = (p_ctrl->Idle == DEF_NO) ? USBD_ACM_SERIAL_ABSTRACT_IDLE : DEF_BIT_NONE;
                 p_ctrl->ReqBufPtr[1] =  DEF_BIT_NONE;

                 (void)USBD_CtrlTx(         dev_nbr,
                                   (void *)&p_ctrl->ReqBufPtr[0],
                                            2u,
                                            USBD_ACM_CTRL_REQ_TIMEOUT_mS,
                                            DEF_NO,
                                           &err);
                 if (err == USBD_ERR_NONE) {
                     valid = DEF_OK;
                 }
             }
             break;



        case USBD_CDC_REQ_CLR_COMM_FEATURE:                     /* --------- CLEAR_COMM_FEATURE (see Note #1c) -------- */
             if (p_setup_req->wValue == USBD_ACM_SERIAL_ABSTRACT_STATE) {
                 CPU_CRITICAL_ENTER();
                 p_ctrl->Idle = DEF_NO;
                 CPU_CRITICAL_EXIT();
                 valid = DEF_OK;
             }
             break;


        case USBD_CDC_REQ_SET_LINE_CODING:                      /* ---------- SET_LINE_CODING (see Note #1d) ---------- */
             (void)USBD_CtrlRx(         dev_nbr,
                               (void *)&p_ctrl->ReqBufPtr[0],
                                        7u,
                                        USBD_ACM_CTRL_REQ_TIMEOUT_mS,
                                       &err);

             if (err == USBD_ERR_NONE) {
                 line_coding.BaudRate = MEM_VAL_GET_INT32U_LITTLE(&p_ctrl->ReqBufPtr[0]);
                 line_coding.StopBits = p_ctrl->ReqBufPtr[4];
                 line_coding.Parity   = p_ctrl->ReqBufPtr[5];
                 line_coding.DataBits = p_ctrl->ReqBufPtr[6];

                 valid = DEF_OK;
                 if (p_ctrl->LineCodingChngdFnct != (USBD_ACM_SERIAL_LINE_CODING_CHNGD)0) {
                     valid = p_ctrl->LineCodingChngdFnct( p_ctrl->Nbr,
                                                         &line_coding,
                                                          p_ctrl->LineCtrlChngdArgPtr);
                 }

                 if (valid == DEF_OK) {
                     CPU_CRITICAL_ENTER();
                     p_ctrl->LineCoding.BaudRate = line_coding.BaudRate;
                     p_ctrl->LineCoding.DataBits = line_coding.DataBits;
                     p_ctrl->LineCoding.StopBits = line_coding.StopBits;
                     p_ctrl->LineCoding.Parity   = line_coding.Parity;
                     CPU_CRITICAL_EXIT();
                 }
             }
             break;


        case USBD_CDC_REQ_GET_LINE_CODING:                      /* ---------- GET_LINE_CODING (see Note #1e) ---------- */
             MEM_VAL_SET_INT32U_LITTLE(&p_ctrl->ReqBufPtr[0], p_ctrl->LineCoding.BaudRate);
             p_ctrl->ReqBufPtr[4] = p_ctrl->LineCoding.StopBits;
             p_ctrl->ReqBufPtr[5] = p_ctrl->LineCoding.Parity;
             p_ctrl->ReqBufPtr[6] = p_ctrl->LineCoding.DataBits;
             (void)USBD_CtrlTx(         dev_nbr,
                               (void *)&p_ctrl->ReqBufPtr[0],
                                        7u,
                                        USBD_ACM_CTRL_REQ_TIMEOUT_mS,
                                        DEF_NO,
                                       &err);

             if (err == USBD_ERR_NONE) {
                 valid = DEF_OK;
             }
             break;


        case USBD_CDC_REQ_SET_CTRL_LINE_STATE:                  /* ------ SET_CONTROL_LINE_STATE (see Note #1f) ------- */
             event_state  = (DEF_BIT_IS_SET(p_setup_req->wValue, USBD_ACM_SERIAL_REQ_RTS) == DEF_YES) ? USBD_ACM_SERIAL_CTRL_RTS
                                                                                                      : DEF_BIT_NONE;
             event_state |= (DEF_BIT_IS_SET(p_setup_req->wValue, USBD_ACM_SERIAL_REQ_DTR) == DEF_YES) ? USBD_ACM_SERIAL_CTRL_DTR
                                                                                                      : DEF_BIT_NONE;
             event_chngd  =  p_ctrl->LineCtrl
                          ^  event_state;

             if ((DEF_BIT_IS_CLR(p_ctrl->LineCtrl   , USBD_ACM_SERIAL_CTRL_DTR) == DEF_YES) &&
                 (DEF_BIT_IS_SET(p_setup_req->wValue, USBD_ACM_SERIAL_REQ_DTR ) == DEF_YES)) {

                 CPU_CRITICAL_ENTER();

                 if (p_ctrl->LineStateSent == DEF_NO) {
                     p_ctrl->LineStateSent                                 = DEF_YES;
                     p_ctrl->LineStateBufPtr[USBD_CDC_NOTIFICATION_HEADER] = p_ctrl->LineState;
                     CPU_CRITICAL_EXIT();

                     (void)USBD_CDC_Notify( p_ctrl->Nbr,
                                            USBD_ACM_SERIAL_REQ_STATE,
                                            0u,
                                           &p_ctrl->LineStateBufPtr[0],
                                            USBD_ACM_SERIAL_REQ_STATE_SIZE,
                                           &err);

                } else {
                    CPU_CRITICAL_EXIT();
                }
             }

             if (event_chngd != DEF_BIT_NONE) {
                 CPU_CRITICAL_ENTER();
                 DEF_BIT_SET(p_ctrl->LineCtrl, ( event_state & event_chngd));
                 DEF_BIT_CLR(p_ctrl->LineCtrl, (~(CPU_INT32U)event_state & event_chngd));
                 CPU_CRITICAL_EXIT();

                 if (p_ctrl->LineCtrlChngdFnct != (USBD_ACM_SERIAL_LINE_CTRL_CHNGD)0) {
                     p_ctrl->LineCtrlChngdFnct(p_ctrl->Nbr,
                                               event_state,
                                               event_chngd,
                                               p_ctrl->LineCtrlChngdArgPtr);
                 }
             }
             valid = DEF_OK;
             break;


        case USBD_CDC_REQ_SEND_BREAK:                           /* ------------- SEND_BREAK (see Note #1g) ------------ */
             if ((p_setup_req->wValue == 0x0000u) &&
                 (DEF_BIT_IS_SET(p_ctrl->LineCtrl, USBD_ACM_SERIAL_CTRL_BREAK))) {

                 CPU_CRITICAL_ENTER();
                 DEF_BIT_CLR(p_ctrl->LineCtrl, USBD_ACM_SERIAL_CTRL_BREAK);
                 CPU_CRITICAL_EXIT();

                 if (p_ctrl->LineCtrlChngdFnct != (void *)0) {
                     p_ctrl->LineCtrlChngdFnct(p_ctrl->Nbr,
                                               DEF_BIT_NONE,
                                               USBD_ACM_SERIAL_CTRL_BREAK,
                                               p_ctrl->LineCtrlChngdArgPtr);
                 }
             } else if ((p_setup_req->wValue != 0x0000u) &&
                        (DEF_BIT_IS_CLR(p_ctrl->LineCtrl, USBD_ACM_SERIAL_CTRL_BREAK))) {

#if 0                                                           /* Feature not implemented (see Note #2).               */
                 if (p_setup_req->wValue != 0xFFFFu) {
                     valid = USBD_ACM_SerialOS_BreakTimeOutSet(p_steup_req->wValue);
                     if (valid == DEF_FAIL) {
                         break;
                     }
                 }
#endif

                 CPU_CRITICAL_ENTER();
                 DEF_BIT_SET(p_ctrl->LineCtrl, USBD_ACM_SERIAL_CTRL_BREAK);
                 CPU_CRITICAL_EXIT();

                 if (p_ctrl->LineCtrlChngdFnct != (void *)0) {
                     p_ctrl->LineCtrlChngdFnct(p_ctrl->Nbr,
                                               USBD_ACM_SERIAL_CTRL_BREAK,
                                               USBD_ACM_SERIAL_CTRL_BREAK,
                                               p_ctrl->LineCtrlChngdArgPtr);
                 }
             }
             valid = DEF_OK;
             break;


        default:
             break;
    }

    return (valid);
}


/*
*********************************************************************************************************
*                                     USBD_ACM_SerialNotifyCmpl()
*
* Description : ACM subclass notification complete callback.
*
* Argument(s) : dev_nbr           Device number.
*
*               p_subclass_arg    Pointer to ACM subclass notification complete callback argument.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_ACM_SerialNotifyCmpl (CPU_INT08U   dev_nbr,
                                         void        *p_subclass_arg)
{
    USBD_ACM_SERIAL_CTRL  *p_ctrl;
    CPU_INT08U             line_state_prev;
    CPU_INT08U             event_chngd;
    USBD_ERR               err;
    CPU_SR_ALLOC();


    (void)dev_nbr;
    p_ctrl  = (USBD_ACM_SERIAL_CTRL *)p_subclass_arg;

    CPU_CRITICAL_ENTER();
                                                                /* Get prev state.                                      */
    line_state_prev = p_ctrl->LineStateBufPtr[USBD_CDC_NOTIFICATION_HEADER];

    if (DEF_BIT_IS_SET_ANY(line_state_prev, USBD_ACM_SERIAL_EVENTS_IRRE)) {
                                                                /* Clr irregular events.                                */
        DEF_BIT_CLR(line_state_prev, line_state_prev & USBD_ACM_SERIAL_EVENTS_IRRE);
    }

    event_chngd = line_state_prev ^ p_ctrl->LineState;

    if (event_chngd != DEF_BIT_NONE) {
        p_ctrl->LineStateBufPtr[USBD_CDC_NOTIFICATION_HEADER] = p_ctrl->LineState;
        DEF_BIT_CLR(p_ctrl->LineState, p_ctrl->LineState & USBD_ACM_SERIAL_EVENTS_IRRE);
        CPU_CRITICAL_EXIT();

        (void)USBD_CDC_Notify( p_ctrl->Nbr,
                               USBD_ACM_SERIAL_REQ_STATE,
                               0u,
                              &p_ctrl->LineStateBufPtr[0],
                               USBD_ACM_SERIAL_REQ_STATE_SIZE,
                              &err);

    } else {
        p_ctrl->LineStateSent = DEF_NO;
        CPU_CRITICAL_EXIT();
    }
}


/*
*********************************************************************************************************
*                                      USBD_ACM_SerialFnctDesc()
*
* Description : CDC Subclass interface descriptor callback.
*
* Argument(s) : dev_nbr             Device number.
*
*               p_subclass_arg      Pointer to subclass argument.
*
*               first_dci_if_nbr    Interface number of the first Data Class Interface following a
*                                   Communication Class Interface (see Note #3).
*
* Return(s)   : none.
*
* Note(s)     : (1) Table 4 of the PSTN specification describes the abstract control management functional
*                   descriptor. The 'bmCapabilities' (offset 3) field describes the requests supported by
*                   the device.
*
*                       BIT       STATE     DESCRIPTION
*                      -------   -------    -------------
*                        D0       '1'       Device supports the request combination of SET_COMM_FEATURE,
*                                           CLR_COMM_FEATURE, and GET_COMM_FEATURE.
*
*                        D1       '1'       Device supports the request combination of SET_LINE_CODING,
*                                           SET_CTRL_LINE_STATE, GET_LINE_CODING and the notification state.
*
*                        D2       '1'       Device supports the request SEND_BREAK.
*
*                        D3       '1'       Device supports the notification NETWORK_CONNECTION.
*                                           Not required in ACM serial emulation subclass.
*
*               (2) Table 3 of the PSTN specification describes the call management functional descriptor.
*                   The 'bmCapabilities' (offset 3) field describes how call management is handled by
*                   the device.
*
*                       BIT       STATE     DESCRIPTION
*                      -------   -------    -------------
*                        D0       '0'       Device does not handle call management itself.
*
*                                 '1'       Device handles call management itself.
*
*                        D1       '0'       Device send/receives call management information only over the
*                                           communication class interface.
*
*                                 '1'       Device can send/receive call management information over a data
*                                           class interface.
*
*               (3) The Call Management Functional Descriptor contains the field 'bDataInterface' which
*                   represents the interface number of Data Class interface (DCI) optionally used for call
*                   management. In case the Communication Class Interface is followed by several DCIs,
*                   the interface number set for 'bDataInterface' will be the first DCI. It is NOT
*                   possible to use another DCI for handling the call management.
*********************************************************************************************************
*/

static  void  USBD_ACM_SerialFnctDesc (CPU_INT08U   dev_nbr,
                                       void        *p_subclass_arg,
                                       CPU_INT08U   first_dci_if_nbr)
{
    USBD_ACM_SERIAL_CTRL  *p_ctrl = (USBD_ACM_SERIAL_CTRL *)p_subclass_arg;


                                                                /* --------- BUILD ABSTRACT CONTROL MGMT DESC --------- */
    USBD_DescWr08(dev_nbr, 4u);                                 /* Desc size.                                           */
    USBD_DescWr08(dev_nbr, USBD_CDC_DESC_TYPE_CS_IF);           /* Desc type.                                           */
    USBD_DescWr08(dev_nbr, USBD_CDC_DESC_SUBTYPE_ACM);          /* Desc subtype.                                        */
                                                                /* Dev request capabilities (see Note #1).              */
    USBD_DescWr08(dev_nbr, DEF_BIT_00 |
                           DEF_BIT_01 |
                           DEF_BIT_02);

                                                                /* --------------- BUILD CALL MGMT DESC --------------- */
    USBD_DescWr08(dev_nbr, 5u);                                 /* Desc size.                                           */
    USBD_DescWr08(dev_nbr, USBD_CDC_DESC_TYPE_CS_IF);           /* Desc type.                                           */
    USBD_DescWr08(dev_nbr, USBD_CDC_DESC_SUBTYPE_CALL_MGMT);    /* Desc subtype.                                        */
    USBD_DescWr08(dev_nbr, p_ctrl->CallMgmtCapabilities);       /* Dev call mgmt capabilities (see Note #2).            */
                                                                /* IF nbr of data class IF optionally used for call ... */
                                                                /* ... mgmt.                                            */
    if (p_ctrl->CallMgmtCapabilities == USBD_ACM_SERIAL_CALL_MGMT_DATA_OVER_DCI) {
        USBD_DescWr08(dev_nbr, first_dci_if_nbr);               /* See Note #3.                                         */
    } else {
        USBD_DescWr08(dev_nbr, 0u);

    }
}


/*
*********************************************************************************************************
*                                  USBD_ACM_SerialFnctDescSizeGet()
*
* Description : Retrieve the size of the CDC subclass interface descriptor.
*
* Argument(s) : dev_nbr           Device number.
*
*               p_subclass_arg    Pointer to subclass argument.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT16U  USBD_ACM_SerialFnctDescSizeGet (CPU_INT08U   dev_nbr,
                                                    void        *p_subclass_arg)
{
    (void)dev_nbr;
    (void)p_subclass_arg;

    return (USBD_ACM_DESC_TOT_SIZE);
}
