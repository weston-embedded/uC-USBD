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
*                                        USB DEVICE HID CLASS
*
* Filename : usbd_hid.c
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    USBD_HID_MODULE
#include  "usbd_hid.h"
#include  "usbd_hid_os.h"
#include  <lib_math.h>


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

                                                                /* Max nbr of comm struct.                              */
#define  USBD_HID_MAX_NBR_COMM                  USBD_HID_CFG_MAX_NBR_CFG

#define  USBD_HID_CTRL_REQ_TIMEOUT_mS                  5000u


/*
*********************************************************************************************************
*                                    CLASS-SPECIFIC LOCAL DEFINES
*
* Note(s) : (1) See 'Device Class Definition for Human Interface Devices (HID), 6/27/01, Version 1.11',
*               section 7.1 for more details about class-descriptor types.
*
*               (a) The 'wValue' field of Get_Descriptor Request specifies the Descriptor Type in the
*                   high byte.
*
*           (2) See 'Device Class Definition for Human Interface Devices (HID), 6/27/01, Version 1.11',
*               section 7.2 for more details about class-specific requests.
*
*               (a) The 'bRequest' field of SETUP packet may contain one of these values.
*********************************************************************************************************
*/

#define  USBD_HID_DESC_LEN                                 6u

                                                                /* --------- CLASS-SPECIFIC DESC (see Note #1) -------- */
#define  USBD_HID_DESC_TYPE_HID                         0x21
#define  USBD_HID_DESC_TYPE_REPORT                      0x22
#define  USBD_HID_DESC_TYPE_PHYSICAL                    0x23
                                                                /* --------- CLASS-SPECIFIC REQ (see Note #2) --------- */
#define  USBD_HID_REQ_GET_REPORT                        0x01
#define  USBD_HID_REQ_GET_IDLE                          0x02
#define  USBD_HID_REQ_GET_PROTOCOL                      0x03
#define  USBD_HID_REQ_SET_REPORT                        0x09
#define  USBD_HID_REQ_SET_IDLE                          0x0A
#define  USBD_HID_REQ_SET_PROTOCOL                      0x0B


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

typedef  struct  usbd_hid_ctrl  USBD_HID_CTRL;


/*
*********************************************************************************************************
*                                          HID CLASS STATES
*********************************************************************************************************
*/

typedef  enum  usbd_hid_state {                                 /* HID class states.                                    */
    USBD_HID_STATE_NONE = 0,
    USBD_HID_STATE_INIT,
    USBD_HID_STATE_CFG
} USBD_HID_STATE;


/*
*********************************************************************************************************
*                                 HID CLASS EP REQUIREMENTS DATA TYPE
*********************************************************************************************************
*/

                                                                /* ---------------- HID CLASS COMM INFO --------------- */
typedef struct usbd_hid_comm {
    USBD_HID_CTRL        *CtrlPtr;                              /* Pointer to control information.                      */
                                                                /* Avail EP for comm: Intr                              */
    CPU_INT08U            DataIntrInEpAddr;
    CPU_INT08U            DataIntrOutEpAddr;
    CPU_BOOLEAN           DataIntrOutActiveXfer;
} USBD_HID_COMM;


struct usbd_hid_ctrl {                                          /* ----------------- HID CLASS CTRL INFO -------------- */
    CPU_INT08U              DevNbr;                             /* Dev   nbr.                                           */
    CPU_INT08U              ClassNbr;                           /* Class nbr.                                           */
    USBD_HID_STATE          State;                              /* HID class state.                                     */
    USBD_HID_COMM          *CommPtr;                            /* HID class comm info ptr.                             */

    USBD_HID_ASYNC_FNCT     IntrRdAsyncFnct;                    /* Ptr to async comm callback and arg.                  */
    void                   *IntrRdAsyncArgPtr;
    USBD_HID_ASYNC_FNCT     IntrWrAsyncFnct;
    void                   *IntrWrAsyncArgPtr;
    CPU_INT32U              DataIntrInXferLen;

    CPU_INT08U              SubClassCode;
    CPU_INT08U              ProtocolCode;
    USBD_HID_COUNTRY_CODE   CountryCode;
    USBD_HID_REPORT         Report;
    CPU_INT08U             *ReportDescPtr;
    CPU_INT16U              ReportDescLen;
    CPU_INT08U             *PhyDescPtr;
    CPU_INT16U              PhyDescLen;
    CPU_INT16U              IntervalIn;
    CPU_INT16U              IntervalOut;
    CPU_BOOLEAN             CtrlRdEn;                           /* En rd operations thru ctrl xfer.                     */
    USBD_HID_CALLBACK      *CallbackPtr;                        /* Ptr to class-specific desc and req callbacks.        */
    CPU_INT08U             *RxBufPtr;
    CPU_INT32U              RxBufLen;
    CPU_BOOLEAN             IsRx;

    CPU_INT08U             *CtrlStatusBufPtr;                   /* Buf used for ctrl status xfers.                      */
};


/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*
* Note(s) : (1) See 'Device Class Definition for Human Interface Devices (HID), 6/27/01, Version 1.11',
*               section 6.2 for more details about HID descriptor fields.
*********************************************************************************************************
*/

static  USBD_HID_CTRL  USBD_HID_CtrlTbl[USBD_HID_CFG_MAX_NBR_DEV];
static  CPU_INT08U     USBD_HID_CtrlNbrNext;
                                                                /* HID class comm array.                                */
static  USBD_HID_COMM  USBD_HID_CommTbl[USBD_HID_MAX_NBR_COMM];
static  CPU_INT08U     USBD_HID_CommNbrNext;


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void         USBD_HID_Conn            (       CPU_INT08U       dev_nbr,
                                                      CPU_INT08U       cfg_nbr,
                                                      void            *p_if_class_arg);

static  void         USBD_HID_Disconn         (       CPU_INT08U       dev_nbr,
                                                      CPU_INT08U       cfg_nbr,
                                                      void            *p_if_class_arg);

static  void         USBD_HID_AltSettingUpdate(       CPU_INT08U       dev_nbr,
                                                      CPU_INT08U       cfg_nbr,
                                                      CPU_INT08U       if_nbr,
                                                      CPU_INT08U       if_alt_nbr,
                                                      void            *p_if_class_arg,
                                                      void            *p_if_alt_class_arg);

static  void         USBD_HID_EP_StateUpdate  (       CPU_INT08U       dev_nbr,
                                                      CPU_INT08U       cfg_nbr,
                                                      CPU_INT08U       if_nbr,
                                                      CPU_INT08U       if_alt_nbr,
                                                      CPU_INT08U       ep_addr,
                                                      void            *p_if_class_arg,
                                                      void            *p_if_alt_class_arg);

static  void         USBD_HID_IF_Desc         (       CPU_INT08U       dev_nbr,
                                                      CPU_INT08U       cfg_nbr,
                                                      CPU_INT08U       if_nbr,
                                                      CPU_INT08U       if_alt_nbr,
                                                      void            *p_if_class_arg,
                                                      void            *p_if_alt_class_arg);

static  void         USBD_HID_IF_DescHandler  (       CPU_INT08U       dev_nbr,
                                                      void            *p_if_class_arg);

static  CPU_INT16U   USBD_HID_IF_DescSizeGet  (       CPU_INT08U       dev_nbr,
                                                      CPU_INT08U       cfg_nbr,
                                                      CPU_INT08U       if_nbr,
                                                      CPU_INT08U       if_alt_nbr,
                                                      void            *p_if_class_arg,
                                                      void            *p_if_alt_class_arg);

static  CPU_BOOLEAN  USBD_HID_IF_Req          (       CPU_INT08U       dev_nbr,
                                               const  USBD_SETUP_REQ  *p_setup_req,
                                                      void            *p_if_class_arg);

static  CPU_BOOLEAN  USBD_HID_ClassReq        (       CPU_INT08U       dev_nbr,
                                               const  USBD_SETUP_REQ  *p_setup_req,
                                                      void            *p_if_class_arg);

static  void         USBD_HID_WrAsyncCmpl     (       CPU_INT08U       dev_nbr,
                                                      CPU_INT08U       ep_addr,
                                                      void            *p_buf,
                                                      CPU_INT32U       buf_len,
                                                      CPU_INT32U       xfer_len,
                                                      void            *p_arg,
                                                      USBD_ERR         err);

static  void         USBD_HID_WrSyncCmpl      (       CPU_INT08U       dev_nbr,
                                                      CPU_INT08U       ep_addr,
                                                      void            *p_buf,
                                                      CPU_INT32U       buf_len,
                                                      CPU_INT32U       xfer_len,
                                                      void            *p_arg,
                                                      USBD_ERR         err);

static  void         USBD_HID_RdAsyncCmpl     (       CPU_INT08U       dev_nbr,
                                                      CPU_INT08U       ep_addr,
                                                      void            *p_buf,
                                                      CPU_INT32U       buf_len,
                                                      CPU_INT32U       xfer_len,
                                                      void            *p_arg,
                                                      USBD_ERR         err);

static  void         USBD_HID_OutputDataCmpl  (       CPU_INT08U       class_nbr,
                                                      void            *p_buf,
                                                      CPU_INT32U       buf_len,
                                                      CPU_INT32U       xfer_len,
                                                      void            *p_arg,
                                                      USBD_ERR         err);


/*
*********************************************************************************************************
*                                          HID CLASS DRIVER
*********************************************************************************************************
*/

