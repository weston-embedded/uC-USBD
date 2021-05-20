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
*                                              AT32UC3x
*
* Filename : usbd_drv_at32uc3x.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)  : (1) You can find specific information about this driver at:
*                https://doc.micrium.com/display/USBDDRV/AT32UC3x
*
*            (2) With an appropriate BSP, this device driver will support the USB device module on
*                the Atmel 32-bit AVR UC3 A(0 and 1) and B series.
*
*            (3) The device controller does not support synthetizing a corrupted SOF packet. It
*                reports an error flag meaning that the software should handle it properly.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                              INCLUDES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#include  "../../Source/usbd_core.h"
#include  "usbd_drv_at32uc3x.h"


/*
*********************************************************************************************************
*                                            BIT DEFINES
*********************************************************************************************************
*/

                                                                /* --------------- USB GENERAL REGISTERS -------------- */
                                                                /* General Control Register.                            */
#define  AT32UC3x_USBB_USBCON_IDTE                     DEF_BIT_00
#define  AT32UC3x_USBB_USBCON_VBUSTE                   DEF_BIT_01
#define  AT32UC3x_USBB_USBCON_VBERRE                   DEF_BIT_03
#define  AT32UC3x_USBB_USBCON_BCERRE                   DEF_BIT_04
#define  AT32UC3x_USBB_USBCON_ROLEEXE                  DEF_BIT_05
#define  AT32UC3x_USBB_USBCON_STOE                     DEF_BIT_07
#define  AT32UC3x_USBB_USBCON_VBUSHWC                  DEF_BIT_08
#define  AT32UC3x_USBB_USBCON_OTGPADE                  DEF_BIT_12
#define  AT32UC3x_USBB_USBCON_VBUSPO                   DEF_BIT_13
#define  AT32UC3x_USBB_USBCON_FRZCLK                   DEF_BIT_14
#define  AT32UC3x_USBB_USBCON_USBE                     DEF_BIT_15
#define  AT32UC3x_USBB_USBCON_TIMVALUE                 DEF_BIT_FIELD_32(2u, 16u)
#define  AT32UC3x_USBB_USBCON_TIMPAGE                  DEF_BIT_FIELD_32(2u, 20u)
#define  AT32UC3x_USBB_USBCON_UNLOCK                   DEF_BIT_22
#define  AT32UC3x_USBB_USBCON_UIDE                     DEF_BIT_24
#define  AT32UC3x_USBB_USBCON_UIMOD                    DEF_BIT_25
#define  AT32UC3x_USBB_USBCON_UIMOD_DEV                DEF_BIT_25
#define  AT32UC3x_USBB_USBCON_UIMOD_HOST               DEF_BIT_NONE

                                                                /* General Status Register.                             */
#define  AT32UC3x_USBB_USBSTA_IDTI                     DEF_BIT_00
#define  AT32UC3x_USBB_USBSTA_VBUSTI                   DEF_BIT_01
#define  AT32UC3x_USBB_USBSTA_VBERRI                   DEF_BIT_03
#define  AT32UC3x_USBB_USBSTA_BCERRI                   DEF_BIT_04
#define  AT32UC3x_USBB_USBSTA_ROLEEXI                  DEF_BIT_05
#define  AT32UC3x_USBB_USBSTA_STOI                     DEF_BIT_07
#define  AT32UC3x_USBB_USBSTA_VBUSRQ                   DEF_BIT_09
#define  AT32UC3x_USBB_USBSTA_ID                       DEF_BIT_10
#define  AT32UC3x_USBB_USBSTA_VBUS                     DEF_BIT_11
#define  AT32UC3x_USBB_USBSTA_SPEED                    DEF_BIT_FIELD_32(2u, 12u)
#define  AT32UC3x_USBB_USBSTA_SPEED_FULL               DEF_BIT_NONE
#define  AT32UC3x_USBB_USBSTA_SPEED_LOW                DEF_BIT_13

                                                                /* Version Register.                                    */
#define  AT32UC3x_USBB_UVERS_VERSION                   DEF_BIT_FIELD_32(12u, 0u)
#define  AT32UC3x_USBB_UVERS_VARIANT                   DEF_BIT_FIELD_32(4u, 16u)

                                                                /* Features Register.                                   */
#define  AT32UC3x_USBB_UFEATURES_EPTNBRMAX             DEF_BIT_FIELD_32(4u, 0u)
#define  AT32UC3x_USBB_UFEATURES_DMACHANNELNBR         DEF_BIT_FIELD_32(3u, 4u)
#define  AT32UC3x_USBB_UFEATURES_DMABUFFERSIZE         DEF_BIT_07
#define  AT32UC3x_USBB_UFEATURES_DMAFIFOWORDDEPTH      DEF_BIT_FIELD_32(4u, 8u)
#define  AT32UC3x_USBB_UFEATURES_FIFOMAXSIZE           DEF_BIT_FIELD_32(3u, 12u)
#define  AT32UC3x_USBB_UFEATURES_BYTEWRITEDPRAM        DEF_BIT_15

                                                                /* Address Size Register.                               */
#define  AT32UC3x_USBB_UADDRSIZE_UADDRSIZE             DEF_BIT_FIELD_32(32u, 0u)

                                                                /* Name Register 1.                                     */
#define  AT32UC3x_USBB_UNAME1_UNAME1                   DEF_BIT_FIELD_32(32u, 0u)

                                                                /* Name Register 2.                                     */
#define  AT32UC3x_USBB_UNAME2_UNAME2                   DEF_BIT_FIELD_32(32u, 0u)

                                                                /* Finite State Machine Status Register.                */
#define  AT32UC3x_USBB_USBFSM_DRDSTATE                 DEF_BIT_FIELD_32(4u, 0u)

                                                                /* --------------- USB DEVICE REGISTERS --------------- */
                                                                /* Device General Control Register.                     */
#define  AT32UC3x_USBB_UDCON_UADD                      DEF_BIT_FIELD_32(7u, 0u)
#define  AT32UC3x_USBB_UDCON_ADDEN                     DEF_BIT_07
#define  AT32UC3x_USBB_UDCON_DETACH                    DEF_BIT_08
#define  AT32UC3x_USBB_UDCON_RMWKUP                    DEF_BIT_09
#define  AT32UC3x_USBB_UDCON_LS                        DEF_BIT_12

                                                                /* Device Global Interrupt Register.                    */
