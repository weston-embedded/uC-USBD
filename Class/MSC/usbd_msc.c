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
*                                        USB DEVICE MSC CLASS
*
* Filename : usbd_msc.c
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    USBD_MSC_MODULE
#include  "usbd_msc.h"
#include  "usbd_scsi.h"
#include  "usbd_msc_os.h"
#include  <lib_str.h>
#include  <lib_ascii.h>


/*
*********************************************************************************************************
*                                     EXTERNAL C LANGUAGE LINKAGE
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  USBD_MSC_SIG_CBW                          0x43425355
#define  USBD_MSC_SIG_CSW                          0x53425355

#define  USBD_MSC_LEN_CBW                                  31
#define  USBD_MSC_LEN_CSW                                  13

#define  USBD_MSC_DEV_STR_LEN                             12u

#define  USBD_MSC_CTRL_REQ_TIMEOUT_mS                   5000u

#define  USBD_DBG_MSC_MSG(msg)               USBD_DBG_GENERIC    ((msg),                                            \
                                                                   USBD_EP_ADDR_NONE,                               \
                                                                   USBD_IF_NBR_NONE)

#define  USBD_DBG_MSC_ARG(msg, arg)          USBD_DBG_GENERIC_ARG((msg),                                            \
                                                                   USBD_EP_ADDR_NONE,                               \
                                                                   USBD_IF_NBR_NONE,                                \
                                                                  (arg))

#define  USBD_MSC_COM_NBR_MAX               (USBD_MSC_CFG_MAX_NBR_DEV * \
                                             USBD_MSC_CFG_MAX_NBR_CFG)


/*
*********************************************************************************************************
*                                           SUBCLASS CODES
*
* Note(s) : (1) See 'USB Mass Storage Class Specification Overview', Revision 1.2, Section 2
*********************************************************************************************************
*/

#define  USBD_MSC_SUBCLASS_CODE_RBC                      0x01
#define  USBD_MSC_SUBCLASS_CODE_SFF_8020i                0x02
#define  USBD_MSC_SUBCLASS_CODE_MMC_2                    0x02
#define  USBD_MSC_SUBCLASS_CODE_QIC_157                  0x03
#define  USBD_MSC_SUBCLASS_CODE_UFI                      0x04
#define  USBD_MSC_SUBCLASS_CODE_SFF_8070i                0x05
#define  USBD_MSC_SUBCLASS_CODE_SCSI                     0x06

/*
*********************************************************************************************************
*                                           PROTOCOL CODES
*
* Note(s) : (1) See 'USB Mass Storage Class Specification Overview', Revision 1.2, Section 3
*********************************************************************************************************
*/

#define  USBD_MSC_PROTOCOL_CODE_CTRL_BULK_INTR_CMD_INTR  0x00
#define  USBD_MSC_PROTOCOL_CODE_CTRL_BULK_INTR           0x01
#define  USBD_MSC_PROTOCOL_CODE_BULK_ONLY                0x50

/*
*********************************************************************************************************
*                                       CLASS-SPECIFIC REQUESTS
*
* Note(s) : (1) See 'USB Mass Storage Class - Bulk Only Transport', Section 3.
*
*           (2) The 'bRequest' field of a class-specific setup request may contain one of these values.
*
*           (3) The mass storage reset request is "used to reset the mass storage device and its
*               associated interface".  The setup request packet will consist of :
*
*               (a) bmRequestType = 00100001b (class, interface, host-to-device)
*               (b) bRequest      =     0xFF
*               (c) wValue        =   0x0000
*               (d) wIndex        = Interface number
*               (e) wLength       =   0x0000
*
*           (4) The get max LUN is used to determine the number of LUN's supported by the device.  The
*               setup request packet will consist of :
*
*               (a) bmRequestType = 10100001b (class, interface, device-to-host)
*               (b) bRequest      =     0xFE
*               (c) wValue        =   0x0000
*               (d) wIndex        = Interface number
*               (e) wLength       =   0x0001
*********************************************************************************************************
*/

#define  USBD_MSC_REQ_MASS_STORAGE_RESET                 0xFF   /* See Notes #3.                                        */
#define  USBD_MSC_REQ_GET_MAX_LUN                        0xFE   /* See Notes #4.                                        */

/*
*********************************************************************************************************
*                                      COMMAND BLOCK FLAG VALUES
*
* Note(s) : (1) See 'USB Mass Storage Class - Bulk Only Transport', Section 5.1.
*
*           (2) The 'bmCBWFlags' field of a command block wrapper may contain one of these values.
*********************************************************************************************************
*/

#define  USBD_MSC_BMCBWFLAGS_DIR_HOST_TO_DEVICE          0x00
#define  USBD_MSC_BMCBWFLAGS_DIR_DEVICE_TO_HOST          0x80

/*
*********************************************************************************************************
*                                     COMMAND BLOCK STATUS VALUES
*
* Note(s) : (1) See 'USB Mass Storage Class - Bulk Only Transport', Section 5.3, Table 5.3.
*
*           (2) The 'bCSWStatus' field of a command status wrapper may contain one of these values.
*********************************************************************************************************
*/

#define  USBD_MSC_BCSWSTATUS_CMD_PASSED                  0x00
#define  USBD_MSC_BCSWSTATUS_CMD_FAILED                  0x01
#define  USBD_MSC_BCSWSTATUS_PHASE_ERROR                 0x02


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
*                                   COMMAND BLOCK WRAPPER DATA TYPE
*
* Note(s) : (1) See 'USB Mass Storage Class - Bulk Only Transport', Section 5.1.
*
*           (2) The 'bmCBWFlags' field is a bit-mapped datum with three subfields :
*
*               (a) Bit  7  : Data transfer direction :
*
*                   (1) 0 = Data-out from host   to device.
*                   (2) 1 = Data-in  from device to host.
*
*               (b) Bit  6  : Obsolete.  Should be set to zero.
*               (c) Bits 5-0: Reserved.  Should be set to zero.
*********************************************************************************************************
*/

typedef struct  usbd_msc_cbw {
    CPU_INT32U  dCBWSignature;                                  /* Sig that helps identify this data pkt as CBW.        */
    CPU_INT32U  dCBWTag;                                        /* A cmd block tag sent by the host.                    */
    CPU_INT32U  dCBWDataTransferLength;                         /* Nbr of bytes of data that host expects to xfer.      */
    CPU_INT08U  bmCBWFlags;                                     /* Flags (see Notes #2).                                */
    CPU_INT08U  bCBWLUN;                                        /* LUN to which the cmd blk is being sent.              */
    CPU_INT08U  bCBWCBLength;                                   /* Length of the CBWB in bytes.                         */
    CPU_INT08U  CBWCB[16];                                      /* Cmd blk to be executed by the dev.                   */
} USBD_MSC_CBW;

/*
*********************************************************************************************************
*                                  COMMAND STATUS WRAPPER DATA TYPE
*
* Note(s) : (1) See 'USB Mass Storage Class - Bulk Only Transport', Section 5.2.
*********************************************************************************************************
*/

typedef  struct  usbd_msc_csw {
    CPU_INT32U  dCSWSignature;                                  /* Sig that helps identify this data pkt as CSW.        */
    CPU_INT32U  dCSWTag;                                        /* Dev shall set this to the value in CBW's dCBWTag.    */
    CPU_INT32U  dCSWDataResidue;                                /* Difference between expected & actual nbr data bytes. */
    CPU_INT08U  bCSWStatus;                                     /* Indicates the success or failure of the cmd.         */
} USBD_MSC_CSW;


/*
*********************************************************************************************************
*                                        FORWARD DECLARATIONS
*********************************************************************************************************
*/

typedef struct usbd_msc_ctrl  USBD_MSC_CTRL;


/*
*********************************************************************************************************
*                                             MSC STATES
*********************************************************************************************************
*/

typedef  enum  usbd_msc_state {
    USBD_MSC_STATE_NONE = 0,
    USBD_MSC_STATE_INIT,
    USBD_MSC_STATE_CFG
} USBD_MSC_STATE;

typedef  enum  usbd_msc_comm_state {
    USBD_MSC_COMM_STATE_NONE = 0,
    USBD_MSC_COMM_STATE_CBW,
    USBD_MSC_COMM_STATE_DATA,
    USBD_MSC_COMM_STATE_CSW,
    USBD_MSC_COMM_STATE_RESET_RECOVERY_BULK_IN_STALL,
    USBD_MSC_COMM_STATE_RESET_RECOVERY_BULK_OUT_STALL,
    USBD_MSC_COMM_STATE_RESET_RECOVERY,
    USBD_MSC_COMM_STATE_BULK_IN_STALL,
    USBD_MSC_COMM_STATE_BULK_OUT_STALL
} USBD_MSC_COMM_STATE;


/*
**********************************************************************************************************
*                                       MSC EP REQUIREMENTS DATA TYPE
**********************************************************************************************************
*/

typedef struct usbd_msc_comm {                                  /* ---------- MSC COMMUNICATION INFORMATION ----------- */
    USBD_MSC_CTRL       *CtrlPtr;                               /* Ptr to ctrl information.                             */
    CPU_INT08U           DataBulkInEpAddr;
    CPU_INT08U           DataBulkOutEpAddr;
    USBD_MSC_COMM_STATE  NextCommState;                         /* Next comm state of the MSC device.                   */
    USBD_MSC_CBW         CBW;                                   /* Cmd blk wrapper (CBW).                               */
    USBD_MSC_CSW         CSW;                                   /* Cmd status word (CSW).                               */
    CPU_BOOLEAN          Stall;                                 /* Used to stall the endpoints.                         */
    CPU_INT32U           BytesToXfer;                           /* Current bytes to xfer during data xfer stage.        */
    void                *SCSIWrBufPtr;                          /* Ptr to the SCSI buf used to wr to SCSI.              */
    CPU_INT32U           SCSIWrBuflen;                          /* SCSI buf len used to wr to SCSI.                     */
} USBD_MSC_COMM;


struct usbd_msc_ctrl {                                          /* ------------- MSC CONTROL INFORMATION -------------- */
    CPU_INT08U         DevNbr;                                  /* MSC dev nbr.                                         */
    USBD_MSC_STATE     State;                                   /* MSC dev state.                                       */
    CPU_INT08U         MaxLun;                                  /* Max logical unit number (LUN).                       */
    USBD_MSC_LUN_CTRL  Lun[USBD_MSC_CFG_MAX_LUN];               /* Array of struct pointing to LUN's                    */
    USBD_MSC_COMM     *CommPtr;                                 /* MSC comm info ptr.                                   */
    CPU_INT08U        *CBW_BufPtr;                              /* Buf to rx Cmd Blk  Wrapper.                          */
    CPU_INT08U        *CSW_BufPtr;                              /* Buf to send Cmd Status Wrapper.                      */
    CPU_INT08U        *DataBufPtr;                              /* Buf to handle data stage.                            */
    CPU_INT08U        *CtrlStatusBufPtr;                        /* Buf used for ctrl status xfers.                      */
    CPU_INT32U         USBD_MSC_SCSI_Data_Len;
    CPU_INT08U         USBD_MSC_SCSI_Data_Dir;
};


