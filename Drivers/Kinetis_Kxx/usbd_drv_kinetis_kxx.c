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
*                                          USB DEVICE DRIVER
*
*                                              KINETIS
*
* Filename : usbd_drv_kinetis.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)  : (1) You can find specific information about this driver at:
*                https://doc.micrium.com/display/USBDDRV/Kinetis_Kxx
*
*            (2) With an appropriate BSP, this device driver will support the USBOTG device module on
*                the following Freescale Kinetis series: K20, K40, K50, K60, K70
*
*            (3) K70 series has one additional register (USBFRMADJUST) in the USB controller.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#include  "../../Source/usbd_core.h"
#include  "usbd_drv_kinetis_kxx.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  DEF_MCU_CTRL                               0u          /* MCU has exclusive access to the BD                   */
#define  DEF_USBFS_CTRL                             1u          /* UsbFs has exclusive access to the BD                 */

                                                                /* ------------- USB CONTROLLER CONSTRAINS ------------ */
#define  USBD_DRV_KINETIS_REG_TO                    0x0000FFFF
#define  USBD_DRV_KINETIS_EP_NBR                    16u
#define  USBD_DRV_KINETIS_BD_TBL_SIZE              (2 * 2 * (USBD_DRV_KINETIS_EP_NBR))

#define  DEF_SETUP_TOKEN                            0x0D
#define  DEF_OUT_TOKEN                              0x01
#define  DEF_IN_TOKEN                               0x09
#define  DEF_SOF_TOKEN                              0x05


#define  ODD(ep_drv_nbr)                        ((((ep_drv_nbr) >> 1) << 1)    )
#define  EVEN(ep_drv_nbr)                       ((((ep_drv_nbr) >> 1) << 1) + 1)

#define  EP_OUT_FLAG(ep_log_nbr)                   (DEF_BIT(2 * ep_log_nbr)    )
#define  EP_IN_FLAG(ep_log_nbr)                    (DEF_BIT(2 * ep_log_nbr + 1))

#define  EP0                                        0u

#define  USBD_DRV_DBG_TRACE_EN                      DEF_ENABLED


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         REGISTER BIT DEFINES
*********************************************************************************************************
*/
                                                                /* ------------ INTERRUPT STATUS BIT FIELDS ----------- */
#define  USBOTG_KINETIS_ISTAT_CLEAR_ALL              0xFFu
#define  USBOTG_KINETIS_ISTAT_USBRST                 0x01u
#define  USBOTG_KINETIS_ISTAT_ERROR                  0x02u
#define  USBOTG_KINETIS_ISTAT_SOFTOK                 0x04u
#define  USBOTG_KINETIS_ISTAT_TOKDNE                 0x08u
#define  USBOTG_KINETIS_ISTAT_SLEEP                  0x10u
#define  USBOTG_KINETIS_ISTAT_RESUME                 0x20u
#define  USBOTG_KINETIS_ISTAT_ATTACH                 0x40u
#define  USBOTG_KINETIS_ISTAT_STALL                  0x80u

                                                                /* ------------ INTERRUPT ENABLE BIT FIELDS ----------- */
#define  USBOTG_KINETIS_INTEN_USBRSTEN               0x01u
#define  USBOTG_KINETIS_INTEN_ERROREN                0x02u
#define  USBOTG_KINETIS_INTEN_SOFTOKEN               0x04u
#define  USBOTG_KINETIS_INTEN_TOKDNEEN               0x08u
#define  USBOTG_KINETIS_INTEN_SLEEPEN                0x10u
#define  USBOTG_KINETIS_INTEN_RESUMEEN               0x20u
#define  USBOTG_KINETIS_INTEN_ATTACHEN               0x40u
#define  USBOTG_KINETIS_INTEN_STALLEN                0x80u
#define  USBOTG_KINETIS_INTEN_DISABLE_ALL            0x00u

                                                                /* --------- ERROR INTERRUPT STATUS BIT FIELDS -------- */
#define  USBOTG_KINETIS_ERRSTAT_CLEAR_ALL            0xFFu
#define  USBOTG_KINETIS_ERRSTAT_PIDERR               0x01u
#define  USBOTG_KINETIS_ERRSTAT_CRC5EOF              0x02u
#define  USBOTG_KINETIS_ERRSTAT_CRC16                0x04u
#define  USBOTG_KINETIS_ERRSTAT_DFN8                 0x08u
#define  USBOTG_KINETIS_ERRSTAT_BTOERR               0x10u
#define  USBOTG_KINETIS_ERRSTAT_DMAERR               0x20u
#define  USBOTG_KINETIS_ERRSTAT_BTSERR               0x80u

                                                                /* -------- ERROR INTERRUPT ENABLE BIT FIELDS --------- */
#define  USBOTG_KINETIS_ERREN_CLEAR_ALL              0xFFu
#define  USBOTG_KINETIS_ERREN_DISABLE_ALL            0x00u

                                                                /* ---------------- STATUS BIT FIELDS ----------------- */
#define  USBOTG_KINETIS_STAT_ODD                     0x04u
#define  USBOTG_KINETIS_STAT_TX                      0x08u
#define  USBOTG_KINETIS_STAT_ENDP                    0xF0u

                                                                /* ---------------- CONTROL BIT FIELDS ---------------- */
#define  USBOTG_KINETIS_CTL_USBENSOFEN               0x01u
#define  USBOTG_KINETIS_CTL_ODDRST                   0x02u
#define  USBOTG_KINETIS_CTL_RESUME                   0x04u
#define  USBOTG_KINETIS_CTL_HOSTMODEEN               0x08u
#define  USBOTG_KINETIS_CTL_RESET                    0x10u
#define  USBOTG_KINETIS_CTL_TXSUSPENDTOKENBUSY       0x20u
#define  USBOTG_KINETIS_CTL_SE0                      0x40u
#define  USBOTG_KINETIS_CTL_JSTATE                   0x80u

                                                                /* ---------------- Address Bit Fields ---------------- */
#define  USBOTG_KINETIS_ADDR_ADDR                    0x7Fu
#define  USBOTG_KINETIS_ADDR_LSEN                    0x80u

                                                                /* ---------------- ADDRESS BIT FIELDS ---------------- */
#define  USBOTG_KINETIS_ENDPT_EPHSHK                 0x01u
#define  USBOTG_KINETIS_ENDPT_EPSTALL                0x02u
#define  USBOTG_KINETIS_ENDPT_EPTXEN                 0x04u
#define  USBOTG_KINETIS_ENDPT_EPRXEN                 0x08u
#define  USBOTG_KINETIS_ENDPT_EPCTLDIS               0x10u
#define  USBOTG_KINETIS_ENDPT_RETRYDIS               0x40u
#define  USBOTG_KINETIS_ENDPT_HOSTWOHUB              0x80u

                                                                /* -------------- USB CONTROL BIT FIELDS -------------- */
#define  USBOTG_KINETIS_USBCTRL_PDE                  0x40u
#define  USBOTG_KINETIS_USBCTRL_SUSP                 0x80u

                                                                /* ------------ USB OTG CONTROL BIT FIELDS ------------ */
#define  USBOTG_KINETIS_CONTROL_DPPULLUPNONOTG       0x10u

                                                                /* -------- USB TRANSCEIVER CONTROL BIT FIELDS -------- */
#define  USBOTG_KINETIS_USBTRC0_USB_RESUME_INT       0x01u
#define  USBOTG_KINETIS_USBTRC0_SYNC_DET             0x02u
#define  USBOTG_KINETIS_USBTRC0_USBRESMEN            0x20u
#define  USBOTG_KINETIS_USBTRC0_USBRESET             0x80u


/*
*********************************************************************************************************
*                                            LOCAL MACROS
*********************************************************************************************************
*/

#if ((USBD_CFG_DBG_TRACE_EN == DEF_ENABLED) && (USBD_DRV_DBG_TRACE_EN == DEF_ENABLED))
#define  USBD_DBG_DRV_BUS(msg)              USBD_DBG_GENERIC((msg),                                                     \
                                                              USBD_EP_ADDR_NONE,                                        \
                                                              USBD_IF_NBR_NONE)

#define  USBD_DBG_DRV_STD(msg)              USBD_DBG_GENERIC((msg),                                                     \
                                                              ep_drv_nbr,                                               \
                                                              DEF_BIT_IS_SET(p_drv_data->UsbToogleFlags, ep_flag))


#define  USBD_DBG_DRV_STD_ARG(msg, arg)     USBD_DBG_GENERIC_ARG((msg),                                                 \
                                                                  ep_drv_nbr,                                           \
                                                                  DEF_BIT_IS_SET(p_drv_data->UsbToogleFlags, ep_flag),  \
                                                                  (arg))

#define  USBD_DBG_DRV_BUS_ARG(msg, arg)     USBD_DBG_GENERIC_ARG((msg),                                                 \
                                                                  USBD_EP_ADDR_NONE,                                    \
                                                                  USBD_IF_NBR_NONE,                                     \
                                                                 (arg))

#else
#define  USBD_DBG_DRV_BUS(msg)
#define  USBD_DBG_DRV_STD(msg)
#define  USBD_DBG_DRV_BUS_ARG(msg, arg)
#define  USBD_DBG_DRV_STD_ARG(msg, arg)
#endif


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

