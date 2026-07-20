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
*                                        ETHERNET CONTROL MODEL (ECM)
*
* Filename : usbd_ecm.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)       : (1) This implementation is compliant with the ECM subclass specification revision 1.2
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#include  "usbd_ecm.h"


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

#define  USBD_ECM_DESC_SIZE                              13u    /* Size of the ECM functional descriptor.               */
#define  USBD_ECM_REQ_BUF_SIZE                           16u    /* Size of the request buffer.                          */
#define  USBD_ECM_NOTIFY_INTERVAL                         8u    /* Notification endpoint polling interval.              */


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
*                                    CDC ECM SERIAL CTRL DATA TYPE
*********************************************************************************************************
*/

typedef  struct  usbd_ecm_ctrl {                                /* --------- ECM SUBCLASS CONTROL INFORMATION --------- */
    CPU_INT08U          Nbr;                                    /* CDC dev nbr.                                         */
    USBD_ECM_MGMT_REQ   MgmtReq;                                /* Mgmt req callback.                                   */
    void               *MgmtReqArg;                             /* Mgmt req callback arg.                               */
    CPU_INT08U          MACAddrStrIdx;                          /* MAC address string index.                            */
    CPU_INT32U          EthernetStats;                          /* Ethernet statistics.                                 */
    CPU_INT16U          MaxSegSize;                             /* Maximum segment size.                                */
    CPU_INT16U          NumMCFilters;                           /* Number of multicast filters.                         */
    CPU_INT08U          NumPwrFilters;                          /* Number of power filters.                             */
    CPU_INT08U          ReqBufPtr[USBD_ECM_REQ_BUF_SIZE];       /* Request buffer.                                      */
} USBD_ECM_CTRL;


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

static  USBD_ECM_CTRL  USBD_ECM_CtrlTbl[USBD_ECM_CFG_MAX_NBR_DEV];
static  CPU_INT08U     USBD_ECM_CtrlNbrNext;


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_ECM_MgmtReq        (       CPU_INT08U       dev_nbr,
                                              const  USBD_SETUP_REQ  *p_setup_req,
                                                     void            *p_subclass_arg);

static  void         USBD_ECM_NotifyCmpl     (       CPU_INT08U       dev_nbr,
                                                     void            *p_subclass_arg);

static  void         USBD_ECM_FnctDesc       (       CPU_INT08U       dev_nbr,
                                                     void            *p_subclass_arg,
                                                     CPU_INT08U       first_dci_if_nbr);

static  CPU_INT16U   USBD_ECM_FnctDescSizeGet(       CPU_INT08U       dev_nbr,
                                                     void            *p_subclass_arg);


/*
*********************************************************************************************************
*                                        CDC ECM CLASS DRIVER
*********************************************************************************************************
*/

