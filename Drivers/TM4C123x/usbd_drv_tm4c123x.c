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
*                              Texas Instruments Tiva C Series USB-OTG
*
* Filename : usbd_drv_tm4c123x.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)  : (1) With an appropriate BSP, this device driver will support the USB-OTG device module
*                on the entire Texas Instruments Tiva C and MSP432E families.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <Source/usbd_core.h>
#include  "usbd_drv_tm4c123x.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  TM4C123X_DBG_STATS_EN                      DEF_DISABLED
#define  TM4C123X_DBG_TRAPS_EN                      DEF_DISABLED

#define  TM4C123X_POWER_SOFTCONN                    DEF_BIT_06
#define  TM4C123X_POWER_PWRDNPHY                    DEF_BIT_00

#define  TM4C123X_USBTXIS_EP0                       DEF_BIT_00

#define  TM4C123X_USBTXIE_EP0                       DEF_BIT_00
#define  TM4C123X_USBTXIE_EP1                       DEF_BIT_01
#define  TM4C123X_USBTXIE_EP2                       DEF_BIT_02
#define  TM4C123X_USBTXIE_EP3                       DEF_BIT_03
#define  TM4C123X_USBTXIE_EP4                       DEF_BIT_04
#define  TM4C123X_USBTXIE_EP5                       DEF_BIT_05
#define  TM4C123X_USBTXIE_EP6                       DEF_BIT_06
#define  TM4C123X_USBTXIE_EP7                       DEF_BIT_07
#define  TM4C123X_USBTXIE_EP_ALL                    DEF_BIT_FIELD (8u, 0u)
#define  TM4C123X_USBTXIE_EP1_TO_EP7                DEF_BIT_FIELD (7u, 1u)

#define  TM4C123X_USBRXIE_EP0                       DEF_BIT_00
#define  TM4C123X_USBRXIE_EP1                       DEF_BIT_01
#define  TM4C123X_USBRXIE_EP2                       DEF_BIT_02
#define  TM4C123X_USBRXIE_EP3                       DEF_BIT_03
#define  TM4C123X_USBRXIE_EP4                       DEF_BIT_04
#define  TM4C123X_USBRXIE_EP5                       DEF_BIT_05
#define  TM4C123X_USBRXIE_EP6                       DEF_BIT_06
#define  TM4C123X_USBRXIE_EP7                       DEF_BIT_07
#define  TM4C123X_USBRXIE_EP_ALL                    DEF_BIT_FIELD (8u, 0u)
#define  TM4C123X_USBRXIE_EP1_TO_EP7                DEF_BIT_FIELD (7u, 1u)

#define  TM4C123X_USBI_SUSPEND                      DEF_BIT_00
#define  TM4C123X_USBI_RESUME                       DEF_BIT_01
#define  TM4C123X_USBI_RESET                        DEF_BIT_02
#define  TM4C123X_USBI_SOF                          DEF_BIT_03
#define  TM4C123X_USBI_DISCON                       DEF_BIT_05
#define  TM4C123X_USBI_EVENTS_ALL                  (TM4C123X_USBI_SUSPEND | \
                                                    TM4C123X_USBI_RESUME  | \
                                                    TM4C123X_USBI_RESET   | \
                                                    TM4C123X_USBI_DISCON)

#define  TM4C123X_USBFRAME_FRAME                    DEF_BIT_FIELD(11u, 0u)

#define  TM4C123X_USBEPIDX_EPIDX                    DEF_BIT_FIELD(4u, 0u)

#define  TM4C123X_USBTEST_FORCEFS                   DEF_BIT_05

                                                                /* TX and RX FIFO sizes.                                */
#define  TM4C123X_USBFIFOSZ_DPB                     DEF_BIT_04
#define  TM4C123X_USBFIFOSZ_SIZE                    DEF_BIT_FIELD(4u, 0u)
#define  TM4C123X_USBFIFOSZ_SIZE_8                  0x0u
#define  TM4C123X_USBFIFOSZ_SIZE_16                 0x1u
#define  TM4C123X_USBFIFOSZ_SIZE_32                 0x2u
#define  TM4C123X_USBFIFOSZ_SIZE_64                 0x3u
#define  TM4C123X_USBFIFOSZ_SIZE_128                0x4u
#define  TM4C123X_USBFIFOSZ_SIZE_256                0x5u
#define  TM4C123X_USBFIFOSZ_SIZE_512                0x6u
#define  TM4C123X_USBFIFOSZ_SIZE_1024               0x7u
#define  TM4C123X_USBFIFOSZ_SIZE_2048               0x8u

#define  TM4C123X_USBFIFOADD_ADDR                   DEF_BIT_FIELD(9u, 0u)
#define  TM4C123X_USBFIFOADD_ADDR_0                 0x0u
#define  TM4C123X_USBFIFOADD_ADDR_64                0x8u
#define  TM4C123X_USBFIFOADD_ADDR_1024              0x128u

#define  TM4C123X_USBCSRL0_RXRDY                    DEF_BIT_00
#define  TM4C123X_USBCSRL0_TXRDY                    DEF_BIT_01
#define  TM4C123X_USBCSRL0_STALLED                  DEF_BIT_02
#define  TM4C123X_USBCSRL0_DATAEND                  DEF_BIT_03
#define  TM4C123X_USBCSRL0_SETEND                   DEF_BIT_04
#define  TM4C123X_USBCSRL0_STALL                    DEF_BIT_05
#define  TM4C123X_USBCSRL0_RXRDYC                   DEF_BIT_06
#define  TM4C123X_USBCSRL0_SETENDC                  DEF_BIT_07

#define  TM4C123X_USBCSRH0_FLUSH                    DEF_BIT_00

                                                                /* Endpoint Maximum TX and RX data.                     */
#define  TM4C123X_USBMAXPN_MAXLOAD                  DEF_BIT_FIELD(11u, 0u)

#define  TM4C123X_USBTXCSRLX_TXRDY                  DEF_BIT_00
#define  TM4C123X_USBTXCSRLX_UNDRN                  DEF_BIT_02
#define  TM4C123X_USBTXCSRLX_FLUSH                  DEF_BIT_03
#define  TM4C123X_USBTXCSRLX_STALL                  DEF_BIT_04
#define  TM4C123X_USBTXCSRLX_STALLED                DEF_BIT_05
#define  TM4C123X_USBTXCSRLX_CLRDT                  DEF_BIT_06

#define  TM4C123X_USBTXCSRHX_DMAEN                  DEF_BIT_04

#define  TM4C123X_USBRXCSRLX_RXRDY                  DEF_BIT_00
#define  TM4C123X_USBRXCSRLX_FULL                   DEF_BIT_01
#define  TM4C123X_USBRXCSRLX_OVER                   DEF_BIT_02
#define  TM4C123X_USBRXCSRLX_DATAERR                DEF_BIT_03
#define  TM4C123X_USBRXCSRLX_FLUSH                  DEF_BIT_04
#define  TM4C123X_USBRXCSRLX_STALL                  DEF_BIT_05
#define  TM4C123X_USBRXCSRLX_STALLED                DEF_BIT_06
#define  TM4C123X_USBRXCSRLX_CLRDT                  DEF_BIT_07

#define  TM4C123X_USBCSRHX_MODE                     DEF_BIT_05
#define  TM4C123X_USBCSRHX_ISO                      DEF_BIT_06

#define  TM4C123X_USBRXCSRHX_DMAEN                  DEF_BIT_05

#define  TM4C123X_USBGPCS_DEVMODOTG                 DEF_BIT_01
#define  TM4C123X_USBGPCS_DEVMOD                    DEF_BIT_00

#define  MSP432E_USBGPCS_DEVMOD                     DEF_BIT_FIELD(3u, 0u)

#define  MSP432E_USBCC_CLKDIV                       DEF_BIT_FIELD(4u, 0u)
#define  MSP432E_USBCC_CLKDIV_1                     DEF_BIT_MASK(0u, 0u)
#define  MSP432E_USBCC_CLKDIV_2                     DEF_BIT_MASK(1u, 0u)
#define  MSP432E_USBCC_CLKDIV_3                     DEF_BIT_MASK(2u, 0u)
#define  MSP432E_USBCC_CLKDIV_4                     DEF_BIT_MASK(3u, 0u)
#define  MSP432E_USBCC_CSD                          DEF_BIT_08
#define  MSP432E_USBCC_CLKEN                        DEF_BIT_09

#define  MSP432E_USBPC_ULPIEN                       DEF_BIT_16


/*
*********************************************************************************************************
*                                   TM4C123X USB DEVICE CONSTRAINTS
*********************************************************************************************************
*/

#define  TM4C123X_NBR_EPS                                8u     /* Maximum number of endpoints.                         */
#define  TM4C123X_MAX_PKT_SIZE                          64u

