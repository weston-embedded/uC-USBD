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
*                                        USB DEVICE VENDOR CLASS
*
* Filename : usbd_vendor.c
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    USBD_VENDOR_MODULE
#include  <lib_mem.h>
#include  <lib_math.h>
#include  "usbd_vendor.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

                                                                /* Max nbr of comm struct.                              */
#define  USBD_VENDOR_COMM_NBR_MAX              (USBD_VENDOR_CFG_MAX_NBR_DEV * \
                                                USBD_VENDOR_CFG_MAX_NBR_CFG)


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

typedef  struct  usbd_vendor_ctrl  USBD_VENDOR_CTRL;            /* Forward declaration.                                 */


/*
*********************************************************************************************************
*                                         VENDOR CLASS STATES
*********************************************************************************************************
*/

typedef  enum  usbd_vendor_state {                              /* Vendor class states.                                 */
    USBD_VENDOR_STATE_NONE = 0,
    USBD_VENDOR_STATE_INIT,
    USBD_VENDOR_STATE_CFG
} USBD_VENDOR_STATE;


/*
*********************************************************************************************************
*                               VENDOR CLASS EP REQUIREMENTS DATA TYPE
*********************************************************************************************************
*/

                                                                /* -------------- VENDOR CLASS COMM INFO -------------- */
typedef  struct  usbd_vendor_comm {
    USBD_VENDOR_CTRL        *CtrlPtr;                           /* Ptr to ctrl info.                                    */
                                                                /* Avail EP for comm: Bulk (and Intr)                   */
    CPU_INT08U               DataBulkInEpAddr;
    CPU_INT08U               DataBulkOutEpAddr;
    CPU_INT08U               IntrInEpAddr;
    CPU_INT08U               IntrOutEpAddr;

    CPU_BOOLEAN              DataBulkInActiveXfer;
    CPU_BOOLEAN              DataBulkOutActiveXfer;
    CPU_BOOLEAN              IntrInActiveXfer;
    CPU_BOOLEAN              IntrOutActiveXfer;
} USBD_VENDOR_COMM;