static  USBD_CDC_SUBCLASS_DRV  USBD_ECM_Drv = {
    USBD_ECM_MgmtReq,
    USBD_ECM_NotifyCmpl,
    USBD_ECM_FnctDesc,
    USBD_ECM_FnctDescSizeGet,
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
*                                           USBD_ECM_Init()
*
* Description : Initialize CDC ECM subclass.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE   CDC ECM subclass initialized successfully.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_ECM_Init (USBD_ERR  *p_err)
{
    CPU_INT08U      ix;
    USBD_ECM_CTRL  *p_ctrl;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

                                                                /* Init ECM ctrl.                                       */
    for (ix = 0u; ix < USBD_ECM_CFG_MAX_NBR_DEV; ix++) {
        p_ctrl                = &USBD_ECM_CtrlTbl[ix];
        p_ctrl->Nbr           =  USBD_CDC_NBR_NONE;
        p_ctrl->MgmtReq       = (USBD_ECM_MGMT_REQ)0;
        p_ctrl->MACAddrStrIdx =  0u;
        p_ctrl->EthernetStats =  0u;
        p_ctrl->MaxSegSize    =  0u;
        p_ctrl->NumMCFilters  =  0u;
        p_ctrl->NumPwrFilters =  0u;

        Mem_Clr(p_ctrl->ReqBufPtr, USBD_ECM_REQ_BUF_SIZE);
    }

    USBD_ECM_CtrlNbrNext = 0u;

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            USBD_ECM_Add()
*
* Description : Add a new instance of the CDC ECM subclass.
*
* Argument(s) : mgmt_req     Callback for ECM requests, will be called on USBD_CDC_REQ_GET/SET_ETHER_* requests.
*
*               mgmt_req_arg Argument to be passed to mgmt_req when called.
*
*               end_tx_en    Enable end of transfer on tx.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           CDC ECM subclass instance successfully added.
*                               USBD_ERR_ALLOC          CDC ECM subclass instance NOT available.
*
*                                                               ---------- RETURNED BY USBD_CDC_Add() : ----------
*                               USBD_ERR_ALLOC          CDC class instance NOT available.
*
*                                                               ------ RETURNED BY USBD_CDC_DataIF_Add() : -------
*                               USBD_ERR_ALLOC          Data interface instance NOT available.
*                               USBD_ERR_INVALID_ARG    Invalid argument(s) passed to 'class_nbr/'isoc_en'.
*
* Return(s)   : CDC ECM subclass instance number.
*
* Note(s)     : None.
*********************************************************************************************************
*/

CPU_INT08U  USBD_ECM_Add (USBD_ECM_MGMT_REQ   mgmt_req,
                          void               *mgmt_req_arg,
                          CPU_BOOLEAN         end_tx_en,
                          USBD_ERR           *p_err)

{
    USBD_ECM_CTRL  *p_ctrl;
    CPU_INT08U      subclass_nbr;
    CPU_INT08U      class_nbr;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    CPU_CRITICAL_ENTER();
    subclass_nbr = USBD_ECM_CtrlNbrNext;                        /* Alloc new CDC ECM emulation subclass.                */

    if (subclass_nbr >= USBD_ECM_CFG_MAX_NBR_DEV) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_CDC_SUBCLASS_INSTANCE_ALLOC;
        return (USBD_ECM_NBR_NONE);
    }

    USBD_ECM_CtrlNbrNext++;
    CPU_CRITICAL_EXIT();
                                                                /* Init control struct.                                 */
    p_ctrl = &USBD_ECM_CtrlTbl[subclass_nbr];
                                                                /* Create new CDC device.                               */
    class_nbr = USBD_CDC_Add(        USBD_CDC_SUBCLASS_ENCM,
                                    &USBD_ECM_Drv,
                             (void *)p_ctrl,
                                     USBD_CDC_COMM_PROTOCOL_NONE,
                                     DEF_ENABLED,
                                     USBD_ECM_NOTIFY_INTERVAL,
                                     p_err);

    if (*p_err != USBD_ERR_NONE) {
        return (USBD_ECM_NBR_NONE);
    }

    USBD_CDC_SetAltIFEn(class_nbr, DEF_ENABLED, p_err);         /* Enable first alternate setting.                      */

    if (*p_err != USBD_ERR_NONE) {
        return (USBD_ECM_NBR_NONE);
    }

    USBD_CDC_SetEndOfTx(class_nbr, end_tx_en, p_err);           /* Set end of transfers.                                */

    if (*p_err != USBD_ERR_NONE) {
        return (USBD_ECM_NBR_NONE);
    }

                                                                /* Add data IF class to CDC device.                     */
    (void)USBD_CDC_DataIF_Add(class_nbr,
                              DEF_DISABLED,
                              USBD_CDC_DATA_PROTOCOL_NONE,
                              p_err);

    if (*p_err != USBD_ERR_NONE) {
        return (USBD_ECM_NBR_NONE);
    }

    p_ctrl->Nbr        = class_nbr;
    p_ctrl->MgmtReq    = mgmt_req;
    p_ctrl->MgmtReqArg = mgmt_req_arg;

   *p_err = USBD_ERR_NONE;

    return (subclass_nbr);
}


/*
*********************************************************************************************************
*                                          USBD_ECM_CfgAdd()
*
* Description : Add CDC ECM subclass instance into USB device configuration.
*
* Argument(s) : subclass_nbr    CDC ECM subclass instance number.
*
*               dev_nbr         Device number.
*
*               cfg_nbr         Configuration index to add new test class interface to.
*
*               mac_addr        MAC address string. Must be at least USBD_ECM_MAC_ADDR_LEN characters long.
*
*               ethernet_stats  Ethernet statistics.
*
*               max_seg_size    Maximum segment size. Typical Ethernet frames are 1514 bytes or less in length
 *                              (not including the CRC), but this can be longer (e.g., 802.1Q VLAN tagging).
 *                              Reference ECM 1.2 Section 3.3.2.
*
*               num_mc_filters  Number of multicast filters.
*
*               num_pwr_filters Number of power filters.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   USBD_ERR_NONE                   CDC ECM subclass configuration successfully added.
*                                   USBD_ERR_CLASS_INVALID_NBR      Invalid subclass number.
*
*                                                               -------- RETURNED BY USBD_CDC_CfgAdd() : --------
*                                   USBD_ERR_ALLOC                  CDC class communication instances NOT available.
*                                   USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'interval'.
*
*                                   USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                                   USBD_ERR_INVALID_CLASS_STATE    Invalid class state.
*                                   USBD_ERR_CFG_INVALID_NBR        Invalid configuration number.
*                                   USBD_ERR_IF_ALLOC               Interfaces                   NOT available.
*                                   USBD_ERR_IF_ALT_ALLOC           Interface alternate settings NOT available.
*                                   USBD_ERR_EP_NONE_AVAIL          Physical endpoint NOT available.
*                                   USBD_ERR_EP_ALLOC               Endpoints NOT available.
*
* Return(s)   : DEF_YES, if CDC ECM subclass instance added to USB device configuration successfully.
*
*               DEF_NO,  otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  USBD_ECM_CfgAdd (      CPU_INT08U   subclass_nbr,
                                    CPU_INT08U   dev_nbr,
                                    CPU_INT08U   cfg_nbr,
                              const CPU_CHAR    *mac_addr,
                                    CPU_INT32U   ethernet_stats,
                                    CPU_INT16U   max_seg_size,
                                    CPU_INT16U   num_mc_filters,
                                    CPU_INT08U   num_pwr_filters,
                                    USBD_ERR    *p_err)
{
    USBD_ECM_CTRL  *p_ctrl;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(DEF_NO);
    }
    if (mac_addr == (CPU_CHAR *)0) {                            /* Validate MAC address ptr.                            */
        CPU_SW_EXCEPTION(DEF_NO);
    }
