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
*
* Filename : usbd_cdc.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)       : (1) This implementation is compliant with the CDC specification revision 1.2
*                     errata 1. November 3, 2010.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#include  "usbd_cdc.h"
#include  <lib_math.h>


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

/*
*********************************************************************************************************
*                        MAXIMUM NUMBER OF COMMUNICATION/DATA IF EP STRUCTURES
*********************************************************************************************************
*/

                                                                /* Max nbr of comm struct.                              */
#define  USBD_CDC_COMM_NBR_MAX                  (USBD_CDC_CFG_MAX_NBR_DEV * \
                                                 USBD_CDC_CFG_MAX_NBR_CFG)

                                                                /* Max nbr of data IF EP struct.                        */
#define  USBD_CDC_DATA_IF_EP_NBR_MAX            (USBD_CDC_CFG_MAX_NBR_DEV     * \
                                                 USBD_CDC_CFG_MAX_NBR_DATA_IF * \
                                                 USBD_CDC_CFG_MAX_NBR_CFG)


/*
*********************************************************************************************************
*                                   CDC FUNCTIONAL DESCRIPTOR SIZE
*********************************************************************************************************
*/

#define  USBD_CDC_DESC_SIZE_HEADER                       5u     /* Header functional desc size.                         */
#define  USBD_CDC_DESC_SIZE_UNION_MIN                    4u     /* Min size of union functional desc.                   */


/*
*********************************************************************************************************
*                                      CDC TOTAL NUMBER DEFINES
*********************************************************************************************************
*/

#define  USBD_CDC_NBR_TOTAL                     (DEF_INT_08U_MAX_VAL - 1u)
#define  USBD_CDC_DATA_IF_NBR_TOTAL             (DEF_INT_08U_MAX_VAL - 1u)


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
*                                         CDC STATE DATA TYPE
*********************************************************************************************************
*/

typedef  enum  usbd_cdc_state {                                 /* CDC device state.                                    */
    USBD_CDC_STATE_NONE = 0,
    USBD_CDC_STATE_INIT,
    USBD_CDC_STATE_CFG
} USBD_CDC_STATE;


/*
*********************************************************************************************************
*                                        FORWARD DECLARATIONS
*********************************************************************************************************
*/

typedef  struct  usbd_cdc_ctrl     USBD_CDC_CTRL;
typedef  struct  usbd_cdc_data_if  USBD_CDC_DATA_IF;


/*
*********************************************************************************************************
*                                     CDC DATA IF CLASS DATA TYPE
*
* Note(s) : (1) USB CDC specification specifies that the type of endpoints belonging to a Data IF are
*               restricted to being either isochronous or bulk, and are expected to exist in pairs of
*               the same type (one IN and one OUT).
*********************************************************************************************************
*/

typedef  struct  usbd_cdc_data_if_ep {
    CPU_INT08U         DataIn;
    CPU_INT08U         DataOut;
} USBD_CDC_DATA_IF_EP;


struct  usbd_cdc_data_if {
    CPU_INT08U         IF_Nbr;                                  /* Data IF nbr.                                         */
    CPU_INT08U         Protocol;                                /* Data IF protocol.                                    */
    CPU_BOOLEAN        IsocEn;                                  /* EP isochronous enable.                               */
    USBD_CDC_DATA_IF  *NextPtr;                                 /* Next data IF.                                        */
};


/*
*********************************************************************************************************
*                                   CDC COMMUNICATION IF DATA TYPE
*
* Note(s) : (1) A CDC device consists in one communication IF, and multiple data IFs (optional).
*
*                    +-----IFs----+-------EPs-------+
*                    |  COMM_IF   |  CTRL           | <--------  Mgmt   Requests.
*                    |            |  INTR (Optional)| ---------> Events Notifications.
*                    +------------+-----------------+
*                    |  DATA_IF   |  BULK/ISOC IN   | ---------> Data Tx (0)
*                    |            |  BULK/ISOC OUT  | <--------- Data Rx (0)
*                    +------------+-----------------+
*                    |  DATA_IF   |  BULK/ISOC IN   | ---------> Data Tx (1)
*                    |            |  BULK/ISOC OUT  | <--------- Data Rx (1)
*                    +------------+-----------------+
*                    |  DATA_IF   |  BULK/ISOC IN   | ---------> Data Tx (2)
*                    |            |  BULK/ISOC OUT  | <--------- Data Rx (2)
*                    +------------+-----------------+
*                                 .
*                                 .
*                                 .
*                    +------------+-----------------+
*                    |  DATA_IF   |  BULK/ISOC IN   | ---------> Data Tx (n - 1)
*                    |            |  BULK/ISOC OUT  | <--------- Data Rx (n - 1)
*                    +------------+-----------------+
*
*               (a)  The communication IF may have an optional notification element. Notifications are
*                    sent using a interrupt endpoint.
*
*               (b)  The communication IF structure contains a link list of Data IFs.
*********************************************************************************************************
*/
                                                                /* ------ CDC DATA CLASS INTERFACE COMMUNICATION ------ */
typedef  struct  usbd_cdc_comm {
    CPU_INT08U      DevNbr;                                     /* Dev nbr.                                             */
    CPU_INT08U      CCI_IF_Nbr;                                 /* Comm Class IF nbr.                                   */
    USBD_CDC_CTRL  *CtrlPtr;                                    /* Ptr to ctrl info.                                    */
    CPU_INT08U      NotifyIn;                                   /* Notification EP (see note #1a).                      */
    CPU_INT16U      DataIF_EP_Ix;                               /* Start ix of data IFs EP information.                 */
    CPU_BOOLEAN     NotifyInActiveXfer;
} USBD_CDC_COMM;


