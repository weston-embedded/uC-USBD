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
*                                              AT32UC3C
*
* Filename : usbd_drv_at32uc3c.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)  : (1) You can find specific information about this driver at:
*                https://doc.micrium.com/display/USBDDRV/AT32UC3C
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                              INCLUDES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#include  "../../Source/usbd_core.h"
#include  "usbd_drv_at32uc3c.h"
#include  <lib_mem.h>


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  AT32UC3C_USBC_UDCON_UADD_MASK              DEF_BIT_FIELD_32(7, 0)
#define  AT32UC3C_USBC_UDCON_ADDEN                  DEF_BIT_07
#define  AT32UC3C_USBC_UDCON_DETACH                 DEF_BIT_08
#define  AT32UC3C_USBC_UDCON_RMWKUP                 DEF_BIT_09
#define  AT32UC3C_USBC_UDCON_LS                     DEF_BIT_12
#define  AT32UC3C_USBC_UDCON_GNAK                   DEF_BIT_17

#define  AT32UC3C_USBC_UDINT_SUSP                   DEF_BIT_00
#define  AT32UC3C_USBC_UDINT_SOF                    DEF_BIT_02
#define  AT32UC3C_USBC_UDINT_EORST                  DEF_BIT_03
#define  AT32UC3C_USBC_UDINT_WAKEUP                 DEF_BIT_04
#define  AT32UC3C_USBC_UDINT_EORSM                  DEF_BIT_05
#define  AT32UC3C_USBC_UDINT_UPRSM                  DEF_BIT_06
#define  AT32UC3C_USBC_UDINT_EP0INT                 DEF_BIT_12
#define  AT32UC3C_USBC_UDINT_EP1INT                 DEF_BIT_13
#define  AT32UC3C_USBC_UDINT_EP2INT                 DEF_BIT_14
#define  AT32UC3C_USBC_UDINT_EP3INT                 DEF_BIT_15
#define  AT32UC3C_USBC_UDINT_EP4INT                 DEF_BIT_16
#define  AT32UC3C_USBC_UDINT_EP5INT                 DEF_BIT_17
#define  AT32UC3C_USBC_UDINT_EP6INT                 DEF_BIT_18
#define  AT32UC3C_USBC_UDINT_EP7INT                 DEF_BIT_19
#define  AT32UC3C_USBC_UDINT_EP8INT                 DEF_BIT_20

#define  AT32UC3C_USBC_UERST_EPEN0                  DEF_BIT_00
#define  AT32UC3C_USBC_UERST_EPEN1                  DEF_BIT_01
#define  AT32UC3C_USBC_UERST_EPEN2                  DEF_BIT_02
#define  AT32UC3C_USBC_UERST_EPEN3                  DEF_BIT_03
#define  AT32UC3C_USBC_UERST_EPEN4                  DEF_BIT_04
#define  AT32UC3C_USBC_UERST_EPEN5                  DEF_BIT_05
#define  AT32UC3C_USBC_UERST_EPEN6                  DEF_BIT_06
#define  AT32UC3C_USBC_UERST_EPEN7                  DEF_BIT_07
#define  AT32UC3C_USBC_UERST_EPEN8                  DEF_BIT_08

#define  AT32UC3C_USBC_UDFNUM_FNUM_MASK             DEF_BIT_FIELD_32(11, 3)
#define  AT32UC3C_USBC_UDFNUM_FNCERR                DEF_BIT_15

#define  AT32UC3C_USBC_UECFGN_EPBK                  DEF_BIT_02
#define  AT32UC3C_USBC_UECFGN_EPSIZE_MASK           DEF_BIT_FIELD_32(3, 4)
#define  AT32UC3C_USBC_UECFGN_EPDIR                 DEF_BIT_08
#define  AT32UC3C_USBC_UECFGN_EPTYPE_MASK           DEF_BIT_FIELD_32(2, 11)

#define  AT32UC3C_USBC_UECFGN_EPTYPE_CTRL           DEF_BIT_NONE
#define  AT32UC3C_USBC_UECFGN_EPTYPE_ISOC           DEF_BIT_11
#define  AT32UC3C_USBC_UECFGN_EPTYPE_BULK           DEF_BIT_12
#define  AT32UC3C_USBC_UECFGN_EPTYPE_INTR           DEF_BIT_FIELD_32(2, 11)

#define  AT32UC3C_USBC_UECFGN_EPDIR_OUT             DEF_BIT_NONE
#define  AT32UC3C_USBC_UECFGN_EPDIR_IN              DEF_BIT_08

#define  AT32UC3C_USBC_UECFGN_EPSIZE_8              DEF_BIT_NONE
#define  AT32UC3C_USBC_UECFGN_EPSIZE_16             DEF_BIT_04
#define  AT32UC3C_USBC_UECFGN_EPSIZE_32             DEF_BIT_05
#define  AT32UC3C_USBC_UECFGN_EPSIZE_64             DEF_BIT_FIELD_32(2, 4)
#define  AT32UC3C_USBC_UECFGN_EPSIZE_128            DEF_BIT_06
#define  AT32UC3C_USBC_UECFGN_EPSIZE_256           (DEF_BIT_04 | DEF_BIT_06)
#define  AT32UC3C_USBC_UECFGN_EPSIZE_512            DEF_BIT_FIELD_32(2, 5)
#define  AT32UC3C_USBC_UECFGN_EPSIZE_1024           DEF_BIT_FIELD_32(3, 4)

#define  AT32UC3C_USBC_UECFGN_EPBK_SINGLE           DEF_BIT_NONE
#define  AT32UC3C_USBC_UECFGN_EPBK_DOUBLE           DEF_BIT_02