/*
**********************************************************************************************************
*                                              LOCAL TABLES
**********************************************************************************************************
*/


/*
**********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
**********************************************************************************************************
*/
                                                                /* MSC instance array.                                  */
static  USBD_MSC_CTRL  USBD_MSCCtrlTbl[USBD_MSC_CFG_MAX_NBR_DEV];
static  CPU_INT08U     USBD_MSCCtrlNbrNext;
                                                                /* MSC comm array.                                      */
static  USBD_MSC_COMM  USBD_MSCCommTbl[USBD_MSC_COM_NBR_MAX];
static  CPU_INT16U     USBD_MSCCommNbrNext;


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void                 USBD_MSC_Conn          (      CPU_INT08U          dev_nbr,
                                                           CPU_INT08U          cfg_nbr,
                                                           void               *p_if_class_arg);

static  void                 USBD_MSC_Disconn       (      CPU_INT08U          dev_nbr,
                                                           CPU_INT08U          cfg_nbr,
                                                           void               *p_if_class_arg);

static  void                 USBD_MSC_EP_StateUpdate(      CPU_INT08U          dev_nbr,
                                                           CPU_INT08U          cfg_nbr,
                                                           CPU_INT08U          if_nbr,
                                                           CPU_INT08U          if_alt_nbr,
                                                           CPU_INT08U          ep_addr,
                                                           void               *p_if_class_arg,
                                                           void               *p_if_alt_class_arg);

static  CPU_BOOLEAN          USBD_MSC_ClassReq      (      CPU_INT08U          dev_nbr,
                                                     const USBD_SETUP_REQ     *p_setup_req,
                                                           void               *p_if_class_arg);

static  USBD_MSC_COMM_STATE  USBD_MSC_CommStateGet  (      CPU_INT08U          class_nbr);

static  void                 USBD_MSC_RxCBW         (      USBD_MSC_CTRL      *p_ctrl,
                                                           USBD_MSC_COMM      *p_comm,
                                                           USBD_ERR           *p_err);

static  void                 USBD_MSC_CMD_Process   (      USBD_MSC_CTRL      *p_ctrl,
                                                           USBD_MSC_COMM      *p_comm);

static  void                 USBD_MSC_TxCSW         (      USBD_MSC_CTRL      *p_ctrl,
                                                           USBD_MSC_COMM      *p_comm,
                                                           USBD_ERR           *p_err);

static  void                 USBD_MSC_CBW_Verify    (const USBD_MSC_CTRL      *p_ctrl,
                                                           USBD_MSC_COMM      *p_comm,
                                                           void               *p_cbw_buf,
                                                           CPU_INT32U          cbw_len,
                                                           USBD_ERR           *p_err);

static  void                 USBD_MSC_RespVerify    (      USBD_MSC_COMM      *p_comm,
                                                           CPU_INT32U          scsi_data_len,
                                                           CPU_INT08U          scsi_data_dir,
                                                           USBD_ERR           *p_err);

static  void                 USBD_MSC_SCSI_TxData   (      USBD_MSC_CTRL      *p_ctrl,
                                                           USBD_MSC_COMM      *p_comm);

static  void                 USBD_MSC_SCSI_Rd       (      USBD_MSC_CTRL      *p_ctrl,
                                                           USBD_MSC_COMM      *p_comm);

static  void                 USBD_MSC_SCSI_Wr       (const USBD_MSC_CTRL      *p_ctrl,
                                                           USBD_MSC_COMM      *p_comm,
                                                           void               *p_buf,
                                                           CPU_INT32U          xfer_len);

static  void                 USBD_MSC_SCSI_RxData   (      USBD_MSC_CTRL      *p_ctrl,
                                                           USBD_MSC_COMM      *p_comm);

static  void                 USBD_MSC_LunClr        (      USBD_MSC_LUN_CTRL  *p_lun);

static  void                 USBD_MSC_CBW_Parse     (      USBD_MSC_CBW       *p_cbw,
                                                           void               *p_buf_src);

static  void                 USBD_MSC_CSW_Fmt       (const USBD_MSC_CSW       *p_csw,
                                                           void               *p_buf_dest);


/*
*********************************************************************************************************
*                                              MSC CLASS DRIVER
*********************************************************************************************************
*/

USBD_CLASS_DRV USBD_MSC_Drv = {
    USBD_MSC_Conn,
    USBD_MSC_Disconn,
    DEF_NULL,                                                   /* MSC does NOT use alternate IF(s).                    */
    USBD_MSC_EP_StateUpdate,
    DEF_NULL,                                                   /* MSC does NOT use IF functional desc.                 */
    DEF_NULL,
    DEF_NULL,                                                   /* MSC does NOT use EP functional desc.                 */
    DEF_NULL,
    DEF_NULL,                                                   /* MSC does NOT handle std req with IF recipient.       */
    USBD_MSC_ClassReq,
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
**********************************************************************************************************
*                                               USBD_MSC_Init()
*
* Description : Initialize internal structures and variables used by the Mass Storage Class
*               Bulk Only Transport.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE    MSC class successfully initialized.
*
* Return(s)   : None.
*
* Note(s)     : None.
**********************************************************************************************************
*/

void  USBD_MSC_Init (USBD_ERR  *p_err)
{
    CPU_INT08U      ix;
    USBD_MSC_CTRL  *p_ctrl;
    USBD_MSC_COMM  *p_comm;
    LIB_ERR         err_lib;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

    for (ix = 0u; ix < USBD_MSC_CFG_MAX_NBR_DEV; ix++) {        /* Init MSC class struct.                               */
        p_ctrl                         = &USBD_MSCCtrlTbl[ix];
        p_ctrl->State                  =  USBD_MSC_STATE_NONE;
        p_ctrl->CommPtr                = (USBD_MSC_COMM *)0;
        p_ctrl->MaxLun                 = (CPU_INT08U     )0;
        p_ctrl->USBD_MSC_SCSI_Data_Len = (CPU_INT08U     )0;
        p_ctrl->USBD_MSC_SCSI_Data_Dir = (CPU_INT08U     )0;

        p_ctrl->CBW_BufPtr = (CPU_INT08U *)Mem_HeapAlloc(              USBD_MSC_LEN_CBW,
                                                                       USBD_CFG_BUF_ALIGN_OCTETS,
                                                         (CPU_SIZE_T *)DEF_NULL,
                                                                      &err_lib);
        if (err_lib != LIB_MEM_ERR_NONE) {
           *p_err = USBD_ERR_ALLOC;
            return;
        }

        Mem_Clr((void *)p_ctrl->CBW_BufPtr,
                        USBD_MSC_LEN_CBW);

        p_ctrl->CSW_BufPtr = (CPU_INT08U *)Mem_HeapAlloc(              USBD_MSC_LEN_CSW,
                                                                       USBD_CFG_BUF_ALIGN_OCTETS,
                                                         (CPU_SIZE_T *)DEF_NULL,
                                                                      &err_lib);
        if (err_lib != LIB_MEM_ERR_NONE) {
           *p_err = USBD_ERR_ALLOC;
            return;
        }

        Mem_Clr((void *)p_ctrl->CSW_BufPtr,
                        USBD_MSC_LEN_CSW);

        p_ctrl->DataBufPtr = (CPU_INT08U *)Mem_HeapAlloc(              USBD_MSC_CFG_DATA_LEN,
                                                                       USBD_CFG_BUF_ALIGN_OCTETS,
                                                         (CPU_SIZE_T *)DEF_NULL,
                                                                      &err_lib);
        if (err_lib != LIB_MEM_ERR_NONE) {
           *p_err = USBD_ERR_ALLOC;
            return;
        }

        Mem_Clr((void *)p_ctrl->DataBufPtr,
                        USBD_MSC_CFG_DATA_LEN);

        p_ctrl->CtrlStatusBufPtr = (CPU_INT08U *)Mem_HeapAlloc(               sizeof(CPU_ADDR),
                                                                              USBD_CFG_BUF_ALIGN_OCTETS,
                                                               (CPU_SIZE_T  *)DEF_NULL,
                                                                             &err_lib);
        if (err_lib != LIB_MEM_ERR_NONE) {
           *p_err = USBD_ERR_ALLOC;
            return;
        }
    }

    for (ix = 0u; ix < USBD_MSC_COM_NBR_MAX; ix++) {            /* Init test class EP tbl.                              */
        p_comm                             = &USBD_MSCCommTbl[ix];
        p_comm->DataBulkInEpAddr           =  USBD_EP_ADDR_NONE;
        p_comm->DataBulkOutEpAddr          =  USBD_EP_ADDR_NONE;
        p_comm->CtrlPtr                    = (USBD_MSC_CTRL *)0;

        p_comm->CBW.dCBWSignature          = (CPU_INT32U )0;
        p_comm->CBW.dCBWTag                = (CPU_INT32U )0;
        p_comm->CBW.dCBWDataTransferLength = (CPU_INT32U )0;
        p_comm->CBW.bmCBWFlags             = (CPU_INT08U )0;
        p_comm->CBW.bCBWLUN                = (CPU_INT08U )0;
        p_comm->CBW.bCBWCBLength           = (CPU_INT08U )0;

        Mem_Clr((void     *)       p_comm->CBW.CBWCB,
                (CPU_SIZE_T)sizeof(p_comm->CBW.CBWCB));

        p_comm->CSW.dCSWSignature          = (CPU_INT32U )0;
        p_comm->CSW.dCSWTag                = (CPU_INT32U )0;
        p_comm->CSW.dCSWDataResidue        = (CPU_INT32U )0;
        p_comm->CSW.bCSWStatus             = (CPU_INT32U )0;

        p_comm->Stall                      = (CPU_BOOLEAN)0;
        p_comm->BytesToXfer                = (CPU_INT32U )0;
        p_comm->SCSIWrBufPtr               = (void      *)0;
        p_comm->SCSIWrBuflen               = (CPU_INT32U )0;
    }

    USBD_MSCCtrlNbrNext = 0u;
    USBD_MSCCommNbrNext = 0u;

    USBD_SCSI_Init(p_err);                                      /* Init SCSI layer.                                     */
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    USBD_MSC_OS_Init(p_err);                                    /* Init MSC OS layer.                                   */
    if (*p_err != USBD_ERR_NONE) {
        return;
    }
}


/*
*********************************************************************************************************
*                                          USBD_MSC_Add()
*
* Description : Add a new instance of the Mass Storage Class.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE    Vendor class instance successfully added.
*
*                               USBD_ERR_ALLOC   No more class instance structure available.
*
* Return(s)   : Class instance number, if NO error(s).
*
*               USBD_CLASS_NBR_NONE,   otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  USBD_MSC_Add (USBD_ERR  *p_err)
{
    CPU_INT08U  msc_nbr;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    CPU_CRITICAL_ENTER();
    msc_nbr = USBD_MSCCtrlNbrNext;                              /* Alloc new MSC class.                                */

    if (msc_nbr >= USBD_MSC_CFG_MAX_NBR_DEV) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_MSC_INSTANCE_ALLOC;
        return (USBD_CLASS_NBR_NONE);
    }

    USBD_MSCCtrlNbrNext++;
    CPU_CRITICAL_EXIT();

   *p_err = USBD_ERR_NONE;
    return (msc_nbr);
}