#endif

    if (subclass_nbr >= USBD_ECM_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (DEF_NO);
    }

    p_ctrl = &USBD_ECM_CtrlTbl[subclass_nbr];

    (void)USBD_CDC_CfgAdd(p_ctrl->Nbr,
                          dev_nbr,
                          cfg_nbr,
                          p_err);

    if (*p_err != USBD_ERR_NONE) {
        return (DEF_NO);
    }

                                                                /* Add string descriptor for MAC address.               */
    USBD_StrAdd(dev_nbr, mac_addr, p_err);

    if (*p_err != USBD_ERR_NONE) {
        return (DEF_NO);
    }

                                                                /* Store MAC address string index.                      */
    p_ctrl->MACAddrStrIdx = USBD_StrIxGet(dev_nbr, mac_addr);

    p_ctrl->EthernetStats = ethernet_stats;
    p_ctrl->MaxSegSize    = max_seg_size;
    p_ctrl->NumMCFilters  = num_mc_filters;
    p_ctrl->NumPwrFilters = num_pwr_filters;

    return (DEF_YES);
}


/*
*********************************************************************************************************
*                                         USBD_ECM_DataRx()
*
* Description : Receive data on CDC ECM subclass.
*
* Argument(s) : subclass_nbr    CDC ECM subclass instance number.
*
*               p_buf           Pointer to destination buffer to receive data.
*
*               buf_len         Number of octets to receive.
*
*               timeout_ms      Timeout in milliseconds.
*
*               p_err           Pointer to variable that will receive return error code from this function :
*
*                                   USBD_ERR_NONE                   Data successfully received.
*                                   USBD_ERR_CLASS_INVALID_NBR      Invalid subclass number.
*                                   USBD_ERR_INVALID_CLASS_STATE    Invalid subclass state or subclass is in
*                                                                   idle mode.
*
*                                                               ------- RETURNED BY  USBD_CDC_DataRx() : -------
*                                   USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'/
*                                                                   'data_if_nbr'.
*                                   USBD_ERR_INVALID_CLASS_STATE    Invalid subclass state.
*                                   USBD_ERR_EP_INVALID_ADDR        Invalid endpoint address.
*                                   USBD_ERR_EP_INVALID_STATE       Invalid endpoint state.
*                                   USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*
*                                   USBD_ERR_OS_TIMEOUT             OS signal NOT successfully acquired in the time
*                                                                   specified by 'timeout_ms'.
*                                   USBD_ERR_OS_ABORT               OS signal aborted.
*                                   USBD_ERR_OS_FAIL                OS signal not acquired because another error.
*
*                                                                   See specific device driver(s) 'EP_RxStart()' for
*                                                                   additional return error codes.
*
*                                                                   See specific device driver(s) 'EP_Rx()' for
*                                                                   additional return error codes.
*
* Return(s)   : Number of octets received, if NO error(s).
*
*               0,                         otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT32U  USBD_ECM_DataRx (CPU_INT08U   subclass_nbr,
                             CPU_INT08U  *p_buf,
                             CPU_INT32U   buf_len,
                             CPU_INT16U   timeout,
                             USBD_ERR    *p_err)
{
    USBD_ECM_CTRL  *p_ctrl;
    CPU_BOOLEAN     conn;
    CPU_INT32U      xfer_len;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    if (subclass_nbr >= USBD_ECM_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (0u);
    }

    p_ctrl = &USBD_ECM_CtrlTbl[subclass_nbr];
    conn   =  USBD_CDC_IsConn(p_ctrl->Nbr);

    if (conn == DEF_NO) {
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
*                                         USBD_ECM_DataTx()
*
* Description : Send data on CDC ECM subclass.
*
* Argument(s) : subclass_nbr   CDC ECM subclass instance number.
*
*               p_buf          Pointer to buffer of data that will be transmitted.
*
*               buf_len        Number of octets to transmit.
*
*               timeout_ms     Timeout in milliseconds.
*
*               p_err          Pointer to variable that will receive return error code from this function :
*
*                                   USBD_ERR_NONE                   Data successfully received.
*                                   USBD_ERR_CLASS_INVALID_NBR      Invalid subclass number.
*                                   USBD_ERR_INVALID_CLASS_STATE    Invalid subclass state or subclass is in
*                                                                   idle mode.
*
*                                                               ------- RETURNED BY  USBD_CDC_DataTx() : -------
*                                   USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'/
*                                                                   'data_if_nbr'.
*                                   USBD_ERR_DEV_INVALID_STATE      Transfer type only available if CDC device is in
*                                                                   configured state.
*                                   USBD_ERR_EP_INVALID_ADDR        Invalid endpoint address.
*                                   USBD_ERR_EP_INVALID_STATE       Invalid endpoint state.
*                                   USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*
*                                   USBD_ERR_OS_TIMEOUT             OS signal NOT successfully acquired in the time
*                                                                    specified by 'timeout_ms'.
*                                   USBD_ERR_OS_ABORT               OS signal aborted.
*                                   USBD_ERR_OS_FAIL                OS signal not acquired because another error.
*
*                                                                   See specific device driver(s) 'EP_Tx()' for
*                                                                   additional return error codes.
*
*                                                                   See specific device driver(s) 'EP_TxStart()' for
*                                                                   additional return error codes.
*
*                                                                   See specific device driver(s) 'EP_TxZLP()' for
*                                                                   additional return error codes.
*
* Return(s)   : Number of octets transmitted, if NO error(s).
*
*               0,                            otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT32U  USBD_ECM_DataTx (CPU_INT08U   subclass_nbr,
                             CPU_INT08U  *p_buf,
                             CPU_INT32U   buf_len,
                             CPU_INT16U   timeout,
                             USBD_ERR    *p_err)
{
    USBD_ECM_CTRL  *p_ctrl;
    CPU_BOOLEAN     conn;
    CPU_INT32U      xfer_len;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    if (subclass_nbr >= USBD_ECM_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (0u);
    }

    p_ctrl = &USBD_ECM_CtrlTbl[subclass_nbr];
    conn   =  USBD_CDC_IsConn(p_ctrl->Nbr);

    if (conn == DEF_NO) {
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
*                                         USBD_ECM_DataRxAsync()
*
* Description : Async receive data on CDC ECM subclass.
*
* Argument(s) : subclass_nbr    CDC ECM subclass instance number.
*
*               p_buf           Pointer to destination buffer to receive data.
*
*               buf_len         Number of octets to receive.
*
*               async           Pointer to function that will be called when the transfer is complete.
*
*               p_async_arg     Pointer to argument that will be passed to the async function.
*
*               p_err           Pointer to variable that will receive return error code from this function :
*
*                                   USBD_ERR_NONE                   Callback successfully registered.
*                                   USBD_ERR_CLASS_INVALID_NBR      Invalid subclass number.
*                                   USBD_ERR_INVALID_CLASS_STATE    Invalid subclass state or subclass is in
*                                                                   idle mode.
*
*                                                                   See specific class driver 'USBD_CDC_DataRxAsync()'
 *                                                                  for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_ECM_DataRxAsync (CPU_INT08U       subclass_nbr,
                            CPU_INT08U      *p_buf,
                            CPU_INT32U       buf_len,
                            USBD_ASYNC_FNCT  async,
                            void            *p_async_arg,
                            USBD_ERR        *p_err)
{
    USBD_ECM_CTRL  *p_ctrl;
    CPU_BOOLEAN     conn;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

    if (subclass_nbr >= USBD_ECM_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    p_ctrl = &USBD_ECM_CtrlTbl[subclass_nbr];
    conn   =  USBD_CDC_IsConn(p_ctrl->Nbr);

    if (conn == DEF_NO) {
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return;
    }

    USBD_CDC_DataRxAsync(p_ctrl->Nbr,
                         0,
                         p_buf,
                         buf_len,
                         async,
                         p_async_arg,
                         p_err);
}


/*
*********************************************************************************************************
*                                          USBD_ECM_Notify()
*
* Description : Send ECM notification to host.
*
* Argument(s) : subclass_nbr    CDC ECM subclass instance number.
*
*               notification    Notification code.
*
*               value           Notification value.
*
*               p_buf           Pointer to notification buffer (see Note #1).
*
*               data_len        Length of the data portion of the notification.
*
*               p_err           Pointer to variable that will receive return error code from this function :
*
*                                   USBD_ERR_NONE                   Notification successfully sent.
*                                   USBD_ERR_CLASS_INVALID_NBR      Invalid subclass number.
*                                   USBD_ERR_INVALID_CLASS_STATE    Invalid class state.
*
*                                                                   See specific class driver(s) 'USBD_CDC_Notify()' for
*                                                                   additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT32U  USBD_ECM_Notify (CPU_INT08U   subclass_nbr,
                             CPU_INT08U   notification,
                             CPU_INT16U   value,
                             CPU_INT08U  *p_data,
                             CPU_INT32U   len,
                             USBD_ERR    *p_err)
{
    USBD_ECM_CTRL  *p_ctrl;
    CPU_BOOLEAN     conn;
    CPU_INT32U      xfer_len;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    if (subclass_nbr >= USBD_ECM_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (0u);
    }

    p_ctrl = &USBD_ECM_CtrlTbl[subclass_nbr];
    conn   =  USBD_CDC_IsConn(p_ctrl->Nbr);

    if (conn == DEF_NO) {
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return (0u);
    }

    xfer_len = USBD_CDC_Notify(p_ctrl->Nbr,
                               notification,
                               value,
                               p_data,
                               len,
                               p_err);

    return (xfer_len);
}


/*
*********************************************************************************************************
*                                       USBD_ECM_NotifyNetConn()
*
* Description : Send NetworkConnection notification to host.
*
* Argument(s) : subclass_nbr    CDC ECM subclass instance number.
*
*               conn_status     Connection status. DEF_TRUE if connected, DEF_FALSE if disconnected.
*
*               p_err           Pointer to variable that will receive return error code from this function :
*
*                                   USBD_ERR_NONE                   Data successfully received.
*                                   USBD_ERR_CLASS_INVALID_NBR      Invalid subclass number.
*                                   USBD_ERR_INVALID_CLASS_STATE    Invalid class state.
*
*                                                                   See specific class driver(s) 'USBD_CDC_Notify()' for
*                                                                   additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_ECM_NotifyNetConn (CPU_INT08U    subclass_nbr,
                              CPU_BOOLEAN   conn_status,
                              USBD_ERR     *p_err)
{
    USBD_ECM_CTRL  *p_ctrl;


    if (subclass_nbr >= USBD_ECM_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    p_ctrl = &USBD_ECM_CtrlTbl[subclass_nbr];

    USBD_ECM_Notify(subclass_nbr,
                    USBD_CDC_NOTIFICATION_NET_CONN,
                    conn_status ? 1 : 0,
                    p_ctrl->ReqBufPtr,
                    0,
                    p_err);
}


/*
*********************************************************************************************************
*                                     USBD_ECM_NotifyConnSpdChng()
*
* Description : Send NetworkConnection notification to host.
*
* Argument(s) : subclass_nbr    CDC ECM subclass instance number.
*
*               dl_bit_rate     Downlink bit rate.
*
*               ul_bit_rate     Uplink bit rate.
*
*               p_err           Pointer to variable that will receive return error code from this function :
*
*                                   USBD_ERR_NONE                   Data successfully received.
*                                   USBD_ERR_CLASS_INVALID_NBR      Invalid subclass number.
*                                   USBD_ERR_INVALID_CLASS_STATE    Invalid class state.
*
*                                                                   See specific class driver(s) 'USBD_CDC_Notify()' for
*                                                                   additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_ECM_NotifyConnSpdChng (CPU_INT08U   subclass_nbr,
                                  CPU_INT32U   dl_bit_rate,
                                  CPU_INT32U   ul_bit_rate,
                                  USBD_ERR    *p_err)
{
    USBD_ECM_CTRL  *p_ctrl;


    if (subclass_nbr >= USBD_ECM_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    p_ctrl = &USBD_ECM_CtrlTbl[subclass_nbr];

    Mem_Copy(p_ctrl->ReqBufPtr + USBD_CDC_NOTIFICATION_HEADER,                       &dl_bit_rate, sizeof(dl_bit_rate));
    Mem_Copy(p_ctrl->ReqBufPtr + USBD_CDC_NOTIFICATION_HEADER + sizeof(dl_bit_rate), &ul_bit_rate, sizeof(ul_bit_rate));

    USBD_ECM_Notify(subclass_nbr,
                    USBD_CDC_NOTIFICATION_CONN_SPEED_CHNG,
                    0,
                    p_ctrl->ReqBufPtr,
                    sizeof(dl_bit_rate) + sizeof(ul_bit_rate),
                    p_err);
}


/*
*********************************************************************************************************
*                                          USBD_ECM_MgmtReq()
*
* Description : CDC ECM class management request.
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
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_ECM_MgmtReq (       CPU_INT08U       dev_nbr,
                                       const  USBD_SETUP_REQ  *p_setup_req,
                                              void            *p_subclass_arg)
{
    USBD_ECM_CTRL  *p_ctrl;
    CPU_INT08U      request_code;
    CPU_BOOLEAN     valid;


    p_ctrl       = (USBD_ECM_CTRL *)p_subclass_arg;
    request_code =  p_setup_req->bRequest;
    valid        =  DEF_FAIL;

    if (p_ctrl->MgmtReq == DEF_NULL) {
        return (valid);
    }

    switch (request_code) {
        case USBD_CDC_REQ_SET_ETHER_MULTI_FILTER:
        case USBD_CDC_REQ_SET_ETHER_PWR_MGT_FILTER:
        case USBD_CDC_REQ_GET_ETHER_PWR_MGT_FILTER:
        case USBD_CDC_REQ_SET_ETHER_PKT_FILTER:
        case USBD_CDC_REQ_GET_ETHER_STAT:
             valid = p_ctrl->MgmtReq(dev_nbr, p_setup_req, p_ctrl->MgmtReqArg);
             break;


        default:
             break;
    }

    return (valid);
}


/*
*********************************************************************************************************
*                                        USBD_ECM_NotifyCmpl()
*
* Description : ECM subclass notification complete callback.
*
* Argument(s) : dev_nbr           Device number.
*
*               p_subclass_arg    Pointer to ECM subclass notification complete callback argument.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_ECM_NotifyCmpl (CPU_INT08U   dev_nbr,
                                   void        *p_subclass_arg)
{
    (void)dev_nbr;
    (void)p_subclass_arg;
}


/*
*********************************************************************************************************
*                                         USBD_ECM_FnctDesc()
*
* Description : CDC ECM Subclass interface descriptor callback.
*
* Argument(s) : dev_nbr             Device number.
*
*               p_subclass_arg      Pointer to subclass argument.
*
*               first_dci_if_nbr    Unused
*
* Return(s)   : none.
*
* Note(s)     : (1) Function descriptor specified in ECM 1.2 Section 5.4
*********************************************************************************************************
*/