#define  AT32UC3C_USBC_UESTAN_TXINI                 DEF_BIT_00
#define  AT32UC3C_USBC_UESTAN_RXOUTI                DEF_BIT_01
#define  AT32UC3C_USBC_UESTAN_RXSTPI_ERRORFI        DEF_BIT_02
#define  AT32UC3C_USBC_UESTAN_NAKOUTI               DEF_BIT_03
#define  AT32UC3C_USBC_UESTAN_NAKINI                DEF_BIT_04
#define  AT32UC3C_USBC_UESTAN_STALLED_CRCERRI       DEF_BIT_06
#define  AT32UC3C_USBC_UESTAN_DTSEQ_MASK            DEF_BIT_FIELD_32(2, 8)
#define  AT32UC3C_USBC_UESTAN_RAMACERI              DEF_BIT_11
#define  AT32UC3C_USBC_UESTAN_NBUSYBK_MASK          DEF_BIT_FIELD_32(2, 12)
#define  AT32UC3C_USBC_UESTAN_CURRBK_MASK           DEF_BIT_FIELD_32(2, 14)
#define  AT32UC3C_USBC_UESTAN_CTRLDIR               DEF_BIT_17

#define  AT32UC3C_USBC_UESTAN_CURRBK_0              DEF_BIT_NONE
#define  AT32UC3C_USBC_UESTAN_CURRBK_1              DEF_BIT_14

#define  AT32UC3C_USBC_UESTAN_NBUSYBK_0             DEF_BIT_NONE
#define  AT32UC3C_USBC_UESTAN_NBUSYBK_1             DEF_BIT_12
#define  AT32UC3C_USBC_UESTAN_NBUSYBK_2             DEF_BIT_13

#define  AT32UC3C_USBC_UECONN_TXINE                 DEF_BIT_00
#define  AT32UC3C_USBC_UECONN_RXOUTE                DEF_BIT_01
#define  AT32UC3C_USBC_UECONN_RXSTPE_ERRORFE        DEF_BIT_02
#define  AT32UC3C_USBC_UECONN_NAKOUTE               DEF_BIT_03
#define  AT32UC3C_USBC_UECONN_NAKINE                DEF_BIT_04
#define  AT32UC3C_USBC_UECONN_STALLEDE_CRCERRE      DEF_BIT_06
#define  AT32UC3C_USBC_UECONN_RAMACERE              DEF_BIT_11
#define  AT32UC3C_USBC_UECONN_NBUSYBKE              DEF_BIT_12
#define  AT32UC3C_USBC_UECONN_KILLBK                DEF_BIT_13
#define  AT32UC3C_USBC_UECONN_FIFOCON               DEF_BIT_14
#define  AT32UC3C_USBC_UECONN_RSTDT                 DEF_BIT_18
#define  AT32UC3C_USBC_UECONN_STALLRQ               DEF_BIT_19
#define  AT32UC3C_USBC_UECONN_BUSY0E                DEF_BIT_24
#define  AT32UC3C_USBC_UECONN_BUSY1E                DEF_BIT_25

#define  AT32UC3C_USBC_USBCON_IDTE                  DEF_BIT_00
#define  AT32UC3C_USBC_USBCON_VBUSTE                DEF_BIT_01
#define  AT32UC3C_USBC_USBCON_SRPE                  DEF_BIT_02
#define  AT32UC3C_USBC_USBCON_VBERRE                DEF_BIT_03
#define  AT32UC3C_USBC_USBCON_BCERRE                DEF_BIT_04
#define  AT32UC3C_USBC_USBCON_ROLEEXE               DEF_BIT_05
#define  AT32UC3C_USBC_USBCON_HNPERRE               DEF_BIT_06
#define  AT32UC3C_USBC_USBCON_STOE                  DEF_BIT_07
#define  AT32UC3C_USBC_USBCON_VBUSHWC               DEF_BIT_08
#define  AT32UC3C_USBC_USBCON_SRPSEL                DEF_BIT_09
#define  AT32UC3C_USBC_USBCON_SRPREQ                DEF_BIT_10
#define  AT32UC3C_USBC_USBCON_HNPREQ                DEF_BIT_11
#define  AT32UC3C_USBC_USBCON_OTGPADE               DEF_BIT_12
#define  AT32UC3C_USBC_USBCON_VBUSPO                DEF_BIT_13
#define  AT32UC3C_USBC_USBCON_FRZCLK                DEF_BIT_14
#define  AT32UC3C_USBC_USBCON_USBE                  DEF_BIT_15
#define  AT32UC3C_USBC_USBCON_TIMVALUE_MASK         DEF_BIT_FIELD_32(2, 16)
#define  AT32UC3C_USBC_USBCON_TIMPAGE_MASK          DEF_BIT_FIELD_32(2, 20)
#define  AT32UC3C_USBC_USBCON_UNLOCK                DEF_BIT_22
#define  AT32UC3C_USBC_USBCON_UIDE                  DEF_BIT_24
#define  AT32UC3C_USBC_USBCON_UIMOD                 DEF_BIT_25

#define  AT32UC3C_USBC_USBCON_UIMOD_DEV             DEF_BIT_25
#define  AT32UC3C_USBC_USBCON_UIMOD_HOST            DEF_BIT_NONE

#define  AT32UC3C_USBC_USBSTA_IDTI                  DEF_BIT_00
#define  AT32UC3C_USBC_USBSTA_VBUSTI                DEF_BIT_01
#define  AT32UC3C_USBC_USBSTA_SRPI                  DEF_BIT_02
#define  AT32UC3C_USBC_USBSTA_VBERRI                DEF_BIT_03
#define  AT32UC3C_USBC_USBSTA_BCERRI                DEF_BIT_04
#define  AT32UC3C_USBC_USBSTA_ROLEEXI               DEF_BIT_05
#define  AT32UC3C_USBC_USBSTA_HNPERRI               DEF_BIT_06
#define  AT32UC3C_USBC_USBSTA_STOI                  DEF_BIT_07
#define  AT32UC3C_USBC_USBSTA_VBUSRQ                DEF_BIT_08
#define  AT32UC3C_USBC_USBSTA_ID                    DEF_BIT_09
#define  AT32UC3C_USBC_USBSTA_VBUS                  DEF_BIT_10
#define  AT32UC3C_USBC_USBSTA_SPEED_MASK            DEF_BIT_FIELD_32(2, 12)
#define  AT32UC3C_USBC_USBSTA_CLKUSABLE             DEF_BIT_10