/*
*********************************************************************************************************
*                                           USBD_MSC_CfgAdd()
*
* Description : Add an existing MSC instance to the specified configuration and device.
*
* Argument(s) : class_nbr   MSC instance number.
*
*               dev_nbr     Device number.
*
*               cfg_nbr     Configuration index to add existing MSC interface to.
*
*               p_err       Pointer to variable that will receive the return error code from this function:

*                               USBD_ERR_NONE                   MSC class instance successfully added.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'class_nbr'/
*                                                                   'dev_nbr'/'intr_en'/'interval'.
*                               USBD_ERR_ALLOC                  No more class communication structure available.

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
* Return(s)   : DEF_YES, if MSC instance added to USB device configuration successfully.
*
*               DEF_NO,  otherwise
*
* Note(s)     : (1) USBD_MSC_CfgAdd() basically adds an Interface descriptor and its associated Endpoint
*                   descriptor(s) to the Configuration descriptor. One call to USBD_MSC_CfgAdd() builds
*                   the Configuration descriptor corresponding to a MSC device with the following format:
*
*                   Configuration Descriptor
*                   |-- Interface Descriptor (MSC)
*                   |-- Endpoint Descriptor (Bulk OUT)
*                   |-- Endpoint Descriptor (Bulk IN)
*
*                   If USBD_MSC_CfgAdd() is called several times from the application, it allows to create
*                   multiple instances and multiple configurations. For instance, the following architecture
*                   could be created for an high-speed device:
*
*                   High-speed
*                   |-- Configuration 0
*                   |-- Interface 0 (MSC 0)
*                   |-- Configuration 1
*                   |-- Interface 0 (MSC 0)
*                   |-- Interface 1 (MSC 1)
*
*                   In that example, there are two instances of MSC: 'MSC 0' and 'MSC 1', and two possible
*                   configurations for the device: 'Configuration 0' and 'Configuration 1'. 'Configuration 1'
*                   is composed of two interfaces. Each class instance has an association with one of the
*                   interfaces. If 'Configuration 1' is activated by the host, it allows the host to access
*                   two different functionalities offered by the device.
*********************************************************************************************************
*/

CPU_BOOLEAN  USBD_MSC_CfgAdd (CPU_INT08U   class_nbr,
                              CPU_INT08U   dev_nbr,
                              CPU_INT08U   cfg_nbr,
                              USBD_ERR    *p_err)
{
           USBD_MSC_CTRL  *p_ctrl;
           USBD_MSC_COMM  *p_comm;
           USBD_DEV_CFG   *p_dev_cfg;
    const  CPU_CHAR       *p_str;
           CPU_INT16U      str_len;
           CPU_INT08U      if_nbr;
           CPU_INT08U      ep_addr;
           CPU_INT16U      comm_nbr;
           CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(DEF_NO);
    }
#endif

    if (class_nbr >= USBD_MSCCtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (DEF_NO);
    }

    p_dev_cfg = USBD_DevCfgGet(dev_nbr, p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (DEF_NO);
    }

    if (p_dev_cfg->SerialNbrStrPtr != (CPU_CHAR *)0) {
        p_str   = &p_dev_cfg->SerialNbrStrPtr[0];
        str_len = 0u;
        while ((*p_str   != (      CPU_CHAR  )'\0') &&          /* Chk for NULL chars ...                               */
               ( p_str   != (const CPU_CHAR *)  0 )) {          /* ...  or NULL ptr found.                              */
            if ((ASCII_IsDigHex(*p_str) == DEF_FALSE) ||        /* Serial nbr must be a hex string.                     */
               ((ASCII_IsAlpha(*p_str)  == DEF_TRUE) &&         /* Make sure that if A-F values are present they ...    */
                (ASCII_IsLower(*p_str)  == DEF_TRUE))) {        /* ... are lower-case.                                  */
               *p_err = USBD_ERR_INVALID_ARG;
                return (DEF_NO);
            }
            p_str++;
            str_len++;
        }

        if (str_len < USBD_MSC_DEV_STR_LEN) {
           *p_err = USBD_ERR_INVALID_ARG;
            return (DEF_NO);
        }
    } else {
       *p_err = USBD_ERR_INVALID_ARG;
        return (DEF_NO);
    }

    p_ctrl = &USBD_MSCCtrlTbl[class_nbr];

    CPU_CRITICAL_ENTER();
    p_ctrl->DevNbr = dev_nbr;

    comm_nbr = USBD_MSCCommNbrNext;
    if (comm_nbr >= USBD_MSC_COM_NBR_MAX) {
        USBD_MSCCtrlNbrNext--;
         CPU_CRITICAL_EXIT();
        *p_err = USBD_ERR_MSC_INSTANCE_ALLOC;
        return (DEF_NO);
    }

    USBD_MSCCommNbrNext++;
    CPU_CRITICAL_EXIT();

    p_comm = &USBD_MSCCommTbl[comm_nbr];

    if_nbr = USBD_IF_Add (        dev_nbr,                      /* Add MSC IF desc to cfg desc.                         */
                                  cfg_nbr,
                                 &USBD_MSC_Drv,
                          (void *)p_comm,
                          (void *)0,
                                  USBD_CLASS_CODE_MASS_STORAGE,
                                  USBD_MSC_SUBCLASS_CODE_SCSI,
                                  USBD_MSC_PROTOCOL_CODE_BULK_ONLY,
                                  "USB Mass Storage Interface",
                                  p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (DEF_NO);
    }

    ep_addr = USBD_BulkAdd (      dev_nbr,                     /* Add bulk-IN EP desc.                                 */
                                  cfg_nbr,
                                  if_nbr,
                                  0u,
                                  DEF_YES,
                                  0u,
                                  p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (DEF_NO);
    }

    p_comm->DataBulkInEpAddr = ep_addr;                         /* Store bulk-IN EP address.                            */

    ep_addr = USBD_BulkAdd (      dev_nbr,                      /* Add bulk-OUT EP desc.                                */
                                  cfg_nbr,
                                  if_nbr,
                                  0u,
                                  DEF_NO,
                                  0u,
                                  p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (DEF_NO);
    }

    p_comm->DataBulkOutEpAddr = ep_addr;                        /* Store bulk-OUT EP address.                           */

    CPU_CRITICAL_ENTER();
    p_ctrl->State   =  USBD_MSC_STATE_INIT;                     /* Set class instance to init state.                    */
    p_ctrl->DevNbr  =  dev_nbr;
    p_ctrl->CommPtr = (USBD_MSC_COMM *)0;
    CPU_CRITICAL_EXIT();

    p_comm->CtrlPtr = p_ctrl;
   *p_err           = USBD_ERR_NONE;

    return (DEF_YES);
}


/*
*********************************************************************************************************
*                                          USBD_MSC_LunAdd()
*
* Description : Add a logical unit number to the MSC interface.
*
* Argument(s) : p_store_name    Pointer to the Logical unit driver.
*
*               class_nbr       MSC instance number.
*
*               p_vend_id       Pointer to string containing vendor id.
*
*               p_prod_id       Pointer to string containing product id.
*
*               prod_rev_level  Product revision level.
*
*               rd_only         Boolean specifying if logical unit is read only or not.
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                               USBD_ERR_NONE                   LUN added successful.
*                               USBD_ERR_INVALID_ARGS           Invalid Arguments.
*                               USBD_ERR_MSC_MAX_LUN_EXCEED     Maximum number of LUNs reached.

*                                                               ---------- RETURNED BY USBD_SCSI_Init() : -------
*                               USBD_ERR_SCSI_LU_NOTRDY         Disk init failed.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_MSC_LunAdd (const  CPU_CHAR    *p_store_name,
                              CPU_INT08U   class_nbr,
                              CPU_CHAR    *p_vend_id,
                              CPU_CHAR    *p_prod_id,
                              CPU_INT32U   prod_rev_level,
                              CPU_BOOLEAN  rd_only,
                              USBD_ERR    *p_err)
{
    CPU_INT08U      max_lun;
    USBD_MSC_CTRL  *p_ctrl;
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    CPU_SIZE_T      str_len;


                                                                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }

    if (p_store_name == (void*)0) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }

    str_len = Str_Len(p_vend_id);
    if (str_len > USBD_MSC_DEV_MAX_VEND_ID_LEN) {
       *p_err = USBD_ERR_MSC_MAX_VEN_ID_LEN_EXCEED;
        return;
    }

    str_len = Str_Len(p_prod_id);
    if (str_len > USBD_MSC_DEV_MAX_PROD_ID_LEN) {
       *p_err = USBD_ERR_MSC_MAX_PROD_ID_LEN_EXCEED;
        return;
    }
#endif

    if (class_nbr >= USBD_MSCCtrlNbrNext) {
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }

    p_ctrl = &USBD_MSCCtrlTbl[class_nbr];

    max_lun = p_ctrl->MaxLun;
    if (max_lun >= USBD_MSC_CFG_MAX_LUN) {
       *p_err = USBD_ERR_MSC_MAX_LUN_EXCEED;
        return;
    }

    USBD_MSC_LunClr(&p_ctrl->Lun[max_lun]);                     /* Init LUN struct.                                     */

    p_ctrl->Lun[max_lun].LunNbr    =         max_lun;
    p_ctrl->Lun[max_lun].LunArgPtr = (void *)p_store_name;

   (void)Str_Copy_N((CPU_CHAR *) &(p_ctrl->Lun[max_lun].LunInfo.VendorId[0]),
                                   p_vend_id,
                                   USBD_MSC_DEV_MAX_VEND_ID_LEN);

   (void)Str_Copy_N((CPU_CHAR *) &(p_ctrl->Lun[max_lun].LunInfo.ProdId[0]),
                                   p_prod_id,
                                   USBD_MSC_DEV_MAX_PROD_ID_LEN);

    p_ctrl->Lun[max_lun].LunInfo.ProdRevisionLevel = prod_rev_level;
    p_ctrl->Lun[max_lun].LunInfo.ReadOnly          = rd_only;

    USBD_SCSI_LunAdd(p_ctrl->Lun[max_lun].LunNbr,
                     p_ctrl->Lun[max_lun].LunArgPtr,
                     p_err);

    if (*p_err == USBD_ERR_NONE) {
        p_ctrl->MaxLun++;
    }
}


/*
*********************************************************************************************************
*                                          USBD_MSC_IsConn()
*
* Description : Get the MSC connection state of the device.
*
* Argument(s) : class_nbr         MSC instance number.
*
* Return(s)   : DEF_YES, if MSC class is connected.
*
*               DEF_NO,  otherwise.
*
* Note(s)     : None.
*********************************************************************************************************
*/