static  USBD_CLASS_DRV  USBD_HID_Drv = {
    USBD_HID_Conn,
    USBD_HID_Disconn,
    USBD_HID_AltSettingUpdate,
    USBD_HID_EP_StateUpdate,
    USBD_HID_IF_Desc,
    USBD_HID_IF_DescSizeGet,
    DEF_NULL,
    DEF_NULL,
    USBD_HID_IF_Req,
    USBD_HID_ClassReq,
    DEF_NULL,

#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
    DEF_NULL,
    DEF_NULL
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
*                                           USBD_HID_Init()
*
* Description : Initialize HID class.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE   HID class initialized successfully.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_HID_Init (USBD_ERR  *p_err)
{
    CPU_INT08U      ix;
    USBD_HID_CTRL  *p_ctrl;
    USBD_HID_COMM  *p_comm;
    LIB_ERR         err_lib;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

    USBD_HID_OS_Init(p_err);
    if (*p_err != USBD_ERR_NONE) {
         return;
    }

    USBD_HID_Report_Init(p_err);
    if (*p_err != USBD_ERR_NONE) {
         return;
    }

    for (ix = 0u; ix < USBD_HID_CFG_MAX_NBR_DEV; ix++) {        /* Init HID class struct.                               */
        p_ctrl                    = &USBD_HID_CtrlTbl[ix];
        p_ctrl->DevNbr            =  USBD_DEV_NBR_NONE;
        p_ctrl->ClassNbr          =  USBD_CLASS_NBR_NONE;
        p_ctrl->State             =  USBD_HID_STATE_NONE;
        p_ctrl->CommPtr           = (USBD_HID_COMM     *)0;
        p_ctrl->SubClassCode      =  USBD_HID_SUBCLASS_NONE;
        p_ctrl->ProtocolCode      =  USBD_HID_PROTOCOL_NONE;
        p_ctrl->CountryCode       =  USBD_HID_COUNTRY_CODE_NOT_SUPPORTED;
        p_ctrl->ReportDescPtr     = (CPU_INT08U        *)0;
        p_ctrl->ReportDescLen     =  0u;
        p_ctrl->PhyDescPtr        = (CPU_INT08U        *)0;
        p_ctrl->PhyDescLen        =  0u;
        p_ctrl->IntervalIn        =  0u;
        p_ctrl->IntervalOut       =  0u;
        p_ctrl->CtrlRdEn          =  DEF_TRUE;
        p_ctrl->CallbackPtr       = (USBD_HID_CALLBACK *)0;

        p_ctrl->IntrWrAsyncFnct   = (USBD_HID_ASYNC_FNCT)0;
        p_ctrl->IntrWrAsyncArgPtr = (void              *)0;
        p_ctrl->IntrRdAsyncFnct   = (USBD_HID_ASYNC_FNCT)0;
        p_ctrl->IntrRdAsyncArgPtr = (void              *)0;

        p_ctrl->CtrlStatusBufPtr  = (CPU_INT08U *)Mem_HeapAlloc(              sizeof(CPU_ADDR),
                                                                              USBD_CFG_BUF_ALIGN_OCTETS,
                                                                (CPU_SIZE_T *)DEF_NULL,
                                                                             &err_lib);
        if (err_lib != LIB_MEM_ERR_NONE) {
           *p_err = USBD_ERR_ALLOC;
            return;
        }
    }

    for (ix = 0u; ix < USBD_HID_MAX_NBR_COMM; ix++) {           /* Init HID EP tbl.                                     */
        p_comm = &USBD_HID_CommTbl[ix];

        p_comm->CtrlPtr               = (USBD_HID_CTRL     *)0;
        p_comm->DataIntrInEpAddr      =  USBD_EP_ADDR_NONE;
        p_comm->DataIntrOutEpAddr     =  USBD_EP_ADDR_NONE;
        p_comm->DataIntrOutActiveXfer =  DEF_NO;
    }

    USBD_HID_CtrlNbrNext = 0u;
    USBD_HID_CommNbrNext = 0u;
}


/*
*********************************************************************************************************
*                                           USBD_HID_Add()
*
* Description : Add a new instance of the HID class.
*
* Argument(s) : subclass            Subclass code.
*
*               protocol            Protocol code.
*
*               country_code        Country code ID.
*
*               p_report_desc       Pointer to report descriptor structure
*
*               report_desc_len     Report descriptor length.
*
*               p_phy_desc          Pointer to physical descriptor structure.
*
*               phy_desc_len        Physical descriptor length.
*
*               interval_in         Polling interval for input  transfers, in milliseconds.
*                                   It must be a power of 2.
*
*               interval_out        Polling interval for output transfers, in milliseconds.
*                                   It must be a power of 2. Used only when read operations are not
*                                   through control transfers.
*
*               ctrl_rd_en          Enable read operations through control transfers.
*
*               p_hid_callback      Pointer to HID descriptor and request callback structure.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           HID class instance successfully added.
*                               USBD_ERR_ALLOC          HID class instances NOT available.
*                               USBD_ERR_NULL_PTR       Argument 'p_hid_callbacks' passed a NULL pointer.
*                               USBD_ERR_INVALID_ARG    Invalid argument(s) passed to 'p_hid_callbacks'/
*                                                           'intr_out_en'/'p_report_desc'/'report_desc_len'/
*                                                           'p_phy_desc'/'phy_desc_len'/'ctrl_rd_en'.
*
*                                                       ------ RETURNED BY USBD_HID_Report_Parse() : -------
*                               USBD_ERR_NULL_PTR       Argument 'p_report_data'/'p_report' passed a NULL
*                                                           pointer.
*                               USBD_ERR_INVALID_ARG    Invalid argument(s) passed to 'p_report_data'/
*                                                           'report_data_len'.
*                               USBD_ERR_FAIL           Invalid HID descriptor data, mismatch on end of
*                                                           collection, or mismatch on pop items.
*                               USBD_ERR_ALLOC          No more Report ID or global item structure available,
*                                                           or memory allocation failed for report buffers.
*
* Return(s)   : Class instance number, if NO error(s).
*
*               USBD_CLASS_NBR_NONE,   otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  USBD_HID_Add (CPU_INT08U              subclass,
                          CPU_INT08U              protocol,
                          USBD_HID_COUNTRY_CODE   country_code,
                          CPU_INT08U             *p_report_desc,
                          CPU_INT16U              report_desc_len,
                          CPU_INT08U             *p_phy_desc,
                          CPU_INT16U              phy_desc_len,
                          CPU_INT16U              interval_in,
                          CPU_INT16U              interval_out,
                          CPU_BOOLEAN             ctrl_rd_en,
                          USBD_HID_CALLBACK      *p_hid_callback,
                          USBD_ERR               *p_err)
{
    USBD_HID_CTRL  *p_ctrl;
    CPU_INT08U      class_nbr;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }

    if (MATH_IS_PWR2(interval_in) == DEF_NO) {                  /* Interval for intr IN must be a power of 2.           */
       *p_err = USBD_ERR_INVALID_ARG;
        return (USBD_CLASS_NBR_NONE);
    }

    if ((ctrl_rd_en                 == DEF_NO) &&
        (MATH_IS_PWR2(interval_out) == DEF_NO)) {               /* Interval for intr OUT must be a power of 2.          */
       *p_err = USBD_ERR_INVALID_ARG;
        return (USBD_CLASS_NBR_NONE);
    }
#endif

    if (((p_report_desc     == (CPU_INT08U *)0) &&
         (  report_desc_len >   0u))            ||
        ((p_report_desc     != (CPU_INT08U *)0) &&
         (  report_desc_len ==  0u))) {
       *p_err = USBD_ERR_INVALID_ARG;
        return (USBD_CLASS_NBR_NONE);
    }

    if (((p_phy_desc     == (CPU_INT08U *)0) &&
         (  phy_desc_len >   0u))            ||
        ((p_phy_desc     != (CPU_INT08U *)0) &&
         (  phy_desc_len <   3u))) {
       *p_err = USBD_ERR_INVALID_ARG;
        return (USBD_CLASS_NBR_NONE);
    }

    CPU_CRITICAL_ENTER();
    class_nbr = USBD_HID_CtrlNbrNext;                           /* Alloc new HID class instance.                        */

    if (class_nbr >= USBD_HID_CFG_MAX_NBR_DEV) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_HID_INSTANCE_ALLOC;
        return (USBD_CLASS_NBR_NONE);
    }

    USBD_HID_CtrlNbrNext++;
    CPU_CRITICAL_EXIT();

    p_ctrl = &USBD_HID_CtrlTbl[class_nbr];

    p_ctrl->SubClassCode  = subclass;
    p_ctrl->ProtocolCode  = protocol;
    p_ctrl->CountryCode   = country_code;
    p_ctrl->ReportDescPtr = p_report_desc;
    p_ctrl->ReportDescLen = report_desc_len;
    p_ctrl->PhyDescPtr    = p_phy_desc;
    p_ctrl->PhyDescLen    = phy_desc_len;
    p_ctrl->IntervalIn    = interval_in;
    p_ctrl->IntervalOut   = interval_out;
    p_ctrl->CtrlRdEn      = ctrl_rd_en;
    p_ctrl->CallbackPtr   = p_hid_callback;

    USBD_HID_Report_Parse(class_nbr,
                          p_ctrl->ReportDescPtr,
                          p_ctrl->ReportDescLen,
                         &p_ctrl->Report,
                          p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (USBD_CLASS_NBR_NONE);
    } else {
        return (class_nbr);
    }
}