struct  usbd_vendor_ctrl {                                      /* -------------- VENDOR CLASS CTRL INFO -------------- */
    CPU_INT08U                DevNbr;                           /* Vendor class dev nbr.                                */
    USBD_VENDOR_STATE         State;                            /* Vendor class state.                                  */
    CPU_INT08U                ClassNbr;                         /* Vendor class instance nbr.                           */
    USBD_VENDOR_COMM         *CommPtr;                          /* Vendor class comm info ptr.                          */
    CPU_BOOLEAN               IntrEn;                           /* Intr IN & OUT EPs en/dis flag.                       */
    CPU_INT16U                IntrInterval;                     /* Polling interval for intr IN & OUT EPs.              */
    USBD_VENDOR_REQ_FNCT      VendorReqCallbackPtr;             /* Ptr to app callback for vendor-specific req.         */
                                                                /* Ptrs to callback and extra arg used for async comm.  */
    USBD_VENDOR_ASYNC_FNCT    BulkRdAsyncFnct;
    void                     *BulkRdAsyncArgPtr;
    USBD_VENDOR_ASYNC_FNCT    BulkWrAsyncFnct;
    void                     *BulkWrAsyncArgPtr;
    USBD_VENDOR_ASYNC_FNCT    IntrRdAsyncFnct;
    void                     *IntrRdAsyncArgPtr;
    USBD_VENDOR_ASYNC_FNCT    IntrWrAsyncFnct;
    void                     *IntrWrAsyncArgPtr;

#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)                        /* Microsoft ext properties.                            */
    USBD_MS_OS_EXT_PROPERTY   MS_ExtPropertyTbl[USBD_VENDOR_CFG_MAX_NBR_MS_EXT_PROPERTY];
    CPU_INT08U                MS_ExtPropertyNext;
#endif
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
                                                                /* Vendor class instances array.                        */
static  USBD_VENDOR_CTRL  USBD_Vendor_CtrlTbl[USBD_VENDOR_CFG_MAX_NBR_DEV];
static  CPU_INT08U        USBD_Vendor_CtrlNbrNext;
                                                                /* Vendor class comm array.                             */
static  USBD_VENDOR_COMM  USBD_Vendor_CommTbl[USBD_VENDOR_COMM_NBR_MAX];
static  CPU_INT08U        USBD_Vendor_CommNbrNext;


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void         USBD_Vendor_Conn           (       CPU_INT08U       dev_nbr,
                                                        CPU_INT08U       cfg_nbr,
                                                        void            *p_if_class_arg);

static  void         USBD_Vendor_Disconn        (       CPU_INT08U       dev_nbr,
                                                        CPU_INT08U       cfg_nbr,
                                                        void            *p_if_class_arg);

static  CPU_BOOLEAN  USBD_Vendor_VendorReq      (       CPU_INT08U       dev_nbr,
                                                 const  USBD_SETUP_REQ  *p_setup_req,
                                                        void            *p_if_class_arg);

#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
static  CPU_INT08U   USBD_Vendor_MS_GetCompatID      (       CPU_INT08U                 dev_nbr,
                                                             CPU_INT08U                *p_sub_compat_id_ix);

static  CPU_INT08U   USBD_Vendor_MS_GetExtPropertyTbl(       CPU_INT08U                 dev_nbr,
                                                             USBD_MS_OS_EXT_PROPERTY  **pp_ext_property_tbl);
#endif

static  void         USBD_Vendor_RdAsyncCmpl         (       CPU_INT08U                 dev_nbr,
                                                             CPU_INT08U                 ep_addr,
                                                             void                      *p_buf,
                                                             CPU_INT32U                 buf_len,
                                                             CPU_INT32U                 xfer_len,
                                                             void                      *p_arg,
                                                             USBD_ERR                   err);

static  void         USBD_Vendor_WrAsyncCmpl         (       CPU_INT08U                 dev_nbr,
                                                             CPU_INT08U                 ep_addr,
                                                             void                      *p_buf,
                                                             CPU_INT32U                 buf_len,
                                                             CPU_INT32U                 xfer_len,
                                                             void                      *p_arg,
                                                             USBD_ERR                   err);

static  void         USBD_Vendor_IntrRdAsyncCmpl     (       CPU_INT08U                 dev_nbr,
                                                             CPU_INT08U                 ep_addr,
                                                             void                      *p_buf,
                                                             CPU_INT32U                 buf_len,
                                                             CPU_INT32U                 xfer_len,
                                                             void                      *p_arg,
                                                             USBD_ERR                   err);

static  void         USBD_Vendor_IntrWrAsyncCmpl     (       CPU_INT08U                 dev_nbr,
                                                             CPU_INT08U                 ep_addr,
                                                             void                      *p_buf,
                                                             CPU_INT32U                 buf_len,
                                                             CPU_INT32U                 xfer_len,
                                                             void                      *p_arg,
                                                             USBD_ERR                   err);


/*
*********************************************************************************************************
*                                         VENDOR CLASS DRIVER
*********************************************************************************************************
*/

static  USBD_CLASS_DRV  USBD_Vendor_Drv = {
    USBD_Vendor_Conn,
    USBD_Vendor_Disconn,
    DEF_NULL,                                                   /* Vendor does NOT use alternate interface(s).          */
    DEF_NULL,
    DEF_NULL,                                                   /* Vendor does NOT use functional EP desc.              */
    DEF_NULL,
    DEF_NULL,                                                   /* Vendor does NOT use functional IF desc.              */
    DEF_NULL,
    DEF_NULL,                                                   /* Vendor does NOT handle std req with IF recipient.    */
    DEF_NULL,                                                   /* Vendor does NOT define class-specific req.           */
    USBD_Vendor_VendorReq,

#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
    USBD_Vendor_MS_GetCompatID,
    USBD_Vendor_MS_GetExtPropertyTbl,
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
*                                         USBD_Vendor_Init()
*
* Description : Initialize internal structures and variables used by the Vendor class.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE   Vendor class successfully initialized.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_Vendor_Init (USBD_ERR  *p_err)
{
    CPU_INT08U         ix;
    USBD_VENDOR_CTRL  *p_ctrl;
    USBD_VENDOR_COMM  *p_comm;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

    for (ix = 0u; ix < USBD_VENDOR_CFG_MAX_NBR_DEV; ix++) {     /* Init vendor class struct.                            */
        p_ctrl                       = &USBD_Vendor_CtrlTbl[ix];
        p_ctrl->DevNbr               =  USBD_DEV_NBR_NONE;
        p_ctrl->State                =  USBD_VENDOR_STATE_NONE;
        p_ctrl->ClassNbr             =  USBD_CLASS_NBR_NONE;
        p_ctrl->CommPtr              = (USBD_VENDOR_COMM   *)0;
        p_ctrl->IntrEn               =  DEF_FALSE;
        p_ctrl->IntrInterval         =  0u;
        p_ctrl->VendorReqCallbackPtr = (USBD_VENDOR_REQ_FNCT)0;

        p_ctrl->BulkRdAsyncFnct      = (USBD_VENDOR_ASYNC_FNCT)0;
        p_ctrl->BulkRdAsyncArgPtr    = (void                 *)0;
        p_ctrl->BulkWrAsyncFnct      = (USBD_VENDOR_ASYNC_FNCT)0;
        p_ctrl->BulkWrAsyncArgPtr    = (void                 *)0;
        p_ctrl->IntrRdAsyncFnct      = (USBD_VENDOR_ASYNC_FNCT)0;
        p_ctrl->IntrRdAsyncArgPtr    = (void                 *)0;
        p_ctrl->IntrWrAsyncFnct      = (USBD_VENDOR_ASYNC_FNCT)0;
        p_ctrl->IntrWrAsyncArgPtr    = (void                 *)0;

#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
        Mem_Clr(p_ctrl->MS_ExtPropertyTbl,
                sizeof(p_ctrl->MS_ExtPropertyTbl));
        p_ctrl->MS_ExtPropertyNext = 0u;
#endif
    }

    for (ix = 0u; ix < USBD_VENDOR_COMM_NBR_MAX; ix++) {        /* Init vendor EP tbl.                                  */
        p_comm                        = &USBD_Vendor_CommTbl[ix];
        p_comm->CtrlPtr               = (USBD_VENDOR_CTRL     *)0;
        p_comm->DataBulkInEpAddr      =  USBD_EP_ADDR_NONE;
        p_comm->DataBulkOutEpAddr     =  USBD_EP_ADDR_NONE;
        p_comm->IntrInEpAddr          =  USBD_EP_ADDR_NONE;
        p_comm->IntrOutEpAddr         =  USBD_EP_ADDR_NONE;

        p_comm->DataBulkInActiveXfer  =  DEF_NO;
        p_comm->DataBulkOutActiveXfer =  DEF_NO;
        p_comm->IntrInActiveXfer      =  DEF_NO;
        p_comm->IntrOutActiveXfer     =  DEF_NO;
    }

    USBD_Vendor_CtrlNbrNext = 0u;
    USBD_Vendor_CommNbrNext = 0u;

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          USBD_Vendor_Add()
*
* Description : Add a new instance of the Vendor class.
*
* Argument(s) : intr_en         Interrupt endpoints IN and OUT flag:
*
*                                           DEF_TRUE    Pair of interrupt endpoints added to interface.
*                                           DEF_FALSE   Pair of interrupt endpoints not added to interface.
*
*               interval        Endpoint interval in milliseconds. It must be a power of 2.
*
*               req_callback    Vendor-specific request callback.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Vendor class instance successfully added.
*                               USBD_ERR_INVALID_ARG        Invalid argument(s) passed to 'intr_en'/'interval'.
*                               USBD_ERR_ALLOC              No more class instance structure available.
*
*
* Return(s)   : Class instance number, if NO error(s).
*
*               USBD_CLASS_NBR_NONE,   otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  USBD_Vendor_Add (CPU_BOOLEAN            intr_en,
                             CPU_INT16U             interval,
                             USBD_VENDOR_REQ_FNCT   req_callback,
                             USBD_ERR              *p_err)
{
    USBD_VENDOR_CTRL  *p_ctrl;
    CPU_INT08U         vendor_class_nbr;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }

    if (intr_en == DEF_TRUE) {
        if (MATH_IS_PWR2(interval) == DEF_NO) {                 /* Interval must be a power of 2.                       */
           *p_err = USBD_ERR_INVALID_ARG;
            return (USBD_CLASS_NBR_NONE);
        }
    }
#endif

    CPU_CRITICAL_ENTER();
    vendor_class_nbr = USBD_Vendor_CtrlNbrNext;                 /* Alloc new vendor class instance nbr.                 */
    if (vendor_class_nbr >= USBD_VENDOR_CFG_MAX_NBR_DEV) {      /* Chk if max nbr of instances reached.                 */
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_VENDOR_INSTANCE_ALLOC;
        return (USBD_CLASS_NBR_NONE);
    }
    USBD_Vendor_CtrlNbrNext++;                                  /* Next avail vendor class instance nbr.                */
    CPU_CRITICAL_EXIT();

    p_ctrl = &USBD_Vendor_CtrlTbl[vendor_class_nbr];            /* Get vendor class instance.                           */
                                                                /* Store vendor class instance info.                    */
    p_ctrl->IntrEn               =  intr_en;                    /* Intr EPs en/dis.                                     */
    p_ctrl->IntrInterval         =  interval;                   /* Polling interval for intr EPs.                       */
    p_ctrl->VendorReqCallbackPtr =  req_callback;               /* App callback for vendor-specific req.                */

   *p_err = USBD_ERR_NONE;
    return (vendor_class_nbr);
}


/*
*********************************************************************************************************
*                                        USBD_Vendor_CfgAdd()
*
* Description : Add Vendor class instance into the specified configuration (see Note #1).
*
* Argument(s) : class_nbr               Class instance number.
*
*               dev_nbr                 Device number.
*
*               cfg_nbr                 Configuration index to add Vendor class instance to.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Vendor class instance successfully added.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'/
*                                                                   'dev_nbr'.
*                               USBD_ERR_ALLOC                  No more class communication structure available.
*
*                                                               ---------- RETURNED BY USBD_IF_Add() : ----------
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_desc'/
*                                                                   'class_desc_size'.
*                               USBD_ERR_NULL_PTR               Argument 'p_class_drv'/'p_class_drv->Conn'/
*                                                                   'p_class_drv->Disconn' passed a NULL
*                                                               pointer.
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE      Invalid device state.
*                               USBD_ERR_CFG_INVALID_NBR        Invalid configuration number.
*                               USBD_ERR_IF_ALLOC               Interfaces                   NOT available.
*                               USBD_ERR_IF_ALT_ALLOC           Interface alternate settings NOT available.
*
*                                                               ---------- RETURNED BY USBD_BulkAdd() : ----------
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'max_pkt_len'.
*
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device        number.
*                               USBD_ERR_CFG_INVALID_NBR        Invalid configuration number.
*                               USBD_ERR_IF_INVALID_NBR         Invalid interface     number.
*                               USBD_ERR_EP_NONE_AVAIL          Physical endpoint NOT available.
*                               USBD_ERR_EP_ALLOC               Endpoints NOT available.
*
*                                                               ---------- RETURNED BY USBD_IntrAdd() : ----------
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'interval'/
*                                                                   'max_pkt_len'.
*
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device        number.
*                               USBD_ERR_CFG_INVALID_NBR        Invalid configuration number.
*                               USBD_ERR_IF_INVALID_NBR         Invalid interface     number.
*                               USBD_ERR_EP_NONE_AVAIL          Physical endpoint NOT available.
*                               USBD_ERR_EP_ALLOC               Endpoints NOT available.
*
* Return(s)   : none.
*
* Note(s)     : (1) Called several times, it allows to create multiple instances and multiple configurations.
*                   For instance, the following architecture could be created :
*
*                   HS
*                   |-- Configuration 0
*                       |-- Interface 0 (Vendor 0)
*                   |-- Configuration 1
*                       |-- Interface 0 (Vendor 0)
*                       |-- Interface 1 (Vendor 1)
*
*                   In that example, there are 2 instances of Vendor class: 'Vendor 0' and '1', and 2
*                   possible configurations: 'Configuration 0' and '1'. 'Configuration 1' is composed
*                   of 2 interfaces. Each class instance has an association with one of the interfaces.
*                   If 'Configuration 1' is activated by the host, it allows the host to access 2
*                   different functionalities offered by the device.
*
*               (2) Configuration Descriptor corresponding to a Vendor-specific device has the following
*                   format :
*
*                   Configuration Descriptor
*                   |-- Interface Descriptor (Vendor class)
*                       |-- Endpoint Descriptor (Bulk OUT)
*                       |-- Endpoint Descriptor (Bulk IN)
*                       |-- Endpoint Descriptor (Interrupt OUT) - optional
*                       |-- Endpoint Descriptor (Interrupt IN)  - optional
*********************************************************************************************************
*/

void  USBD_Vendor_CfgAdd (CPU_INT08U   class_nbr,
                          CPU_INT08U   dev_nbr,
                          CPU_INT08U   cfg_nbr,
                          USBD_ERR    *p_err)
{
    USBD_VENDOR_CTRL  *p_ctrl;
    USBD_VENDOR_COMM  *p_comm;
    CPU_INT08U         if_nbr;
    CPU_INT08U         ep_addr;
    CPU_INT16U         comm_nbr;
    CPU_INT16U         intr_interval;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

    if (class_nbr >= USBD_Vendor_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    p_ctrl = &USBD_Vendor_CtrlTbl[class_nbr];                   /* Get vendor class instance.                           */
    CPU_CRITICAL_ENTER();
    if ((p_ctrl->DevNbr != USBD_DEV_NBR_NONE) &&                /* Chk if class is associated with a different dev.     */
        (p_ctrl->DevNbr != dev_nbr)) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }

    p_ctrl->DevNbr = dev_nbr;

    comm_nbr = USBD_Vendor_CommNbrNext;                         /* Alloc new vendor class comm info nbr.                */
    if (comm_nbr >= USBD_VENDOR_COMM_NBR_MAX) {                 /* Chk if max nbr of comm instances reached.            */
        USBD_Vendor_CtrlNbrNext--;
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_VENDOR_INSTANCE_ALLOC;
        return;
    }
    USBD_Vendor_CommNbrNext++;                                  /* Next avail vendor class comm info nbr.               */
    CPU_CRITICAL_EXIT();

    p_comm = &USBD_Vendor_CommTbl[comm_nbr];                    /* Get vendor class comm info.                          */

                                                                /* -------------- CFG DESC CONSTRUCTION --------------- */
                                                                /* See Note #2.                                         */
                                                                /* Add vendor IF desc to cfg desc.                      */
    if_nbr = USBD_IF_Add(        dev_nbr,
                                 cfg_nbr,
                                &USBD_Vendor_Drv,
                         (void *)p_comm,                        /* EP comm struct associated to Vendor IF.              */
                         (void *)0,
                                 USBD_CLASS_CODE_VENDOR_SPECIFIC,
                                 USBD_SUBCLASS_CODE_VENDOR_SPECIFIC,
                                 USBD_PROTOCOL_CODE_VENDOR_SPECIFIC,
                                 "Vendor-specific class",
                                 p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }
                                                                /* Add bulk IN EP desc.                                 */
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

    p_comm->DataBulkInEpAddr = ep_addr;                         /* Store bulk IN EP addr.                               */

                                                                /* Add bulk OUT EP desc.                                */
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

    p_comm->DataBulkOutEpAddr = ep_addr;                        /* Store bulk OUT EP addr.                              */

    if (p_ctrl->IntrEn == DEF_TRUE) {

        if (DEF_BIT_IS_CLR(cfg_nbr, USBD_CFG_NBR_SPD_BIT) == DEF_YES) {
            intr_interval = p_ctrl->IntrInterval;               /* In FS, bInterval in frames.                          */
        } else {
            intr_interval = p_ctrl->IntrInterval * 8u;          /* In HS, bInterval in microframes.                     */
        }
                                                                /* Add intr IN EP desc.                                 */
        ep_addr = USBD_IntrAdd(dev_nbr,
                               cfg_nbr,
                               if_nbr,
                               0u,
                               DEF_YES,
                               0u,
                               intr_interval,
                               p_err);
        if (*p_err != USBD_ERR_NONE) {
            return;
        }

        p_comm->IntrInEpAddr = ep_addr;                         /* Store intr IN EP addr.                               */

                                                                /* Add intr OUT EP desc.                                */
        ep_addr = USBD_IntrAdd(dev_nbr,
                               cfg_nbr,
                               if_nbr,
                               0u,
                               DEF_NO,
                               0u,
                               intr_interval,
                               p_err);
        if (*p_err != USBD_ERR_NONE) {
            return;
        }

        p_comm->IntrOutEpAddr = ep_addr;                        /* Store intr OUT EP addr.                              */
    }
                                                                /* Store vendor class instance info.                    */
    CPU_CRITICAL_ENTER();
    p_ctrl->State                =  USBD_VENDOR_STATE_INIT;     /* Set class instance to init state.                    */
    p_ctrl->ClassNbr             =  class_nbr;
    p_ctrl->CommPtr              = (USBD_VENDOR_COMM *)0;
    CPU_CRITICAL_EXIT();

    p_comm->CtrlPtr = p_ctrl;                                   /* Save ref to vendor class instance ctrl struct.       */

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        USBD_Vendor_IsConn()
*
* Description : Get the vendor class connection state.
*
* Argument(s) : class_nbr   Class instance number.
*
* Return(s)   : DEF_YES, if Vendor class is connected.
*
*               DEF_NO,  otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  USBD_Vendor_IsConn (CPU_INT08U  class_nbr)
{
    USBD_VENDOR_CTRL  *p_ctrl;
    USBD_DEV_STATE     state;
    USBD_ERR           err;


                                                                /* ------------------- VALIDATE ARG ------------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (class_nbr >= USBD_Vendor_CtrlNbrNext) {
        return (DEF_NO);
    }
#endif

    p_ctrl = &USBD_Vendor_CtrlTbl[class_nbr];                   /* Get Vendor class instance ctrl struct.               */
    state  =  USBD_DevStateGet(p_ctrl->DevNbr, &err);           /* Get dev state.                                       */

    if ((err           == USBD_ERR_NONE            ) &&
        (state         == USBD_DEV_STATE_CONFIGURED) &&
        (p_ctrl->State == USBD_VENDOR_STATE_CFG    )) {
        return (DEF_YES);
    } else {
        return (DEF_NO);
    }
}


/*
*********************************************************************************************************
*                                   USBD_Vendor_MS_ExtPropertyAdd()
*
* Description : Add a Microsoft OS extended property to this vendor class instance.
*
* Argument(s) : class_nbr           Class instance number.
*
*               property_type       Property type. See Note (2).
*
*                           USBD_MS_OS_PROPERTY_TYPE_REG_SZ
*                           USBD_MS_OS_PROPERTY_TYPE_REG_EXPAND_SZ
*                           USBD_MS_OS_PROPERTY_TYPE_REG_BINARY
*                           USBD_MS_OS_PROPERTY_TYPE_REG_DWORD_LITTLE_ENDIAN
*                           USBD_MS_OS_PROPERTY_TYPE_REG_DWORD_BIG_ENDIAN
*                           USBD_MS_OS_PROPERTY_TYPE_REG_LINK
*                           USBD_MS_OS_PROPERTY_TYPE_REG_MULTI_SZ
*
*               p_property_name     Pointer to buffer that contains property name.
*
*               property_name_len   Length of property name in octets.
*
*               p_property          Pointer to buffer that contains property.
*
*               property_len        Length of property in octets.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   MS extended property successfully set.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr',
*                                                               'property_type'.
*                               USBD_ERR_NULL_PTR               Argument 'p_property_name', 'p_property'
*                                                               passed a NULL.
*
* Return(s)   : None.
*
* Note(s)     : (1) For more information on Microsoft OS descriptors, see
*                   'http://msdn.microsoft.com/en-us/library/windows/hardware/gg463179.aspx'.
*
*               (2) For more information on property types, refer to "Table 3. Property Data Types" of
*                   "Extended Properties OS Feature Descriptor Specification" document provided by
*                   Microsoft available at
*                   'http://msdn.microsoft.com/en-us/library/windows/hardware/gg463179.aspx'.
*********************************************************************************************************
*/
#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
void  USBD_Vendor_MS_ExtPropertyAdd (       CPU_INT08U   class_nbr,
                                            CPU_INT08U   property_type,
                                     const  CPU_INT08U  *p_property_name,
                                            CPU_INT16U   property_name_len,
                                     const  CPU_INT08U  *p_property,
                                            CPU_INT32U   property_len,
                                            USBD_ERR    *p_err)
{
    CPU_INT08U                ext_property_nbr;
    USBD_VENDOR_CTRL         *p_ctrl;
    USBD_MS_OS_EXT_PROPERTY  *p_ext_property;
    CPU_SR_ALLOC();


                                                                    /* ------------------- VALIDATE ARG ------------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == (USBD_ERR *)0) {                                   /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }

    if (class_nbr >= USBD_Vendor_CtrlNbrNext) {
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }

    if ((property_type != USBD_MS_OS_PROPERTY_TYPE_REG_SZ                 ) &&
        (property_type != USBD_MS_OS_PROPERTY_TYPE_REG_EXPAND_SZ          ) &&
        (property_type != USBD_MS_OS_PROPERTY_TYPE_REG_BINARY             ) &&
        (property_type != USBD_MS_OS_PROPERTY_TYPE_REG_DWORD_LITTLE_ENDIAN) &&
        (property_type != USBD_MS_OS_PROPERTY_TYPE_REG_DWORD_BIG_ENDIAN   ) &&
        (property_type != USBD_MS_OS_PROPERTY_TYPE_REG_LINK               ) &&
        (property_type != USBD_MS_OS_PROPERTY_TYPE_REG_MULTI_SZ)) {
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }

    if ((p_property_name   == (const  CPU_INT08U *)0) &&
        (property_name_len !=  0u)) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }

    if ((p_property   == (const  CPU_INT08U *)0) &&
        (property_len !=  0u)) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }
#endif

    p_ctrl = &USBD_Vendor_CtrlTbl[class_nbr];                   /* Get Vendor class instance ctrl struct.               */

    CPU_CRITICAL_ENTER();
    ext_property_nbr = p_ctrl->MS_ExtPropertyNext;
    if (ext_property_nbr >= USBD_VENDOR_CFG_MAX_NBR_MS_EXT_PROPERTY) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_ALLOC;
        return;
    }
    p_ctrl->MS_ExtPropertyNext++;

    p_ext_property = &p_ctrl->MS_ExtPropertyTbl[ext_property_nbr];

    p_ext_property->PropertyType    = property_type;
    p_ext_property->PropertyNamePtr = p_property_name;
    p_ext_property->PropertyNameLen = property_name_len;
    p_ext_property->PropertyPtr     = p_property;
    p_ext_property->PropertyLen     = property_len;
    CPU_CRITICAL_EXIT();

   *p_err = USBD_ERR_NONE;
}
#endif

/*
*********************************************************************************************************
*                                          USBD_Vendor_Rd()
*
* Description : Receive data from host through Bulk OUT endpoint. This function is blocking.
*
* Argument(s) : class_nbr   Class instance number.
*
*               p_buf       Pointer to receive buffer.
*
*               buf_len     Receive buffer length in octets.
*
*               timeout     Timeout in milliseconds.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Data successfully received from host.
*                               USBD_ERR_NULL_PTR               Argument 'p_buf' passed a NULL pointer.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'.
*                               USBD_ERR_INVALID_CLASS_STATE    Invalid class state.
*
*                                                               ------ RETURNED BY USBD_BulkRx() : ------
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_EP_INVALID_NBR         Invalid endpoint number.
*                               USBD_ERR_DEV_INVALID_STATE      Bulk transfer can ONLY be used after the
*                                                                   device is in configured state.
*                               USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*
* Return(s)   : Number of octets received, if NO error(s).
*
*               0,                         otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT32U  USBD_Vendor_Rd (CPU_INT08U   class_nbr,
                            void        *p_buf,
                            CPU_INT32U   buf_len,
                            CPU_INT16U   timeout,
                            USBD_ERR    *p_err)
{
    USBD_VENDOR_CTRL  *p_ctrl;
    CPU_INT32U         xfer_len;
    CPU_BOOLEAN        conn;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }

    if ((p_buf   == (void *)0) &&
        (buf_len !=         0)) {
       *p_err = USBD_ERR_NULL_PTR;
        return (0u);
    }
#endif

    if (class_nbr >= USBD_Vendor_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (0u);
    }

    conn = USBD_Vendor_IsConn(class_nbr);
    if (conn != DEF_YES) {                                      /* Chk class state.                                     */
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return (0u);
    }