CPU_BOOLEAN  USBD_MSC_IsConn (CPU_INT08U  class_nbr)
{
    USBD_MSC_CTRL   *p_ctrl;
    USBD_DEV_STATE   state;
    USBD_ERR         err;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (class_nbr >= USBD_MSCCtrlNbrNext) {                    /* Check MSC instance nbr.                               */
        return (DEF_NO);
    }
#endif

    p_ctrl = &USBD_MSCCtrlTbl[class_nbr];

    if (p_ctrl->CommPtr == (USBD_MSC_COMM *)0) {
        return (DEF_NO);
    }

    state = USBD_DevStateGet(p_ctrl->DevNbr, &err);             /* Get dev state.                                       */

    if ((err           == USBD_ERR_NONE            ) &&         /* Return true if dev state is cfg & MSC state is cfg.  */
        (state         == USBD_DEV_STATE_CONFIGURED) &&
        (p_ctrl->State == USBD_MSC_STATE_CFG       )) {
        return (DEF_YES);
    } else {
        return (DEF_NO);
    }
}


/*
**********************************************************************************************************
*                                            USBD_MSC_TaskHandler()
*
* Description : This function is used to handle MSC transfers.
*
* Argument(s) : class_nbr   MSC instance number.
*
* Return(s)   : none.
*
* Note(s)     : none.
**********************************************************************************************************
*/

void  USBD_MSC_TaskHandler (CPU_INT08U  class_nbr)
{
    USBD_ERR              err;
    USBD_ERR              os_err;
    USBD_MSC_CTRL        *p_ctrl;
    USBD_MSC_COMM        *p_comm;
    CPU_BOOLEAN           conn;
    USBD_MSC_COMM_STATE   comm_state;


    p_ctrl = &USBD_MSCCtrlTbl[class_nbr];

    while (DEF_TRUE) {

        conn = USBD_MSC_IsConn(class_nbr);
        if (conn != DEF_YES) {
                                                                /* Wait till MSC state and dev state is connected.      */
            USBD_MSC_OS_EnumSignalPend((CPU_INT16U)0,
                                                  &os_err);
        }

        comm_state = USBD_MSC_CommStateGet(class_nbr);          /* Get MSC comm state.                                  */
        p_comm     = p_ctrl->CommPtr;

        if (p_comm != (USBD_MSC_COMM *)0) {
            switch(comm_state) {
                case USBD_MSC_COMM_STATE_CBW:                   /* ---------------- RECEIVE CBW STATE ----------------- */
                     USBD_MSC_RxCBW(p_ctrl,
                                    p_comm,
                                   &err);
                     break;


                case USBD_MSC_COMM_STATE_DATA:                  /* --------------- DATA TRANSPORT STATE --------------- */
                     USBD_MSC_CMD_Process(p_ctrl,
                                          p_comm);
                     break;


                case USBD_MSC_COMM_STATE_CSW:                   /* ---------------- TRAMSIT CSW STATE ----------------- */
                     USBD_MSC_TxCSW(p_ctrl,
                                    p_comm,
                                   &err);
                     break;


                case USBD_MSC_COMM_STATE_RESET_RECOVERY:        /* ----------------RESET RECOVERY STATE --------------- */
                case USBD_MSC_COMM_STATE_RESET_RECOVERY_BULK_IN_STALL:
                case USBD_MSC_COMM_STATE_RESET_RECOVERY_BULK_OUT_STALL:
                                                                /* Wait on sem for Reset Recovery to complete.          */
                     USBD_MSC_OS_CommSignalPend(            class_nbr,
                                                (CPU_INT16U)0,
                                                           &os_err);
                     p_comm->NextCommState = USBD_MSC_COMM_STATE_CBW;

                     break;


                case USBD_MSC_COMM_STATE_BULK_IN_STALL:         /* --------------- BULK-IN STALL STATE ---------------- */
                                                                /* Wait on sem for clear bulk-IN stall to complete.     */
                     USBD_MSC_OS_CommSignalPend(           class_nbr,
                                               (CPU_INT16U)0,
                                                          &os_err);
                     p_comm->NextCommState = USBD_MSC_COMM_STATE_CSW;

                     break;


                case USBD_MSC_COMM_STATE_BULK_OUT_STALL:        /* --------------- BULK-OUT STALL STATE -------------- */
                                                                /* Wait on sem for clear bulk-OUT stall to complete.   */
                     USBD_MSC_OS_CommSignalPend(            class_nbr,
                                                (CPU_INT16U)0,
                                                           &os_err);
                     p_comm->NextCommState = USBD_MSC_COMM_STATE_CSW;
                     break;


                case USBD_MSC_COMM_STATE_NONE:
                default:
                     break;
            }
        }
    }
}


/*
**********************************************************************************************************
**********************************************************************************************************
*                                            LOCAL FUNCTIONS
**********************************************************************************************************
**********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         USBD_MSC_Conn()
*
* Description : Notify class that configuration is active.
*
* Argument(s) : dev_nbr         Device number.
*
*               cfg_nbr         Configuration index to add the interface to.
*
*               p_if_class_arg  Pointer to class argument.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  USBD_MSC_Conn (CPU_INT08U   dev_nbr,
                             CPU_INT08U   cfg_nbr,
                             void        *p_if_class_arg)
{
    USBD_MSC_COMM  *p_comm;
    USBD_ERR        os_err;
    CPU_INT08U      lun;
    CPU_SR_ALLOC();


    (void)dev_nbr;
    (void)cfg_nbr;
    p_comm  = (USBD_MSC_COMM *)p_if_class_arg;

    CPU_CRITICAL_ENTER();
    p_comm->CtrlPtr->CommPtr = p_comm;
    p_comm->CtrlPtr->State   = USBD_MSC_STATE_CFG;              /* Set initial MSC state to cfg.                        */
    p_comm->NextCommState    = USBD_MSC_COMM_STATE_CBW;         /* Set initial MSC comm state to rx CBW.                */
    CPU_CRITICAL_EXIT();

    for (lun = 0 ; lun < p_comm->CtrlPtr->MaxLun; lun++){       /* Perform some SCSI operations on each logical unit.   */

        USBD_SCSI_Conn(&p_comm->CtrlPtr->Lun[lun]);
    }

    USBD_MSC_OS_EnumSignalPost(&os_err);
    USBD_DBG_MSC_MSG("MSC: Conn");
}


/*
*********************************************************************************************************
*                                       USBD_MSC_Disconn()
*
* Description : Notify class that configuration is not active.
*
* Argument(s) : dev_nbr         Device number.
*
*               cfg_nbr         Configuration index to add the interface.
*
*               p_if_class_arg  Pointer to class argument.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  USBD_MSC_Disconn (CPU_INT08U   dev_nbr,
                                CPU_INT08U   cfg_nbr,
                                void        *p_if_class_arg)
{
    USBD_MSC_COMM        *p_comm;
    USBD_MSC_COMM_STATE   comm_state;
    CPU_BOOLEAN           post_signal;
    USBD_ERR              os_err;
    CPU_INT08U            lun_ix;
    USBD_ERR              err;
    CPU_SR_ALLOC();


    (void)dev_nbr;
    (void)cfg_nbr;
    p_comm  = (USBD_MSC_COMM *)p_if_class_arg;

    CPU_CRITICAL_ENTER();
    comm_state = p_comm->NextCommState;
    switch (comm_state){
        case USBD_MSC_COMM_STATE_RESET_RECOVERY:
        case USBD_MSC_COMM_STATE_RESET_RECOVERY_BULK_IN_STALL:
        case USBD_MSC_COMM_STATE_RESET_RECOVERY_BULK_OUT_STALL:
        case USBD_MSC_COMM_STATE_BULK_IN_STALL:
        case USBD_MSC_COMM_STATE_BULK_OUT_STALL:
             post_signal = DEF_TRUE;
             break;


        case USBD_MSC_COMM_STATE_NONE:
        case USBD_MSC_COMM_STATE_CBW:
        case USBD_MSC_COMM_STATE_DATA:
        case USBD_MSC_COMM_STATE_CSW:
        default:
             post_signal = DEF_FALSE;
             break;
    }
    p_comm->CtrlPtr->CommPtr = (USBD_MSC_COMM *)0;
    p_comm->CtrlPtr->State   =  USBD_MSC_STATE_INIT;            /* Set MSC state to init.                               */
    p_comm->NextCommState    =  USBD_MSC_COMM_STATE_NONE;       /* Set MSC comm state to none.                          */
    CPU_CRITICAL_EXIT();

    if (post_signal == DEF_TRUE) {
         USBD_MSC_OS_CommSignalPost(dev_nbr, &os_err);          /* Post sem to notify waiting task if comm ...          */
    }                                                           /* ... is in reset recovery and bulk-IN or bulk-OUT ... */
                                                                /* ... stall states.                                    */
                                                                /* Unlock each logical unit added to configuration      */
    for (lun_ix = 0 ; lun_ix < p_comm->CtrlPtr->MaxLun; lun_ix++){
        USBD_SCSI_Unlock(&p_comm->CtrlPtr->Lun[lun_ix], &err);
    }

    USBD_DBG_MSC_MSG("MSC: Disconn");
}


/*
*********************************************************************************************************
*                                      USBD_MSC_EP_StateUpdate()
*
* Description : Notify class that endpoint state has been updated.
*
* Argument(s) : dev_nbr             Device number.
*
*               cfg_nbr             Configuration ix to add the interface.
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
* Return(s)   : None.
*
* Note(s)     : (1) For Reset Recovery, the host shall issue:
*
*                   (a) a Bulk-Only Mass Storage Reset
*                   (b) a Clear Feature HALT to the Bulk-In endpoint or Bulk-Out endpoint.
*                   (c) a Clear Feature HALT to the complement Bulk-Out endpoint or Bulk-In endpoint.
*********************************************************************************************************
*/