struct  usbd_cdc_ctrl {                                         /* ----------- CDC CLASS CONTROL INFORMATION ---------- */
    USBD_CDC_STATE          State;                              /* CDC state.                                           */
    CPU_BOOLEAN             NotifyEn;                           /* CDC mgmt element notifications enable.               */
    CPU_INT16U              NotifyInterval;                     /* CDC mgmt element notifications interval.             */
    CPU_INT08U              DataIF_Nbr;                         /* Number of data IFs.                                  */
    USBD_CDC_DATA_IF       *DataIF_HeadPtr;                     /* Data IFs list head ptr. (see note #1b)               */
    USBD_CDC_DATA_IF       *DataIF_TailPtr;                     /* Data IFs list tail ptr.                              */
    CPU_INT08U              SubClassCode;                       /* CDC subclass code.                                   */
    CPU_INT08U              SubClassProtocol;                   /* CDC subclass protocol.                               */
    USBD_CDC_SUBCLASS_DRV  *SubClassDrvPtr;                     /* CDC subclass drv.                                    */
    void                   *SubClassArg;                        /* CDC subclass drv argument.                           */
    USBD_CDC_COMM          *CommPtr;                            /* CDC comm information ptr.                            */
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

static  USBD_CDC_CTRL        USBD_CDC_CtrlTbl[USBD_CDC_CFG_MAX_NBR_DEV];
static  CPU_INT08U           USBD_CDC_CtrlNbrNext;

static  USBD_CDC_COMM        USBD_CDC_CommTbl[USBD_CDC_COMM_NBR_MAX];
static  CPU_INT16U           USBD_CDC_CommNbrNext;

static  USBD_CDC_DATA_IF     USBD_CDC_DataIF_Tbl[USBD_CDC_CFG_MAX_NBR_DATA_IF];
static  CPU_INT08U           USBD_CDC_DataIF_NbrNext;

static  USBD_CDC_DATA_IF_EP  USBD_CDC_DataIF_EP_Tbl[USBD_CDC_DATA_IF_EP_NBR_MAX];
static  CPU_INT16U           USBD_CDC_DataIF_EP_NbrNext;


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void         USBD_CDC_Conn              (       CPU_INT08U       dev_nbr,
                                                        CPU_INT08U       cfg_nbr,
                                                        void            *p_if_class_arg);

static  void         USBD_CDC_Disconn           (       CPU_INT08U       dev_nbr,
                                                        CPU_INT08U       cfg_nbr,
                                                        void            *p_if_class_arg);

static  void         USBD_CDC_CommIF_Desc       (       CPU_INT08U       dev_nbr,
                                                        CPU_INT08U       cfg_nbr,
                                                        CPU_INT08U       if_nbr,
                                                        CPU_INT08U       if_alt_nbr,
                                                        void            *p_if_class_arg,
                                                        void            *p_if_alt_class_arg);

static  CPU_INT16U   USBD_CDC_CommIF_DescSizeGet(       CPU_INT08U       dev_nbr,
                                                        CPU_INT08U       cfg_nbr,
                                                        CPU_INT08U       if_nbr,
                                                        CPU_INT08U       if_alt_nbr,
                                                        void            *p_if_class_arg,
                                                        void            *p_if_alt_class_arg);

static  CPU_BOOLEAN  USBD_CDC_ClassReq          (       CPU_INT08U       dev_nbr,
                                                 const  USBD_SETUP_REQ  *p_setup_req,
                                                        void            *p_if_class_arg);

static  void         USBD_CDC_NotifyCmpl        (       CPU_INT08U       dev_nbr,
                                                        CPU_INT08U       ep_addr,
                                                        void            *p_buf,
                                                        CPU_INT32U       buf_len,
                                                        CPU_INT32U       xfer_len,
                                                        void            *p_arg,
                                                        USBD_ERR         err);


/*
*********************************************************************************************************
*                                          CDC CLASS DRIVERS
*********************************************************************************************************
*/
                                                                /* Comm IF class drv.                                   */
static  USBD_CLASS_DRV  USBD_CDC_CommDrv = {
    USBD_CDC_Conn,
    USBD_CDC_Disconn,
    DEF_NULL,
    DEF_NULL,
    USBD_CDC_CommIF_Desc,
    USBD_CDC_CommIF_DescSizeGet,
    DEF_NULL,
    DEF_NULL,
    DEF_NULL,
    USBD_CDC_ClassReq,
    DEF_NULL,

#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
    DEF_NULL,
    DEF_NULL
#endif
};