    p_ctrl = &USBD_Vendor_CtrlTbl[class_nbr];                   /* Get Vendor class instance ctrl struct.               */

    xfer_len = USBD_BulkRx(p_ctrl->DevNbr,
                           p_ctrl->CommPtr->DataBulkOutEpAddr,
                           p_buf,
                           buf_len,
                           timeout,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (0u);
    }

    return (xfer_len);
}


/*
*********************************************************************************************************
*                                          USBD_Vendor_Wr()
*
* Description : Send data to host through Bulk IN endpoint. This function is blocking.
*
* Argument(s) : class_nbr   Class instance number.
*
*               p_buf       Pointer to transmit buffer.
*
*               buf_len     Transmit buffer length in octets.
*
*               timeout     Timeout in milliseconds.
*
*               end         End-of-transfer flag (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Data successfully sent to host.
*                               USBD_ERR_NULL_PTR               Argument 'p_buf' passed a NULL pointer.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'.
*                               USBD_ERR_INVALID_CLASS_STATE    Invalid class state.
*
*                                                               ------ RETURNED BY USBD_BulkTx() : ------
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_EP_INVALID_NBR         Invalid endpoint number.
*                               USBD_ERR_DEV_INVALID_STATE      Bulk transfer can ONLY be used after the
*                                                                   device is in configured state.
*                               USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*
* Return(s)   : Number of octets sent, if NO error(s).
*
*               0,                     otherwise.
*
* Note(s)     : (1) If end-of-transfer is set and transfer length is multiple of maximum packet size,
*                   a zero-length packet is transferred to indicate the end of transfer to the host.
*********************************************************************************************************
*/