static  void  USBD_MSC_EP_StateUpdate (CPU_INT08U   dev_nbr,
                                       CPU_INT08U   cfg_nbr,
                                       CPU_INT08U   if_nbr,
                                       CPU_INT08U   if_alt_nbr,
                                       CPU_INT08U   ep_addr,
                                       void        *p_if_class_arg,
                                       void        *p_if_alt_class_arg)
 {
    USBD_MSC_COMM  *p_comm;
    CPU_BOOLEAN     ep_is_stall;
    CPU_BOOLEAN     ep_in_is_stall;
    CPU_BOOLEAN     ep_out_is_stall;
    USBD_ERR        err;
    USBD_ERR        os_err;


    (void)cfg_nbr;
    (void)if_nbr;
    (void)if_alt_nbr;
    (void)p_if_alt_class_arg;

    p_comm     = (USBD_MSC_COMM *)p_if_class_arg;
    err        = USBD_ERR_NONE;


    switch (p_comm->NextCommState) {

        case USBD_MSC_COMM_STATE_BULK_IN_STALL:                 /* ---------------- BULK-IN STALL STATE --------------- */
             if (ep_addr == p_comm->DataBulkInEpAddr) {         /* Verify correct EP addr.                              */
                 ep_is_stall = USBD_EP_IsStalled(dev_nbr, ep_addr, &err);
                 if (ep_is_stall == DEF_FALSE) {                /* Verify that EP is unstalled.                         */
                           USBD_DBG_MSC_MSG("MSC: UpdateEP Bulk IN Stall, Signal");
                                                                /* Post sem to notify waiting task.                     */
                           USBD_MSC_OS_CommSignalPost (dev_nbr, &os_err);

                 } else {
                     USBD_DBG_MSC_MSG("MSC: UpdateEp Bulk In Stall, EP not stalled");
                 }
             } else {
                 USBD_DBG_MSC_MSG("MSC: UpdateEP Bulk IN Stall, invalid endpoint");
             }
             break;


        case USBD_MSC_COMM_STATE_BULK_OUT_STALL:                /* --------------- BULK-OUT STALL STATE --------------- */
             if (ep_addr == p_comm->DataBulkOutEpAddr) {        /* Verify correct EP addr.                              */
                 ep_is_stall = USBD_EP_IsStalled(dev_nbr, ep_addr, &err);
                 if (ep_is_stall == DEF_FALSE){                 /* Verify that EP is unstalled.                         */
                         USBD_DBG_MSC_MSG("MSC: UpdateEP Bulk OUT Stall, Signal");
                                                                /* Post sem to notify waiting task.                     */
                         USBD_MSC_OS_CommSignalPost (dev_nbr, &os_err);
                 } else {
                     USBD_DBG_MSC_MSG("MSC: UpdateEP Bulk OUT Stall, EP not stalled");
                 }
             } else {
                 USBD_DBG_MSC_MSG("MSC: UpdateEP Bulk OUT Stall, invalid endpoint");
             }
             break;


        case USBD_MSC_COMM_STATE_CBW:                           /* -------------- RX CBW / TX CSW STATE --------------- */
        case USBD_MSC_COMM_STATE_CSW:
        case USBD_MSC_COMM_STATE_DATA:
             USBD_DBG_MSC_MSG("MSC: UpdateEP Rx CBW / Process Data / Tx CSW, skip");
             break;

        case USBD_MSC_COMM_STATE_RESET_RECOVERY_BULK_IN_STALL:  /* ---------- RESET RECOVERY BULK-IN STATE ------------ */
             if (ep_addr == p_comm->DataBulkInEpAddr) {         /* Verify correct EP addr.                              */
                 ep_is_stall = USBD_EP_IsStalled(dev_nbr, ep_addr, &err);
                 if (ep_is_stall == DEF_FALSE) {                /* Verify that EP is unstalled.                         */
                     USBD_DBG_MSC_MSG("MSC: UpdateEP Reset Recovery, EP In Cleared, Stall again");
                     USBD_EP_Stall(dev_nbr, p_comm->DataBulkInEpAddr,  DEF_SET, &err);
                 } else {
                     USBD_DBG_MSC_MSG("MSC: UpdateEP Reset Recovery, EP In Stalled");
                 }
             } else {
                 USBD_DBG_MSC_MSG("MSC: UpdateEP Reset Recovery, invalid endpoint");
             }
             p_comm->NextCommState = USBD_MSC_COMM_STATE_RESET_RECOVERY_BULK_OUT_STALL;
             break;


        case USBD_MSC_COMM_STATE_RESET_RECOVERY_BULK_OUT_STALL: /* ---------- RESET RECOVERY BULK-OUT STATE ----------- */
             if (ep_addr == p_comm->DataBulkOutEpAddr) {        /* Verify correct EP addr.                              */
                 ep_is_stall = USBD_EP_IsStalled(dev_nbr, ep_addr, &err);
                 if (ep_is_stall == DEF_FALSE) {                /* Verify that EP is unstalled.                         */
                     USBD_DBG_MSC_MSG("MSC: UpdateEP Reset Recovery, EP OUT Cleared, Stall again");
                     USBD_EP_Stall(dev_nbr, p_comm->DataBulkOutEpAddr,  DEF_SET, &err);
                 } else {
                     USBD_DBG_MSC_MSG("MSC: UpdateEP Reset Recovery, EP OUT Stalled");
                 }
             } else {
                 USBD_DBG_MSC_MSG("MSC: UpdateEP Reset Recovery, invalid endpoint");
             }
             p_comm->NextCommState = USBD_MSC_COMM_STATE_RESET_RECOVERY;
             break;


        case USBD_MSC_COMM_STATE_RESET_RECOVERY:                /* -------- RESET RECOVERY STATE (see Notes #1) ------- */
             ep_in_is_stall = USBD_EP_IsStalled(dev_nbr, p_comm->DataBulkInEpAddr, &err);
             if (err != USBD_ERR_NONE){                         /* Check stall condition of bulk-IN EP.                 */
                 USBD_DBG_MSC_MSG("MSC: UpdateEP Reset Recovery, IsStalled failed");
             }

             ep_out_is_stall = USBD_EP_IsStalled(dev_nbr, p_comm->DataBulkOutEpAddr, &err);
             if (err != USBD_ERR_NONE){                         /* Check stall condition of bulk-OUT EP.                */
                 USBD_DBG_MSC_MSG("MSC: UpdateEP Reset Recovery, IsStalled failed");
             }

             if ((ep_in_is_stall == DEF_FALSE) && (ep_out_is_stall == DEF_FALSE)){
                 USBD_DBG_MSC_MSG("MSC: UpdateEP Reset Recovery, Signal");
                 USBD_MSC_OS_CommSignalPost(dev_nbr, &os_err);
             } else {
                 USBD_DBG_MSC_MSG("MSC: UpdateEP Reset Recovery, MSC Reset, Clear Stalled");
             }
             break;


        case USBD_MSC_COMM_STATE_NONE:
        default:
             USBD_DBG_MSC_MSG("MSC: UpdateEP Invalid State, Stall IN/OUT");
                                                                /* Clr stall unexpected here.                           */
             USBD_EP_Stall(dev_nbr, p_comm->DataBulkInEpAddr,  DEF_CLR, &err);
             USBD_EP_Stall(dev_nbr, p_comm->DataBulkOutEpAddr, DEF_CLR, &err);
             break;
        }
}


/*
*********************************************************************************************************
*                                      USBD_MSC_ClassReq()
*
* Description : Process class-specific request.
*
* Argument(s) : dev_nbr         Device number.
*
*               p_setup_req     Pointer to SETUP request structure.
*
*               p_if_class_arg  Pointer to class argument provided when calling USBD_IF_Add().
*
*
* Return(s)   : DEF_YES, if class request was successful
*
*               DEF_NO,  otherwise.
*
* Note(s)     : (1) The Mass Storage Reset class request is used to reset the device and its associated
*                   interface. This request readies the device for the next CBW from the host. The host
*                   sends this request via the control endpoint to the device. The device shall preserve
*                   the value of its bulk data toggle bits and endpoint stall conditions despite the
*                   Bulk-Only Mass Storage Reset. The device shall NAK the status stage of the device
*                   request until the Bulk-Only Mass Storage Reset is complete.
*
*               (2) The Get Max LUN class request is used to determine the number of logical units supported
*                   by the device. The device shall return one byte of data that contains the maximum LUN
*                   supported by the device.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_MSC_ClassReq (       CPU_INT08U       dev_nbr,
                                        const  USBD_SETUP_REQ  *p_setup_req,
                                               void            *p_if_class_arg)
{
    CPU_INT08U      request;
    CPU_BOOLEAN     valid;
    USBD_MSC_CTRL  *p_ctrl;
    USBD_MSC_COMM  *p_comm;
    USBD_ERR        err;

    (void)dev_nbr;
    request = p_setup_req->bRequest;
    p_comm  = (USBD_MSC_COMM *)p_if_class_arg;
    p_ctrl  = p_comm->CtrlPtr;
    valid   = DEF_NO;

    switch(request) {

        case USBD_MSC_REQ_MASS_STORAGE_RESET:                   /* -------- MASS STORAGE RESET (see Notes #1) --------- */
             if ((p_setup_req->wValue  == 0) &&
                 (p_setup_req->wLength == 0)) {

                 USBD_DBG_MSC_MSG("MSC: Class Mass Storage Reset, Stall IN/OUT");
                 USBD_EP_Abort(p_comm->CtrlPtr->DevNbr, p_comm->DataBulkInEpAddr, &err);
                 if (err != USBD_ERR_NONE) {
                     USBD_DBG_MSC_MSG("MSC: Class Mass Storage Reset, EP IN Abort failed");
                 }
                 USBD_EP_Abort(p_comm->CtrlPtr->DevNbr, p_comm->DataBulkOutEpAddr, &err);
                 if (err != USBD_ERR_NONE) {
                     USBD_DBG_MSC_MSG("MSC: Class Mass Storage Reset, EP OUT Abort failed");
                 }
                 USBD_SCSI_Reset();                             /* Reset the SCSI.                                      */

                 if (err == USBD_ERR_NONE){
                     valid = DEF_YES;
                 }
             }
             break;


        case USBD_MSC_REQ_GET_MAX_LUN:                          /* ------------ GET MAX LUN (see Notes #2) ------------ */
             if ((p_setup_req->wValue  == 0) &&
                 (p_setup_req->wLength == 1)) {

                  USBD_DBG_MSC_MSG("MSC: Class Get Max Lun");
                  if (p_comm->CtrlPtr->MaxLun > 0) {
                                                                /* Store max LUN info.                                  */
                      p_ctrl->CtrlStatusBufPtr[0] = p_comm->CtrlPtr->MaxLun - 1;
                      (void)USBD_CtrlTx(         p_ctrl->DevNbr,/* Tx max LUN info through ctrl EP.                     */
                                        (void *)&p_ctrl->CtrlStatusBufPtr[0],
                                                 1u,
                                                 USBD_MSC_CTRL_REQ_TIMEOUT_mS,
                                                 DEF_NO,
                                                &err);
                      if ((err                    == USBD_ERR_NONE) &&
                          (p_comm->CtrlPtr->State == USBD_MSC_STATE_CFG)) {
                          valid = DEF_YES;
                      }
                  }
             }
             break;


        default:
             break;
    }

    return (valid);
}


/*
*********************************************************************************************************
*                                          USBD_MSC_CommStateGet()
*
* Description : Get the MSC class communication state.
*
* Argument(s) : class_nbr   MSC instance number.
*
* Return(s)   : The communication state of the MSC device.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  USBD_MSC_COMM_STATE  USBD_MSC_CommStateGet (CPU_INT08U  class_nbr)
{
    USBD_MSC_CTRL  *p_ctrl;


    if (class_nbr >=  USBD_MSCCtrlNbrNext){                     /* --------------------- CHECK ARG -------------------- */
        return (USBD_MSC_COMM_STATE_NONE);
    }

    p_ctrl = &USBD_MSCCtrlTbl[class_nbr];

    if (p_ctrl->CommPtr == (USBD_MSC_COMM *)0  ){
        return (USBD_MSC_COMM_STATE_NONE);
    }

    return (p_ctrl->CommPtr->NextCommState);
}