                                                                /* Data IF class drv.                                   */
static  USBD_CLASS_DRV  USBD_CDC_DataDrv = {
    DEF_NULL,
    DEF_NULL,
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
    DEF_NULL
#endif
};


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/

#if   (USBD_CFG_MAX_NBR_IF_GRP < 1u)
#error  "USBD_CFG_MAX_NBR_IF_GRP illegally #define'd in 'usbd_cfg.h' [MUST be >= 1 for CDC]"
#endif


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
*                                           USBD_CDC_Init()
*
* Description : Initialize CDC class.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE   CDC class initialized successfully.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_CDC_Init (USBD_ERR  *p_err)
{
    CPU_INT08U            ix;
    USBD_CDC_CTRL        *p_ctrl;
    USBD_CDC_COMM        *p_comm;
    USBD_CDC_DATA_IF     *p_data_if;
    USBD_CDC_DATA_IF_EP  *p_data_ep;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

    for (ix = 0u; ix < USBD_CDC_CFG_MAX_NBR_DEV; ix++) {        /* Init CDC ctrl tbl.                                   */
        p_ctrl                   = &USBD_CDC_CtrlTbl[ix];
        p_ctrl->State            =  USBD_CDC_STATE_NONE;
        p_ctrl->NotifyEn         =  DEF_DISABLED;
        p_ctrl->NotifyInterval   =  0u;
        p_ctrl->DataIF_Nbr       =  0u;
        p_ctrl->DataIF_HeadPtr   = (USBD_CDC_DATA_IF *)0;
        p_ctrl->DataIF_TailPtr   = (USBD_CDC_DATA_IF *)0;
        p_ctrl->SubClassCode     =  USBD_CDC_SUBCLASS_RSVD;
        p_ctrl->SubClassProtocol =  USBD_CDC_COMM_PROTOCOL_NONE;
        p_ctrl->SubClassDrvPtr   = (USBD_CDC_SUBCLASS_DRV *)0;
        p_ctrl->SubClassArg      = (void                  *)0;
        p_ctrl->CommPtr          = (USBD_CDC_COMM         *)0;
    }

    for (ix = 0u; ix < USBD_CDC_COMM_NBR_MAX; ix++) {           /* Init CDC comm tbl.                                   */
        p_comm                     = &USBD_CDC_CommTbl[ix];
        p_comm->CtrlPtr            = (USBD_CDC_CTRL *)0;
        p_comm->NotifyIn           =  USBD_EP_ADDR_NONE;
        p_comm->DataIF_EP_Ix       =  0u;
        p_comm->CCI_IF_Nbr         =  USBD_IF_NBR_NONE;
        p_comm->NotifyInActiveXfer =  DEF_NO;
    }


    for (ix = 0u; ix < USBD_CDC_CFG_MAX_NBR_DATA_IF; ix++) {    /* Init CDC data IF tbl.                                */
        p_data_if           = &USBD_CDC_DataIF_Tbl[ix];
        p_data_if->Protocol =  USBD_CDC_DATA_PROTOCOL_NONE;
        p_data_if->IsocEn   =  DEF_DISABLED;
        p_data_if->NextPtr  = (USBD_CDC_DATA_IF *)0;
        p_data_if->IF_Nbr   =  USBD_IF_NBR_NONE;
    }

    for (ix = 0u; ix < USBD_CDC_DATA_IF_EP_NBR_MAX; ix++) {    /* Init CDC data IF EP tbl.                              */
        p_data_ep          = &USBD_CDC_DataIF_EP_Tbl[ix];
        p_data_ep->DataIn  =  USBD_EP_ADDR_NONE;
        p_data_ep->DataOut =  USBD_EP_ADDR_NONE;
    }

    USBD_CDC_CtrlNbrNext       = 0u;
    USBD_CDC_CommNbrNext       = 0u;
    USBD_CDC_DataIF_NbrNext    = 0u;
    USBD_CDC_DataIF_EP_NbrNext = 0u;

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           USBD_CDC_Add()
*
* Description : Add a new instance of the CDC class.
*
* Argument(s) : subclass            Communication class subclass subcode (see Note #1).
*
*               p_subclass_drv      Pointer to CDC subclass driver.
*
*               p_subclass_arg      Pointer to CDC subclass driver argument.
*
*               protocol            Communication class protocol code.
*
*               notify_en           Notification enabled :
*
*                                       DEF_ENABLED   Enable  CDC class notifications.
*                                       DEF_DISABLED  Disable CDC class notifications.
*
*               notify_interval     Notification interval in milliseconds. It must be a power of 2.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE       CDC class instance successfully added.
*                               USBD_ERR_ALLOC      CDC class instances NOT available.
*
* Return(s)   : CDC class instance number, if NO error(s).
*
*               USBD_CDC_NBR_NONE,         otherwise.
*
* Note(s)     : (1) Communication class subclass codes are defined in 'usbd_cdc.h'
*                   'USBD_CDC_SUBCLASS_XXXX'.
*********************************************************************************************************
*/

CPU_INT08U  USBD_CDC_Add (CPU_INT08U              subclass,
                          USBD_CDC_SUBCLASS_DRV  *p_subclass_drv,
                          void                   *p_subclass_arg,
                          CPU_INT08U              protocol,
                          CPU_BOOLEAN             notify_en,
                          CPU_INT16U              notify_interval,
                          USBD_ERR               *p_err)
{
    CPU_INT08U      cdc_nbr;
    USBD_CDC_CTRL  *p_ctrl;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }

    if ((notify_en                     == DEF_ENABLED) &&
        (MATH_IS_PWR2(notify_interval) == DEF_NO)) {            /* Interval must be a power of 2.                       */
       *p_err = USBD_ERR_INVALID_ARG;
        return (USBD_CDC_NBR_NONE);
    }
#endif

    CPU_CRITICAL_ENTER();
    cdc_nbr = USBD_CDC_CtrlNbrNext;                             /* Alloc new CDC class.                                 */

    if (cdc_nbr >= USBD_CDC_CFG_MAX_NBR_DEV) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_CDC_INSTANCE_ALLOC;
        return (USBD_CDC_NBR_NONE);
    }

    USBD_CDC_CtrlNbrNext++;
    CPU_CRITICAL_EXIT();

    p_ctrl = &USBD_CDC_CtrlTbl[cdc_nbr];                        /* Get & init CDC struct.                               */

    p_ctrl->SubClassCode     = subclass;
    p_ctrl->SubClassProtocol = protocol;
    p_ctrl->NotifyEn         = notify_en;
    p_ctrl->NotifyInterval   = notify_interval;
    p_ctrl->SubClassDrvPtr   = p_subclass_drv;
    p_ctrl->SubClassArg      = p_subclass_arg;

   *p_err = USBD_ERR_NONE;

    return (cdc_nbr);
}


/*
*********************************************************************************************************
*                                          USBD_CDC_AddCfg()
*
* Description : Add CDC instance into USB device configuration.
*
* Argument(s) : class_nbr   Class instance number.
*
*               dev_nbr     Device number.
*
*               cfg_nbr     Configuration index to add new CDC class interface to.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   CDC class configuration successfully added.
*                               USBD_ERR_ALLOC                  CDC class communication instance NOT available.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'.
*
*                                                               --------- RETURNED BY USBD_IF_Add() : ----------
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_desc'/
*                                                                   'class_desc_size'.
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE      Invalid device state.
*                               USBD_ERR_CFG_INVALID_NBR        Invalid configuration number.
*                               USBD_ERR_IF_ALLOC               Interfaces                   NOT available.
*                               USBD_ERR_IF_ALT_ALLOC           Interface alternate settings NOT available.
*
*                                                               --------- RETURNED BY USBD_BulkAdd() : ---------
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'max_pkt_len'.
*
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device        number.
*                               USBD_ERR_CFG_INVALID_NBR        Invalid configuration number.
*                               USBD_ERR_IF_INVALID_NBR         Invalid interface     number.
*                               USBD_ERR_EP_NONE_AVAIL          Physical endpoint NOT available.
*                               USBD_ERR_EP_ALLOC               Endpoints NOT available.
*
*                                                               --------- RETURNED BY USBD_IntrAdd() : ---------
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'interval'/
*                                                                   'max_pkt_len'.
*
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device        number.
*                               USBD_ERR_CFG_INVALID_NBR        Invalid configuration number.
*                               USBD_ERR_IF_INVALID_NBR         Invalid interface     number.
*                               USBD_ERR_EP_NONE_AVAIL          Physical endpoint NOT available.
*                               USBD_ERR_EP_ALLOC               Endpoints NOT available.
*
*                                                               --------- RETURNED BY USBD_IF_Grp() : ----------
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device        number.
*                               USBD_ERR_CFG_INVALID_NBR        Invalid configuration number.
*                               USBD_ERR_IF_INVALID_NBR         Invalid interface     number.
*                               USBD_ERR_IF_GRP_NBR_IN_USE      Interface is part of another group.
*                               USBD_ERR_IF_GRP_ALLOC           Interface groups NOT available.
*
* Return(s)   : DEF_YES, if CDC class instance added to USB device configuration successfully.
*
*               DEF_NO,  otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  USBD_CDC_CfgAdd (CPU_INT08U   class_nbr,
                              CPU_INT08U   dev_nbr,
                              CPU_INT08U   cfg_nbr,
                              USBD_ERR    *p_err)
{
    USBD_CDC_CTRL        *p_ctrl;
    USBD_CDC_COMM        *p_comm;
    USBD_CDC_DATA_IF_EP  *p_data_ep;
    USBD_CDC_DATA_IF     *p_data_if;
    CPU_INT08U            if_nbr;
    CPU_INT08U            ep_addr;
    CPU_INT16U            comm_nbr;
    CPU_INT16U            data_if_nbr_cur = 0u;
    CPU_INT16U            data_if_nbr_end = 0u;
    CPU_INT16U            data_if_ix;
    CPU_INT16U            notify_interval;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(DEF_NO);
    }
#endif

    if (class_nbr >= USBD_CDC_CtrlNbrNext) {                    /* Check CDC class instance nbr.                        */
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (DEF_NO);
    }

    p_ctrl = &USBD_CDC_CtrlTbl[class_nbr];

    CPU_CRITICAL_ENTER();
    comm_nbr = USBD_CDC_CommNbrNext;                            /* Alloc CDC class comm info.                           */
    if (comm_nbr >= USBD_CDC_COMM_NBR_MAX) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_CDC_INSTANCE_ALLOC;
        return (DEF_NO);
    }

    USBD_CDC_CommNbrNext++;
    p_comm = &USBD_CDC_CommTbl[comm_nbr];

    if (p_ctrl->DataIF_Nbr > 0u) {
        data_if_nbr_cur = USBD_CDC_DataIF_EP_NbrNext;           /* Alloc data IFs EP struct.                            */
        data_if_nbr_end = data_if_nbr_cur + p_ctrl->DataIF_Nbr;
        if (data_if_nbr_end > USBD_CDC_DATA_IF_EP_NBR_MAX) {
            CPU_CRITICAL_EXIT();
           *p_err = USBD_ERR_CDC_INSTANCE_ALLOC;
            return (DEF_NO);
        }

        USBD_CDC_DataIF_EP_NbrNext = data_if_nbr_end;
    }
    CPU_CRITICAL_EXIT();

    if_nbr = USBD_IF_Add(        dev_nbr,                       /* Add CDC comm IF to cfg.                              */
                                 cfg_nbr,
                                &USBD_CDC_CommDrv,
                         (void *)p_comm,
                         (void *)0,
                                 USBD_CLASS_CODE_CDC_CONTROL,
                                 p_ctrl->SubClassCode,
                                 p_ctrl->SubClassProtocol,
                                "CDC Comm Interface",
                                 p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (DEF_NO);
    }

    p_comm->CCI_IF_Nbr = if_nbr;

    if (p_ctrl->NotifyEn == DEF_TRUE) {

        if (DEF_BIT_IS_CLR(cfg_nbr, USBD_CFG_NBR_SPD_BIT) == DEF_YES) {
            notify_interval = p_ctrl->NotifyInterval;           /* In FS, bInterval in frames.                          */
        } else {
            notify_interval = p_ctrl->NotifyInterval * 8u;      /* In HS, bInterval in microframes.                     */
        }

        ep_addr = USBD_IntrAdd(dev_nbr,                         /* Add interrupt (IN) EP for notifications.             */
                               cfg_nbr,
                               if_nbr,
                               0u,
                               DEF_YES,
                               0u,
                               notify_interval,
                               p_err);
        if (*p_err != USBD_ERR_NONE) {
            return (DEF_NO);
        }

        p_comm->NotifyIn = ep_addr;
    }


    if (p_ctrl->DataIF_Nbr > 0u) {                              /* Add CDC data IFs to cfg.                             */
        p_comm->DataIF_EP_Ix = data_if_nbr_cur;
        p_data_if            = p_ctrl->DataIF_HeadPtr;

        for (data_if_ix = data_if_nbr_cur; data_if_ix < data_if_nbr_end; data_if_ix++) {
            p_data_ep  = &USBD_CDC_DataIF_EP_Tbl[data_if_ix];

                                                                /* Add CDC data IF to cfg.                              */
            if_nbr = USBD_IF_Add(        dev_nbr,
                                         cfg_nbr,
                                        &USBD_CDC_DataDrv,
                                 (void *)p_comm,
                                 (void *)0u,
                                         USBD_CLASS_CODE_CDC_DATA,
                                         USBD_SUBCLASS_CODE_USE_IF_DESC,
                                         p_data_if->Protocol,
                                        "CDC Data Interface",
                                         p_err);
            if (*p_err != USBD_ERR_NONE) {
                return (DEF_NO);
            }

            p_data_if->IF_Nbr = if_nbr;

            if (p_data_if->IsocEn == DEF_DISABLED) {
                                                                /* Add Bulk IN EP.                                      */
                ep_addr = USBD_BulkAdd(dev_nbr,
                                       cfg_nbr,
                                       if_nbr,
                                       0u,
                                       DEF_YES,
                                       0u,
                                       p_err);
                if (*p_err != USBD_ERR_NONE) {
                    return (DEF_NO);
                }

                p_data_ep->DataIn = ep_addr;
                                                                /* Add Bulk OUT EP.                                     */
                ep_addr = USBD_BulkAdd(dev_nbr,
                                       cfg_nbr,
                                       if_nbr,
                                       0u,
                                       DEF_NO,
                                       0u,
                                       p_err);
                if (*p_err != USBD_ERR_NONE) {
                    return (DEF_NO);
                }

                p_data_ep->DataOut = ep_addr;
                p_data_if          = p_data_if->NextPtr;
                                                                /* Group comm IF with data IFs.                         */
                (void)USBD_IF_Grp( dev_nbr,
                                   cfg_nbr,
                                   USBD_CLASS_CODE_CDC_CONTROL,
                                   p_ctrl->SubClassCode,
                                   p_ctrl->SubClassProtocol,
                                   p_comm->CCI_IF_Nbr,
                                   p_ctrl->DataIF_Nbr + 1u,
                                  "CDC Device",
                                   p_err);
                if (*p_err != USBD_ERR_NONE) {
                    return (DEF_NO);
                }
            }
        }
    }

    p_comm->DevNbr  = dev_nbr;
    p_comm->CtrlPtr = p_ctrl;

    CPU_CRITICAL_ENTER();
    p_ctrl->State   =  USBD_CDC_STATE_INIT;
    p_ctrl->CommPtr = (USBD_CDC_COMM *)0;
    CPU_CRITICAL_EXIT();

   *p_err = USBD_ERR_NONE;

    return (DEF_YES);
}