static  void  USBD_ECM_FnctDesc (CPU_INT08U   dev_nbr,
                                 void        *p_subclass_arg,
                                 CPU_INT08U   first_dci_if_nbr)
{
    (void)first_dci_if_nbr;

    USBD_ECM_CTRL  *p_ctrl = (USBD_ECM_CTRL *)p_subclass_arg;

                                                                /* ------- BUILD ECM FUNCTION DESC (see Note #1)------- */
    USBD_DescWr08(dev_nbr, USBD_ECM_DESC_SIZE);                 /* Desc size.                                           */
    USBD_DescWr08(dev_nbr, USBD_CDC_DESC_TYPE_CS_IF);           /* Desc type.                                           */
    USBD_DescWr08(dev_nbr, USBD_CDC_DESC_SUBTYPE_ETHER_NET);    /* Desc subtype.                                        */
    USBD_DescWr08(dev_nbr, p_ctrl->MACAddrStrIdx);              /* MAC address string index.                            */
    USBD_DescWr32(dev_nbr, p_ctrl->EthernetStats);              /* Ethernet statistics.                                 */
    USBD_DescWr16(dev_nbr, p_ctrl->MaxSegSize);                 /* Maximum segment size.                                */
    USBD_DescWr16(dev_nbr, p_ctrl->NumMCFilters);               /* Number of multicast filters.                         */
    USBD_DescWr08(dev_nbr, p_ctrl->NumPwrFilters);              /* Number of power filters.                             */
}


/*
*********************************************************************************************************
*                                      USBD_ECM_FnctDescSizeGet()
*
* Description : Retrieve the size of the CDC ECM subclass interface descriptor.
*
* Argument(s) : dev_nbr           Device number.
*
*               p_subclass_arg    Pointer to subclass argument.
*
* Return(s)   : Descriptor size.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT16U  USBD_ECM_FnctDescSizeGet (CPU_INT08U   dev_nbr,
                                              void        *p_subclass_arg)
{
    (void)dev_nbr;
    (void)p_subclass_arg;

    return (USBD_ECM_DESC_SIZE);
}
