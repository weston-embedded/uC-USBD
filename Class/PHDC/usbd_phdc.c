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
**********************************************************************************************************
*
*                                        USB DEVICE PHDC CLASS
*
* Filename : usbd_phdc.c
* Version  : V4.06.01
**********************************************************************************************************
*/

/*
**********************************************************************************************************
*                                            INCLUDE FILES
**********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    USBD_PHDC_MODULE
#include  "usbd_phdc.h"
#include  "usbd_phdc_os.h"
#include  <lib_math.h>


/*
**********************************************************************************************************
*                                               MODULE
**********************************************************************************************************
*/


/*
**********************************************************************************************************
*                                            LOCAL DEFINES
**********************************************************************************************************
*/

#define  USBD_PHDC_COM_NBR_MAX                           (USBD_PHDC_CFG_MAX_NBR_DEV * \
                                                          USBD_PHDC_CFG_MAX_NBR_CFG)

#define  USBD_PHDC_CLASS_FNCT_DESC_LEN                     4
#define  USBD_PHDC_FUNCT_EXTN_DESC_DFLT_LEN                4
#define  USBD_PHDC_EXTRA_EP_DESC_MAX_LEN                 (USBD_PHDC_QOS_DESC_LEN + \
                                                          USBD_PHDC_CFG_DATA_OPAQUE_MAX_LEN + \
                                                          USBD_PHDC_METADATA_DESC_LEN)

#define  USBD_PHDC_QOS_DESC_LEN                            4u
#define  USBD_PHDC_QOS_ENCOD_VER                        0x01u
#define  USBD_PHDC_QOS_VER_OFFSET                         17u


#define  USBD_PHDC_QOS_SIGNATURE                         "PhdcQoSSignature"
#define  USBD_PHDC_QOS_SIGNATURE_LEN                      16u

#define  USBD_PHDC_METADATA_MSG_PREAMBLE_DFLT_LEN         20
#define  USBD_PHDC_METADATA_MSG_PREAMBLE_MAX_LEN         (USBD_PHDC_CFG_DATA_OPAQUE_MAX_LEN + \
                                                          USBD_PHDC_METADATA_MSG_PREAMBLE_DFLT_LEN)

#define  USBD_PHDC_METADATA_DESC_LEN                       2

#define  USBD_PHDC_RELY_LATENCY_FLAGS_EP_BULK_IN         (USBD_PHDC_LATENCY_VERYHIGH_RELY_BEST | \
                                                          USBD_PHDC_LATENCY_HIGH_RELY_BEST     | \
                                                          USBD_PHDC_LATENCY_MEDIUM_RELY_BEST   | \
                                                          USBD_PHDC_LATENCY_MEDIUM_RELY_BETTER | \
                                                          USBD_PHDC_LATENCY_MEDIUM_RELY_GOOD)
#define  USBD_PHDC_RELY_LATENCY_FLAGS_EP_BULK_OUT        (USBD_PHDC_LATENCY_VERYHIGH_RELY_BEST | \
                                                          USBD_PHDC_LATENCY_HIGH_RELY_BEST     | \
                                                          USBD_PHDC_LATENCY_MEDIUM_RELY_BEST)
#define  USBD_PHDC_RELY_LATENCY_FLAGS_EP_INTR_IN         (USBD_PHDC_LATENCY_LOW_RELY_GOOD)

#define  USBD_PHDC_XFER_PRIO_MAX                           6
#define  USBD_PHDC_XFER_PRIO_LOW_LATENCY                   0u

#define  USBD_PHDC_CTRL_REQ_TIMEOUT_mS                  5000u


/*
*********************************************************************************************************
*                                             PHDC NBR EP
*
* Note(s) : (1) PHDC require 2 bulk endpoints (IN and OUT) for very high, high and medium latency
*               transfers. It optionally needs 1 interrupt IN endpoint for low latency transfer. See
*               'USB PHDC Class Specification Overview', Revision 1.0, Section 4.4.1.
*********************************************************************************************************
*/

#define  USBD_PHDC_NBR_EP                                  3u

/*
*********************************************************************************************************
*                                          DESCRIPTOR TYPES
*
* Note(s) : (1) See 'USB PHDC Class Specification Overview', Revision 1.0, Section 8 and Appendix A.
*********************************************************************************************************
*/

#define  USBD_PHDC_DESC_TYPE_CLASS_FNCT                 0x20u
#define  USBD_PHDC_DESC_TYPE_FUNCT_EXTN_11073           0x30u
#define  USBD_PHDC_DESC_TYPE_QOS                        0x21u
#define  USBD_PHDC_DESC_TYPE_METADATA                   0x22u


/*
*********************************************************************************************************
*                                             DATA FORMAT
*
* Note(s) : (1) See 'USB PHDC Class Specification Overview', Revision 1.0, Appendix A.
*********************************************************************************************************
*/

#define  USBD_PHDC_DATA_FMT_VENDOR                      0x01u
#define  USBD_PHDC_DATA_FMT_11073_20601                 0x02u


/*
*********************************************************************************************************
*                                           PHDC CAPABILITY
*
* Note(s) : (1) See 'USB PHDC Class Specification Overview', Revision 1.0, Section 5.2.1.
*********************************************************************************************************
*/

#define  USBD_PHDC_PREAMBLE_CAPABLE                       DEF_BIT_00
#define  USBD_PHDC_PREAMBLE_NOT_CAPABLE                   DEF_BIT_NONE


/*
*********************************************************************************************************
*                                           SUBCLASS CODES
*
* Note(s) : (1) See 'USB PHDC Class Specification Overview', Revision 1.0, Section 5.1.1.
*               The PHDC does not assign a SUBCLASS CODE.
*********************************************************************************************************
*/

#define  USBD_PHDC_SUBCLASS_CODE_NONE                   0x00


/*
*********************************************************************************************************
*                                           PROTOCOL CODES
*
* Note(s) : (1) See 'USB PHDC Class Specification Overview', Revision 1.0, Section 5.1.1.
*               The PHDC does not assign a PROTOCOL CODE.
*********************************************************************************************************
*/

#define  USBD_PHDC_PROTOCOL_CODE_NONE                   0x00


/*
*********************************************************************************************************
*                                          FEATURE SELECTORS
*
* Note(s) : (1) See 'Universal Serial Bus Specification Revision 2.0', Section 9.4, Table 9-6, and
*               Section 9.4.1.
*
*           (2) For a 'clear feature' setup request, the 'wValue' field may contain one of these values.
*********************************************************************************************************
*/

#define  USBD_PHDC_CLASS_SPEC_FEATURE_PHDC_QOS          0x01u


/*
**********************************************************************************************************
*                                           LOCAL CONSTANTS
**********************************************************************************************************
*/


/*
**********************************************************************************************************
*                                          LOCAL DATA TYPES
**********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                        ENDPOINT IDENTIFIERS
*********************************************************************************************************
*/

typedef  enum  usbd_phdc_ep_id {
    USBD_PHDC_EP_BULK_IN = 0x00,
    USBD_PHDC_EP_BULK_OUT,
    USBD_PHDC_EP_INTR_IN
} USBD_PHDC_EP_ID;

/*
*********************************************************************************************************
*                                             PHDC STATE
*********************************************************************************************************
*/

typedef  enum  usbd_phdc_state {
    USBD_PHDC_STATE_NONE = 0,
    USBD_PHDC_STATE_INIT,
    USBD_PHDC_STATE_CFG
} USBD_PHDC_STATE;


/*
*********************************************************************************************************
*                                           PHDC EP PARAMS
*********************************************************************************************************
*/

typedef  struct  usbd_phdc_ep_params {
    const  CPU_INT08U  *DataOpaquePtr;                          /* Opaque data written in metadata desc.                */
           CPU_INT16U   DataOpaqueLen;
} USBD_PHDC_EP_PARAMS;

/*
*********************************************************************************************************
*                                             PHDC PARAMS
*********************************************************************************************************
*/

typedef  struct  usbd_phdc_params {
    CPU_BOOLEAN   DataFmt11073;                                 /* DEF_TRUE if dev use 11073 data format.               */
    CPU_BOOLEAN   PreambleCapable;                              /* DEF_TRUE if dev can send metadata preamble.          */
    CPU_INT16U   *DevSpecialization;                            /* Array of dev specialization codes.                   */
    CPU_INT16U    NbrDevSpecialization;
} USBD_PHDC_PARAMS;

/*
*********************************************************************************************************
*                                        FORWARD DECLARATIONS
*********************************************************************************************************
*/

typedef struct usbd_phdc_ctrl  USBD_PHDC_CTRL;


/*
**********************************************************************************************************
*                                         PRIORITY Q PREAMBLE
**********************************************************************************************************
*/

/*
**********************************************************************************************************
*                                           PHDC COMM INFO
**********************************************************************************************************
*/

typedef struct usbd_phdc_comm {
    USBD_PHDC_CTRL  *CtrlPtr;
    CPU_INT08U       DataBulkOut;
    CPU_INT08U       DataBulkIn;
    CPU_INT08U       DataIntrIn;
} USBD_PHDC_COMM;

/*
**********************************************************************************************************
*                                           PHDC CTRL INFO
*
* Note(s) : (1) Rx and Tx DataXfersTotal holds the value of following tranfers to which QoS settings of a
*               preamble applies. Rx and Tx DataXfersCur indicates what is the count of the current
*               processed transfer.
**********************************************************************************************************
*/

struct usbd_phdc_ctrl {
    CPU_INT08U                     Nbr;                         /* PHDC class instance number.                          */
    CPU_INT08U                     DevNbr;                      /* Device number.                                       */
    USBD_PHDC_STATE                State;                       /* PHDC state.                                          */
    USBD_PHDC_COMM                *CommPtr;                     /* PHDC communication information ptr.                  */

    USBD_PHDC_PARAMS               IF_Params;

    USBD_PHDC_EP_PARAMS            EP_Params[USBD_PHDC_NBR_EP];
    CPU_INT16U                     EP_DataStatus;               /* See 'PHDC specifications Revision 1.0', Section 7.1.2*/
    CPU_INT08U                    *CtrlStatusBufPtr;            /* Buf used for ctrl status xfers.                      */

    CPU_INT08U                     RxDataXfersTotal;            /* See note 1.                                          */
    CPU_INT08U                     RxDataXfersCur;
    LATENCY_RELY_FLAGS             RxLatencyRelyBitmap;

    CPU_INT08U                     TxDataXfersTotal[USBD_PHDC_XFER_PRIO_MAX - 1u];
    CPU_INT08U                     TxDataXfersCur[USBD_PHDC_XFER_PRIO_MAX - 1u];
    LATENCY_RELY_FLAGS             TxLatencyRelyBitmap;
    CPU_INT16U                     TxLowLatencyInterval;        /* Intr EP interval in frames or micro-frames.          */