/*
**********************************************************************************************************
*                                        USBD_MSC_CBW_Verify()
*
* Description : Verify the CBW sent by the Host.
*
* Argument(s) : class_nbr   MSC instance number.
*
*               p_cbw_buf   Pointer to the raw cbw buffer.
*
*               cbw_len     Length of the raw cbw buffer.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Valid   CBW.
*                               USBD_ERR_MSC_INVALID_CBW    Unvalid CBW.
*
* Return(s)   : None.
*
* Note(s)     : (1) The device performs two verifications on every CBW received. First is that the CBW
*                   is valid. Second is that the CBW is meaningful.
*
*                   (a) The device shall consider a CBW valid when:
*                       (1) The CBW was received after the device had sent a CSW or after a reset.
*                       (2) The CBW is 31 (1Fh) bytes in length.
*                       (3) The dCBWSignature is equal to 43425355h.
*
*                   (b) The device shall consider a CBW meaningful when:
*                       (1) No reserve bits are set.
*                       (2) bCBWLUN contains a valid LUN supported by the device.
*                       (3) both cCBWCBLength and the content of the CBWCB are in accordance with
*                           bInterfaceSubClass
*
**********************************************************************************************************
*/

static  void  USBD_MSC_CBW_Verify (const USBD_MSC_CTRL  *p_ctrl,
                                         USBD_MSC_COMM  *p_comm,
                                         void           *p_cbw_buf,
                                         CPU_INT32U      cbw_len,
                                         USBD_ERR       *p_err)
{
   *p_err  = USBD_ERR_NONE;

    if (cbw_len != USBD_MSC_LEN_CBW) {                          /* See note #1a2.                                       */
        USBD_DBG_MSC_MSG("MSC: Verify CBW, invalid length");
       *p_err = USBD_ERR_MSC_INVALID_CBW;
    }

    USBD_MSC_CBW_Parse(&p_comm->CBW, p_cbw_buf);                /* Parse the raw buffer into CBW struct.                */

                                                                /* See note #1a3.                                       */
    if (p_comm->CBW.dCBWSignature != USBD_MSC_SIG_CBW) {
        USBD_DBG_MSC_MSG("MSC: Verify CBW, invalid signature");
       *p_err = USBD_ERR_MSC_INVALID_CBW;
    }
                                                                /* See note #2b.                                        */
    if (((p_comm->CBW.bCBWLUN      & 0xF0) >  0) ||
        ((p_comm->CBW.bCBWCBLength & 0xE0) >  0) ||
         (p_comm->CBW.bCBWLUN              >= p_ctrl->MaxLun)) {
        USBD_DBG_MSC_MSG("MSC: Verify CBW, invalid LUN/reserved bits");
       *p_err =  USBD_ERR_MSC_INVALID_CBW;
    }
}


/*
**********************************************************************************************************
*                                            USBD_MSC_RespVerify()
*
* Description : Check the data transfer conditions of host and device and set the CSW status field.
*
* Argument(s) : class_nbr       MSC instance number.
*
*               scsi_data_len   The length of the response the device intends to transfer.
*
*               scsi_data_dir   The data transfer direction.
*
*               p_err       Pointer to variable that will receive the return error code from this function :

*                               USBD_ERR_NONE               CBW satisfies one of the thirteen cases.
*                               USBD_MSC_ERR_INVALID_DIR    Mismatch between direction indicated by CBW and
*                                                               SCSI command.
*
* Return(s)   : None.
*
* Note(s)     : (1) "USB Mass Storage Class - Bulk Only Transport", Revision 1.0, Section 6.7, lists
*                   the thirteen cases of host expectation & device intent with descriptions of the
*                   appropriate action.
**********************************************************************************************************
*/

static  void  USBD_MSC_RespVerify (USBD_MSC_COMM  *p_comm,
                                   CPU_INT32U      scsi_data_len,
                                   CPU_INT08U      scsi_data_dir,
                                   USBD_ERR       *p_err)
{
    CPU_SR_ALLOC();


   *p_err = USBD_ERR_NONE;

    CPU_CRITICAL_ENTER();
    p_comm->Stall = DEF_FALSE;
                                                                /* Check for host and SCSI dirs.                        */
    if ((scsi_data_len) &&
        (p_comm->CBW.bmCBWFlags != scsi_data_dir)) {
                                                                /* Case  8: Hi <> Do || 10: Hn <> Di.                   */
        p_comm->CSW.bCSWStatus = USBD_MSC_BCSWSTATUS_PHASE_ERROR;
        CPU_CRITICAL_EXIT();

       *p_err = USBD_ERR_MSC_INVALID_DIR;
        return;
    }

    if (p_comm->CBW.dCBWDataTransferLength == 0) {              /* -------------------- Hn CASES: --------------------- */

        if (scsi_data_len == 0) {                               /* Case  1: Hn = Dn.                                    */
            p_comm->CSW.bCSWStatus = USBD_MSC_BCSWSTATUS_CMD_PASSED;

        } else {                                                /* Case  2: Hn < Di || 3: Hn < Do.                      */
            p_comm->CSW.bCSWStatus = USBD_MSC_BCSWSTATUS_PHASE_ERROR;

        }
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_NONE;
        return;
    }
                                                                /* -------------------- Hi CASES: --------------------- */
    if (p_comm->CBW.bmCBWFlags == USBD_MSC_BMCBWFLAGS_DIR_DEVICE_TO_HOST) {

        if (scsi_data_len == 0) {                               /* Case  4: Hi >  Dn.                                   */
            p_comm->CSW.bCSWStatus = USBD_MSC_BCSWSTATUS_CMD_FAILED;
            p_comm->Stall          = DEF_TRUE;
                                                                /* Case  5: Hi >  Di.                                   */
        } else if (p_comm->CBW.dCBWDataTransferLength > scsi_data_len) {
            p_comm->CSW.bCSWStatus = USBD_MSC_BCSWSTATUS_CMD_PASSED;
            p_comm->Stall          = DEF_TRUE;
                                                                /* Case  7: Hi <  Di.                                   */
        } else if (p_comm->CBW.dCBWDataTransferLength < scsi_data_len) {
            p_comm->CSW.bCSWStatus = USBD_MSC_BCSWSTATUS_PHASE_ERROR;

        } else {                                                /* Case  6: Hi == Di.                                   */
            p_comm->CSW.bCSWStatus = USBD_MSC_BCSWSTATUS_CMD_PASSED;

        }
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_NONE;
         return;
    }
                                                                /* -------------------- Ho CASES: --------------------- */
    if (p_comm->CBW.bmCBWFlags == USBD_MSC_BMCBWFLAGS_DIR_HOST_TO_DEVICE) {
                                                                /* Case 9: Ho > Dn || 11: Ho >  Do.                     */
        if (p_comm->CBW.dCBWDataTransferLength > scsi_data_len) {
            p_comm->CSW.bCSWStatus = USBD_MSC_BCSWSTATUS_CMD_PASSED;
            p_comm->Stall          = DEF_TRUE;
                                                                /* Case 13: Ho <  Do.                                   */
        } else if (p_comm->CBW.dCBWDataTransferLength < scsi_data_len) {
            p_comm->CSW.bCSWStatus = USBD_MSC_BCSWSTATUS_PHASE_ERROR;

        } else {                                                /* Case 12: Ho ==  Do.                                  */
            p_comm->CSW.bCSWStatus = USBD_MSC_BCSWSTATUS_CMD_PASSED;

        }
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_NONE;
        return;
    }
    CPU_CRITICAL_EXIT();
}


/*
**********************************************************************************************************
*                                       USBD_MSC_CMD_Process()
*
* Description : Process the CBW sent by the host.
*
* Argument(s) : class_nbr   MSC instance number.
*
* Return(s)   : None.
*
* Note(s)     : None.
**********************************************************************************************************
*/

static  void  USBD_MSC_CMD_Process (USBD_MSC_CTRL  *p_ctrl,
                                    USBD_MSC_COMM  *p_comm)
{
    USBD_ERR    err;
    USBD_ERR    stall_err;
    CPU_INT08U  lun;
    CPU_SR_ALLOC();


    lun = p_comm->CBW.bCBWLUN;

    USBD_SCSI_CmdProcess(&p_ctrl->Lun[lun],                     /* Send the CBWCB to SCSI dev.                          */
                          p_comm->CBW.CBWCB,
                         &(p_ctrl->USBD_MSC_SCSI_Data_Len),
                         &(p_ctrl->USBD_MSC_SCSI_Data_Dir),
                         &err);

    if (err == USBD_ERR_NONE) {                                 /* Verify data xfer conditions.                         */
        USBD_MSC_RespVerify(p_comm, p_ctrl->USBD_MSC_SCSI_Data_Len, p_ctrl->USBD_MSC_SCSI_Data_Dir, &err);

    } else {                                                    /* Set CSW field in preparation to be returned to host. */
        p_comm->CSW.bCSWStatus = USBD_MSC_BCSWSTATUS_CMD_FAILED;
    }

    if (err == USBD_ERR_NONE) {                                 /* SCSI command success.                                */
        p_comm->BytesToXfer = DEF_MIN(p_comm->CBW.dCBWDataTransferLength, p_ctrl->USBD_MSC_SCSI_Data_Len);
        if (p_comm->BytesToXfer > 0) {                          /* Host expects data and device has data.               */
            if (p_comm->CBW.bmCBWFlags == USBD_MSC_BMCBWFLAGS_DIR_HOST_TO_DEVICE) {
                USBD_MSC_SCSI_RxData(p_ctrl, p_comm);           /* Rx data from host on bulk-OUT.                       */

            } else {
                USBD_MSC_SCSI_TxData(p_ctrl, p_comm);           /* Tx data to host on bulk-IN.                          */
            }

        } else {
            if (p_comm->Stall) {                                /* Host expects data and but dev has NO data.           */
                if (p_comm->CBW.bmCBWFlags == USBD_MSC_BMCBWFLAGS_DIR_HOST_TO_DEVICE) {
                    CPU_CRITICAL_ENTER();
                    p_comm->NextCommState = USBD_MSC_COMM_STATE_BULK_OUT_STALL;
                    CPU_CRITICAL_EXIT();
                    USBD_EP_Stall(p_ctrl->DevNbr, p_comm->DataBulkOutEpAddr, DEF_SET, &stall_err);
                } else {                                        /* Direction from dev to the host.                      */
                    CPU_CRITICAL_ENTER();
                    p_comm->NextCommState = USBD_MSC_COMM_STATE_BULK_IN_STALL;
                    CPU_CRITICAL_EXIT();
                    USBD_EP_Stall(p_ctrl->DevNbr, p_comm->DataBulkInEpAddr, DEF_SET, &stall_err);
                }

            } else {                                            /* Host expects NO data.                                */
                CPU_CRITICAL_ENTER();
                p_comm->NextCommState = USBD_MSC_COMM_STATE_CSW;
                CPU_CRITICAL_EXIT();
            }
        }
    } else {                                                    /* SCSI cmd failed.                                     */
        if (p_comm->CBW.dCBWDataTransferLength == 0) {          /* If no data stage send CSW to host.                   */
            CPU_CRITICAL_ENTER();
            p_comm->NextCommState = USBD_MSC_COMM_STATE_CSW;
            CPU_CRITICAL_EXIT();
                                                                /* If data stage is dost to dev, ...                    */
                                                                /* ... stall OUT pipe and send CSW.                     */
        } else if (p_comm->CBW.bmCBWFlags == USBD_MSC_BMCBWFLAGS_DIR_HOST_TO_DEVICE) {
            CPU_CRITICAL_ENTER();
            p_comm->NextCommState = USBD_MSC_COMM_STATE_BULK_OUT_STALL;
            CPU_CRITICAL_EXIT();
            USBD_EP_Stall(p_ctrl->DevNbr, p_comm->DataBulkOutEpAddr, DEF_SET, &stall_err);


        } else {                                                /* If data stage is dev to host, ...                    */
                                                                /* ... stall IN pipe and wait for clear stall.          */
            CPU_CRITICAL_ENTER();
            p_comm->NextCommState = USBD_MSC_COMM_STATE_BULK_IN_STALL;
            CPU_CRITICAL_EXIT();
            USBD_EP_Stall(p_ctrl->DevNbr, p_comm->DataBulkInEpAddr, DEF_SET, &stall_err);
        }
    }

}