typedef  struct  usbd_Kinetis_reg {                             /* ---------------------- USBOTG REGISTERS ---------------------- */

    CPU_REG08    PERID;                                         /* Peripheral ID Register, offset: 0x0                            */
    CPU_REG08    RESERVED_0[3];
    CPU_REG08    IDCOMP;                                        /* Peripheral ID Complement Register, offset: 0x4                 */
    CPU_REG08    RESERVED_1[3];
    CPU_REG08    REV;                                           /* Peripheral Revision Register, offset: 0x8                      */
    CPU_REG08    RESERVED_2[3];
    CPU_REG08    ADDINFO;                                       /* Peripheral Additional Info Register, offset: 0xC               */
    CPU_REG08    RESERVED_3[3];
    CPU_REG08    OTGISTAT;                                      /* OTG Interrupt Status Register, offset: 0x10                    */
    CPU_REG08    RESERVED_4[3];
    CPU_REG08    OTGICR;                                        /* OTG Interrupt Control Register, offset: 0x14                   */
    CPU_REG08    RESERVED_5[3];
    CPU_REG08    OTGSTAT;                                       /* OTG Status Register, offset: 0x18                              */
    CPU_REG08    RESERVED_6[3];
    CPU_REG08    OTGCTL;                                        /* OTG Control Register, offset: 0x1C                             */
    CPU_REG08    RESERVED_7[99];
    CPU_REG08    ISTAT;                                         /* Interrupt Status Register, offset: 0x80                        */
    CPU_REG08    RESERVED_8[3];
    CPU_REG08    INTEN;                                         /* Interrupt Enable Register, offset: 0x84                        */
    CPU_REG08    RESERVED_9[3];
    CPU_REG08    ERRSTAT;                                       /* Error Interrupt Status Register, offset: 0x88                  */
    CPU_REG08    RESERVED_10[3];
    CPU_REG08    ERREN;                                         /* Error Interrupt Enable Register, offset: 0x8C                  */
    CPU_REG08    RESERVED_11[3];
    CPU_REG08    STAT;                                          /* Status Register, offset: 0x90                                  */
    CPU_REG08    RESERVED_12[3];
    CPU_REG08    CTL;                                           /* Control Register, offset: 0x94                                 */
    CPU_REG08    RESERVED_13[3];
    CPU_REG08    ADDR;                                          /* Address Register, offset: 0x98                                 */
    CPU_REG08    RESERVED_14[3];
    CPU_REG08    BDTPAGE1;                                      /* BDT Page Register 1, offset: 0x9C                              */
    CPU_REG08    RESERVED_15[3];
    CPU_REG08    FRMNUML;                                       /* Frame Number Register Low, offset: 0xA0                        */
    CPU_REG08    RESERVED_16[3];
    CPU_REG08    FRMNUMH;                                       /* Frame Number Register High, offset: 0xA4                       */
    CPU_REG08    RESERVED_17[3];
    CPU_REG08    TOKEN;                                         /* Token Register, offset: 0xA8                                   */
    CPU_REG08    RESERVED_18[3];
    CPU_REG08    SOFTHLD;                                       /* SOF Threshold Register, offset: 0xAC                           */
    CPU_REG08    RESERVED_19[3];
    CPU_REG08    BDTPAGE2;                                      /* BDT Page Register 2, offset: 0xB0                              */
    CPU_REG08    RESERVED_20[3];
    CPU_REG08    BDTPAGE3;                                      /* BDT Page Register 3, offset: 0xB4                              */
    CPU_REG08    RESERVED_21[11];

  struct {                                                      /* offset: 0xC0, array step: 0x4                                  */
    CPU_REG08    ENDPT;                                         /* Endpoint Control Register, array offset: 0xC0, array step: 0x4 */
    CPU_REG08    RESERVED_0[3];
  } ENDPOINT[16];

    CPU_REG08    USBCTRL;                                       /* USB Control Register, offset: 0x100                            */
    CPU_REG08    RESERVED_22[3];
    CPU_REG08    OBSERVE;                                       /* USB OTG Observe Register, offset: 0x104                        */
    CPU_REG08    RESERVED_23[3];
    CPU_REG08    CONTROL;                                       /* USB OTG Control Register, offset: 0x108                        */
    CPU_REG08    RESERVED_24[3];
    CPU_REG08    USBTRC0;                                       /* USB Transceiver Control Register 0, offset: 0x10C              */

} USBD_KINETIS_REG;


typedef  union  bd_status_byte {
    CPU_INT08U  StatusByte;
    struct {
        CPU_INT08U          :1;
        CPU_INT08U          :1;
        CPU_INT08U  Bstall  :1;                                 /* Buffer Stall Enable                                            */
        CPU_INT08U  Dts     :1;                                 /* Data Toggle Synch Enable                                       */
        CPU_INT08U  Ninc    :1;                                 /* Address Increment Disable                                      */
        CPU_INT08U  Keep    :1;                                 /* BD Keep Enable                                                 */
        CPU_INT08U  Data    :1;                                 /* Data Toggle Synch Value                                        */
        CPU_INT08U  Uown    :1;                                 /* USB Ownership                                                  */
    } BD_CTRL_BITS;

    struct {
        CPU_INT08U          :2;
        CPU_INT08U  Pid     :4;                                 /* Packet Identifier                                              */
        CPU_INT08U          :2;
    } BD_PID_BITS;
} BD_STATUS_BYTE;                                               /* Buffer Descriptor Status Register                              */


typedef  struct  bd_entry {                                     /* Buffer Descriptor Entry                                        */
    BD_STATUS_BYTE  Status;
    CPU_REG08       dummy;
    CPU_REG16       Cnt;
    CPU_REG32       Addr;
} BD_ENTRY;


typedef  struct  usbd_drv_data {
    BD_ENTRY     BDTbl[USBD_DRV_KINETIS_BD_TBL_SIZE];           /* Buffer Descriptor Table                                        */
    CPU_REG08   *BufPtrTbl[USBD_DRV_KINETIS_BD_TBL_SIZE];       /* Buffer Pointer Table                                           */
    CPU_INT32U   UsbToogleFlags;                                /* USB Toggle flag (Data0/Data1)                                  */
    CPU_INT32U   UsbPingPongFlags;                              /* Driver PingPong register flag (Odd/Even)                       */
    CPU_INT16U   UsbMaxPktSizeTbl[2 * USBD_DRV_KINETIS_EP_NBR]; /* USB negociated wMaxPacketSize value                            */
} USBD_DRV_DATA;


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                             USB DEVICE CONTROLLER DRIVER API PROTOTYPES
*********************************************************************************************************
*/

static  void         USBD_DrvInit       (USBD_DRV     *p_drv,
                                         USBD_ERR     *p_err);

static  void         USBD_DrvStart      (USBD_DRV     *p_drv,
                                         USBD_ERR     *p_err);

static  void         USBD_DrvStop       (USBD_DRV     *p_drv);

static  CPU_BOOLEAN  USBD_DrvAddrSet    (USBD_DRV     *p_drv,
                                         CPU_INT08U    dev_addr);

static  void         USBD_DrvAddrEn     (USBD_DRV     *p_drv,
                                         CPU_INT08U    dev_addr);

static  CPU_BOOLEAN  USBD_DrvCfgSet     (USBD_DRV     *p_drv,
                                         CPU_INT08U    cfg_val);

static  void         USBD_DrvCfgClr     (USBD_DRV     *p_drv,
                                         CPU_INT08U    cfg_val);

static  CPU_INT16U   USBD_DrvGetFrameNbr(USBD_DRV     *p_drv);

static  void         USBD_DrvEP_Open    (USBD_DRV     *p_drv,
                                         CPU_INT08U    ep_addr,
                                         CPU_INT08U    ep_type,
                                         CPU_INT16U    max_pkt_size,
                                         CPU_INT08U    transaction_frame,
                                         USBD_ERR     *p_err);

static  void         USBD_DrvEP_Close   (USBD_DRV     *p_drv,
                                         CPU_INT08U    ep_addr);

static  CPU_INT32U   USBD_DrvEP_RxStart (USBD_DRV     *p_drv,
                                         CPU_INT08U    ep_addr,
                                         CPU_INT08U   *p_buf,
                                         CPU_INT32U    buf_len,
                                         USBD_ERR     *p_err);

static  CPU_INT32U   USBD_DrvEP_Rx      (USBD_DRV     *p_drv,
                                         CPU_INT08U    ep_addr,
                                         CPU_INT08U   *p_buf,
                                         CPU_INT32U    buf_len,
                                         USBD_ERR     *p_err);

static  void         USBD_DrvEP_RxZLP   (USBD_DRV     *p_drv,
                                         CPU_INT08U    ep_addr,
                                         USBD_ERR     *p_err);

static  CPU_INT32U   USBD_DrvEP_Tx      (USBD_DRV     *p_drv,
                                         CPU_INT08U    ep_addr,
                                         CPU_INT08U   *p_buf,
                                         CPU_INT32U    buf_len,
                                         USBD_ERR     *p_err);

static  void         USBD_DrvEP_TxStart (USBD_DRV     *p_drv,
                                         CPU_INT08U    ep_addr,
                                         CPU_INT08U   *p_buf,
                                         CPU_INT32U    buf_len,
                                         USBD_ERR     *p_err);


static  void         USBD_DrvEP_TxZLP   (USBD_DRV     *p_drv,
                                         CPU_INT08U    ep_addr,
                                         USBD_ERR     *p_err);

static  CPU_BOOLEAN  USBD_DrvEP_Abort   (USBD_DRV     *p_drv,
                                         CPU_INT08U    ep_addr);

static  CPU_BOOLEAN  USBD_DrvEP_Stall   (USBD_DRV     *p_drv,
                                         CPU_INT08U    ep_addr,
                                         CPU_BOOLEAN   state);

static  void         USBD_DrvISR_Handler(USBD_DRV     *p_drv);


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void        USBD_DrvISR_UsbrstHandler(USBD_DRV  *p_drv);
static  void        USBD_DrvISR_TokdneHandler(USBD_DRV  *p_drv);


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                  USB DEVICE CONTROLLER DRIVER API
*********************************************************************************************************
*/

USBD_DRV_API  USBD_DrvAPI_KINETIS = { USBD_DrvInit,
                                      USBD_DrvStart,
                                      USBD_DrvStop,
                                      USBD_DrvAddrSet,
                                      USBD_DrvAddrEn,
                                      USBD_DrvCfgSet,
                                      USBD_DrvCfgClr,
                                      USBD_DrvGetFrameNbr,
                                      USBD_DrvEP_Open,
                                      USBD_DrvEP_Close,
                                      USBD_DrvEP_RxStart,
                                      USBD_DrvEP_Rx,
                                      USBD_DrvEP_RxZLP,
                                      USBD_DrvEP_Tx,
                                      USBD_DrvEP_TxStart,
                                      USBD_DrvEP_TxZLP,
                                      USBD_DrvEP_Abort,
                                      USBD_DrvEP_Stall,
                                      USBD_DrvISR_Handler,
};