    CPU_INT08U                    *PreambleBufTxPtr;
    CPU_INT08U                    *PreambleBufRxPtr;
    CPU_BOOLEAN                    PreambleEn;                  /* Indicate if host enabled preamble or not.            */
    USBD_PHDC_PREAMBLE_EN_NOTIFY   PreambleEnNotify;
};


/*
**********************************************************************************************************
*                                            LOCAL TABLES
**********************************************************************************************************
*/


/*
**********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
**********************************************************************************************************
*/

static  USBD_PHDC_CTRL  USBD_PHDC_CtrlTbl[USBD_PHDC_CFG_MAX_NBR_DEV];
static  CPU_INT08U      USBD_PHDC_CtrlNbrNext;

static  USBD_PHDC_COMM  USBD_PHDC_CommTbl[USBD_PHDC_COM_NBR_MAX];
static  CPU_INT16U      USBD_PHDC_CommNbrNext;


/*
**********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
**********************************************************************************************************
*/

static  void         USBD_PHDC_Conn          (       CPU_INT08U       dev_nbr,
                                                     CPU_INT08U       cfg_nbr,
                                                     void            *p_if_class_arg);

static  void         USBD_PHDC_Disconn       (       CPU_INT08U       dev_nbr,
                                                     CPU_INT08U       cfg_nbr,
                                                     void            *p_if_class_arg);

static  void         USBD_PHDC_SettingUpdate (       CPU_INT08U       dev_nbr,
                                                     CPU_INT08U       cfg_nbr,
                                                     CPU_INT08U       if_nbr,
                                                     CPU_INT08U       if_alt_nbr,
                                                     void            *p_if_class_arg,
                                                     void            *p_if_alt_class_arg);

static  void         USBD_PHDC_IF_Desc       (       CPU_INT08U       dev_nbr,
                                                     CPU_INT08U       cfg_nbr,
                                                     CPU_INT08U       if_nbr,
                                                     CPU_INT08U       if_alt_nbr,
                                                     void            *p_if_class_arg,
                                                     void            *p_if_alt_class_arg);

static  CPU_INT16U   USBD_PHDC_IF_DescSizeGet(       CPU_INT08U       dev_nbr,
                                                     CPU_INT08U       cfg_nbr,
                                                     CPU_INT08U       if_nbr,
                                                     CPU_INT08U       if_alt_nbr,
                                                     void            *p_if_class_arg,
                                                     void            *p_if_alt_class_arg);

static  void         USBD_PHDC_EP_Desc       (       CPU_INT08U       dev_nbr,
                                                     CPU_INT08U       cfg_nbr,
                                                     CPU_INT08U       if_nbr,
                                                     CPU_INT08U       if_alt_nbr,
                                                     CPU_INT08U       ep_addr,
                                                     void            *p_if_class_arg,
                                                     void            *p_if_alt_class_arg);

static  CPU_INT16U   USBD_PHDC_EP_DescSizeGet(       CPU_INT08U       dev_nbr,
                                                     CPU_INT08U       cfg_nbr,
                                                     CPU_INT08U       if_nbr,
                                                     CPU_INT08U       if_alt_nbr,
                                                     CPU_INT08U       ep_addr,
                                                     void            *p_if_class_arg,
                                                     void            *p_if_alt_class_arg);

static  CPU_BOOLEAN  USBD_PHDC_ClassReq      (       CPU_INT08U       dev_nbr,
                                              const  USBD_SETUP_REQ  *p_setup_req,
                                                     void            *p_if_class_arg);


/*
*********************************************************************************************************
*                                          PHDC CLASS DRIVER
*********************************************************************************************************
*/

static  USBD_CLASS_DRV  USBD_PHDC_Drv = {
    USBD_PHDC_Conn,
    USBD_PHDC_Disconn,
    USBD_PHDC_SettingUpdate,
    DEF_NULL,
    USBD_PHDC_IF_Desc,
    USBD_PHDC_IF_DescSizeGet,
    USBD_PHDC_EP_Desc,
    USBD_PHDC_EP_DescSizeGet,
    DEF_NULL,
    USBD_PHDC_ClassReq,
    DEF_NULL,

#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
    DEF_NULL,
    DEF_NULL
#endif
};


/*
**********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
**********************************************************************************************************
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
*                                          USBD_PHDC_Init()
*
* Description : Initialize PHDC.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE   PHDC initialized successfully.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/


void  USBD_PHDC_Init (USBD_ERR  *p_err)
{
    CPU_INT08U       ix;
    CPU_INT08U       i;
    USBD_PHDC_CTRL  *p_ctrl;
    USBD_PHDC_COMM  *p_comm;
    CPU_SIZE_T       ctrl_buf_len;
    LIB_ERR          err_lib;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

    for (ix = 0u; ix < USBD_PHDC_CFG_MAX_NBR_DEV; ix++) {       /* Init PHDC ctrl struct.                               */
        p_ctrl          = &USBD_PHDC_CtrlTbl[ix];
        p_ctrl->Nbr     =  ix;
        p_ctrl->DevNbr  =  USBD_DEV_NBR_NONE;
        p_ctrl->State   =  USBD_PHDC_STATE_NONE;
        p_ctrl->CommPtr = (USBD_PHDC_COMM *)0;

        p_ctrl->EP_Params[0].DataOpaquePtr = (const CPU_INT08U *)0;
        p_ctrl->EP_Params[1].DataOpaquePtr = (const CPU_INT08U *)0;
        p_ctrl->EP_Params[2].DataOpaquePtr = (const CPU_INT08U *)0;
        p_ctrl->EP_Params[0].DataOpaqueLen = 0;
        p_ctrl->EP_Params[1].DataOpaqueLen = 0;
        p_ctrl->EP_Params[2].DataOpaqueLen = 0;
        p_ctrl->EP_DataStatus              = DEF_BIT_NONE;

        p_ctrl->RxDataXfersTotal    = 0u;
        p_ctrl->RxDataXfersCur      = 0u;
        p_ctrl->RxLatencyRelyBitmap = DEF_BIT_NONE;

        p_ctrl->TxLatencyRelyBitmap  = DEF_BIT_NONE;
        p_ctrl->TxLowLatencyInterval = 0u;
        p_ctrl->PreambleEn           = DEF_DISABLED;

        for (i = 0; i < (USBD_PHDC_XFER_PRIO_MAX - 1u); i++) {
            p_ctrl->TxDataXfersTotal[i] = 0u;
            p_ctrl->TxDataXfersCur[i]   = 0u;
        }

        p_ctrl->PreambleBufTxPtr = (CPU_INT08U *)Mem_HeapAlloc(              USBD_PHDC_METADATA_MSG_PREAMBLE_MAX_LEN,
                                                                             USBD_CFG_BUF_ALIGN_OCTETS,
                                                               (CPU_SIZE_T *)DEF_NULL,
                                                                            &err_lib);
        if (err_lib != LIB_MEM_ERR_NONE) {
           *p_err = USBD_ERR_ALLOC;
            return;
        }

        p_ctrl->PreambleBufRxPtr = (CPU_INT08U *)Mem_HeapAlloc(              USBD_PHDC_METADATA_MSG_PREAMBLE_MAX_LEN,
                                                                             USBD_CFG_BUF_ALIGN_OCTETS,
                                                               (CPU_SIZE_T *)DEF_NULL,
                                                                            &err_lib);
        if (err_lib != LIB_MEM_ERR_NONE) {
           *p_err = USBD_ERR_ALLOC;
            return;
        }

        Mem_Copy((void *)p_ctrl->PreambleBufTxPtr,              /* Init preamble template.                              */
                 (void *)USBD_PHDC_QOS_SIGNATURE,
                         USBD_PHDC_QOS_SIGNATURE_LEN);
        p_ctrl->PreambleBufTxPtr[17] = USBD_PHDC_QOS_ENCOD_VER;

        ctrl_buf_len = sizeof(CPU_ADDR);
        if (sizeof(CPU_ADDR) < 2u) {
            ctrl_buf_len = 2u;
        }
        p_ctrl->CtrlStatusBufPtr = (CPU_INT08U *)Mem_HeapAlloc(              ctrl_buf_len,
                                                                             USBD_CFG_BUF_ALIGN_OCTETS,
                                                               (CPU_SIZE_T *)DEF_NULL,
                                                                            &err_lib);
        if (err_lib != LIB_MEM_ERR_NONE) {
           *p_err = USBD_ERR_ALLOC;
            return;
        }

        p_ctrl->IF_Params.NbrDevSpecialization = 0;
        p_ctrl->IF_Params.DataFmt11073         = DEF_NO;
        p_ctrl->IF_Params.PreambleCapable      = DEF_NO;
    }

    for (ix = 0u; ix < USBD_PHDC_COM_NBR_MAX; ix++) {           /* Init PHDC comm tbl.                                  */
        p_comm = &USBD_PHDC_CommTbl[ix];

        p_comm->DataBulkIn  = USBD_EP_ADDR_NONE;
        p_comm->DataBulkOut = USBD_EP_ADDR_NONE;
        p_comm->DataIntrIn  = USBD_EP_ADDR_NONE;
        p_comm->CtrlPtr     = (USBD_PHDC_CTRL *)0;
    }

    USBD_PHDC_CtrlNbrNext = 0;
    USBD_PHDC_CommNbrNext = 0;

    USBD_PHDC_OS_Init(p_err);
}


/*
*********************************************************************************************************
*                                           USBD_PHDC_Add()
*
* Description : Add a new instance of PHDC.
*
* Argument(s) : data_fmt_11073          Class instance use ieee 11073 protocol if set to DEF_YES.
*                                       Otherwise, vendor defined protocol is used.
*
*               preamble_capable        Class instance support metadata message preamble if set
*                                       to DEF_YES.
*
*               preamble_en_notify      Pointer to a callback function that notify application if
*                                       metadata message preambles are enabled.
*
*               low_latency_interval    Interrupt endpoint interval in milliseconds. Can be 0
*                                       if PHDC device will not send low latency data. When the value
*                                       is different from 0, it must be a power of 2.
*
*               p_err                   Pointer to variable that will receive the return error
*                                       code from this function :
*
*                               USBD_ERR_NONE                   PHDC instance successfully added.
*                               USBD_ERR_ALLOC                  No more class instance structure available.
*
* Return(s)   : PHDC device number, if NO error(s).
*
*               USBD_PHDC_NBR_NONE, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  USBD_PHDC_Add (CPU_BOOLEAN                    data_fmt_11073,
                           CPU_BOOLEAN                    preamble_capable,
                           USBD_PHDC_PREAMBLE_EN_NOTIFY   preamble_en_notify,
                           CPU_INT16U                     low_latency_interval,
                           USBD_ERR                      *p_err)
{
    CPU_INT08U       phdc_nbr;
    USBD_PHDC_CTRL  *p_ctrl;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }

    if ((low_latency_interval               != 0u) &&
        (MATH_IS_PWR2(low_latency_interval) == DEF_NO)) {       /* Interval must be a power of 2.                       */
       *p_err = USBD_ERR_INVALID_ARG;
        return (USBD_CLASS_NBR_NONE);
    }