/*
*********************************************************************************************************
*                                          USBD_CDC_IsConn()
*
* Description : Get the CDC class connection state.
*
* Argument(s) : class_nbr   Class instance number.
*
* Return(s)   : DEF_YES, if CDC class is connected.
*
*               DEF_NO,  otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  USBD_CDC_IsConn (CPU_INT08U  class_nbr)
{
    USBD_CDC_CTRL   *p_ctrl;
    USBD_CDC_COMM   *p_comm;
    USBD_DEV_STATE   state;
    USBD_ERR         err;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (class_nbr >= USBD_CDC_CtrlNbrNext) {                    /* Check CDC class instance nbr.                        */
        return (DEF_NO);
    }
#endif

    p_ctrl = &USBD_CDC_CtrlTbl[class_nbr];

    if (p_ctrl->CommPtr == (USBD_CDC_COMM *)0) {
        return (DEF_NO);
    }

    p_comm = p_ctrl->CommPtr;
    state  = USBD_DevStateGet(p_comm->DevNbr, &err);            /* Get dev state.                                       */

    if ((err           == USBD_ERR_NONE            ) &&
        (state         == USBD_DEV_STATE_CONFIGURED) &&
        (p_ctrl->State == USBD_CDC_STATE_CFG       )) {
        return (DEF_YES);
    }

    return (DEF_NO);
}