#define  AT32UC3C_USBC_USBSTA_SPEED_FULL            DEF_BIT_NONE
#define  AT32UC3C_USBC_USBSTA_SPEED_LOW             DEF_BIT_13

#define  AT32UC3C_USBC_EP_DESC_BYTE_CNT_MASK        DEF_BIT_FIELD_32(15, 0)
#define  AT32UC3C_USBC_EP_DESC_STALL_REQ_NEXT       DEF_BIT_00
#define  AT32UC3C_USBC_EP_DESC_CRC_ERR              DEF_BIT_16
#define  AT32UC3C_USBC_EP_DESC_OVRF                 DEF_BIT_17
#define  AT32UC3C_USBC_EP_DESC_UNDF                 DEF_BIT_18


/*
*********************************************************************************************************
*                                 AT32UC3C OTG USB DEVICE CONSTRAINTS
*********************************************************************************************************
*/

#define  AT32UC3C_NBR_EPS                                 7u      /* Maximum number of endpoints.                       */
#define  AT32UC3C_MAX_XFER_LEN                        32767u      /* Maximum transfer length supported by hw.           */
#define  AT32UC3C_MAX_TIMEOUT                          4095u


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                    AT32UC3C REGISTERS DATA TYPE
*********************************************************************************************************
*/

typedef  struct  usbd_at32uc3c_reg {
                                                                /* ------- DEVICE CONTROL AND STATUS REGISTERS -------- */
    CPU_REG32  UDCON;                                           /* Device general control.                              */
    CPU_REG32  UDINT;                                           /* Device global interrupt.                             */
    CPU_REG32  UDINTCLR;                                        /* Device global interrupt clear.                       */
    CPU_REG32  UDINTSET;                                        /* Device global interrupt set.                         */
    CPU_REG32  UDINTE;                                          /* Device global interrupt enable.                      */
    CPU_REG32  UDINTECLR;                                       /* Device global interrupt enable clear.                */
    CPU_REG32  UDINTESET;                                       /* Device global interrupt enable set.                  */
    CPU_REG32  UERST;                                           /* Endpoint enable/reset.                               */
    CPU_REG32  UDFNUM;                                          /* Device frame number.                                 */
    CPU_REG32  RSVD00[55];
    CPU_REG32  UECFGN[AT32UC3C_NBR_EPS];                        /* Endpoint n config.                                   */
    CPU_REG32  RSVD01[5];
    CPU_REG32  UESTAN[AT32UC3C_NBR_EPS];                        /* Endpoint n status.                                   */
    CPU_REG32  RSVD02[5];
    CPU_REG32  UESTANCLR[AT32UC3C_NBR_EPS];                     /* Endpoint n status clear.                             */
    CPU_REG32  RSVD03[5];
    CPU_REG32  UESTANSET[AT32UC3C_NBR_EPS];                     /* Endpoint n status set.                               */
    CPU_REG32  RSVD04[5];
    CPU_REG32  UECONN[AT32UC3C_NBR_EPS];                        /* Endpoint n control.                                  */
    CPU_REG32  RSVD05[5];
    CPU_REG32  UECONNSET[AT32UC3C_NBR_EPS];                     /* Endpoint n control clear.                            */
    CPU_REG32  RSVD06[5];
    CPU_REG32  UECONNCLR[AT32UC3C_NBR_EPS];                     /* Endpoint n control set.                              */
    CPU_REG32  RSVD07[113];

                                                                /* --- HOST CONTROL AND STATUS REGISTERS (NOT USED) --- */
    CPU_REG32  UHCON;                                           /* Host general control.                                */
    CPU_REG32  UHINT;                                           /* Host global interrupt.                               */
    CPU_REG32  UHINTCLR;                                        /* Host global interrupt clear.                         */
    CPU_REG32  UHINTSET;                                        /* Host global interrupt set.                           */
    CPU_REG32  UHINTE;                                          /* Host global interrupt enable.                        */
    CPU_REG32  UHINTECLR;                                       /* Host global interrupt enable clear.                  */
    CPU_REG32  UHINTESET;                                       /* Host global interrupt enable set.                    */
    CPU_REG32  UPRST;                                           /* Pipe enable/reset.                                   */
    CPU_REG32  UHFNUM;                                          /* Host frame number.                                   */
    CPU_REG32  RSVD08[55];
    CPU_REG32  UPCFGN[AT32UC3C_NBR_EPS];                        /* Pipe n configuration.                                */
    CPU_REG32  RSVD09[5];
    CPU_REG32  UPSTAN[AT32UC3C_NBR_EPS];                        /* Pipe n status.                                       */
    CPU_REG32  RSVD10[5];
    CPU_REG32  UPSTANCLR[AT32UC3C_NBR_EPS];                     /* Pipe n status clear.                                 */
    CPU_REG32  RSVD11[5];
    CPU_REG32  UPSTANSET[AT32UC3C_NBR_EPS];                     /* Pipe n status set.                                   */
    CPU_REG32  RSVD12[5];
    CPU_REG32  UPCONN[AT32UC3C_NBR_EPS];                        /* Pipe n control.                                      */
    CPU_REG32  RSVD13[5];
    CPU_REG32  UPCONNSET[AT32UC3C_NBR_EPS];                     /* Pipe n control set.                                  */
    CPU_REG32  RSVD14[5];
    CPU_REG32  UPCONNCLR[AT32UC3C_NBR_EPS];                     /* Pipe n control clear.                                */
    CPU_REG32  RSVD15[5];
    CPU_REG32  UPINRQN[AT32UC3C_NBR_EPS];                       /* Pipe n IN request.                                   */
    CPU_REG32  RSVD16[101];

                                                                /* ------- GLOBAL CONTROL AND STATUS REGISTERS -------- */
    CPU_REG32  USBCON;                                          /* General control.                                     */
    CPU_REG32  USBSTA;                                          /* General status.                                      */
    CPU_REG32  USBSTACLR;                                       /* General status clear.                                */
    CPU_REG32  USBSTASET;                                       /* General status set.                                  */
    CPU_REG32  RSVD17[2];
    CPU_REG32  UVERS;                                           /* IP version.                                          */
    CPU_REG32  UFEATURES;                                       /* IP features.                                         */
    CPU_REG32  UADDRSIZE;                                       /* IP PB address size.                                  */
    CPU_REG32  UNAME1;                                          /* IP name register 1.                                  */
    CPU_REG32  UNAME2;                                          /* IP name register 2.                                  */
    CPU_REG32  USBFSM;                                          /* USB finite state machine status.                     */
    CPU_REG32  UDESC;                                           /* USB descriptor address.                              */
} USBD_AT32UC3C_REG;