#endif

    CPU_CRITICAL_ENTER();
    phdc_nbr = USBD_PHDC_CtrlNbrNext;                           /* Alloc new PHDC instance.                             */

    if (phdc_nbr >= USBD_PHDC_CFG_MAX_NBR_DEV) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_PHDC_INSTANCE_ALLOC;
        return (USBD_CLASS_NBR_NONE);
    }

    USBD_PHDC_CtrlNbrNext++;
    CPU_CRITICAL_EXIT();

    p_ctrl = &USBD_PHDC_CtrlTbl[phdc_nbr];

    p_ctrl->IF_Params.DataFmt11073    = data_fmt_11073;
    p_ctrl->IF_Params.PreambleCapable = preamble_capable;
    p_ctrl->PreambleEnNotify          = preamble_en_notify;
    p_ctrl->TxLowLatencyInterval      = low_latency_interval;

    return (phdc_nbr);
}


/*
*********************************************************************************************************
*                                         USBD_PHDC_CfgAdd()
*
* Description : Add PHDC instance into USB device configuration.
*
* Argument(s) : class_nbr   PHDC instance number.
*
*               dev_nbr     Device number.
*
*               cfg_nbr     Configuration index to add new PHDC interface to.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   PHDC instance successfully added.
*                               USBD_ERR_INVALID_ARG,           if invalid argument(s) passed to class_nbr.
*                               USBD_ERR_ALLOC                  No more class communication structure available.
*
*                                                               --------- RETURNED BY USBD_IF_Add() : ---------
*                               USBD_ERR_NULL_PTR               Invalid NULL pointer was passed to 3rd argument.
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_IF_ALLOC               NO available device interfaces.
*
*                                                               -------- RETURNED BY USBD_BulkAdd() : --------
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_CFG_INVALID_NBR        Invalid configuration number.
*                               USBD_ERR_IF_ALTINVALID_NBR      Invalid interface alternative setting number.
*                               USBD_ERR_IF_INVALID_NBR         Invalid interface number.
*
*                                                               --------- RETURNED BY USBD_IF_Add() : ---------
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_CFG_INVALID_NBR        Invalid configuration number.
*                               USBD_ERR_IF_ALTINVALID_NBR      Invalid interface alternative setting number.
*                               USBD_ERR_IF_INVALID_NBR         Invalid interface number.
*
* Return(s)   : DEF_YES, if PHDC instance added to USB device configuration successfully.
*
*               DEF_NO,  otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  USBD_PHDC_CfgAdd (CPU_INT08U   class_nbr,
                               CPU_INT08U   dev_nbr,
                               CPU_INT08U   cfg_nbr,
                               USBD_ERR    *p_err)
{
    CPU_BOOLEAN      ep_intr_en;
    CPU_INT08U       if_nbr;
    CPU_INT08U       ep_addr;
    CPU_INT16U       comm_nbr;
    CPU_INT16U       tx_low_latency_interval;
    USBD_PHDC_CTRL  *p_ctrl;
    USBD_PHDC_COMM  *p_comm;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(DEF_NO);
    }
#endif

    if (class_nbr >= USBD_PHDC_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (DEF_NO);
    }

    p_ctrl = &USBD_PHDC_CtrlTbl[class_nbr];

    if ((p_ctrl->RxLatencyRelyBitmap == (LATENCY_RELY_FLAGS)0) ||
        (p_ctrl->TxLatencyRelyBitmap == (LATENCY_RELY_FLAGS)0)) {
        *p_err = USBD_ERR_INVALID_CLASS_STATE;
         return (DEF_NO);
    }

    CPU_CRITICAL_ENTER();
    comm_nbr = USBD_PHDC_CommNbrNext;                           /* Alloc new PHDC comm info.                            */

    if (comm_nbr >= USBD_PHDC_COM_NBR_MAX) {
        USBD_PHDC_CtrlNbrNext--;
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_PHDC_INSTANCE_ALLOC;
        return (DEF_NO);
    }

    USBD_PHDC_CommNbrNext++;
    CPU_CRITICAL_EXIT();

    p_comm = &USBD_PHDC_CommTbl[comm_nbr];

    if_nbr = USBD_IF_Add(        dev_nbr,                       /* Add PHDC to cfg.                                         */
                                 cfg_nbr,
                                &USBD_PHDC_Drv,
                         (void *)p_comm,
                         (void *)0,
                                 USBD_CLASS_CODE_PERSONAL_HEALTHCARE,
                                 USBD_PHDC_SUBCLASS_CODE_NONE,
                                 USBD_PHDC_PROTOCOL_CODE_NONE,
                                 "Personal Healthcare Device Class",
                                 p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (DEF_NO);
    }

                                                                /* --------- ADD ENPOINTS TO DEFAULT INTERFACE -------- */
    ep_addr = USBD_BulkAdd(dev_nbr,                             /* Add bulk (IN) EP.                                    */
                           cfg_nbr,
                           if_nbr,
                           0u,
                           DEF_YES,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (DEF_NO);
    }

    p_comm->DataBulkIn = ep_addr;                               /* Store bulk (IN) EP address.                          */


    ep_addr = USBD_BulkAdd(dev_nbr,                             /* Add bulk (OUT) EP.                                   */
                           cfg_nbr,
                           if_nbr,
                           0u,
                           DEF_NO,
                           0u,
                           p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (DEF_NO);
    }

    p_comm->DataBulkOut = ep_addr;


    ep_intr_en = DEF_BIT_IS_SET(p_ctrl->TxLatencyRelyBitmap, USBD_PHDC_RELY_LATENCY_FLAGS_EP_INTR_IN);
    if (ep_intr_en == DEF_TRUE) {

        if (DEF_BIT_IS_CLR(cfg_nbr, USBD_CFG_NBR_SPD_BIT) == DEF_YES) {
                                                                /* In FS, bInterval in frames.                          */
            tx_low_latency_interval = p_ctrl->TxLowLatencyInterval;
        } else {                                                /* In HS, bInterval in microframes.                     */
            tx_low_latency_interval = p_ctrl->TxLowLatencyInterval * 8u;
        }

        ep_addr = USBD_IntrAdd(dev_nbr,                         /* Add intr (IN) EP.                                    */
                               cfg_nbr,
                               if_nbr,
                               0u,
                               DEF_YES,
                               0u,
                               tx_low_latency_interval,
                               p_err);
        if (*p_err != USBD_ERR_NONE) {
            return (DEF_NO);
        }

        p_comm->DataIntrIn = ep_addr;                           /* Store intr (IN) EP address.                          */
    }

    CPU_CRITICAL_ENTER();
    p_comm->CtrlPtr = p_ctrl;
    p_ctrl->State   = USBD_PHDC_STATE_INIT;
    p_ctrl->DevNbr  = dev_nbr;
    p_ctrl->CommPtr = (USBD_PHDC_COMM *)0;
    CPU_CRITICAL_EXIT();

   *p_err = USBD_ERR_NONE;

    return (DEF_YES);
}


/*
*********************************************************************************************************
*                                        USBD_PHDC_IsConn()
*
* Description : Get PHDC connection state.
*
* Argument(s) : class_nbr   Class instance number.
*
* Return(s)   : DEF_YES, if PHDC is connected.
*
*               DEF_NO,  otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  USBD_PHDC_IsConn (CPU_INT08U  class_nbr)
{
    USBD_ERR         err;
    USBD_PHDC_CTRL  *p_ctrl;
    USBD_DEV_STATE   state;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (class_nbr >= USBD_PHDC_CtrlNbrNext) {
        return (DEF_NO);
    }
#endif

    p_ctrl = &USBD_PHDC_CtrlTbl[class_nbr];
    state  = USBD_DevStateGet(p_ctrl->DevNbr, &err);            /* Get dev state.                                       */

    if ((err           == USBD_ERR_NONE            ) &&
        (state         == USBD_DEV_STATE_CONFIGURED) &&
        (p_ctrl->State == USBD_PHDC_STATE_CFG      )) {
        return (DEF_YES);
    }

    return (DEF_NO);
}