#define  AT32UC3x_USBB_UDINT_SUSP                      DEF_BIT_00
#define  AT32UC3x_USBB_UDINT_SOF                       DEF_BIT_02
#define  AT32UC3x_USBB_UDINT_EORST                     DEF_BIT_03
#define  AT32UC3x_USBB_UDINT_WAKEUP                    DEF_BIT_04
#define  AT32UC3x_USBB_UDINT_EORSM                     DEF_BIT_05
#define  AT32UC3x_USBB_UDINT_UPRSM                     DEF_BIT_06
#define  AT32UC3x_USBB_UDINT_EP0INT                    DEF_BIT_12
#define  AT32UC3x_USBB_UDINT_EP1INT                    DEF_BIT_13
#define  AT32UC3x_USBB_UDINT_EP2INT                    DEF_BIT_14
#define  AT32UC3x_USBB_UDINT_EP3INT                    DEF_BIT_15
#define  AT32UC3x_USBB_UDINT_EP4INT                    DEF_BIT_16
#define  AT32UC3x_USBB_UDINT_EP5INT                    DEF_BIT_17
#define  AT32UC3x_USBB_UDINT_EP6INT                    DEF_BIT_18
#define  AT32UC3x_USBB_UDINT_DMA1INT                   DEF_BIT_25
#define  AT32UC3x_USBB_UDINT_DMA2INT                   DEF_BIT_26
#define  AT32UC3x_USBB_UDINT_DMA3INT                   DEF_BIT_27
#define  AT32UC3x_USBB_UDINT_DMA4INT                   DEF_BIT_28
#define  AT32UC3x_USBB_UDINT_DMA5INT                   DEF_BIT_29
#define  AT32UC3x_USBB_UDINT_DMA6INT                   DEF_BIT_30

                                                                /* Endpoint Enable/Reset Register.                      */
#define  AT32UC3x_USBB_UERST_EPEN0                     DEF_BIT_00
#define  AT32UC3x_USBB_UERST_EPEN1                     DEF_BIT_01
#define  AT32UC3x_USBB_UERST_EPEN2                     DEF_BIT_02
#define  AT32UC3x_USBB_UERST_EPEN3                     DEF_BIT_03
#define  AT32UC3x_USBB_UERST_EPEN4                     DEF_BIT_04
#define  AT32UC3x_USBB_UERST_EPEN5                     DEF_BIT_05
#define  AT32UC3x_USBB_UERST_EPEN6                     DEF_BIT_06
#define  AT32UC3x_USBB_UERST_EPRST0                    DEF_BIT_16
#define  AT32UC3x_USBB_UERST_EPRST1                    DEF_BIT_17
#define  AT32UC3x_USBB_UERST_EPRST2                    DEF_BIT_18
#define  AT32UC3x_USBB_UERST_EPRST3                    DEF_BIT_19
#define  AT32UC3x_USBB_UERST_EPRST4                    DEF_BIT_20
#define  AT32UC3x_USBB_UERST_EPRST5                    DEF_BIT_21
#define  AT32UC3x_USBB_UERST_EPRST6                    DEF_BIT_22

                                                                /* Device Frame Number Register.                        */
#define  AT32UC3x_USBB_UDFNUM_FNUM                     DEF_BIT_FIELD_32(11u, 3u)
#define  AT32UC3x_USBB_UDFNUM_FNCERR                   DEF_BIT_15

                                                                /* Endpoint n Configuration Register.                   */
#define  AT32UC3x_USBB_UECFGN_ALLOC                    DEF_BIT_01
#define  AT32UC3x_USBB_UECFGN_EPBK                     DEF_BIT_FIELD_32(2u, 2u)
#define  AT32UC3x_USBB_UECFGN_EPBK_SINGLE              DEF_BIT_NONE
#define  AT32UC3x_USBB_UECFGN_EPBK_DOUBLE              DEF_BIT_02
#define  AT32UC3x_USBB_UECFGN_EPBK_TRIPLE              DEF_BIT_03
#define  AT32UC3x_USBB_UECFGN_EPSIZE                   DEF_BIT_FIELD_32(3u, 4u)
#define  AT32UC3x_USBB_UECFGN_EPSIZE_8                 DEF_BIT_NONE
#define  AT32UC3x_USBB_UECFGN_EPSIZE_16                DEF_BIT_04
#define  AT32UC3x_USBB_UECFGN_EPSIZE_32                DEF_BIT_05
#define  AT32UC3x_USBB_UECFGN_EPSIZE_64                DEF_BIT_FIELD_32(2u, 4u)
#define  AT32UC3x_USBB_UECFGN_EPSIZE_128               DEF_BIT_06
#define  AT32UC3x_USBB_UECFGN_EPSIZE_256              (DEF_BIT_04 | DEF_BIT_06)
#define  AT32UC3x_USBB_UECFGN_EPSIZE_512               DEF_BIT_FIELD_32(2u, 5u)
#define  AT32UC3x_USBB_UECFGN_EPSIZE_1024              DEF_BIT_FIELD_32(3u, 4u)
#define  AT32UC3x_USBB_UECFGN_EPDIR                    DEF_BIT_08
#define  AT32UC3x_USBB_UECFGN_EPDIR_OUT                DEF_BIT_NONE
#define  AT32UC3x_USBB_UECFGN_EPDIR_IN                 DEF_BIT_08
#define  AT32UC3x_USBB_UECFGN_EPTYPE                   DEF_BIT_FIELD_32(2u, 11u)
#define  AT32UC3x_USBB_UECFGN_EPTYPE_CTRL              DEF_BIT_NONE
#define  AT32UC3x_USBB_UECFGN_EPTYPE_ISOC              DEF_BIT_11
#define  AT32UC3x_USBB_UECFGN_EPTYPE_BULK              DEF_BIT_12
#define  AT32UC3x_USBB_UECFGN_EPTYPE_INTR              DEF_BIT_FIELD_32(2u, 11u)

                                                                /* Endpoint n Status Register.                          */