/*
*********************************************************************************************************
*                                        USBD_CDC_DataIF_Add()
*
* Description : Add data interface class to CDC communication interface class.
*
* Argument(s) : class_nbr   Class instance number.
*
*               isoc_en     Data interface isochronous enable (see Note #1) :
*
*                               DEF_ENABLED   Data interface uses isochronous EPs.
*                               DEF_DISABLED  Data interface uses bulk        EPs.
*
*               protocol    Data interface protocol code.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Data interface successfully added.
*                               USBD_ERR_ALLOC                  Data interface instance NOT available.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr/
*                                                                   'isoc_en'.
*
* Return(s)   : Data interface number.
*
* Note(s)     : (1) $$$$ The value of 'isoc_en' must be DEF_DISABLED. Isochronous EPs are not supported.
*********************************************************************************************************
*/

CPU_INT08U  USBD_CDC_DataIF_Add (CPU_INT08U    class_nbr,
                                 CPU_BOOLEAN   isoc_en,
                                 CPU_INT08U    protocol,
                                 USBD_ERR     *p_err)
{
    USBD_CDC_CTRL     *p_ctrl;
    USBD_CDC_DATA_IF  *p_data_if;
    CPU_INT16U         data_if_ix;
    CPU_INT08U         data_if_nbr;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }

                                                                /* Check 'isoc_en' argument (see Note #1) .             */
    if (isoc_en == DEF_ENABLED) {
       *p_err = USBD_ERR_INVALID_ARG;
        return (USBD_CDC_DATA_IF_NBR_NONE);
    }
#endif
                                                                /* Check CDC class instance nbr.                        */
    if (class_nbr >= USBD_CDC_CtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (USBD_CDC_DATA_IF_NBR_NONE);
    }

    CPU_CRITICAL_ENTER();
    data_if_ix = USBD_CDC_DataIF_NbrNext;                       /* Alloc data interface.                                */

    if (data_if_ix >= USBD_CDC_CFG_MAX_NBR_DATA_IF) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_CDC_DATA_IF_ALLOC;
        return (USBD_CDC_DATA_IF_NBR_NONE);
    }

    USBD_CDC_DataIF_NbrNext++;

    p_ctrl      = &USBD_CDC_CtrlTbl[class_nbr];
    p_data_if   = &USBD_CDC_DataIF_Tbl[data_if_ix];
    data_if_nbr =  p_ctrl->DataIF_Nbr;

    if (data_if_nbr == USBD_CDC_DATA_IF_NBR_TOTAL) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_CDC_DATA_IF_ALLOC;
        return (USBD_CDC_DATA_IF_NBR_NONE);
    }

    if (p_ctrl->DataIF_HeadPtr == (USBD_CDC_DATA_IF *)0) {      /* Add data IF in the list.                             */
        p_ctrl->DataIF_HeadPtr = p_data_if;
    } else {
        p_ctrl->DataIF_TailPtr->NextPtr = p_data_if;
    }
    p_ctrl->DataIF_TailPtr = p_data_if;
    p_ctrl->DataIF_Nbr++;

    CPU_CRITICAL_EXIT();

    p_data_if->Protocol =  protocol;
    p_data_if->IsocEn   =  isoc_en;
    p_data_if->NextPtr  = (USBD_CDC_DATA_IF *)0;

    return (data_if_nbr);
}