CPU_INT32U  USBD_Vendor_Wr (CPU_INT08U    class_nbr,
                            void         *p_buf,
                            CPU_INT32U    buf_len,
                            CPU_INT16U    timeout,
                            CPU_BOOLEAN   end,
                            USBD_ERR     *p_err)
{
    USBD_VENDOR_CTRL  *p_ctrl;
    CPU_INT32U         xfer_len;
    CPU_BOOLEAN        conn;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }

    if ((p_buf   == (void *)0) &&
        (buf_len !=         0)) {
       *p_err = USBD_ERR_NULL_PTR;
        return (0u);
    }
#endif

    if (class_nbr >= USBD_Vendor_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (0u);
    }

    conn = USBD_Vendor_IsConn(class_nbr);
    if (conn != DEF_YES) {                                      /* Chk class state.                                     */
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return (0u);
    }

    p_ctrl = &USBD_Vendor_CtrlTbl[class_nbr];                   /* Get Vendor class instance ctrl struct.               */

    xfer_len = USBD_BulkTx(p_ctrl->DevNbr,
                           p_ctrl->CommPtr->DataBulkInEpAddr,
                           p_buf,
                           buf_len,
                           timeout,
                           end,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (0u);
    }

    return (xfer_len);
}


/*
*********************************************************************************************************
*                                        USBD_Vendor_RdAsync()
*
* Description : Receive data from host through Bulk OUT endpoint. This function is non-blocking.
*               It returns immediately after transfer preparation. Upon transfer completion, a
*               callback provided by the application will be called to finalize the transfer.
*
* Argument(s) : class_nbr       Class instance number.
*
*               p_buf           Pointer to receive buffer.
*
*               buf_len         Receive buffer length in octets.
*
*               async_fnct      Receive callback.
*
*               p_async_arg     Additional argument provided by application for receive callback.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Transfer successfully prepared.
*                               USBD_ERR_NULL_PTR               Argument 'p_buf' passed a NULL pointer.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'.
*                               USBD_ERR_INVALID_CLASS_STATE    Invalid class state.
*
*                                                               --- RETURNED BY USBD_BulkRxAsync() : ---
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_EP_INVALID_NBR         Invalid endpoint number.
*                               USBD_ERR_DEV_INVALID_STATE      Bulk transfer can ONLY be used after the
*                                                                   device is in configured state.
*                               USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*                               USBD_ERR_EP_INVALID_STATE       Endpoint not opened.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_Vendor_RdAsync (CPU_INT08U               class_nbr,
                           void                    *p_buf,
                           CPU_INT32U               buf_len,
                           USBD_VENDOR_ASYNC_FNCT   async_fnct,
                           void                    *p_async_arg,
                           USBD_ERR                *p_err)
{
    USBD_VENDOR_CTRL  *p_ctrl;
    CPU_BOOLEAN        conn;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }

    if ((p_buf   == (void *)0) &&
        (buf_len !=         0)) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }
#endif

    if (class_nbr >= USBD_Vendor_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    conn = USBD_Vendor_IsConn(class_nbr);
    if (conn != DEF_YES) {                                      /* Chk class state.                                     */
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return;
    }

    p_ctrl = &USBD_Vendor_CtrlTbl[class_nbr];                   /* Get Vendor class instance ctrl struct.               */

    p_ctrl->BulkRdAsyncFnct   = async_fnct;
    p_ctrl->BulkRdAsyncArgPtr = p_async_arg;

    if (p_ctrl->CommPtr->DataBulkOutActiveXfer == DEF_NO) {     /* Check if another xfer is already in progress.        */
        p_ctrl->CommPtr->DataBulkOutActiveXfer = DEF_YES;       /* Indicate that a xfer is in progres.                  */
        USBD_BulkRxAsync(        p_ctrl->DevNbr,
                                 p_ctrl->CommPtr->DataBulkOutEpAddr,
                                 p_buf,
                                 buf_len,
                                 USBD_Vendor_RdAsyncCmpl,
                         (void *)p_ctrl,
                                 p_err);
        if (*p_err != USBD_ERR_NONE) {
            p_ctrl->CommPtr->DataBulkOutActiveXfer = DEF_NO;
        }
    } else {
       *p_err = USBD_ERR_CLASS_XFER_IN_PROGRESS;
    }
}