/*
*********************************************************************************************************
*                                          USBD_PHDC_RdCfg()
*
* Description : Configure read communication pipe parameters.
*
* Argument(s) : class_nbr           PHDC instance number.
*
*               latency_rely        Latency / reliability flags. Can be one or more of these values:
*
*                               USBD_PHDC_LATENCY_VERYHIGH_RELY_BEST    Very high latency, best reliability
*                               USBD_PHDC_LATENCY_HIGH_RELY_BEST,       High latency, best reliability
*                               USBD_PHDC_LATENCY_MEDIUM_RELY_BEST,     Medium latency, best reliability
*
*               p_data_opaque       Pointer to a buffer that contains opaque data.
*
*               data_opaque_len     Opaque data length. If 0, no metadata descriptor will be written.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   if communication pipe has been succesfully initialized.
*                               USBD_ERR_INVALID_ARG            if invalid argument(s) passed to class_nbr or latency_rely.
*                               USBD_ERR_NULL_PTR               if p_data_opaque is an invalid null pointer.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_PHDC_RdCfg (       CPU_INT08U           class_nbr,
                              LATENCY_RELY_FLAGS   latency_rely,
                       const  CPU_INT08U          *p_data_opaque,
                              CPU_INT08U           data_opaque_len,
                              USBD_ERR            *p_err)
{
    LATENCY_RELY_FLAGS   other_latency_rely;
    LATENCY_RELY_FLAGS   latency_rely_test;
    USBD_PHDC_CTRL      *p_ctrl;
    CPU_INT08U           latency_rely_bit;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }

    if ((p_data_opaque   == (CPU_INT08U *)0 ) &&
        (data_opaque_len !=               0u)) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }
    if (latency_rely == DEF_BIT_NONE) {
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }
#endif

    if (class_nbr >= USBD_PHDC_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    p_ctrl = &USBD_PHDC_CtrlTbl[class_nbr];

    if (p_ctrl->State != USBD_PHDC_STATE_NONE) {
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
    }

    if (p_ctrl->IF_Params.PreambleCapable == DEF_NO) {          /* If device is not capable of preamble send/recv...    */
        latency_rely_test = latency_rely;
        latency_rely_bit  = CPU_CntTrailZeros08(latency_rely);
        DEF_BIT_CLR(latency_rely_test, DEF_BIT(latency_rely_bit));

        if (latency_rely_test != DEF_BIT_NONE) {
           *p_err = USBD_ERR_INVALID_ARG;                       /* ... only one QoS bin shall be set.                   */
            return;
        }
    }
                                                                /* Rx pipe can only have best rely.                     */
    other_latency_rely = latency_rely;
    DEF_BIT_CLR(other_latency_rely, USBD_PHDC_RELY_LATENCY_FLAGS_EP_BULK_OUT);
    if (other_latency_rely != DEF_BIT_NONE) {
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }

    CPU_CRITICAL_ENTER();
    p_ctrl->EP_Params[USBD_PHDC_EP_BULK_OUT].DataOpaquePtr = p_data_opaque;
    p_ctrl->EP_Params[USBD_PHDC_EP_BULK_OUT].DataOpaqueLen = data_opaque_len;

    p_ctrl->RxLatencyRelyBitmap = latency_rely;
    CPU_CRITICAL_EXIT();

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          USBD_PHDC_WrCfg()
*
* Description : Configure write communication pipe parameters.
*
* Argument(s) : class_nbr           PHDC instance number.
*
*               latency_rely        Latency / reliability flags. Can be one or more of these values:
*
*                               USBD_PHDC_LATENCY_VERYHIGH_RELY_BEST    Very high latency, best reliability
*                               USBD_PHDC_LATENCY_HIGH_RELY_BEST        High latency, best reliability
*                               USBD_PHDC_LATENCY_MEDIUM_RELY_BEST      Medium latency, best reliability
*                               USBD_PHDC_LATENCY_MEDIUM_RELY_BETTER    Medium latency, better reliability
*                               USBD_PHDC_LATENCY_MEDIUM_RELY_GOOD      Medium latency, good reliability
*                               USBD_PHDC_LATENCY_LOW_RELY_GOOD         Low latency, good reliability
*
*               p_data_opaque       Pointer to a buffer that contains opaque data.
*
*               data_opaque_len     Opaque data length. If 0, no metadata descriptor will be written.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   if communication pipe has been succesfully initialized.
*                               USBD_ERR_INVALID_ARG            if invalid argument(s) passed to class_nbr or latency_rely.
*                               USBD_ERR_NULL_PTR               if p_data_opaque is an invalid null pointer.
*
* Return(s)   : none.
*
* Note(s)     : (1) If application needs to have different opaque data for low latency pipe, than
*                   this function should be called twice. Once for pipe carrying medium /
*                   high / very high latency transfers, and once for pipe carrying low latency transfers.
*********************************************************************************************************
*/