#define  TM4C123X_RX_ISR                            DEF_BIT_00
#define  TM4C123X_RX_START_FNCT                     DEF_BIT_01


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            LOCAL MACROS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          EP0 STATE MACHINE
*
* Note(s) : (1) The state of endpoint 0 must be kept by software, the hardware does not provide any mean
*               to determine what caused the interrupt on endpoint 0. The flow of the state machine is
*               the following:
*
*                   OUT Data Transfer:
*
*                       Setup       ISR:SetupCmpl() [IDLE/WAIT_FOR_RX_ZLP_CALL -> WAIT_FOR_RX_CALL]
*
*                       Data OUT    RxStart()    [WAIT_FOR_RX_CALL -> WAIT_FOR_RX_CMPL]     ->
*                                   ISR:RxCmpl() [WAIT_FOR_RX_CMPL -> WAIT_FOR_TX_ZLP_CALL] ->
*                                   Rx()
*
*            (Optional) Data OUT    RxStart()    [WAIT_FOR_TX_ZLP_CALL -> WAIT_FOR_RX_CMPL]     ->
*                                   ISR:RxCmpl() [WAIT_FOR_RX_CMPL     -> WAIT_FOR_TX_ZLP_CALL] ->
*                                   Rx()
*
*                       Status IN   TxZLP()      [WAIT_FOR_TX_ZLP_CALL -> WAIT_FOR_TX_ZLP_CMPL] ->
*                                   ISR:TxCmpl() [WAIT_FOR_TX_ZLP_CMPL -> STATUS -> IDLE]
*
*
*                   IN Data Transfer:
*
*                       Setup       ISR:SetupCmpl() [IDLE/WAIT_FOR_RX_ZLP_CALL -> WAIT_FOR_TX_CALL]
*
*                       Data IN     Tx() ->
*                                   TxStart()    [WAIT_FOR_TX_CALL -> WAIT_FOR_TX_CMPL] ->
*                                   ISR:TxCmpl() [WAIT_FOR_TX_CMPL -> WAIT_FOR_RX_ZLP_CALL]
*
*            (Optional) Data IN     Tx() ->
*                                   TxStart()    [WAIT_FOR_TX_ZLP_CALL -> WAIT_FOR_TX_CMPL] ->
*                                   ISR:TxCmpl() [WAIT_FOR_TX_CMPL     -> WAIT_FOR_RX_ZLP_CALL]
*
*                       Status OUT  RxStart(): simulated ISR:RxCmpl() ->
*                                   RxZLP()
*
*
*                   IN No-Data Transfer
*
*                       Setup       ISR:SetupCmpl() [IDLE/WAIT_FOR_RX_ZLP_CALL -> WAIT_FOR_TX_ZLP_CALL]
*
*                       Status IN   TxZLP()      [WAIT_FOR_TX_ZLP_CALL -> WAIT_FOR_TX_ZLP_CMPL] ->
*                                   ISR:TxCmpl() [WAIT_FOR_TX_ZLP_CMPL -> STATUS -> IDLE]
*********************************************************************************************************
*/

typedef  enum  usbd_tm4c123x_ep0_state {
    USBD_DRV_EP0_STATE_IDLE,

    USBD_DRV_EP0_STATE_WAITING_FOR_TX_CALL,
    USBD_DRV_EP0_STATE_WAITING_FOR_TX_CMPL,
    USBD_DRV_EP0_STATE_WAITING_FOR_RX_ZLP_CALL,

    USBD_DRV_EP0_STATE_WAITING_FOR_RX_CALL,
    USBD_DRV_EP0_STATE_WAITING_FOR_RX_CMPL,
    USBD_DRV_EP0_STATE_WAITING_FOR_TX_ZLP_CALL,
    USBD_DRV_EP0_STATE_WAITING_FOR_TX_ZLP_CMPL,

    USBD_DRV_EP0_STATE_STATUS
} USBD_TM4C123X_EP0_STATE;


/*
*********************************************************************************************************
*                                              REGISTERS
*
* Note(s) : (1) USB Device Control HOST register within this reserved region.
*
*                   [USBDEVCTL] (offset 0x060)
*
*
*           (2) (a) USB Transmit Functional Address EP 0-7 HOST registers within this reserved region.
*
*                       [USBTXFUNCADDRn] (offset 0x080, 0x088, 0x090, 0x098, 0x0A0, 0x0A8, 0x0B0, 0x0B8)
*
*               (b) USB Transmit Hub Address EP 0-7 HOST registers within this reserved region.
*
*                       [USBTXHUBADDRn] (offset 0x082, 0x08A, 0x092, 0x09A, 0x0A2, 0x0AA, 0x0B2, 0x0BA)
*
*               (c) USB Transmit Hub Port EP 0-7 HOST registers within this reserved region.
*
*                       [USBTXHUBPORTn] (offset 0x083, 0x08B, 0x093, 0x09B, 0x0A3, 0x0AB, 0x0B3, 0x0BB)
*
*               (d) USB Receive Functional Address EP 1-7 HOST registers within this reserved region.
*
*                       [USBRXFUNCADDRn] (offset 0x08C, 0x094, 0x09C, 0x0A4, 0x0Ac, 0x0B4, 0x0BC)
*
*               (e) USB Receive Hub Address EP 1-7 HOST registers within this reserved region.
*
*                       [USBRXHUBADDRn] (offset 0x08E, 0x096, 0x09E, 0x0A6, 0x0AE, 0x0B6, 0x0BE)
*
*               (f) USB Receive Hub Port EP 1-7 HOST registers within this reserved region.
*
*                       [USBRXHUBPORTn] (offset 0x08F, 0x097, 0x09F, 0x0A7, 0x0AF, 0x0B7, 0x0BF)
*
*
*           (3) (a) USB Type Endpoint 0 HOST register within this reserved region.
*
*                       [USBTYPE0] (offset 0x10A)
*
*               (b) USB NAK Limit HOST register within this reserved region.
*
*                       [USBNAKLMT] (offset 0x10B)
*
*
*           (4) (a) USB Host Transmit Configure Type EP 1-7 HOST registers within this reserved region.
*
*                       [USBTXTYPEn] (offset 0x11A, 0x12A, 0x13A, 0x14A, 0x15A, 0x16A, 0x17A)
*
*               (b) USB Host Transmit Interval EP 1-7 HOST registers within this reserved region.
*
*                       [USBTXINTERVALn] (offset 0x11B, 0x12B, 0x13B, 0x14B, 0x15B, 0x16B, 0x17B)
*
*               (c) USB Host Configure Receive Type EP 1-7 HOST registers within this reserved region.
*
*                       [USBRXTYPEn] (offset 0x11C, 0x12C, 0x13C, 0x14C, 0x15C, 0x16C, 0x17C)
*
*               (d) USB Host Receive Polling Interval EP 1-7 HOST registers within this reserved region.
* 
*                       [USBRXINTERVALn] (offset 0x11D, 0x12D, 0x13D, 0x14D, 0x15D, 0x16D, 0x17D)
*
*
*           (5) USB Request Packet Count in Block Transfer EP 1-7 HOST registers within this reserved region.
*
*                       [USBRQPKTCOUNTn] (offset 0x304, 0x308, 0x30C, 0x310, 0x314, 0x318, 0x31C)
*
*
*           (6) USB LPM Function Address HOST register within this reserved region.
*
*                       [USBLPMFADDR] (offset 0x365)
*
*
*           (7) (a) USB VBUS Droop Control HOST register within this reserved region.
*
*                       [USBVDC] (offset 0x430)
*
*               (b) USB VBUS Droop Control Raw Interrupt Status HOST register within this reserved region.
*
*                       [USBVDCRIS] (offset 0x434)
*
*               (c) USB VBUS Droop Control Interrupt Mask HOST register within this reserved region.
*
*                       [USBVDCIM] (offset 0x438)
*
*               (d) USB VBUS Droop Control Interrupt Status and Clear HOST register within this reserved region.
*
*                       [USBVDCISC] (offset 0x43C)
*********************************************************************************************************
*/

typedef  struct  usbd_tm4c123x_ep_fifo_reg {                    /* ------------- ENDPOINT FIFO REGISTERS -------------- */
    CPU_REG32  EPDATA;                                          /* USB FIFO Endpoint                                    */
} USBD_TM4C123X_EP_FIFO_REG;


typedef  struct  usbd_tm4c123x_ep_reg {                         /* ---------------- ENDPOINT REGISTERS ---------------- */
    CPU_REG16  USBTXMAXPx;                                      /* USB Maximum Transmit Data Endpoint                   */
    CPU_REG08  USBTXCSRLx;                                      /* USB Transmit Control and Status Endpoint Low         */
    CPU_REG08  USBTXCSRHx;                                      /* USB Transmit Control and Status Endpoint High        */
    CPU_REG16  USBRXMAXPx;                                      /* USB Maximum Receive Data Endpoint                    */
    CPU_REG08  USBRXCSRLx;                                      /* USB Receive Control and Status Endpoint Low          */
    CPU_REG08  USBRXCSRHx;                                      /* USB Receive Control and Status Endpoint High         */
    CPU_REG16  USBRXCOUNTx;                                     /* USB Receive Byte Count Endpoint                      */
    CPU_REG08  RSVD8[6u];                                       /* See Note (4)                                         */
} USBD_TM4C123X_EP_REG;


typedef  struct  usbd_tm4c123x_ep_dma {                         /* -------------- ENDPOINT DMA REGISTERS -------------- */
    CPU_REG16  USBDMACTLx;                                      /* USB DMA Control 0-7                                  */
    CPU_REG08  RSVD11[2u];
    CPU_REG32  USBDMAADDRx;                                     /* USB DMA Address 0-7                                  */
    CPU_REG32  USBDMACOUNTx;                                    /* USB DMA Count 0-7                                    */
    CPU_REG08  RSVD12[4u];
} USBD_TM4C123X_EP_DMA;

                                                                /* ---------------- TM4C123X REGISTERS ---------------- */