/*
**********************************************************************************************************
*                                             USBD_MSC_SCSI_TxData()
*
* Description : Reads data from the SCSI.
*
* Argument(s) : class_nbr   MSC instance number.
*
* Return(s)   : None.
*
* Note(s)     : None.
**********************************************************************************************************
*/

static  void  USBD_MSC_SCSI_TxData (USBD_MSC_CTRL  *p_ctrl,
                                    USBD_MSC_COMM  *p_comm)
{
    CPU_INT32U  scsi_buf_len;
    USBD_ERR    stall_err;
    CPU_SR_ALLOC();


    scsi_buf_len = DEF_MIN(p_comm->BytesToXfer, USBD_MSC_CFG_DATA_LEN);

    while (scsi_buf_len > 0) {

        USBD_MSC_SCSI_Rd(p_ctrl, p_comm);
        p_comm->BytesToXfer         -= scsi_buf_len;            /* Update remaining bytes to transmit.                  */
        p_comm->CSW.dCSWDataResidue -= scsi_buf_len;            /* Update CSW data residue field.                       */
        scsi_buf_len = DEF_MIN(p_comm->BytesToXfer, USBD_MSC_CFG_DATA_LEN);
    }
    if (p_comm->Stall == DEF_TRUE) {
        p_comm->Stall = DEF_FALSE;

        CPU_CRITICAL_ENTER();                                   /* Set the next state to bulk-IN stall.                 */
        p_comm->NextCommState = USBD_MSC_COMM_STATE_BULK_IN_STALL;
        CPU_CRITICAL_EXIT();

        USBD_EP_Stall(p_ctrl->DevNbr, p_comm->DataBulkInEpAddr, DEF_SET, &stall_err);

    } else {
        CPU_CRITICAL_ENTER();                                   /* Set the next state to tx CSW.                        */
        p_comm->NextCommState = USBD_MSC_COMM_STATE_CSW;
        CPU_CRITICAL_EXIT();
    }
}


/*
**********************************************************************************************************
*                                            USBD_MSC_SCSI_Rd()
*
* Description : Reads data from the SCSI and transmits data to the host. CSW will be transmitted
*               after the data completion stage.
*
* Argument(s) : class_nbr   MSC instance number.
*
* Return(s)   : None.
*
* Note(s)     : None.
**********************************************************************************************************
*/

static  void  USBD_MSC_SCSI_Rd (USBD_MSC_CTRL  *p_ctrl,
                                USBD_MSC_COMM  *p_comm)
{
    CPU_INT32U  scsi_ret_len;
    CPU_INT32U  scsi_buf_len;
    CPU_INT08U  lun;
    USBD_ERR    err;
    USBD_ERR    stall_err;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    scsi_buf_len = DEF_MIN(p_comm->BytesToXfer, USBD_MSC_CFG_DATA_LEN);
    lun = p_comm->CBW.bCBWLUN;
    CPU_CRITICAL_EXIT();
    USBD_SCSI_DataRd(&p_ctrl->Lun[lun],                         /* Rd data from the SCSI.                               */
                      p_comm->CBW.CBWCB[0],
                      p_ctrl->DataBufPtr,
                      scsi_buf_len,
                     &scsi_ret_len,
                     &err);
    if ((err != USBD_ERR_NONE) &&
        (err != USBD_ERR_SCSI_MORE_DATA)) {
        CPU_CRITICAL_ENTER();
        p_comm->NextCommState  = USBD_MSC_COMM_STATE_BULK_IN_STALL;
        p_comm->CSW.bCSWStatus = (CPU_INT08U)USBD_MSC_BCSWSTATUS_CMD_FAILED;
        CPU_CRITICAL_EXIT();

        USBD_EP_Stall( p_ctrl->DevNbr, p_comm->DataBulkInEpAddr, DEF_SET, &err);

    } else {
        (void)USBD_BulkTx(p_ctrl->DevNbr,                       /* Tx data to the host.                                 */
                          p_comm->DataBulkInEpAddr,
                          p_ctrl->DataBufPtr,
                          scsi_ret_len,
                          0,
                          DEF_NO,
                         &err);

        if (err != USBD_ERR_NONE) {
            CPU_CRITICAL_ENTER();                               /* Enter reset recovery state if tx err.                */
            p_comm->NextCommState = USBD_MSC_COMM_STATE_BULK_IN_STALL;
            CPU_CRITICAL_EXIT();
            USBD_EP_Stall (p_ctrl->DevNbr, p_comm->DataBulkInEpAddr, DEF_SET, &stall_err);
        }
    }
}


/*
**********************************************************************************************************
*                                            USBD_MSC_SCSI_Wr()
*
* Description : Save the buffer and buffer length and write data to the SCSI.
*
* Argument(s) : class_nbr         MSC instance number.
*
*               p_buf            Pointer to the data buffer.
*
*               xfer_len         Length of the data buffer.
*
* Return(s)   : None.
*
* Note(s)     : None.
**********************************************************************************************************
*/

static  void  USBD_MSC_SCSI_Wr (const USBD_MSC_CTRL  *p_ctrl,
                                      USBD_MSC_COMM  *p_comm,
                                      void           *p_buf,
                                      CPU_INT32U      xfer_len)
{
    USBD_ERR       err;
    USBD_ERR       stall_err;
    CPU_INT08U     lun;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    p_comm->SCSIWrBufPtr = p_buf;
    p_comm->SCSIWrBuflen = xfer_len;
    CPU_CRITICAL_EXIT();

    lun = p_comm->CBW.bCBWLUN;
    USBD_SCSI_DataWr(&p_ctrl->Lun[lun],                         /* Wr data to SCSI sto.                                 */
                      p_comm->CBW.CBWCB[0],
                      p_comm->SCSIWrBufPtr,
                      p_comm->SCSIWrBuflen,
                     &err);
    if ((err != USBD_ERR_NONE) &&
        (err != USBD_ERR_SCSI_MORE_DATA)) {
        CPU_CRITICAL_ENTER();                                   /* Enter bulk-OUT stall state.                          */
        p_comm->CSW.bCSWStatus = USBD_MSC_BCSWSTATUS_CMD_FAILED;
        p_comm->NextCommState  = USBD_MSC_COMM_STATE_BULK_OUT_STALL;
        CPU_CRITICAL_EXIT();
        USBD_DBG_MSC_MSG("MSC: SCSI Wr, Stall OUT");

        USBD_EP_Stall(p_ctrl->DevNbr, p_comm->DataBulkOutEpAddr, DEF_SET, &stall_err);
    }
}


/*
**********************************************************************************************************
*                                             USBD_MSC_SCSI_RxData()
*
* Description : It receives data from the host. After the data stage is complete, CSW is sent.
*
* Argument(s) : class_nbr   MSC instance number.
*
* Return(s)   : None.
*
* Note(s)     : None.
**********************************************************************************************************
*/

static  void  USBD_MSC_SCSI_RxData (USBD_MSC_CTRL  *p_ctrl,
                                    USBD_MSC_COMM  *p_comm)
{
    CPU_INT32U      scsi_buf_len;
    CPU_INT32U      xfer_len;
    USBD_ERR        err;
    USBD_ERR        stall_err;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    scsi_buf_len = DEF_MIN(p_comm->BytesToXfer, USBD_MSC_CFG_DATA_LEN);
    CPU_CRITICAL_EXIT();

    while (scsi_buf_len > 0){
        USBD_DBG_MSC_ARG("MSC: Rx Data Len:", scsi_buf_len);
        xfer_len = USBD_BulkRx(p_ctrl->DevNbr,                  /* Rx data from host on bulk-OUT pipe                   */
                               p_comm->DataBulkOutEpAddr,
                               p_ctrl->DataBufPtr,
                               scsi_buf_len,
                               0,
                              &err);
        if (err != USBD_ERR_NONE){
            CPU_CRITICAL_ENTER();                               /* Enter reset recovery state if err.                   */
            p_comm->NextCommState = USBD_MSC_COMM_STATE_BULK_OUT_STALL;
            CPU_CRITICAL_EXIT();

            USBD_DBG_MSC_MSG("MSC: Rx Data, Stall OUT");

            USBD_EP_Stall(p_ctrl->DevNbr, p_comm->DataBulkOutEpAddr, DEF_SET, &stall_err);
            return;

        } else {
                                                                /* Process rx data if no err.                           */
            USBD_MSC_SCSI_Wr(p_ctrl, p_comm, p_ctrl->DataBufPtr, xfer_len);
            p_comm->BytesToXfer         -= xfer_len;
            p_comm->CSW.dCSWDataResidue -= xfer_len;
            scsi_buf_len = DEF_MIN(p_comm->BytesToXfer, USBD_MSC_CFG_DATA_LEN);
        }
    }

    if (p_comm->Stall){
        CPU_CRITICAL_ENTER();
        p_comm->Stall = DEF_FALSE;                              /* Enter bulk-OUT stall state.                          */
        p_comm->NextCommState = USBD_MSC_COMM_STATE_BULK_OUT_STALL;
        CPU_CRITICAL_EXIT();

        USBD_DBG_MSC_MSG("MSC: Rx Data, Stall OUT");
        USBD_EP_Stall(p_ctrl->DevNbr, p_comm->DataBulkOutEpAddr, DEF_SET, &stall_err);
        return;
    }

    CPU_CRITICAL_ENTER();                                       /* Enter tx CSW state.                                  */
    p_comm->NextCommState = USBD_MSC_COMM_STATE_CSW;
    CPU_CRITICAL_EXIT();
}


/*
**********************************************************************************************************
*                                             USBD_MSC_TxCSW()
*
* Description : Send status CSW to the host.
*
* Argument(s) : class_nbr   MSC instance number.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE, if device sends status successfully.
*
*                                                               ------ RETURNED BY USBD_BulkTx() : ------
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_EP_INVALID_NBR         Invalid endpoint number.
*                               USBD_ERR_DEV_INVALID_STATE      Bulk transfer can ONLY be used after the
*                                                                   device is in configured state.
*                               USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*
* Return(s)   : None.
*
* Note(s)     : None.
**********************************************************************************************************
*/