/*
*********************************************************************************************************
*                                          USBD_CDC_DataRx()
*
* Description : Receive data on CDC data interface.
*
* Argument(s) : class_nbr       Class instance number.
*
*               data_if_nbr     CDC data interface number.
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
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'/
*                                                                   'data_if_nbr'.
*                               USBD_ERR_INVALID_CLASS_STATE    Invalid subclass state.
*
*                                                               --------- RETURNED BY USBD_BulkRx() : ---------
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE      Transfer type only available if device is in
*                                                                   configured state.
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

* Return(s)   : Number of octets received, if NO error(s).
*
*               0,                         otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT32U  USBD_CDC_DataRx (CPU_INT08U   class_nbr,
                             CPU_INT08U   data_if_nbr,
                             CPU_INT08U  *p_buf,
                             CPU_INT32U   buf_len,
                             CPU_INT16U   timeout,
                             USBD_ERR    *p_err)
{
    USBD_CDC_CTRL        *p_ctrl;
    USBD_CDC_COMM        *p_comm;
    USBD_CDC_DATA_IF     *p_data_if;
    USBD_CDC_DATA_IF_EP  *p_data_ep;
    CPU_INT32U            xfer_len;
    CPU_INT16U            data_if_ix;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    if (class_nbr >= USBD_CDC_CtrlNbrNext) {                    /* Check CDC class instance nbr.                        */
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (0u);
    }

    p_ctrl = &USBD_CDC_CtrlTbl[class_nbr];

    if (p_ctrl->State != USBD_CDC_STATE_CFG) {                  /* Transfers are only valid in cfg state.               */
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return (0u);
    }

    if (data_if_nbr >= p_ctrl->DataIF_Nbr) {                    /* Check 'data_if_nbr' is valid.                        */
       *p_err = USBD_ERR_INVALID_ARG;
        return(0u);
    }

    p_comm    = p_ctrl->CommPtr;
    p_data_if = p_ctrl->DataIF_HeadPtr;
                                                                /* Find data IF struct.                                 */
    for (data_if_ix = 0u; data_if_ix < data_if_nbr; data_if_ix++) {
        p_data_if = p_data_if->NextPtr;
    }

    data_if_ix =  p_comm->DataIF_EP_Ix + data_if_nbr;
    p_data_ep  = &USBD_CDC_DataIF_EP_Tbl[data_if_ix];
    xfer_len   =  0u;

    if (p_data_if->IsocEn == DEF_DISABLED) {
        xfer_len = USBD_BulkRx(p_comm->DevNbr,
                               p_data_ep->DataOut,
                               p_buf,
                               buf_len,
                               timeout,
                               p_err);
    } else {
        *p_err = USBD_ERR_DEV_UNAVAIL_FEAT;                     /* $$$$ Isoc transfer not supported.                    */
         return (0u);
    }


    return (xfer_len);
}


/*
*********************************************************************************************************
*                                          USBD_CDC_DataTx()
*
* Description : Send data on CDC data interface.
*
* Argument(s) : class_nbr       Class instance number.
*
*               data_if_nbr     CDC data interface number.
*
*               p_buf           Pointer to buffer of data that will be transmitted.
*
*               buf_len         Number of octets to transmit.
*
*               timeout_ms      Timeout in milliseconds.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE                   Data successfully received.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'/
*                                                                   'data_if_nbr'.
*                               USBD_ERR_INVALID_CLASS_STATE    Invalid subclass state.
*
*                                                               --------- RETURNED BY USBD_BulkTx() : ---------
*                               USBD_ERR_DEV_INVALID_STATE      Transfer type only available if device is in
*                                                                   configured state.
*                               USBD_ERR_EP_INVALID_ADDR        Invalid endpoint address.
*                               USBD_ERR_EP_INVALID_STATE       Invalid endpoint state.
*                               USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*
*                               USBD_ERR_OS_TIMEOUT             OS signal NOT successfully acquired in the time
*                                                                   specified by 'timeout_ms'.
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
*
* Return(s)   : Number of octets transmitted, if NO error(s).
*
*               0,                            otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT32U  USBD_CDC_DataTx (CPU_INT08U   class_nbr,
                             CPU_INT08U   data_if_nbr,
                             CPU_INT08U  *p_buf,
                             CPU_INT32U   buf_len,
                             CPU_INT16U   timeout,
                             USBD_ERR    *p_err)
{
    USBD_CDC_CTRL        *p_ctrl;
    USBD_CDC_COMM        *p_comm;
    USBD_CDC_DATA_IF     *p_data_if;
    USBD_CDC_DATA_IF_EP  *p_data_ep;
    CPU_INT32U            xfer_len;
    CPU_INT16U            data_if_ix;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    if (class_nbr >= USBD_CDC_CtrlNbrNext) {                    /* Check CDC class instance nbr.                        */
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (0u);
    }

    p_ctrl = &USBD_CDC_CtrlTbl[class_nbr];

    if (p_ctrl->State != USBD_CDC_STATE_CFG) {                  /* Transfers are only valid in cfg state.               */
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return (0u);
    }

    if (data_if_nbr >= p_ctrl->DataIF_Nbr) {                    /* Check 'data_if_nbr' is valid.                        */
       *p_err = USBD_ERR_INVALID_ARG;
        return(0u);
    }

    p_comm    = p_ctrl->CommPtr;
    p_data_if = p_ctrl->DataIF_HeadPtr;
                                                                /* Find data IF struct.                                 */
    for (data_if_ix = 0u; data_if_ix < data_if_nbr; data_if_ix++) {
        p_data_if = p_data_if->NextPtr;
    }

    data_if_ix =  p_comm->DataIF_EP_Ix + data_if_nbr;
    p_data_ep  = &USBD_CDC_DataIF_EP_Tbl[data_if_ix];
    xfer_len   =  0u;

    if (p_data_if->IsocEn == DEF_DISABLED) {
        xfer_len = USBD_BulkTx(p_comm->DevNbr,
                               p_data_ep->DataIn,
                               p_buf,
                               buf_len,
                               timeout,
                               DEF_YES,
                               p_err);
    } else {
        *p_err = USBD_ERR_DEV_UNAVAIL_FEAT;                     /* $$$$ Isoc transfer not supported.                    */
         return (0u);
    }

    return (xfer_len);
}