/*
*********************************************************************************************************
*                                          USBD_HID_CfgAdd()
*
* Description : Add HID class instance into USB device configuration (see Note #1).
*
* Argument(s) : class_nbr   Class instance number.
*
*               dev_nbr     Device number.
*
*               cfg_nbr     Configuration index to add HID class instance to.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   HID class configuration successfully added.
*                               USBD_ERR_ALLOC                  HID class communication instances NOT available.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'.
*
*                                                               ---------- RETURNED BY USBD_IF_Add() : ----------
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_desc'/
*                                                                   'class_desc_size'.
*                               USBD_ERR_NULL_PTR               Argument 'p_class_drv'/'p_class_drv->Conn'/
*                                                                   'p_class_drv->Disconn' passed a NULL pointer.
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
* Return(s)   : DEF_YES, if HID class instance added to USB device configuration successfully.
*
*               DEF_NO,  otherwise.
*
* Note(s)     : (1) Called several times, it allows to create multiple instances and multiple configurations.
*                   For instance, the following architecture could be created :
*
*                   HS
*                   |-- Configuration 0 (HID class 0)
*                                       (HID class 1)
*                                       (HID class 2)
*                       |-- Interface 0
*                   |-- Configuration 1 (HID class 0)
*                       |-- Interface 0
*
*               (2) Configuration Descriptor corresponding to a HID device has the following format :
*
*                   Configuration Descriptor
*                   |-- Interface Descriptor (HID class)
*                       |-- Endpoint Descriptor (Interrupt IN)
*                       |-- Endpoint Descriptor (Interrupt OUT) - optional
*********************************************************************************************************
*/

CPU_BOOLEAN  USBD_HID_CfgAdd (CPU_INT08U   class_nbr,
                              CPU_INT08U   dev_nbr,
                              CPU_INT08U   cfg_nbr,
                              USBD_ERR    *p_err)
{
    USBD_HID_CTRL  *p_ctrl;
    USBD_HID_COMM  *p_comm;
    CPU_INT08U      if_nbr;
    CPU_INT08U      ep_addr;
    CPU_INT16U      comm_nbr;
    CPU_INT16U      interval_in;
    CPU_INT16U      interval_out;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(DEF_NO);
    }
#endif

    if (class_nbr >= USBD_HID_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (DEF_NO);
    }

    p_ctrl = &USBD_HID_CtrlTbl[class_nbr];                      /* Get HID class instance.                              */
    CPU_CRITICAL_ENTER();
    if ((p_ctrl->DevNbr != USBD_DEV_NBR_NONE) &&                /* Chk if class is associated with a different dev.     */
        (p_ctrl->DevNbr != dev_nbr)) {
         CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_INVALID_ARG;
        return (DEF_NO);
    }

    p_ctrl->DevNbr = dev_nbr;

    comm_nbr = USBD_HID_CommNbrNext;                            /* Alloc new HID class comm info.                       */
    if (comm_nbr >= USBD_HID_MAX_NBR_COMM) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_HID_INSTANCE_ALLOC;
        return (DEF_NO);
    }
    USBD_HID_CommNbrNext++;
    CPU_CRITICAL_EXIT();

    p_comm = &USBD_HID_CommTbl[comm_nbr];

                                                                /* -------------- CFG DESC CONSTRUCTION --------------- */
                                                                /* See Note #2.                                         */

                                                                /* Add HID IF desc to cfg desc.                         */
    if_nbr = USBD_IF_Add(        dev_nbr,
                                 cfg_nbr,
                                &USBD_HID_Drv,
                         (void *)p_comm,                        /* Comm struct associated to HID IF.                    */
                         (void *)0,
                                 USBD_CLASS_CODE_HID,
                                 p_ctrl->SubClassCode,
                                 p_ctrl->ProtocolCode,
                                "HID Class",
                                 p_err);
    if (*p_err != USBD_ERR_NONE) {
         return (DEF_NO);
    }

    if (DEF_BIT_IS_CLR(cfg_nbr, USBD_CFG_NBR_SPD_BIT) == DEF_YES) {
                                                                /* In FS, bInterval in frames.                          */
        interval_in  = p_ctrl->IntervalIn;
        interval_out = p_ctrl->IntervalOut;

    } else {                                                    /* In HS, bInterval in microframes.                     */
        interval_in  = p_ctrl->IntervalIn * 8u;
        interval_out = p_ctrl->IntervalOut * 8u;
    }

    ep_addr = USBD_IntrAdd(dev_nbr,                             /* Add intr IN EP desc.                                 */
                           cfg_nbr,
                           if_nbr,
                           0u,
                           DEF_YES,
                           0u,
                           interval_in,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
         return (DEF_NO);
    }

    p_comm->DataIntrInEpAddr = ep_addr;                         /* Store intr IN EP addr.                               */

    if (p_ctrl->CtrlRdEn == DEF_FALSE) {
        ep_addr = USBD_IntrAdd(dev_nbr,                         /* Add intr OUT EP desc.                                */
                               cfg_nbr,
                               if_nbr,
                               0u,
                               DEF_NO,
                               0u,
                               interval_out,
                               p_err);
        if (*p_err != USBD_ERR_NONE) {
             return (DEF_NO);
        }
    } else {
        ep_addr = USBD_EP_ADDR_NONE;
    }

    p_comm->DataIntrOutEpAddr = ep_addr;                        /* Store intr OUT EP addr.                              */

                                                                /* Store HID class instance info.                       */
    CPU_CRITICAL_ENTER();
    p_ctrl->ClassNbr = class_nbr;
    p_ctrl->State    = USBD_HID_STATE_INIT;
    CPU_CRITICAL_EXIT();

    p_comm->CtrlPtr = p_ctrl;

   *p_err = USBD_ERR_NONE;

    return (DEF_YES);
}


/*
*********************************************************************************************************
*                                          USBD_HID_IsConn()
*
* Description : Get the HID class connection state.
*
* Argument(s) : class_nbr   Class instance number.
*
* Return(s)   : DEF_YES, if HID class is connected.
*
*               DEF_NO,  otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  USBD_HID_IsConn (CPU_INT08U  class_nbr)
{
    USBD_HID_CTRL   *p_ctrl;
    USBD_DEV_STATE   state;
    USBD_ERR         err;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (class_nbr >= USBD_HID_CtrlNbrNext) {
        return (DEF_NO);
    }
#endif

    p_ctrl = &USBD_HID_CtrlTbl[class_nbr];
    state  =  USBD_DevStateGet(p_ctrl->DevNbr, &err);           /* Get dev state.                                       */

    if ((err           == USBD_ERR_NONE            ) &&
        (state         == USBD_DEV_STATE_CONFIGURED) &&
        (p_ctrl->State == USBD_HID_STATE_CFG       )) {
        return (DEF_YES);
    } else {
        return (DEF_NO);
    }
}