/*
*********************************************************************************************************
*                                        USBD_Vendor_WrAsync()
*
* Description : Send data to host through Bulk IN endpoint. This function is non-blocking.
*               It returns immediately after transfer preparation. Upon transfer completion, a
*               callback provided by the application will be called to finalize the transfer.
*
* Argument(s) : class_nbr       Class instance number.
*
*               p_buf           Pointer to transmit buffer.
*
*               buf_len         Transmit buffer length in octets.
*
*               async_fnct      Transmit callback.
*
*               p_async_arg     Additional argument provided by application for transmit callback.
*
*               end             End-of-transfer flag (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Transfer successfully prepared.
*                               USBD_ERR_NULL_PTR               Argument 'p_buf' passed a NULL pointer.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'.
*                               USBD_ERR_INVALID_CLASS_STATE    Invalid class state.
*
*                                                               --- RETURNED BY USBD_BulkTxAsync() : ---
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_EP_INVALID_NBR         Invalid endpoint number.
*                               USBD_ERR_DEV_INVALID_STATE      Bulk transfer can ONLY be used after the
*                                                                   device is in configured state.
*                               USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*                               USBD_ERR_EP_INVALID_STATE       Endpoint not opened.
*
* Return(s)   : none.
*
* Note(s)     : (1) If end-of-transfer is set and transfer length is multiple of maximum packet size,
*                   a zero-length packet is transferred to indicate the end of transfer to the host.
*********************************************************************************************************
*/

void  USBD_Vendor_WrAsync (CPU_INT08U               class_nbr,
                           void                    *p_buf,
                           CPU_INT32U               buf_len,
                           USBD_VENDOR_ASYNC_FNCT   async_fnct,
                           void                    *p_async_arg,
                           CPU_BOOLEAN              end,
                           USBD_ERR                *p_err)
{
    USBD_VENDOR_CTRL  *p_ctrl;
    CPU_BOOLEAN        conn;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }

    if ((p_buf   == (void *)0) &&
        (buf_len !=         0)) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }
#endif

    if (class_nbr >= USBD_Vendor_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    conn = USBD_Vendor_IsConn(class_nbr);
    if (conn != DEF_YES) {                                      /* Chk class state.                                     */
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return;
    }

    p_ctrl = &USBD_Vendor_CtrlTbl[class_nbr];                   /* Get Vendor class instance ctrl struct.               */

    p_ctrl->BulkWrAsyncFnct   =   async_fnct;
    p_ctrl->BulkWrAsyncArgPtr = p_async_arg;

    if (p_ctrl->CommPtr->DataBulkInActiveXfer == DEF_NO) {      /* Check if another xfer is already in progress.        */
        p_ctrl->CommPtr->DataBulkInActiveXfer = DEF_YES;        /* Indicate that a xfer is in progres.                  */
        USBD_BulkTxAsync(        p_ctrl->DevNbr,
                                 p_ctrl->CommPtr->DataBulkInEpAddr,
                                 p_buf,
                                 buf_len,
                                 USBD_Vendor_WrAsyncCmpl,
                         (void *)p_ctrl,
                                 end,
                                 p_err);
        if (*p_err != USBD_ERR_NONE) {
            p_ctrl->CommPtr->DataBulkInActiveXfer = DEF_NO;
        }
    } else {
       *p_err = USBD_ERR_CLASS_XFER_IN_PROGRESS;
    }
}


/*
*********************************************************************************************************
*                                        USBD_Vendor_IntrRd()
*
* Description : Receive data from host through Interrupt OUT endpoint. This function is blocking.
*
* Argument(s) : class_nbr   Class instance number.
*
*               p_buf       Pointer to receive buffer.
*
*               buf_len     Receive buffer length in octets.
*
*               timeout     Timeout in milliseconds.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Data successfully received from host.
*                               USBD_ERR_NULL_PTR               Argument 'p_buf' passed a NULL pointer.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'.
*                               USBD_ERR_INVALID_CLASS_STATE    Invalid class state.
*
*                                                               ------ RETURNED BY USBD_IntrRx() : ------
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_EP_INVALID_NBR         Invalid endpoint number.
*                               USBD_ERR_DEV_INVALID_STATE      Bulk transfer can ONLY be used after the
*                                                                   device is in configured state.
*                               USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*
* Return(s)   : Number of octets received, if NO error(s).
*
*               0,                         otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT32U  USBD_Vendor_IntrRd (CPU_INT08U   class_nbr,
                                void        *p_buf,
                                CPU_INT32U   buf_len,
                                CPU_INT16U   timeout,
                                USBD_ERR    *p_err)
{
    USBD_VENDOR_CTRL  *p_ctrl;
    CPU_INT32U         xfer_len;
    CPU_BOOLEAN        conn;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }

    if ((p_buf   == (void *)0) &&
        (buf_len !=         0)) {
       *p_err = USBD_ERR_NULL_PTR;
        return (0u);
    }
#endif

    if (class_nbr >= USBD_Vendor_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (0u);
    }

    conn = USBD_Vendor_IsConn(class_nbr);
    if (conn != DEF_YES) {                                      /* Chk class state.                                     */
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return (0u);
    }

    p_ctrl = &USBD_Vendor_CtrlTbl[class_nbr];                   /* Get Vendor class instance ctrl struct.               */

    xfer_len = USBD_IntrRx(p_ctrl->DevNbr,
                           p_ctrl->CommPtr->IntrOutEpAddr,
                           p_buf,
                           buf_len,
                           timeout,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (0u);
    }

    return (xfer_len);
}


/*
*********************************************************************************************************
*                                        USBD_Vendor_IntrWr()
*
* Description : Send data to host through Interrupt IN endpoint. This function is blocking.
*
* Argument(s) : class_nbr   Class instance number.
*
*               p_buf       Pointer to transmit buffer.
*
*               buf_len     Transmit buffer length in octets.
*
*               timeout     Timeout in milliseconds.
*
*               end         End-of-transfer flag (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Data successfully sent to host.
*                               USBD_ERR_NULL_PTR               Argument 'p_buf' passed a NULL pointer.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'.
*                               USBD_ERR_INVALID_CLASS_STATE    Invalid class state.
*
*                                                               ------ RETURNED BY USBD_IntrTx() : ------
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_EP_INVALID_NBR         Invalid endpoint number.
*                               USBD_ERR_DEV_INVALID_STATE      Bulk transfer can ONLY be used after the
*                                                                   device is in configured state.
*                               USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*
* Return(s)   : Number of octets sent, if NO error(s).
*
*               0,                     otherwise.
*
* Note(s)     : (1) If end-of-transfer is set and transfer length is multiple of maximum packet size,
*                   a zero-length packet is transferred to indicate the end of transfer to the host.
*********************************************************************************************************
*/

CPU_INT32U  USBD_Vendor_IntrWr (CPU_INT08U    class_nbr,
                                void         *p_buf,
                                CPU_INT32U    buf_len,
                                CPU_INT16U    timeout,
                                CPU_BOOLEAN   end,
                                USBD_ERR     *p_err)
{
    USBD_VENDOR_CTRL  *p_ctrl;
    CPU_INT32U         xfer_len;
    CPU_BOOLEAN        conn;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }

    if ((p_buf   == (void *)0) &&
        (buf_len !=         0)) {
       *p_err = USBD_ERR_NULL_PTR;
        return (0u);
    }
#endif

    if (class_nbr >= USBD_Vendor_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (0u);
    }

    conn = USBD_Vendor_IsConn(class_nbr);
    if (conn != DEF_YES) {                                      /* Chk class state.                                     */
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return (0u);
    }

    p_ctrl = &USBD_Vendor_CtrlTbl[class_nbr];                   /* Get Vendor class instance ctrl struct.               */

    xfer_len = USBD_IntrTx(p_ctrl->DevNbr,
                           p_ctrl->CommPtr->IntrInEpAddr,
                           p_buf,
                           buf_len,
                           timeout,
                           end,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (0u);
    }

    return (xfer_len);
}