void  USBD_PHDC_WrCfg (       CPU_INT08U           class_nbr,
                              LATENCY_RELY_FLAGS   latency_rely,
                       const  CPU_INT08U          *p_data_opaque,
                              CPU_INT08U           data_opaque_len,
                              USBD_ERR            *p_err)
{
    CPU_BOOLEAN          ep_bulk;
    CPU_BOOLEAN          ep_intr;
    CPU_INT08U           latency_rely_bit;
    LATENCY_RELY_FLAGS   latency_rely_test;
    USBD_PHDC_CTRL      *p_ctrl;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }

    if ((p_data_opaque   == (CPU_INT08U *)0) &&
        (data_opaque_len !=               0)) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }

    if (latency_rely == 0) {
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }
#endif

    if (class_nbr >= USBD_PHDC_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    p_ctrl = &USBD_PHDC_CtrlTbl[class_nbr];

    if (p_ctrl->State != USBD_PHDC_STATE_NONE) {
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return;
    }

    if (p_ctrl->IF_Params.PreambleCapable == DEF_NO) {
        latency_rely_test = latency_rely;
        latency_rely_bit  = CPU_CntTrailZeros08(latency_rely);
        DEF_BIT_CLR(latency_rely_test, DEF_BIT(latency_rely_bit));

        if (latency_rely_test != DEF_BIT_NONE) {
           *p_err = USBD_ERR_INVALID_ARG;                       /* ... not preamble capable.                            */
            return;
        }
    }

    ep_bulk = DEF_BIT_IS_SET_ANY(latency_rely, USBD_PHDC_RELY_LATENCY_FLAGS_EP_BULK_IN);
    ep_intr = DEF_BIT_IS_SET_ANY(latency_rely, USBD_PHDC_RELY_LATENCY_FLAGS_EP_INTR_IN);

    CPU_CRITICAL_ENTER();
    p_ctrl->TxLatencyRelyBitmap |= latency_rely;
                                                                /* Check if dev need intr EP                            */
    if (ep_intr == DEF_TRUE) {
        p_ctrl->EP_Params[USBD_PHDC_EP_INTR_IN].DataOpaquePtr = p_data_opaque;
        p_ctrl->EP_Params[USBD_PHDC_EP_INTR_IN].DataOpaqueLen = data_opaque_len;
    }

    if (ep_bulk == DEF_TRUE) {
        p_ctrl->EP_Params[USBD_PHDC_EP_BULK_IN].DataOpaquePtr = p_data_opaque;
        p_ctrl->EP_Params[USBD_PHDC_EP_BULK_IN].DataOpaqueLen = data_opaque_len;
    }

    CPU_CRITICAL_EXIT();

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                      USBD_PHDC_11073_ExtCfg()
*
* Description : Configure function extension for given class instance.
*
* Argument(s) : class_nbr                      PHDC instance number.
*
*               p_dev_specialization           Pointer to an array that contains a list of device
*                                              specializations. (See 'Personal Healthcare Device Class
*                                              specifications Revision 1.0', Appendix A.)
*
*               nbr_dev_specialization         Number of device specialzations specified in p_fnct_ext array.
*
*               p_err                          Pointer to variable that will receive the return error
*                                              code from this function :
*
*                               USBD_ERR_NONE                   if function extensions successfully configured.
*                               USBD_ERR_INVALID_ARG            if invalid argument(s) passed to class_nbr.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_PHDC_11073_ExtCfg (CPU_INT08U   class_nbr,
                              CPU_INT16U  *p_dev_specialization,
                              CPU_INT08U   nbr_dev_specialization,
                              USBD_ERR    *p_err)
{
    USBD_PHDC_CTRL  *p_ctrl;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

    if (class_nbr >= USBD_PHDC_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    p_ctrl                                 = &USBD_PHDC_CtrlTbl[class_nbr];
    p_ctrl->IF_Params.DevSpecialization    =    p_dev_specialization;
    p_ctrl->IF_Params.NbrDevSpecialization =  nbr_dev_specialization;
}


/*
**********************************************************************************************************
*                                       USBD_PHDC_PreambleRd()
*
* Description : Read metadata preamble. This function is blocking.
*
* Argument(s) : class_nbr   PHDC class instance number.
*
*               p_buf       Pointer to buffer that will contain opaque data from metadata
*                           message preamble.
*
*               buf_len     Opaque data buffer length in octets.
*
*               p_nbr_xfer  Pointer to a variable that will contain number of transfer the preamble apply to.
*                           After this call, USBD_PHDC_Rd shall be called nbr_xfer times by the application.
*
*               timeout     Timeout (ms).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Data successfully received from host.
*                               USBD_ERR_INVALID_CLASS_STATE    Invalid PHDC state.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to class_nbr.
*                               USBD_ERR_NULL_PTR               Invalid NULL receive buffer.
*                               USBD_ERR_RX                     No metadata preamble expected from host or
*                                                               invalid metadata preamble received.
*
*                                                               ------ RETURNED BY USBD_BulkRx() : ------
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_EP_INVALID_NBR         Invalid endpoint number.
*                               USBD_ERR_DEV_INVALID_STATE      Bulk transfer can ONLY be used after the
*                                                               device is in configured state.
*                               USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*
*                                                               --- RETURNED BY USBD_PHDC_OS_RdLock() ---
*                               USBD_ERR_NONE                   OS signal     successfully acquired.
*                               USBD_OS_ERR_TIMEOUT             OS signal NOT successfully acquired in the time
*                                                               specified by 'timeout_ms'.
*                               USBD_OS_ERR_ABORT               OS signal aborted.
*                               USBD_OS_ERR_FAIL                OS signal not acquired because another error.
*
* Return(s)   : Opaque data length in octets, if no error(s).
*
*               0,                            otherwise.
*
* Note(s)     : (1) Metadata message preamble contains the following fields (see 'USB PHDC Class Specification
*                   Overview', Revision 1.0, Section 6.1.):
*
*                   +--------+-----------------------+----------------------+-----------------------------------+
*                   | Offset |        Field          |         Size         |            Description            |
*                   +--------+-----------------------+----------------------+-----------------------------------+
*                   |   0    | aSignature            |          16          | Cst used to verify preamble.      |
*                   |        |                       |                      | Set to  PhdcQoSSignature  string. |
*                   +--------+-----------------------+----------------------+-----------------------------------+
*                   |   16   | bNumTransfers         |           1          | Cnt of following xfers to which   |
*                   |        |                       |                      | QoS setting applies.              |
*                   +--------+-----------------------+----------------------+-----------------------------------+
*                   |   17   | bQoSEncodingVersion   |           1          | QoS info encoding version. Set to |
*                   |        |                       |                      | 0x01 for the moment.              |
*                   +--------+-----------------------+----------------------+-----------------------------------+
*                   |   18   | bmLatencyReliability  |           1          | Bitmap that refers to latency/rely|
*                   |        |                       |                      | bin for data. Shall be set to one |
*                   |        |                       |                      | of the value specified in section |
*                   |        |                       |                      | LATENCY / RELIABILITY BITMAPS of  |
*                   |        |                       |                      | usbd_phdc.h.                      |
*                   +--------+-----------------------+----------------------+-----------------------------------+
*                   |   19   | bOpaqueDataSize       |           1          | Length, in bytes, of opaque data. |
*                   +--------+-----------------------+----------------------+-----------------------------------+
*                   |   20   | bOpaqueData           | [0 ..                | Opaque data.                      |
*                   |        |                       |  MaxPacketSize - 21] |                                   |
*                   +--------+-----------------------+----------------------+-----------------------------------+
*
*               (2) If host disable preamble while the application is pending on this function, the call will
*                   immediately return with error USBD_OS_ERR_ABORT.
*
**********************************************************************************************************
*/

CPU_INT08U  USBD_PHDC_PreambleRd (CPU_INT08U   class_nbr,
                                  void        *p_buf,
                                  CPU_INT08U   buf_len,
                                  CPU_INT08U  *p_nbr_xfer,
                                  CPU_INT16U   timeout,
                                  USBD_ERR    *p_err)
{
    USBD_PHDC_CTRL  *p_ctrl;
    USBD_PHDC_COMM  *p_comm;
    CPU_INT08U       data_opaque_len;
    CPU_INT16U       xfer_len;
    CPU_BOOLEAN      preamble_valid;
    CPU_BOOLEAN      latency_rely_valid;


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

    if (class_nbr >= USBD_PHDC_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (0u);
    }

    p_ctrl = &USBD_PHDC_CtrlTbl[class_nbr];
    if (p_ctrl->State != USBD_PHDC_STATE_CFG) {
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return (0u);
    }

    if (p_ctrl->PreambleEn == DEF_DISABLED) {                   /* Chk that preamble is expected from host.             */
       *p_err = USBD_ERR_RX;
        return (0u);
    }

    if (p_ctrl->RxDataXfersTotal != 0) {
       *p_err = USBD_ERR_RX;
        return (0u);
    }

    USBD_PHDC_OS_RdLock(class_nbr, timeout, p_err);             /* Lock rd xfer.                                        */
    if (*p_err != USBD_ERR_NONE) {
        return (0u);
    }
    if (p_ctrl->PreambleEn == DEF_DISABLED) {                   /* Make sure preamble not been disabled while pending.  */
       *p_err = USBD_ERR_RX;
        USBD_PHDC_OS_RdUnlock(class_nbr);
        return (0u);
    }

    p_comm = p_ctrl->CommPtr;

    xfer_len = USBD_BulkRx(p_ctrl->DevNbr,
                           p_comm->DataBulkOut,
                           p_ctrl->PreambleBufRxPtr,
                           USBD_PHDC_METADATA_MSG_PREAMBLE_MAX_LEN,
                           timeout,
                           p_err);

    if (*p_err != USBD_ERR_NONE) {
        if (*p_err == USBD_ERR_OS_ABORT) {                      /* Xfer is aborted when preamble status change.         */
           *p_err = USBD_ERR_RX;
        }
        USBD_PHDC_OS_RdUnlock(class_nbr);
        return (0u);
    }

    preamble_valid = DEF_NO;
    if (xfer_len >= USBD_PHDC_METADATA_MSG_PREAMBLE_DFLT_LEN) { /* Validate metadata msg preamble.                      */
        preamble_valid  = Mem_Cmp(p_ctrl->PreambleBufRxPtr,     /* Chk if preamble contain signature.                   */
                                  USBD_PHDC_QOS_SIGNATURE,
                                  USBD_PHDC_QOS_SIGNATURE_LEN);

                                                                /* Chk QoS, QoS encoding version and nbr xfers is valid.*/
        latency_rely_valid = DEF_BIT_IS_SET(p_ctrl->RxLatencyRelyBitmap, p_ctrl->PreambleBufRxPtr[18]);
        preamble_valid     = (p_ctrl->PreambleBufRxPtr[16] >  0u                     ) ? preamble_valid : DEF_NO;
        preamble_valid     = (p_ctrl->PreambleBufRxPtr[17] == USBD_PHDC_QOS_ENCOD_VER) ? preamble_valid : DEF_NO;
        preamble_valid     = (latency_rely_valid        == DEF_YES                   ) ? preamble_valid : DEF_NO;
    }

    if (preamble_valid == DEF_NO) {
        USBD_PHDC_OS_RdUnlock(class_nbr);
        USBD_EP_Stall(p_ctrl->DevNbr,
                      p_comm->DataBulkOut,
                      DEF_SET,
                      p_err);
       *p_err = USBD_ERR_RX;
        return (0u);
    }

    p_ctrl->RxDataXfersTotal = p_ctrl->PreambleBufRxPtr[16];    /* Update xfers ctr.                                    */
    p_ctrl->RxDataXfersCur   = 0u;

   *p_nbr_xfer      = p_ctrl->PreambleBufRxPtr[16];             /* Return nbr xfers and opaque data to app.             */
    data_opaque_len = DEF_MIN(p_ctrl->PreambleBufRxPtr[19], buf_len);
    if (data_opaque_len > 0) {
        Mem_Copy(p_buf, (void *)&p_ctrl->PreambleBufRxPtr[20], data_opaque_len);
    }

    USBD_PHDC_OS_RdUnlock(class_nbr);                           /* Unlock rd xfer.                                      */

   *p_err = USBD_ERR_NONE;

    return (data_opaque_len);
}


/*
**********************************************************************************************************
*                                           USBD_PHDC_Rd()
*
* Description : Read PHDC data. This function is blocking.
*
* Argument(s) : class_nbr   PHDC class instance number.
*
*               p_buf       Pointer to buffer that will contain data.
*
*               buf_len     Receive buffer length in octets.
*
*               timeout     Timeout (ms).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Data successfully received from host.
*                               USBD_ERR_INVALID_CLASS_STATE    Invalid PHDC state.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to class_nbr.
*                               USBD_ERR_NULL_PTR               Invalid NULL receive buffer.
*                               USBD_ERR_RX                     A preamble is expected. USBD_PHDC_PreambleRd
*                                                               shall be called instead.
*
*                                                               ------ RETURNED BY USBD_BulkRx() : ------
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_EP_INVALID_NBR         Invalid endpoint number.
*                               USBD_ERR_DEV_INVALID_STATE      Bulk transfer can ONLY be used after the
*                                                               device is in configured state.
*                               USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*
*                                                               --- RETURNED BY USBD_PHDC_OS_RdLock() ---
*                               USBD_ERR_NONE                   OS signal     successfully acquired.
*                               USBD_OS_ERR_TIMEOUT             OS signal NOT successfully acquired in the time
*                                                               specified by 'timeout_ms'.
*                               USBD_OS_ERR_ABORT               OS signal aborted.
*                               USBD_OS_ERR_FAIL                OS signal not acquired because another error.
*
* Return(s)   : Number of octets received, if no error(s).
*
*               0,                         otherwise.
*
* Note(s)     : (1) If host enable preamble while the application is pending on this function, the call will
*                   immediately return with error USBD_OS_ERR_ABORT.
**********************************************************************************************************
*/

CPU_INT16U  USBD_PHDC_Rd (CPU_INT08U   class_nbr,
                          void        *p_buf,
                          CPU_INT16U   buf_len,
                          CPU_INT16U   timeout,
                          USBD_ERR    *p_err)
{
    USBD_PHDC_CTRL  *p_ctrl;
    USBD_PHDC_COMM  *p_comm;
    CPU_INT16U       xfer_len;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }

    if (p_buf == (void *)0) {
       *p_err = USBD_ERR_NULL_PTR;
        return (0u);
    }
#endif

    if (class_nbr >= USBD_PHDC_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (0u);
    }

    p_ctrl = &USBD_PHDC_CtrlTbl[class_nbr];
    if (p_ctrl->State != USBD_PHDC_STATE_CFG) {
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return (0u);
    }

    if ((p_ctrl->PreambleEn       == DEF_ENABLED)  &&
        (p_ctrl->RxDataXfersTotal == 0u         )) {
       *p_err = USBD_ERR_RX;
        return (0u);
    }

    USBD_PHDC_OS_RdLock(class_nbr, timeout, p_err);             /* Lock rd pipe.                                        */
    if (*p_err != USBD_ERR_NONE) {
        return (0u);
    }
    if ((p_ctrl->PreambleEn       == DEF_ENABLED)  &&           /* Make sure preamble status not changed while pending. */
        (p_ctrl->RxDataXfersTotal == 0u         )) {
       *p_err = USBD_ERR_RX;
        USBD_PHDC_OS_RdUnlock(class_nbr);
        return (0u);
    }

    p_comm   = p_ctrl->CommPtr;
    xfer_len = USBD_BulkRx(              p_ctrl->DevNbr,
                                         p_comm->DataBulkOut,
                           (CPU_INT08U *)p_buf,
                                         buf_len,
                                         timeout,
                                         p_err);
    if (*p_err != USBD_ERR_NONE) {
        if (*p_err == USBD_ERR_OS_ABORT) {                      /* Xfer is aborted when preamble status change.         */
           *p_err = USBD_ERR_RX;
        }
        USBD_PHDC_OS_RdUnlock(class_nbr);
        return (0u);
    }

    if (p_ctrl->PreambleEn == DEF_ENABLED) {
        p_ctrl->RxDataXfersCur++;                               /* Update internal ctr.                                 */

        if (p_ctrl->RxDataXfersCur  == p_ctrl->RxDataXfersTotal) {
            p_ctrl->RxDataXfersTotal = 0u;
        }
    }

    USBD_PHDC_OS_RdUnlock(class_nbr);                           /* Unlock rd pipe.                                      */

    return (xfer_len);
}



/*
**********************************************************************************************************
*                                       USBD_PHDC_PreambleWr()
*
* Description : Add PHDC metadata preamble to transmit priority queue.
*
* Argument(s) : class_nbr             PHDC instance number.
*
*               p_data_opaque         Pointer to buffer that will supply opaque data.
*
*               data_opaque_len       Length of opaque data in octets.
*
*               latency_rely          Latency / reliability to be specified in preamble.
*
*               nbr_xfers             Number of transfer this preamble will apply to.
*
*               timeout               Timeout (ms).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Data successfully added to priority queue.
*                               USBD_ERR_NULL_PTR           Invalid NULL transmit buffer.
*                               USBD_ERR_INVALID_ARG        Invalid argument(s) passed to class_nbr,
*                                                           data_opaque_len, nbr_xfers or latency_rely.
*                               USBD_ERR_TX                 Host doesn't expect a preamble.
*
*                                                           ------ RETURNED BY USBD_BulkTx() : ------
*                               USBD_ERR_NONE               Data successfully transmitted.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE  Transfer type only available if device is in
*                                                               configured state.
*                               USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                               USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                               USBD_ERR_EP_INVALID_TYPE    Invalid endpoint type.
*
*                                                           - RETURNED BY USBD_PHDC_OS_WrBulkLock() -
*                               USBD_ERR_NONE               OS signal     successfully acquired.
*                               USBD_OS_ERR_TIMEOUT         OS signal NOT successfully acquired in the time
*                                                           specified by 'timeout_ms'.
*                               USBD_OS_ERR_ABORT           OS signal aborted.
*                               USBD_OS_ERR_FAIL            OS signal not acquired because another error.
*
* Return(s)   : none.
*
* Note(s)     : none.
**********************************************************************************************************
*/

void  USBD_PHDC_PreambleWr (CPU_INT08U           class_nbr,
                            void                *p_data_opaque,
                            CPU_INT08U           data_opaque_len,
                            LATENCY_RELY_FLAGS   latency_rely,
                            CPU_INT08U           nbr_xfers,
                            CPU_INT16U           timeout,
                            USBD_ERR            *p_err)
{
    USBD_PHDC_CTRL      *p_ctrl;
    USBD_PHDC_COMM      *p_comm;
    CPU_INT16U           preamble_len;
    CPU_INT16U           data_opaque_max_len;
    LATENCY_RELY_FLAGS   latency_rely_flag;
    CPU_BOOLEAN          bulk_ep_xfer;
    CPU_INT08U           prio;
    CPU_INT08U           ep_log_nbr;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }

    if (nbr_xfers == 0u) {
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }

    if ((p_data_opaque   == (void *)0) &&
        (data_opaque_len >  0u     )) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }
#endif

    if (class_nbr >= USBD_PHDC_CtrlNbrNext) {                   /* -------------------- CHK ARG ----------------------- */
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    p_ctrl = &USBD_PHDC_CtrlTbl[class_nbr];

    if (p_ctrl->State != USBD_PHDC_STATE_CFG) {
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return;
    }

    p_comm = p_ctrl->CommPtr;

    data_opaque_max_len = USBD_EP_MaxPktSizeGet(p_ctrl->DevNbr,
                                                p_comm->DataBulkIn,
                                                p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    data_opaque_max_len -= (USBD_PHDC_METADATA_MSG_PREAMBLE_DFLT_LEN + 1u);
    data_opaque_max_len  = DEF_MIN(data_opaque_max_len, 255u);  /* Opaque data len field makes 8 bits in preamble.      */

    if (data_opaque_len > data_opaque_max_len) {
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }

    latency_rely_flag  = latency_rely;                          /* Validate EP support QoS and metadata preamble.       */
    latency_rely_flag &= p_ctrl->TxLatencyRelyBitmap;

    bulk_ep_xfer = DEF_BIT_IS_SET_ANY(latency_rely_flag, USBD_PHDC_RELY_LATENCY_FLAGS_EP_BULK_IN);

    if (bulk_ep_xfer == DEF_NO) {
        *p_err = USBD_ERR_INVALID_ARG;
         return;
    }

    prio       = CPU_CntTrailZeros08(latency_rely) - 1;
    ep_log_nbr = USBD_EP_ADDR_TO_LOG(p_ctrl->CommPtr->DataBulkIn);

    if (p_ctrl->PreambleEn == DEF_DISABLED) {
       *p_err = USBD_ERR_TX;                                    /* Host did not enable preamble. Call USBD_PHDC_Wr.     */
        return;
    }

    if (p_ctrl->TxDataXfersTotal[prio] != 0u) {
       *p_err = USBD_ERR_TX;                                    /* Preamble not expected by host, call USBD_PHDC_Wr.    */
        return;
    }

    USBD_PHDC_OS_WrBulkLock(class_nbr, prio, timeout, p_err);   /* Lock bulk wr pipe.                                   */

    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    CPU_CRITICAL_ENTER();
    DEF_BIT_SET(p_ctrl->EP_DataStatus, DEF_BIT(ep_log_nbr));
    p_ctrl->TxDataXfersTotal[prio] = nbr_xfers;
    p_ctrl->TxDataXfersCur[prio]   = 0u;
    CPU_CRITICAL_EXIT();

    p_ctrl->PreambleBufTxPtr[16] = nbr_xfers;                   /* Prepare metadata message preamble. See note 1 of...  */
    p_ctrl->PreambleBufTxPtr[18] = latency_rely;                /* ...USBD_PHDC_PreambleRd().                           */
    p_ctrl->PreambleBufTxPtr[19] = data_opaque_len;

    if (data_opaque_len > 0) {
        Mem_Copy(&p_ctrl->PreambleBufTxPtr[20],
                  p_data_opaque,
                  data_opaque_len);
    }

    preamble_len = USBD_PHDC_METADATA_MSG_PREAMBLE_DFLT_LEN + data_opaque_len;

    USBD_BulkTx(        p_ctrl->DevNbr,
                        p_comm->DataBulkIn,
                (void *)p_ctrl->PreambleBufTxPtr,
                        preamble_len,
                        timeout,
                        DEF_YES,
                        p_err);

    if (*p_err != USBD_ERR_NONE) {
        CPU_CRITICAL_ENTER();
        DEF_BIT_CLR(p_ctrl->EP_DataStatus, DEF_BIT(ep_log_nbr));
        CPU_CRITICAL_EXIT();

        p_ctrl->TxDataXfersTotal[prio] = 0u;
        p_ctrl->TxDataXfersCur[prio]   = 0u;

        USBD_PHDC_OS_WrBulkUnlock(class_nbr);
    }
}


/*
**********************************************************************************************************
*                                           USBD_PHDC_Wr()
*
* Description : Write PHDC data.
*
* Argument(s) : class_nbr               PHDC instance number.
*
*               p_buf                   Pointer to buffer that will supply data.
*
*               buf_len                 Data length in octets.
*
*               latency_rely            Latency / reliability of this transfer.
*
*               timeout                 Timeout (ms).
*
*               p_err                   Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Data successfully added to priority queue.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to class_nbr or latency_rely.
*                               USBD_ERR_NULL_PTR               Invalid null buffer.
*                               USBD_ERR_TX                     USBD_PHDC_PreambleWr shall be called first.
*                               USBD_ERR_INVALID_CLASS_STATE    Invalid PHDC state.
*                               USBD_ERR_ALLOC                  Data queue is full.
*
*                                                               ------ RETURNED BY USBD_BulkTx() : ------
*                               USBD_ERR_NONE                   Data successfully transmitted.
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE      Transfer type only available if device is in
*                                                               configured state.
*                               USBD_ERR_EP_INVALID_ADDR        Invalid endpoint address.
*                               USBD_ERR_EP_INVALID_STATE       Invalid endpoint state.
*                               USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*
*                                                               ------ RETURNED BY USBD_IntrTx() : ------
*                               USBD_ERR_NONE                   Data successfully transmitted.
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE      Transfer type only available if device is in
*                                                               configured state.
*                               USBD_ERR_EP_INVALID_ADDR        Invalid endpoint address.
*                               USBD_ERR_EP_INVALID_STATE       Invalid endpoint state.
*                               USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*
*                                                               - RETURNED BY USBD_PHDC_OS_WrBulkLock() -
*                               USBD_ERR_NONE                   OS signal     successfully acquired.
*                               USBD_OS_ERR_TIMEOUT             OS signal NOT successfully acquired in the time
*                                                               specified by 'timeout_ms'.
*                               USBD_OS_ERR_ABORT               OS signal aborted.
*                               USBD_OS_ERR_FAIL                OS signal not acquired because another error.
*
*                                                               - RETURNED BY USBD_PHDC_OS_WrIntrLock() -
*                               USBD_ERR_NONE                   OS signal     successfully acquired.
*                               USBD_OS_ERR_TIMEOUT             OS signal NOT successfully acquired in the time
*                                                               specified by 'timeout_ms'.
*                               USBD_OS_ERR_ABORT               OS signal aborted.
*                               USBD_OS_ERR_FAIL                OS signal not acquired because another error.
*
* Return(s)   : none.
*
* Note(s)     : none.
**********************************************************************************************************
*/

void  USBD_PHDC_Wr (CPU_INT08U           class_nbr,
                    void                *p_buf,
                    CPU_INT16U           buf_len,
                    LATENCY_RELY_FLAGS   latency_rely,
                    CPU_INT16U           timeout,
                    USBD_ERR            *p_err)
{
    USBD_PHDC_CTRL  *p_ctrl;
    USBD_PHDC_COMM  *p_comm;
    CPU_INT08U       latency_rely_test;
    CPU_BOOLEAN      qos_supported;
    CPU_INT08U       prio;
    CPU_INT08U       ep_log_nbr;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }

    if (p_buf == (void *)0) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }

    if (latency_rely == 0u) {
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }
#endif

    if (class_nbr >= USBD_PHDC_CtrlNbrNext) {                   /* -------------------- CHK ARG ----------------------- */
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    p_ctrl = &USBD_PHDC_CtrlTbl[class_nbr];
    if (p_ctrl->State != USBD_PHDC_STATE_CFG) {
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return;
    }
    prio = CPU_CntTrailZeros08(latency_rely);

    if (prio >= USBD_PHDC_XFER_PRIO_MAX) {
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }

    latency_rely_test = latency_rely;

    DEF_BIT_CLR(latency_rely_test, DEF_BIT(prio));

    if (latency_rely_test != DEF_BIT_NONE) {
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }

    qos_supported = DEF_BIT_IS_SET(p_ctrl->TxLatencyRelyBitmap, latency_rely);

    if (qos_supported != DEF_YES) {                             /* Validate EP support QoS.                             */
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }

    p_comm = p_ctrl->CommPtr;

    if (latency_rely == USBD_PHDC_LATENCY_LOW_RELY_GOOD) {      /* Low latency xfer use intr EP.                        */
        ep_log_nbr = USBD_EP_ADDR_TO_LOG(p_comm->DataIntrIn);

        CPU_CRITICAL_ENTER();
        DEF_BIT_SET(p_ctrl->EP_DataStatus, DEF_BIT(ep_log_nbr));
        CPU_CRITICAL_EXIT();

        USBD_PHDC_OS_WrIntrLock(class_nbr, timeout, p_err);     /* Lock intr EP.                                        */

        if (*p_err != USBD_ERR_NONE) {
            CPU_CRITICAL_ENTER();
            DEF_BIT_CLR(p_ctrl->EP_DataStatus, DEF_BIT(ep_log_nbr));
            CPU_CRITICAL_EXIT();
            return;
        }

        USBD_IntrTx(p_ctrl->DevNbr,
                    p_comm->DataIntrIn,
                    p_buf,
                    buf_len,
                    timeout,
                    DEF_YES,
                    p_err);

        USBD_PHDC_OS_WrIntrUnlock(class_nbr);                   /* Unlock intr EP.                                      */

        CPU_CRITICAL_ENTER();
        DEF_BIT_CLR(p_ctrl->EP_DataStatus, DEF_BIT(ep_log_nbr));
        CPU_CRITICAL_EXIT();
    } else {                                                    /* Very high, high and medium latency xfer use bulk EP. */
        ep_log_nbr = USBD_EP_ADDR_TO_LOG(p_comm->DataBulkIn);
        prio--;                                                 /* There are 5 QoS level on bulk IN EP.                 */

        CPU_CRITICAL_ENTER();
        if ((p_ctrl->PreambleEn             == DEF_ENABLED) &&
            (p_ctrl->TxDataXfersTotal[prio] == 0u         )) {
            CPU_CRITICAL_EXIT();
           *p_err = USBD_ERR_TX;                                /* App need to call USBD_PHDC_PreambleWr first.         */
            return;
        }

        DEF_BIT_SET(p_ctrl->EP_DataStatus, DEF_BIT(ep_log_nbr));
        CPU_CRITICAL_EXIT();

        if (p_ctrl->PreambleEn == DEF_DISABLED) {
            USBD_PHDC_OS_WrBulkLock(class_nbr, prio, timeout, p_err);

            if (*p_err != USBD_ERR_NONE) {
                if (*p_err != USBD_ERR_OS_TIMEOUT) {
                    CPU_CRITICAL_ENTER();
                    DEF_BIT_CLR(p_ctrl->EP_DataStatus, DEF_BIT(ep_log_nbr));
                    CPU_CRITICAL_EXIT();
                }
                return;
            }
        }

        USBD_BulkTx(p_ctrl->DevNbr,
                    p_comm->DataBulkIn,
                    p_buf,
                    buf_len,
                    timeout,
                    DEF_YES,
                    p_err);

        if (p_ctrl->PreambleEn == DEF_ENABLED) {
            if (*p_err == USBD_ERR_NONE) {
                p_ctrl->TxDataXfersCur[prio]++;
                                                                /* Update xfers ctr.                                    */
                if (p_ctrl->TxDataXfersCur[prio] == p_ctrl->TxDataXfersTotal[prio]) {
                    p_ctrl->TxDataXfersCur[prio]   = 0u;
                    p_ctrl->TxDataXfersTotal[prio] = 0u;
                    USBD_PHDC_OS_WrBulkUnlock(class_nbr);       /* Unlock bulk EP if all xfer cmpleted under preamble.  */

                    CPU_CRITICAL_ENTER();
                    DEF_BIT_CLR(p_ctrl->EP_DataStatus, DEF_BIT(ep_log_nbr));
                    CPU_CRITICAL_EXIT();
                }
            }
        } else {
            CPU_CRITICAL_ENTER();
            DEF_BIT_CLR(p_ctrl->EP_DataStatus, DEF_BIT(ep_log_nbr));
            CPU_CRITICAL_EXIT();

            USBD_PHDC_OS_WrBulkUnlock(class_nbr);
        }
    }
}


/*
**********************************************************************************************************
*                                          USBD_PHDC_Reset()
*
* Description : Reset PHDC instance.
*
* Argument(s) : class_nbr   PHDC instance number.
*
* Return(s)   : none.
*
* Note(s)     : none.
**********************************************************************************************************
*/

void  USBD_PHDC_Reset (CPU_INT08U  class_nbr)
{
    CPU_INT16U       cnt;
    USBD_PHDC_CTRL  *p_ctrl;
    USBD_ERR         err;
    CPU_SR_ALLOC();


    if (class_nbr >= USBD_PHDC_CtrlNbrNext) {
         return;
    }

    p_ctrl = &USBD_PHDC_CtrlTbl[class_nbr];

    USBD_EP_Abort(p_ctrl->DevNbr,
                  p_ctrl->CommPtr->DataBulkIn,
                 &err);
    USBD_EP_Abort(p_ctrl->DevNbr,
                  p_ctrl->CommPtr->DataIntrIn,
                 &err);
    USBD_EP_Abort(p_ctrl->DevNbr,
                  p_ctrl->CommPtr->DataBulkOut,
                 &err);

    CPU_CRITICAL_ENTER();

    if (p_ctrl->PreambleEn != DEF_DISABLED) {
        p_ctrl->PreambleEn = DEF_DISABLED;

        if (p_ctrl->PreambleEnNotify != (USBD_PHDC_PREAMBLE_EN_NOTIFY)0) {
            p_ctrl->PreambleEnNotify(class_nbr, DEF_DISABLED);
        }
    }

    for (cnt = 0; cnt < (USBD_PHDC_XFER_PRIO_MAX - 1u); cnt++) {
        p_ctrl->TxDataXfersTotal[cnt] = 0u;
        p_ctrl->TxDataXfersCur[cnt]   = 0u;
    }
    p_ctrl->EP_DataStatus    = DEF_BIT_NONE;
    p_ctrl->RxDataXfersTotal = 0u;
    p_ctrl->RxDataXfersCur   = 0u;

    CPU_CRITICAL_EXIT();

    USBD_PHDC_OS_Reset(class_nbr);
}


/*
**********************************************************************************************************
**********************************************************************************************************
*                                           LOCAL FUNCTIONS
**********************************************************************************************************
**********************************************************************************************************
*/



/*
**********************************************************************************************************
*                                     INTERFACE DRIVER FUNCTIONS
**********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          USBD_PHDC_Conn()
*
* Description : Notify class that configuration is active.
*
* Argument(s) : cfg_nbr         Configuration ix to add the interface.
*
*               p_if_class_arg  Pointer to class argument specific to interface..
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_PHDC_Conn (CPU_INT08U   dev_nbr,
                              CPU_INT08U   cfg_nbr,
                              void        *p_if_class_arg)
{
    USBD_PHDC_COMM  *p_comm;
    CPU_SR_ALLOC();


    (void)cfg_nbr;
    (void)dev_nbr;

    p_comm = (USBD_PHDC_COMM *)p_if_class_arg;
    CPU_CRITICAL_ENTER();
    p_comm->CtrlPtr->CommPtr = p_comm;
    p_comm->CtrlPtr->State   = USBD_PHDC_STATE_CFG;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                         USBD_PHDC_Disconn()
*
* Description : Notify class that configuration is not active.
*
* Argument(s) : cfg_nbr         Configuration ix to add the interface.
*
*               p_if_class_arg  Pointer to class argument specific to interface..
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_PHDC_Disconn (CPU_INT08U   dev_nbr,
                                 CPU_INT08U   cfg_nbr,
                                 void        *p_if_class_arg)
{
    USBD_PHDC_COMM  *p_comm;
    CPU_SR_ALLOC();


    (void)cfg_nbr;
    (void)dev_nbr;

    p_comm = (USBD_PHDC_COMM *)p_if_class_arg;

    USBD_PHDC_Reset(p_comm->CtrlPtr->Nbr);

    CPU_CRITICAL_ENTER();
    p_comm->CtrlPtr->CommPtr = (USBD_PHDC_COMM *)0;
    p_comm->CtrlPtr->State   =  USBD_PHDC_STATE_INIT;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                      USBD_PHDC_SettingUpdate()
*
* Description : Notify class that interface alternative setting has been updated.
*
* Argument(s) : cfg_nbr             Configuration ix to add the interface.
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

static  void  USBD_PHDC_SettingUpdate (CPU_INT08U   dev_nbr,
                                       CPU_INT08U   cfg_nbr,
                                       CPU_INT08U   if_nbr,
                                       CPU_INT08U   if_alt_nbr,
                                       void        *p_if_class_arg,
                                       void        *p_if_alt_class_arg)
{
    USBD_PHDC_COMM  *p_comm;
    USBD_PHDC_CTRL  *p_ctrl;
    CPU_SR_ALLOC();


    (void)cfg_nbr;
    (void)if_nbr;
    (void)if_alt_nbr;
    (void)dev_nbr;
    (void)p_if_alt_class_arg;

    p_comm = (USBD_PHDC_COMM *)p_if_class_arg;
    p_ctrl = p_comm->CtrlPtr;
    CPU_CRITICAL_ENTER();
    p_ctrl->CommPtr = p_comm;
    CPU_CRITICAL_EXIT();
}


/*
**********************************************************************************************************
*                                         USBD_PHDC_IF_Desc()
*
* Description : Write extra interface descriptor.
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
**********************************************************************************************************
*/

static  void  USBD_PHDC_IF_Desc (CPU_INT08U   dev_nbr,
                                 CPU_INT08U   cfg_nbr,
                                 CPU_INT08U   if_nbr,
                                 CPU_INT08U   if_alt_nbr,
                                 void        *p_if_class_arg,
                                 void        *p_if_alt_class_arg)

{
    CPU_INT08U       cnt;
    USBD_PHDC_CTRL  *p_ctrl;
    USBD_PHDC_COMM  *p_comm;


    (void)cfg_nbr;
    (void)if_nbr;
    (void)if_alt_nbr;
    (void)p_if_alt_class_arg;

    p_comm = (USBD_PHDC_COMM *)p_if_class_arg;
    p_ctrl = p_comm->CtrlPtr;

                                                                /* Class fnct desc                                      */
    USBD_DescWr08(dev_nbr, USBD_PHDC_CLASS_FNCT_DESC_LEN);
    USBD_DescWr08(dev_nbr, USBD_PHDC_DESC_TYPE_CLASS_FNCT);

    if (p_ctrl->IF_Params.DataFmt11073 == DEF_YES) {
        USBD_DescWr08(dev_nbr, USBD_PHDC_DATA_FMT_11073_20601);
    } else {
        USBD_DescWr08(dev_nbr, USBD_PHDC_DATA_FMT_VENDOR);
    }

    if (p_ctrl->IF_Params.PreambleCapable == DEF_YES) {
        USBD_DescWr08(dev_nbr, USBD_PHDC_PREAMBLE_CAPABLE);
    } else {
        USBD_DescWr08(dev_nbr, USBD_PHDC_PREAMBLE_NOT_CAPABLE);
    }
                                                                /* Fnct ext desc                                        */
    USBD_DescWr08(dev_nbr,
                  USBD_PHDC_FUNCT_EXTN_DESC_DFLT_LEN + (p_ctrl->IF_Params.NbrDevSpecialization * 2));
    USBD_DescWr08(dev_nbr, USBD_PHDC_DESC_TYPE_FUNCT_EXTN_11073);
    USBD_DescWr08(dev_nbr, 0x00);                               /* Reserved                                             */
    USBD_DescWr08(dev_nbr, p_ctrl->IF_Params.NbrDevSpecialization);

    for (cnt = 0; cnt < p_ctrl->IF_Params.NbrDevSpecialization; cnt++) {
        USBD_DescWr16(dev_nbr, p_ctrl->IF_Params.DevSpecialization[cnt]);
    }
}


/*
**********************************************************************************************************
*                                      USBD_PHDC_IF_DescSizeGet()
*
* Description : Return size of extra interface descriptor.
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
* Return(s)   : Size of extra interface descriptor in octets.
*
* Note(s)     : none.
**********************************************************************************************************
*/

static  CPU_INT16U  USBD_PHDC_IF_DescSizeGet (CPU_INT08U   dev_nbr,
                                              CPU_INT08U   cfg_nbr,
                                              CPU_INT08U   if_nbr,
                                              CPU_INT08U   if_alt_nbr,
                                              void        *p_if_class_arg,
                                              void        *p_if_alt_class_arg)
{
    CPU_INT16U       desc_len;
    USBD_PHDC_CTRL  *p_ctrl;
    USBD_PHDC_COMM  *p_comm;


    (void)dev_nbr;
    (void)cfg_nbr;
    (void)if_nbr;
    (void)if_alt_nbr;
    (void)p_if_alt_class_arg;

    p_comm = (USBD_PHDC_COMM *)p_if_class_arg;
    p_ctrl = p_comm->CtrlPtr;

    desc_len =  USBD_PHDC_CLASS_FNCT_DESC_LEN      +
                USBD_PHDC_FUNCT_EXTN_DESC_DFLT_LEN +
               (p_ctrl->IF_Params.NbrDevSpecialization * 2);

    return (desc_len);
}


/*
**********************************************************************************************************
*                                         USBD_PHDC_EP_Desc()
*
* Description : Write extra endpoint descriptor.
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
* Note(s)     : none.
**********************************************************************************************************
*/

static  void  USBD_PHDC_EP_Desc (CPU_INT08U   dev_nbr,
                                 CPU_INT08U   cfg_nbr,
                                 CPU_INT08U   if_nbr,
                                 CPU_INT08U   if_alt_nbr,
                                 CPU_INT08U   ep_addr,
                                 void        *p_if_class_arg,
                                 void        *p_if_alt_class_arg)

{
    USBD_PHDC_CTRL       *p_ctrl;
    USBD_PHDC_COMM       *p_comm;
    USBD_PHDC_EP_PARAMS  *ep_params;
    LATENCY_RELY_FLAGS    latency_rely_bitmap;


    (void)cfg_nbr;
    (void)if_nbr;
    (void)if_alt_nbr;
    (void)p_if_alt_class_arg;

    p_comm = (USBD_PHDC_COMM *)p_if_class_arg;
    p_ctrl = p_comm->CtrlPtr;

    if (ep_addr == p_comm->DataBulkIn) {
        ep_params           = &p_ctrl->EP_Params[USBD_PHDC_EP_BULK_IN];
        latency_rely_bitmap =  p_ctrl->TxLatencyRelyBitmap;
        DEF_BIT_CLR(latency_rely_bitmap, USBD_PHDC_RELY_LATENCY_FLAGS_EP_INTR_IN);
    } else if (ep_addr == p_comm->DataBulkOut) {
        ep_params           = &p_ctrl->EP_Params[USBD_PHDC_EP_BULK_OUT];
        latency_rely_bitmap =  p_ctrl->RxLatencyRelyBitmap;
    } else if (ep_addr == p_comm->DataIntrIn) {
        ep_params           = &p_ctrl->EP_Params[USBD_PHDC_EP_INTR_IN];
        latency_rely_bitmap =  p_ctrl->TxLatencyRelyBitmap;
        DEF_BIT_CLR(latency_rely_bitmap, USBD_PHDC_RELY_LATENCY_FLAGS_EP_BULK_IN);
    } else {
        return;
    }

    USBD_DescWr08(dev_nbr, USBD_PHDC_QOS_DESC_LEN);             /* QoS desc                                             */
    USBD_DescWr08(dev_nbr, USBD_PHDC_DESC_TYPE_QOS);
    USBD_DescWr08(dev_nbr, USBD_PHDC_QOS_ENCOD_VER);
    USBD_DescWr08(dev_nbr, latency_rely_bitmap);

    if (ep_params->DataOpaqueLen > 0) {
        USBD_DescWr08(dev_nbr,                                  /* Metadata descriptor                                  */
                      USBD_PHDC_METADATA_DESC_LEN + ep_params->DataOpaqueLen);
        USBD_DescWr08(dev_nbr, USBD_PHDC_DESC_TYPE_METADATA);

        USBD_DescWr(dev_nbr,
                    ep_params->DataOpaquePtr,
                    ep_params->DataOpaqueLen);
    }
}


/*
**********************************************************************************************************
*                                      USBD_PHDC_EP_DescSizeGet()
*
* Description : Return size of extra endpoint descriptor.
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
* Return(s)   : Size of extra endpoint descriptor in octets.
*
* Note(s)     : none.
**********************************************************************************************************
*/

static  CPU_INT16U  USBD_PHDC_EP_DescSizeGet (CPU_INT08U   dev_nbr,
                                              CPU_INT08U   cfg_nbr,
                                              CPU_INT08U   if_nbr,
                                              CPU_INT08U   if_alt_nbr,
                                              CPU_INT08U   ep_addr,
                                              void        *p_if_class_arg,
                                              void        *p_if_alt_class_arg)

{
    CPU_INT16U            desc_len;
    USBD_PHDC_CTRL       *p_ctrl;
    USBD_PHDC_COMM       *p_comm;
    USBD_PHDC_EP_PARAMS  *ep_params;


    (void)dev_nbr;
    (void)cfg_nbr;
    (void)if_nbr;
    (void)if_alt_nbr;
    (void)p_if_alt_class_arg;

    p_comm = (USBD_PHDC_COMM *)p_if_class_arg;
    p_ctrl = p_comm->CtrlPtr;

    if (ep_addr == p_comm->DataBulkIn) {
        ep_params = &p_ctrl->EP_Params[USBD_PHDC_EP_BULK_IN];
    } else if (ep_addr == p_comm->DataBulkOut) {
        ep_params = &p_ctrl->EP_Params[USBD_PHDC_EP_BULK_OUT];
    } else if (ep_addr == p_comm->DataIntrIn) {
        ep_params = &p_ctrl->EP_Params[USBD_PHDC_EP_INTR_IN];
    } else {
        return (0);
    }

    desc_len = USBD_PHDC_QOS_DESC_LEN;
    if (ep_params->DataOpaqueLen > 0) {
        desc_len += USBD_PHDC_METADATA_DESC_LEN +
                    ep_params->DataOpaqueLen;
    }

    return (desc_len);
}


/*
*********************************************************************************************************
*                                        USBD_PHDC_ClassReq()
*
* Description : Class request handler.
*
* Argument(s) : p_setup_req     Pointer to setup request structure.
*
*               p_if_class_arg  Pointer to class argument specific to interface.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request should be sent.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_PHDC_ClassReq (       CPU_INT08U       dev_nbr,
                                         const  USBD_SETUP_REQ  *p_setup_req,
                                                void            *p_if_class_arg)
{
    CPU_BOOLEAN      rtn_val;
    USBD_ERR         err;
    USBD_PHDC_CTRL  *p_ctrl;
    USBD_PHDC_COMM  *p_comm;


    p_comm  = (USBD_PHDC_COMM *)p_if_class_arg;
    p_ctrl  = p_comm->CtrlPtr;
    rtn_val = DEF_FAIL;

    switch (p_setup_req->bRequest) {
        case USBD_REQ_GET_STATUS:                               /* -------------------- GET STATUS -------------------  */
             switch (p_ctrl->State) {
                 case USBD_PHDC_STATE_CFG:
                      if (p_setup_req->wValue != 0) {
                          break;
                      }
                      p_ctrl->CtrlStatusBufPtr[0] = (CPU_INT08U)( p_ctrl->EP_DataStatus        & DEF_INT_08_MASK);
                      p_ctrl->CtrlStatusBufPtr[1] = (CPU_INT08U)((p_ctrl->EP_DataStatus >> 8u) & DEF_INT_08_MASK);
                      USBD_CtrlTx(         dev_nbr,
                                  (void *)&p_ctrl->CtrlStatusBufPtr[0],
                                           2u,
                                           USBD_PHDC_CTRL_REQ_TIMEOUT_mS,
                                           DEF_FALSE,
                                          &err);
                      if (err == USBD_ERR_NONE) {
                          rtn_val = DEF_OK;
                      }
                     break;

                 case USBD_PHDC_STATE_NONE:
                 case USBD_PHDC_STATE_INIT:
                 default :
                     break;
             }
             break;

        case USBD_REQ_CLEAR_FEATURE:                            /* ------------------ CLEAR FEATURE ------------------- */
             switch (p_ctrl->State) {
                 case USBD_PHDC_STATE_CFG:
                     if (((CPU_INT08U)  p_setup_req->wValue                 == USBD_PHDC_CLASS_SPEC_FEATURE_PHDC_QOS) &&
                         ((CPU_INT08U)((p_setup_req->wValue & 0xFF00) >> 8) == 0x00)) {

                                                                /* Make sure metadata msg preambles are supported       */
                          if (p_ctrl->IF_Params.PreambleCapable == DEF_YES) {
                              if (p_ctrl->PreambleEn != DEF_DISABLED) {
                                  p_ctrl->PreambleEn = DEF_DISABLED;
                                  if (p_ctrl->PreambleEnNotify != (USBD_PHDC_PREAMBLE_EN_NOTIFY)0) {
                                      p_ctrl->PreambleEnNotify(p_ctrl->Nbr, DEF_DISABLED);
                                  }

                                  USBD_EP_Abort(dev_nbr,
                                                p_ctrl->CommPtr->DataBulkOut,
                                               &err);
                              }
                              rtn_val = DEF_OK;
                          }
                      }
                      break;

                 case USBD_PHDC_STATE_NONE:
                 case USBD_PHDC_STATE_INIT:
                 default :
                      break;
             }
             break;

        case USBD_REQ_SET_FEATURE:                              /* ------------------- SET FEATURE -------------------- */
             switch (p_ctrl->State) {
                 case USBD_PHDC_STATE_CFG:
                     if (((CPU_INT08U)  p_setup_req->wValue                 == USBD_PHDC_CLASS_SPEC_FEATURE_PHDC_QOS) &&
                         ((CPU_INT08U)((p_setup_req->wValue & 0xFF00) >> 8) == USBD_PHDC_QOS_ENCOD_VER)) {

                                                                /* Make sure metadata msg preambles are supported       */
                          if (p_ctrl->IF_Params.PreambleCapable == DEF_YES) {
                              if (p_ctrl->PreambleEn != DEF_ENABLED) {
                                  p_ctrl->PreambleEn = DEF_ENABLED;
                                  if (p_ctrl->PreambleEnNotify != (USBD_PHDC_PREAMBLE_EN_NOTIFY)0) {
                                      p_ctrl->PreambleEnNotify(p_ctrl->Nbr, DEF_ENABLED);
                                  }

                                  USBD_EP_Abort(dev_nbr,
                                                p_ctrl->CommPtr->DataBulkOut,
                                               &err);
                              }
                              rtn_val = DEF_OK;
                          }
                      }
                      break;

                 case USBD_PHDC_STATE_NONE:
                 case USBD_PHDC_STATE_INIT:
                 default :
                      break;
                 }
                 break;

        default:                                                /* Request not supported                                */
             break;
    }

    return (rtn_val);
}


/*
**********************************************************************************************************
*                                             MODULE END
**********************************************************************************************************
*/