static  void  USBD_MSC_TxCSW  (USBD_MSC_CTRL  *p_ctrl,
                               USBD_MSC_COMM  *p_comm,
                               USBD_ERR       *p_err)
{
    USBD_ERR  stall_err;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    p_comm->CSW.dCSWSignature = USBD_MSC_SIG_CSW;               /* Set CSW signature and rcvd tag from CBW.             */
    p_comm->CSW.dCSWTag       = p_comm->CBW.dCBWTag;

    USBD_MSC_CSW_Fmt(        &p_comm->CSW,                      /* Wr CSW to raw buf.                                   */
                     (void *) p_ctrl->CSW_BufPtr);
    CPU_CRITICAL_EXIT();
                                                                /* Tx CSW to host through bulk-IN pipe.                 */
    (void)USBD_BulkTx(p_ctrl->DevNbr,
                      p_comm->DataBulkInEpAddr,
                      p_ctrl->CSW_BufPtr,
                      USBD_MSC_LEN_CSW,
                      0,
                      DEF_NO,
                      p_err);
    if (*p_err != USBD_ERR_NONE){                               /* Enter reset recovery state.                          */
        CPU_CRITICAL_ENTER();
        p_comm->NextCommState = USBD_MSC_COMM_STATE_BULK_IN_STALL;
        CPU_CRITICAL_EXIT();

        USBD_DBG_MSC_MSG("MSC: TxCSW, Stall IN");
        USBD_EP_Stall(p_ctrl->DevNbr, p_comm->DataBulkInEpAddr, DEF_SET, &stall_err);

    } else {
        if (p_comm->CSW.bCSWStatus == USBD_MSC_BCSWSTATUS_PHASE_ERROR){
            CPU_CRITICAL_ENTER();
            p_comm->NextCommState = USBD_MSC_COMM_STATE_RESET_RECOVERY;
            CPU_CRITICAL_EXIT();
        } else {
            CPU_CRITICAL_ENTER();                               /* Enter rx CBW state.                                  */
            p_comm->NextCommState =  USBD_MSC_COMM_STATE_CBW;
            CPU_CRITICAL_EXIT();
        }
    }
}


/*
**********************************************************************************************************
*                                             USBD_MSC_RxCBW()
*
* Description : Receive CBW from the host.
*
* Argument(s) : class_nbr   MSC instance number.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*                               USBD_ERR_NONE                   CBW successfully received from host

*                                                               ------ RETURNED BY USBD_BulkRx() : ------
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_EP_INVALID_NBR         Invalid endpoint number.
*                               USBD_ERR_DEV_INVALID_STATE      Bulk transfer can ONLY be used after the
*                                                                   device is in configured state.
*                               USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*
* Return(s)   : None.
*
* Note(s)     : None.
**********************************************************************************************************
*/

static  void  USBD_MSC_RxCBW (USBD_MSC_CTRL  *p_ctrl,
                              USBD_MSC_COMM  *p_comm,
                              USBD_ERR       *p_err)
{
    USBD_ERR    err;
    USBD_ERR    stall_err;
    CPU_INT32U  xfer_len;
    CPU_SR_ALLOC();


    xfer_len = USBD_BulkRx (p_ctrl->DevNbr,                     /*Rx CBW and returns xfer_len upon success.             */
                            p_comm->DataBulkOutEpAddr,
                            p_ctrl->CBW_BufPtr,
                            USBD_MSC_LEN_CBW,
                            0,
                            p_err);

    USBD_DBG_MSC_ARG("MSC: RxCBW", xfer_len);

    switch (*p_err) {
        case USBD_ERR_NONE:
             break;

        case USBD_ERR_DEV_INVALID_NBR:
        case USBD_ERR_DEV_INVALID_STATE:
        case USBD_ERR_OS_ABORT:
        case USBD_ERR_EP_INVALID_ADDR:
        case USBD_ERR_EP_INVALID_STATE:
        case USBD_ERR_EP_INVALID_TYPE:
             CPU_CRITICAL_ENTER();
             if (p_ctrl->State == USBD_MSC_STATE_CFG){
                p_comm->NextCommState = USBD_MSC_COMM_STATE_CBW;
             }
             CPU_CRITICAL_EXIT();
             USBD_DBG_MSC_MSG("MSC: RxCBW, OS Abort");
             break;

        case USBD_ERR_OS_TIMEOUT:
             CPU_CRITICAL_ENTER();
             if (p_ctrl->State == USBD_MSC_STATE_CFG){
                p_comm->NextCommState = USBD_MSC_COMM_STATE_CBW;
             }
             CPU_CRITICAL_EXIT();
             USBD_DBG_MSC_MSG("MSC: RxCBW, OS Timeout");
             break;

        case USBD_ERR_EP_ABORT:
        case USBD_ERR_RX:
        case USBD_ERR_DRV_BUF_OVERFLOW:
            CPU_CRITICAL_ENTER();
            p_comm->NextCommState = USBD_MSC_COMM_STATE_RESET_RECOVERY_BULK_IN_STALL;
            CPU_CRITICAL_EXIT();
            USBD_DBG_MSC_MSG("MSC: RxCBW, Stall IN/OUT");
            USBD_EP_Stall(p_ctrl->DevNbr, p_comm->DataBulkInEpAddr, DEF_SET, &stall_err);
            USBD_EP_Stall(p_ctrl->DevNbr, p_comm->DataBulkOutEpAddr, DEF_SET, &stall_err);
            break;

        case USBD_ERR_OS_FAIL:
        case USBD_ERR_NULL_PTR:
        case USBD_ERR_EP_IO_PENDING:
        case USBD_ERR_EP_QUEUING:
        case USBD_ERR_DRV_INVALID_PKT:
        default:
            CPU_CRITICAL_ENTER();
            p_comm->NextCommState = USBD_MSC_COMM_STATE_BULK_OUT_STALL;
            CPU_CRITICAL_EXIT();
            USBD_DBG_MSC_MSG("MSC: RxCBW, Stall OUT");
            USBD_EP_Stall(p_ctrl->DevNbr, p_comm->DataBulkOutEpAddr, DEF_SET, &stall_err);
            break;
    }

    if (*p_err == USBD_ERR_NONE) {
                                                                /* Verify that CBW is valid and meaningful.             */
        USBD_MSC_CBW_Verify(p_ctrl, p_comm, p_ctrl->CBW_BufPtr, xfer_len, &err);
        if (err != USBD_ERR_NONE) {                             /* Enter reset recovery state.                          */
            CPU_CRITICAL_ENTER();
            p_comm->NextCommState = USBD_MSC_COMM_STATE_RESET_RECOVERY_BULK_IN_STALL;
            CPU_CRITICAL_EXIT();

            USBD_DBG_MSC_MSG("MSC: RxCBW, Stall IN/OUT");

            USBD_EP_Stall(p_ctrl->DevNbr, p_comm->DataBulkInEpAddr, DEF_SET, &stall_err);
            USBD_EP_Stall(p_ctrl->DevNbr, p_comm->DataBulkOutEpAddr, DEF_SET, &stall_err);
            return;

        }
        CPU_CRITICAL_ENTER();
                                                                /* Host expected transfer length.                       */
        p_comm->CSW.dCSWDataResidue = p_comm->CBW.dCBWDataTransferLength;
        p_comm->BytesToXfer         = 0;
        p_comm->NextCommState       = USBD_MSC_COMM_STATE_DATA; /* Enter data transport state.                          */
        CPU_CRITICAL_EXIT();
    }
}


/*
*********************************************************************************************************
*                                          USBD_MSC_LunClr()
*
* Description : Initialize all logical unit fields.
*
* Argument(s) : p_lun       Pointer to Logical Unit information.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  USBD_MSC_LunClr  (USBD_MSC_LUN_CTRL  *p_lun)
{
    p_lun->LunNbr                    = 0;
    p_lun->LunInfo.ProdRevisionLevel = 0;
    p_lun->LunInfo.ReadOnly          = DEF_FALSE;
    p_lun->NbrBlocks                 = 0;
    p_lun->BlockSize                 = 0;

    Mem_Clr((void     *)p_lun->LunInfo.VendorId,
            (CPU_SIZE_T)8);
    Mem_Clr((void     *)p_lun->LunInfo.ProdId,
            (CPU_SIZE_T)16);

    p_lun->LunArgPtr = (void *)0;
}



/*
*********************************************************************************************************
*                                        USBD_MSC_CBW_Parse()
*
* Description : Parse CBW into CBW structure.
*
* Argument(s) : p_cbw       Variable that will hold the CBW parsed in this function.
*
*               p_buf_src   Pointer to buffer that holds CBW.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  USBD_MSC_CBW_Parse (USBD_MSC_CBW  *p_cbw,
                                  void          *p_buf_src)
{
    CPU_INT08U  *p_buf_src_08;


    p_buf_src_08 = (CPU_INT08U *)p_buf_src;

    p_cbw->dCBWSignature          = MEM_VAL_GET_INT32U_LITTLE((void *)((CPU_INT08U *)p_buf_src_08 +  0u));
    p_cbw->dCBWTag                = MEM_VAL_GET_INT32U_LITTLE((void *)((CPU_INT08U *)p_buf_src_08 +  4u));
    p_cbw->dCBWDataTransferLength = MEM_VAL_GET_INT32U_LITTLE((void *)((CPU_INT08U *)p_buf_src_08 +  8u));
    p_cbw->bmCBWFlags             = p_buf_src_08[12];
    p_cbw->bCBWLUN                = p_buf_src_08[13];
    p_cbw->bCBWCBLength           = p_buf_src_08[14];

    Mem_Copy((void     *)&p_cbw->CBWCB[0],
             (void     *)&p_buf_src_08[15],
             (CPU_SIZE_T) 16);
}


/*
*********************************************************************************************************
*                                         USBD_MSC_CSW_Fmt()
*
* Description : Format CSW from CSW structure.
*
* Argument(s) : p_csw       Variable holds the CSW information.
*
*               p_buf_dest  Pointer to buffer that will hold CSW.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  USBD_MSC_CSW_Fmt (const  USBD_MSC_CSW  *p_csw,
                                       void          *p_buf_dest)
{
    CPU_INT08U  *p_buf_dest_08;


    p_buf_dest_08 = (CPU_INT08U *)p_buf_dest;

    MEM_VAL_SET_INT32U_LITTLE((void *)((CPU_INT08U *)p_buf_dest_08 + 0u), p_csw->dCSWSignature);
    MEM_VAL_SET_INT32U_LITTLE((void *)((CPU_INT08U *)p_buf_dest_08 + 4u), p_csw->dCSWTag);
    MEM_VAL_SET_INT32U_LITTLE((void *)((CPU_INT08U *)p_buf_dest_08 + 8u), p_csw->dCSWDataResidue);

    p_buf_dest_08[12] = p_csw->bCSWStatus;
}