/*
*********************************************************************************************************
*                                            USBD_HID_Rd()
*
* Description : Receive data from host through Interrupt OUT endpoint. This function is blocking.
*
* Argument(s) : class_nbr   Class instance number.
*
*               p_buf       Pointer to receive buffer.
*
*               buf_len     Receive buffer length, in octets.
*
*               timeout     Timeout, in milliseconds.
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
*               0,                        otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT32U  USBD_HID_Rd (CPU_INT08U   class_nbr,
                         void        *p_buf,
                         CPU_INT32U   buf_len,
                         CPU_INT16U   timeout,
                         USBD_ERR    *p_err)
{
    USBD_HID_CTRL  *p_ctrl;
    USBD_HID_COMM  *p_comm;
    CPU_INT32U      xfer_len;
    CPU_BOOLEAN     conn;
    USBD_ERR        err;
    CPU_SR_ALLOC();


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

    if (class_nbr >= USBD_HID_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (0u);
    }

    p_ctrl = &USBD_HID_CtrlTbl[class_nbr];

    CPU_CRITICAL_ENTER();
    p_comm = p_ctrl->CommPtr;
    conn   = USBD_HID_IsConn(class_nbr);
    CPU_CRITICAL_EXIT();

    if (conn != DEF_YES) {                                      /* Chk class state.                                     */
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return (0u);
    }

    if (p_ctrl->CtrlRdEn == DEF_TRUE) {                         /* Use SET_REPORT to rx data instead.                   */
        USBD_HID_OS_OutputLock(class_nbr, p_err);
        if (*p_err != USBD_ERR_NONE) {
            return (0u);
        }

        if (p_ctrl->IsRx == DEF_TRUE) {
            USBD_HID_OS_OutputUnlock(class_nbr);
           *p_err = USBD_ERR_FAIL;
            return (0u);
        }
        p_ctrl->IsRx     =  DEF_TRUE;
        p_ctrl->RxBufLen =  buf_len;
        p_ctrl->RxBufPtr = (CPU_INT08U *)p_buf;
                                                                /* Save app rx callback.                                */
        p_ctrl->IntrRdAsyncFnct   =  USBD_HID_OutputDataCmpl;
        p_ctrl->IntrRdAsyncArgPtr = &xfer_len;

        USBD_HID_OS_OutputUnlock(class_nbr);

        USBD_HID_OS_OutputDataPend(class_nbr, timeout, &err);

        USBD_HID_OS_OutputLock(class_nbr, p_err);
        if (*p_err != USBD_ERR_NONE) {
            return (0u);
        }

       *p_err = err;

        if (*p_err == USBD_ERR_OS_TIMEOUT) {
            p_ctrl->RxBufLen = 0u;
            p_ctrl->RxBufPtr = (CPU_INT08U *)0;

            p_ctrl->IntrRdAsyncFnct   = 0;
            p_ctrl->IntrRdAsyncArgPtr = 0;

            p_ctrl->IsRx = DEF_FALSE;
            USBD_HID_OS_OutputUnlock(class_nbr);
            return (0u);
        }

        USBD_HID_OS_OutputUnlock(class_nbr);

        if (*p_err != USBD_ERR_NONE) {
            return (0u);
        }

        return (xfer_len);
    }


                                                                /* ------------------ INTR OUT COMM ------------------- */
    xfer_len = USBD_IntrRx(p_ctrl->DevNbr,
                           p_comm->DataIntrOutEpAddr,
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
*                                         USBD_HID_RdAsync()
*
* Description : Receive data from host through Interrupt OUT endpoint. This function is non-blocking and
*               returns immediately after transfer preparation. Upon transfer completion, the callback
*               provided is called to notify the application.
*
* Argument(s) : class_nbr       Class instance number.
*
*               p_buf           Pointer to receive buffer.
*
*               buf_len         Receive buffer length, in octets.
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
*                               USBD_ERR_FAIL                   Read operation already in progress.
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

void  USBD_HID_RdAsync (CPU_INT08U            class_nbr,
                        void                 *p_buf,
                        CPU_INT32U            buf_len,
                        USBD_HID_ASYNC_FNCT   async_fnct,
                        void                 *p_async_arg,
                        USBD_ERR             *p_err)
{
    USBD_HID_CTRL  *p_ctrl;
    USBD_HID_COMM  *p_comm;
    CPU_BOOLEAN     conn;
    CPU_SR_ALLOC();


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

    if (class_nbr >= USBD_HID_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    p_ctrl = &USBD_HID_CtrlTbl[class_nbr];

    CPU_CRITICAL_ENTER();
    p_comm = p_ctrl->CommPtr;
    conn   = USBD_HID_IsConn(class_nbr);
    CPU_CRITICAL_EXIT();

    if (conn != DEF_YES) {                                      /* Chk class state.                                     */
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return;
    }

    if (p_ctrl->CtrlRdEn == DEF_TRUE) {                         /* Use SET_REPORT to rx data instead.                   */
        USBD_HID_OS_OutputLock(class_nbr, p_err);
        if (*p_err != USBD_ERR_NONE) {
            return;
        }

        if (p_ctrl->IsRx == DEF_TRUE) {
            USBD_HID_OS_OutputUnlock(class_nbr);
           *p_err = USBD_ERR_FAIL;
            return;
        }
        p_ctrl->IsRx     =  DEF_TRUE;
        p_ctrl->RxBufLen =  buf_len;
        p_ctrl->RxBufPtr = (CPU_INT08U *)p_buf;
                                                                /* Save app rx callback.                                */
        p_ctrl->IntrRdAsyncFnct   =   async_fnct;
        p_ctrl->IntrRdAsyncArgPtr = p_async_arg;
        USBD_HID_OS_OutputUnlock(class_nbr);

    } else {
                                                                /* Save app rx callback.                                */
        CPU_CRITICAL_ENTER();
        p_ctrl->IntrRdAsyncFnct   =   async_fnct;
        p_ctrl->IntrRdAsyncArgPtr = p_async_arg;
                                                                /* ------------------ INTR OUT COMM ------------------- */
        if (p_comm->DataIntrOutActiveXfer == DEF_NO) {          /* Check if another xfer is already in progress.        */
            p_comm->DataIntrOutActiveXfer = DEF_YES;            /* Indicate that a xfer is in progres.                  */
            CPU_CRITICAL_EXIT();

            USBD_IntrRxAsync(        p_ctrl->DevNbr,
                                     p_comm->DataIntrOutEpAddr,
                                     p_buf,
                                     buf_len,
                                     USBD_HID_RdAsyncCmpl,
                             (void *)p_comm,
                                     p_err);
            if (*p_err != USBD_ERR_NONE) {
                CPU_CRITICAL_ENTER();
                p_comm->DataIntrOutActiveXfer = DEF_NO;
                CPU_CRITICAL_EXIT();
            }
        } else {
            CPU_CRITICAL_ENTER();
           *p_err = USBD_ERR_CLASS_XFER_IN_PROGRESS;
            return;
        }
    }
}


/*
*********************************************************************************************************
*                                            USBD_HID_Wr()
*
* Description : Send data to host through Interrupt IN endpoint. This function is blocking.
*
* Argument(s) : class_nbr   Class instance number.
*
*               p_buf       Pointer to transmit buffer. If more than one input report exists, the first
*                           byte must represent the Report ID.
*
*               buf_len     Transmit buffer length, in octets.
*
*               timeout     Timeout in milliseconds.
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
*               0,                    otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT32U  USBD_HID_Wr (CPU_INT08U   class_nbr,
                         void        *p_buf,
                         CPU_INT32U   buf_len,
                         CPU_INT16U   timeout,
                         USBD_ERR    *p_err)
{
    USBD_HID_CTRL  *p_ctrl;
    USBD_HID_COMM  *p_comm;
    CPU_INT32U      xfer_len;
    CPU_INT08U      report_id;
    CPU_INT08U     *p_buf_data;
    CPU_INT08U     *p_buf_report;
    CPU_INT16U      report_len;
    CPU_BOOLEAN     is_largest;
    CPU_BOOLEAN     conn;
    CPU_BOOLEAN     eot;
    USBD_ERR        err;
    CPU_SR_ALLOC();


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

    if (class_nbr >= USBD_HID_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (0u);
    }

    p_ctrl = &USBD_HID_CtrlTbl[class_nbr];

    CPU_CRITICAL_ENTER();
    p_comm = p_ctrl->CommPtr;
    conn   = USBD_HID_IsConn(class_nbr);
    CPU_CRITICAL_EXIT();

    if (conn != DEF_YES) {                                      /* Chk class state.                                     */
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return (0u);
    }

                                                                /* ----------------- STORE REPORT DATA ---------------- */
    if (buf_len > 0) {
        p_buf_data = (CPU_INT08U *)p_buf;
        if (p_ctrl->Report.HasReports == DEF_YES) {
            report_id = p_buf_data[0];
        } else {
            report_id = 0;
        }

        report_len = USBD_HID_ReportID_InfoGet(&p_ctrl->Report,
                                                USBD_HID_REPORT_TYPE_INPUT,
                                                report_id,
                                               &p_buf_report,
                                               &is_largest,
                                                p_err);
        if (*p_err != USBD_ERR_NONE) {
            return (0u);
        }

        if (report_len > buf_len) {
           *p_err = USBD_ERR_INVALID_ARG;
            return (0u);
        }

        eot = (is_largest == DEF_YES) ? DEF_NO : DEF_YES;
    } else {
        p_buf_data   = (CPU_INT08U *)0;
        p_buf_report = (CPU_INT08U *)0;
        report_len   = 0;
        eot          = DEF_YES;
    }

    USBD_HID_OS_TxLock(class_nbr, p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (0u);
    }

                                                                /* ------------------- INTR IN COMM ------------------- */
    USBD_IntrTxAsync(        p_ctrl->DevNbr,
                             p_comm->DataIntrInEpAddr,
                             p_buf,
                             buf_len,
                             USBD_HID_WrSyncCmpl,
                     (void *)p_comm,
                             eot,
                             p_err);
    if (*p_err != USBD_ERR_NONE) {
        USBD_HID_OS_TxUnlock(class_nbr);
        return (0u);
    }

    if (buf_len > 0) {                                          /* Defer copy while transmitting.                       */
        USBD_HID_OS_InputLock(class_nbr, p_err);
        if (*p_err != USBD_ERR_NONE) {
            USBD_HID_OS_TxUnlock(class_nbr);
            return (0u);
        }
        Mem_Copy(&p_buf_report[0], &p_buf_data[0], report_len);
        USBD_HID_OS_InputUnlock(class_nbr);
    }

    USBD_HID_OS_InputDataPend(class_nbr, timeout, p_err);
    if (*p_err != USBD_ERR_NONE) {
        if (*p_err == USBD_ERR_OS_TIMEOUT) {
            USBD_EP_Abort(p_ctrl->DevNbr,
                          p_comm->DataIntrInEpAddr,
                         &err);
        }
        USBD_HID_OS_TxUnlock(class_nbr);
        return (0u);
    }

    xfer_len = p_ctrl->DataIntrInXferLen;

    USBD_HID_OS_TxUnlock(class_nbr);
    return (xfer_len);
}