/*
*********************************************************************************************************
*********************************************************************************************************
*                                     DRIVER INTERFACE FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                           USBD_DrvInit()
*
* Description : Initialize the device.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE       Device successfully initialized.
*                               USBD_ERR_ALLOC      Memory allocation failed.
*
* Return(s)   : none.
*
* Note(s)     : (1) Since the CPU frequency could be higher than OTG module clock, a timeout is needed
*                   to reset the OTG controller successfully.
*********************************************************************************************************
*/

static  void  USBD_DrvInit (USBD_DRV  *p_drv,
                            USBD_ERR  *p_err)
{
    USBD_DRV_BSP_API  *p_bsp_api;
    USBD_KINETIS_REG  *p_reg;
    USBD_DRV_DATA     *p_drv_data;
    LIB_ERR            err_lib;
    CPU_INT16U         reg_to;


    p_bsp_api =  p_drv->BSP_API_Ptr;                            /* Get driver BSP API reference.                        */
    p_reg     = (USBD_KINETIS_REG *)p_drv->CfgPtr->BaseAddr;    /* Get USB ctrl reg ref.                                */

    if (p_bsp_api->Init != (void *)0) {
        p_bsp_api->Init(p_drv);                                 /* Call board/chip specific device controller ...       */
    }


                                                                /* -------------------- RESET ------------------------  */
    DEF_BIT_SET_08(p_reg->USBTRC0, USBOTG_KINETIS_USBTRC0_USBRESET);
    reg_to = USBD_DRV_KINETIS_REG_TO;
    while (reg_to > 0) {
        reg_to--;
    }

                                                                /* ------------------- INTERRUPT FLAGS ---------------- */
    p_reg->ISTAT   = USBOTG_KINETIS_ISTAT_CLEAR_ALL;            /* Clear pending local interupts                        */
    p_reg->ERRSTAT = USBOTG_KINETIS_ERRSTAT_CLEAR_ALL;
    p_reg->INTEN   = USBOTG_KINETIS_INTEN_DISABLE_ALL;          /* Disable local interrupts                             */
    p_reg->ERREN   = USBOTG_KINETIS_ERREN_DISABLE_ALL;


                                                                /* ------------------ ADDRESS SETTING ----------------- */
    USBD_DrvAddrEn(p_drv, 0);


                                                                /* --------------- ALLOCATION DRIVER DATA ------------- */
    p_drv->DataPtr = (USBD_DRV_DATA *)Mem_HeapAlloc(sizeof(USBD_DRV_DATA),
                                                    512u,       /* Buffer Descriptor Table shall be 512 bytes aligned   */
                                                    DEF_NULL,
                                                   &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

    Mem_Clr(p_drv->DataPtr, sizeof(USBD_DRV_DATA));

    p_drv_data                   = p_drv->DataPtr;
    p_drv_data->UsbPingPongFlags = 0x00u;

    p_drv_data->BufPtrTbl[ODD(EP0)] = Mem_HeapAlloc(p_drv->CfgPtr->EP_InfoTbl[EP0].MaxPktSize,
                                                    sizeof(CPU_ALIGN),
                                                    DEF_NULL,
                                                   &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

    p_drv_data->BufPtrTbl[EVEN(EP0)] = Mem_HeapAlloc(p_drv->CfgPtr->EP_InfoTbl[EP0].MaxPktSize,
                                                     sizeof(CPU_ALIGN),
                                                     DEF_NULL,
                                                    &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

                                                                /* -------------- BD TABLE ADDR SETTINGS -------------- */
    p_reg->BDTPAGE1 = (CPU_REG08)(((CPU_INT32U)p_drv_data->BDTbl) >> 8);
    p_reg->BDTPAGE2 = (CPU_REG08)(((CPU_INT32U)p_drv_data->BDTbl) >> 16);
    p_reg->BDTPAGE3 = (CPU_REG08)(((CPU_INT32U)p_drv_data->BDTbl) >> 24);

    USBD_DBG_DRV_BUS("DrvInit");

   *p_err = USBD_ERR_NONE;
}

/*
*********************************************************************************************************
*                                           USBD_DrvStart()
*
* Description : Start device operation :
*
*                   (1) Enable device controller bus state interrupts.
*                   (2) Call board/chip specific connect function.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE    Device successfully connected.
*
* Return(s)   : none.
*
* Note(s)     : Typically, the start function activates the pull-down on the D- pin to simulate
*               attachment to host. Some MCUs/MPUs have an internal pull-down that is activated by a
*               device controller register; for others, this may be a GPIO pin. Additionally, interrupts
*               for reset and suspend are activated.
*********************************************************************************************************
*/

static  void  USBD_DrvStart (USBD_DRV  *p_drv,
                             USBD_ERR  *p_err)
{
    USBD_DRV_BSP_API  *p_bsp_api;
    USBD_KINETIS_REG  *p_reg;


    p_bsp_api =  p_drv->BSP_API_Ptr;                                /* Get driver BSP API reference.                        */
    p_reg     = (USBD_KINETIS_REG *)p_drv->CfgPtr->BaseAddr;        /* Get USB ctrl reg ref.                                */

    if (p_bsp_api->Conn != (void *)0) {
        p_bsp_api->Conn();                                          /* Call board/chip specific connect function.           */
    }

                                                                    /* ---------------- INTERRUPT FLAGS ------------------- */
    p_reg->ISTAT   = USBOTG_KINETIS_ISTAT_CLEAR_ALL;                /* Clean pending local interrupts flags                 */
    p_reg->ERRSTAT = USBOTG_KINETIS_ERRSTAT_CLEAR_ALL;
    p_reg->ERREN   = USBOTG_KINETIS_ERREN_CLEAR_ALL;

    DEF_BIT_SET_08(p_reg->INTEN, USBOTG_KINETIS_INTEN_TOKDNEEN);    /* Enable local interrupts                              */
    DEF_BIT_SET_08(p_reg->INTEN, USBOTG_KINETIS_INTEN_SLEEPEN);
    DEF_BIT_SET_08(p_reg->INTEN, USBOTG_KINETIS_INTEN_ERROREN);
    DEF_BIT_SET_08(p_reg->INTEN, USBOTG_KINETIS_INTEN_USBRSTEN);
    DEF_BIT_SET_08(p_reg->INTEN, USBOTG_KINETIS_INTEN_STALLEN);

                                                                    /* -------------------- PULLUPS ----------------------- */
    DEF_BIT_SET_08(p_reg->USBCTRL, USBOTG_KINETIS_USBCTRL_PDE);     /* Enables the weak pulldowns (Doc p 1270)              */
    DEF_BIT_CLR_08(p_reg->USBCTRL, USBOTG_KINETIS_USBCTRL_SUSP);    /* Not in suspend state (Doc p 1270)                    */
    DEF_BIT_SET_08(p_reg->USBTRC0, DEF_BIT_06);                     /* Software must set this bit to one.                   */
    DEF_BIT_SET_08(p_reg->CONTROL,
                   USBOTG_KINETIS_CONTROL_DPPULLUPNONOTG);          /* Non-OTG device mode is enabled (Doc p 1271)          */

                                                                    /* --------------- ENABLE USB MODULE ------------------ */
    DEF_BIT_SET_08(p_reg->CTL, USBOTG_KINETIS_CTL_USBENSOFEN);

    USBD_DBG_DRV_BUS("DrvStart");
   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           USBD_DrvStop()
*
* Description : Stop device operation.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
* Return(s)   : none.
*
* Note(s)     : Typically, the stop function performs the following operations:
*               (1) Clear and disable USB interrupts.
*               (2) Disconnect from the USB host (e.g, reset the pull-down on the D- pin).
*********************************************************************************************************
*/

static  void  USBD_DrvStop (USBD_DRV  *p_drv)
{
    USBD_DRV_BSP_API  *p_bsp_api;
    USBD_KINETIS_REG  *p_reg;


    p_bsp_api =  p_drv->BSP_API_Ptr;                            /* Get driver BSP API reference.                        */
    p_reg     = (USBD_KINETIS_REG *)p_drv->CfgPtr->BaseAddr;    /* Get USB ctrl reg ref.                                */


    if (p_bsp_api->Disconn != (void *)0) {
        p_bsp_api->Disconn();
    }

                                                                /* -------------- DISABLE USB MODULE ------------------ */
    DEF_BIT_CLR_08(p_reg->CTL, USBOTG_KINETIS_CTL_USBENSOFEN);


                                                                /* ---------------- INTERRUPT FLAGS ------------------- */
                                                                /* Clear pending local interrupts flag                  */
    p_reg->ISTAT   = USBOTG_KINETIS_ISTAT_CLEAR_ALL;
    p_reg->ERRSTAT = USBOTG_KINETIS_ERRSTAT_CLEAR_ALL;
                                                                /* Disable local interrupts                             */
    p_reg->ERREN   = USBOTG_KINETIS_ERREN_DISABLE_ALL;
    p_reg->INTEN   = USBOTG_KINETIS_INTEN_DISABLE_ALL;


                                                                /* -------------------- PULLUPS ----------------------- */
    DEF_BIT_CLR_08(p_reg->USBCTRL, USBOTG_KINETIS_USBCTRL_PDE);
    DEF_BIT_CLR_08(p_reg->CONTROL, USBOTG_KINETIS_CONTROL_DPPULLUPNONOTG);

    USBD_DBG_DRV_BUS("USBD_DrvStop");
}


/*
*********************************************************************************************************
*                                          USBD_DrvAddrSet()
*
* Description : Assign an address to device.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               dev_addr    Device address assigned by the host.
*
* Return(s)   : DEF_OK,   if NO error(s).
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) For device controllers that have hardware assistance to enable the device address after
*                   the status stage has completed, the assignment of the device address can also be
*                   combined with enabling the device address mode.
*
*               (2) For device controllers that change the device address immediately, without waiting the
*                   status phase to complete, see USBD_DrvAddrEn().
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_DrvAddrSet (USBD_DRV    *p_drv,
                                      CPU_INT08U   dev_addr)
{
    (void)p_drv;
    (void)dev_addr;

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                          USBD_DrvAddrEn()
*
* Description : Enable address on device.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               dev_addr    Device address assigned by the host.
*
* Return(s)   : none.
*
* Note(s)     : (1) For device controllers that have hardware assistance to enable the device address after
*                   the status stage has completed, no operation needs to be performed.
*
*               (2) For device controllers that change the device address immediately, without waiting the
*                   status phase to complete, the device address must be set and enabled.
*********************************************************************************************************
*/

static  void  USBD_DrvAddrEn (USBD_DRV    *p_drv,
                              CPU_INT08U   dev_addr)
{
    USBD_KINETIS_REG  *p_reg;

    p_reg = (USBD_KINETIS_REG *)p_drv->CfgPtr->BaseAddr;        /* Get USB ctrl reg ref.                                */

    DEF_BIT_CLR_08(p_reg->ADDR, USBOTG_KINETIS_ADDR_ADDR);
    p_reg->ADDR |= dev_addr & USBOTG_KINETIS_ADDR_ADDR;

    USBD_DBG_DRV_BUS_ARG("USBD_DrvAddrEn", dev_addr);
}


/*
*********************************************************************************************************
*                                          USBD_DrvCfgSet()
*
* Description : Bring device into configured state.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               cfg_val     Configuration value.
*
* Return(s)   : DEF_OK,   if NO error(s).
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : Typically, the set configuration function sets the device as configured. For some
*               controllers, this may not be necessary.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_DrvCfgSet (USBD_DRV    *p_drv,
                                     CPU_INT08U   cfg_val)
{
    (void)p_drv;
    (void)cfg_val;

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                          USBD_DrvCfgClr()
*
* Description : Bring device into de-configured state.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               cfg_val     Configuration value.
*
* Return(s)   : none.
*
* Note(s)     : (1) Typically, the clear configuration function sets the device as not being configured.
*                   For some controllers, this may not be necessary.
*
*               (2) This functions in invoked after a bus reset or before the status stage of some
*                   SET_CONFIGURATION requests.
*********************************************************************************************************
*/

static  void  USBD_DrvCfgClr (USBD_DRV    *p_drv,
                              CPU_INT08U   cfg_val)
{
    (void)p_drv;
    (void)cfg_val;
}


/*
*********************************************************************************************************
*                                        USBD_DrvGetFrameNbr()
*
* Description : Retrieve current frame number.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
* Return(s)   : Frame number.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT16U  USBD_DrvGetFrameNbr (USBD_DRV  *p_drv)
{
    CPU_INT16U         frame_nbr;
    USBD_KINETIS_REG  *p_reg;

    p_reg     = (USBD_KINETIS_REG *)p_drv->CfgPtr->BaseAddr;    /* Get USB ctrl reg ref.                                */

    frame_nbr  = (CPU_INT16U)(p_reg->FRMNUML);
    frame_nbr |= (CPU_INT16U)(p_reg->FRMNUMH << 8);

    USBD_DBG_DRV_BUS("DrvGetFrameNbr");
    return (frame_nbr);
}


/*
*********************************************************************************************************
*                                          USBD_DrvEP_Open()
*
* Description : Open and configure a device endpoint, given its characteristics (e.g., endpoint type,
*               endpoint address, maximum packet size, etc).
*
* Argument(s) : p_drv               Pointer to device driver structure.
*
*               ep_addr             Endpoint address.
*
*               ep_type             Endpoint type :
*
*                                       USBD_EP_TYPE_CTRL,
*                                       USBD_EP_TYPE_ISOC,
*                                       USBD_EP_TYPE_BULK,
*                                       USBD_EP_TYPE_INTR.
*
*               max_pkt_size        Maximum packet size.
*
*               transaction_frame   Endpoint transactions per frame.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Endpoint successfully opened.
*                               USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*
* Return(s)   : none.
*
* Note(s)     : (1) Typically, the endpoint open function performs the following operations:
*
*                   (a) Validate endpoint address, type and maximum packet size.
*                   (b) Configure endpoint information in the device controller. This may include not
*                       only assigning the type and maximum packet size, but also making certain that
*                       the endpoint is successfully configured (or  realized  or  mapped ). For some
*                       device controllers, this may not be necessary.
*
*               (2) If the endpoint address is valid, then the endpoint open function should validate
*                   the attributes allowed by the hardware endpoint :
*
*                   (a) The maximum packet size 'max_pkt_size' should be validated to match hardware
*                       capabilities.
*********************************************************************************************************
*/

static  void  USBD_DrvEP_Open (USBD_DRV    *p_drv,
                               CPU_INT08U   ep_addr,
                               CPU_INT08U   ep_type,
                               CPU_INT16U   max_pkt_size,
                               CPU_INT08U   transaction_frame,
                               USBD_ERR    *p_err)
{
    USBD_KINETIS_REG  *p_reg;
    USBD_DRV_DATA     *p_drv_data;
    CPU_INT08U         ep_log_nbr;
    CPU_INT08U         ep_phy_nbr;
    CPU_INT08U         ep_drv_nbr;
    CPU_INT32U         ep_flag;


    (void)transaction_frame;


    p_drv_data =  p_drv->DataPtr;                               /* Get driver Data structure reference.                 */
    p_reg      = (USBD_KINETIS_REG *)p_drv->CfgPtr->BaseAddr;   /* Get USB ctrl reg ref.                                */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical number [0-15].                        */
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);                 /* Get EP physical number [0-31].                       */
    ep_drv_nbr = (ep_phy_nbr * 2);                              /* Get the index for EP odd entry in the BD table       */
    ep_flag    =  DEF_BIT(ep_phy_nbr);

                                                                /* ---------- SOFTWARE FLAGS AND VARIABLES ------------ */
    p_drv_data->UsbMaxPktSizeTbl[ep_phy_nbr] = max_pkt_size;
    DEF_BIT_CLR(p_drv_data->UsbToogleFlags, ep_flag);



                                                                /* ---------------- EP TYPE SETTING ------------------- */
    switch (ep_type) {
        case USBD_EP_TYPE_CTRL:
             DEF_BIT_SET_08(p_reg->ENDPOINT[ep_log_nbr].ENDPT, USBOTG_KINETIS_ENDPT_EPHSHK);
             DEF_BIT_CLR_08(p_reg->ENDPOINT[ep_log_nbr].ENDPT, USBOTG_KINETIS_ENDPT_EPCTLDIS);
             break;

        case USBD_EP_TYPE_ISOC:
             DEF_BIT_CLR_08(p_reg->ENDPOINT[ep_log_nbr].ENDPT, USBOTG_KINETIS_ENDPT_EPHSHK);
             break;

        case USBD_EP_TYPE_INTR:
             DEF_BIT_SET_08(p_reg->ENDPOINT[ep_log_nbr].ENDPT, USBOTG_KINETIS_ENDPT_EPHSHK);
             break;

        case USBD_EP_TYPE_BULK:
             DEF_BIT_SET_08(p_reg->ENDPOINT[ep_log_nbr].ENDPT, USBOTG_KINETIS_ENDPT_EPHSHK);
             break;

        default:
            *p_err = USBD_ERR_EP_INVALID_TYPE;
             return;                                            /* Prevent 'break NOT reachable' compiler warning.      */
    }


                                                                /* ---------------- BD ENTRY MANAGEMENT --------------- */
    if (USBD_EP_IS_IN(ep_addr)) {                               /* Tx (In packets): Device to Host                      */
                                                                /* Odd buffer                                           */
        p_drv_data->BDTbl[ODD(ep_drv_nbr)].Status.StatusByte         = DEF_CLR;
        p_drv_data->BDTbl[ODD(ep_drv_nbr)].Status.BD_CTRL_BITS.Data  = DEF_BIT_IS_SET(p_drv_data->UsbToogleFlags, ep_flag);
        p_drv_data->BDTbl[ODD(ep_drv_nbr)].Status.BD_CTRL_BITS.Dts   = DEF_SET;
        p_drv_data->BDTbl[ODD(ep_drv_nbr)].Cnt                       = max_pkt_size;
        p_drv_data->BDTbl[ODD(ep_drv_nbr)].Addr                      = (CPU_INT32U)(p_drv_data->BufPtrTbl[ODD(ep_drv_nbr)]);
        p_drv_data->BDTbl[ODD(ep_drv_nbr)].Status.BD_CTRL_BITS.Uown  = DEF_MCU_CTRL;

                                                                /* Even buffer                                          */
        p_drv_data->BDTbl[EVEN(ep_drv_nbr)].Status.StatusByte        = DEF_CLR;
        p_drv_data->BDTbl[EVEN(ep_drv_nbr)].Status.BD_CTRL_BITS.Data = DEF_BIT_IS_SET(p_drv_data->UsbToogleFlags, ep_flag);
        p_drv_data->BDTbl[EVEN(ep_drv_nbr)].Status.BD_CTRL_BITS.Dts  = DEF_SET;
        p_drv_data->BDTbl[EVEN(ep_drv_nbr)].Cnt                      = max_pkt_size;
        p_drv_data->BDTbl[EVEN(ep_drv_nbr)].Addr                     = (CPU_INT32U)(p_drv_data->BufPtrTbl[EVEN(ep_drv_nbr)]);
        p_drv_data->BDTbl[EVEN(ep_drv_nbr)].Status.BD_CTRL_BITS.Uown = DEF_MCU_CTRL;

        DEF_BIT_SET_08(p_reg->ENDPOINT[ep_log_nbr].ENDPT, USBOTG_KINETIS_ENDPT_EPTXEN);


    } else {                                                    /* Rx (Setup/Out packets): Host to Device               */
                                                                /* Odd buffer                                           */
        p_drv_data->BDTbl[ODD(ep_drv_nbr)].Status.StatusByte         = DEF_CLR;
        p_drv_data->BDTbl[ODD(ep_drv_nbr)].Status.BD_CTRL_BITS.Data  = DEF_BIT_IS_SET(p_drv_data->UsbToogleFlags, ep_flag);
        p_drv_data->BDTbl[ODD(ep_drv_nbr)].Status.BD_CTRL_BITS.Dts   = DEF_SET;
        p_drv_data->BDTbl[ODD(ep_drv_nbr)].Cnt                       = max_pkt_size;
        p_drv_data->BDTbl[ODD(ep_drv_nbr)].Addr                      = (CPU_INT32U)(p_drv_data->BufPtrTbl[ODD(ep_drv_nbr)]);


                                                                /* Even buffer                                          */
        p_drv_data->BDTbl[EVEN(ep_drv_nbr)].Status.StatusByte        = DEF_CLR;
        p_drv_data->BDTbl[EVEN(ep_drv_nbr)].Status.BD_CTRL_BITS.Data = DEF_BIT_IS_SET(p_drv_data->UsbToogleFlags, ep_flag);
        p_drv_data->BDTbl[EVEN(ep_drv_nbr)].Status.BD_CTRL_BITS.Dts  = DEF_SET;
        p_drv_data->BDTbl[EVEN(ep_drv_nbr)].Cnt                      = max_pkt_size;
        p_drv_data->BDTbl[EVEN(ep_drv_nbr)].Addr                     = (CPU_INT32U)(p_drv_data->BufPtrTbl[EVEN(ep_drv_nbr)]);


        if (ep_type == USBD_EP_TYPE_CTRL) {
            p_drv_data->BDTbl[ODD(ep_drv_nbr)].Status.BD_CTRL_BITS.Uown  = DEF_USBFS_CTRL;
            p_drv_data->BDTbl[EVEN(ep_drv_nbr)].Status.BD_CTRL_BITS.Uown = DEF_MCU_CTRL;

        } else {
            p_drv_data->BDTbl[ODD(ep_drv_nbr)].Status.BD_CTRL_BITS.Uown  = DEF_MCU_CTRL;
            p_drv_data->BDTbl[EVEN(ep_drv_nbr)].Status.BD_CTRL_BITS.Uown = DEF_MCU_CTRL;
        }

        DEF_BIT_SET_08(p_reg->ENDPOINT[ep_log_nbr].ENDPT, USBOTG_KINETIS_ENDPT_EPRXEN);
    }

    USBD_DBG_DRV_STD("DrvEP_Open");
   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         USBD_DrvEP_Close()
*
* Description : Close a device endpoint, and uninitialize/clear endpoint configuration in hardware.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_addr     Endpoint address.
*
* Return(s)   : none.
*
* Note(s)     : Typically, the endpoint close function clears the endpoint information in the device
*               controller. For some controllers, this may not be necessary.
*********************************************************************************************************
*/

static  void  USBD_DrvEP_Close (USBD_DRV    *p_drv,
                                CPU_INT08U   ep_addr)
{
    USBD_DRV_DATA     *p_drv_data;
    USBD_KINETIS_REG  *p_reg;
    CPU_INT08U         ep_log_nbr;
    CPU_INT08U         ep_drv_nbr;
    CPU_INT08U         ep_phy_nbr;
    CPU_INT32U         ep_flag;


    p_drv_data =  p_drv->DataPtr;                               /* Get driver Data structure reference.                 */
    p_reg      = (USBD_KINETIS_REG *)p_drv->CfgPtr->BaseAddr;   /* Get USB ctrl reg ref.                                */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical number [0-15].                        */
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);                 /* Get EP physical number [0-31].                       */
    ep_drv_nbr = (ep_phy_nbr * 2);                              /* Get the index for EP odd entry in the BD table       */
    ep_flag    =  DEF_BIT(ep_phy_nbr);


                                                                /* --------------- EP REGISTER BITS ------------------- */
    if (USBD_EP_IS_IN(ep_addr) == DEF_YES) {
        DEF_BIT_CLR(p_reg->ENDPOINT[ep_log_nbr].ENDPT,
                    USBOTG_KINETIS_ENDPT_EPTXEN);
    } else {
        DEF_BIT_CLR(p_reg->ENDPOINT[ep_log_nbr].ENDPT,
                    USBOTG_KINETIS_ENDPT_EPRXEN);
    }
    DEF_BIT_CLR_08(p_reg->ENDPOINT[ep_log_nbr].ENDPT,
                   USBOTG_KINETIS_ENDPT_EPSTALL);

                                                                /* --------------- BD ENTRY MANAGEMENT ---------------- */
                                                                /* Odd buffer                                           */
    p_drv_data->BDTbl[ODD(ep_phy_nbr)].Status.StatusByte  = DEF_CLR;
    p_drv_data->BDTbl[ODD(ep_phy_nbr)].Cnt                = 0;
    p_drv_data->BDTbl[ODD(ep_phy_nbr)].Addr               = (CPU_INT32U)DEF_NULL;

                                                                /* Even buffer                                          */
    p_drv_data->BDTbl[EVEN(ep_phy_nbr)].Status.StatusByte = DEF_CLR;
    p_drv_data->BDTbl[EVEN(ep_phy_nbr)].Cnt               = 0;
    p_drv_data->BDTbl[EVEN(ep_phy_nbr)].Addr              = (CPU_INT32U)DEF_NULL;


                                                                /* ----------- SOFTWARE FLAGS AND VARIABLES ----------- */
    p_drv_data->UsbMaxPktSizeTbl[ep_phy_nbr] = 0;
    DEF_BIT_CLR(p_drv_data->UsbToogleFlags, ep_flag);

    USBD_DBG_DRV_STD("DrvEP_Close");
    (void)ep_drv_nbr;
}


/*
*********************************************************************************************************
*                                        USBD_DrvEP_RxStart()
*
* Description : Configure endpoint with buffer to receive data.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_addr     Endpoint address.
*
*               p_buf       Pointer to data buffer.
*
*               buf_len     Length of the buffer.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE   Receive successfully configured.
*                               USBD_ERR_RX     Generic Rx error.
*
* Return(s)   : Number of bytes that can be transferred in one iteration.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT32U  USBD_DrvEP_RxStart (USBD_DRV    *p_drv,
                                        CPU_INT08U   ep_addr,
                                        CPU_INT08U  *p_buf,
                                        CPU_INT32U   buf_len,
                                        USBD_ERR    *p_err)
{
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT08U      ep_phy_nbr;
    CPU_INT08U      ep_log_nbr;
    CPU_INT08U      ep_drv_nbr;
    CPU_INT32U      ep_flag;
    CPU_BOOLEAN     is_bd_odd;


    p_drv_data =  p_drv->DataPtr;                               /* Get driver Data structure reference.                 */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical number [0-15].                        */
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);                 /* Get EP physical number [0-31].                       */
    ep_drv_nbr = (ep_phy_nbr * 2);                              /* Get EP driver number [0-63].                         */

    ep_flag = DEF_BIT(ep_phy_nbr);

    (void)ep_log_nbr;


                                                                /* ------------- PING PONG FLAG MANAGEMENT ------------ */
    is_bd_odd   = DEF_BIT_IS_SET(p_drv_data->UsbPingPongFlags, ep_flag);
    ep_drv_nbr +=is_bd_odd;


                                                                /* ---------------- BD ENTRY MANAGEMENT --------------- */
   if (buf_len <= p_drv_data->UsbMaxPktSizeTbl[ep_phy_nbr]) {
        p_drv_data->BDTbl[ep_drv_nbr].Cnt                    = buf_len;

   } else {
        p_drv_data->BDTbl[ep_drv_nbr].Cnt                    = p_drv_data->UsbMaxPktSizeTbl[ep_phy_nbr];
   }
    p_drv_data->BDTbl[ep_drv_nbr].Addr                       = (CPU_INT32U)p_buf;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Bstall = DEF_CLR;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Dts    = DEF_SET;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Ninc   = DEF_CLR;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Keep   = DEF_CLR;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Data   = DEF_BIT_IS_SET(p_drv_data->UsbToogleFlags, ep_flag);
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Uown   = DEF_USBFS_CTRL;


    USBD_DBG_DRV_STD_ARG("RxStart", buf_len);

   *p_err = USBD_ERR_NONE;

    return (p_drv_data->BDTbl[ep_drv_nbr].Cnt);
}