/*
********************************************************************************************************
*                                 AT32UC3C BANK DESCRIPTOR DATA TYPE
********************************************************************************************************
*/

typedef  struct  usbd_at32uc3c_desc_bank {
     CPU_INT32U  EP_BankAddr;                                   /* Buffer address.                                      */
     CPU_INT32U  EP_PktSize;                                    /* Pkt size information.                                */
     CPU_INT32U  EP_CtrStatus;                                  /* General status.                                      */
     CPU_INT32U  Rsvd;
} USBD_AT32UC3C_DESC_BANK;


/*
********************************************************************************************************
*                               AT32UC3C ENDPOINT DESCRIPTOR DATA TYPE
********************************************************************************************************
*/

typedef  struct usbd_at32uc3c_ep_desc {
    USBD_AT32UC3C_DESC_BANK  Bank0;
    USBD_AT32UC3C_DESC_BANK  Bank1;
} USBD_AT32UC3C_DESC_EP;


/*
*********************************************************************************************************
*                                          DRIVER DATA TYPE
*********************************************************************************************************
*/

typedef  struct  usbd_drv_data {
    USBD_AT32UC3C_DESC_EP  *EP_DescTblPtr;                      /* Endpoint descriptor table.                           */
    CPU_INT32U              EP_CtrlBuf[16u];                    /* Ctrl endpoint buffer.                                */
} USBD_DRV_DATA;


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
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
*                                  USB DEVICE CONTROLLER DRIVER API
*********************************************************************************************************
*/