/*
*********************************************************************************************************
*                                         USBD_HID_WrAsync()
*
* Description : Send data to host through Interrupt IN endpoint. This function is non-blocking, and returns
*               immediately after transfer preparation. Upon transfer completion, the callback provided
*               is called to notify the application.
*
* Argument(s) : class_nbr       Class instance number.
*
*               p_buf           Pointer to transmit buffer. If more than one input report exists, the first
*                               byte must represent the Report ID.
*
*               buf_len         Transmit buffer length, in octets.
*
*               async_fnct      Transmit callback.
*
*               p_async_arg     Additional argument provided by application for transmit callback.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Transfer successfully prepared.
*                               USBD_ERR_NULL_PTR               Argument 'p_buf' passed a NULL pointer.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'.
*                               USBD_ERR_INVALID_CLASS_STATE    Invalid class state.
*
*                                                               ----- RETURNED BY USBD_IntrTxAsync() : -----
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_EP_INVALID_NBR         Invalid endpoint number.
*                               USBD_ERR_DEV_INVALID_STATE      Transfer type only available if device is in
*                                                                   configured state.
*                               USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*                               USBD_ERR_EP_INVALID_STATE       Endpoint not opened.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_HID_WrAsync (CPU_INT08U            class_nbr,
                        void                 *p_buf,
                        CPU_INT32U            buf_len,
                        USBD_HID_ASYNC_FNCT   async_fnct,
                        void                 *p_async_arg,
                        USBD_ERR             *p_err)
{
    USBD_HID_CTRL  *p_ctrl;
    USBD_HID_COMM  *p_comm;
    CPU_INT08U      report_id;
    CPU_INT08U     *p_buf_data;
    CPU_INT08U     *p_buf_report;
    CPU_INT16U      report_len;
    CPU_BOOLEAN     is_largest;
    CPU_BOOLEAN     conn;
    CPU_BOOLEAN     eot;
    CPU_SR_ALLOC();


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

    if (class_nbr >= USBD_HID_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    p_ctrl = &USBD_HID_CtrlTbl[class_nbr];

    CPU_CRITICAL_ENTER();
    p_comm = p_ctrl->CommPtr;
    conn   = USBD_HID_IsConn(class_nbr);
    CPU_CRITICAL_EXIT();

    if (conn != DEF_YES) {                                      /* Chk class state.                                     */
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return;
    }

                                                                /* ----------------- STORE REPORT DATA ---------------- */
    if (buf_len > 0) {
        p_buf_data = (CPU_INT08U *)p_buf;
        if (p_ctrl->Report.HasReports == DEF_YES) {
            report_id = p_buf_data[0];
        } else {
            report_id = 0;
        }
        report_len = USBD_HID_ReportID_InfoGet(&p_ctrl->Report,
                                                USBD_HID_REPORT_TYPE_INPUT,
                                                report_id,
                                               &p_buf_report,
                                               &is_largest,
                                                p_err);
        if (*p_err != USBD_ERR_NONE) {
            return;
        }

        if (report_len > buf_len) {
           *p_err = USBD_ERR_INVALID_ARG;
            return;
        }

        eot = (is_largest == DEF_YES) ? DEF_NO : DEF_YES;
    } else {
        p_buf_data   = (CPU_INT08U *)0;
        p_buf_report = (CPU_INT08U *)0;
        report_len   = 0;
        eot          = DEF_YES;
    }

    USBD_HID_OS_TxLock(class_nbr, p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

                                                                /* Save app rx callback.                                */
    p_ctrl->IntrWrAsyncFnct   =   async_fnct;
    p_ctrl->IntrWrAsyncArgPtr = p_async_arg;


                                                                /* ------------------- INTR IN COMM ------------------- */
    USBD_IntrTxAsync(        p_ctrl->DevNbr,
                             p_comm->DataIntrInEpAddr,
                             p_buf,
                             buf_len,
                             USBD_HID_WrAsyncCmpl,
                     (void *)p_comm,
                             eot,
                             p_err);
    if (*p_err != USBD_ERR_NONE) {
        USBD_HID_OS_TxUnlock(class_nbr);
        return;
    }

    if (buf_len > 0) {                                          /* Defer copy while transmitting.                       */
        USBD_HID_OS_InputLock(class_nbr, p_err);
        if (*p_err != USBD_ERR_NONE) {
            USBD_HID_OS_TxUnlock(class_nbr);
            return;
        }
        Mem_Copy(&p_buf_report[0], &p_buf_data[0], report_len);
        USBD_HID_OS_InputUnlock(class_nbr);
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
*                                           USBD_HID_Conn()
*
* Description : Notify class that configuration is active.
*
* Argument(s) : dev_nbr         Device number.
*
*               cfg_nbr         Configuration number.
*
*               p_if_class_arg  Pointer to class argument specific to interface.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_HID_Conn (CPU_INT08U   dev_nbr,
                             CPU_INT08U   cfg_nbr,
                             void        *p_if_class_arg)
{
    USBD_HID_COMM  *p_comm;
    CPU_SR_ALLOC();


    (void)dev_nbr;
    (void)cfg_nbr;

    p_comm = (USBD_HID_COMM *)p_if_class_arg;
    CPU_CRITICAL_ENTER();
    p_comm->CtrlPtr->CommPtr = p_comm;
    p_comm->CtrlPtr->State   = USBD_HID_STATE_CFG;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                         USBD_HID_Disconn()
*
* Description : Notify class that configuration is not active.
*
* Argument(s) : dev_nbr         Device number.
*
*               cfg_nbr         Configuration number.
*
*               p_if_class_arg  Pointer to class argument specific to interface.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_HID_Disconn (CPU_INT08U   dev_nbr,
                                CPU_INT08U   cfg_nbr,
                                void        *p_if_class_arg)
{
    USBD_HID_COMM  *p_comm;
    USBD_HID_CTRL  *p_ctrl;
    CPU_SR_ALLOC();


    (void)dev_nbr;
    (void)cfg_nbr;

    p_comm = (USBD_HID_COMM *)p_if_class_arg;
    p_ctrl =  p_comm->CtrlPtr;

    USBD_HID_Report_RemoveAllIdle(&p_ctrl->Report);

    CPU_CRITICAL_ENTER();
    p_ctrl->CommPtr = (USBD_HID_COMM *)0;
    p_ctrl->State   =  USBD_HID_STATE_INIT;
    if (p_ctrl->IsRx == DEF_TRUE) {
        CPU_CRITICAL_EXIT();

        p_ctrl->IntrRdAsyncFnct(         p_ctrl->ClassNbr,
                                         p_ctrl->RxBufPtr,
                                         p_ctrl->RxBufLen,
                                         0,
                                 (void *)p_ctrl->IntrRdAsyncArgPtr,
                                         USBD_ERR_OS_ABORT);

        CPU_CRITICAL_ENTER();
        p_ctrl->IsRx     =  DEF_FALSE;
        p_ctrl->RxBufLen =  0;
        p_ctrl->RxBufPtr = (CPU_INT08U *)0;

        p_ctrl->IntrRdAsyncFnct   = 0;
        p_ctrl->IntrRdAsyncArgPtr = 0;
        CPU_CRITICAL_EXIT();
    } else {
        CPU_CRITICAL_EXIT();
    }
}


/*
*********************************************************************************************************
*                                     USBD_HID_AltSettingUpdate()
*
* Description : Notify class that interface alternate setting has been updated.
*
* Argument(s) : dev_nbr             Device number.
*
*               cfg_nbr             Configuration number.
*
*               if_nbr              Interface number.
*
*               if_alt_nbr          Interface alternate setting number.
*
*               p_if_class_arg      Pointer to class argument specific to interface.
*
*               p_if_alt_class_arg  Pointer to class argument specific to alternate interface.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_HID_AltSettingUpdate (CPU_INT08U   dev_nbr,
                                         CPU_INT08U   cfg_nbr,
                                         CPU_INT08U   if_nbr,
                                         CPU_INT08U   if_alt_nbr,
                                         void        *p_if_class_arg,
                                         void        *p_if_alt_class_arg)
{
    USBD_HID_COMM  *p_comm;
    CPU_SR_ALLOC();


    (void)dev_nbr;
    (void)cfg_nbr;
    (void)if_nbr;
    (void)if_alt_nbr;
    (void)p_if_alt_class_arg;

    p_comm = (USBD_HID_COMM *)p_if_class_arg;
    CPU_CRITICAL_ENTER();
    p_comm->CtrlPtr->CommPtr = p_comm;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                      USBD_HID_EP_StateUpdate()
*
* Description : Notify class that endpoint state has been updated.
*
* Argument(s) : dev_nbr             Device number.
*
*               cfg_nbr             Configuration number.
*
*               if_nbr              Interface number.
*
*               if_alt_nbr          Interface alternate setting number.
*
*               ep_addr             Endpoint address.
*
*               p_if_class_arg      Pointer to class argument specific to interface.
*
*               p_if_alt_class_arg  Pointer to class argument specific to alternate interface.

*
* Return(s)   : none.
*
* Note(s)     : (1) EP state may have changed, it can be checked through USBD_EP_IsStalled().
*********************************************************************************************************
*/

static  void  USBD_HID_EP_StateUpdate (CPU_INT08U   dev_nbr,
                                       CPU_INT08U   cfg_nbr,
                                       CPU_INT08U   if_nbr,
                                       CPU_INT08U   if_alt_nbr,
                                       CPU_INT08U   ep_addr,
                                       void        *p_if_class_arg,
                                       void        *p_if_alt_class_arg)

{
    (void)dev_nbr;
    (void)cfg_nbr;
    (void)if_nbr;
    (void)if_alt_nbr;
    (void)ep_addr;
    (void)p_if_class_arg;
    (void)p_if_alt_class_arg;
}


/*
*********************************************************************************************************
*                                          USBD_HID_IF_Desc()
*
* Description : Class interface descriptor callback.
*
* Argument(s) : dev_nbr             Device number.
*
*               cfg_nbr             Configuration number.
*
*               if_nbr              Interface number.
*
*               if_alt_nbr          Interface alternate setting number.
*
*               p_if_class_arg      Pointer to class argument specific to interface.
*
*               p_if_alt_class_arg  Pointer to class argument specific to alternate interface.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_HID_IF_Desc (CPU_INT08U   dev_nbr,
                                CPU_INT08U   cfg_nbr,
                                CPU_INT08U   if_nbr,
                                CPU_INT08U   if_alt_nbr,
                                void        *p_if_class_arg,
                                void        *p_if_alt_class_arg)

{
    (void)cfg_nbr;
    (void)if_nbr;
    (void)if_alt_nbr;
    (void)p_if_alt_class_arg;

    USBD_HID_IF_DescHandler(dev_nbr, p_if_class_arg);
}


/*
*********************************************************************************************************
*                                      USBD_HID_IF_DescHandler()
*
* Description : Class interface descriptor callback handler.
*
* Argument(s) : dev_nbr         Device number.
*
*               p_if_class_arg  Pointer to class argument.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_HID_IF_DescHandler (CPU_INT08U   dev_nbr,
                                       void        *p_if_class_arg)
{
    USBD_HID_CTRL  *p_ctrl;
    USBD_HID_COMM  *p_comm;
    CPU_INT08U      nbr_desc;


    p_comm = (USBD_HID_COMM *)p_if_class_arg;
    p_ctrl = p_comm->CtrlPtr;

    nbr_desc = 0u;
    if (p_ctrl->ReportDescLen > 0u) {
        nbr_desc++;
    }
    if (p_ctrl->PhyDescLen > 0u) {
        nbr_desc++;
    }

    USBD_DescWr08(dev_nbr, USBD_HID_DESC_LEN + nbr_desc * 3u);
    USBD_DescWr08(dev_nbr, USBD_HID_DESC_TYPE_HID);
    USBD_DescWr16(dev_nbr, 0x0111);
    USBD_DescWr08(dev_nbr, (CPU_INT08U)p_ctrl->CountryCode);
    USBD_DescWr08(dev_nbr, nbr_desc);

    if (p_ctrl->ReportDescLen > 0u) {
        USBD_DescWr08(dev_nbr, USBD_HID_DESC_TYPE_REPORT);
        USBD_DescWr16(dev_nbr, p_ctrl->ReportDescLen);
    }
    if (p_ctrl->PhyDescLen > 0u) {
        USBD_DescWr08(dev_nbr, USBD_HID_DESC_TYPE_PHYSICAL);
        USBD_DescWr16(dev_nbr, p_ctrl->PhyDescLen);
    }
}


/*
*********************************************************************************************************
*                                      USBD_HID_IF_DescSizeGet()
*
* Description : Retrieve the size of the class interface descriptor.
*
* Argument(s) : dev_nbr             Device number.
*
*               cfg_nbr             Configuration number.
*
*               if_nbr              Interface number.
*
*               if_alt_nbr          Interface alternate setting number.
*
*               p_if_class_arg      Pointer to class argument specific to interface.
*
*               p_if_alt_class_arg  Pointer to class argument specific to alternate interface.

*
* Return(s)   : Size of the class interface descriptor.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT16U  USBD_HID_IF_DescSizeGet (CPU_INT08U   dev_nbr,
                                             CPU_INT08U   cfg_nbr,
                                             CPU_INT08U   if_nbr,
                                             CPU_INT08U   if_alt_nbr,
                                             void        *p_if_class_arg,
                                             void        *p_if_alt_class_arg)
{
    USBD_HID_CTRL  *p_ctrl;
    USBD_HID_COMM  *p_comm;
    CPU_INT08U      nbr_desc;


    (void)dev_nbr;
    (void)cfg_nbr;
    (void)if_nbr;
    (void)if_alt_nbr;
    (void)p_if_alt_class_arg;

    p_comm = (USBD_HID_COMM *)p_if_class_arg;
    p_ctrl = p_comm->CtrlPtr;

    nbr_desc = 0u;
    if (p_ctrl->ReportDescLen > 0u) {
        nbr_desc++;
    }
    if (p_ctrl->PhyDescLen > 0u) {
        nbr_desc++;
    }

    return (USBD_HID_DESC_LEN + nbr_desc * 3u);
}


/*
*********************************************************************************************************
*                                          USBD_HID_IF_Req()
*
* Description : Process interface requests.
*
* Argument(s) : dev_nbr         Device number.
*
*               p_setup_req     Pointer to setup request structure.
*
*               p_if_class_arg  Pointer to class argument specific to interface.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) HID class supports 3 class-specific descriptors:
*
*                   (a) HID      descriptor (mandatory)
*
*                   (a) Report   descriptor (mandatory)
*
*                   (a) Physical descriptor (optional)
*
*                   HID descriptor is sent to Host as part of the Configuration descriptor. Report and
*                   Physical descriptors are retrieved by the host using a GET_DESCRIPTOR standard request
*                   with the recipient being the interface. The way to get Report and Physical descriptors
*                   is specific to the HID class. The HID class specification indicates in the section
*                   '7.1 Standard Requests':
*
*                   "The HID class uses the standard request Get_Descriptor as described in the USB Specification.
*                    When a Get_Descriptor(Configuration) request is issued, it returns the Configuration descriptor,
*                    all Interface descriptors, all Endpoint descriptors, and the HID descriptor for each interface.
*                    It shall not return the String descriptor, HID Report descriptor or any of the
*                    optional HID class descriptors."
*
*               (2) The HID descriptor identifies the length and type of subordinate descriptors for a
*                   device.
*                   See 'Device Class Definition for Human Interface Devices (HID), 6/27/01, Version 1.11',
*                   section 6.2.1 for more details about HID descriptor.
*
*               (3) The Report descriptor is made up of items that provide information about the device.
*                   The first part of an item contains three fields: item type, item tag, and item size.
*                   Together these fields identify the kind of information the item provides.
*                   See 'Device Class Definition for Human Interface Devices (HID), 6/27/01, Version 1.11',
*                   section 6.2.2 for more details about Report descriptor.
*
*               (4) A Physical Descriptor is a data structure that provides information about the
*                   specific part or parts of the human body that are activating a control or controls.
*                   See 'Device Class Definition for Human Interface Devices (HID), 6/27/01, Version 1.11',
*                   section 6.2.3 for more details about Physical descriptor.
*
*                   (a) Descriptor set 0 is a special descriptor set that specifies the number of additional
*                       descriptor sets, and also the number of Physical Descriptors in each set.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_HID_IF_Req (       CPU_INT08U       dev_nbr,
                                      const  USBD_SETUP_REQ  *p_setup_req,
                                             void            *p_if_class_arg)
{
    USBD_HID_COMM  *p_comm;
    CPU_BOOLEAN     valid;
    CPU_BOOLEAN     dev_to_host;
    CPU_INT08U      desc_type;
    CPU_INT08U      w_value;
    CPU_INT16U      desc_len;
    CPU_INT32U      desc_offset;


    (void)dev_nbr;

    p_comm = (USBD_HID_COMM *)p_if_class_arg;
    valid  =  DEF_FAIL;

    dev_to_host = DEF_BIT_IS_SET(p_setup_req->bmRequestType, USBD_REQ_DIR_BIT);

    if (p_setup_req->bRequest == USBD_REQ_GET_DESCRIPTOR) {     /* ---------- GET DESCRIPTOR (see Note #1) ------------ */

        if (dev_to_host != DEF_YES) {
            return (valid);
        }
                                                                /* Get desc type.                                       */
        desc_type = (CPU_INT08U)((p_setup_req->wValue >> 8u) & DEF_INT_08_MASK);
        w_value   = (CPU_INT08U) (p_setup_req->wValue        & DEF_INT_08_MASK);

        switch (desc_type) {
            case USBD_HID_DESC_TYPE_HID:                        /* ------------- HID DESC (see Note #2) --------------- */
                 if (w_value != 0) {
                     break;
                 }

                 USBD_HID_IF_DescHandler(dev_nbr, p_if_class_arg);
                 valid = DEF_OK;
                 break;


            case USBD_HID_DESC_TYPE_REPORT:                     /* ------------ REPORT DESC (see Note #3) ------------- */
                 if (w_value != 0) {
                     break;
                 }

                 if (p_comm->CtrlPtr->ReportDescLen > 0) {
                     USBD_DescWr(dev_nbr,
                                 p_comm->CtrlPtr->ReportDescPtr,
                                 p_comm->CtrlPtr->ReportDescLen);
                     valid = DEF_OK;
                 }
                 break;


            case USBD_HID_DESC_TYPE_PHYSICAL:                   /* ------------ PHYSICAL DESC (see Note #4) ----------- */
                 if (p_comm->CtrlPtr->PhyDescLen < 3) {
                     break;
                 }

                 if (w_value > 0) {
                     desc_len    = MEM_VAL_GET_INT16U_LITTLE(p_comm->CtrlPtr->PhyDescPtr + 1);
                     desc_offset = desc_len * (w_value - 1) + 3;

                     if (p_comm->CtrlPtr->PhyDescLen < (desc_offset + desc_len)) {
                         break;
                     }

                     USBD_DescWr(dev_nbr,
                                &p_comm->CtrlPtr->PhyDescPtr[desc_offset],
                                 desc_len);

                 } else {
                     USBD_DescWr(dev_nbr,                       /* See Note #4a.                                        */
                                 p_comm->CtrlPtr->PhyDescPtr,
                                 3);
                 }

                 valid = DEF_OK;
                 break;


            default:                                            /* Other desc type not supported.                       */
                 break;
        }
    }

    return (valid);
}


/*
*********************************************************************************************************
*                                         USBD_HID_ClassReq()
*
* Description : Process class-specific requests.
*
* Argument(s) : dev_nbr         Device number.
*
*               p_setup_req     Pointer to setup request structure.
*
*               p_if_class_arg  Pointer to class argument specific to interface.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) HID defines the following class-specific requests :
*
*                   (a) GET_REPORT   allows the host to receive a report.
*
*                   (b) SET_REPORT   allows the host to send a report to the device, possibly setting
*                                    the state of input, output, or feature controls.
*
*                   (c) GET_IDLE     reads the current idle rate for a particular Input report.
*
*                   (d) SET_IDLE     silences a particular report on the Interrupt In pipe until a
*                                    new event occurs or the specified amount of time passes.
*
*                   (e) GET_PROTOCOL reads which protocol is currently active (either the boot
*                                    protocol or the report protocol).
*
*                       (1) Protocol is either BOOT or REPORT.
*
*                   (f) SET_PROTOCOL switches between the boot protocol and the report protocol (or
*                                    vice versa).
*
*                   See 'Device Class Definition for Human Interface Devices (HID), 6/27/01, Version 1.11',
*                   section 7.2 for more details about Class-Specific Requests.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_HID_ClassReq (       CPU_INT08U       dev_nbr,
                                        const  USBD_SETUP_REQ  *p_setup_req,
                                               void            *p_if_class_arg)
{
    USBD_HID_COMM        *p_comm;
    USBD_HID_CTRL        *p_ctrl;
    CPU_BOOLEAN           valid;
    CPU_BOOLEAN           dev_to_host;
    USBD_ERR              err;
    CPU_INT08U            idle_rate;
    CPU_INT08U            report_id;
    CPU_INT08U            report_type;
    CPU_INT16U            report_len;
    CPU_INT08U            protocol;
    CPU_INT08U           *p_buf;
    CPU_INT32U            buf_len;
    CPU_INT16U            req_len;
    CPU_INT08U            w_value;
    CPU_BOOLEAN           is_largest;
    USBD_HID_CALLBACK    *p_callback;


    (void)dev_nbr;

    p_comm      = (USBD_HID_COMM *)p_if_class_arg;
    p_ctrl     =  p_comm->CtrlPtr;
    valid      =  DEF_FAIL;
    p_callback =  p_ctrl->CallbackPtr;

    dev_to_host = DEF_BIT_IS_SET(p_setup_req->bmRequestType, USBD_REQ_DIR_BIT);

    switch (p_setup_req->bRequest) {                            /* See Note #1.                                         */
        case USBD_HID_REQ_GET_REPORT:                           /* ------------- GET_REPORT (see Note #1a) ------------ */
             if (dev_to_host != DEF_YES) {
                 break;
             }

             report_type = (CPU_INT08U)((p_setup_req->wValue >> 8u) & DEF_INT_08_MASK);
             report_id   = (CPU_INT08U)( p_setup_req->wValue        & DEF_INT_08_MASK);

             switch (report_type) {
                 case 1:                                        /* Input report.                                        */
                      report_len = USBD_HID_ReportID_InfoGet(&p_ctrl->Report,
                                                              USBD_HID_REPORT_TYPE_INPUT,
                                                              report_id,
                                                             &p_buf,
                                                             &is_largest,
                                                             &err);
                      if (err != USBD_ERR_NONE) {
                          break;
                      }

                                                                /* Get min between req len & report size.               */
                      buf_len = DEF_MIN(p_setup_req->wLength, report_len);

                      USBD_HID_OS_InputLock(p_ctrl->ClassNbr, &err);
                      if (err != USBD_ERR_NONE) {
                          break;
                      }
                                                                /* Send Report to host.                                 */
                      USBD_CtrlTx(        p_ctrl->DevNbr,
                                  (void *)p_buf,
                                          buf_len,
                                          USBD_HID_CTRL_REQ_TIMEOUT_mS,
                                          DEF_NO,
                                         &err);
                      if (err == USBD_ERR_NONE) {
                          valid = DEF_OK;
                      }

                      USBD_HID_OS_InputUnlock(p_ctrl->ClassNbr);
                      break;


                 case 3:                                        /* Feature report.                                      */
                      if ((p_callback                   == (USBD_HID_CALLBACK *)0) ||
                          (p_callback->FeatureReportGet == (void              *)0)) {
                          break;                                /* Stall request.                                       */
                      }

                      report_len = USBD_HID_ReportID_InfoGet(&p_ctrl->Report,
                                                              USBD_HID_REPORT_TYPE_FEATURE,
                                                              report_id,
                                                             &p_buf,
                                                             &is_largest,
                                                             &err);
                      if (err != USBD_ERR_NONE) {
                          break;
                      }

                                                                /* Get min between req len & report size.               */
                      req_len = DEF_MIN(p_setup_req->wLength, report_len);
                      valid   = p_callback->FeatureReportGet(p_ctrl->ClassNbr,
                                                             report_id,
                                                             p_buf,
                                                             req_len);
                      if (valid != DEF_OK) {
                          break;
                      }
                                                                /* Send Report to host.                                 */
                      USBD_CtrlTx(        p_ctrl->DevNbr,
                                  (void *)p_buf,
                                          req_len,
                                          USBD_HID_CTRL_REQ_TIMEOUT_mS,
                                          DEF_NO,
                                         &err);
                      if (err == USBD_ERR_NONE) {
                         valid = DEF_OK;
                      }
                      break;


                 case 0:
                 default:
                      break;
             }
             break;


        case USBD_HID_REQ_SET_REPORT:                           /* ------------- SET_REPORT (see Note #1b) ------------ */
             if (dev_to_host != DEF_NO) {
                 break;
             }

             report_type = (CPU_INT08U)((p_setup_req->wValue >> 8u) & DEF_INT_08_MASK);
             report_id   = (CPU_INT08U)( p_setup_req->wValue        & DEF_INT_08_MASK);

             switch (report_type) {
                 case 2:                                        /* Output report.                                       */
                      if ((p_callback            == (USBD_HID_CALLBACK *)0) ||
                          (p_callback->ReportSet == (void              *)0)) {
                          break;                                /* Stall request.                                       */
                      }

                      report_len = USBD_HID_ReportID_InfoGet(&p_ctrl->Report,
                                                              USBD_HID_REPORT_TYPE_OUTPUT,
                                                              report_id,
                                                             &p_buf,
                                                             &is_largest,
                                                             &err);
                      if (err != USBD_ERR_NONE) {
                          break;
                      }

                      USBD_HID_OS_OutputLock(p_ctrl->ClassNbr, &err);
                      if (err != USBD_ERR_NONE) {
                          break;
                      }

                      if (p_setup_req->wLength > report_len) {
                          USBD_HID_OS_OutputUnlock(p_ctrl->ClassNbr);
                          break;
                      }

                                                                /* Receive report from host.                            */
                      USBD_CtrlRx(        p_ctrl->DevNbr,
                                  (void *)p_buf,
                                          report_len,
                                          USBD_HID_CTRL_REQ_TIMEOUT_mS,
                                         &err);
                      if (err != USBD_ERR_NONE) {
                          USBD_HID_OS_OutputUnlock(p_ctrl->ClassNbr);
                          break;
                      }

                      USBD_HID_OS_OutputUnlock(p_ctrl->ClassNbr);

                      p_callback->ReportSet(p_ctrl->ClassNbr,
                                            report_id,
                                            p_buf,
                                            report_len);

                      valid = DEF_OK;
                      break;


                 case 3:                                        /* Feature report.                                      */
                      if ((p_callback                   == (USBD_HID_CALLBACK *)0) ||
                          (p_callback->FeatureReportSet == (void              *)0)) {
                          break;                                /* Stall request.                                       */
                      }

                      report_len = USBD_HID_ReportID_InfoGet(&p_ctrl->Report,
                                                              USBD_HID_REPORT_TYPE_FEATURE,
                                                              report_id,
                                                             &p_buf,
                                                             &is_largest,
                                                             &err);
                      if (err != USBD_ERR_NONE) {
                          break;
                      }

                      if (p_setup_req->wLength != report_len) {
                          break;
                      }

                                                                /* Receive report from host.                            */
                      USBD_CtrlRx(        p_ctrl->DevNbr,
                                  (void *)p_buf,
                                          report_len,
                                          0,
                                         &err);
                      if (err != USBD_ERR_NONE) {
                          break;
                      }

                      valid = p_callback->FeatureReportSet(p_ctrl->ClassNbr,
                                                           report_id,
                                                           p_buf,
                                                           report_len);
                      break;

                 case 0:
                 default:
                      break;
             }
             break;


        case USBD_HID_REQ_GET_IDLE:                             /* -------------- GET_IDLE (see Note #1c) ------------- */
             if (dev_to_host != DEF_YES) {
                 break;
             }

             w_value = (CPU_INT08U)((p_setup_req->wValue >> 8u) & DEF_INT_08_MASK);

             if ((p_setup_req->wLength == 1) &&                 /* Chk if setup req is valid.                           */
                 (w_value              == 0)) {

                 report_id = (CPU_INT08U)(p_setup_req->wValue & DEF_INT_08_MASK);
                                                                /* Get idle rate (duration).                            */
                 p_ctrl->CtrlStatusBufPtr[0] = USBD_HID_ReportID_IdleGet(&p_ctrl->Report,
                                                                          report_id,
                                                                         &err);
                 if (err != USBD_ERR_NONE) {
                     break;
                 }

                 USBD_CtrlTx(         p_ctrl->DevNbr,
                             (void *)&p_ctrl->CtrlStatusBufPtr[0],
                                      1u,
                                      USBD_HID_CTRL_REQ_TIMEOUT_mS,
                                      DEF_NO,
                                     &err);
                 if (err == USBD_ERR_NONE) {
                     valid = DEF_OK;
                 }
             }
             break;


        case USBD_HID_REQ_SET_IDLE:                             /* -------------- SET_IDLE (see Note #1d) ------------- */
             if (dev_to_host != DEF_NO) {
                 break;
             }

             if (p_setup_req->wLength > 0) {                    /* Chk if setup req is valid.                           */
                 break;
             }

             idle_rate = (CPU_INT08U)((p_setup_req->wValue >> 8u) & DEF_INT_08_MASK);
             report_id = (CPU_INT08U)( p_setup_req->wValue        & DEF_INT_08_MASK);

             USBD_HID_ReportID_IdleSet(&p_ctrl->Report,
                                        report_id,
                                        idle_rate,
                                       &err);
             if (err == USBD_ERR_NONE) {
                 valid = DEF_OK;
             }
             break;


        case USBD_HID_REQ_GET_PROTOCOL:                         /* ------------ GET_PROTOCOL (see Note #1e) ----------- */
             if ((p_callback              == (USBD_HID_CALLBACK *)0) ||
                 (p_callback->ProtocolGet == (void              *)0)) {
                 break;                                         /* Stall request.                                       */
             }

             if (dev_to_host != DEF_YES) {
                 break;
             }

             if ((p_setup_req->wLength != 1) ||                 /* Chk if setup req is valid.                           */
                 (p_setup_req->wValue  != 0)) {
                 break;
             }
                                                                /* Get currently active protocol from app.              */
             p_ctrl->CtrlStatusBufPtr[0] = p_callback->ProtocolGet(p_ctrl->ClassNbr,
                                                                  &err);
             if (err != USBD_ERR_NONE) {
                 break;
             }
                                                                /* Send active protocol to host (see Note #1e1).        */
             USBD_CtrlTx(         p_ctrl->DevNbr,
                         (void *)&p_ctrl->CtrlStatusBufPtr[0],
                                  1u,
                                  USBD_HID_CTRL_REQ_TIMEOUT_mS,
                                  DEF_NO,
                                 &err);
             if (err == USBD_ERR_NONE) {
                 valid = DEF_OK;
             }
             break;


        case USBD_HID_REQ_SET_PROTOCOL:                         /* ------------ SET_PROTOCOL (see Note #1f) ----------- */
             if ((p_callback              == (USBD_HID_CALLBACK *)0) ||
                 (p_callback->ProtocolSet == (void              *)0)) {
                 break;                                         /* Stall request.                                       */
             }

             if (dev_to_host != DEF_NO) {
                 break;
             }

             if ((p_setup_req->wLength != 0) ||                 /* Chk if setup req is valid.                           */
                ((p_setup_req->wValue  != 0) &&
                 (p_setup_req->wValue  != 1))) {
                 break;
             }

             protocol = (CPU_INT08U)p_setup_req->wValue;
                                                                /* Set new protocol.                                    */
             p_callback->ProtocolSet(p_ctrl->ClassNbr,
                                     protocol,
                                    &err);
             if (err == USBD_ERR_NONE) {
                 valid = DEF_OK;
             }
             break;


        default:
             break;
    }

    return (valid);
}