typedef  struct  usbd_tm4c123x_reg {

    CPU_REG08                  USBFADDR;                        /* USB Device Functional Address                        */
    CPU_REG08                  USBPOWER;                        /* USB Power                                            */
    CPU_REG16                  USBTXIS;                         /* USB Transmit Interrupt Status                        */
    CPU_REG16                  USBRXIS;                         /* USB Receive Interrupt Status                         */
    CPU_REG16                  USBTXIE;                         /* USB Transmit Interrupt Enable                        */
    CPU_REG16                  USBRXIE;                         /* USB Receive Interrupt Enable                         */
    CPU_REG08                  USBIS;                           /* USB General Interrupt Status                         */
    CPU_REG08                  USBIE;                           /* USB Interrupt Enable                                 */
    CPU_REG16                  USBFRAME;                        /* USB Frame Value                                      */
    CPU_REG08                  USBEPIDX;                        /* USB Endpoint Index                                   */
    CPU_REG08                  USBTEST;                         /* USB Test Mode                                        */
    CPU_REG08                  RSVD0[16u];
    USBD_TM4C123X_EP_FIFO_REG  USBFIFOx[TM4C123X_NBR_EPS];      /* USB FIFO Endpoint                                    */
    CPU_REG08                  RSVD1[33u];                      /* See Note (1)                                         */
    CPU_REG08                  USBCCONF;                        /* USB Common Configuration                             */
    CPU_REG08                  USBTXFIFOSZ;                     /* USB Transmit Dynamic FIFO Sizing                     */
    CPU_REG08                  USBRXFIFOSZ;                     /* USB Receive Dynamic FIFO Sizing                      */
    CPU_REG16                  USBTXFIFOADD;                    /* USB Transmit FIFO Start Address                      */
    CPU_REG16                  USBRXFIFOADD;                    /* USB Receive FIFO Start Address                       */
    CPU_REG08                  RSVD2[8u];
    CPU_REG08                  ULPIVBUSCTL;                     /* USB ULPI VBUS Control                                */
    CPU_REG08                  RSVD3[3u];
    CPU_REG08                  ULPIREGDATA;                     /* USB ULPI Register Data                               */
    CPU_REG08                  ULPIREGADDR;                     /* USB ULPI Register Address                            */
    CPU_REG08                  ULPIREGCTL;                      /* USB ULPI Register Control                            */
    CPU_REG08                  RSVD4;
    CPU_REG08                  USBEPINFO;                       /* USB Endpoint Information                             */
    CPU_REG08                  USBRAMINFO;                      /* USB RAM Information                                  */
    CPU_REG08                  USBCONTIM;                       /* USB Connect Timing                                   */
    CPU_REG08                  USBVPLEN;                        /* USB OTG VBUS Pulse Timing                            */
    CPU_REG08                  USBHSEOF;                        /* USB HS Last Transaction to End of Frame Timing       */
    CPU_REG08                  USBFSEOF;                        /* USB FS Last Transaction to End of Frame Timing       */
    CPU_REG08                  USBLSEOF;                        /* USB LS Last Transaction to end of Frame Timing       */
    CPU_REG08                  RSVD5[131u];                     /* See Note (2)                                         */
    CPU_REG08                  USBCSRL0;                        /* USB Control and Status Endpoint 0 Low                */
    CPU_REG08                  USBCSRH0;                        /* USB Control and Status Endpoint 0 High               */
    CPU_REG08                  RSVD6[4u];
    CPU_REG08                  USBCOUNT0;                       /* USB Receive Byte Count Endpoint 0                    */
    CPU_REG08                  RSVD7[7u];                       /* See Note (3)                                         */
    USBD_TM4C123X_EP_REG       TM4C123X_EP_REG[7u];             /* Endpoints 1 to 7 Registers                           */
    CPU_REG08                  RSVD9[128u];
    CPU_REG08                  USBDMAINTR;                      /* USB DMA Interrupt                                    */
    CPU_REG08                  RSVD10[3u];
    USBD_TM4C123X_EP_DMA       TM4C123X_EP_DMA_REG[8u];         /* USB Endpoint DMA Registers                           */
    CPU_REG08                  RSVD13[188u];                    /* See Note (5)                                         */
    CPU_REG16                  USBRXDPKTBUFDIS;                 /* USB Receive Double Packet Buffer Disable             */
    CPU_REG16                  USBTXDPKTBUFDIS;                 /* USB Transmit Double Packet Buffer Disable            */
    CPU_REG16                  USBCTO;                          /* USB Chirp Time-out                                   */
    CPU_REG16                  USBHHSRTN;                       /* USB High Speed to UTM Operating Delay                */
    CPU_REG16                  USBHSBT;                         /* USB High Speed Time-out Adder                        */
    CPU_REG08                  RSVD14[22u];
    CPU_REG16                  USBLPMATTR;                      /* USB LPM Attributes                                   */
    CPU_REG08                  USBLPMCNTRL;                     /* USB LPM Control                                      */
    CPU_REG08                  USBLPMIM;                        /* USB LPM Interrupt Mask                               */
    CPU_REG08                  USBLPMRIS;                       /* USB LPM Raw Interrupt Status                         */
    CPU_REG08                  RSVD15[155u];                    /* See Note (6)                                         */
    CPU_REG32                  USBEPC;                          /* USB External Power Control                           */
    CPU_REG32                  USBEPCRIS;                       /* USB External Power Control Raw Interrupt Status      */
    CPU_REG32                  USBEPCIM;                        /* USB External Power Control Interrupt Mask            */
    CPU_REG32                  USBEPCISC;                       /* USB External Power Control Interrupt Status and Clr. */
    CPU_REG32                  USBDRRIS;                        /* USB Device RESUME Raw Interrupt Status               */
    CPU_REG32                  USBDRIM;                         /* USB Device RESUME Interrupt Mask                     */
    CPU_REG32                  USBDRISC;                        /* USB Device RESUME Interrupt Status and Clear         */
    CPU_REG32                  USBGPCS;                         /* USB General-Purpose Control and Status               */
    CPU_REG08                  RSVD16[36u];                     /* See Note (7)                                         */
    CPU_REG32                  USBIDVRIS;                       /* USB ID Valid Detect Raw Interrupt Status             */
    CPU_REG32                  USBIDVIM;                        /* USB ID Valid Detect Interrupt Mask                   */
    CPU_REG32                  USBIDVISC;                       /* USB ID Valid Detect Interrupt Status and Clear       */
    CPU_REG32                  USBDMASEL;                       /* USB DMA Select                                       */
    CPU_REG08                  RSVD43[2924u];
    CPU_REG32                  USBPP;                           /* USB Peripheral Properties                            */
    CPU_REG32                  USBPC;                           /* USB Peripheral Configuration                         */
    CPU_REG32                  USBCC;                           /* USB Clock Configuration                              */
} USBD_TM4C123X_REG;


/*
*********************************************************************************************************
*                                            DEBUG STRUCTS
*********************************************************************************************************
*/

#if (TM4C123X_DBG_STATS_EN == DEF_ENABLED)
typedef  CPU_INT08U  USBD_DRV_DBG_STATS_CNT_TYPE;               /* Adjust size of the stats cntrs.                      */


typedef  struct  usbd_drv_dbg_ep0_state {
    USBD_DRV_DBG_STATS_CNT_TYPE  EP0_StatusCntNbr;
    USBD_DRV_DBG_STATS_CNT_TYPE  EP0_IdleCntNbr;
    USBD_DRV_DBG_STATS_CNT_TYPE  EP0_IdleCntNbrRXRDY;
    USBD_DRV_DBG_STATS_CNT_TYPE  EP0_IdleCntNbrRXRDY_Len0;
    USBD_DRV_DBG_STATS_CNT_TYPE  EP0_IdleCntNbrRXRDY_Len8;
    USBD_DRV_DBG_STATS_CNT_TYPE  EP0_WaitTxZLP_Cmpl;

    USBD_DRV_DBG_STATS_CNT_TYPE  EP0_WaitTxCmpl;
    USBD_DRV_DBG_STATS_CNT_TYPE  EP0_WaitRxCmpl;
    USBD_DRV_DBG_STATS_CNT_TYPE  EP0_Default;

    USBD_DRV_DBG_STATS_CNT_TYPE  EP0_IdleGoingToWaitTxCall;
    USBD_DRV_DBG_STATS_CNT_TYPE  EP0_IdleGoingToWaitRxCall;
    USBD_DRV_DBG_STATS_CNT_TYPE  EP0_IdleGoingToWaitRxZLP_Call;
    USBD_DRV_DBG_STATS_CNT_TYPE  EP0_IdleGoingToWaitTxZLP_Call;

    USBD_DRV_DBG_STATS_CNT_TYPE  EP0_RxStartGoingToWaitRxCmpl;
    USBD_DRV_DBG_STATS_CNT_TYPE  EP0_RxStartCallingRxCmplAndGoingToIdle;
    USBD_DRV_DBG_STATS_CNT_TYPE  EP0_RxStartCallingRxCmplAndGoingToRxZLP_Cmpl;

    USBD_DRV_DBG_STATS_CNT_TYPE  EP0_TxStartGoingToTxCmpl;
    USBD_DRV_DBG_STATS_CNT_TYPE  EP0_TxZLP_GoingToTxZLP_Cmpl;

    USBD_DRV_DBG_STATS_CNT_TYPE  EP0_WaitRxZLP_Call;

    USBD_DRV_DBG_STATS_CNT_TYPE  EP0_ISR_SET_END_IsSet;
} USBD_DRV_DBG_EP0_STATE;


typedef  struct  usbd_drv_dbg_gen_stats {
    USBD_DRV_DBG_STATS_CNT_TYPE  RxCmplCallFromRxStart;
    USBD_DRV_DBG_STATS_CNT_TYPE  EP0_RxCmplCalledFromRxStart;

    USBD_DRV_DBG_STATS_CNT_TYPE  UNDRN_WhileWritingFIFO;
    USBD_DRV_DBG_STATS_CNT_TYPE  UNDRN_AfterWritingFIFO32;
    USBD_DRV_DBG_STATS_CNT_TYPE  UNDRN_BeforeSettingTXRDY;

    USBD_DRV_DBG_STATS_CNT_TYPE  EP_TxAbortCnt;

    USBD_DRV_DBG_STATS_CNT_TYPE  NbStallSetCallRx;
    USBD_DRV_DBG_STATS_CNT_TYPE  NbStallSetCallTx;
    USBD_DRV_DBG_STATS_CNT_TYPE  NbStallClrCallRx;
    USBD_DRV_DBG_STATS_CNT_TYPE  NbStallClrCallTx;

    USBD_DRV_DBG_STATS_CNT_TYPE  RxCmplCallFromISR;
    USBD_DRV_DBG_STATS_CNT_TYPE  TxCmplCallFromISR;
    USBD_DRV_DBG_STATS_CNT_TYPE  TxISR_ElseTxcmpl;
    USBD_DRV_DBG_STATS_CNT_TYPE  TxISR_Flushed;

    USBD_DRV_DBG_STATS_CNT_TYPE  RxISR_Flushed;
    USBD_DRV_DBG_STATS_CNT_TYPE  RxISR_WillCallRxCmpl;
    USBD_DRV_DBG_STATS_CNT_TYPE  RxISR_ElseCnt;

} USBD_DRV_DBG_GEN_STATS;
#endif


/*
*********************************************************************************************************
*                                          DRIVER DATA TYPE
*********************************************************************************************************
*/

typedef  struct  usbd_drv_ep_status {
    CPU_INT08U  RxXferInProgress;                               /* Bitmap indicating that a rx xfer is in progress.     */
    CPU_INT08U  TxXferInProgress;                               /* Bitmap indicating that a tx xfer is in progress.     */

    CPU_INT08U  CallRxCmpl;                                     /* Bitmap indicating an early OUT token has been rx'd.  */

    CPU_INT08U  HasBeenFlushed;                                 /* Bitmap indicating that an EP has just been flushed.  */
} USBD_DRV_EP_STATUS;