USBD_DRV_API  USBD_DrvAPI_AT32UC3C = { USBD_DrvInit,
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
********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
********************************************************************************************************
*/


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
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  USBD_DrvInit (USBD_DRV  *p_drv,
                            USBD_ERR  *p_err)
{
    LIB_ERR            err_lib;
    CPU_SIZE_T         reqd_octets;
    USBD_DRV_DATA     *p_data;
    USBD_DRV_BSP_API  *p_bsp_api;


                                                                /* Allocate driver internal data.                       */
    p_data = (USBD_DRV_DATA *)Mem_HeapAlloc(sizeof(USBD_DRV_DATA),
                                            sizeof(CPU_DATA),
                                           &reqd_octets,
                                           &err_lib);
    if (p_data == (USBD_DRV_DATA *)0) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

                                                                /* Allocate EP desc. table                              */
    p_data->EP_DescTblPtr = (USBD_AT32UC3C_DESC_EP *)Mem_HeapAlloc(sizeof(USBD_AT32UC3C_DESC_EP) * AT32UC3C_NBR_EPS,
                                                                   32u,
                                                                  &reqd_octets,
                                                                  &err_lib);
    if (p_data->EP_DescTblPtr == (USBD_AT32UC3C_DESC_EP *)0) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

    p_bsp_api = p_drv->BSP_API_Ptr;
    if (p_bsp_api->Init != 0) {
        p_bsp_api->Init(p_drv);
    }

    p_drv->DataPtr = p_data;

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
*                               USBD_ERR_FAIL    Timeout occured when setting a value in register.
*
* Return(s)   : none.
*
* Note(s)     : Typically, the start function activates the pull-up on the D+ or the D- pin to simulate
*               attachment to host. Some MCUs/MPUs have an internal pull-down that is activated by a
*               device controller register; for others, this may be a GPIO pin. Additionally, interrupts
*               for reset and suspend are activated.
*********************************************************************************************************
*/

static  void  USBD_DrvStart (USBD_DRV  *p_drv,
                             USBD_ERR  *p_err)
{
    CPU_INT16U          reg_to;
    USBD_DRV_BSP_API   *p_bsp_api;
    USBD_AT32UC3C_REG  *p_reg;
    USBD_DRV_DATA      *p_data;
    CPU_SR_ALLOC();


    p_reg     = (USBD_AT32UC3C_REG *)p_drv->CfgPtr->BaseAddr;
    p_data    = (USBD_DRV_DATA     *)p_drv->DataPtr;
    p_bsp_api =  p_drv->BSP_API_Ptr;

    CPU_CRITICAL_ENTER();

    DEF_BIT_CLR(p_reg->USBCON, AT32UC3C_USBC_USBCON_USBE);

    if (p_bsp_api->Conn != 0) {
        p_bsp_api->Conn();
    }

    DEF_BIT_SET(p_reg->USBCON,                                  /* Force device mode.                                   */
                AT32UC3C_USBC_USBCON_UIMOD);

    DEF_BIT_CLR(p_reg->USBCON,                                  /* Clr OTG mode.                                        */
                AT32UC3C_USBC_USBCON_UIDE);

    DEF_BIT_CLR(p_reg->USBCON,
                AT32UC3C_USBC_USBCON_OTGPADE);

    DEF_BIT_SET(p_reg->USBCON,
                AT32UC3C_USBC_USBCON_OTGPADE);

    DEF_BIT_SET(p_reg->USBCON,                                  /* Enable USB transceiver                               */
                AT32UC3C_USBC_USBCON_USBE);

    Mem_Clr((void *)p_data->EP_DescTblPtr,
                    sizeof(USBD_AT32UC3C_DESC_EP) * AT32UC3C_NBR_EPS);

    DEF_BIT_CLR(p_reg->UDCON,                                   /* Set USB ctrl to FS.                                  */
                AT32UC3C_USBC_UDCON_LS);

    DEF_BIT_SET(p_reg->USBCON,                                  /* Enable VBUS transition interrupt.                    */
                AT32UC3C_USBC_USBCON_VBUSTE);
    CPU_CRITICAL_EXIT();

    DEF_BIT_CLR(p_reg->USBCON, AT32UC3C_USBC_USBCON_FRZCLK);    /* Enable clk.                                          */

    reg_to = AT32UC3C_MAX_TIMEOUT;                              /* Wait for clk to be ready.                            */
    while ((DEF_BIT_IS_CLR(p_reg->USBSTA, AT32UC3C_USBC_USBSTA_CLKUSABLE) == DEF_YES) &&
          (reg_to                                                          > 0)) {
        reg_to--;
    }
    if (reg_to == 0) {
       *p_err = USBD_ERR_FAIL;
        return;
    }

    DEF_BIT_CLR(p_reg->UDCON, AT32UC3C_USBC_UDCON_DETACH);      /* Connect to host.                                     */

    p_reg->UDINTESET = AT32UC3C_USBC_UDINT_EORST;               /* Enable interrupts.                                   */
    p_reg->UDINTESET = AT32UC3C_USBC_UDINT_SUSP;
    p_reg->UDINTESET = AT32UC3C_USBC_UDINT_WAKEUP;
    p_reg->UDINTCLR  = AT32UC3C_USBC_UDINT_EORST;
    p_reg->UDINTSET  = AT32UC3C_USBC_UDINT_SUSP;
    p_reg->UDINTCLR  = AT32UC3C_USBC_UDINT_WAKEUP;

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
    USBD_DRV_BSP_API   *p_bsp_api;
    USBD_AT32UC3C_REG  *p_reg;


    p_reg     = (USBD_AT32UC3C_REG *)p_drv->CfgPtr->BaseAddr;
    p_bsp_api =  p_drv->BSP_API_Ptr;

    if (p_bsp_api->Disconn != 0) {
        p_bsp_api->Disconn();
    }

    p_reg->UDINTECLR = 0xFFFFFFFF;
    p_reg->UDINTCLR  = 0xFFFFFFFF;

    DEF_BIT_SET(p_reg->UDCON,                                   /* Disconnect from USB host                             */
                AT32UC3C_USBC_UDCON_DETACH);

    DEF_BIT_CLR(p_reg->USBCON,                                  /* Disable USB transceiver                              */
                AT32UC3C_USBC_USBCON_USBE);
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
    USBD_AT32UC3C_REG  *p_reg;


    p_reg = (USBD_AT32UC3C_REG *)p_drv->CfgPtr->BaseAddr;

    p_reg->UDCON |= dev_addr;
    DEF_BIT_CLR(p_reg->UDCON, AT32UC3C_USBC_UDCON_ADDEN);

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
    USBD_AT32UC3C_REG  *p_reg;


    (void)dev_addr;

    p_reg = (USBD_AT32UC3C_REG *)p_drv->CfgPtr->BaseAddr;

    DEF_BIT_SET(p_reg->UDCON, AT32UC3C_USBC_UDCON_ADDEN);
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
    (void)p_drv;

    return (0u);
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
*                               USBD_ERR_INVALID_ARG        Invalid max_pkt_size value.
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
    CPU_BOOLEAN               ep_dir_in;
    CPU_INT08U                ep_log_nbr;
    CPU_INT32U                uecfgn_reg;
    USBD_DRV_DATA            *p_data;
    USBD_AT32UC3C_DESC_BANK  *p_bank_desc;
    USBD_AT32UC3C_REG        *p_reg;


    (void)transaction_frame;

    ep_dir_in   =  USBD_EP_IS_IN(ep_addr);
    ep_log_nbr  =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_data      = (USBD_DRV_DATA     *)p_drv->DataPtr;
    p_reg       = (USBD_AT32UC3C_REG *)p_drv->CfgPtr->BaseAddr;
    p_bank_desc = &p_data->EP_DescTblPtr[ep_log_nbr].Bank0;
    uecfgn_reg  =  DEF_BIT_NONE;

    switch (max_pkt_size) {
        case 8u:
             uecfgn_reg |= AT32UC3C_USBC_UECFGN_EPSIZE_8;
             break;

        case 16u:
             uecfgn_reg |= AT32UC3C_USBC_UECFGN_EPSIZE_16;
             break;

        case 32u:
             uecfgn_reg |= AT32UC3C_USBC_UECFGN_EPSIZE_32;
             break;

        case 64u:
             uecfgn_reg |= AT32UC3C_USBC_UECFGN_EPSIZE_64;
             break;

        case 128u:
             uecfgn_reg |= AT32UC3C_USBC_UECFGN_EPSIZE_128;
             break;

        case 256u:
             uecfgn_reg |= AT32UC3C_USBC_UECFGN_EPSIZE_256;
             break;

        case 512u:
             uecfgn_reg |= AT32UC3C_USBC_UECFGN_EPSIZE_512;
             break;

        case 1024u:
             uecfgn_reg |= AT32UC3C_USBC_UECFGN_EPSIZE_1024;
             break;

        default:
            *p_err = USBD_ERR_INVALID_ARG;
             return;
    }

    if (ep_log_nbr != 0u) {
          switch (ep_type) {
            case USBD_EP_TYPE_CTRL:
                 uecfgn_reg |= AT32UC3C_USBC_UECFGN_EPTYPE_CTRL;
                 break;

            case USBD_EP_TYPE_ISOC:
                 uecfgn_reg |= AT32UC3C_USBC_UECFGN_EPTYPE_ISOC;
                 break;

            case USBD_EP_TYPE_BULK:
                 uecfgn_reg |= AT32UC3C_USBC_UECFGN_EPTYPE_BULK;
                 break;

            case USBD_EP_TYPE_INTR:
                 uecfgn_reg |= AT32UC3C_USBC_UECFGN_EPTYPE_INTR;
                 break;

            default:
                 break;
        }

        uecfgn_reg |= AT32UC3C_USBC_UECFGN_EPBK_SINGLE;

        if (ep_dir_in == DEF_YES) {
            uecfgn_reg |= AT32UC3C_USBC_UECFGN_EPDIR_IN;
        } else {
            uecfgn_reg |= AT32UC3C_USBC_UECFGN_EPDIR_OUT;
        }

        p_reg->UECFGN[ep_log_nbr] = uecfgn_reg;
    } else {
        p_reg->UECFGN[0u] = (uecfgn_reg                       |
                             AT32UC3C_USBC_UECFGN_EPTYPE_CTRL |
                             AT32UC3C_USBC_UECFGN_EPBK_SINGLE);

        p_bank_desc->EP_BankAddr = (CPU_INT32U)&p_data->EP_CtrlBuf[0];
        p_reg->UECONNCLR[0u]     =  AT32UC3C_USBC_UECONN_BUSY0E;
        p_reg->UECONNSET[0u]     =  AT32UC3C_USBC_UECONN_RXSTPE_ERRORFE;
        p_reg->UECONNCLR[0u]     =  AT32UC3C_USBC_UECONN_TXINE;
        p_reg->UECONNCLR[0u]     =  AT32UC3C_USBC_UECONN_RXOUTE;
    }

    p_reg->UECONNSET[ep_log_nbr] = AT32UC3C_USBC_UECONN_BUSY0E;

    p_bank_desc->EP_PktSize   = 0u;
    p_bank_desc->EP_CtrStatus = 0u;

    DEF_BIT_SET(p_reg->UDINTESET,                               /* Unmask EP int.                                       */
                DEF_BIT(ep_log_nbr) << 12u);

    DEF_BIT_SET(p_reg->UERST, DEF_BIT(ep_log_nbr));             /* Enable EP.                                           */

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
    CPU_INT08U          ep_log_nbr;
    USBD_AT32UC3C_REG  *p_reg;


    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_reg      = (USBD_AT32UC3C_REG *)p_drv->CfgPtr->BaseAddr;

    if (ep_log_nbr != 0u) {
        DEF_BIT_CLR(p_reg->UERST, DEF_BIT(ep_log_nbr));         /* Disable EP.                                          */
    }
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
* Return(s)   : none.
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
    CPU_INT08U                ep_log_nbr;
    USBD_DRV_DATA            *p_data;
    USBD_AT32UC3C_DESC_BANK  *p_bank_desc;
    USBD_AT32UC3C_REG        *p_reg;
    CPU_SR_ALLOC();


    ep_log_nbr  =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_reg       = (USBD_AT32UC3C_REG *)p_drv->CfgPtr->BaseAddr;
    p_data      = (USBD_DRV_DATA     *)p_drv->DataPtr;
    p_bank_desc = &p_data->EP_DescTblPtr[ep_log_nbr].Bank0;

    if (ep_log_nbr != 0u) {                                     /* Prepare bank descriptor.                             */
        p_bank_desc->EP_BankAddr = (CPU_INT32U)p_buf;
        p_bank_desc->EP_PktSize   = (CPU_INT32U)((CPU_INT16U)buf_len << 16u);
        DEF_BIT_CLR(p_bank_desc->EP_PktSize, DEF_BIT_31);
    } else {
        p_bank_desc->EP_PktSize = 0u;
    }
    p_bank_desc->EP_CtrStatus =  0u;

    CPU_CRITICAL_ENTER();
    p_reg->UECONNCLR[ep_log_nbr] = AT32UC3C_USBC_UECONN_BUSY0E;
    p_reg->UECONNSET[ep_log_nbr] = AT32UC3C_USBC_UECONN_RXOUTE;
    CPU_CRITICAL_EXIT();

   *p_err = USBD_ERR_NONE;

    return (DEF_MIN(buf_len, AT32UC3C_MAX_XFER_LEN));
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
*                               USBD_ERR_NONE           Data successfully received.
*                               USBD_ERR_RX             Generic Rx error.
*                               USBD_ERR_SHORT_XFER     Short transfer detected.
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
    CPU_INT08U                ep_log_nbr;
    CPU_INT32U                xfer_len;
    USBD_DRV_DATA            *p_data;
    USBD_AT32UC3C_DESC_BANK  *p_bank_desc;
    USBD_AT32UC3C_REG        *p_reg;


    ep_log_nbr  =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_reg       = (USBD_AT32UC3C_REG *)p_drv->CfgPtr->BaseAddr;
    p_data      = (USBD_DRV_DATA     *)p_drv->DataPtr;
    p_bank_desc = &p_data->EP_DescTblPtr[ep_log_nbr].Bank0;
    xfer_len    = (p_bank_desc->EP_PktSize & AT32UC3C_USBC_EP_DESC_BYTE_CNT_MASK);

    if (ep_log_nbr == 0u) {
        if (xfer_len > 0u) {                                    /* Copy data from ctrl EP dedicated buf.                */
            Mem_Copy((void *) p_buf,
                     (void *)&p_data->EP_CtrlBuf[0],
                              xfer_len);
        }
    } else {
        p_reg->UECONNCLR[ep_log_nbr] = AT32UC3C_USBC_UECONN_FIFOCON;
    }

    if ((DEF_BIT_IS_SET(p_bank_desc->EP_CtrStatus, AT32UC3C_USBC_EP_DESC_OVRF) == DEF_YES) ||
        (xfer_len                                                               > buf_len)) {
       *p_err = USBD_ERR_RX;
    } else if (DEF_BIT_IS_SET(p_bank_desc->EP_CtrStatus, AT32UC3C_USBC_EP_DESC_UNDF) == DEF_YES) {
       *p_err = USBD_ERR_NONE;                                  /* #### To fix when xfer multiple of max pkt size & ... */
                                                                /* ...ZLP used.                                         */
    } else {
       *p_err = USBD_ERR_NONE;
    }

    return (xfer_len);
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
    CPU_INT08U          ep_log_nbr;
    USBD_AT32UC3C_REG  *p_reg;


    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_reg      = (USBD_AT32UC3C_REG *)p_drv->CfgPtr->BaseAddr;

    if (ep_log_nbr != 0u) {
        p_reg->UECONNCLR[ep_log_nbr] = AT32UC3C_USBC_UECONN_FIFOCON;
    }

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
    CPU_INT08U                ep_log_nbr;
    CPU_INT32U                xfer_len;
    USBD_DRV_DATA            *p_data;
    USBD_AT32UC3C_DESC_BANK  *p_bank_desc;


    ep_log_nbr  =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_data      = (USBD_DRV_DATA *)p_drv->DataPtr;
    p_bank_desc = &p_data->EP_DescTblPtr[ep_log_nbr].Bank0;
    xfer_len    =  DEF_MIN(buf_len, AT32UC3C_MAX_XFER_LEN);

                                                                /* Prepare bank descriptor.                             */
    if (ep_log_nbr == 0u) {                                     /* Copy data to ctrl EP dedicated buf.                  */
        xfer_len = DEF_MIN(xfer_len, 8u);
        Mem_Copy((void *)p_data->EP_CtrlBuf,
                 (void *)p_buf,
                         xfer_len);
    } else {
        p_bank_desc->EP_BankAddr = (CPU_INT32U)p_buf;
    }

    p_bank_desc->EP_PktSize = xfer_len;

   *p_err = USBD_ERR_NONE;

    return (xfer_len);
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
    CPU_INT08U          ep_log_nbr;
    USBD_AT32UC3C_REG  *p_reg;
    CPU_SR_ALLOC();


    (void)p_buf;
    (void)buf_len;

    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_reg      = (USBD_AT32UC3C_REG *)p_drv->CfgPtr->BaseAddr;

    CPU_CRITICAL_ENTER();
    if (ep_log_nbr != 0u) {
        p_reg->UECONNCLR[ep_log_nbr] = AT32UC3C_USBC_UECONN_FIFOCON;
    }

    p_reg->UECONNCLR[ep_log_nbr] = AT32UC3C_USBC_UECONN_BUSY0E;
    p_reg->UESTANCLR[ep_log_nbr] = AT32UC3C_USBC_UESTAN_TXINI;
    p_reg->UECONNSET[ep_log_nbr] = AT32UC3C_USBC_UECONN_TXINE;
    CPU_CRITICAL_EXIT();

   *p_err = USBD_ERR_NONE;
}


/*********************************************************************************************************
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
    (void)USBD_DrvEP_Tx(              p_drv,
                                      ep_addr,
                        (CPU_INT08U *)0,
                                      0u,
                                      p_err);

    USBD_DrvEP_TxStart(              p_drv,
                                     ep_addr,
                       (CPU_INT08U *)0,
                                     0u,
                                     p_err);
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
    CPU_INT08U          ep_log_nbr;
    USBD_AT32UC3C_REG  *p_reg;
    CPU_SR_ALLOC();


    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_reg      = (USBD_AT32UC3C_REG *)p_drv->CfgPtr->BaseAddr;

    CPU_CRITICAL_ENTER();
    p_reg->UECONNSET[ep_log_nbr] = AT32UC3C_USBC_UECONN_BUSY0E;

                                                                /* Chk if an OUT xfer is in progress.                   */
    if (DEF_BIT_IS_SET(p_reg->UECONN[ep_log_nbr], AT32UC3C_USBC_UECONN_RXOUTE) == DEF_YES) {
        p_reg->UESTANCLR[ep_log_nbr] = AT32UC3C_USBC_UESTAN_RXOUTI;
        p_reg->UECONNCLR[ep_log_nbr] = AT32UC3C_USBC_UECONN_RXOUTE;
        p_reg->UECONNCLR[ep_log_nbr] = AT32UC3C_USBC_UECONN_FIFOCON;
    }
    CPU_CRITICAL_EXIT();

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
*                           DEF_SET     Stall endpoint.
*                           DEF_CLR     Clear endpoint stall condition.
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
    CPU_INT08U          ep_log_nbr;
    USBD_AT32UC3C_REG  *p_reg;


    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_reg      = (USBD_AT32UC3C_REG *)p_drv->CfgPtr->BaseAddr;

    if (state == DEF_SET) {
        p_reg->UECONNSET[ep_log_nbr] = AT32UC3C_USBC_UECONN_STALLRQ;
    } else {
                                                                /* Reset data toggle.                                   */
        p_reg->UECONNSET[ep_log_nbr] = AT32UC3C_USBC_UECONN_RSTDT;
        p_reg->UECONNCLR[ep_log_nbr] = AT32UC3C_USBC_UECONN_STALLRQ;
    }

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
* Note(s)     : (1) In interrupt handlers one usually wants to make sure that the interrupt request has
*                   been cleared before executing the return instruction, otherwise the same interrupt
*                   may be serviced immediately after executing the return instruction. A data memory
*                   barrier must be inserted between the code clearing the interrupt request and the
*                   return instruction. For more details, See section 6.6 of ATMEL AVR32UC technical
*                   reference manual (doc32002).
*********************************************************************************************************
*/

static  void  USBD_DrvISR_Handler (USBD_DRV  *p_drv)
{
    CPU_INT08U          ep_log_nbr;
    CPU_INT32U          udint_reg;
    CPU_INT32U          ep_int_bitmap;
    CPU_INT32U          test;
    USBD_DRV_DATA      *p_data;
    USBD_AT32UC3C_REG  *p_reg;


    p_reg      = (USBD_AT32UC3C_REG *)p_drv->CfgPtr->BaseAddr;
    p_data     = (USBD_DRV_DATA     *)p_drv->DataPtr;
    udint_reg  = (CPU_INT32U         )p_reg->UDINT;
    udint_reg &=  p_reg->UDINTE;

                                                                /* ------------- VBUS TRANSITION DETECTED ------------- */
    if (DEF_BIT_IS_SET(p_reg->USBSTA, AT32UC3C_USBC_USBSTA_VBUSTI) == DEF_YES) {

        DEF_BIT_CLR(p_reg->USBCON, AT32UC3C_USBC_USBCON_FRZCLK);
        p_reg->USBSTACLR = AT32UC3C_USBC_USBSTA_VBUSTI;

        if (DEF_BIT_IS_CLR(p_reg->USBSTA, AT32UC3C_USBC_USBSTA_VBUS) == DEF_YES) {
            USBD_EventDisconn(p_drv);
        }
    }

                                                                /* ------------------ END OF RESET -------------------- */
    if (DEF_BIT_IS_SET(udint_reg, AT32UC3C_USBC_UDINT_EORST) == DEF_YES) {

        p_reg->UDINTCLR = AT32UC3C_USBC_UDINT_EORST;

        USBD_EventReset(p_drv);                                 /* Notify bus reset.                                    */

        p_reg->UDESC = (CPU_INT32U)p_data->EP_DescTblPtr;
    }

                                                                /* --------------------- SUSPEND ---------------------- */
    if (DEF_BIT_IS_SET(udint_reg, AT32UC3C_USBC_UDINT_SUSP) == DEF_YES) {

        p_reg->UDINTECLR = AT32UC3C_USBC_UDINT_SUSP;
        p_reg->UDINTESET = AT32UC3C_USBC_UDINT_WAKEUP;

        USBD_EventSuspend(p_drv);
    }

                                                                /* ------------------ START OF FRAME ------------------ */
    if (DEF_BIT_IS_SET(udint_reg, AT32UC3C_USBC_UDINT_SOF) == DEF_YES) {

        p_reg->UDINTCLR = AT32UC3C_USBC_UDINT_SOF;
    }

                                                                /* ------------ UPSTREAM RESUME INTERRUPT ------------- */
    if (DEF_BIT_IS_SET(udint_reg, AT32UC3C_USBC_UDINT_UPRSM) == DEF_YES) {

        p_reg->UDINTCLR = AT32UC3C_USBC_UDINT_UPRSM;
    }

                                                                /* ------------------ END OF RESUME ------------------- */
    if (DEF_BIT_IS_SET(udint_reg, AT32UC3C_USBC_UDINT_EORSM) == DEF_YES) {

        p_reg->UDINTCLR  = AT32UC3C_USBC_UDINT_EORSM;
    }

                                                                /* --------------------- WAKE-UP ---------------------- */
    if (DEF_BIT_IS_SET(udint_reg, AT32UC3C_USBC_UDINT_WAKEUP) == DEF_YES) {

        p_reg->UDINTECLR = AT32UC3C_USBC_UDINT_WAKEUP;
        p_reg->UDINTESET = AT32UC3C_USBC_UDINT_SUSP;

        USBD_EventResume(p_drv);
    }

                                                                /* ----------------- ENDPOINT EVENTS ------------------ */
    ep_int_bitmap = (udint_reg >> 12u) & 0x000001FF;
    while (ep_int_bitmap != DEF_BIT_NONE) {
        ep_log_nbr = CPU_CntTrailZeros32(ep_int_bitmap);
                                                                /* Rx complete int.                                     */
        if (DEF_BIT_IS_SET(p_reg->UESTAN[ep_log_nbr], AT32UC3C_USBC_UESTAN_RXOUTI) == DEF_YES) {

            p_reg->UECONNSET[ep_log_nbr] = AT32UC3C_USBC_UECONN_BUSY0E;
            p_reg->UESTANCLR[ep_log_nbr] = AT32UC3C_USBC_UESTAN_RXOUTI;
            p_reg->UECONNCLR[ep_log_nbr] = AT32UC3C_USBC_UECONN_RXOUTE;

            USBD_EP_RxCmpl(p_drv, ep_log_nbr);
        }
                                                                /* Tx complete int.                                     */
        if (DEF_BIT_IS_SET(p_reg->UESTAN[ep_log_nbr], AT32UC3C_USBC_UESTAN_TXINI) == DEF_YES) {

            p_reg->UECONNSET[ep_log_nbr] = AT32UC3C_USBC_UECONN_BUSY0E;
            p_reg->UECONNCLR[ep_log_nbr] = AT32UC3C_USBC_UECONN_TXINE;

            USBD_EP_TxCmpl(p_drv, ep_log_nbr);
        }
                                                                /* Setup pkt recv'd.                                    */
        if ((DEF_BIT_IS_SET(p_reg->UESTAN[ep_log_nbr], AT32UC3C_USBC_UESTAN_RXSTPI_ERRORFI) == DEF_YES) &&
            (ep_log_nbr                                                                     == 0u)) {

            p_reg->UESTANCLR[ep_log_nbr] = AT32UC3C_USBC_UESTAN_RXSTPI_ERRORFI;

            USBD_EventSetup(               p_drv,
                            (CPU_INT08U *)&p_data->EP_CtrlBuf[0u]);
        }

        DEF_BIT_CLR(ep_int_bitmap, DEF_BIT(ep_log_nbr));
    }

    test = p_reg->UVERS;                                        /* Memory barrier. (See note #1)                        */
    (void)test;
    return;
}