/*
*********************************************************************************************************
*                                      USBD_HID_OutputDataCmpl()
*
* Description : Inform the class about the set report transfer completion on control endpoint.
*
* Argument(s) : class_nbr   Class instance number.
*
*               p_buf       Pointer to the receive buffer.
*
*               buf_len     Receive buffer length.
*
*               xfer_len    Number of octets received.
*
*               p_arg       Additional argument provided by application.
*
*               err         Transfer status.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_HID_OutputDataCmpl (CPU_INT08U   class_nbr,
                                       void        *p_buf,
                                       CPU_INT32U   buf_len,
                                       CPU_INT32U   xfer_len,
                                       void        *p_arg,
                                       USBD_ERR     err)
{
    CPU_INT32U  *p_xfer_len;


    (void)class_nbr;
    (void)p_buf;
    (void)buf_len;

    p_xfer_len = (CPU_INT32U *)p_arg;

    if (err == USBD_ERR_NONE) {
       *p_xfer_len = xfer_len;
        USBD_HID_OS_OutputDataPost(class_nbr);
    } else {
       *p_xfer_len = 0;
        USBD_HID_OS_OutputDataPendAbort(class_nbr);
    }
}


/*
*********************************************************************************************************
*                                       USBD_HID_RdAsyncCmpl()
*
* Description : Inform the application about the Bulk OUT transfer completion.
*
* Argument(s) : dev_nbr     Device number.
*
*               ep_addr     Endpoint address.
*
*               p_buf       Pointer to the receive buffer.
*
*               buf_len     Receive buffer length.
*
*               xfer_len    Number of octets received.
*
*               p_arg       Additional argument provided by application.
*
*               err         Transfer status: success or error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_HID_RdAsyncCmpl (CPU_INT08U   dev_nbr,
                                    CPU_INT08U   ep_addr,
                                    void        *p_buf,
                                    CPU_INT32U   buf_len,
                                    CPU_INT32U   xfer_len,
                                    void        *p_arg,
                                    USBD_ERR     err)
{
    USBD_HID_COMM        *p_comm;
    USBD_HID_CTRL        *p_ctrl;
    USBD_HID_ASYNC_FNCT     fnct;
    void                 *p_fnct_arg;


    (void)dev_nbr;
    (void)ep_addr;

    p_comm     = (USBD_HID_COMM *)p_arg;
    p_ctrl     =  p_comm->CtrlPtr;
      fnct     =  p_ctrl->IntrRdAsyncFnct;
    p_fnct_arg =  p_ctrl->IntrRdAsyncArgPtr;

    p_comm->DataIntrOutActiveXfer = DEF_NO;                     /* Xfer finished, no more active xfer.                  */

    fnct(p_ctrl->ClassNbr,                                      /* Notify app about xfer completion.                    */
         p_buf,
         buf_len,
         xfer_len,
         p_fnct_arg,
         err);
}