typedef  struct  usbd_drv_data {                                /* ---------- DEVICE ENDPOINT DATA STRUCTURE ---------- */
                                                                /* Max pkt size info tbl.                               */
    CPU_INT16U               EP_MaxPktSize[TM4C123X_NBR_EPS * 2u];
    USBD_DRV_EP_STATUS       EP_Status;
    USBD_TM4C123X_EP0_STATE  EP0_State;
} USBD_DRV_DATA;


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

#if (TM4C123X_DBG_STATS_EN == DEF_ENABLED)
USBD_DRV_DBG_EP0_STATE   DbgEP0_State;
USBD_DRV_DBG_GEN_STATS   DbgGenStats;

USBD_TM4C123X_REG       *GlobalRegPtr;
USBD_DRV_DATA           *GlobalDrvDataPtr;
#endif


/*
*********************************************************************************************************
*                                             DBG MACROS
*********************************************************************************************************
*/

#if (TM4C123X_DBG_STATS_EN == DEF_ENABLED)
#define  USBD_DRV_DBG_EP0_STATS_RESET()     {                                                           \
                                                Mem_Clr((void     *)&DbgEP0_State,                      \
                                                        (CPU_SIZE_T) sizeof(USBD_DRV_DBG_EP0_STATE));   \
                                            }

#define  USBD_DRV_DBG_EP0_STATS_INC(stat)   {                                                           \
                                                DbgEP0_State.stat++;                                    \
                                            }

#define  USBD_DRV_DBG_GEN_STATS_RESET()     {                                                           \
                                                Mem_Clr((void     *)&DbgGenStats,                       \
                                                        (CPU_SIZE_T) sizeof(USBD_DRV_DBG_GEN_STATS));   \
                                            }
#define  USBD_DRV_DBG_GEN_STATS_INC(stat)   {                                                           \
                                                DbgGenStats.stat++;                                     \
                                            }
#else
#define  USBD_DRV_DBG_EP0_STATS_RESET()
#define  USBD_DRV_DBG_EP0_STATS_INC(stat)
#define  USBD_DRV_DBG_GEN_STATS_RESET()
#define  USBD_DRV_DBG_GEN_STATS_INC(stat)
#endif

#if (TM4C123X_DBG_TRAPS_EN == DEF_ENABLED)
#define  USBD_DRV_DBG_TRAP()                    while (1) {;}
#else
#define  USBD_DRV_DBG_TRAP()
#endif


/*
*********************************************************************************************************
*                             USB DEVICE CONTROLLER DRIVER API PROTOTYPES
*********************************************************************************************************
*/