#define  AT32UC3x_USBB_UESTAN_TXINI                    DEF_BIT_00
#define  AT32UC3x_USBB_UESTAN_RXOUTI                   DEF_BIT_01
#define  AT32UC3x_USBB_UESTAN_RXSTPI_UNDERFI           DEF_BIT_02
#define  AT32UC3x_USBB_UESTAN_NAKOUTI                  DEF_BIT_03
#define  AT32UC3x_USBB_UESTAN_NAKINI                   DEF_BIT_04
#define  AT32UC3x_USBB_UESTAN_STALLED_CRCERRI          DEF_BIT_06
#define  AT32UC3x_USBB_UESTAN_SHORT_PACKET             DEF_BIT_07
#define  AT32UC3x_USBB_UESTAN_BASE_ISR_EN_MASK         DEF_BIT_FIELD(8u, 0u)
#define  AT32UC3x_USBB_UESTAN_DTSEQ                    DEF_BIT_FIELD_32(2u, 8u)
#define  AT32UC3x_USBB_UESTAN_DTSEQ_DATA0              DEF_BIT_NONE
#define  AT32UC3x_USBB_UESTAN_DTSEQ_DATA1              DEF_BIT_08
#define  AT32UC3x_USBB_UESTAN_NBUSYBK                  DEF_BIT_FIELD_32(2u, 12u)
#define  AT32UC3x_USBB_UESTAN_NBUSYBK_0                DEF_BIT_NONE
#define  AT32UC3x_USBB_UESTAN_NBUSYBK_1                DEF_BIT_12
#define  AT32UC3x_USBB_UESTAN_NBUSYBK_2                DEF_BIT_13
#define  AT32UC3x_USBB_UESTAN_NBUSYBK_3                DEF_BIT_FIELD_32(2u, 12u)
#define  AT32UC3x_USBB_UESTAN_CURRBK                   DEF_BIT_FIELD_32(2u, 14u)
#define  AT32UC3x_USBB_UESTAN_CURRBK_0                 DEF_BIT_NONE
#define  AT32UC3x_USBB_UESTAN_CURRBK_1                 DEF_BIT_14
#define  AT32UC3x_USBB_UESTAN_CURRBK_2                 DEF_BIT_15
#define  AT32UC3x_USBB_UESTAN_RWALL                    DEF_BIT_16
#define  AT32UC3x_USBB_UESTAN_CTRLDIR                  DEF_BIT_17
#define  AT32UC3x_USBB_UESTAN_CFGOK                    DEF_BIT_18
#define  AT32UC3x_USBB_UESTAN_BYCT                     DEF_BIT_FIELD_32(11u, 20u)

                                                                /* Endpoint n Control Register.                         */
#define  AT32UC3x_USBB_UECONN_TXINE                    DEF_BIT_00
#define  AT32UC3x_USBB_UECONN_RXOUTE                   DEF_BIT_01
#define  AT32UC3x_USBB_UECONN_RXSTPE_UNDERFE           DEF_BIT_02
#define  AT32UC3x_USBB_UECONN_NAKOUTE                  DEF_BIT_03
#define  AT32UC3x_USBB_UECONN_NAKINE                   DEF_BIT_04
#define  AT32UC3x_USBB_UECONN_OVERFE                   DEF_BIT_05
#define  AT32UC3x_USBB_UECONN_STALLEDE_CRCERRE         DEF_BIT_06
#define  AT32UC3x_USBB_UECONN_SHORT_PACKETE            DEF_BIT_07
#define  AT32UC3x_USBB_UECONN_NBUSYBKE                 DEF_BIT_12
#define  AT32UC3x_USBB_UECONN_KILLBK                   DEF_BIT_13
#define  AT32UC3x_USBB_UECONN_FIFOCON                  DEF_BIT_14
#define  AT32UC3x_USBB_UECONN_EPDISHDMA                DEF_BIT_16
#define  AT32UC3x_USBB_UECONN_RSTDT                    DEF_BIT_18
#define  AT32UC3x_USBB_UECONN_STALLRQ                  DEF_BIT_19

                                                                /* Device DMA Channel n Next Descriptor Address Reg.    */
#define  AT32UC3x_USBB_UDDMANNEXTDESC_NXTDESCADDR      DEF_BIT_FIELD_32(28u, 4u)

                                                                /* Device DMA Channel n HSB Address Register.           */
#define  AT32UC3x_USBB_UDDMANADDR_HSBADDR              DEF_BIT_FIELD_32(32u, 0u)

                                                                /* Device DMA Channel n Control Register.               */
#define  AT32UC3x_USBB_UDDMANCONTROL_CHEN              DEF_BIT_00
#define  AT32UC3x_USBB_UDDMANCONTROL_LDNXTCHDESCEN     DEF_BIT_01
#define  AT32UC3x_USBB_UDDMANCONTROL_BUFFCLOSEINEN     DEF_BIT_02
#define  AT32UC3x_USBB_UDDMANCONTROL_DMAENDEN          DEF_BIT_03
#define  AT32UC3x_USBB_UDDMANCONTROL_EOTIRQEN          DEF_BIT_04
#define  AT32UC3x_USBB_UDDMANCONTROL_EOBUFFIRQEN       DEF_BIT_05
#define  AT32UC3x_USBB_UDDMANCONTROL_DESCLDIRQEN       DEF_BIT_06
#define  AT32UC3x_USBB_UDDMANCONTROL_BURSTLOCKEN       DEF_BIT_07
#define  AT32UC3x_USBB_UDDMANCONTROL_CHBYTELENGTH      DEF_BIT_FIELD_32(16u, 16u)

                                                                /* Device DMA Channel n Status Register.                */
#define  AT32UC3x_USBB_UDDMANSTATUS_CHEN               DEF_BIT_00
#define  AT32UC3x_USBB_UDDMANSTATUS_CHACTIVE           DEF_BIT_01
#define  AT32UC3x_USBB_UDDMANSTATUS_EOTSTA             DEF_BIT_04
#define  AT32UC3x_USBB_UDDMANSTATUS_EOCHBUFFSTA        DEF_BIT_05
#define  AT32UC3x_USBB_UDDMANSTATUS_DESCLDSTA          DEF_BIT_06
#define  AT32UC3x_USBB_UDDMANSTATUS_CHBYTECNT          DEF_BIT_FIELD_32(16u, 16u)


/*
*********************************************************************************************************
*                                 AT32UC3x OTG USB DEVICE CONSTRAINTS
*********************************************************************************************************
*/

#define  AT32UC3x_NBR_EPS                                 7u      /* Maximum number of endpoints.                       */
#define  AT32UC3x_USBB_SLAVE_MEM_SIZE            0x00010000u      /* USB physical memory size (64kb).                   */


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                    AT32UC3x REGISTERS DATA TYPE
*********************************************************************************************************
*/