/*
*********************************************************************************************************
*                                           USBD_DrvEP_Rx()
*
* Description : Receive the specified amount of data from device endpoint.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_addr     Endpoint address.
*
*               p_buf       Pointer to data buffer.
*
*               buf_len     Length of the buffer.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE   Data successfully received.
*                               USBD_ERR_RX     Generic Rx error.
*
* Return(s)   : Number of octets received, if NO error(s).
*
*               0,                         otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT32U  USBD_DrvEP_Rx (USBD_DRV    *p_drv,
                                   CPU_INT08U   ep_addr,
                                   CPU_INT08U  *p_buf,
                                   CPU_INT32U   buf_len,
                                   USBD_ERR    *p_err)
{
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT08U      ep_phy_nbr;
    CPU_INT08U      ep_log_nbr;
    CPU_INT08U      ep_drv_nbr;
    CPU_INT32U      ep_flag;
    CPU_INT08U      is_bd_odd;


    (void)p_buf;
    (void)buf_len;

    p_drv_data =  p_drv->DataPtr;                               /* Get driver Data structure reference.                 */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical number [0-15].                        */
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);                 /* Get EP physical number [0-31].                       */
    ep_drv_nbr = (ep_phy_nbr * 2);                              /* Get EP driver number [0-63].                         */
    ep_flag    =  DEF_BIT(ep_phy_nbr);

    (void)ep_log_nbr;


                                                                /* ------------- PING PONG FLAG MANAGEMENT ------------ */

    is_bd_odd   = DEF_BIT_IS_CLR(p_drv_data->UsbPingPongFlags, ep_flag);
    ep_drv_nbr +=is_bd_odd;

                                                                /* ------------- USB DATA FLAG MANAGEMENT ------------- */
    if (DEF_BIT_IS_SET(p_drv_data->UsbToogleFlags, ep_flag) == DEF_YES) {
        DEF_BIT_CLR(p_drv_data->UsbToogleFlags, ep_flag);

    } else {
        DEF_BIT_SET(p_drv_data->UsbToogleFlags, ep_flag);
    }

                                                                /* ---------------- RETURNED VALUE --------------------- */
   *p_err = USBD_ERR_NONE;
    USBD_DBG_DRV_STD("Rx");

    return (p_drv_data->BDTbl[ep_drv_nbr].Cnt);
}