/*
*********************************************************************************************************
*                                       USBD_HID_WrAsyncCmpl()
*
* Description : Inform the application about the Bulk IN transfer completion.
*
* Argument(s) : dev_nbr     Device number.
*
*               ep_addr     Endpoint address.
*
*               p_buf       Pointer to the transmit buffer.
*
*               buf_len     Transmit buffer length.
*
*               xfer_len    Number of octets sent.
*
*               p_arg       Additional argument provided by application.
*
*               err         Transfer status: success or error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_HID_WrAsyncCmpl (CPU_INT08U   dev_nbr,
                                    CPU_INT08U   ep_addr,
                                    void        *p_buf,
                                    CPU_INT32U   buf_len,
                                    CPU_INT32U   xfer_len,
                                    void        *p_arg,
                                    USBD_ERR     err)
{
    USBD_HID_COMM  *p_comm;
    USBD_HID_CTRL  *p_ctrl;


    (void)dev_nbr;
    (void)ep_addr;

    p_comm = (USBD_HID_COMM *)p_arg;
    p_ctrl = (USBD_HID_CTRL *)p_comm->CtrlPtr;

    USBD_HID_OS_TxUnlock(p_ctrl->ClassNbr);

    p_ctrl->IntrWrAsyncFnct(p_ctrl->ClassNbr,                   /* Notify app about xfer completion.                    */
                            p_buf,
                            buf_len,
                            xfer_len,
                            p_ctrl->IntrWrAsyncArgPtr,
                            err);
}