typedef  struct  usbd_at32uc3x_reg {
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
    CPU_REG32  UECFGN[AT32UC3x_NBR_EPS];                        /* Endpoint n config.                                   */
    CPU_REG32  RSVD01[5];
    CPU_REG32  UESTAN[AT32UC3x_NBR_EPS];                        /* Endpoint n status.                                   */
    CPU_REG32  RSVD02[5];
    CPU_REG32  UESTANCLR[AT32UC3x_NBR_EPS];                     /* Endpoint n status clear.                             */
    CPU_REG32  RSVD03[5];
    CPU_REG32  UESTANSET[AT32UC3x_NBR_EPS];                     /* Endpoint n status set.                               */
    CPU_REG32  RSVD04[5];
    CPU_REG32  UECONN[AT32UC3x_NBR_EPS];                        /* Endpoint n control.                                  */
    CPU_REG32  RSVD05[5];
    CPU_REG32  UECONNSET[AT32UC3x_NBR_EPS];                     /* Endpoint n control set.                              */
    CPU_REG32  RSVD06[5];
    CPU_REG32  UECONNCLR[AT32UC3x_NBR_EPS];                     /* Endpoint n control clear.                            */
    CPU_REG32  RSVD07[53];
    CPU_REG32  UDDMA1NEXTDESC;                                  /* DMA Channel 1 Next Descriptor Address.               */
    CPU_REG32  UDDMA1ADDR;                                      /* DMA Channel 1 HSB Address.                           */
    CPU_REG32  UDDMA1CONTROL;                                   /* DMA Channel 1 Control.                               */
    CPU_REG32  UDDMA1STATUS;                                    /* DMA Channel 1 Status.                                */
    CPU_REG32  UDDMA2NEXTDESC;                                  /* DMA Channel 2 Next Descriptor Address.               */
    CPU_REG32  UDDMA2ADDR;                                      /* DMA Channel 2 HSB Address.                           */
    CPU_REG32  UDDMA2CONTROL;                                   /* DMA Channel 2 Control.                               */
    CPU_REG32  UDDMA2STATUS;                                    /* DMA Channel 2 Status.                                */
    CPU_REG32  UDDMA3NEXTDESC;                                  /* DMA Channel 3 Next Descriptor Address.               */
    CPU_REG32  UDDMA3ADDR;                                      /* DMA Channel 3 HSB Address.                           */
    CPU_REG32  UDDMA3CONTROL;                                   /* DMA Channel 3 Control.                               */
    CPU_REG32  UDDMA3STATUS;                                    /* DMA Channel 3 Status.                                */
    CPU_REG32  UDDMA4NEXTDESC;                                  /* DMA Channel 4 Next Descriptor Address.               */
    CPU_REG32  UDDMA4ADDR;                                      /* DMA Channel 4 HSB Address.                           */
    CPU_REG32  UDDMA4CONTROL;                                   /* DMA Channel 4 Control.                               */
    CPU_REG32  UDDMA4STATUS;                                    /* DMA Channel 4 Status.                                */
    CPU_REG32  UDDMA5NEXTDESC;                                  /* DMA Channel 5 Next Descriptor Address.               */
    CPU_REG32  UDDMA5ADDR;                                      /* DMA Channel 5 HSB Address.                           */
    CPU_REG32  UDDMA5CONTROL;                                   /* DMA Channel 5 Control.                               */
    CPU_REG32  UDDMA5STATUS;                                    /* DMA Channel 5 Status.                                */
    CPU_REG32  UDDMA6NEXTDESC;                                  /* DMA Channel 6 Next Descriptor Address.               */
    CPU_REG32  UDDMA6ADDR;                                      /* DMA Channel 6 HSB Address.                           */
    CPU_REG32  UDDMA6CONTROL;                                   /* DMA Channel 6 Control.                               */
    CPU_REG32  UDDMA6STATUS;                                    /* DMA Channel 6 Status.                                */
    CPU_REG32  RSVD08[36];

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
    CPU_REG32  UHADDR1;                                         /* Host Address 1.                                      */
    CPU_REG32  UHADDR2;                                         /* Host Address 2.                                      */
    CPU_REG32  RSVD09[53];
    CPU_REG32  UPCFGN[AT32UC3x_NBR_EPS];                        /* Pipe n configuration.                                */
    CPU_REG32  RSVD10[5];
    CPU_REG32  UPSTAN[AT32UC3x_NBR_EPS];                        /* Pipe n status.                                       */
    CPU_REG32  RSVD11[5];
    CPU_REG32  UPSTANCLR[AT32UC3x_NBR_EPS];                     /* Pipe n status clear.                                 */
    CPU_REG32  RSVD12[5];
    CPU_REG32  UPSTANSET[AT32UC3x_NBR_EPS];                     /* Pipe n status set.                                   */
    CPU_REG32  RSVD13[5];
    CPU_REG32  UPCONN[AT32UC3x_NBR_EPS];                        /* Pipe n control.                                      */
    CPU_REG32  RSVD14[5];
    CPU_REG32  UPCONNSET[AT32UC3x_NBR_EPS];                     /* Pipe n control set.                                  */
    CPU_REG32  RSVD15[5];
    CPU_REG32  UPCONNCLR[AT32UC3x_NBR_EPS];                     /* Pipe n control clear.                                */
    CPU_REG32  RSVD16[5];
    CPU_REG32  UPINRQN[AT32UC3x_NBR_EPS];                       /* Pipe n IN request.                                   */
    CPU_REG32  RSVD17[5];
    CPU_REG32  UPERRN[AT32UC3x_NBR_EPS];                        /* Pipe n Error.                                        */
    CPU_REG32  RSVD18[29];
    CPU_REG32  UHDMA1NEXTDESC;                                  /* Host DMA Channel 1 Next Descriptor Address.          */
    CPU_REG32  UHDMA1ADDR;                                      /* Host DMA Channel 1 HSB Address.                      */
    CPU_REG32  UHDMA1CONTROL;                                   /* Host DMA Channel 1 Control.                          */
    CPU_REG32  UHDMA1STATUS;                                    /* Host DMA Channel 1 Status.                           */
    CPU_REG32  UHDMA2NEXTDESC;                                  /* Host DMA Channel 2 Next Descriptor Address.          */
    CPU_REG32  UHDMA2ADDR;                                      /* Host DMA Channel 2 HSB Address.                      */
    CPU_REG32  UHDMA2CONTROL;                                   /* Host DMA Channel 2 Control.                          */
    CPU_REG32  UHDMA2STATUS;                                    /* Host DMA Channel 2 Status.                           */
    CPU_REG32  UHDMA3NEXTDESC;                                  /* Host DMA Channel 3 Next Descriptor Address.          */
    CPU_REG32  UHDMA3ADDR;                                      /* Host DMA Channel 3 HSB Address.                      */
    CPU_REG32  UHDMA3CONTROL;                                   /* Host DMA Channel 3 Control.                          */
    CPU_REG32  UHDMA3STATUS;                                    /* Host DMA Channel 3 Status.                           */
    CPU_REG32  UHDMA4NEXTDESC;                                  /* Host DMA Channel 4 Next Descriptor Address.          */
    CPU_REG32  UHDMA4ADDR;                                      /* Host DMA Channel 4 HSB Address.                      */
    CPU_REG32  UHDMA4CONTROL;                                   /* Host DMA Channel 4 Control.                          */
    CPU_REG32  UHDMA4STATUS;                                    /* Host DMA Channel 4 Status.                           */
    CPU_REG32  UHDMA5NEXTDESC;                                  /* Host DMA Channel 5 Next Descriptor Address.          */
    CPU_REG32  UHDMA5ADDR;                                      /* Host DMA Channel 5 HSB Address.                      */
    CPU_REG32  UHDMA5CONTROL;                                   /* Host DMA Channel 5 Control.                          */
    CPU_REG32  UHDMA5STATUS;                                    /* Host DMA Channel 5 Status.                           */
    CPU_REG32  UHDMA6NEXTDESC;                                  /* Host DMA Channel 6 Next Descriptor Address.          */
    CPU_REG32  UHDMA6ADDR;                                      /* Host DMA Channel 6 HSB Address.                      */
    CPU_REG32  UHDMA6CONTROL;                                   /* Host DMA Channel 6 Control.                          */
    CPU_REG32  UHDMA6STATUS;                                    /* Host DMA Channel 6 Status.                           */
    CPU_REG32  RSVD19[36];

                                                                /* ------- GLOBAL CONTROL AND STATUS REGISTERS -------- */
    CPU_REG32  USBCON;                                          /* General control.                                     */
    CPU_REG32  USBSTA;                                          /* General status.                                      */
    CPU_REG32  USBSTACLR;                                       /* General status clear.                                */
    CPU_REG32  USBSTASET;                                       /* General status set.                                  */
    CPU_REG32  RSVD20[2];
    CPU_REG32  UVERS;                                           /* IP version.                                          */
    CPU_REG32  UFEATURES;                                       /* IP features.                                         */
    CPU_REG32  UADDRSIZE;                                       /* IP PB address size.                                  */
    CPU_REG32  UNAME1;                                          /* IP name register 1.                                  */
    CPU_REG32  UNAME2;                                          /* IP name register 2.                                  */
    CPU_REG32  USBFSM;                                          /* USB finite state machine status.                     */
} USBD_AT32UC3x_REG;