/*
*********************************************************************************************************
*                                         USBD_DrvEP_RxZLP()
*
* Description : Receive zero-length packet from endpoint.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_addr     Endpoint address.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE   Zero-length packet successfully received.
*                               USBD_ERR_RX     Generic Rx error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_DrvEP_RxZLP (USBD_DRV    *p_drv,
                                CPU_INT08U   ep_addr,
                                USBD_ERR    *p_err)
{
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT08U      ep_phy_nbr;
    CPU_INT08U      ep_log_nbr;
    CPU_INT08U      ep_drv_nbr;
    CPU_INT32U      ep_flag;
    CPU_BOOLEAN     is_bd_odd;
    CPU_INT08U      ep0_drv_nbr;

    p_drv_data =  p_drv->DataPtr;                               /* Get driver Data structure reference.                 */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical number [0-15].                        */
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);                 /* Get EP physical number [0-31].                       */
    ep_drv_nbr = (ep_phy_nbr * 2);                              /* Get EP driver number [0-63].                         */

    ep_flag = DEF_BIT(ep_phy_nbr);


                                                                /* ------------ PING PONG FLAG MANAGEMENT ------------- */

    is_bd_odd   = DEF_BIT_IS_CLR(p_drv_data->UsbPingPongFlags, ep_flag);
    ep_drv_nbr +=is_bd_odd;


                                                                /* -------------- BD ENTRY MANAGEMENT ----------------- */
    p_drv_data->BDTbl[ep_drv_nbr].Cnt                        = 0;
    p_drv_data->BDTbl[ep_drv_nbr].Addr                       = (CPU_INT32U)DEF_NULL;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Bstall = DEF_CLR;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Dts    = DEF_SET;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Ninc   = DEF_CLR;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Keep   = DEF_CLR;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Data   = DEF_BIT_IS_SET(p_drv_data->UsbToogleFlags, ep_flag);
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Uown   = DEF_MCU_CTRL;


                                                                /* ------------- USB DATA FLAG MANAGEMENT ------------- */
    if (DEF_BIT_IS_CLR(p_drv_data->UsbToogleFlags, ep_flag) == DEF_YES) {
        DEF_BIT_CLR(p_drv_data->UsbToogleFlags, ep_flag);

    } else {
        DEF_BIT_SET(p_drv_data->UsbToogleFlags, ep_flag);
    }

                                                               /* ------------ CONTROL TRANSFERT EP REASSERTING ------- */
    if (ep_log_nbr == EP0) {
        ep0_drv_nbr = EP0 + DEF_BIT_IS_SET(p_drv_data->UsbPingPongFlags, EP_OUT_FLAG(EP0));

        p_drv_data->BDTbl[ep0_drv_nbr].Status.BD_CTRL_BITS.Bstall =  DEF_CLR;
        p_drv_data->BDTbl[ep0_drv_nbr].Status.BD_CTRL_BITS.Dts    =  DEF_SET;
        p_drv_data->BDTbl[ep0_drv_nbr].Status.BD_CTRL_BITS.Ninc   =  DEF_CLR;
        p_drv_data->BDTbl[ep0_drv_nbr].Status.BD_CTRL_BITS.Keep   =  DEF_CLR;
        p_drv_data->BDTbl[ep0_drv_nbr].Status.BD_CTRL_BITS.Data   =  DEF_CLR;
        p_drv_data->BDTbl[ep0_drv_nbr].Cnt                        =  p_drv_data->UsbMaxPktSizeTbl[EP0];
        p_drv_data->BDTbl[ep0_drv_nbr].Addr                       = (CPU_INT32U)(p_drv_data->BufPtrTbl[ep0_drv_nbr]);
        p_drv_data->BDTbl[ep0_drv_nbr].Status.BD_CTRL_BITS.Uown   =  DEF_USBFS_CTRL;
    }


    USBD_DBG_DRV_STD("RxZLP");
   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           USBD_DrvEP_Tx()