/*
*********************************************************************************************************
*                                      USBD_Vendor_IntrRdAsync()
*
* Description : Receive data from host through Interrupt OUT endpoint. This function is non-blocking.
*               It returns immediately after transfer preparation. Upon transfer completion, a
*               callback provided by the application will be called to finalize the transfer.
*
* Argument(s) : class_nbr       Class instance number.
*
*               p_buf           Pointer to receive buffer.
*
*               buf_len         Receive buffer length in octets.
*
*               async_fnct      Receive callback.
*
*               p_async_arg     Additional argument provided by application for receive callback.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Transfer successfully prepared.
*                               USBD_ERR_NULL_PTR               Argument 'p_buf' passed a NULL pointer.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'.
*                               USBD_ERR_INVALID_CLASS_STATE    Invalid class state.
*
*                                                               --- RETURNED BY USBD_IntrRxAsync() : ---
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_EP_INVALID_NBR         Invalid endpoint number.
*                               USBD_ERR_DEV_INVALID_STATE      Bulk transfer can ONLY be used after the
*                                                               device is in configured state.
*                               USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*                               USBD_ERR_EP_INVALID_STATE       Endpoint not opened.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_Vendor_IntrRdAsync (CPU_INT08U               class_nbr,
                               void                    *p_buf,
                               CPU_INT32U               buf_len,
                               USBD_VENDOR_ASYNC_FNCT   async_fnct,
                               void                    *p_async_arg,
                               USBD_ERR                *p_err)
{
    USBD_VENDOR_CTRL  *p_ctrl;
    CPU_BOOLEAN        conn;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }

    if ((p_buf   == (void *)0) &&
        (buf_len !=         0)) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }
#endif

    if (class_nbr >= USBD_Vendor_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    conn = USBD_Vendor_IsConn(class_nbr);
    if (conn != DEF_YES) {                                      /* Chk class state.                                     */
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return;
    }

    p_ctrl = &USBD_Vendor_CtrlTbl[class_nbr];                   /* Get Vendor class instance ctrl struct.               */

    p_ctrl->IntrRdAsyncFnct   =   async_fnct;
    p_ctrl->IntrRdAsyncArgPtr = p_async_arg;

    if (p_ctrl->CommPtr->IntrOutActiveXfer == DEF_NO) {         /* Check if another xfer is already in progress.        */
        p_ctrl->CommPtr->IntrOutActiveXfer = DEF_YES;           /* Indicate that a xfer is in progres.                  */
        USBD_IntrRxAsync(        p_ctrl->DevNbr,
                                 p_ctrl->CommPtr->IntrOutEpAddr,
                                 p_buf,
                                 buf_len,
                                 USBD_Vendor_IntrRdAsyncCmpl,
                         (void *)p_ctrl,
                                 p_err);
        if (*p_err != USBD_ERR_NONE) {
            p_ctrl->CommPtr->IntrOutActiveXfer = DEF_NO;
        }
    } else {
       *p_err = USBD_ERR_CLASS_XFER_IN_PROGRESS;
    }
}


/*
*********************************************************************************************************
*                                      USBD_Vendor_IntrWrAsync()
*
* Description : Send data to host through Interrupt IN endpoint. This function is non-blocking.
*               It returns immediately after transfer preparation. Upon transfer completion, a
*               callback provided by the application will be called to finalize the transfer.
*
* Argument(s) : class_nbr       Class instance number.
*
*               p_buf           Pointer to transmit buffer.
*
*               buf_len         Transmit buffer length in octets.
*
*               async_fnct      Transmit callback.
*
*               p_async_arg     Additional argument provided by application for transmit callback.
*
*               end             End-of-transfer flag (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Transfer successfully prepared.
*                               USBD_ERR_NULL_PTR               Argument 'p_buf' passed a NULL pointer.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'.
*                               USBD_ERR_INVALID_CLASS_STATE    Invalid class state.
*
*                                                               --- RETURNED BY USBD_IntrTxAsync() : ---
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_EP_INVALID_NBR         Invalid endpoint number.
*                               USBD_ERR_DEV_INVALID_STATE      Bulk transfer can ONLY be used after the
*                                                                   device is in configured state.
*                               USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*                               USBD_ERR_EP_INVALID_STATE       Endpoint not opened.
*
* Return(s)   : none.
*
* Note(s)     : (1) If end-of-transfer is set and transfer length is multiple of maximum packet size,
*                   a zero-length packet is transferred to indicate the end of transfer to the host.
*********************************************************************************************************
*/

void  USBD_Vendor_IntrWrAsync (CPU_INT08U               class_nbr,
                               void                    *p_buf,
                               CPU_INT32U               buf_len,
                               USBD_VENDOR_ASYNC_FNCT   async_fnct,
                               void                    *p_async_arg,
                               CPU_BOOLEAN              end,
                               USBD_ERR                *p_err)
{
    USBD_VENDOR_CTRL  *p_ctrl;
    CPU_BOOLEAN        conn;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }

    if ((p_buf   == (void *)0) &&
        (buf_len !=         0)) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }
#endif

    if (class_nbr >= USBD_Vendor_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    conn = USBD_Vendor_IsConn(class_nbr);
    if (conn != DEF_YES) {                                      /* Chk class state.                                     */
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return;
    }

    p_ctrl = &USBD_Vendor_CtrlTbl[class_nbr];                   /* Get Vendor class instance ctrl struct.               */

    p_ctrl->IntrWrAsyncFnct   =   async_fnct;
    p_ctrl->IntrWrAsyncArgPtr = p_async_arg;

    if (p_ctrl->CommPtr->IntrInActiveXfer == DEF_NO) {          /* Check if another xfer is already in progress.        */
        p_ctrl->CommPtr->IntrInActiveXfer = DEF_YES;            /* Indicate that a xfer is in progres.                  */
        USBD_IntrTxAsync(        p_ctrl->DevNbr,
                                 p_ctrl->CommPtr->IntrInEpAddr,
                                 p_buf,
                                 buf_len,
                                 USBD_Vendor_IntrWrAsyncCmpl,
                         (void *)p_ctrl,
                                 end,
                                 p_err);
        if (*p_err != USBD_ERR_NONE) {
            p_ctrl->CommPtr->IntrInActiveXfer = DEF_NO;
        }
    } else {
       *p_err = USBD_ERR_CLASS_XFER_IN_PROGRESS;
    }
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
*                                         USBD_Vendor_Conn()
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

static  void  USBD_Vendor_Conn (CPU_INT08U   dev_nbr,
                                CPU_INT08U   cfg_nbr,
                                void        *p_if_class_arg)
{
    USBD_VENDOR_COMM  *p_comm;
    CPU_SR_ALLOC();


    (void)dev_nbr;
    (void)cfg_nbr;

    p_comm = (USBD_VENDOR_COMM *)p_if_class_arg;
    CPU_CRITICAL_ENTER();
    p_comm->CtrlPtr->CommPtr = p_comm;
    p_comm->CtrlPtr->State   = USBD_VENDOR_STATE_CFG;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                        USBD_Vendor_Disconn()
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

static  void  USBD_Vendor_Disconn (CPU_INT08U   dev_nbr,
                                   CPU_INT08U   cfg_nbr,
                                   void        *p_if_class_arg)
{
    USBD_VENDOR_COMM  *p_comm;
    CPU_SR_ALLOC();


    (void)dev_nbr;
    (void)cfg_nbr;

    p_comm = (USBD_VENDOR_COMM *)p_if_class_arg;
    CPU_CRITICAL_ENTER();
    p_comm->CtrlPtr->CommPtr = (USBD_VENDOR_COMM *)0;
    p_comm->CtrlPtr->State   =  USBD_VENDOR_STATE_INIT;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                       USBD_Vendor_VendorReq()
*
* Description : Process vendor-specific request.
*
* Argument(s) : dev_nbr         Device number.
*
*               p_setup_req     Pointer to setup request structure.
*
*               p_if_class_arg  Pointer to class argument passed to USBD_IF_Add().
*
* Return(s)   : DEF_OK,   if vendor-specific request successfully processed.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Vendor_VendorReq (       CPU_INT08U       dev_nbr,
                                            const  USBD_SETUP_REQ  *p_setup_req,
                                                   void            *p_if_class_arg)
{
    USBD_VENDOR_COMM  *p_comm;
    CPU_BOOLEAN        valid;


    (void)dev_nbr;

    p_comm = (USBD_VENDOR_COMM *)p_if_class_arg;
                                                                /* Process req if callback avail.                       */
    if (p_comm->CtrlPtr->VendorReqCallbackPtr != (USBD_VENDOR_REQ_FNCT)0) {
        valid = p_comm->CtrlPtr->VendorReqCallbackPtr(p_comm->CtrlPtr->ClassNbr,
                                                      p_comm->CtrlPtr->DevNbr,
                                                      p_setup_req);
    } else {
        valid = DEF_FAIL;
    }

    return (valid);
}


/*
*********************************************************************************************************
*                                    USBD_Vendor_MS_GetCompatID()
*
* Description : Returns Microsoft descriptor compatible ID.
*
* Argument(s) : dev_nbr                 Device number.
*
*               p_sub_compat_id_ix      Pointer to variable that will receive subcompatible ID.
*
* Return(s)   : Compatible ID.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : None.
*********************************************************************************************************
*/
#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
static  CPU_INT08U  USBD_Vendor_MS_GetCompatID (CPU_INT08U   dev_nbr,
                                                CPU_INT08U  *p_sub_compat_id_ix)
{
     (void)dev_nbr;

    *p_sub_compat_id_ix = USBD_MS_OS_SUBCOMPAT_ID_NULL;

     return (USBD_MS_OS_COMPAT_ID_WINUSB);
}
#endif

/*
*********************************************************************************************************
*                                USBD_Vendor_MS_GetExtPropertyTbl()
*
* Description : Returns Microsoft descriptor extended properties table.
*
* Argument(s) : dev_nbr                 Device number.
*
*               pp_ext_property_tbl     Pointer to variable that will receive the Microsoft extended
*                                       properties table.
*
* Return(s)   : Number of Microsoft extended properties in table.
*
* Note(s)     : None.
*********************************************************************************************************
*/
#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
static  CPU_INT08U  USBD_Vendor_MS_GetExtPropertyTbl (CPU_INT08U                 dev_nbr,
                                                      USBD_MS_OS_EXT_PROPERTY  **pp_ext_property_tbl)
{
    USBD_VENDOR_CTRL  *p_ctrl;


    p_ctrl              = &USBD_Vendor_CtrlTbl[dev_nbr];        /* Get Vendor class instance ctrl struct.               */
   *pp_ext_property_tbl =  p_ctrl->MS_ExtPropertyTbl;

    return (p_ctrl->MS_ExtPropertyNext);                        /* Only one extended property (GUID) supported.         */
}
#endif

/*
*********************************************************************************************************
*                                      USBD_Vendor_RdAsyncCmpl()
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
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_Vendor_RdAsyncCmpl (CPU_INT08U   dev_nbr,
                                       CPU_INT08U   ep_addr,
                                       void        *p_buf,
                                       CPU_INT32U   buf_len,
                                       CPU_INT32U   xfer_len,
                                       void        *p_arg,
                                       USBD_ERR     err)
{
    USBD_VENDOR_CTRL  *p_ctrl;
    void              *p_callback_arg;


    (void)dev_nbr;
    (void)ep_addr;

    p_ctrl         = (USBD_VENDOR_CTRL *)p_arg;                 /* Get Vendor class instance ctrl struct.               */
    p_callback_arg =  p_ctrl->BulkRdAsyncArgPtr;

    p_ctrl->CommPtr->DataBulkOutActiveXfer = DEF_NO;            /* Xfer finished, no more active xfer.                  */
    p_ctrl->BulkRdAsyncFnct(p_ctrl->ClassNbr,                   /* Call app callback to inform about xfer completion.   */
                            p_buf,
                            buf_len,
                            xfer_len,
                            p_callback_arg,
                            err);
}


/*
*********************************************************************************************************
*                                      USBD_Vendor_WrAsyncCmpl()
*
* Description : Inform the application about the Bulk IN transfer completion.
*
* Argument(s) : dev_nbr          Device number
*
*               ep_addr          Endpoint address.
*
*               p_buf            Pointer to the transmit buffer.
*
*               buf_len          Transmit buffer length.
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

static  void  USBD_Vendor_WrAsyncCmpl (CPU_INT08U   dev_nbr,
                                       CPU_INT08U   ep_addr,
                                       void        *p_buf,
                                       CPU_INT32U   buf_len,
                                       CPU_INT32U   xfer_len,
                                       void        *p_arg,
                                       USBD_ERR     err)
{
    USBD_VENDOR_CTRL  *p_ctrl;
    void              *p_callback_arg;


    (void)dev_nbr;
    (void)ep_addr;

    p_ctrl         = (USBD_VENDOR_CTRL *)p_arg;                 /* Get Vendor class instance ctrl struct.               */
    p_callback_arg =  p_ctrl->BulkWrAsyncArgPtr;

    p_ctrl->CommPtr->DataBulkInActiveXfer = DEF_NO;             /* Xfer finished, no more active xfer.                  */
    p_ctrl->BulkWrAsyncFnct(p_ctrl->ClassNbr,                   /* Call app callback to inform about xfer completion.   */
                            p_buf,
                            buf_len,
                            xfer_len,
                            p_callback_arg,
                            err);
}


/*
*********************************************************************************************************
*                                    USBD_Vendor_IntrRdAsyncCmpl()
*
* Description : Inform the application about the Interrupt OUT transfer completion.
*
* Argument(s) : dev_nbr          Device number.
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
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_Vendor_IntrRdAsyncCmpl (CPU_INT08U   dev_nbr,
                                           CPU_INT08U   ep_addr,
                                           void        *p_buf,
                                           CPU_INT32U   buf_len,
                                           CPU_INT32U   xfer_len,
                                           void        *p_arg,
                                           USBD_ERR     err)
{
    USBD_VENDOR_CTRL  *p_ctrl;
    void              *p_callback_arg;


    (void)dev_nbr;
    (void)ep_addr;

    p_ctrl         = (USBD_VENDOR_CTRL *)p_arg;                 /* Get Vendor class instance ctrl struct.               */
    p_callback_arg =  p_ctrl->IntrRdAsyncArgPtr;

    p_ctrl->CommPtr->IntrOutActiveXfer = DEF_NO;                /* Xfer finished, no more active xfer.                  */
    p_ctrl->IntrRdAsyncFnct(p_ctrl->ClassNbr,                   /* Call app callback to inform about xfer completion.   */
                            p_buf,
                            buf_len,
                            xfer_len,
                            p_callback_arg,
                            err);
}


/*
*********************************************************************************************************
*                                    USBD_Vendor_IntrWrAsyncCmpl()
*
* Description : Inform the application about the Interrupt IN transfer completion.
*
* Argument(s) : dev_nbr          Device number.
*
*               ep_addr          Endpoint address.
*
*               p_buf            Pointer to the transmit buffer.
*
*               buf_len          Transmit buffer length.
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

static  void  USBD_Vendor_IntrWrAsyncCmpl (CPU_INT08U   dev_nbr,
                                           CPU_INT08U   ep_addr,
                                           void        *p_buf,
                                           CPU_INT32U   buf_len,
                                           CPU_INT32U   xfer_len,
                                           void        *p_arg,
                                           USBD_ERR     err)
{
    USBD_VENDOR_CTRL  *p_ctrl;
    void              *p_callback_arg;


    (void)dev_nbr;
    (void)ep_addr;

    p_ctrl         = (USBD_VENDOR_CTRL *)p_arg;                 /* Get Vendor class instance ctrl struct.               */
    p_callback_arg =  p_ctrl->IntrWrAsyncArgPtr;

    p_ctrl->CommPtr->IntrInActiveXfer = DEF_NO;                 /* Xfer finished, no more active xfer.                  */
    p_ctrl->IntrWrAsyncFnct(p_ctrl->ClassNbr,                   /* Call app callback to inform about xfer completion.   */
                            p_buf,
                            buf_len,
                            xfer_len,
                            p_callback_arg,
                            err);
}