/*
*********************************************************************************************************
*                                          DRIVER DATA TYPE
*********************************************************************************************************
*/

typedef  struct  usbd_drv_data {                                /* ---------- DEVICE ENDPOINT DATA STRUCTURE ---------- */
    CPU_INT16U   EP_MaxPktSize[AT32UC3x_NBR_EPS];

    CPU_INT08U  *CtrlOutDataBufPtr;
    CPU_INT16U   CtrlOutDataBufLen;
    CPU_INT16U   CtrlOutDataRxLen;
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

USBD_DRV_API  USBD_DrvAPI_AT32UC3x = { USBD_DrvInit,
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
    USBD_DRV_BSP_API   *p_bsp_api;
    USBD_AT32UC3x_REG  *p_reg;
    CPU_SR_ALLOC();


    p_reg     = (USBD_AT32UC3x_REG *)p_drv->CfgPtr->BaseAddr;
    p_bsp_api =  p_drv->BSP_API_Ptr;

    CPU_CRITICAL_ENTER();

    if (p_bsp_api->Conn != 0) {
        p_bsp_api->Conn();
    }

    DEF_BIT_SET(p_reg->USBCON,                                  /* Enable USB pads.                                     */
                AT32UC3x_USBB_USBCON_OTGPADE);

    if (DEF_BIT_IS_SET(p_reg->USBSTA, AT32UC3x_USBB_USBSTA_ID) == DEF_NO) {

        DEF_BIT_CLR(p_reg->USBCON,                              /* Overwrite ID with SW setting.                        */
                    AT32UC3x_USBB_USBCON_UIDE);

        DEF_BIT_SET(p_reg->USBCON,                              /* Force device mode.                                   */
                    AT32UC3x_USBB_USBCON_UIMOD);
    }

    DEF_BIT_SET(p_reg->USBSTACLR,                               /* Clear ID change event indication.                    */
                AT32UC3x_USBB_USBSTA_IDTI);

    DEF_BIT_SET(p_reg->USBSTACLR,                               /* Clear VBUS change event indication.                  */
                AT32UC3x_USBB_USBSTA_VBUSTI);

    DEF_BIT_CLR(p_reg->USBCON,                                  /* Reset USB controller.                                */
                AT32UC3x_USBB_USBCON_USBE);

    DEF_BIT_SET(p_reg->USBCON,                                  /* Enable USB transceiver                               */
                AT32UC3x_USBB_USBCON_USBE);

    DEF_BIT_CLR(p_reg->USBCON,                                  /* Enable clk.                                          */
                AT32UC3x_USBB_USBCON_FRZCLK);

    DEF_BIT_SET(p_reg->UDINTESET,                               /* End of reset interrupt.                              */
                AT32UC3x_USBB_UDINT_EORST);

    DEF_BIT_SET(p_reg->UDINTESET,                               /* Suspend interrupt.                                   */
                AT32UC3x_USBB_UDINT_SUSP);

    DEF_BIT_SET(p_reg->UDINTESET,                               /* End of resume interrupt.                             */
                AT32UC3x_USBB_UDINT_EORSM);

    p_reg->UDINTCLR = 0xFFFFFFFFu;                              /* Acknowledge all pending interrupts.                  */

    DEF_BIT_CLR(p_reg->UDCON,                                   /* Reconnect the device.                                */
                AT32UC3x_USBB_UDCON_DETACH);

    CPU_CRITICAL_EXIT();

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
    USBD_AT32UC3x_REG  *p_reg;
    CPU_SR_ALLOC();


    p_reg     = (USBD_AT32UC3x_REG *)p_drv->CfgPtr->BaseAddr;
    p_bsp_api =  p_drv->BSP_API_Ptr;

    CPU_CRITICAL_ENTER();

    if (p_bsp_api->Disconn != 0) {
        p_bsp_api->Disconn();
    }

    p_reg->UDINTECLR = 0xFFFFFFFFu;                             /* Disable all interrupts in UDINTE.                    */
    p_reg->UDINTCLR  = 0xFFFFFFFFu;                             /* Acknowledge all interrupts in UDINT.                 */

    DEF_BIT_SET(p_reg->UDCON,                                   /* Disconnect from USB host                             */
                AT32UC3x_USBB_UDCON_DETACH);

    DEF_BIT_CLR(p_reg->USBCON,                                  /* Disable USB transceiver                              */
                AT32UC3x_USBB_USBCON_USBE);

    CPU_CRITICAL_EXIT();
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
    USBD_AT32UC3x_REG  *p_reg;


    p_reg = (USBD_AT32UC3x_REG *)p_drv->CfgPtr->BaseAddr;

    p_reg->UDCON |= dev_addr;                                   /* Set device address bitfield.                        */
    DEF_BIT_CLR(p_reg->UDCON, AT32UC3x_USBB_UDCON_ADDEN);       /* Disable device address bitfield.                    */

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
    USBD_AT32UC3x_REG  *p_reg;


    (void)dev_addr;

    p_reg = (USBD_AT32UC3x_REG *)p_drv->CfgPtr->BaseAddr;

    DEF_BIT_SET(p_reg->UDCON, AT32UC3x_USBB_UDCON_ADDEN);       /* Enable device address bitfield.                      */
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
    USBD_AT32UC3x_REG  *p_reg;
    CPU_INT16U          frame_nbr;


    p_reg = (USBD_AT32UC3x_REG *)p_drv->CfgPtr->BaseAddr;

    frame_nbr = (CPU_INT16U)(p_reg->UDFNUM & AT32UC3x_USBB_UDFNUM_FNUM);

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
    USBD_AT32UC3x_REG  *p_reg;
    USBD_DRV_DATA      *p_drv_data;
    CPU_BOOLEAN         ep_dir_in;
    CPU_INT08U          ep_log_nbr;
    CPU_INT32U          uecfgn_reg;


    (void)transaction_frame;

    p_reg       = (USBD_AT32UC3x_REG *)p_drv->CfgPtr->BaseAddr;
    p_drv_data  = (USBD_DRV_DATA     *)p_drv->DataPtr;
    ep_log_nbr  =  USBD_EP_ADDR_TO_LOG(ep_addr);                    /* Extract the endpoint id out of address.          */
    ep_dir_in   =  USBD_EP_IS_IN(ep_addr);                          /* Extract the endpoint direction.                  */
    uecfgn_reg  =  DEF_BIT_NONE;


    switch (ep_type) {                                              /* ----------- CONFIGURE ENDPOINT TYPE ------------ */
        case USBD_EP_TYPE_CTRL:
             uecfgn_reg |= AT32UC3x_USBB_UECFGN_EPTYPE_CTRL;
             break;

        case USBD_EP_TYPE_ISOC:
             uecfgn_reg |= AT32UC3x_USBB_UECFGN_EPTYPE_ISOC;
             break;

        case USBD_EP_TYPE_BULK:
             uecfgn_reg |= AT32UC3x_USBB_UECFGN_EPTYPE_BULK;
             break;

        case USBD_EP_TYPE_INTR:
             uecfgn_reg |= AT32UC3x_USBB_UECFGN_EPTYPE_INTR;
             break;

        default:
            *p_err = USBD_ERR_EP_INVALID_TYPE;
             break;
    }

    if (ep_dir_in == DEF_YES) {                                     /* --------- CONFIGURE ENDPOINT DIRECTION --------- */
        uecfgn_reg |= AT32UC3x_USBB_UECFGN_EPDIR_IN;
    }

    switch (max_pkt_size) {                                         /* ----------- CONFIGURE ENDPOINT SIZE ------------ */
        case 8u:
             uecfgn_reg |= AT32UC3x_USBB_UECFGN_EPSIZE_8;
             break;

        case 16u:
             uecfgn_reg |= AT32UC3x_USBB_UECFGN_EPSIZE_16;
             break;

        case 32u:
             uecfgn_reg |= AT32UC3x_USBB_UECFGN_EPSIZE_32;
             break;

        case 64u:
             uecfgn_reg |= AT32UC3x_USBB_UECFGN_EPSIZE_64;
             break;

        case 128u:
             uecfgn_reg |= AT32UC3x_USBB_UECFGN_EPSIZE_128;
             break;

        case 256u:
             uecfgn_reg |= AT32UC3x_USBB_UECFGN_EPSIZE_256;
             break;

        case 512u:
             uecfgn_reg |= AT32UC3x_USBB_UECFGN_EPSIZE_512;
             break;

        default:
             break;
    }

    if (*p_err != USBD_ERR_NONE) {
        return;
    }

                                                                    /* ----------- CONFIGURE ENDPOINT BANKS ----------- */
    uecfgn_reg |= AT32UC3x_USBB_UECFGN_EPBK_SINGLE;                 /* Double banking not supported by core.            */
    uecfgn_reg |= AT32UC3x_USBB_UECFGN_ALLOC;                       /* Allocate DPRAM for Endpoint.                     */

    DEF_BIT_SET(p_reg->UERST,                                       /* Reset Endpoint for safety.                       */
                AT32UC3x_USBB_UERST_EPRST0 << ep_log_nbr);

    DEF_BIT_SET(p_reg->UERST,                                       /* Activate Endpoint.                               */
                AT32UC3x_USBB_UERST_EPEN0  << ep_log_nbr);

    p_reg->UECFGN[ep_log_nbr]  = uecfgn_reg;                        /* Write Endpoint configuration.                    */

                                                                    /* Check if Endpoint configuration is OK.           */
    if (DEF_BIT_IS_SET(p_reg->UESTAN[ep_log_nbr], AT32UC3x_USBB_UESTAN_CFGOK) == DEF_TRUE) {
                                                                    /* Enable Endpoint.                                 */
        DEF_BIT_CLR(p_reg->UERST,
                    AT32UC3x_USBB_UERST_EPRST0 << ep_log_nbr);
                                                                    /* Enable interrupt for EP.                         */
        p_reg->UDINTESET = AT32UC3x_USBB_UDINT_EP0INT << ep_log_nbr;

        if (ep_dir_in == DEF_NO) {
                                                                    /* Clr any pending int and en rx int.               */
            p_reg->UESTANCLR[ep_log_nbr] = AT32UC3x_USBB_UESTAN_RXOUTI;
            p_reg->UECONNSET[ep_log_nbr] = AT32UC3x_USBB_UECONN_RXOUTE;

            if (ep_log_nbr == 0u) {
                                                                    /* Clr any pending int and en rx setup pkt int.     */
                p_reg->UESTANCLR[0u] = AT32UC3x_USBB_UESTAN_RXSTPI_UNDERFI;
                p_reg->UECONNSET[0] = AT32UC3x_USBB_UECONN_RXSTPE_UNDERFE;
            }
        }

    } else {
                                                                    /* Otherwise, configuration fails.                  */
        DEF_BIT_CLR(p_reg->UERST,
                    AT32UC3x_USBB_UERST_EPEN0 << ep_log_nbr);

       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }

    p_drv_data->EP_MaxPktSize[ep_log_nbr] = max_pkt_size;

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
    USBD_AT32UC3x_REG  *p_reg;


    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_reg      = (USBD_AT32UC3x_REG *)p_drv->CfgPtr->BaseAddr;

    DEF_BIT_CLR(p_reg->UERST, DEF_BIT(ep_log_nbr));             /* Disable EP.                                          */
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
    CPU_INT08U          ep_log_nbr;
    USBD_AT32UC3x_REG  *p_reg;
    USBD_DRV_DATA      *p_drv_data;


    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_reg      = (USBD_AT32UC3x_REG *)p_drv->CfgPtr->BaseAddr;
    p_drv_data = (USBD_DRV_DATA     *)p_drv->DataPtr;

    if (ep_log_nbr != 0u) {
        p_reg->UECONNCLR[ep_log_nbr] = AT32UC3x_USBB_UECONN_FIFOCON;
    } else {
        p_drv_data->CtrlOutDataBufPtr = p_buf;
        p_drv_data->CtrlOutDataBufLen = buf_len;
        p_drv_data->CtrlOutDataRxLen  = 0u;
                                                                /* Clr any pending int and en rx int.                   */
        p_reg->UESTANCLR[ep_log_nbr]  = AT32UC3x_USBB_UESTAN_RXOUTI;
        p_reg->UECONNSET[ep_log_nbr]  = AT32UC3x_USBB_UECONN_RXOUTE;
    }

   *p_err = USBD_ERR_NONE;

    return (DEF_MIN(buf_len, p_drv_data->EP_MaxPktSize[ep_log_nbr]));
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
    USBD_AT32UC3x_REG  *p_reg;
    USBD_DRV_DATA      *p_drv_data;
    CPU_INT08U          ep_log_nbr;                             /* Endpoint number.                                     */
    CPU_INT32U          rd_len;                                 /* Number of received octets.                           */
    CPU_REG08          *p_fifo08;                               /* Pointer to FIFO Memory.                              */
    CPU_REG32          *p_fifo32;
    CPU_INT32U         *p_buf32;
    CPU_INT16U          cnt;


    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_drv_data = (USBD_DRV_DATA     *)p_drv->DataPtr;

    if (ep_log_nbr == 0u) {
       *p_err = USBD_ERR_NONE;

        return (p_drv_data->CtrlOutDataRxLen);
    }

    p_reg    = (USBD_AT32UC3x_REG *)p_drv->CfgPtr->BaseAddr;
    p_fifo08 = (CPU_REG08  *)((p_drv->CfgPtr->MemAddr) + (ep_log_nbr * AT32UC3x_USBB_SLAVE_MEM_SIZE));
    p_fifo32 = (CPU_REG32  *)((p_drv->CfgPtr->MemAddr) + (ep_log_nbr * AT32UC3x_USBB_SLAVE_MEM_SIZE));
    p_buf32  = (CPU_INT32U *) p_buf;
    rd_len   = (p_reg->UESTAN[ep_log_nbr] & AT32UC3x_USBB_UESTAN_BYCT) >> 20u;
    cnt      =  rd_len;

    if (rd_len > buf_len) {
       *p_err = USBD_ERR_RX;
        return (0u);
    }

    while (cnt >= sizeof(CPU_ALIGN)) {
       *p_buf32 = *p_fifo32;
        p_fifo32++;
        p_buf32++;
        cnt-= sizeof(CPU_ALIGN);
    }

    p_fifo08 = (CPU_REG08  *)p_fifo32;
    p_buf    = (CPU_INT08U *)p_buf32;

    while (cnt > 0u) {
       *p_buf = *p_fifo08;
        p_fifo08++;
        p_buf++;
        cnt--;
    }

    if (ep_log_nbr == 0u) {                                     /* Clr rx int bit.                                      */
        p_reg->UESTANCLR[0u] = AT32UC3x_USBB_UECONN_RXOUTE;
    }

   *p_err = USBD_ERR_NONE;

    return (rd_len);
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
   (void)p_drv;
   (void)ep_addr;

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
    CPU_REG08      *p_fifo08;
    CPU_REG32      *p_fifo32;
    CPU_INT32U     *p_buf32;
    CPU_INT08U      ep_log_nbr;
    CPU_INT16U      ep_pkt_len;
    CPU_INT16U      cnt;


    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    ep_pkt_len = (CPU_INT16U)DEF_MIN(p_drv_data->EP_MaxPktSize[ep_log_nbr], buf_len);
    p_fifo32   = (CPU_REG32  *)((p_drv->CfgPtr->MemAddr) + (ep_log_nbr * AT32UC3x_USBB_SLAVE_MEM_SIZE));
    p_buf32    = (CPU_INT32U *)  p_buf;
    cnt        =  ep_pkt_len;

    while (cnt >= sizeof(CPU_ALIGN)) {
       *p_fifo32 = *p_buf32;
        p_fifo32++;
        p_buf32++;
        cnt-= sizeof(CPU_ALIGN);
    }

    p_fifo08 = (CPU_REG08  *)p_fifo32;
    p_buf    = (CPU_INT08U *)p_buf32;

    while (cnt > 0u) {
       *p_fifo08 = *p_buf;
        p_fifo08++;
        p_buf++;
        cnt--;
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
    CPU_INT08U          ep_log_nbr;
    USBD_AT32UC3x_REG  *p_reg;
    CPU_SR_ALLOC();


   (void)p_buf;
   (void)buf_len;

    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_reg      = (USBD_AT32UC3x_REG *)p_drv->CfgPtr->BaseAddr;

    CPU_CRITICAL_ENTER();

    p_reg->UESTANCLR[ep_log_nbr] = AT32UC3x_USBB_UESTAN_TXINI;  /* Clear any pending int.                               */
    p_reg->UECONNSET[ep_log_nbr] = AT32UC3x_USBB_UECONN_TXINE;  /* Enable the Transmitted IN Data interrupt.            */

    if (ep_log_nbr == 0u) {                                     /* EP is a control EP.                                  */

        p_reg->UESTANCLR[0] = AT32UC3x_USBB_UESTAN_TXINI;       /* Acknowledges the interrupt and sends packet.         */

    } else {
                                                                /* EP is not a control EP.                              */
        p_reg->UESTANCLR[ep_log_nbr] = AT32UC3x_USBB_UESTAN_TXINI;
                                                                /* Sends the FIFO data.                                 */
        p_reg->UECONNCLR[ep_log_nbr] = AT32UC3x_USBB_UECONN_FIFOCON;
    }

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
    USBD_DrvEP_TxStart(              p_drv,
                                     ep_addr,
                       (CPU_INT08U *)0,
                                     0u,
                                     p_err);

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
    CPU_INT08U          ep_log_nbr;
    USBD_AT32UC3x_REG  *p_reg;
    CPU_SR_ALLOC();


    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_reg      = (USBD_AT32UC3x_REG *)p_drv->CfgPtr->BaseAddr;

    CPU_CRITICAL_ENTER();
                                                                /* Chk if an OUT xfer is in progress.                   */
    if (DEF_BIT_IS_SET(p_reg->UECONN[ep_log_nbr],
                       AT32UC3x_USBB_UECONN_RXOUTE) == DEF_YES) {

        p_reg->UESTANCLR[ep_log_nbr] = AT32UC3x_USBB_UESTAN_RXOUTI;
        p_reg->UECONNCLR[ep_log_nbr] = AT32UC3x_USBB_UECONN_FIFOCON;
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
    USBD_AT32UC3x_REG  *p_reg;
    CPU_SR_ALLOC();


    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_reg      = (USBD_AT32UC3x_REG *)p_drv->CfgPtr->BaseAddr;

    CPU_CRITICAL_ENTER();

    if (state == DEF_SET) {
                                                                /* Clr any pending int and en stall int.                */
        p_reg->UESTANCLR[ep_log_nbr] = AT32UC3x_USBB_UESTAN_STALLED_CRCERRI;
        p_reg->UECONNSET[ep_log_nbr] = AT32UC3x_USBB_UECONN_STALLRQ;
    } else {
                                                                /* Reset data toggle.                                   */
        p_reg->UECONNSET[ep_log_nbr] = AT32UC3x_USBB_UECONN_RSTDT;
        p_reg->UECONNCLR[ep_log_nbr] = AT32UC3x_USBB_UECONN_STALLRQ;
    }

    CPU_CRITICAL_EXIT();

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
    CPU_REG08          *p_fifo08;
    CPU_INT08U          ep_log_nbr;
    CPU_INT16U          rd_len;
    CPU_INT32U          ep_int_bitmap;
    CPU_INT32U          udint_reg;
    CPU_INT32U          test;
    CPU_INT32U          setup_pkt[2u];                          /* Allocate memory for setup package                    */
    USBD_AT32UC3x_REG  *p_reg;
    USBD_DRV_DATA      *p_drv_data;


    p_reg      = (USBD_AT32UC3x_REG *)p_drv->CfgPtr->BaseAddr;
    udint_reg  =  p_reg->UDINT;
    udint_reg &=  p_reg->UDINTE;

                                                                /* ------------------ END OF RESET -------------------- */
    if (DEF_BIT_IS_SET(udint_reg, AT32UC3x_USBB_UDINT_EORST) == DEF_YES) {
                                                                /* Notify the USB stack of a reset event.               */
        USBD_EventReset(p_drv);
                                                                /* Acknowledge the interrupt.                           */
        DEF_BIT_SET(p_reg->UDINTCLR, AT32UC3x_USBB_UDINT_EORST);
    }

                                                                /* --------------------- SUSPEND ---------------------- */
    if (DEF_BIT_IS_SET(udint_reg, AT32UC3x_USBB_UDINT_SUSP) == DEF_YES) {

        USBD_EventSuspend(p_drv);                               /* Notify the USB stack of a suspend event.             */
        DEF_BIT_SET(p_reg->UDINTCLR,
                    AT32UC3x_USBB_UDINT_SUSP);                  /* Acknowledge interrupt.                               */
        DEF_BIT_SET(p_reg->UDINTESET,
                    AT32UC3x_USBB_UDINT_WAKEUP);
    }

                                                                /* ------------------ END OF RESUME ------------------- */
    if (DEF_BIT_IS_SET(udint_reg, AT32UC3x_USBB_UDINT_EORSM) == DEF_YES) {

        USBD_EventResume(p_drv);                                /* Notify the USB stack of a resume event.              */
        DEF_BIT_SET(p_reg->UDINTCLR,
                    AT32UC3x_USBB_UDINT_EORSM);                 /* Acknowledge interrupt.                               */
        DEF_BIT_SET(p_reg->UDINTESET,
                    AT32UC3x_USBB_UDINT_SUSP);
    }

                                                                /* --------------------- WAKE-UP ---------------------- */
    if (DEF_BIT_IS_SET(udint_reg, AT32UC3x_USBB_UDINT_WAKEUP) == DEF_YES) {

        USBD_EventResume(p_drv);                                /* Notify the USB stack of a resume event.              */
        DEF_BIT_SET(p_reg->UDINTCLR,
                    AT32UC3x_USBB_UDINT_WAKEUP);                /* Acknowledge interrupt.                               */
        DEF_BIT_SET(p_reg->UDINTECLR,
                    AT32UC3x_USBB_UDINT_WAKEUP);
        DEF_BIT_SET(p_reg->UDINTESET,
                    AT32UC3x_USBB_UDINT_SUSP);
    }

                                                                /* ----------------- ENDPOINT EVENTS ------------------ */
    ep_int_bitmap = (udint_reg >> 12u) & 0x000001FF;
    while (ep_int_bitmap != DEF_BIT_NONE) {
        ep_log_nbr = CPU_CntTrailZeros32(ep_int_bitmap);
                                                                /* Rx complete interrupt.                               */
        if (DEF_BIT_IS_SET(p_reg->UESTAN[ep_log_nbr], AT32UC3x_USBB_UESTAN_RXOUTI) == DEF_YES) {

            if (ep_log_nbr == 0u) {
                p_reg->UECONNCLR[0] = AT32UC3x_USBB_UECONN_RXOUTE;

                p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;
                p_fifo08   = (CPU_REG08 *)(p_drv->CfgPtr->MemAddr);
                rd_len     =  DEF_MIN((p_reg->UESTAN[0u] & AT32UC3x_USBB_UESTAN_BYCT) >> 20u,
                                       p_drv_data->CtrlOutDataBufLen);

                for (p_drv_data->CtrlOutDataRxLen = 0u; p_drv_data->CtrlOutDataRxLen < rd_len; p_drv_data->CtrlOutDataRxLen++) {
                    p_drv_data->CtrlOutDataBufPtr[p_drv_data->CtrlOutDataRxLen] = p_fifo08[p_drv_data->CtrlOutDataRxLen];
                }
            }

            p_reg->UESTANCLR[ep_log_nbr] = AT32UC3x_USBB_UESTAN_RXOUTI;

            USBD_EP_RxCmpl(p_drv, ep_log_nbr);
        }
                                                                /* Tx complete interrupt.                               */
        if (DEF_BIT_IS_SET(p_reg->UESTAN[ep_log_nbr], AT32UC3x_USBB_UESTAN_TXINI) == DEF_YES) {
            p_reg->UECONNCLR[ep_log_nbr] = AT32UC3x_USBB_UECONN_TXINE;
            USBD_EP_TxCmpl(p_drv, ep_log_nbr);
        }
                                                                /* Setup packet received.                               */
        if ((DEF_BIT_IS_SET(p_reg->UESTAN[ep_log_nbr], AT32UC3x_USBB_UESTAN_RXSTPI_UNDERFI) == DEF_YES) &&
            (ep_log_nbr                                                         == 0u)) {
                                                                /* Clr int src.                                         */
            p_reg->UESTANCLR[ep_log_nbr] = AT32UC3x_USBB_UESTAN_RXSTPI_UNDERFI;
                                                                /* Get data from FIFO.                                  */
            setup_pkt[0] = *((CPU_REG32  *)(p_drv->CfgPtr->MemAddr));
            setup_pkt[1] = *(((CPU_REG32 *)(p_drv->CfgPtr->MemAddr)) + 4u);

            USBD_EventSetup(p_drv, (void *)&setup_pkt[0]);
        }

        DEF_BIT_CLR(ep_int_bitmap, DEF_BIT(ep_log_nbr));
    }

    test = p_reg->UVERS;                                        /* Memory barrier. (See note #1)                        */
    (void)test;
    return;
}