*
* Description : Configure endpoint with buffer to transmit data.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_addr     Endpoint address.
*
*               p_buf       Pointer to buffer of data that will be transmitted.
*
*               buf_len     Number of octets to transmit.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE   Transmit successfully configured.
*                               USBD_ERR_TX     Generic Tx error.
*
* Return(s)   : Number of octets transmitted, if NO error(s).
*
*               0,                            otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT32U  USBD_DrvEP_Tx (USBD_DRV    *p_drv,
                                   CPU_INT08U   ep_addr,
                                   CPU_INT08U  *p_buf,
                                   CPU_INT32U   buf_len,
                                   USBD_ERR    *p_err)
{
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT08U      ep_log_nbr;
    CPU_INT08U      ep_phy_nbr;
    CPU_INT08U      ep_drv_nbr;
    CPU_INT32U      ep_flag;
    CPU_BOOLEAN     is_bd_odd;


    p_drv_data =  p_drv->DataPtr;                               /* Get driver Data structure reference.                 */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical number [0-15].                        */
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);                 /* Get EP physical number [0-31].                       */
    ep_drv_nbr = (ep_phy_nbr * 2);                              /* Get EP driver number [0-63].                         */
    ep_flag    =  DEF_BIT(ep_phy_nbr);

    (void)ep_log_nbr;
                                                                /* ------------- PING PONG FLAG MANAGEMENT ------------ */

    is_bd_odd   = DEF_BIT_IS_SET(p_drv_data->UsbPingPongFlags, ep_flag);
    ep_drv_nbr +=is_bd_odd;


                                                                /* ------------------- SET BD ENTRY ------------------- */
    if (buf_len <= (p_drv_data->UsbMaxPktSizeTbl[ep_phy_nbr])) {
        p_drv_data->BDTbl[ep_drv_nbr].Cnt = buf_len;

    } else {
        p_drv_data->BDTbl[ep_drv_nbr].Cnt = p_drv_data->UsbMaxPktSizeTbl[ep_phy_nbr] ;
    }
    p_drv_data->BDTbl[ep_drv_nbr].Addr    = (CPU_INT32U)p_buf;


                                                                /* ------------------- RETURNED VALUE ----------------- */
    USBD_DBG_DRV_STD_ARG("Tx", buf_len);
   *p_err = USBD_ERR_NONE;

    return (p_drv_data->BDTbl[ep_drv_nbr].Cnt);
}

/*
*********************************************************************************************************
*                                        USBD_DrvEP_TxStart()
*
* Description : Transmit the specified amount of data to device endpoint.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_addr     Endpoint address.
*
*               p_buf       Pointer to buffer of data that will be transmitted.
*
*               buf_len     Number of octets to transmit.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE   Data successfully transmitted.
*                               USBD_ERR_TX     Generic Tx error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_DrvEP_TxStart (USBD_DRV    *p_drv,
                                  CPU_INT08U   ep_addr,
                                  CPU_INT08U  *p_buf,
                                  CPU_INT32U   buf_len,
                                  USBD_ERR    *p_err)
{
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT08U      ep_drv_nbr;
    CPU_INT08U      ep_log_nbr;
    CPU_INT08U      ep_phy_nbr;
    CPU_INT32U      ep_flag;
    CPU_BOOLEAN     is_bd_odd;


    (void)p_buf;
    (void)buf_len;

    p_drv_data =  p_drv->DataPtr;                               /* Get driver Data structure reference.                 */
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);                 /* Get EP physical number.                              */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP physical number.                              */
    ep_drv_nbr = (ep_phy_nbr * 2);                              /* Get EP driver number [0-63].                         */
    ep_flag    =  DEF_BIT(ep_phy_nbr);

    (void)ep_log_nbr;
                                                                /* ------------- PING PONG FLAG MANAGEMENT ------------ */
    is_bd_odd   = DEF_BIT_IS_SET(p_drv_data->UsbPingPongFlags, ep_flag);
    ep_drv_nbr +=is_bd_odd;


                                                                /* -------------- BD ENTRY MANAGEMENT ----------------- */
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Data   = DEF_BIT_IS_SET(p_drv_data->UsbToogleFlags, ep_flag);
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Bstall = DEF_CLR;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Dts    = DEF_SET;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Ninc   = DEF_CLR;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Keep   = DEF_CLR;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Uown   = DEF_USBFS_CTRL;


                                                                /* ------------ USB DATA FLAG MANAGEMENT -------------- */
    if (DEF_BIT_IS_SET(p_drv_data->UsbToogleFlags, ep_flag) == DEF_YES) {
        DEF_BIT_CLR(p_drv_data->UsbToogleFlags, ep_flag);

    } else {
        DEF_BIT_SET(p_drv_data->UsbToogleFlags, ep_flag);
    }


    USBD_DBG_DRV_STD_ARG("TxStart", buf_len);
   *p_err = USBD_ERR_NONE;

}


/*
*********************************************************************************************************
*                                         USBD_DrvEP_TxZLP()
*
* Description : Transmit zero-length packet from endpoint.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_addr     Endpoint address.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE   Zero-length packet successfully transmitted.
*                               USBD_ERR_TX     Generic Tx error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_DrvEP_TxZLP (USBD_DRV    *p_drv,
                                CPU_INT08U   ep_addr,
                                USBD_ERR    *p_err)
{
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT08U      ep_drv_nbr;
    CPU_INT08U      ep_phy_nbr;
    CPU_INT08U      ep_log_nbr;
    CPU_INT32U      ep_flag;
    CPU_BOOLEAN     is_bd_odd;
    CPU_INT08U      ep0_drv_nbr;


    p_drv_data =  p_drv->DataPtr;                               /* Get driver Data structure reference.                 */
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);                 /* Get EP physical number.                              */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP physical number.                              */
    ep_drv_nbr = (ep_phy_nbr * 2);                              /* Get EP driver number [0-63].                         */
    ep_flag    =  DEF_BIT(ep_phy_nbr);


                                                                /* ------------- PING PONG FLAG MANAGEMENT ------------ */

    is_bd_odd   = DEF_BIT_IS_SET(p_drv_data->UsbPingPongFlags, ep_flag);
    ep_drv_nbr +=is_bd_odd;


                                                                /* -------------- BD ENTRY MANAGEMENT ----------------- */
    p_drv_data->BDTbl[ep_drv_nbr].Cnt                        =  0;
    p_drv_data->BDTbl[ep_drv_nbr].Addr                       = (CPU_INT32U)DEF_NULL;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Data   =  DEF_BIT_IS_SET(p_drv_data->UsbToogleFlags, ep_flag);
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Bstall =  DEF_CLR;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Dts    =  DEF_SET;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Ninc   =  DEF_CLR;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Keep   =  DEF_CLR;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Uown   =  DEF_USBFS_CTRL;


                                                                /* ------------ USB DATA FLAG MANAGEMENT --------------- */
    if (DEF_BIT_IS_SET(p_drv_data->UsbToogleFlags, ep_flag)) {
        DEF_BIT_CLR(p_drv_data->UsbToogleFlags, ep_flag);

    } else {
        DEF_BIT_SET(p_drv_data->UsbToogleFlags, ep_flag);
    }


                                                                /* ------------ CONTROL TRANSFERT REASSERTING ---------- */
    if (ep_log_nbr == EP0) {
        ep0_drv_nbr = EP0 + DEF_BIT_IS_SET(p_drv_data->UsbPingPongFlags, EP_OUT_FLAG(EP0));

        p_drv_data->BDTbl[ep0_drv_nbr].Status.BD_CTRL_BITS.Bstall =  DEF_CLR;
        p_drv_data->BDTbl[ep0_drv_nbr].Status.BD_CTRL_BITS.Dts    =  DEF_SET;
        p_drv_data->BDTbl[ep0_drv_nbr].Status.BD_CTRL_BITS.Ninc   =  DEF_CLR;
        p_drv_data->BDTbl[ep0_drv_nbr].Status.BD_CTRL_BITS.Keep   =  DEF_CLR;
        p_drv_data->BDTbl[ep0_drv_nbr].Status.BD_CTRL_BITS.Data   =  DEF_CLR;
        p_drv_data->BDTbl[ep0_drv_nbr].Cnt                        =  p_drv_data->UsbMaxPktSizeTbl[EP0];
        p_drv_data->BDTbl[ep0_drv_nbr].Addr                       = (CPU_INT32U)(p_drv_data->BufPtrTbl[ep0_drv_nbr]);
        p_drv_data->BDTbl[ep0_drv_nbr].Status.BD_CTRL_BITS.Uown   =  DEF_USBFS_CTRL;
    }


    USBD_DBG_DRV_STD("TxZLP");
   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         USBD_DrvEP_Abort()