/*
*********************************************************************************************************
*                                          USBD_CDC_Notify()
*
* Description : Send communication interface class notification to the host.
*
* Argument(s) : class_nbr       Class instance number.
*
*               notification    Notification code.
*
*               value           Notification value.
*
*               p_buf           Pointer to notification buffer (see Note #1).
*
*               data_len        Length of the data portion of the notification.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*
*                               USBD_ERR_NONE                   Data successfully received.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'
*                               USBD_ERR_INVALID_CLASS_STATE    Invalid class state.
*
*                                                               ------- RETURNED BY USBD_IntrTxAsync() : -------
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE      Transfer type only available if device is in
*                                                                   configured state.
*                               USBD_ERR_EP_INVALID_ADDR        Invalid endpoint address.
*                               USBD_ERR_EP_INVALID_STATE       Invalid endpoint state.
*                               USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*                               USBD_ERR_OS_TIMEOUT             OS signal NOT successfully acquired in the time
*                                                                   specified by 'timeout_ms'.
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
*
* Return(s)   : none.
*
* Note(s)     : (1) The notification buffer MUST contain space for the notification header
*                   'USBD_CDC_NOTIFICATION_HEADER' plus the variable-length data portion.
*********************************************************************************************************
*/

CPU_BOOLEAN  USBD_CDC_Notify (CPU_INT08U   class_nbr,
                              CPU_INT08U   notification,
                              CPU_INT16U   value,
                              CPU_INT08U  *p_buf,
                              CPU_INT16U   data_len,
                              USBD_ERR    *p_err)
{
    USBD_CDC_COMM  *p_comm;
    USBD_CDC_CTRL  *p_ctrl;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(DEF_FAIL);
    }