/*
*********************************************************************************************************
*                                        USBD_HID_WrSyncCmpl()
*
* Description : Inform the class about the Bulk IN transfer completion.
*
* Argument(s) : dev_nbr     Device number.
*
*               ep_addr     Endpoint address.
*
*               p_buf       Pointer to the transmit buffer.
*
*               buf_len     Transmit buffer length.
*
*               xfer_len    Number of octets sent.
*
*               p_arg       Additional argument provided by application.
*
*               err         Transfer status: success or error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_HID_WrSyncCmpl (CPU_INT08U   dev_nbr,
                                   CPU_INT08U   ep_addr,
                                   void        *p_buf,
                                   CPU_INT32U   buf_len,
                                   CPU_INT32U   xfer_len,
                                   void        *p_arg,
                                   USBD_ERR     err)
{
    USBD_HID_CTRL  *p_ctrl;
    USBD_HID_COMM  *p_comm;
    CPU_INT08U      class_nbr;


    (void)dev_nbr;
    (void)ep_addr;
    (void)p_buf;
    (void)buf_len;

    p_comm    = (USBD_HID_COMM *)p_arg;
    p_ctrl    =  p_comm->CtrlPtr;
    class_nbr =  p_ctrl->ClassNbr;

    if (err == USBD_ERR_NONE) {
        p_ctrl->DataIntrInXferLen = xfer_len;
        USBD_HID_OS_InputDataPost(class_nbr);
    } else {
        p_ctrl->DataIntrInXferLen = 0;
        USBD_HID_OS_InputDataPendAbort(class_nbr);
    }
}