*
* Description : Abort any pending transfer on endpoint.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_addr     Endpoint Address.
*
* Return(s)   : DEF_OK,   if NO error(s).
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_DrvEP_Abort (USBD_DRV    *p_drv,
                                       CPU_INT08U   ep_addr)
{
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT08U      ep_drv_nbr;
    CPU_INT08U      ep_phy_nbr;
    CPU_INT08U      ep_log_nbr;
    CPU_BOOLEAN     is_bd_odd;
    CPU_INT08U      ep_flag;


    p_drv_data =  p_drv->DataPtr;                               /* Get driver Data structure reference.                 */
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);                 /* Get EP physical number.                              */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP physical number.                              */
    ep_drv_nbr = (ep_phy_nbr * 2);                              /* Get EP driver number [0-63].                         */
    ep_flag    =  DEF_BIT(ep_phy_nbr);

    (void)ep_log_nbr;

                                                                /* ------------- PING PONG FLAG MANAGEMENT ------------ */
    is_bd_odd   = DEF_BIT_IS_SET(p_drv_data->UsbPingPongFlags, ep_flag);
    ep_drv_nbr +=is_bd_odd;


                                                                /* -------------- BD ENTRY MANAGEMENT ----------------- */
    p_drv_data->BDTbl[ep_drv_nbr].Cnt                        =  0;
    p_drv_data->BDTbl[ep_drv_nbr].Addr                       = (CPU_INT32U)DEF_NULL;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Data   =  DEF_BIT_IS_SET(p_drv_data->UsbToogleFlags, ep_flag);
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Bstall =  DEF_CLR;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Dts    =  DEF_SET;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Ninc   =  DEF_CLR;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Keep   =  DEF_CLR;
    p_drv_data->BDTbl[ep_drv_nbr].Status.BD_CTRL_BITS.Uown   =  DEF_MCU_CTRL;


    USBD_DBG_DRV_STD("USBD_DrvEP_Abort");
    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                         USBD_DrvEP_Stall()
*
* Description : Set or clear stall condition on endpoint.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_addr     Endpoint address.
*
*               state       Endpoint stall state.
*
* Return(s)   : DEF_OK,   if NO error(s).
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_DrvEP_Stall (USBD_DRV     *p_drv,
                                       CPU_INT08U    ep_addr,
                                       CPU_BOOLEAN   state)
{
    USBD_KINETIS_REG  *p_reg;
    USBD_DRV_DATA     *p_drv_data;
    CPU_INT08U         ep_phy_nbr;
    CPU_INT08U         ep_log_nbr;
    CPU_INT08U         ep_drv_nbr;
    CPU_INT08U         ep_flag;
    CPU_INT08U         ep0_drv_nbr;


    p_drv_data =  p_drv->DataPtr;                               /* Get driver Data structure reference.                 */
    p_reg      = (USBD_KINETIS_REG *)p_drv->CfgPtr->BaseAddr;   /* Get USB ctrl reg ref.                                */
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);                 /* Get EP physical number.                              */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP physical number.                              */
    ep_drv_nbr = (ep_phy_nbr * 2);                              /* Get BD table odd entry  index.                       */
    ep_flag    =  DEF_BIT(ep_phy_nbr);


    if (state == DEF_SET) {                                     /* ------------------ SET STALL STATE ----------------- */
        DEF_BIT_CLR_32(p_drv_data->UsbToogleFlags, (CPU_INT32U)ep_flag);
        if (ep_log_nbr == EP0) {                                /* Control Endpoint (EP0)                               */
            DEF_BIT_SET_08(p_reg->ENDPOINT[ep_log_nbr].ENDPT,
                           USBOTG_KINETIS_ENDPT_EPSTALL);

        } else {                                                /* Data Endpoints                                       */
            p_drv_data->BDTbl[ODD(ep_drv_nbr)].Status.BD_CTRL_BITS.Bstall  = DEF_SET;
            p_drv_data->BDTbl[ODD(ep_drv_nbr)].Status.BD_CTRL_BITS.Uown    = DEF_USBFS_CTRL;
            p_drv_data->BDTbl[EVEN(ep_drv_nbr)].Status.BD_CTRL_BITS.Bstall = DEF_SET;
            p_drv_data->BDTbl[EVEN(ep_drv_nbr)].Status.BD_CTRL_BITS.Uown   = DEF_USBFS_CTRL;
        }


    } else {                                                    /* -------------- CLEAR STALL STATE ------------------- */
        DEF_BIT_CLR_32(p_drv_data->UsbToogleFlags, (CPU_INT32U)ep_flag);
        if (ep_log_nbr == EP0) {                                /* Control Endpoint (EP0)                               */
            DEF_BIT_CLR_08(p_reg->ENDPOINT[ep_log_nbr].ENDPT,
                           USBOTG_KINETIS_ENDPT_EPSTALL);

        } else {                                                /* Data Endpoints                                       */
            DEF_BIT_CLR_08(p_reg->ENDPOINT[ep_log_nbr].ENDPT,
                           USBOTG_KINETIS_ENDPT_EPSTALL);
            p_drv_data->BDTbl[ODD(ep_drv_nbr)].Status.BD_CTRL_BITS.Bstall  = DEF_CLR;
            p_drv_data->BDTbl[ODD(ep_drv_nbr)].Status.BD_CTRL_BITS.Dts     = DEF_SET;
            p_drv_data->BDTbl[ODD(ep_drv_nbr)].Status.BD_CTRL_BITS.Ninc    = DEF_CLR;
            p_drv_data->BDTbl[ODD(ep_drv_nbr)].Status.BD_CTRL_BITS.Keep    = DEF_CLR;
            p_drv_data->BDTbl[ODD(ep_drv_nbr)].Status.BD_CTRL_BITS.Uown    = DEF_USBFS_CTRL;

            p_drv_data->BDTbl[EVEN(ep_drv_nbr)].Status.BD_CTRL_BITS.Bstall = DEF_CLR;
            p_drv_data->BDTbl[EVEN(ep_drv_nbr)].Status.BD_CTRL_BITS.Dts    = DEF_SET;
            p_drv_data->BDTbl[EVEN(ep_drv_nbr)].Status.BD_CTRL_BITS.Ninc   = DEF_CLR;
            p_drv_data->BDTbl[EVEN(ep_drv_nbr)].Status.BD_CTRL_BITS.Keep   = DEF_CLR;
            p_drv_data->BDTbl[EVEN(ep_drv_nbr)].Status.BD_CTRL_BITS.Uown   = DEF_USBFS_CTRL;
        }
    }

    if (ep_log_nbr == EP0) {
            ep0_drv_nbr = EP0 + DEF_BIT_IS_SET(p_drv_data->UsbPingPongFlags, EP_OUT_FLAG(EP0));

            p_drv_data->BDTbl[ep0_drv_nbr].Status.BD_CTRL_BITS.Bstall =  DEF_CLR;
            p_drv_data->BDTbl[ep0_drv_nbr].Status.BD_CTRL_BITS.Dts    =  DEF_SET;
            p_drv_data->BDTbl[ep0_drv_nbr].Status.BD_CTRL_BITS.Ninc   =  DEF_CLR;
            p_drv_data->BDTbl[ep0_drv_nbr].Status.BD_CTRL_BITS.Keep   =  DEF_CLR;
            p_drv_data->BDTbl[ep0_drv_nbr].Status.BD_CTRL_BITS.Data   =  DEF_CLR;
            p_drv_data->BDTbl[ep0_drv_nbr].Cnt                        =  p_drv_data->UsbMaxPktSizeTbl[EP0];
            p_drv_data->BDTbl[ep0_drv_nbr].Addr                       = (CPU_INT32U)(p_drv_data->BufPtrTbl[ep0_drv_nbr]);
            p_drv_data->BDTbl[ep0_drv_nbr].Status.BD_CTRL_BITS.Uown   =  DEF_USBFS_CTRL;
    }

    USBD_DBG_DRV_STD_ARG("DrvEP_Stall", state);
    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                        USBD_DrvISR_Handler()