#endif

    if (class_nbr >= USBD_CDC_CtrlNbrNext) {                    /* Check CDC class instance nbr.                        */
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (DEF_FAIL);
    }

    p_ctrl = &USBD_CDC_CtrlTbl[class_nbr];

    if (p_ctrl->State != USBD_CDC_STATE_CFG) {                  /* Transfers are only valid in cfg state.               */
       *p_err = USBD_ERR_INVALID_CLASS_STATE;
        return (DEF_FAIL);
    }
    p_comm = p_ctrl->CommPtr;

    p_buf[0] = (1u      )                                       /* Recipient : Interface.                               */
             | (1u << 5u)                                       /* Type      : Class.                                   */
             |  DEF_BIT_07;                                     /* Direction : Device to host.                          */
    p_buf[1] =  notification;
    p_buf[4] =  p_comm->CCI_IF_Nbr;
    p_buf[5] =  0u;
    MEM_VAL_SET_INT16U_LITTLE(&p_buf[2], value);
    MEM_VAL_SET_INT16U_LITTLE(&p_buf[6], data_len);

    CPU_CRITICAL_ENTER();
    if (p_comm->NotifyInActiveXfer == DEF_NO) {                 /* Check if another xfer is already in progress.        */
        p_comm->NotifyInActiveXfer = DEF_YES;                   /* Indicate that a xfer is in progres.                  */
        CPU_CRITICAL_EXIT();
        USBD_IntrTxAsync(            p_comm->DevNbr,
                                     p_comm->NotifyIn,
                                     p_buf,
                         (CPU_INT32U)data_len + USBD_CDC_NOTIFICATION_HEADER,
                                     USBD_CDC_NotifyCmpl,
                         (void     *)p_comm,
                                     DEF_YES,
                                     p_err);
        if (*p_err != USBD_ERR_NONE) {
            CPU_CRITICAL_ENTER();
            p_comm->NotifyInActiveXfer = DEF_NO;
            CPU_CRITICAL_EXIT();
            return (DEF_FAIL);
        }
    } else {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_CLASS_XFER_IN_PROGRESS;
        return (DEF_FAIL);
    }

    return (DEF_OK);
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
*                                           USBD_CDC_Conn()
*
* Description : Notify class that configuration is active.
*
* Argument(s) : dev_nbr         Device number.
*
*               cfg_nbr         Configuration ix to add the interface.
*
*               p_if_class_arg  Pointer to class argument specific to interface.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_CDC_Conn (CPU_INT08U   dev_nbr,
                             CPU_INT08U   cfg_nbr,
                             void        *p_if_class_arg)
{
    USBD_CDC_COMM  *p_comm;
    CPU_SR_ALLOC();


    (void)cfg_nbr;
    (void)dev_nbr;

    p_comm = (USBD_CDC_COMM *)p_if_class_arg;
    CPU_CRITICAL_ENTER();
    p_comm->CtrlPtr->CommPtr = p_comm;
    p_comm->CtrlPtr->State   = USBD_CDC_STATE_CFG;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                         USBD_CDC_Disconn()
*
* Description : Notify class that configuration is not active.
*
* Argument(s) : dev_nbr         Device number.
*
*               cfg_nbr         Configuration ix to add the interface.
*
*               p_if_class_arg  Pointer to class argument specific to interface.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_CDC_Disconn (CPU_INT08U   dev_nbr,
                                CPU_INT08U   cfg_nbr,
                                void        *p_if_class_arg)
{
    USBD_CDC_COMM  *p_comm;
    CPU_SR_ALLOC();


    (void)cfg_nbr;
    (void)dev_nbr;

    p_comm = (USBD_CDC_COMM *)p_if_class_arg;
    CPU_CRITICAL_ENTER();
    p_comm->CtrlPtr->CommPtr = (USBD_CDC_COMM *)0;
    p_comm->CtrlPtr->State   =  USBD_CDC_STATE_INIT;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                         USBD_CDC_ClassReq()
*
* Description : Class request handler.
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
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_CDC_ClassReq (       CPU_INT08U       dev_nbr,
                                        const  USBD_SETUP_REQ  *p_setup_req,
                                               void            *p_if_class_arg)
{
    USBD_CDC_CTRL          *p_ctrl;
    USBD_CDC_COMM          *p_comm;
    USBD_CDC_SUBCLASS_DRV  *p_drv;
    CPU_BOOLEAN             valid;


    p_comm = (USBD_CDC_COMM *)p_if_class_arg;
    p_ctrl = p_comm->CtrlPtr;
    p_drv  = p_ctrl->SubClassDrvPtr;
    valid  = DEF_OK;

    if (p_drv->MngmtReq != (void *)0) {
                                                                /* Call subclass-specific management request handler.   */
        valid = p_drv->MngmtReq(dev_nbr,
                                p_setup_req,
                                p_ctrl->SubClassArg);
    }

    return (valid);
}


/*
*********************************************************************************************************
*                                        USBD_CDC_NotifyCmpl()
*
* Description : CDC notification complete callback.
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

static  void   USBD_CDC_NotifyCmpl (CPU_INT08U   dev_nbr,
                                    CPU_INT08U   ep_addr,
                                    void        *p_buf,
                                    CPU_INT32U   buf_len,
                                    CPU_INT32U   xfer_len,
                                    void        *p_arg,
                                    USBD_ERR     err)
{
    USBD_CDC_CTRL          *p_ctrl;
    USBD_CDC_COMM          *p_comm;
    USBD_CDC_SUBCLASS_DRV  *p_drv;


    (void)ep_addr;
    (void)p_buf;
    (void)buf_len;
    (void)xfer_len;
    (void)err;

    p_comm = (USBD_CDC_COMM *)p_arg;
    p_ctrl =  p_comm->CtrlPtr;
    p_drv  =  p_ctrl->SubClassDrvPtr;

    p_comm->NotifyInActiveXfer = DEF_NO;                        /* Xfer finished, no more active xfer.                  */
    if (p_drv->NotifyCmpl != (void *)0) {
        p_drv->NotifyCmpl(dev_nbr, p_ctrl->SubClassArg);
    }
}


/*
*********************************************************************************************************
*                                        USBD_CDC_CommIF_Desc()
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

static  void  USBD_CDC_CommIF_Desc (CPU_INT08U   dev_nbr,
                                    CPU_INT08U   cfg_nbr,
                                    CPU_INT08U   if_nbr,
                                    CPU_INT08U   if_alt_nbr,
                                    void        *p_if_class_arg,
                                    void        *p_if_alt_class_arg)
{
    USBD_CDC_CTRL          *p_ctrl;
    USBD_CDC_COMM          *p_comm;
    USBD_CDC_SUBCLASS_DRV  *p_drv;
    USBD_CDC_DATA_IF       *p_data_if;
    CPU_INT08U              desc_size;
    CPU_INT08U              data_if_nbr;


    (void)cfg_nbr;
    (void)if_nbr;
    (void)if_alt_nbr;
    (void)p_if_alt_class_arg;

    p_comm = (USBD_CDC_COMM *)p_if_class_arg;
    p_ctrl =  p_comm->CtrlPtr;
                                                                /* ------------- BUILD HEADER DESCRIPTOR -------------- */
    USBD_DescWr08(dev_nbr, USBD_CDC_DESC_SIZE_HEADER);
    USBD_DescWr08(dev_nbr, USBD_CDC_DESC_TYPE_CS_IF);
    USBD_DescWr08(dev_nbr, USBD_CDC_DESC_SUBTYPE_HEADER);
    USBD_DescWr16(dev_nbr, 0x0120u);                            /* CDC release number (1.20) in BCD fmt.                */

                                                                /* ------------- BUILD UNION IF DESCRIPTOR ------------ */
    if (p_ctrl->DataIF_Nbr > 0u) {
        desc_size = USBD_CDC_DESC_SIZE_UNION_MIN + p_ctrl->DataIF_Nbr;

        USBD_DescWr08(dev_nbr, desc_size);
        USBD_DescWr08(dev_nbr, USBD_CDC_DESC_TYPE_CS_IF);
        USBD_DescWr08(dev_nbr, USBD_CDC_DESC_SUBTYPE_UNION);
        USBD_DescWr08(dev_nbr, p_comm->CCI_IF_Nbr);

        p_data_if = p_ctrl->DataIF_HeadPtr;                     /* Add all subordinate data IFs.                        */

        for (data_if_nbr = 0u; data_if_nbr < p_ctrl->DataIF_Nbr; data_if_nbr++) {
            USBD_DescWr08(dev_nbr, p_data_if->IF_Nbr);
            p_data_if = p_data_if->NextPtr;
        }
    }

    p_drv = p_ctrl->SubClassDrvPtr;

    if (p_drv->FnctDesc != (void *)0) {
                                                                /* Call subclass-specific functional descriptor.        */;
        p_drv->FnctDesc(dev_nbr,
                        p_ctrl->SubClassArg,
                        p_ctrl->DataIF_HeadPtr->IF_Nbr);
    }
}



/*
*********************************************************************************************************
*                                    USBD_CDC_CommIF_DescSizeGet()
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

static  CPU_INT16U  USBD_CDC_CommIF_DescSizeGet (CPU_INT08U   dev_nbr,
                                                 CPU_INT08U   cfg_nbr,
                                                 CPU_INT08U   if_nbr,
                                                 CPU_INT08U   if_alt_nbr,
                                                 void        *p_if_class_arg,
                                                 void        *p_if_alt_class_arg)
{
    USBD_CDC_CTRL          *p_ctrl;
    USBD_CDC_COMM          *p_comm;
    USBD_CDC_SUBCLASS_DRV  *p_drv;
    CPU_INT16U              desc_size;


    (void)cfg_nbr;
    (void)if_nbr;
    (void)if_alt_nbr;
    (void)p_if_alt_class_arg;

    p_comm = (USBD_CDC_COMM *)p_if_class_arg;
    p_ctrl = p_comm->CtrlPtr;

    desc_size = USBD_CDC_DESC_SIZE_HEADER;

    if (p_ctrl->DataIF_Nbr > 0u) {
        desc_size += USBD_CDC_DESC_SIZE_UNION_MIN;
        desc_size += p_ctrl->DataIF_Nbr;
    }

    p_drv = p_ctrl->SubClassDrvPtr;

    if (p_drv->FnctDescSizeGet != (void *)0) {
        desc_size += p_drv->FnctDescSizeGet(dev_nbr, p_ctrl->SubClassArg);
    }

    return (desc_size);
}