static  void         USBD_DrvInit          (USBD_DRV     *p_drv,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvStart_TM4C123X(USBD_DRV     *p_drv,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvStart_MSP432E (USBD_DRV     *p_drv,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvStop          (USBD_DRV     *p_drv);

static  void         USBD_DrvAddrEn        (USBD_DRV     *p_drv,
                                            CPU_INT08U    dev_addr);

static  CPU_INT16U   USBD_DrvFrameNbrGet   (USBD_DRV     *p_drv);

static  void         USBD_DrvEP_Open       (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U    ep_type,
                                            CPU_INT16U    max_pkt_size,
                                            CPU_INT08U    transaction_frame,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvEP_Close      (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr);

static  CPU_INT32U   USBD_DrvEP_RxStart    (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U   *p_buf,
                                            CPU_INT32U    buf_len,
                                            USBD_ERR     *p_err);

static  CPU_INT32U   USBD_DrvEP_Rx         (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U   *p_buf,
                                            CPU_INT32U    buf_len,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvEP_RxZLP      (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            USBD_ERR     *p_err);

static  CPU_INT32U   USBD_DrvEP_Tx         (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U   *p_buf,
                                            CPU_INT32U    buf_len,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvEP_TxStart    (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U   *p_buf,
                                            CPU_INT32U    buf_len,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvEP_TxZLP      (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            USBD_ERR     *p_err);

static  CPU_BOOLEAN  USBD_DrvEP_Abort      (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr);

static  CPU_BOOLEAN  USBD_DrvEP_Stall      (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_BOOLEAN   state);

static  void         USBD_DrvISR_Handler   (USBD_DRV     *p_drv);


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  USBD_TM4C123x_EP0_ISR_Handle (USBD_DRV           *p_drv,
                                            USBD_TM4C123X_REG  *p_reg,
                                            USBD_DRV_DATA      *p_drv_data);


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

USBD_DRV_API  USBD_DrvAPI_TM4C123X = { USBD_DrvInit,
                                       USBD_DrvStart_TM4C123X,
                                       USBD_DrvStop,
                                       DEF_NULL,
                                       USBD_DrvAddrEn,
                                       DEF_NULL,
                                       DEF_NULL,
                                       USBD_DrvFrameNbrGet,
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


USBD_DRV_API  USBD_DrvAPI_MSP432E  = { USBD_DrvInit,
                                       USBD_DrvStart_MSP432E,
                                       USBD_DrvStop,
                                       DEF_NULL,
                                       USBD_DrvAddrEn,
                                       DEF_NULL,
                                       DEF_NULL,
                                       USBD_DrvFrameNbrGet,
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
    USBD_DRV_DATA     *p_drv_data;
    LIB_ERR            err_lib;


    p_bsp_api = p_drv->BSP_API_Ptr;                             /* Get driver BSP API reference.                        */

                                                                /* Allocate driver internal data.                       */
    p_drv_data = (USBD_DRV_DATA *)Mem_HeapAlloc(sizeof(USBD_DRV_DATA),
                                                sizeof(CPU_ALIGN),
                                                DEF_NULL,
                                               &err_lib);
    if (p_drv_data == (USBD_DRV_DATA *)0) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

    USBD_DRV_DBG_EP0_STATS_RESET();
    USBD_DRV_DBG_GEN_STATS_RESET();

#if (TM4C123X_DBG_STATS_EN == DEF_ENABLED)
    GlobalDrvDataPtr =  p_drv_data;
    GlobalRegPtr     = (USBD_TM4C123X_REG *)p_drv->CfgPtr->BaseAddr;
#endif

    Mem_Clr((void *)&(p_drv_data->EP_MaxPktSize[0u]),           /* Clr EP max pkt size info tbl.                        */
               sizeof(p_drv_data->EP_MaxPktSize));

    p_drv->DataPtr = p_drv_data;                                /* Store drv's internal data ptr.                       */

    if (p_bsp_api->Init != (void *)0) {
        p_bsp_api->Init(p_drv);                                 /* Call board/chip specific device controller ...       */
                                                                /* ... initialization function.                         */
    }

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                      USBD_DrvStart_TM4C123X()
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
* Note(s)     : Typically, the start function activates the pull-up on the D+ or D- pin to simulate
*               attachment to host. Some MCUs/MPUs have an internal pull-down that is activated by a
*               device controller register; for others, this may be a GPIO pin. Additionally, interrupts
*               for reset and suspend are activated.
*********************************************************************************************************
*/

static  void  USBD_DrvStart_TM4C123X (USBD_DRV  *p_drv,
                                      USBD_ERR  *p_err)
{
    USBD_DRV_BSP_API   *p_bsp_api;
    USBD_TM4C123X_REG  *p_reg;


    p_reg     = (USBD_TM4C123X_REG *)p_drv->CfgPtr->BaseAddr;   /* Get USB controller register reference.               */
    p_bsp_api =  p_drv->BSP_API_Ptr;                            /* Get driver BSP API reference.                        */

                                                                /* Set controller to Device mode                        */
    DEF_BIT_SET(p_reg->USBGPCS, (TM4C123X_USBGPCS_DEVMODOTG |
                                 TM4C123X_USBGPCS_DEVMOD));

    DEF_BIT_CLR(p_reg->USBIE,    TM4C123X_USBI_EVENTS_ALL);     /* Disable all USB interrupts.                          */
    DEF_BIT_CLR(p_reg->USBTXIE,  TM4C123X_USBTXIE_EP_ALL);      /* Disable all TX interrupts.                           */
    DEF_BIT_CLR(p_reg->USBRXIE,  TM4C123X_USBRXIE_EP_ALL);      /* Disable all RX interrupts.                           */

    DEF_BIT_SET(p_reg->USBTEST,  TM4C123X_USBTEST_FORCEFS);     /* Set device to Full Speed.                            */
    DEF_BIT_SET(p_reg->USBPOWER, TM4C123X_POWER_SOFTCONN);      /* Enable D+ and D- lines.                              */

    p_reg->USBIE = TM4C123X_USBI_EVENTS_ALL;                    /* Enable all USB interrupts.                           */

    if (p_bsp_api->Conn != (void *)0) {
        p_bsp_api->Conn();                                      /* Call board/chip specific connect function.           */
    }

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       USBD_DrvStart_MSP432E()
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
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_DrvStart_MSP432E (USBD_DRV  *p_drv,
                                     USBD_ERR  *p_err)
{
    USBD_DRV_BSP_API   *p_bsp_api;
    USBD_TM4C123X_REG  *p_reg;


    p_reg     = (USBD_TM4C123X_REG *)p_drv->CfgPtr->BaseAddr;   /* Get USB controller register reference.               */
    p_bsp_api =  p_drv->BSP_API_Ptr;                            /* Get driver BSP API reference.                        */

                                                                /* Use USB0VBUS and USB0ID pins                         */
    DEF_BIT_CLR(p_reg->USBGPCS,  MSP432E_USBGPCS_DEVMOD);

    DEF_BIT_CLR(p_reg->USBCC,    MSP432E_USBCC_CSD);            /* Use internal clock source.                           */
    DEF_BIT_SET(p_reg->USBCC,    MSP432E_USBCC_CLKDIV_4);       /* Divide PLL VCO by 4.                                 */
    DEF_BIT_SET(p_reg->USBCC,    MSP432E_USBCC_CLKEN);          /* Enable USB clock.                                    */

    DEF_BIT_CLR(p_reg->USBPC,    MSP432E_USBPC_ULPIEN);         /* Select Integrated PHY (no ULPI).                     */

    DEF_BIT_CLR(p_reg->USBIE,    TM4C123X_USBI_EVENTS_ALL);     /* Disable all USB interrupts.                          */

    DEF_BIT_CLR(p_reg->USBTXIE,  TM4C123X_USBTXIE_EP_ALL);      /* Disable all TX interrupts.                           */
    DEF_BIT_CLR(p_reg->USBRXIE,  TM4C123X_USBRXIE_EP_ALL);      /* Disable all RX interrupts.                           */

    DEF_BIT_SET(p_reg->USBTEST,  TM4C123X_USBTEST_FORCEFS);     /* Set device to Full Speed.                            */
    DEF_BIT_SET(p_reg->USBPOWER, TM4C123X_POWER_SOFTCONN);      /* Enable D+ and D- lines.                              */

    p_reg->USBIE = TM4C123X_USBI_EVENTS_ALL;                    /* Enable all USB interrupts.                           */

    if (p_bsp_api->Conn != (void *)0) {
        p_bsp_api->Conn();                                      /* Call board/chip specific connect function.           */
    }

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
    USBD_TM4C123X_REG  *p_reg;


    p_reg     = (USBD_TM4C123X_REG *)p_drv->CfgPtr->BaseAddr;   /* Get USB controller register reference.               */
    p_bsp_api =  p_drv->BSP_API_Ptr;                            /* Get driver BSP API reference.                        */

    DEF_BIT_CLR(p_reg->USBIE,    TM4C123X_USBI_EVENTS_ALL);     /* Disable all USB interrupts.                          */
    DEF_BIT_CLR(p_reg->USBTXIE,  TM4C123X_USBTXIE_EP_ALL);      /* Disable all TX interrupts.                           */
    DEF_BIT_CLR(p_reg->USBRXIE,  TM4C123X_USBRXIE_EP_ALL);      /* Disable all RX interrupts.                           */

    DEF_BIT_CLR(p_reg->USBPOWER, TM4C123X_POWER_SOFTCONN);      /* Disable D+ and D- lines.                             */

    if (p_bsp_api->Disconn != (void *)0) {
        p_bsp_api->Disconn();
    }
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
    USBD_TM4C123X_REG  *p_reg;


    p_reg = (USBD_TM4C123X_REG *)p_drv->CfgPtr->BaseAddr;       /* Get USB controller register reference.               */

    p_reg->USBFADDR = dev_addr;
}


/*
*********************************************************************************************************
*                                        USBD_DrvFrameNbrGet()
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

static  CPU_INT16U  USBD_DrvFrameNbrGet (USBD_DRV  *p_drv)
{
    USBD_TM4C123X_REG  *p_reg;
    CPU_INT16U          frame_nbr;


    p_reg = (USBD_TM4C123X_REG *)p_drv->CfgPtr->BaseAddr;       /* Get USB controller register reference.               */

    frame_nbr = (p_reg->USBFRAME & TM4C123X_USBFRAME_FRAME);

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
*
*               (3) The USB FIFO has a capacity of 2KB. Only 512 bytes are used since ISO endpoints are
*                   not available at this time. The 512 bytes are partioned evenly from endpoints 0 to 7
*                   with 64 bytes per endpoint.
*********************************************************************************************************
*/

static  void  USBD_DrvEP_Open (USBD_DRV    *p_drv,
                               CPU_INT08U   ep_addr,
                               CPU_INT08U   ep_type,
                               CPU_INT16U   max_pkt_size,
                               CPU_INT08U   transaction_frame,
                               USBD_ERR    *p_err)
{
    USBD_TM4C123X_REG  *p_reg;
    USBD_DRV_DATA      *p_drv_data;
    CPU_INT08U          ep_phy_nbr;
    CPU_INT08U          ep_log_nbr;
    CPU_INT16U          fifo_add;
    CPU_INT08U          epidx_reg;
    CPU_INT08U          fifosz_reg;
    CPU_INT16U          fifoadd_reg;
    CPU_INT16U          maxpx_reg;
    CPU_INT08U          csrhx_reg;
    CPU_INT08U          csrlx_reg;
    CPU_SR_ALLOC();


    (void)transaction_frame;

    if (ep_type == USBD_EP_TYPE_ISOC) {
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }

                                                                /* ------------ SET SIZE OF ENDPOINT FIFO ------------- */
    switch (max_pkt_size) {
        case 8u:
             fifosz_reg = TM4C123X_USBFIFOSZ_SIZE_8;
             break;


        case 16u:
             fifosz_reg = TM4C123X_USBFIFOSZ_SIZE_16;
             break;


        case 32u:
             fifosz_reg = TM4C123X_USBFIFOSZ_SIZE_32;
             break;


        case 64u:
             fifosz_reg = TM4C123X_USBFIFOSZ_SIZE_64;
             break;


        default:
            *p_err = USBD_ERR_INVALID_ARG;
             return;
    }

    p_reg      = (USBD_TM4C123X_REG *)p_drv->CfgPtr->BaseAddr;  /* Get USB controller register reference.               */
    p_drv_data = (USBD_DRV_DATA     *)p_drv->DataPtr;           /* Get a ref to the drv's internal data.                */
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);                 /* Get EP physical number.                              */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical number.                               */

                                                                /* Init reg cfg vars.                                   */
    csrhx_reg = 0u;
    csrlx_reg = 0u;
                                                                /* -------- ENABLE EP INTERRUPTS & CONFIGURE EP ------- */
    if (ep_log_nbr == 0u) {
                                                                /* Enable Device EP0 Interrupt.                         */
        DEF_BIT_SET(p_reg->USBTXIE, TM4C123X_USBTXIE_EP0);

        CPU_CRITICAL_ENTER();
        p_drv_data->EP0_State                 = USBD_DRV_EP0_STATE_IDLE;
        p_drv_data->EP_MaxPktSize[ep_phy_nbr] = max_pkt_size;
        CPU_CRITICAL_EXIT();
    } else {
        epidx_reg   = (ep_log_nbr & TM4C123X_USBEPIDX_EPIDX);   /* Set EP ix.                                           */
        fifo_add    = (TM4C123X_USBFIFOADD_ADDR_64 * ep_phy_nbr);
        fifoadd_reg = (fifo_add & TM4C123X_USBFIFOADD_ADDR);    /* Set starting addr for FIFO (See note 3).             */

        maxpx_reg = (max_pkt_size & TM4C123X_USBMAXPN_MAXLOAD); /* Set max pkt size.                                    */

        DEF_BIT_SET(csrlx_reg, TM4C123X_USBTXCSRLX_CLRDT);      /* Reset the Data toggle to zero.                       */

                                                                /* ---------------------- IN EP ----------------------- */
        if (USBD_EP_IS_IN(ep_addr) == DEF_YES) {
            CPU_CRITICAL_ENTER();
            p_reg->USBEPIDX     = epidx_reg;                        /* Write regs.                                          */
            p_reg->USBTXFIFOSZ  = fifosz_reg;
            p_reg->USBTXFIFOADD = fifoadd_reg;
            p_drv_data->EP_MaxPktSize[ep_phy_nbr] = max_pkt_size;
            CPU_CRITICAL_EXIT();

            DEF_BIT_SET(csrhx_reg, TM4C123X_USBCSRHX_MODE);     /* Set dir to TX.                                       */

            p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBTXMAXPx = maxpx_reg;
            p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBTXCSRHx = csrhx_reg;
            p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBRXCSRHx = 0u;
            p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBTXCSRLx = csrlx_reg;

            DEF_BIT_SET(p_reg->USBTXIE, DEF_BIT(ep_log_nbr));   /* En EP intr.                                          */
        } else {                                                /* ---------------------- OUT EP ---------------------- */
            CPU_CRITICAL_ENTER();
            p_reg->USBEPIDX     = epidx_reg;                        /* Write regs.                                          */
            p_reg->USBRXFIFOSZ  = fifosz_reg;
            p_reg->USBRXFIFOADD = fifoadd_reg;
            p_drv_data->EP_MaxPktSize[ep_phy_nbr] = max_pkt_size;
            CPU_CRITICAL_EXIT();

            p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBRXMAXPx = maxpx_reg;
            p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBTXCSRHx = 0u;
            p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBRXCSRHx = 0u;
            p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBRXCSRLx = csrlx_reg;

            DEF_BIT_SET(p_reg->USBRXIE, DEF_BIT(ep_log_nbr));   /* En EP intr.                                          */
        }
    }

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
    USBD_TM4C123X_REG  *p_reg;
    CPU_INT08U          ep_log_nbr;


    p_reg      = (USBD_TM4C123X_REG *)p_drv->CfgPtr->BaseAddr;  /* Get USB controller register reference.               */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical number.                               */

                                                                /* --------- DISABLE EP INTERRUPTS & RESET EP --------- */
    if (ep_log_nbr == 0) {
        DEF_BIT_CLR(p_reg->USBTXIE, TM4C123X_USBTXIE_EP0);      /* Disable Device EP0 Interrupt.                        */
    } else {
        if (USBD_EP_IS_IN(ep_addr) == DEF_YES) {                /* ------------------- IN ENDPOINTS ------------------- */
            DEF_BIT_CLR(p_reg->USBTXIE, DEF_BIT(ep_log_nbr));   /* Disable endpoint interrupt.                          */
        } else {                                                /* ------------------ OUT ENDPOINTS ------------------- */
            DEF_BIT_CLR(p_reg->USBRXIE, DEF_BIT(ep_log_nbr));   /* Disable endpoint interrupt.                          */
        }
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
* Note(s)     : (1) USBD_EP_RxCmpl() needs to be called whenever the core calls RxStart() for a ZLP since
*                   the controller does not trigger an interrupt when a ZLP is received. Therefore, to
*                   allow the core to continue to work correctly, the driver assumes the ZLP have been
*                   received and calls USBD_EP_RxCmpl(). There is no way of knowing if a ZLP have not
*                   been received, but this could lead to errors between the core and the driver if it
*                   happened.
*
*               (2) The EP0 state must be left untouched here, because this call may happen after the
*                   next setup packet has been received by the driver. Setting the EP0 state here could
*                   mess with the EP0 state machine, since it may already have been set for the next
*                   setup packet.
*********************************************************************************************************
*/

static  CPU_INT32U  USBD_DrvEP_RxStart (USBD_DRV    *p_drv,
                                        CPU_INT08U   ep_addr,
                                        CPU_INT08U  *p_buf,
                                        CPU_INT32U   buf_len,
                                        USBD_ERR    *p_err)
{
    USBD_TM4C123X_REG  *p_reg;
    USBD_DRV_DATA      *p_drv_data;
    CPU_INT08U          ep_log_nbr;
    CPU_INT08U          max_xfer_len;
    CPU_SR_ALLOC();


    (void)p_buf;
                                                                /* Get USB controller register reference.               */
    p_reg        = (USBD_TM4C123X_REG *)(p_drv->CfgPtr->BaseAddr);
    p_drv_data   = (USBD_DRV_DATA     *) p_drv->DataPtr;        /* Get a ref to the drv's internal data.                */
    ep_log_nbr   =  USBD_EP_ADDR_TO_LOG(ep_addr);               /* Get EP logical number.                               */
    max_xfer_len = (CPU_INT08U)DEF_MIN(buf_len, p_drv_data->EP_MaxPktSize[USBD_EP_ADDR_TO_PHY(ep_addr)]);

    if (ep_log_nbr != 0) {
        CPU_CRITICAL_ENTER();
        if (DEF_BIT_IS_SET(p_drv_data->EP_Status.CallRxCmpl, DEF_BIT(ep_log_nbr)) == DEF_YES) {
            USBD_DRV_DBG_GEN_STATS_INC(RxCmplCallFromRxStart)
            DEF_BIT_CLR(p_drv_data->EP_Status.CallRxCmpl, DEF_BIT(ep_log_nbr));
            CPU_CRITICAL_EXIT();
            USBD_EP_RxCmpl(p_drv, ep_log_nbr);
        } else {
            DEF_BIT_SET(p_drv_data->EP_Status.RxXferInProgress, DEF_BIT(ep_log_nbr));
            CPU_CRITICAL_EXIT();
        }
    } else {
        if (buf_len != 0u) {
            CPU_CRITICAL_ENTER();
            if ((p_drv_data->EP0_State == USBD_DRV_EP0_STATE_WAITING_FOR_RX_CALL) ||
                (p_drv_data->EP0_State == USBD_DRV_EP0_STATE_WAITING_FOR_TX_ZLP_CALL)) {

                DEF_BIT_SET(p_reg->USBCSRL0, TM4C123X_USBCSRL0_RXRDYC);

                USBD_DRV_DBG_EP0_STATS_INC(EP0_RxStartGoingToWaitRxCmpl);
                p_drv_data->EP0_State = USBD_DRV_EP0_STATE_WAITING_FOR_RX_CMPL;
            } else {
                USBD_DRV_DBG_TRAP();
            }
            CPU_CRITICAL_EXIT();
        } else {
            USBD_EP_RxCmpl(p_drv, 0u);                          /* See Note #1.                                         */
                                                                /* EP0 State must not be changed, here. See Note #2.    */
        }
    }

   *p_err = USBD_ERR_NONE;

    return (max_xfer_len);
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
    USBD_TM4C123X_REG  *p_reg;
    CPU_INT08U          ep_log_nbr;
    CPU_INT16U          pkt_len;
    CPU_INT16U          bytes_rxd;
    CPU_INT16U          i;
    CPU_INT32U          nbr_words;
    CPU_INT32U         *p_buf32;
    CPU_INT08U         *p_buf08;
    CPU_REG08          *p_fifo08;


    p_reg      = (USBD_TM4C123X_REG *)p_drv->CfgPtr->BaseAddr;  /* Get USB controller register reference.               */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical number.                               */

    if (ep_log_nbr != 0u) {
        pkt_len = p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBRXCOUNTx;
    } else {
        pkt_len = p_reg->USBCOUNT0;
    }

    bytes_rxd =  DEF_MIN(pkt_len, buf_len);
    nbr_words = (bytes_rxd / 4u);
    p_buf32   = (CPU_INT32U *)p_buf;

    for (i = 0u; i < nbr_words; i++) {
       *p_buf32 = p_reg->USBFIFOx[ep_log_nbr].EPDATA;
        p_buf32++;
    }

    bytes_rxd = (bytes_rxd % 4u);
    if (bytes_rxd != 0u) {

        p_buf08  = (CPU_INT08U *) p_buf32;
        p_fifo08 = (CPU_REG08  *)&p_reg->USBFIFOx[ep_log_nbr].EPDATA;

        for (i = 0u; i < bytes_rxd; i++) {
           *p_buf08++  = *p_fifo08;
        }
    }

    if (ep_log_nbr != 0u) {
        DEF_BIT_CLR(p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBRXCSRLx, TM4C123X_USBRXCSRLX_RXRDY);
    }

    if (pkt_len > buf_len) {
       *p_err = USBD_ERR_DRV_BUF_OVERFLOW;
    } else {
       *p_err = USBD_ERR_NONE;
    }

    return (pkt_len);
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
    USBD_TM4C123X_REG  *p_reg;
    CPU_INT08U          ep_log_nbr;


    p_reg      = (USBD_TM4C123X_REG *)p_drv->CfgPtr->BaseAddr;  /* Get USB controller register reference.               */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical number.                               */

    if (ep_log_nbr != 0u) {
        DEF_BIT_SET(p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBRXCSRLx, TM4C123X_USBRXCSRLX_FLUSH);
        DEF_BIT_CLR(p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBRXCSRLx, TM4C123X_USBRXCSRLX_RXRDY);
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
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT08U      ep_phy_nbr;
    CPU_INT08U      ep_log_nbr;
    CPU_INT16U      ep_pkt_len;
    CPU_SR_ALLOC();


    (void)p_buf;

    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;               /* Get a ref to the drv's internal data.                */
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);                 /* Get EP physical number.                              */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical number.                               */
    ep_pkt_len = (CPU_INT16U)DEF_MIN(p_drv_data->EP_MaxPktSize[ep_phy_nbr], buf_len);

    if (ep_log_nbr != 0u) {
        CPU_CRITICAL_ENTER();
        DEF_BIT_SET(p_drv_data->EP_Status.TxXferInProgress, DEF_BIT(ep_log_nbr));
        CPU_CRITICAL_EXIT();
    }

   *p_err = USBD_ERR_NONE;

    return (ep_pkt_len);
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
    USBD_TM4C123X_REG  *p_reg;
    USBD_DRV_DATA      *p_drv_data;
    CPU_INT08U          ep_log_nbr;
    CPU_INT08U          ep_phy_nbr;
    CPU_INT32U          i;
    CPU_INT32U          nbr_words;
    CPU_INT16U          tx_len;
    CPU_REG08          *p_fifo08;
    CPU_INT32U         *p_buf32;
    CPU_INT08U         *p_buf08;
    CPU_SR_ALLOC();


    p_reg      = (USBD_TM4C123X_REG *)(p_drv->CfgPtr->BaseAddr);/* Get USB controller register reference.               */
    p_drv_data = (USBD_DRV_DATA     *)(p_drv->DataPtr);         /* Get a ref to the drv's internal data.                */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical number.                               */
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);                 /* Get EP physical number.                              */
    tx_len     =  DEF_MIN(p_drv_data->EP_MaxPktSize[ep_phy_nbr], buf_len);
    nbr_words  = (tx_len / 4u);
    p_buf32    = (CPU_INT32U *)p_buf;

    if (ep_log_nbr == 0u) {
        DEF_BIT_SET(p_reg->USBCSRL0, TM4C123X_USBCSRL0_RXRDYC);
    } else {
        DEF_BIT_CLR(p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBTXCSRLx, TM4C123X_USBTXCSRLX_UNDRN);
        DEF_BIT_CLR(p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBTXCSRLx, TM4C123X_USBTXCSRLX_TXRDY);
    }

    for (i = 0u; i < nbr_words; i++) {
        p_reg->USBFIFOx[ep_log_nbr].EPDATA = *p_buf32;
        p_buf32++;
        if (ep_log_nbr != 0u) {
            if (DEF_BIT_IS_SET(p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBTXCSRLx, TM4C123X_USBTXCSRLX_UNDRN)) {
                USBD_DRV_DBG_GEN_STATS_INC(UNDRN_WhileWritingFIFO);
            }
        }
    }

    if (ep_log_nbr != 0u) {
        if (DEF_BIT_IS_SET(p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBTXCSRLx, TM4C123X_USBTXCSRLX_UNDRN)) {
            USBD_DRV_DBG_GEN_STATS_INC(UNDRN_AfterWritingFIFO32);
        }
    } else {
        CPU_CRITICAL_ENTER();
        if ((p_drv_data->EP0_State == USBD_DRV_EP0_STATE_WAITING_FOR_TX_CALL) ||
            (p_drv_data->EP0_State == USBD_DRV_EP0_STATE_WAITING_FOR_RX_ZLP_CALL)) {
            USBD_DRV_DBG_EP0_STATS_INC(EP0_TxStartGoingToTxCmpl);
            p_drv_data->EP0_State = USBD_DRV_EP0_STATE_WAITING_FOR_TX_CMPL;
        } else {
            USBD_DRV_DBG_TRAP();
        }
        CPU_CRITICAL_EXIT();
    }

    tx_len = (tx_len % 4u);
    if (tx_len != 0u) {                                         /* Write extra byte(s).                                 */

        p_buf08  = (CPU_INT08U *) p_buf32;
        p_fifo08 = (CPU_REG08  *)&p_reg->USBFIFOx[ep_log_nbr].EPDATA;

        for (i = 0u; i < tx_len; i++) {
           *p_fifo08 = *p_buf08++;
        }
    }

    if (ep_log_nbr == 0u) {
        DEF_BIT_SET(p_reg->USBCSRL0, TM4C123X_USBCSRL0_TXRDY);
    } else {
        if (DEF_BIT_IS_SET(p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBTXCSRLx, TM4C123X_USBTXCSRLX_UNDRN)) {
            USBD_DRV_DBG_GEN_STATS_INC(UNDRN_BeforeSettingTXRDY);
        }

        DEF_BIT_CLR(p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBTXCSRLx, TM4C123X_USBTXCSRLX_UNDRN);
        DEF_BIT_SET(p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBTXCSRLx, TM4C123X_USBTXCSRLX_TXRDY);
    }

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
    USBD_TM4C123X_REG  *p_reg;
    USBD_DRV_DATA      *p_drv_data;
    CPU_INT08U          ep_log_nbr;
    CPU_SR_ALLOC();


    p_reg      = (USBD_TM4C123X_REG *)p_drv->CfgPtr->BaseAddr;  /* Get USB controller register reference.               */
    p_drv_data = (USBD_DRV_DATA     *)(p_drv->DataPtr);         /* Get a ref to the drv's internal data.                */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical number.                               */

    if (ep_log_nbr == 0u) {
        CPU_CRITICAL_ENTER();
        if (p_drv_data->EP0_State == USBD_DRV_EP0_STATE_WAITING_FOR_TX_ZLP_CALL) {
            p_drv_data->EP0_State = USBD_DRV_EP0_STATE_WAITING_FOR_TX_ZLP_CMPL;
            USBD_DRV_DBG_EP0_STATS_INC(EP0_TxZLP_GoingToTxZLP_Cmpl);
            DEF_BIT_SET(p_reg->USBCSRL0,
                       (TM4C123X_USBCSRL0_RXRDYC | TM4C123X_USBCSRL0_DATAEND));
        } else {
            USBD_DRV_DBG_TRAP();
        }
        CPU_CRITICAL_EXIT();
    } else {
        CPU_CRITICAL_ENTER();
        DEF_BIT_SET(p_drv_data->EP_Status.TxXferInProgress, DEF_BIT(ep_log_nbr));
        CPU_CRITICAL_EXIT();
        DEF_BIT_SET(p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBTXCSRLx, TM4C123X_USBTXCSRLX_TXRDY);
    }

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
* Note(s)     : (1) There is no way to clear, flush or abort the FIFO for an IN endpoint that is not the
*                   endpoint 0 once the TXRDY bit has been set. This can lead to some 'de-synchronization'
*                   problems between what kind of data the host is expecting and the actual data that the
*                   device will send. For example, if an IN transfer sending a buffer containing 0x01s
*                   values gets aborted and that another IN transfer supposed to send a buffer containing
*                   0x02s values is started, only the 0x01s values will be sent. The 0x02s will not
*                   'overwrite' the FIFO containing the 0x01s and this will not produce any kind of error.
*                   The following link contains more information about that limitation of the controller:
*                   http://e2e.ti.com/support/microcontrollers/stellaris_arm/f/471/t/117028.aspx?pi171693=1
*                   The function still returns DEF_OK, to allow the core to continue to work as correctly
*                   as possible. It would be possible to implement a particular protocol on the host's
*                   side to make sure this kind of situation is handled.
*
*               (2) This method of handling an 'Abort' call on an IN endpoint that is not endpoint 0 has
*                   been found by doing various tests. This is the best way to handle the 'Abort', even
*                   though it is not fully functional, it does not lead to any unexpected behavior or
*                   worse problems, like a USB bus error.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_DrvEP_Abort (USBD_DRV    *p_drv,
                                       CPU_INT08U   ep_addr)
{
    USBD_TM4C123X_REG  *p_reg;
    USBD_DRV_DATA      *p_drv_data;
    CPU_INT08U          ep_log_nbr;
    CPU_SR_ALLOC();


    p_reg      = (USBD_TM4C123X_REG *) p_drv->CfgPtr->BaseAddr; /* Get USB controller register reference.               */
    p_drv_data = (USBD_DRV_DATA     *)(p_drv->DataPtr);         /* Get a ref to the drv's internal data.                */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical number.                               */

    CPU_CRITICAL_ENTER();
    if (ep_log_nbr != 0u) {
        if (USBD_EP_IS_IN(ep_addr) == DEF_YES) {                /* See Note #1.                                         */
            USBD_DRV_DBG_GEN_STATS_INC(EP_TxAbortCnt);

                                                                /* See Note #2.                                         */
            DEF_BIT_CLR(p_drv_data->EP_Status.TxXferInProgress, DEF_BIT(ep_log_nbr));
        } else {
            DEF_BIT_SET(p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBRXCSRLx, TM4C123X_USBRXCSRLX_FLUSH);

            DEF_BIT_CLR(p_drv_data->EP_Status.RxXferInProgress, DEF_BIT(ep_log_nbr));
            DEF_BIT_CLR(p_drv_data->EP_Status.CallRxCmpl,       DEF_BIT(ep_log_nbr));
        }
        DEF_BIT_SET(p_drv_data->EP_Status.HasBeenFlushed, DEF_BIT(ep_log_nbr));
    } else {
        if (USBD_EP_IS_IN(ep_addr) == DEF_YES) {
            DEF_BIT_SET(p_reg->USBCSRL0, TM4C123X_USBCSRL0_TXRDY);
            DEF_BIT_SET(p_reg->USBCSRH0, TM4C123X_USBCSRH0_FLUSH);
        } else {
            DEF_BIT_SET(p_reg->USBCSRH0, TM4C123X_USBCSRH0_FLUSH);
        }
        p_drv_data->EP0_State = USBD_DRV_EP0_STATE_IDLE;
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
    USBD_TM4C123X_REG  *p_reg;
    USBD_DRV_DATA      *p_drv_data;
    CPU_INT08U          ep_log_nbr;
    CPU_SR_ALLOC();


    p_reg      = (USBD_TM4C123X_REG *) p_drv->CfgPtr->BaseAddr; /* Get USB controller register reference.               */
    p_drv_data = (USBD_DRV_DATA     *)(p_drv->DataPtr);         /* Get a ref to the drv's internal data.                */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical number.                               */

    if (ep_log_nbr == 0) {
        if (state == DEF_SET) {
                                                                /* Perform a stall on control endpoint 0.               */
            DEF_BIT_SET(p_reg->USBCSRL0, (TM4C123X_USBCSRL0_STALL |
                                          TM4C123X_USBCSRL0_RXRDYC));
            CPU_CRITICAL_ENTER();
            p_drv_data->EP0_State = USBD_DRV_EP0_STATE_IDLE;
            CPU_CRITICAL_EXIT();
        } else {
                                                                /* Clear stall condition on control endpoint 0.         */
            CPU_CRITICAL_ENTER();
            p_drv_data->EP0_State = USBD_DRV_EP0_STATE_IDLE;
            CPU_CRITICAL_EXIT();

            DEF_BIT_CLR(p_reg->USBCSRL0, TM4C123X_USBCSRL0_STALLED);
        }

    } else if (USBD_EP_IS_IN(ep_addr) == DEF_YES) {

        if (state == DEF_SET) {
            USBD_DRV_DBG_GEN_STATS_INC(NbStallSetCallTx);
                                                                /* Perform a stall on an IN endpoint.                   */
            DEF_BIT_SET(p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBTXCSRLx,
                        TM4C123X_USBTXCSRLX_STALL);

        } else {                                                /* Clear stall condition on an IN endpoint.             */
            USBD_DRV_DBG_GEN_STATS_INC(NbStallClrCallTx);
                                                                /* Reset the data toggle.                               */
            DEF_BIT_SET(p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBTXCSRLx,
                        TM4C123X_USBTXCSRLX_CLRDT);

            DEF_BIT_CLR(p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBTXCSRLx,
                       (TM4C123X_USBTXCSRLX_STALL |
                        TM4C123X_USBTXCSRLX_STALLED));
        }
    } else {
        if (state == DEF_SET) {
            USBD_DRV_DBG_GEN_STATS_INC(NbStallSetCallRx);
                                                                /* Perform a stall on an OUT endpoint.                  */
            DEF_BIT_SET(p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBRXCSRLx,
                        TM4C123X_USBRXCSRLX_STALL);

        } else {                                                /* Clear stall condition on an OUT endpoint.            */
            USBD_DRV_DBG_GEN_STATS_INC(NbStallClrCallRx);
                                                                /* Reset the data toggle.                               */
            DEF_BIT_SET(p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBRXCSRLx,
                        TM4C123X_USBRXCSRLX_CLRDT);

            DEF_BIT_CLR(p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBRXCSRLx,
                       (TM4C123X_USBRXCSRLX_STALL |
                        TM4C123X_USBRXCSRLX_STALLED));

            DEF_BIT_SET(p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBRXCSRLx, TM4C123X_USBRXCSRLX_FLUSH);
            CPU_CRITICAL_ENTER();
            DEF_BIT_SET(p_drv_data->EP_Status.HasBeenFlushed, DEF_BIT(ep_log_nbr));
            CPU_CRITICAL_EXIT();
        }
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
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_DrvISR_Handler (USBD_DRV  *p_drv)
{
    USBD_TM4C123X_REG  *p_reg;
    USBD_DRV_DATA      *p_drv_data;
    CPU_INT32U          usbis_reg;
    CPU_INT32U          usbtxis_reg;
    CPU_INT32U          usbrxis_reg;
    CPU_INT16U          rx_ep_int_bitmap;
    CPU_INT16U          tx_ep_int_bitmap;
    CPU_INT08U          ep_log_nbr;


    p_reg       = (USBD_TM4C123X_REG *)p_drv->CfgPtr->BaseAddr; /* Get USB controller register reference.               */
    p_drv_data  = (USBD_DRV_DATA     *)p_drv->DataPtr;          /* Get a ref to the drv's internal data.                */
    usbis_reg   =  p_reg->USBIS;
    usbtxis_reg =  p_reg->USBTXIS;
    usbrxis_reg =  p_reg->USBRXIS;

                                                                /* ---------------------- RESET ----------------------- */
    if (DEF_BIT_IS_SET(usbis_reg, TM4C123X_USBI_RESET) == DEF_YES) {
        p_drv_data->EP0_State = USBD_DRV_EP0_STATE_IDLE;
        USBD_EventReset(p_drv);                                 /* Notify the USB stack of a reset event.               */
    }

                                                                /* ---------------------- SUSPEND ----------------------*/
    if (DEF_BIT_IS_SET(usbis_reg, TM4C123X_USBI_SUSPEND) == DEF_YES) {
        USBD_EventSuspend(p_drv);                               /* Notify the USB stack of a suspend event.             */
    }

                                                                /* ---------------------- RESUME ---------------------- */
    if (DEF_BIT_IS_SET(usbis_reg, TM4C123X_USBI_RESUME) == DEF_YES) {
        USBD_EventResume(p_drv);                                /* Notify the USB stack of a resume event.              */
    }

                                                                /* -------------------- DISCONNECT -------------------- */
    if (DEF_BIT_IS_SET(usbis_reg, TM4C123X_USBI_DISCON) == DEF_YES) {
        USBD_EventDisconn(p_drv);                               /* Notify the USB stack of a disconnect event.          */
    }

                                                                /* -------------------- ENDPOINT 0 -------------------- */
    if (DEF_BIT_IS_SET(usbtxis_reg, TM4C123X_USBTXIS_EP0) == DEF_YES) {

        USBD_TM4C123x_EP0_ISR_Handle(p_drv, p_reg, p_drv_data);

        if (DEF_BIT_IS_SET(p_reg->USBCSRL0, TM4C123X_USBCSRL0_SETEND) == DEF_YES) {
            DEF_BIT_SET(p_reg->USBCSRL0, TM4C123X_USBCSRL0_SETENDC);
            USBD_DRV_DBG_EP0_STATS_INC(EP0_ISR_SET_END_IsSet);
        }
    }

                                                                /* ------------------- IN ENDPOINTS ------------------- */
    if (DEF_BIT_IS_SET_ANY(usbtxis_reg, TM4C123X_USBTXIE_EP1_TO_EP7)) {
        tx_ep_int_bitmap = (usbtxis_reg & TM4C123X_USBTXIE_EP1_TO_EP7);

        while (tx_ep_int_bitmap != DEF_BIT_NONE) {
            ep_log_nbr = CPU_CntTrailZeros16(tx_ep_int_bitmap);

            if (DEF_BIT_IS_SET(p_drv_data->EP_Status.HasBeenFlushed, DEF_BIT(ep_log_nbr)) == DEF_YES) {
                DEF_BIT_CLR(p_drv_data->EP_Status.HasBeenFlushed, DEF_BIT(ep_log_nbr));
                USBD_DRV_DBG_GEN_STATS_INC(TxISR_Flushed);
                USBD_EP_TxCmpl(p_drv, ep_log_nbr);
            } else if (DEF_BIT_IS_SET(p_drv_data->EP_Status.TxXferInProgress, DEF_BIT(ep_log_nbr))) {
                DEF_BIT_CLR(p_drv_data->EP_Status.TxXferInProgress, DEF_BIT(ep_log_nbr));
                USBD_DRV_DBG_GEN_STATS_INC(TxCmplCallFromISR);
                USBD_EP_TxCmpl(p_drv, ep_log_nbr);
            } else {
                USBD_DRV_DBG_GEN_STATS_INC(TxISR_ElseTxcmpl);
            }
            DEF_BIT_CLR(tx_ep_int_bitmap, DEF_BIT(ep_log_nbr));
        }
    }

                                                                /* ------------------- OUT ENDPOINTS ------------------ */
    if (DEF_BIT_IS_SET_ANY(usbrxis_reg, TM4C123X_USBRXIE_EP1_TO_EP7)) {
        rx_ep_int_bitmap = (usbrxis_reg & TM4C123X_USBRXIE_EP1_TO_EP7);

        while (rx_ep_int_bitmap != DEF_BIT_NONE) {
            ep_log_nbr = CPU_CntTrailZeros16(rx_ep_int_bitmap);

            if (DEF_BIT_IS_SET(p_drv_data->EP_Status.HasBeenFlushed, DEF_BIT(ep_log_nbr)) == DEF_YES) {
                DEF_BIT_CLR(p_drv_data->EP_Status.HasBeenFlushed, DEF_BIT(ep_log_nbr));
                USBD_DRV_DBG_GEN_STATS_INC(RxISR_Flushed);
                USBD_EP_RxCmpl(p_drv, ep_log_nbr);
            } else if (DEF_BIT_IS_SET(p_drv_data->EP_Status.RxXferInProgress, DEF_BIT(ep_log_nbr))) {
                DEF_BIT_CLR(p_drv_data->EP_Status.RxXferInProgress, DEF_BIT(ep_log_nbr));
                USBD_DRV_DBG_GEN_STATS_INC(RxCmplCallFromISR);
                USBD_EP_RxCmpl(p_drv, ep_log_nbr);
            } else if ((DEF_BIT_IS_CLR(p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBRXCSRLx, TM4C123X_USBRXCSRLX_STALLED) == DEF_YES) &&
                       (DEF_BIT_IS_CLR(p_reg->TM4C123X_EP_REG[ep_log_nbr - 1u].USBRXCSRLx, TM4C123X_USBRXCSRLX_STALL)   == DEF_YES)) {
                DEF_BIT_SET(p_drv_data->EP_Status.CallRxCmpl, DEF_BIT(ep_log_nbr));
                USBD_DRV_DBG_GEN_STATS_INC(RxISR_WillCallRxCmpl);
            } else {
                USBD_DRV_DBG_GEN_STATS_INC(RxISR_ElseCnt);
            }

            DEF_BIT_CLR(rx_ep_int_bitmap, DEF_BIT(ep_log_nbr));
        }
    }
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                      USBD_TM4C123x_EP0_ISR_Handle()
*
* Description : Endpoint 0 ISR handler.
*
* Argument(s) : p_drv       Pointer to device driver          structure.
*
*               p_reg       Pointer to device driver register structure.
*
*               p_drv_data  Pointer to device driver data     structure.
*
* Return(s)   : none.
*
* Note(s)     : (1) See note about enum USBD_TM4C123X_EP0_STATE above for more details about the state
*                   machine for endpoint 0.
*********************************************************************************************************
*/

static  void  USBD_TM4C123x_EP0_ISR_Handle(USBD_DRV           *p_drv,
                                           USBD_TM4C123X_REG  *p_reg,
                                           USBD_DRV_DATA      *p_drv_data)
{
    CPU_INT32U   ep_status;
    CPU_INT32U   setup_pkt[2u];
    CPU_INT08U  *p_buf_08;
    CPU_INT16U   len;
    CPU_INT16U   wLength;


    ep_status = p_reg->USBCSRL0;

    switch (p_drv_data->EP0_State) {
        case USBD_DRV_EP0_STATE_WAITING_FOR_TX_CMPL:
             USBD_DRV_DBG_EP0_STATS_INC(EP0_WaitTxCmpl);
                                                                /* See Note #2 of USBD_DrvEP_RxStart().                 */
             p_drv_data->EP0_State = USBD_DRV_EP0_STATE_WAITING_FOR_RX_ZLP_CALL;
             USBD_EP_TxCmpl(p_drv, 0u);
             break;


        case USBD_DRV_EP0_STATE_WAITING_FOR_RX_CMPL:
             USBD_DRV_DBG_EP0_STATS_INC(EP0_WaitRxCmpl);
             p_drv_data->EP0_State = USBD_DRV_EP0_STATE_WAITING_FOR_TX_ZLP_CALL;
             USBD_EP_RxCmpl(p_drv, 0u);
             break;


        case USBD_DRV_EP0_STATE_WAITING_FOR_TX_ZLP_CMPL:
             USBD_DRV_DBG_EP0_STATS_INC(EP0_WaitTxZLP_Cmpl);
             p_drv_data->EP0_State = USBD_DRV_EP0_STATE_STATUS;
             USBD_EP_TxCmpl(p_drv, 0u);
             break;


        case USBD_DRV_EP0_STATE_STATUS:
             USBD_DRV_DBG_EP0_STATS_INC(EP0_StatusCntNbr);
        case USBD_DRV_EP0_STATE_IDLE:
             USBD_DRV_DBG_EP0_STATS_INC(EP0_IdleCntNbr);
        case USBD_DRV_EP0_STATE_WAITING_FOR_RX_ZLP_CALL:
        default:
             USBD_DRV_DBG_EP0_STATS_INC(EP0_Default);
             if (DEF_BIT_IS_SET(ep_status, TM4C123X_USBCSRL0_RXRDY) == DEF_YES) {
                 USBD_DRV_DBG_EP0_STATS_INC(EP0_IdleCntNbrRXRDY);
                 len = p_reg->USBCOUNT0;

                 if (len == 8u) {
                     USBD_DRV_DBG_EP0_STATS_INC(EP0_IdleCntNbrRXRDY_Len8);
                     setup_pkt[0u] = p_reg->USBFIFOx[0u].EPDATA;
                     setup_pkt[1u] = p_reg->USBFIFOx[0u].EPDATA;

                     p_buf_08 = (CPU_INT08U *)setup_pkt;

                     wLength = MEM_VAL_GET_INT16U_LITTLE(p_buf_08 + 6u);
                     if (wLength != 0u) {
                         if ((p_buf_08[0u] & USBD_REQ_DIR_MASK) == USBD_REQ_DIR_DEVICE_TO_HOST) {
                             USBD_DRV_DBG_EP0_STATS_INC(EP0_IdleGoingToWaitTxCall);
                             p_drv_data->EP0_State = USBD_DRV_EP0_STATE_WAITING_FOR_TX_CALL;
                         } else {
                             USBD_DRV_DBG_EP0_STATS_INC(EP0_IdleGoingToWaitRxCall);
                             p_drv_data->EP0_State = USBD_DRV_EP0_STATE_WAITING_FOR_RX_CALL;
                         }
                     } else {
                         if ((p_buf_08[0u] & USBD_REQ_DIR_MASK) != USBD_REQ_DIR_DEVICE_TO_HOST) {
                             USBD_DRV_DBG_EP0_STATS_INC(EP0_IdleGoingToWaitTxZLP_Call);
                             p_drv_data->EP0_State = USBD_DRV_EP0_STATE_WAITING_FOR_TX_ZLP_CALL;
                         } else {
                             USBD_DRV_DBG_TRAP();
                         }
                     }

                     USBD_EventSetup(p_drv, (void *)&setup_pkt[0]);
                 } else if (len == 0u) {
                     USBD_DRV_DBG_EP0_STATS_INC(EP0_IdleCntNbrRXRDY_Len0);
                 } else {
                     USBD_DRV_DBG_TRAP();
                 }
             }
             break;
    }
}