*
* Description : USB device Interrupt Service Routine (ISR) handler.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_DrvISR_Handler (USBD_DRV  *p_drv)
{
    USBD_KINETIS_REG  *p_reg;
    CPU_SR_ALLOC();


    p_reg = (USBD_KINETIS_REG *)p_drv->CfgPtr->BaseAddr;        /* Get USB ctrl reg ref.                                */

    CPU_CRITICAL_ENTER();

                                                                /* ----------------- USBRST INTERRUPT ----------------- */
    if (DEF_BIT_IS_SET(p_reg->ISTAT, USBOTG_KINETIS_ISTAT_USBRST)) {
        USBD_DrvISR_UsbrstHandler (p_drv);
        USBD_EventReset(p_drv);
        p_reg->ISTAT = USBOTG_KINETIS_ISTAT_USBRST;
    }

                                                                /* ----------------- RESUME INTERRUPT ----------------- */
    if (DEF_BIT_IS_SET(p_reg->ISTAT, USBOTG_KINETIS_ISTAT_RESUME)) {
        DEF_BIT_CLR_08(p_reg->INTEN, USBOTG_KINETIS_INTEN_RESUMEEN);
        DEF_BIT_CLR_08(p_reg->USBCTRL, USBOTG_KINETIS_USBCTRL_SUSP);
        DEF_BIT_SET_08(p_reg->USBCTRL, USBOTG_KINETIS_USBCTRL_PDE);
        USBD_EventResume(p_drv);
        USBD_DBG_DRV_BUS("ISR_Resume");
        p_reg->ISTAT = USBOTG_KINETIS_ISTAT_RESUME;
        DEF_BIT_SET_08(p_reg->INTEN, USBOTG_KINETIS_INTEN_SLEEPEN);
    }

                                                                /* ----------------- SLEEP INTERRUPT ------------------ */
    if (DEF_BIT_IS_SET(p_reg->ISTAT, USBOTG_KINETIS_ISTAT_SLEEP)) {
        DEF_BIT_CLR_08(p_reg->INTEN, USBOTG_KINETIS_INTEN_SLEEPEN);
        USBD_EventSuspend(p_drv);
        USBD_DBG_DRV_BUS("ISR_Sleep");
        p_reg->ISTAT = USBOTG_KINETIS_ISTAT_SLEEP;
        DEF_BIT_SET_08(p_reg->INTEN, USBOTG_KINETIS_INTEN_RESUMEEN);
    }

                                                                /* ----------------- TOKDNE INTERRUPT ----------------- */
    if (DEF_BIT_IS_SET(p_reg->ISTAT, USBOTG_KINETIS_ISTAT_TOKDNE)) {
        USBD_DrvISR_TokdneHandler(p_drv);
        p_reg->ISTAT = USBOTG_KINETIS_ISTAT_TOKDNE;
    }

                                                                /* ----------------- SOFTOK INTERRUPT ----------------- */
    if (DEF_BIT_IS_SET(p_reg->ISTAT, USBOTG_KINETIS_ISTAT_SOFTOK)) {
        p_reg->ISTAT = USBOTG_KINETIS_ISTAT_SOFTOK;
    }

                                                                /* ------------------ STALL INTERRUPT ----------------- */
    if (DEF_BIT_IS_SET(p_reg->ISTAT, USBOTG_KINETIS_ISTAT_STALL)) {
        USBD_DBG_DRV_BUS("ISR_Stall");
        p_reg->ISTAT = USBOTG_KINETIS_ISTAT_STALL;
    }

                                                                /* ------------------ ERROR INTERRUPT ----------------- */
    if (DEF_BIT_IS_SET(p_reg->ISTAT, USBOTG_KINETIS_ISTAT_ERROR)) {
        USBD_DBG_DRV_BUS_ARG("ISR_Err", p_reg->ERRSTAT);
        if (DEF_BIT_IS_SET(p_reg->ERRSTAT, USBOTG_KINETIS_ERRSTAT_DFN8)) {
            USBD_DrvISR_UsbrstHandler(p_drv);
            USBD_EventReset(p_drv);
        }
        p_reg->ERRSTAT = USBOTG_KINETIS_ERRSTAT_CLEAR_ALL;
        p_reg->ISTAT = USBOTG_KINETIS_ISTAT_ERROR;
    }
     CPU_CRITICAL_EXIT();
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
*                                        USBD_DrvISR_UsbrstHandler()
*
* Description : USB device Interrupt Service Routine (ISR) handler for USBRST interrupt .
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_DrvISR_UsbrstHandler (USBD_DRV  *p_drv)
{
    USBD_KINETIS_REG  *p_reg;
    USBD_DRV_DATA     *p_drv_data;
    CPU_INT16U         reg_to;


    p_drv_data =  p_drv->DataPtr;                               /* Get driver Data structure reference.                 */
    p_reg      = (USBD_KINETIS_REG *)p_drv->CfgPtr->BaseAddr;   /* Get USB ctrl reg ref.                                */

                                                                /* -------------- CLEAR PENDING INTERRUPTS ------------ */
    p_reg->ISTAT   = USBOTG_KINETIS_ISTAT_CLEAR_ALL;
    p_reg->ERRSTAT = USBOTG_KINETIS_ERRSTAT_CLEAR_ALL;


                                                                /* -------------- RESET ODD/EVEN COUNTER -------------- */
    DEF_BIT_SET_08(p_reg->CTL, USBOTG_KINETIS_CTL_ODDRST);
    reg_to = 0x000F;                                            /* Delay                                                */
    while (reg_to > 0) {
        reg_to--;
    }
    DEF_BIT_CLR_08(p_reg->CTL, USBOTG_KINETIS_CTL_ODDRST);


                                                                /* -------------- CLEAN SOFTWARE FLAGS ---------------- */
    p_drv_data->UsbToogleFlags   = 0x00;
    p_drv_data->UsbPingPongFlags = 0x00;


                                                                /* -------------- CLEAR SUSPEND STATE ----------------- */
    DEF_BIT_CLR_08(p_reg->USBCTRL, USBOTG_KINETIS_USBCTRL_SUSP);


                                                                /* -------------- SET SLEEP INTERRUPT ----------------- */
    DEF_BIT_SET_08(p_reg->INTEN, USBOTG_KINETIS_INTEN_SLEEPEN);


                                                                /* -------------- SET ADDRESS TO ZERO ---------------- */
    USBD_DrvAddrEn(p_drv, 0);

    USBD_DBG_DRV_BUS("ISR_Reset");
}

/*
*********************************************************************************************************
*                                        USBD_DrvISR_TokdneHandler()
*
* Description : USB device Interrupt Service Routine (ISR) handler for TOKDNE interrupt .
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_DrvISR_TokdneHandler (USBD_DRV  *p_drv)
{
    USBD_KINETIS_REG  *p_reg;
    USBD_DRV_DATA     *p_drv_data;
    CPU_BOOLEAN        is_bd_in;
    CPU_BOOLEAN        is_bd_odd;
    CPU_INT08U         ep_phy_nbr;
    CPU_INT08U         ep_log_nbr;
    CPU_INT08U         ep_drv_nbr;
    CPU_INT08U         ep_flag;


    p_drv_data =  p_drv->DataPtr;                               /* Get driver Data structure reference.                 */
    p_reg      = (USBD_KINETIS_REG *)p_drv->CfgPtr->BaseAddr;   /* Get USB ctrl reg ref.                                */
    ep_log_nbr =  p_reg->STAT >> 4;                             /* Get EP logical number [0-15].                        */
    ep_phy_nbr =  p_reg->STAT >> 3;                             /* Get EP physical number [0-31].                       */
    ep_drv_nbr =  p_reg->STAT >> 2;                             /* Get EP driver number [0-63].                         */
    ep_flag    =  DEF_BIT(ep_phy_nbr);
    is_bd_in   =  DEF_BIT_IS_SET(p_reg->STAT, USBOTG_KINETIS_STAT_TX);
    is_bd_odd  =  DEF_BIT_IS_SET(p_reg->STAT, USBOTG_KINETIS_STAT_ODD);


                                                                /* -------------- CONTROL ENDPOINT (EP0) -------------- */
    if (ep_log_nbr == EP0) {
        if (is_bd_in == DEF_NO) {                               /* Setup/Out (Rx) packets                               */
            USBD_DBG_DRV_STD_ARG("ISR_Rx", p_drv_data->BDTbl[ep_drv_nbr].Status.BD_PID_BITS.Pid);

            if (p_drv_data->BDTbl[ep_drv_nbr].Status.BD_PID_BITS.Pid == DEF_SETUP_TOKEN) {
                USBD_EventSetup(        p_drv,
                                (void *)p_drv_data->BufPtrTbl[ep_drv_nbr]);
                DEF_BIT_CLR_08(p_reg->CTL, USBOTG_KINETIS_CTL_TXSUSPENDTOKENBUSY);

                DEF_BIT_SET_32(p_drv_data->UsbToogleFlags,EP_OUT_FLAG(ep_log_nbr));
                DEF_BIT_SET_32(p_drv_data->UsbToogleFlags,EP_IN_FLAG(ep_log_nbr));

            } else {
                USBD_EP_RxCmpl(p_drv, ep_log_nbr);
            }

        } else {                                                /* In (Tx) packets                                      */
            USBD_DBG_DRV_STD_ARG("ISR_Tx", p_drv_data->BDTbl[ep_drv_nbr].Status.BD_PID_BITS.Pid);
            USBD_EP_TxCmpl(p_drv, ep_log_nbr);
        }
                                                                /* ------------------ DATA ENDPOINTS ------------------ */
    } else {
        if (is_bd_in == DEF_NO) {                               /* Out (Rx) packets                                     */
            USBD_EP_RxCmpl(p_drv, ep_log_nbr);
            USBD_DBG_DRV_STD_ARG("ISR_Rx", p_drv_data->BDTbl[ep_drv_nbr].Status.BD_PID_BITS.Pid);

        } else {                                                /* In (Tx) packets                                      */
            USBD_EP_TxCmpl(p_drv, ep_log_nbr);
            USBD_DBG_DRV_STD_ARG("ISR_Tx", p_drv_data->BDTbl[ep_drv_nbr].Status.BD_PID_BITS.Pid);
        }
    }


                                                                /* ------------- PING PONG FLAG MANAGEMENT ------------ */
    if (is_bd_odd == DEF_FALSE) {
        DEF_BIT_SET(p_drv_data->UsbPingPongFlags, ep_flag);
    } else {
        DEF_BIT_CLR(p_drv_data->UsbPingPongFlags, (CPU_INT32U)ep_flag);
    }
}

