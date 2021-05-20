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
*                                  USB ON-THE-GO FULL-SPEED (OTG_FS)
*
* Filename : usbd_drv_stm32f_fs.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)  : (1) You can find specific information about this driver at:
*                https://doc.micrium.com/display/USBDDRV/STM32F_FS
*
*            (2) With an appropriate BSP, this device driver will support the OTG_FS device module on
*                the STMicroelectronics STM32F10xxx, STM32F2xx, STM32F4xx, and STM32F7xx MCUs, this
*                applies to the following devices:
*
*                    STM32F105xx series.
*                    STM32F107xx series.
*                    STM32F205xx series.
*                    STM32F207xx series.
*                    STM32F215xx series.
*                    STM32F217xx series.
*                    STM32F405xx series.
*                    STM32F407xx series.
*                    STM32F415xx series.
*                    STM32F417xx series.
*                    STM32F74xxx series.
*                    STM32F75xxx series.
*
*            (3) With an appropriate BSP, this device driver will support the OTG_FS device module on
*                the Silicon Labs EFM32 MCUs, this applies to the following devices:
*
*                    EFM32 Giant   Gecko series.
*                    EFM32 Wonder  Gecko series.
*                    EFM32 Leopard Gecko series.
*
*            (4) With an appropriate BSP, this device driver will support the USB device module on
*                the Infineon XMC4000 MCUs, this applies to the following devices:
*
*                    Infineon XMC4100 series.
*                    Infineon XMC4200 series.
*                    Infineon XMC4300 series.
*                    Infineon XMC4400 series.
*                    Infineon XMC4500 series.
*                    Infineon XMC4700 series.
*                    Infineon XMC4800 series.
*
*            (5) This device driver DOES NOT support the USB on-the-go high-speed(OTG_HS) device module
*                of the STM32F2xx, STM32F4xx, STM32F7xx, or EFM32 Gecko series.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#include  "../../Source/usbd_core.h"
#include  "usbd_drv_stm32f_fs.h"

/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  DRV_STM32F_FS                               0u
#define  DRV_STM32F_OTG_FS                           1u
#define  DRV_EFM32_OTG_FS                            2u
#define  DRV_XMC_OTG_FS                              3u

#define  STM32F_FS_REG_VAL_TO                        0x3FFu
#define  STM32F_FS_REG_FMOD_TO                       0x7FFFFu

#define  USBD_DBG_DRV_EP(msg, ep_addr)               USBD_DBG_GENERIC((msg),                                            \
                                                                      (ep_addr),                                        \
                                                                       USBD_IF_NBR_NONE)

#define  USBD_DBG_DRV_EP_ARG(msg, ep_addr, arg)      USBD_DBG_GENERIC_ARG((msg),                                        \
                                                                          (ep_addr),                                    \
                                                                           USBD_IF_NBR_NONE,                            \
                                                                          (arg))

/*
*********************************************************************************************************
*                                   STM32 OTG USB DEVICE CONSTRAINTS
*
* Note(s) : (1) Depending on which USB controller the MCU supports, the USB system features one of the
*               following dedicated data RAM allocations for TX and RX endpoints:
*                   (a) 1.25Kbytes = 1280 bytes = 320 32-bit words.
*                   (b) 2KBbytes   = 2048 bytes = 512 32-bit words.
*
*               RX-FIFO : The USB device uses a single receive FIFO that receiveds the data directed
*                         to all OUT enpoints. The size of the receive FIFO is configured with
*                         the receive FIFO Size register (GRXFSIZ). The value written to GRXFSIZ is
*                         term of 32-bit words.
*
*               TX-FIFO : The core has a dedicated FIFO for each IN endpoint. These can be configured
*                         by writing to DIEPTXF0 for IN endpoint0 and DIEPTXFx for IN endpoint x.
*                         The Tx FIFO depth value is in terms of 32-bit words. The minimum RAM space
*                         required for each IN endpoint Tx FIFO is 64bytes (Max packet size), which
*                         is equal to 16 32-bit words.
*
*********************************************************************************************************
*/

#define  STM32F_FS_NBR_EPS                             16u      /* Maximum number of endpoints                          */
#define  STM32F_FS_NBR_CHANNEL                          8u      /* Maximum number of channels                           */
#define  STM32F_FS_DFIFO_SIZE                        1024u      /* Number of entries                                    */
#define  STM32F_FS_MAX_PKT_SIZE                        64u

#define  STM32F_FS_RXFIFO_SIZE                        128u      /* See note #1.                                         */
#define  STM32F_FS_TXFIFO_EP0_SIZE                     16u
#define  STM32F_FS_TXFIFO_EP1_SIZE                     32u
#define  STM32F_FS_TXFIFO_EP2_SIZE                     32u
#define  STM32F_FS_TXFIFO_EP3_SIZE                     32u
#define  STM32F_FS_TXFIFO_EP4_SIZE                     32u
#define  STM32F_FS_TXFIFO_EP5_SIZE                     32u
#define  STM32F_FS_TXFIFO_EP6_SIZE                     32u

#define  STM32F_FS_TXFIFO_EP0_START_ADDR         STM32F_FS_RXFIFO_SIZE
#define  STM32F_FS_TXFIFO_EP1_START_ADDR        (STM32F_FS_TXFIFO_EP0_START_ADDR + STM32F_FS_TXFIFO_EP0_SIZE)
#define  STM32F_FS_TXFIFO_EP2_START_ADDR        (STM32F_FS_TXFIFO_EP1_START_ADDR + STM32F_FS_TXFIFO_EP1_SIZE)
#define  STM32F_FS_TXFIFO_EP3_START_ADDR        (STM32F_FS_TXFIFO_EP2_START_ADDR + STM32F_FS_TXFIFO_EP2_SIZE)
#define  STM32F_FS_TXFIFO_EP4_START_ADDR        (STM32F_FS_TXFIFO_EP3_START_ADDR + STM32F_FS_TXFIFO_EP3_SIZE)
#define  STM32F_FS_TXFIFO_EP5_START_ADDR        (STM32F_FS_TXFIFO_EP4_START_ADDR + STM32F_FS_TXFIFO_EP4_SIZE)
#define  STM32F_FS_TXFIFO_EP6_START_ADDR        (STM32F_FS_TXFIFO_EP5_START_ADDR + STM32F_FS_TXFIFO_EP5_SIZE)


/*
*********************************************************************************************************
*                                        REGISTER BIT DEFINES
*********************************************************************************************************
*/

#define  STM32F_FS_GOTGCTL_BIT_BVALOEN           DEF_BIT_06
#define  STM32F_FS_GOTGCTL_BIT_BVALOVAL          DEF_BIT_07

#define  STM32F_FS_GOTGINT_BIT_SEDET             DEF_BIT_02
#define  STM32F_FS_GOTGINT_BIT_SRSSCHG           DEF_BIT_08
#define  STM32F_FS_GOTGINT_BIT_HNSSCHG           DEF_BIT_09
#define  STM32F_FS_GOTGINT_BIT_HNGDET            DEF_BIT_17
#define  STM32F_FS_GOTGINT_BIT_ADTOCHG           DEF_BIT_18
#define  STM32F_FS_GOTGINT_BIT_DBCDNE            DEF_BIT_19

#define  STM32F_FS_GAHBCFG_BIT_TXFELVL           DEF_BIT_07
#define  STM32F_FS_GAHBCFG_BIT_GINTMSK           DEF_BIT_00

#define  STM32F_FS_GUSBCFG_BIT_FDMOD             DEF_BIT_30
#define  STM32F_FS_GUSBCFG_BIT_FHMOD             DEF_BIT_29
#define  STM32F_FS_GUSBCFG_BIT_HNPCAP            DEF_BIT_09
#define  STM32F_FS_GUSBCFG_BIT_SRPCAP            DEF_BIT_08
#define  STM32F_FS_GUSBCFG_BIT_PHYSEL            DEF_BIT_07
#define  STM32F_FS_GUSBCFG_TRDT_MASK             DEF_BIT_FIELD(15u, 10u)

#define  STM32F_FS_GRSTCTL_BIT_AHBIDL            DEF_BIT_31
#define  STM32F_FS_GRSTCTL_FLUSH_TXFIFO_00       DEF_BIT_NONE
#define  STM32F_FS_GRSTCTL_FLUSH_TXFIFO_01       DEF_BIT_MASK(1u, 6u)
#define  STM32F_FS_GRSTCTL_FLUSH_TXFIFO_02       DEF_BIT_MASK(2u, 6u)
#define  STM32F_FS_GRSTCTL_FLUSH_TXFIFO_03       DEF_BIT_MASK(3u, 6u)
#define  STM32F_FS_GRSTCTL_FLUSH_TXFIFO_ALL      DEF_BIT_MASK(16u, 6u)
#define  STM32F_FS_GRSTCTL_BIT_TXFFLSH           DEF_BIT_05
#define  STM32F_FS_GRSTCTL_BIT_RXFFLSH           DEF_BIT_04
#define  STM32F_FS_GRSTCTL_BIT_HSRST             DEF_BIT_01
#define  STM32F_FS_GRSTCTL_BIT_CSRST             DEF_BIT_00

#define  STM32F_FS_GINTSTS_BIT_WKUPINT           DEF_BIT_31
#define  STM32F_FS_GINTSTS_BIT_SRQINT            DEF_BIT_30
#define  STM32F_FS_GINTSTS_BIT_DISCINT           DEF_BIT_29
#define  STM32F_FS_GINTSTS_BIT_CIDSCHG           DEF_BIT_28
#define  STM32F_FS_GINTSTS_BIT_INCOMPISOOUT      DEF_BIT_21
#define  STM32F_FS_GINTSTS_BIT_IISOIXFR          DEF_BIT_20
#define  STM32F_FS_GINTSTS_BIT_OEPINT            DEF_BIT_19
#define  STM32F_FS_GINTSTS_BIT_IEPINT            DEF_BIT_18
#define  STM32F_FS_GINTSTS_BIT_EOPF              DEF_BIT_15
#define  STM32F_FS_GINTSTS_BIT_ISOODRP           DEF_BIT_14
#define  STM32F_FS_GINTSTS_BIT_ENUMDNE           DEF_BIT_13
#define  STM32F_FS_GINTSTS_BIT_USBRST            DEF_BIT_12
#define  STM32F_FS_GINTSTS_BIT_USBSUSP           DEF_BIT_11
#define  STM32F_FS_GINTSTS_BIT_ESUSP             DEF_BIT_10
#define  STM32F_FS_GINTSTS_BIT_GONAKEFF          DEF_BIT_07
#define  STM32F_FS_GINTSTS_BIT_GINAKEFF          DEF_BIT_06
#define  STM32F_FS_GINTSTS_BIT_RXFLVL            DEF_BIT_04
#define  STM32F_FS_GINTSTS_BIT_SOF               DEF_BIT_03
#define  STM32F_FS_GINTSTS_BIT_OTGINT            DEF_BIT_02
#define  STM32F_FS_GINTSTS_BIT_MMIS              DEF_BIT_01
#define  STM32F_FS_GINTSTS_BIT_CMOD              DEF_BIT_00
                                                                /* Interrupts are clear by writing 1 to its bit         */
#define  STM32F_FS_GINTSTS_INT_ALL              (STM32F_FS_GINTSTS_BIT_WKUPINT  | STM32F_FS_GINTSTS_BIT_SRQINT       |                 \
                                                 STM32F_FS_GINTSTS_BIT_CIDSCHG  | STM32F_FS_GINTSTS_BIT_INCOMPISOOUT |                 \
                                                 STM32F_FS_GINTSTS_BIT_IISOIXFR | STM32F_FS_GINTSTS_BIT_EOPF         |                 \
                                                 STM32F_FS_GINTSTS_BIT_ISOODRP  | STM32F_FS_GINTSTS_BIT_ENUMDNE      |                 \
                                                 STM32F_FS_GINTSTS_BIT_USBRST   | STM32F_FS_GINTSTS_BIT_USBSUSP      |                 \
                                                 STM32F_FS_GINTSTS_BIT_ESUSP    | STM32F_FS_GINTSTS_BIT_SOF          |                 \
                                                 STM32F_FS_GINTSTS_BIT_MMIS)

#define  STM32F_FS_GINTMSK_BIT_WUIM              DEF_BIT_31
#define  STM32F_FS_GINTMSK_BIT_SRQIM             DEF_BIT_30
#define  STM32F_FS_GINTMSK_BIT_DISCINT           DEF_BIT_29
#define  STM32F_FS_GINTMSK_BIT_CIDSCHGM          DEF_BIT_28
#define  STM32F_FS_GINTMSK_BIT_IISOOXFRM         DEF_BIT_21
#define  STM32F_FS_GINTMSK_BIT_IISOIXFRM         DEF_BIT_20
#define  STM32F_FS_GINTMSK_BIT_OEPINT            DEF_BIT_19
#define  STM32F_FS_GINTMSK_BIT_IEPINT            DEF_BIT_18
#define  STM32F_FS_GINTMSK_BIT_EPMISM            DEF_BIT_17
#define  STM32F_FS_GINTMSK_BIT_EOPFM             DEF_BIT_15
#define  STM32F_FS_GINTMSK_BIT_ISOODRPM          DEF_BIT_14
#define  STM32F_FS_GINTMSK_BIT_ENUMDNEM          DEF_BIT_13
#define  STM32F_FS_GINTMSK_BIT_USBRST            DEF_BIT_12
#define  STM32F_FS_GINTMSK_BIT_USBSUSPM          DEF_BIT_11
#define  STM32F_FS_GINTMSK_BIT_ESUSPM            DEF_BIT_10
#define  STM32F_FS_GINTMSK_BIT_GONAKEFFM         DEF_BIT_07
#define  STM32F_FS_GINTMSK_BIT_GINAKEFFM         DEF_BIT_06
#define  STM32F_FS_GINTMSK_BIT_RXFLVLM           DEF_BIT_04
#define  STM32F_FS_GINTMSK_BIT_SOFM              DEF_BIT_03
#define  STM32F_FS_GINTMSK_BIT_OTGINT            DEF_BIT_02
#define  STM32F_FS_GINTMSK_BIT_MMISM             DEF_BIT_01

#define  STM32F_FS_GRXSTSx_PKTSTS_OUT_NAK        1u
#define  STM32F_FS_GRXSTSx_PKTSTS_OUT_RX         2u
#define  STM32F_FS_GRXSTSx_PKTSTS_OUT_COMPL      3u
#define  STM32F_FS_GRXSTSx_PKTSTS_SETUP_COMPL    4u
#define  STM32F_FS_GRXSTSx_PKTSTS_SETUP_RX       6u
#define  STM32F_FS_GRXSTSx_PKTSTS_MASK           DEF_BIT_FIELD(4u, 17u)
#define  STM32F_FS_GRXSTSx_EPNUM_MASK            DEF_BIT_FIELD(2u, 0u)
#define  STM32F_FS_GRXSTSx_BCNT_MASK             DEF_BIT_FIELD(11u, 4u)

#define  STM32F_FS_GCCFG_BIT_VBDEN               DEF_BIT_21     /* This bit is use for STM32F74xx & STM32F75xx driver.  */
#define  STM32F_FS_GCCFG_BIT_NONVBUSSENS         DEF_BIT_21     /* This bit is use for STM32F107, STM32F2 & STM32F4.    */
#define  STM32F_FS_GCCFG_BIT_SOFOUTEN            DEF_BIT_20
#define  STM32F_FS_GCCFG_BIT_VBUSBEN             DEF_BIT_19
#define  STM32F_FS_GCCFG_BIT_VBUSAEN             DEF_BIT_18
#define  STM32F_FS_GCCFG_BIT_PWRDWN              DEF_BIT_16

#define  STM32F_FS_DCFG_PFIVL_80                 DEF_BIT_MASK(0u, 11u)
#define  STM32F_FS_DCFG_PFIVL_85                 DEF_BIT_MASK(1u, 11u)
#define  STM32F_FS_DCFG_PFIVL_90                 DEF_BIT_MASK(2u, 11u)
#define  STM32F_FS_DCFG_PFIVL_95                 DEF_BIT_MASK(3u, 11u)
#define  STM32F_FS_DCFG_BIT_NZLSOHSK             DEF_BIT_02
#define  STM32F_FS_DCFG_DSPD_FULLSPEED           DEF_BIT_MASK(3u, 0u)
#define  STM32F_FS_DCFG_DAD_MASK                 DEF_BIT_FIELD(7u, 4u)

#define  STM32F_FS_DCTL_BIT_POPRGDNE             DEF_BIT_11
#define  STM32F_FS_DCTL_BIT_CGONAK               DEF_BIT_10
#define  STM32F_FS_DCTL_BIT_SGONAK               DEF_BIT_09
#define  STM32F_FS_DCTL_BIT_CGINAK               DEF_BIT_08
#define  STM32F_FS_DCTL_BIT_SGINAK               DEF_BIT_07
#define  STM32F_FS_DCTL_BIT_GONSTS               DEF_BIT_03
#define  STM32F_FS_DCTL_BIT_GINSTS               DEF_BIT_02
#define  STM32F_FS_DCTL_BIT_SDIS                 DEF_BIT_01
#define  STM32F_FS_DCTL_BIT_RWUSIG               DEF_BIT_00

#define  STM32F_FS_DSTS_BIT_EERR                 DEF_BIT_03
#define  STM32F_FS_DSTS_ENUMSPD_FS_PHY_48MHZ     DEF_BIT_MASK(3u, 1u)
#define  STM32F_FS_DSTS_BIT_SUSPSTS              DEF_BIT_00
#define  STM32F_FS_DSTS_FNSOF_MASK               DEF_BIT_FIELD(14u, 8u)

#define  STM32F_FS_DIEPMSK_BIT_INEPNEM           DEF_BIT_06
#define  STM32F_FS_DIEPMSK_BIT_INEPNMM           DEF_BIT_05
#define  STM32F_FS_DIEPMSK_BIT_ITTXFEMSK         DEF_BIT_04
#define  STM32F_FS_DIEPMSK_BIT_TOM               DEF_BIT_03
#define  STM32F_FS_DIEPMSK_BIT_EPDM              DEF_BIT_01
#define  STM32F_FS_DIEPMSK_BIT_XFRCM             DEF_BIT_00

#define  STM32F_FS_DOEPMSK_BIT_OTEPDM            DEF_BIT_04
#define  STM32F_FS_DOEPMSK_BIT_STUPM             DEF_BIT_03
#define  STM32F_FS_DOEPMSK_BIT_EPDM              DEF_BIT_01
#define  STM32F_FS_DOEPMSK_BIT_XFRCM             DEF_BIT_00

#define  STM32F_FS_DAINT_BIT_OEPINT_EP0          DEF_BIT_16
#define  STM32F_FS_DAINT_BIT_OEPINT_EP1          DEF_BIT_17
#define  STM32F_FS_DAINT_BIT_OEPINT_EP2          DEF_BIT_18
#define  STM32F_FS_DAINT_BIT_OEPINT_EP3          DEF_BIT_19
#define  STM32F_FS_DAINT_BIT_IEPINT_EP0          DEF_BIT_00
#define  STM32F_FS_DAINT_BIT_IEPINT_EP1          DEF_BIT_01
#define  STM32F_FS_DAINT_BIT_IEPINT_EP2          DEF_BIT_02
#define  STM32F_FS_DAINT_BIT_IEPINT_EP3          DEF_BIT_03

#define  STM32F_FS_DAINTMSK_BIT_OEPINT_EP0       DEF_BIT_16
#define  STM32F_FS_DAINTMSK_BIT_OEPINT_EP1       DEF_BIT_17
#define  STM32F_FS_DAINMSKT_BIT_OEPINT_EP2       DEF_BIT_18
#define  STM32F_FS_DAINTMSK_BIT_OEPINT_EP3       DEF_BIT_19
#define  STM32F_FS_DAINTMSK_OEPINT_ALL          (STM32F_FS_DAINTMSK_BIT_OEPINT_EP0 | STM32F_FS_DAINTMSK_BIT_OEPINT_EP1 |               \
                                                 STM32F_FS_DAINTMSK_BIT_OEPINT_EP2 | STM32F_FS_DAINTMSK_BIT_OEPINT_EP3)
#define  STM32F_FS_DAINTMSK_BIT_IEPINT_EP0       DEF_BIT_00
#define  STM32F_FS_DAINTMSK_BIT_IEPINT_EP1       DEF_BIT_01
#define  STM32F_FS_DAINTMSK_BIT_IEPINT_EP2       DEF_BIT_02
#define  STM32F_FS_DAINTMSK_BIT_IEPINT_EP3       DEF_BIT_03
#define  STM32F_FS_DAINTMSK_IEPINT_ALL          (STM32F_FS_DAINTMSK_BIT_IEPINT_EP0 | STM32F_FS_DAINTMSK_BIT_IEPINT_EP1 |               \
                                                 STM32F_FS_DAINTMSK_BIT_IEPINT_EP2 | STM32F_FS_DAINTMSK_BIT_IEPINT_EP3)

#define  STM32F_FS_DIEPEMPMSK_BIT_INEPTXFEM_EP0  DEF_BIT_00
#define  STM32F_FS_DIEPEMPMSK_BIT_INEPTXFEM_EP1  DEF_BIT_01
#define  STM32F_FS_DIEPEMPMSK_BIT_INEPTXFEM_EP2  DEF_BIT_02
#define  STM32F_FS_DIEPEMPMSK_BIT_INEPTXFEM_EP3  DEF_BIT_03

#define  STM32F_FS_DxEPCTLx_BIT_EPENA            DEF_BIT_31
#define  STM32F_FS_DxEPCTLx_BIT_EPDIS            DEF_BIT_30
#define  STM32F_FS_DxEPCTLx_BIT_SODDFRM          DEF_BIT_29
#define  STM32F_FS_DxEPCTLx_BIT_SD0PID           DEF_BIT_28
#define  STM32F_FS_DxEPCTLx_BIT_SEVNFRM          DEF_BIT_28
#define  STM32F_FS_DxEPCTLx_BIT_SNAK             DEF_BIT_27
#define  STM32F_FS_DxEPCTLx_BIT_CNAK             DEF_BIT_26
#define  STM32F_FS_DxEPCTLx_BIT_STALL            DEF_BIT_21
#define  STM32F_FS_DxEPCTLx_EPTYPE_CTRL          DEF_BIT_MASK(0u, 18u)
#define  STM32F_FS_DxEPCTLx_EPTYPE_ISO           DEF_BIT_MASK(1u, 18u)
#define  STM32F_FS_DxEPCTLx_EPTYPE_BULK          DEF_BIT_MASK(2u, 18u)
#define  STM32F_FS_DxEPCTLx_EPTYPE_INTR          DEF_BIT_MASK(3u, 18u)
#define  STM32F_FS_DxEPCTLx_EPTYPE_MASK          DEF_BIT_FIELD(3u, 18u)
#define  STM32F_FS_DxEPCTLx_BIT_NAKSTS           DEF_BIT_17
#define  STM32F_FS_DxEPCTLx_BIT_EONUM            DEF_BIT_16
#define  STM32F_FS_DxEPCTLx_BIT_DPID             DEF_BIT_16
#define  STM32F_FS_DxEPCTLx_BIT_USBAEP           DEF_BIT_15
#define  STM32F_FS_DxEPCTLx_MPSIZ_MASK           DEF_BIT_FIELD(11u, 0u)

#define  STM32F_FS_DxEPCTL0_MPSIZ_64             DEF_BIT_MASK(0u, 0u)
#define  STM32F_FS_DxEPCTL0_MPSIZ_64_MSK         DEF_BIT_FIELD(2u, 0u)

#define  STM32F_FS_DOEPCTLx_BIT_SD1PID           DEF_BIT_29
#define  STM32F_FS_DOEPCTLx_BIT_SNPM             DEF_BIT_20

#define  STM32F_FS_DIEPINTx_BIT_TXFE             DEF_BIT_07
#define  STM32F_FS_DIEPINTx_BIT_INEPNE           DEF_BIT_06
#define  STM32F_FS_DIEPINTx_BIT_ITTXFE           DEF_BIT_04
#define  STM32F_FS_DIEPINTx_BIT_TOC              DEF_BIT_03
#define  STM32F_FS_DIEPINTx_BIT_EPDISD           DEF_BIT_01
#define  STM32F_FS_DIEPINTx_BIT_XFRC             DEF_BIT_00

#define  STM32F_FS_DOEPINTx_BIT_B2BSTUP          DEF_BIT_06
#define  STM32F_FS_DOEPINTx_BIT_OTEPDIS          DEF_BIT_04
#define  STM32F_FS_DOEPINTx_BIT_STUP             DEF_BIT_03
#define  STM32F_FS_DOEPINTx_BIT_EPDISD           DEF_BIT_01
#define  STM32F_FS_DOEPINTx_BIT_XFRC             DEF_BIT_00

#define  STM32F_FS_DOEPTSIZx_STUPCNT_1_PKT       DEF_BIT_MASK(1u, 29u)
#define  STM32F_FS_DOEPTSIZx_STUPCNT_2_PKT       DEF_BIT_MASK(2u, 29u)
#define  STM32F_FS_DOEPTSIZx_STUPCNT_3_PKT       DEF_BIT_MASK(3u, 29u)
#define  STM32F_FS_DOEPTSIZx_XFRSIZ_MSK          DEF_BIT_FIELD(19u, 0u)
#define  STM32F_FS_DOEPTSIZx_PKTCNT_MSK          DEF_BIT_FIELD(10u, 19u)

#define  STM32F_FS_DOEPTSIZ0_BIT_PKTCNT          DEF_BIT_19
#define  STM32F_FS_DOEPTSIZ0_XFRSIZ_MAX_64       DEF_BIT_MASK(64u, 0u)

#define  STM32F_FS_DIEPTSIZx_MCNT_1_PKT          DEF_BIT_MASK(1u, 29u)
#define  STM32F_FS_DIEPTSIZx_MCNT_2_PKT          DEF_BIT_MASK(2u, 29u)
#define  STM32F_FS_DIEPTSIZx_MCNT_3_PKT          DEF_BIT_MASK(3u, 29u)
#define  STM32F_FS_DIEPTSIZx_XFRSIZ_MSK          DEF_BIT_FIELD(19u, 0u)
#define  STM32F_FS_DIEPTSIZx_PKTCNT_MSK          DEF_BIT_FIELD(10u, 19u)
#define  STM32F_FS_DIEPTSIZx_PKTCNT_01           DEF_BIT_MASK(1u, 19u)

#define  STM32F_FS_DTXFSTSx_EP_FIFO_FULL         DEF_BIT_NONE
#define  STM32F_FS_DTXFSTSx_EP_FIFO_WAVAIL_01    DEF_BIT_MASK(1u, 0u)
#define  STM32F_FS_DTXFSTSx_EP_FIFO_WAVAIL_02    DEF_BIT_MASK(2u, 0u)

#define  STM32F_FS_DOEPTSIZx_RXDPID_DATA0        DEF_BIT_MASK(0u, 29u)
#define  STM32F_FS_DOEPTSIZx_RXDPID_DATA1        DEF_BIT_MASK(2u, 29u)
#define  STM32F_FS_DOEPTSIZx_RXDPID_DATA2        DEF_BIT_MASK(1u, 29u)
#define  STM32F_FS_DOEPTSIZx_RXDPID_MDATA        DEF_BIT_MASK(3u, 29u)

#define  STM32F_FS_PCGCCTL_BIT_PHYSUSP           DEF_BIT_04
#define  STM32F_FS_PCGCCTL_BIT_GATEHCLK          DEF_BIT_01
#define  STM32F_FS_PCGCCTL_BIT_STPPCLK           DEF_BIT_00


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

#define  USBD_GET_EP_TYPE(val)                       (((val) & STM32F_FS_DxEPCTLx_EPTYPE_MASK) >> 18u)


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

typedef  struct  usbd_stm32f_fs_in_ep_reg {                     /* -------------- IN-ENDPOINT REGISTERS --------------- */
    CPU_REG32                  CTLx;                            /* Device control IN endpoint-x                         */
    CPU_REG32                  RSVD0;
    CPU_REG32                  INTx;                            /* Device IN endpoint-x interrupt                       */
    CPU_REG32                  RSVD1;
    CPU_REG32                  TSIZx;                           /* Device IN endpoint-x transfer size                   */
    CPU_REG32                  DMAx;                            /* OTG_HS device IN endpoint-x DMA address register     */
    CPU_REG32                  DTXFSTSx;                        /* Device IN endpoint-x transmit FIFO status            */
    CPU_REG32                  RSVD2;
} USBD_STM32F_FS_IN_EP_REG;


typedef  struct  usbd_stm32f_fs_out_ep_reg {                    /* ------------- OUT ENDPOINT REGISTERS --------------- */
    CPU_REG32                  CTLx;                            /* Device control OUT endpoint-x                        */
    CPU_REG32                  RSVD0;
    CPU_REG32                  INTx;                            /* Device OUT endpoint-x interrupt                      */
    CPU_REG32                  RSVD1;
    CPU_REG32                  TSIZx;                           /* Device OUT endpoint-x transfer size                  */
    CPU_REG32                  DMAx;                            /* OTG_HS device OUT endpoint-x DMA address register    */
    CPU_REG32                  RSVD2[2u];
} USBD_STM32F_FS_OUT_EP_REG;

typedef  struct  usbd_stm32f_fs_dfifo_reg {                     /* ---------- DATA FIFO ACCESS REGISTERS -------------- */
    CPU_REG32                  DATA[STM32F_FS_DFIFO_SIZE];      /* 4K bytes per endpoint                                */
} USBD_STM32F_FS_DFIFO_REG;

typedef  struct  usbd_stm32f_fs_reg {
                                                                /* ----- CORE GLOBAL CONTROL AND STATUS REGISTERS ----- */
    CPU_REG32                  GOTGCTL;                         /* Core control and status                              */
    CPU_REG32                  GOTGINT;                         /* Core interrupt                                       */
    CPU_REG32                  GAHBCFG;                         /* Core AHB configuration                               */
    CPU_REG32                  GUSBCFG;                         /* Core USB configuration                               */
    CPU_REG32                  GRSTCTL;                         /* Core reset                                           */
    CPU_REG32                  GINTSTS;                         /* Core interrupt                                       */
    CPU_REG32                  GINTMSK;                         /* Core interrupt mask                                  */
    CPU_REG32                  GRXSTSR;                         /* Core receive status debug read                       */
    CPU_REG32                  GRXSTSP;                         /* Core status read and pop                             */
    CPU_REG32                  GRXFSIZ;                         /* Core receive FIFO size                               */
    CPU_REG32                  DIEPTXF0;                        /* Endpoint 0 transmit FIFO size                        */
    CPU_REG32                  HNPTXSTS;                        /* Core Non Periodic Tx FIFO/Queue status               */
    CPU_REG32                  GI2CCTL;                         /* OTG_HS I2C access register                           */
    CPU_REG32                  RSVD0;
    CPU_REG32                  GCCFG;                           /* General core configuration                           */
    CPU_REG32                  CID;                             /* Core ID register                                     */

    CPU_REG32                  RSVD1[48u];

    CPU_REG32                  HPTXFSIZ;                        /* Core Host Periodic Tx FIFO size                      */
    CPU_REG32                  DIEPTXFx[STM32F_FS_NBR_EPS - 1]; /* Device IN endpoint transmit FIFO size                */

    CPU_REG32                  RSVD2[432u];
                                                                /* ----- DEVICE MODE CONTROL AND STATUS REGISTERS ----- */
    CPU_REG32                  DCFG;                            /* Device configuration                                 */
    CPU_REG32                  DCTL;                            /* Device control                                       */
    CPU_REG32                  DSTS;                            /* Device status                                        */
    CPU_REG32                  RSVD3;
    CPU_REG32                  DIEPMSK;                         /* Device IN endpoint common interrupt mask             */
    CPU_REG32                  DOEPMSK;                         /* Device OUT endpoint common interrupt mask            */
    CPU_REG32                  DAINT;                           /* Device All endpoints interrupt                       */
    CPU_REG32                  DAINTMSK;                        /* Device All endpoints interrupt mask                  */

    CPU_REG32                  RSVD4[2u];

    CPU_REG32                  DVBUSDIS;                        /* Device VBUS discharge time                           */
    CPU_REG32                  DVBUSPULSE;                      /* Device VBUS pulsing time                             */

    CPU_REG32                  RESVD5;

    CPU_REG32                  DIEPEMPMSK;                      /* Device IN ep FIFO empty interrupt mask               */

    CPU_REG32                  EACHHINT;                        /* OTG_HS device each endpoint interrupt register       */
    CPU_REG32                  EACHHINTMSK;                     /* OTG_HS device each endpoint interrupt register mask  */
    CPU_REG32                  DIEPEACHMSK1;                    /* OTG_HS device each IN endpoint-1 interrupt register  */
    CPU_REG32                  RSVD6[15u];
    CPU_REG32                  DOEPEACHMSK1;                    /* OTG_HS device each OUT endpoint-1 interrupt register */

    CPU_REG32                  RSVD7[31u];

    USBD_STM32F_FS_IN_EP_REG   DIEP[STM32F_FS_NBR_EPS];         /* Device IN EP registers                               */
    USBD_STM32F_FS_OUT_EP_REG  DOEP[STM32F_FS_NBR_EPS];         /* Device OUT EP registers                              */

    CPU_REG32                  RSVD8[64u];

    CPU_REG32                  PCGCR;                           /* Power anc clock gating control                       */

    CPU_REG32                  RSVD9[127u];

    USBD_STM32F_FS_DFIFO_REG   DFIFO[STM32F_FS_NBR_EPS];        /* Data FIFO access registers                           */
} USBD_STM32F_FS_REG;


/*
*********************************************************************************************************
*                                          DRIVER DATA TYPE
*********************************************************************************************************
*/

typedef  struct  usbd_drv_data_ep {                             /* ---------- DEVICE ENDPOINT DATA STRUCTURE ---------- */
    CPU_INT32U   DataBuf[STM32F_FS_MAX_PKT_SIZE / 4u];          /* Drv internal aligned buffer.                         */
    CPU_INT16U   EP_MaxPktSize[STM32F_FS_NBR_CHANNEL];          /* Max pkt size of opened EPs.                          */
    CPU_INT16U   EP_PktXferLen[STM32F_FS_NBR_CHANNEL];          /* EPs current xfer len.                                */
    CPU_INT08U  *EP_AppBufPtr[STM32F_FS_NBR_CHANNEL];           /* Ptr to app buffer.                                   */
    CPU_INT16U   EP_AppBufLen[STM32F_FS_NBR_CHANNEL];           /* Len of app buffer.                                   */
    CPU_INT16U   EP_AppBufBlk[STM32F_FS_NBR_CHANNEL];           /* Number of packets remaining to read.                 */
    CPU_INT32U   EP_SetupBuf[2u];                               /* Buffer that contains setup pkt.                      */
    CPU_INT16U   DrvType;                                       /* STM32F_FS/ STM32F_OTG_FS/ EFM32_OTG_FS/ XMC_OTG_FS   */
} USBD_DRV_DATA_EP;


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

                                                                /* ------------ FULL-SPEED MODE DRIVER API ------------ */
static  void         USBD_DrvSTM32F_FS_Init     (USBD_DRV     *p_drv,
                                                 USBD_ERR     *p_err);

static  void         USBD_DrvSTM32F_OTG_FS_Init (USBD_DRV     *p_drv,
                                                 USBD_ERR     *p_err);

static  void         USBD_DrvEFM32_OTG_FS_Init  (USBD_DRV     *p_drv,
                                                 USBD_ERR     *p_err);

static  void         USBD_DrvXMC_OTG_FS_Init    (USBD_DRV     *p_drv,
                                                 USBD_ERR     *p_err);

static  void         USBD_DrvHandlerInit        (USBD_DRV     *p_drv,
                                                 CPU_INT16U    drv_type,
                                                 USBD_ERR     *p_err);

static  void         USBD_DrvSTM32F_FS_Start    (USBD_DRV     *p_drv,
                                                 USBD_ERR     *p_err);

static  void         USBD_DrvSTM32F_OTG_FS_Start(USBD_DRV     *p_drv,
                                                 USBD_ERR     *p_err);

static  void         USBD_DrvEFM32_OTG_FS_Start (USBD_DRV     *p_drv,
                                                 USBD_ERR     *p_err);

static  void         USBD_DrvXMC_OTG_FS_Start   (USBD_DRV     *p_drv,
                                                 USBD_ERR     *p_err);

static  void         USBD_DrvHandlerStart       (USBD_DRV     *p_drv,
                                                 CPU_INT16U    drv_type,
                                                 USBD_ERR     *p_err);

static  void         USBD_DrvStop               (USBD_DRV     *p_drv);

static  CPU_BOOLEAN  USBD_DrvAddrSet            (USBD_DRV     *p_drv,
                                                 CPU_INT08U    dev_addr);

static  CPU_INT16U   USBD_DrvFrameNbrGet        (USBD_DRV     *p_drv);

static  void         USBD_DrvEP_Open            (USBD_DRV     *p_drv,
                                                 CPU_INT08U    ep_addr,
                                                 CPU_INT08U    ep_type,
                                                 CPU_INT16U    max_pkt_size,
                                                 CPU_INT08U    transaction_frame,
                                                 USBD_ERR     *p_err);

static  void         USBD_DrvEP_Close           (USBD_DRV     *p_drv,
                                                 CPU_INT08U    ep_addr);

static  CPU_INT32U   USBD_DrvEP_RxStart         (USBD_DRV     *p_drv,
                                                 CPU_INT08U    ep_addr,
                                                 CPU_INT08U   *p_buf,
                                                 CPU_INT32U    buf_len,
                                                 USBD_ERR     *p_err);

static  CPU_INT32U   USBD_DrvEP_Rx              (USBD_DRV     *p_drv,
                                                 CPU_INT08U    ep_addr,
                                                 CPU_INT08U   *p_buf,
                                                 CPU_INT32U    buf_len,
                                                 USBD_ERR     *p_err);

static  void         USBD_DrvEP_RxZLP           (USBD_DRV     *p_drv,
                                                 CPU_INT08U    ep_addr,
                                                 USBD_ERR     *p_err);

static  CPU_INT32U   USBD_DrvEP_Tx              (USBD_DRV     *p_drv,
                                                 CPU_INT08U    ep_addr,
                                                 CPU_INT08U   *p_buf,
                                                 CPU_INT32U    buf_len,
                                                 USBD_ERR     *p_err);

static  void         USBD_DrvEP_TxStart         (USBD_DRV     *p_drv,
                                                 CPU_INT08U    ep_addr,
                                                 CPU_INT08U   *p_buf,
                                                 CPU_INT32U    buf_len,
                                                 USBD_ERR     *p_err);


static  void         USBD_DrvEP_TxZLP           (USBD_DRV     *p_drv,
                                                 CPU_INT08U    ep_addr,
                                                 USBD_ERR     *p_err);

static  CPU_BOOLEAN  USBD_DrvEP_Abort           (USBD_DRV     *p_drv,
                                                 CPU_INT08U    ep_addr);

static  CPU_BOOLEAN  USBD_DrvEP_Stall           (USBD_DRV     *p_drv,
                                                 CPU_INT08U    ep_addr,
                                                 CPU_BOOLEAN   state);

static  void         USBD_DrvISR_Handler        (USBD_DRV     *p_drv);


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void         STM32_RxFIFO_Rd           (USBD_DRV     *p_drv);

static  void         STM32_TxFIFO_Wr           (USBD_DRV     *p_drv,
                                                CPU_INT08U    ep_log_nbr,
                                                CPU_INT08U   *p_buf,
                                                CPU_INT16U    ep_pkt_len);

static  void         STM32_EP_OutProcess       (USBD_DRV     *p_drv);

static  void         STM32_EP_InProcess        (USBD_DRV     *p_drv);


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

                                                                /* --------------- STM32F_FS DRIVER API --------------- */
USBD_DRV_API  USBD_DrvAPI_STM32F_FS     = { USBD_DrvSTM32F_FS_Init,
                                            USBD_DrvSTM32F_FS_Start,
                                            USBD_DrvStop,
                                            USBD_DrvAddrSet,
                                            DEF_NULL,
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


                                                                /* ------------- STM32F_OTG_FS DRIVER API ------------- */
USBD_DRV_API  USBD_DrvAPI_STM32F_OTG_FS = { USBD_DrvSTM32F_OTG_FS_Init,
                                            USBD_DrvSTM32F_OTG_FS_Start,
                                            USBD_DrvStop,
                                            USBD_DrvAddrSet,
                                            DEF_NULL,
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

                                                                /* -------------- EFM32_OTG_FS DRIVER API ------------- */
USBD_DRV_API  USBD_DrvAPI_EFM32_OTG_FS  = { USBD_DrvEFM32_OTG_FS_Init,
                                            USBD_DrvEFM32_OTG_FS_Start,
                                            USBD_DrvStop,
                                            USBD_DrvAddrSet,
                                            DEF_NULL,
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

                                                                /* --------------- XMC_OTG_FS DRIVER API -------------- */
USBD_DRV_API  USBD_DrvAPI_XMC_OTG_FS    = { USBD_DrvXMC_OTG_FS_Init,
                                            USBD_DrvXMC_OTG_FS_Start,
                                            USBD_DrvStop,
                                            USBD_DrvAddrSet,
                                            DEF_NULL,
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
*                                      USBD_DrvSTM32F_FS_Init()
*                                    USBD_DrvSTM32F_OTG_FS_Init()
*                                     USBD_DrvEFM32_OTG_FS_Init()
*                                      USBD_DrvXMC_OTG_FS_Init()
*
* Description : Initialize the USB Full-speed device controller.
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

static  void  USBD_DrvSTM32F_FS_Init (USBD_DRV  *p_drv,
                                      USBD_ERR  *p_err)
{
    USBD_DrvHandlerInit(p_drv, DRV_STM32F_FS, p_err);
}


static  void  USBD_DrvSTM32F_OTG_FS_Init (USBD_DRV  *p_drv,
                                          USBD_ERR  *p_err)
{
    USBD_DrvHandlerInit(p_drv, DRV_STM32F_OTG_FS, p_err);
}


static  void  USBD_DrvEFM32_OTG_FS_Init (USBD_DRV  *p_drv,
                                         USBD_ERR  *p_err)
{
    USBD_DrvHandlerInit(p_drv, DRV_EFM32_OTG_FS, p_err);
}


static  void  USBD_DrvXMC_OTG_FS_Init (USBD_DRV  *p_drv,
                                       USBD_ERR  *p_err)
{
    USBD_DrvHandlerInit(p_drv, DRV_XMC_OTG_FS, p_err);
}


/*
*********************************************************************************************************
*                                        USBD_DrvHandlerInit()
*
* Description : Initialize the Full-speed device.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               drv_type    Driver type:
*
*                               DRV_STM32F_FS              Driver for STM32F10x, STM32F2x and STM32F4x
*                               DRV_STM32F_OTG_FS          Driver for STM32F74xx and STM32F75xx series.
*                               DRV_EFM32_OTG_FS           Driver for EFM32 Gecko series.
*                               DRV_XMC_OTG_FS             Driver for XMC4000 series.
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

static  void  USBD_DrvHandlerInit (USBD_DRV    *p_drv,
                                   CPU_INT16U   drv_type,
                                   USBD_ERR    *p_err)
{
    CPU_INT08U           ep_nbr;
    CPU_INT32U           reg_to;
    CPU_REG32            ctrl_reg;
    CPU_SIZE_T           reqd_octets;
    LIB_ERR              lib_mem_err;
    USBD_STM32F_FS_REG  *p_reg;
    USBD_DRV_BSP_API    *p_bsp_api;
    USBD_DRV_DATA_EP    *p_drv_data;


                                                                /* Alloc drv internal data.                             */
    p_drv_data = (USBD_DRV_DATA_EP *)Mem_HeapAlloc(sizeof(USBD_DRV_DATA_EP),
                                                   sizeof(CPU_ALIGN),
                                                  &reqd_octets,
                                                  &lib_mem_err);
    if (p_drv_data == (void *)0) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

    Mem_Clr(p_drv_data, sizeof(USBD_DRV_DATA_EP));

    p_drv_data->DrvType = drv_type;
    p_drv->DataPtr      = p_drv_data;                           /* Store drv internal data ptr.                         */

    p_bsp_api =  p_drv->BSP_API_Ptr;                            /* Get driver BSP API reference.                        */
    p_reg     = (USBD_STM32F_FS_REG *)p_drv->CfgPtr->BaseAddr;  /* Get USB ctrl reg ref.                                */

    if (p_bsp_api->Init != (void *)0) {
        p_bsp_api->Init(p_drv);                                 /* Call board/chip specific device controller ...       */
                                                                /* ... initialization function.                         */
    }

                                                                /* Disable the global interrupt                         */
    DEF_BIT_CLR(p_reg->GAHBCFG, STM32F_FS_GAHBCFG_BIT_GINTMSK);

                                                                /* --------------------- PHY CFG ---------------------- */
    switch (drv_type){
        case DRV_STM32F_FS:
        case DRV_STM32F_OTG_FS:
                                                                /* Full Speed serial transceivers                       */
             DEF_BIT_SET(p_reg->GUSBCFG, STM32F_FS_GUSBCFG_BIT_PHYSEL);
             break;


        case DRV_EFM32_OTG_FS:
        case DRV_XMC_OTG_FS:
        default:
             break;
    }
                                                                /* -------------------- CORE RESET -------------------- */
    reg_to = STM32F_FS_REG_VAL_TO;                              /* Check AHB master state machine is in IDLE condition  */
    while ((DEF_BIT_IS_CLR(p_reg->GRSTCTL, STM32F_FS_GRSTCTL_BIT_AHBIDL)) &&
           (reg_to > 0u)) {
        reg_to--;
    }

    switch (drv_type) {
        case DRV_STM32F_FS:
                                                                /* Clear the control logic in the AHB Clock domain      */
             DEF_BIT_SET(p_reg->GRSTCTL, STM32F_FS_GRSTCTL_BIT_HSRST);
             reg_to = STM32F_FS_REG_VAL_TO;                     /* Check all necessary logic is reset in the core       */
             while ((DEF_BIT_IS_SET(p_reg->GRSTCTL, STM32F_FS_GRSTCTL_BIT_HSRST)) &&
                    (reg_to > 0u)) {
                 reg_to--;
             }
             break;


        default:
             break;
    }

    DEF_BIT_SET(p_reg->GRSTCTL, STM32F_FS_GRSTCTL_BIT_CSRST);   /* Resets the HCLK and PCLK domains                     */

    reg_to = STM32F_FS_REG_VAL_TO;                              /* Check all necessary logic is reset in the core       */
    while ((DEF_BIT_IS_SET(p_reg->GRSTCTL, STM32F_FS_GRSTCTL_BIT_CSRST)) &&
           (reg_to > 0u)) {
        reg_to--;
    }

    if (drv_type != DRV_XMC_OTG_FS) {
        p_reg->GCCFG = STM32F_FS_GCCFG_BIT_PWRDWN;
    }

    DEF_BIT_SET(p_reg->GUSBCFG, STM32F_FS_GUSBCFG_BIT_FDMOD);   /* Force the core to device mode                        */
    reg_to = STM32F_FS_REG_FMOD_TO;                             /* Wait at least 25ms before the change takes effect    */
    while (reg_to > 0u) {
        reg_to--;
    }

    switch (drv_type) {
        case DRV_STM32F_FS:
             p_reg->GCCFG |= STM32F_FS_GCCFG_BIT_NONVBUSSENS;   /* Vbus sensing not needed for device mode without OTG  */
             break;


        case DRV_STM32F_OTG_FS:
                                                                /* Vbus detection disable.                              */
             DEF_BIT_CLR(p_reg->GCCFG, STM32F_FS_GCCFG_BIT_VBDEN);
             DEF_BIT_SET(p_reg->GOTGCTL, (STM32F_FS_GOTGCTL_BIT_BVALOEN  |
                                          STM32F_FS_GOTGCTL_BIT_BVALOVAL));
             break;


        case DRV_EFM32_OTG_FS:
        case DRV_XMC_OTG_FS:
        default:
             break;
    }

                                                                /* ------------------- DEVICE INIT -------------------- */
    p_reg->PCGCR = DEF_BIT_NONE;                                /* Reset the PHY clock                                  */

    p_reg->DCFG = STM32F_FS_DCFG_PFIVL_80 |
                  STM32F_FS_DCFG_DSPD_FULLSPEED;

    p_reg->GRSTCTL = STM32F_FS_GRSTCTL_BIT_TXFFLSH |            /* Flush all transmit FIFOs                             */
                     STM32F_FS_GRSTCTL_FLUSH_TXFIFO_ALL;

    reg_to = STM32F_FS_REG_VAL_TO;                              /* Wait for the flush completion                        */
    while ((DEF_BIT_IS_SET(p_reg->GRSTCTL, STM32F_FS_GRSTCTL_BIT_TXFFLSH)) &&
           (reg_to > 0u)) {
        reg_to--;
    }

                                                                /* Flush the receive FIFO                               */
    DEF_BIT_SET(p_reg->GRSTCTL, STM32F_FS_GRSTCTL_BIT_RXFFLSH);

    reg_to = STM32F_FS_REG_VAL_TO;                              /* Wait for the flush completion                        */
    while ((DEF_BIT_IS_SET(p_reg->GRSTCTL, STM32F_FS_GRSTCTL_BIT_RXFFLSH)) &&
           (reg_to > 0u)) {
        reg_to--;
    }
                                                                /* Clear all pending Device interrupts                  */
    p_reg->DIEPMSK  = DEF_BIT_NONE;                             /* Dis. interrupts for the Device IN Endpoints          */
    p_reg->DOEPMSK  = DEF_BIT_NONE;                             /* Dis. interrupts for the Device OUT Endpoints         */
    p_reg->DAINTMSK = DEF_BIT_NONE;                             /* Dis. interrupts foo all Device Endpoints             */

    ctrl_reg = STM32F_FS_DxEPCTLx_BIT_EPDIS |
               STM32F_FS_DxEPCTLx_BIT_SNAK;

    for (ep_nbr = 0u; ep_nbr < STM32F_FS_NBR_EPS; ep_nbr++) {
                                                                /* ----------------- IN ENDPOINT RESET ---------------- */
        p_reg->DIEP[ep_nbr].CTLx  = ctrl_reg;
        p_reg->DIEP[ep_nbr].TSIZx = DEF_BIT_NONE;
        p_reg->DIEP[ep_nbr].INTx  = 0x000000FFu;                /* Clear any pending Interrupt                          */

                                                                /* ---------------- OUT ENDPOINT RESET ---------------- */
        p_reg->DOEP[ep_nbr].CTLx  = ctrl_reg;
        p_reg->DOEP[ep_nbr].TSIZx = DEF_BIT_NONE;
        p_reg->DOEP[ep_nbr].INTx  = 0x000000FFu;                /* Clear any pending Interrupt                          */
    }

    p_reg->GINTMSK = DEF_BIT_NONE;                              /* Disable all interrupts                               */

    DEF_BIT_CLR(p_reg->DCFG, STM32F_FS_DCFG_DAD_MASK);          /* Set Device Address to zero                           */

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                      USBD_DrvSTM32F_FS_Start()
*                                    USBD_DrvSTM32F_OTG_FS_Start()
*                                    USBD_DrvEFM32_OTG_FS_Start()
*                                     USBD_DrvXMC_OTG_FS_Start()
*
* Description : Start device operation with VBUS detection enable.
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

static  void  USBD_DrvSTM32F_FS_Start (USBD_DRV  *p_drv,
                                       USBD_ERR  *p_err)
{
    USBD_DrvHandlerStart(p_drv, DRV_STM32F_FS, p_err);
}


static  void  USBD_DrvSTM32F_OTG_FS_Start (USBD_DRV  *p_drv,
                                           USBD_ERR  *p_err)
{
    USBD_DrvHandlerStart(p_drv, DRV_STM32F_OTG_FS, p_err);
}


static  void  USBD_DrvEFM32_OTG_FS_Start (USBD_DRV  *p_drv,
                                          USBD_ERR  *p_err)
{
    USBD_DrvHandlerStart(p_drv, DRV_EFM32_OTG_FS, p_err);
}


static  void  USBD_DrvXMC_OTG_FS_Start (USBD_DRV  *p_drv,
                                        USBD_ERR  *p_err)
{
    USBD_DrvHandlerStart(p_drv, DRV_XMC_OTG_FS, p_err);
}


/*
*********************************************************************************************************
*                                           USBD_DrvHandlerStart()
*
* Description : Start device operation :
*
*                   (1) Enable device controller bus state interrupts.
*                   (2) Call board/chip specific connect function.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               drv_type    Driver type:
*
*                               DRV_STM32F_FS          Driver for STM32F10x, STM32F2x, & STM32F4x series.
*                               DRV_STM32F_OTG_FS      Driver for STM32F74xx and STM32F75xx series.
*                               DRV_EFM32_OTG_FS       Driver for EFM32 Gecko series.
*                               DRV_XMC_OTG_FS         Driver for XMC4000 series.
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

static  void  USBD_DrvHandlerStart (USBD_DRV    *p_drv,
                                    CPU_INT16U   drv_type,
                                    USBD_ERR    *p_err)
{
    USBD_DRV_BSP_API    *p_bsp_api;
    USBD_STM32F_FS_REG  *p_reg;


    p_bsp_api =  p_drv->BSP_API_Ptr;                            /* Get driver BSP API reference.                        */
    p_reg     = (USBD_STM32F_FS_REG *)p_drv->CfgPtr->BaseAddr;  /* Get USB ctrl reg ref.                                */

    p_reg->GINTSTS = 0xFFFFFFFFu;                               /* Clear any pending interrupt                          */
                                                                /* Enable interrupts                                    */
    p_reg->GINTMSK = STM32F_FS_GINTMSK_BIT_USBSUSPM |
                     STM32F_FS_GINTMSK_BIT_USBRST   |
                     STM32F_FS_GINTMSK_BIT_ENUMDNEM |
                     STM32F_FS_GINTMSK_BIT_IISOOXFRM |
                     STM32F_FS_GINTMSK_BIT_IISOIXFRM |
                     STM32F_FS_GINTMSK_BIT_WUIM;

    switch (drv_type) {
        case DRV_STM32F_FS:
             p_reg->GINTMSK |= (STM32F_FS_GINTMSK_BIT_OTGINT  |
                                STM32F_FS_GINTMSK_BIT_SRQIM);
             break;


        case DRV_STM32F_OTG_FS:
        case DRV_EFM32_OTG_FS:
        case DRV_XMC_OTG_FS:
        default:
             break;
    }

                                                                /* Enable Global Interrupt                              */
    DEF_BIT_SET(p_reg->GAHBCFG, STM32F_FS_GAHBCFG_BIT_GINTMSK);

    if (p_bsp_api->Conn != (void *)0) {
        p_bsp_api->Conn();                                      /* Call board/chip specific connect function.           */
    }

    DEF_BIT_CLR(p_reg->DCTL, STM32F_FS_DCTL_BIT_SDIS);          /* Generate Device connect event to the USB host        */

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           USBD_DrvStop()
*
* Description : Stop Full-speed device operation.
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
    USBD_DRV_BSP_API    *p_bsp_api;
    USBD_STM32F_FS_REG  *p_reg;


    p_bsp_api =  p_drv->BSP_API_Ptr;                            /* Get driver BSP API reference.                        */
    p_reg     = (USBD_STM32F_FS_REG *)p_drv->CfgPtr->BaseAddr;  /* Get USB ctrl reg ref.                                */

    if (p_bsp_api->Disconn != (void *)0) {
        p_bsp_api->Disconn();
    }

    p_reg->GINTMSK = DEF_BIT_NONE;                              /* Disable all interrupts                               */
    p_reg->GINTSTS = 0xFFFFFFFF;                                /* Clear any pending interrupt                          */
                                                                /* Disable the global interrupt                         */
    DEF_BIT_CLR(p_reg->GAHBCFG, STM32F_FS_GAHBCFG_BIT_GINTMSK);

    DEF_BIT_SET(p_reg->DCTL, STM32F_FS_DCTL_BIT_SDIS);          /* Generate Device Disconnect event to the USB host     */
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
    USBD_STM32F_FS_REG  *p_reg;


    p_reg = (USBD_STM32F_FS_REG *)(p_drv->CfgPtr->BaseAddr);

    DEF_BIT_CLR(p_reg->DCFG, STM32F_FS_DCFG_DAD_MASK);
    p_reg->DCFG |= (dev_addr << 4u);                            /* Set Device Address                                   */

    return (DEF_OK);
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
    CPU_INT16U           frame_nbr;
    USBD_STM32F_FS_REG  *p_reg;


    p_reg     = (USBD_STM32F_FS_REG *)(p_drv->CfgPtr->BaseAddr);
    frame_nbr = (p_reg->DSTS & STM32F_FS_DSTS_FNSOF_MASK) >> 8u;

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

    CPU_INT08U           ep_log_nbr;
    CPU_INT08U           ep_phy_nbr;
    CPU_INT32U           reg_val;
    USBD_DRV_DATA_EP    *p_drv_data;
    USBD_STM32F_FS_REG  *p_reg;


    (void)transaction_frame;

    p_reg      = (USBD_STM32F_FS_REG *)(p_drv->CfgPtr->BaseAddr);
    p_drv_data = (USBD_DRV_DATA_EP   *)(p_drv->DataPtr);
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    reg_val    =  0u;

    USBD_DBG_DRV_EP("  Drv EP FIFO Open", ep_addr);

    switch (ep_type) {
        case USBD_EP_TYPE_CTRL:
             if (USBD_EP_IS_IN(ep_addr) == DEF_YES) {
                                                                /* En. Device EP0 IN interrupt                          */
                 DEF_BIT_SET(p_reg->DAINTMSK, STM32F_FS_DAINTMSK_BIT_IEPINT_EP0);
                                                                /* En. common IN EP Transfer complete interrupt         */
                 DEF_BIT_SET(p_reg->DIEPMSK, STM32F_FS_DIEPMSK_BIT_XFRCM);

             } else {
                                                                /* En. EP0 OUT interrupt                                */
                 DEF_BIT_SET(p_reg->DAINTMSK, STM32F_FS_DAINTMSK_BIT_OEPINT_EP0);
                                                                /* En. common OUT EP Setup $ Xfer complete interrupt    */
                 DEF_BIT_SET(p_reg->DOEPMSK, (STM32F_FS_DOEPMSK_BIT_STUPM | STM32F_FS_DOEPMSK_BIT_XFRCM));
                 p_reg->DOEP[ep_log_nbr].TSIZx = (STM32F_FS_DOEPTSIZx_STUPCNT_3_PKT |
                                                  STM32F_FS_DOEPTSIZ0_BIT_PKTCNT    |
                                                  STM32F_FS_DOEPTSIZ0_XFRSIZ_MAX_64);

                 DEF_BIT_SET(p_reg->DOEP[ep_log_nbr].CTLx, STM32F_FS_DxEPCTLx_BIT_SNAK);
             }

             break;


        case USBD_EP_TYPE_INTR:
        case USBD_EP_TYPE_BULK:
        case USBD_EP_TYPE_ISOC:
             if (USBD_EP_IS_IN(ep_addr)) {
                 reg_val = (max_pkt_size                 |      /* cfg EP max packet size                               */
                           (ep_type    << 18u)           |      /* cfg EP type                                          */
                           (ep_log_nbr << 22u)           |      /* Tx FIFO number                                       */
                           STM32F_FS_DxEPCTLx_BIT_SD0PID |      /* EP start data toggle                                 */
                           STM32F_FS_DxEPCTLx_BIT_USBAEP);      /* USB active EP                                        */

                 p_reg->DIEP[ep_log_nbr].CTLx = reg_val;
                                                                /* En. Device EPx IN Interrupt                          */
                 DEF_BIT_SET(p_reg->DAINTMSK, DEF_BIT(ep_log_nbr));
             } else {
                 reg_val = (max_pkt_size                 |      /* cfg EP max packet size                               */
                           (ep_type << 18u)              |      /* cfg EP type                                          */
                           STM32F_FS_DxEPCTLx_BIT_SD0PID |      /* EP start data toggle                                 */
                           STM32F_FS_DxEPCTLx_BIT_USBAEP);      /* USB active EP                                        */

                 p_reg->DOEP[ep_log_nbr].CTLx = reg_val;

                 reg_val = DEF_BIT(ep_log_nbr) << 16u;
                 DEF_BIT_SET(p_reg->DAINTMSK, reg_val);         /* En. Device EPx OUT Interrupt                         */
             }
             break;


        default:
            *p_err = USBD_ERR_INVALID_ARG;
             return;
    }

    p_drv_data->EP_MaxPktSize[ep_phy_nbr] = max_pkt_size;

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
    CPU_INT08U           ep_log_nbr;
    CPU_INT32U           reg_val;
    USBD_STM32F_FS_REG  *p_reg;


    p_reg      = (USBD_STM32F_FS_REG *)(p_drv->CfgPtr->BaseAddr);
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    reg_val    =  DEF_BIT_NONE;


    USBD_DBG_DRV_EP("  Drv EP Closed", ep_addr);

    if (USBD_EP_IS_IN(ep_addr)) {                               /* ------------------- IN ENDPOINTS ------------------- */
                                                                /* Deactive the Endpoint                                */
        DEF_BIT_CLR(p_reg->DIEP[ep_log_nbr].CTLx, STM32F_FS_DxEPCTLx_BIT_USBAEP);
        reg_val = DEF_BIT(ep_log_nbr);
    } else {                                                    /* ------------------ OUT ENDPOINTS ------------------- */
                                                                /* Deactive the Endpoint                                */
        DEF_BIT_CLR(p_reg->DOEP[ep_log_nbr].CTLx ,STM32F_FS_DxEPCTLx_BIT_USBAEP);
        reg_val = (DEF_BIT(ep_log_nbr)  << 16u);
    }

    DEF_BIT_CLR(p_reg->DAINTMSK, reg_val);                      /* Dis. EP interrupt                                    */
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
*
* Return(s)   : Number of bytes that can be handled in one transfer.
*
* Note(s)     : (1) Isochronous endpoints will allow larger buffers than the packet size in
*                   order to deliver better performance. In the case of larger buffers, those
*                   will be filled by multiple FIFO calls without notifying the application until full.
*********************************************************************************************************
*/

static  CPU_INT32U  USBD_DrvEP_RxStart (USBD_DRV    *p_drv,
                                        CPU_INT08U   ep_addr,
                                        CPU_INT08U  *p_buf,
                                        CPU_INT32U   buf_len,
                                        USBD_ERR    *p_err)
{

    CPU_INT08U           ep_log_nbr;
    CPU_INT08U           ep_phy_nbr;
    CPU_INT08U           ep_type;
    CPU_INT16U           ep_pkt_len;
    CPU_INT16U           pkt_cnt;
    CPU_INT32U           ctl_reg;
    CPU_INT32U           tsiz_reg;
    USBD_DRV_DATA_EP    *p_drv_data;
    USBD_STM32F_FS_REG  *p_reg;


    p_reg      = (USBD_STM32F_FS_REG *)(p_drv->CfgPtr->BaseAddr);
    p_drv_data = (USBD_DRV_DATA_EP   *)p_drv->DataPtr;
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    ep_pkt_len = (CPU_INT16U)DEF_MIN(buf_len, p_drv_data->EP_MaxPktSize[ep_phy_nbr]);

    USBD_DBG_DRV_EP("  Drv EP FIFO RxStart", ep_addr);

    ctl_reg  = p_reg->DOEP[ep_log_nbr].CTLx;                    /* Read Control EP reg                                  */
    tsiz_reg = p_reg->DOEP[ep_log_nbr].TSIZx;                   /* Read Transer EP reg                                  */

                                                                /* Clear EP transfer size and packet count              */
    DEF_BIT_CLR(tsiz_reg, (STM32F_FS_DOEPTSIZx_XFRSIZ_MSK | STM32F_FS_DOEPTSIZx_PKTCNT_MSK));

    if (buf_len == 0u) {
        tsiz_reg |=  p_drv_data->EP_MaxPktSize[ep_phy_nbr];     /* Set transfer size to max pkt size                    */
        tsiz_reg |= (1u << 19u);                                /* Set packet count                                     */
    } else {
        pkt_cnt   = (ep_pkt_len + (p_drv_data->EP_MaxPktSize[ep_phy_nbr] - 1u))
                  / p_drv_data->EP_MaxPktSize[ep_phy_nbr];
        tsiz_reg |= (pkt_cnt << 19u);                           /* Set packet count                                     */
                                                                /* Set transfer size                                    */
        tsiz_reg |= pkt_cnt * p_drv_data->EP_MaxPktSize[ep_phy_nbr];
    }

    p_drv_data->EP_AppBufPtr[ep_phy_nbr] = p_buf;
    p_drv_data->EP_AppBufLen[ep_phy_nbr] = ep_pkt_len;
    p_drv_data->EP_AppBufBlk[ep_phy_nbr] = 0;

    ep_type = USBD_GET_EP_TYPE(ctl_reg);
    if (ep_type == USBD_EP_TYPE_ISOC) {                         /* Check if EP is type is Isochronous                    */
      p_drv_data->EP_AppBufLen[ep_phy_nbr] = buf_len;           /* Isochronous endpoints allow larger buffers, see (1)   */
      p_drv_data->EP_AppBufBlk[ep_phy_nbr] = (buf_len + (p_drv_data->EP_MaxPktSize[ep_phy_nbr] - 1u))
                                           / p_drv_data->EP_MaxPktSize[ep_phy_nbr];
      if ((p_reg->DSTS & (1U << 8)) == 0U) {
          ctl_reg |= STM32F_FS_DxEPCTLx_BIT_SODDFRM;

        } else {
          ctl_reg |= STM32F_FS_DxEPCTLx_BIT_SEVNFRM;
        }
    }

                                                                /* Clear EP NAK and Enable EP for receiving             */
    ctl_reg |= STM32F_FS_DxEPCTLx_BIT_CNAK | STM32F_FS_DxEPCTLx_BIT_EPENA;

    p_drv_data->EP_PktXferLen[ep_phy_nbr] = 0u;
    p_reg->DOEP[ep_log_nbr].TSIZx         = tsiz_reg;
    p_reg->DOEP[ep_log_nbr].CTLx          = ctl_reg;

   *p_err = USBD_ERR_NONE;

    return (p_drv_data->EP_AppBufLen[ep_phy_nbr]);
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
    CPU_INT08U         ep_phy_nbr;
    CPU_INT16U         xfer_len;
    USBD_DRV_DATA_EP  *p_drv_data;


    (void)p_buf;

    p_drv_data = (USBD_DRV_DATA_EP *)p_drv->DataPtr;
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    xfer_len   =  p_drv_data->EP_PktXferLen[ep_phy_nbr];

    USBD_DBG_DRV_EP("  Drv EP FIFO Rx", ep_addr);

    if (xfer_len > buf_len) {
       *p_err = USBD_ERR_RX;
    } else {
       *p_err = USBD_ERR_NONE;
    }

    return (DEF_MIN(xfer_len, buf_len));
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


    USBD_DBG_DRV_EP("  Drv EP RxZLP", ep_addr);

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
    CPU_INT08U         ep_phy_nbr;
    CPU_INT16U         ep_pkt_len;
    USBD_DRV_DATA_EP  *p_drv_data;


    (void)p_buf;

    p_drv_data = (USBD_DRV_DATA_EP *)p_drv->DataPtr;
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    ep_pkt_len = (CPU_INT16U)DEF_MIN(p_drv_data->EP_MaxPktSize[ep_phy_nbr], buf_len);

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
    CPU_INT08U           ep_log_nbr;
    CPU_INT08U           ep_type;
    CPU_INT32U           ctl_reg;
    CPU_INT32U           tsiz_reg;
    USBD_STM32F_FS_REG  *p_reg;


    p_reg      = (USBD_STM32F_FS_REG *)(p_drv->CfgPtr->BaseAddr);
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);

    ctl_reg  = p_reg->DIEP[ep_log_nbr].CTLx;                    /* Read EP control reg.                                 */
    tsiz_reg = p_reg->DIEP[ep_log_nbr].TSIZx;                   /* Read EP transfer reg                                 */

    DEF_BIT_CLR(tsiz_reg, STM32F_FS_DIEPTSIZx_XFRSIZ_MSK);      /* Clear EP transfer size                               */
    DEF_BIT_CLR(tsiz_reg, STM32F_FS_DIEPTSIZx_PKTCNT_MSK);      /* Clear EP packet count                                */
    tsiz_reg |=  buf_len;                                       /* Transfer size                                        */
    tsiz_reg |= (1u << 19u);                                    /* Packet count                                         */

    ep_type = USBD_GET_EP_TYPE(ctl_reg);
    if (ep_type == USBD_EP_TYPE_ISOC) {
        DEF_BIT_CLR(tsiz_reg, STM32F_FS_DIEPTSIZx_MCNT_3_PKT);
        tsiz_reg |= (1 << 29u);                                 /* Multi count set to 1 packet.                         */
    }

                                                                /* Clear EP NAK mode & Enable EP transmitting.          */
    ctl_reg |= STM32F_FS_DxEPCTLx_BIT_CNAK | STM32F_FS_DxEPCTLx_BIT_EPENA;

    p_reg->DIEP[ep_log_nbr].TSIZx = tsiz_reg;
    p_reg->DIEP[ep_log_nbr].CTLx  = ctl_reg;

    if (ep_type == USBD_EP_TYPE_ISOC) {                         /* Chek if EP is type is Isochronous                    */
        if ((p_reg->DSTS & (1U << 8)) == 0U) {
          p_reg->DIEP[ep_log_nbr].CTLx |= STM32F_FS_DxEPCTLx_BIT_SODDFRM;
        } else {
          p_reg->DIEP[ep_log_nbr].CTLx |= STM32F_FS_DxEPCTLx_BIT_SEVNFRM;
        }
    }

    STM32_TxFIFO_Wr(p_drv, ep_log_nbr, p_buf, buf_len);         /* Write to Tx FIFO of associated EP                    */

    USBD_DBG_DRV_EP_ARG("  Drv EP FIFO Tx Len:", ep_addr, buf_len);

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
    USBD_DBG_DRV_EP("  Drv EP TxZLP", ep_addr);

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
    CPU_INT08U           ep_log_nbr;
    CPU_INT32U           reg_to;
    USBD_STM32F_FS_REG  *p_reg;


    p_reg      = (USBD_STM32F_FS_REG *)p_drv->CfgPtr->BaseAddr;
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);

    if (USBD_EP_IS_IN(ep_addr) == DEF_YES) {                    /* ------------------- IN ENDPOINTS ------------------- */
                                                                /* Set Endpoint to NAK mode                             */
        DEF_BIT_SET(p_reg->DIEP[ep_log_nbr].CTLx, STM32F_FS_DxEPCTLx_BIT_SNAK);

        reg_to = STM32F_FS_REG_VAL_TO;                          /* Wait for IN EP NAK effective                         */
        while ((DEF_BIT_IS_CLR(p_reg->DIEP[ep_log_nbr].INTx, STM32F_FS_DIEPINTx_BIT_INEPNE) == DEF_YES) &&
               (reg_to > 0u)) {
            reg_to--;
        }

        DEF_BIT_SET(p_reg->DIEP[ep_log_nbr].CTLx, STM32F_FS_DxEPCTLx_BIT_EPDIS);

        reg_to = STM32F_FS_REG_VAL_TO;                          /* Wait for EP disable                                  */
        while ((DEF_BIT_IS_SET(p_reg->DIEP[ep_log_nbr].CTLx, STM32F_FS_DxEPCTLx_BIT_EPDIS) == DEF_YES) &&
               (reg_to > 0u)) {
            reg_to--;
        }
                                                                /* Clear EP Disable interrupt                           */
        DEF_BIT_SET(p_reg->DIEP[ep_log_nbr].INTx, STM32F_FS_DIEPINTx_BIT_EPDISD);

                                                                /* Flush EPx TX FIFO                                    */
        p_reg->GRSTCTL = STM32F_FS_GRSTCTL_BIT_TXFFLSH |  (ep_log_nbr << 6u);

        reg_to = STM32F_FS_REG_VAL_TO;                          /* Wait for the flush completion                        */
        while ((DEF_BIT_IS_SET(p_reg->GRSTCTL, STM32F_FS_GRSTCTL_BIT_TXFFLSH) == DEF_YES) &&
               (reg_to > 0u)) {
            reg_to--;
        }

    } else {                                                    /* ------------------ OUT ENDPOINTS ------------------- */

        DEF_BIT_SET(p_reg->DOEP[ep_log_nbr].CTLx, STM32F_FS_DxEPCTLx_BIT_SNAK);

                                                                /* Flush EPx RX FIFO                                    */
        DEF_BIT_SET(p_reg->GRSTCTL, STM32F_FS_GRSTCTL_BIT_RXFFLSH);

        reg_to = STM32F_FS_REG_VAL_TO;                          /* Wait for the flush completion                        */
        while ((DEF_BIT_IS_SET(p_reg->GRSTCTL, STM32F_FS_GRSTCTL_BIT_RXFFLSH) == DEF_YES) &&
               (reg_to > 0u)) {
            reg_to--;
        }
    }

    USBD_DBG_DRV_EP("  Drv EP Abort", ep_addr);

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
    CPU_INT08U           ep_log_nbr;
    CPU_INT32U           ep_type;
    USBD_STM32F_FS_REG  *p_reg;


    p_reg      = (USBD_STM32F_FS_REG *)p_drv->CfgPtr->BaseAddr;
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);

    USBD_DBG_DRV_EP("  Drv EP Stall", ep_addr);

    if (USBD_EP_IS_IN(ep_addr) == DEF_YES) {                    /* ------------------- IN ENDPOINTS ------------------- */
        if (state == DEF_SET) {
                                                                /* Disable Endpoint if enable for transmit              */
            if (DEF_BIT_IS_SET(p_reg->DIEP[ep_log_nbr].CTLx, STM32F_FS_DxEPCTLx_BIT_EPENA) == DEF_YES) {
                DEF_BIT_SET(p_reg->DIEP[ep_log_nbr].CTLx, STM32F_FS_DxEPCTLx_BIT_EPDIS);
            }
                                                                /* Set Stall bit                                        */
            DEF_BIT_SET(p_reg->DIEP[ep_log_nbr].CTLx, STM32F_FS_DxEPCTLx_BIT_STALL);

        } else {
            ep_type = (p_reg->DIEP[ep_log_nbr].CTLx >> 18u) & DEF_BIT_FIELD(2u, 0u);
                                                                /* Clear Stall Bit                                      */
            DEF_BIT_CLR(p_reg->DIEP[ep_log_nbr].CTLx, STM32F_FS_DxEPCTLx_BIT_STALL);

            if ((ep_type == USBD_EP_TYPE_INTR) ||
                (ep_type == USBD_EP_TYPE_BULK)) {
                                                                /* Set DATA0 PID                                        */
                DEF_BIT_SET(p_reg->DIEP[ep_log_nbr].CTLx, STM32F_FS_DxEPCTLx_BIT_SD0PID);
            }
        }

    } else {                                                    /* ------------------- OUT ENDPOINTS ------------------ */
        if (state == DEF_SET) {
                                                                /* Set Stall bit                                        */
            DEF_BIT_SET(p_reg->DOEP[ep_log_nbr].CTLx, STM32F_FS_DxEPCTLx_BIT_STALL);

        } else {
            ep_type = (p_reg->DOEP[ep_log_nbr].CTLx >> 18u) & DEF_BIT_FIELD(2u, 0u);
                                                                /* Clear Stall Bit                                      */
            DEF_BIT_CLR(p_reg->DOEP[ep_log_nbr].CTLx, STM32F_FS_DxEPCTLx_BIT_STALL);

            if ((ep_type == USBD_EP_TYPE_INTR) || (ep_type == USBD_EP_TYPE_BULK)) {
                                                                /* Set DATA0 PID                                        */
                DEF_BIT_SET(p_reg->DOEP[ep_log_nbr].CTLx, STM32F_FS_DxEPCTLx_BIT_SD0PID);
            }
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
    CPU_INT32U           int_stat;
    CPU_INT32U           otgint_stat;
    USBD_STM32F_FS_REG  *p_reg;
    USBD_DRV_DATA_EP    *p_drv_data;


    p_drv_data = (USBD_DRV_DATA_EP   *)p_drv->DataPtr;
    p_reg      = (USBD_STM32F_FS_REG *)p_drv->CfgPtr->BaseAddr;
    int_stat   =  p_reg->GINTSTS;                               /* Read global interrrupt status register               */


                                                                /* ------------ RX FIFO NON-EMPTY DETECTION ----------- */
    if (DEF_BIT_IS_SET(int_stat, STM32F_FS_GINTSTS_BIT_RXFLVL) == DEF_YES) {
        STM32_RxFIFO_Rd(p_drv);
    }

                                                                /* --------------- IN ENDPOINT INTERRUPT -------------- */
    if (DEF_BIT_IS_SET(int_stat, STM32F_FS_GINTSTS_BIT_IEPINT) == DEF_YES) {
        STM32_EP_InProcess(p_drv);
    }

                                                                /* -------------- OUT ENDPOINT INTERRUPT -------------- */
    if (DEF_BIT_IS_SET(int_stat, STM32F_FS_GINTSTS_BIT_OEPINT) == DEF_YES) {
        STM32_EP_OutProcess(p_drv);
    }
                                                                /* -------- INCOMPLETE ISOCHRONOUS OUT TRANSFER ------- */
    if (DEF_BIT_IS_SET(int_stat, STM32F_FS_GINTSTS_BIT_INCOMPISOOUT) == DEF_YES) {
        p_reg->GINTSTS &= STM32F_FS_GINTSTS_BIT_INCOMPISOOUT;
    }
                                                                /* -------- INCOMPLETE ISOCHRONOUS IN TRANSFER -------- */
    if (DEF_BIT_IS_SET(int_stat, STM32F_FS_GINTSTS_BIT_IISOIXFR) == DEF_YES) {
        p_reg->GINTSTS &= STM32F_FS_GINTSTS_BIT_IISOIXFR;
    }

                                                                /* ---------------- USB RESET DETECTION --------------- */
    if (DEF_BIT_IS_SET(int_stat, STM32F_FS_GINTSTS_BIT_USBRST) == DEF_YES) {
        USBD_EventReset(p_drv);                                 /* Notify bus reset event.                              */
                                                                /* Enable Global OUT/IN EP interrupt...                 */
                                                                /* ...and RX FIFO non-empty interrupt.                  */
        DEF_BIT_CLR(p_reg->DCTL, STM32F_FS_DCTL_BIT_RWUSIG);    /* Clear Remote wakeup signaling                        */

        DEF_BIT_SET(p_reg->GINTMSK, (STM32F_FS_GINTMSK_BIT_OEPINT   |
                                     STM32F_FS_GINTMSK_BIT_IEPINT   |
                                     STM32F_FS_GINTMSK_BIT_RXFLVLM));
                                                                /* Clear USB bus reset interrupt                        */
        DEF_BIT_SET(p_reg->GINTSTS, STM32F_FS_GINTSTS_BIT_USBRST);

        USBD_DrvAddrSet(p_drv, 0u);

        p_reg->GRXFSIZ     =  STM32F_FS_RXFIFO_SIZE;            /* Set Rx FIFO depth                                    */
                                                                /* Set EP0 to EP3 Tx FIFO depth                         */
        p_reg->DIEPTXF0    = (STM32F_FS_TXFIFO_EP0_SIZE << 16u) | STM32F_FS_TXFIFO_EP0_START_ADDR;
        p_reg->DIEPTXFx[0] = (STM32F_FS_TXFIFO_EP1_SIZE << 16u) | STM32F_FS_TXFIFO_EP1_START_ADDR;
        p_reg->DIEPTXFx[1] = (STM32F_FS_TXFIFO_EP2_SIZE << 16u) | STM32F_FS_TXFIFO_EP2_START_ADDR;
        p_reg->DIEPTXFx[2] = (STM32F_FS_TXFIFO_EP3_SIZE << 16u) | STM32F_FS_TXFIFO_EP3_START_ADDR;

                                                                /* --------- STM32F_OTG_FS EXTRA EP ALLOCATION -------- */
        if (p_drv_data->DrvType != DRV_STM32F_FS) {             /* Set EP4 to EP5 Tx FIFO depth                         */
            p_reg->DIEPTXFx[3] = (STM32F_FS_TXFIFO_EP4_SIZE << 16u) | STM32F_FS_TXFIFO_EP4_START_ADDR;
            p_reg->DIEPTXFx[4] = (STM32F_FS_TXFIFO_EP5_SIZE << 16u) | STM32F_FS_TXFIFO_EP5_START_ADDR;

            if (p_drv_data->DrvType == DRV_XMC_OTG_FS) {        /* Set EP6 Tx FIFO depth                                */
                p_reg->DIEPTXFx[5] = (STM32F_FS_TXFIFO_EP6_SIZE << 16u) | STM32F_FS_TXFIFO_EP6_START_ADDR;
            }
        }
    }

                                                                /* ------------ ENUMERATION DONE DETECTION ------------ */
    if (DEF_BIT_IS_SET(int_stat, STM32F_FS_GINTSTS_BIT_ENUMDNE) == DEF_YES) {
        if (DEF_BIT_IS_SET(p_reg->DSTS, STM32F_FS_DSTS_ENUMSPD_FS_PHY_48MHZ) == DEF_YES) {
            DEF_BIT_CLR(p_reg->DIEP[0].CTLx, STM32F_FS_DxEPCTL0_MPSIZ_64_MSK);
            DEF_BIT_CLR(p_reg->GUSBCFG, STM32F_FS_GUSBCFG_TRDT_MASK);
            p_reg->GUSBCFG |= (5 << 10);                        /* turn around time                                     */
        }

        DEF_BIT_SET(p_reg->DCTL, STM32F_FS_DCTL_BIT_CGINAK);
                                                                /* Clear Enumeration done interrupt                     */
        DEF_BIT_SET(p_reg->GINTSTS, STM32F_FS_GINTSTS_BIT_ENUMDNE);
    }

                                                                /* ------------------ MODE MISMATCH ------------------- */
    if (DEF_BIT_IS_SET(int_stat, STM32F_FS_GINTSTS_BIT_MMIS) == DEF_YES) {
        DEF_BIT_SET(p_reg->GINTSTS, STM32F_FS_GINTSTS_BIT_MMIS);
    }

                                                                /* -------------- SESSION REQ DETECTION --------------- */
    if (DEF_BIT_IS_SET(int_stat, STM32F_FS_GINTSTS_BIT_SRQINT) == DEF_YES) {

        USBD_EventConn(p_drv);                                  /* Notify connect event.                                */
                                                                /* Clear all OTG interrupt sources.                     */
        DEF_BIT_SET(p_reg->GINTSTS, STM32F_FS_GINTSTS_BIT_SRQINT);
    }

                                                                /* ------------------ OTG INTERRUPT ------------------- */
    if (DEF_BIT_IS_SET(int_stat, STM32F_FS_GINTSTS_BIT_OTGINT) == DEF_YES) {

        otgint_stat = p_reg->GOTGINT;
        if (DEF_BIT_IS_SET(otgint_stat, STM32F_FS_GOTGINT_BIT_SEDET) == DEF_YES) {
            USBD_EventDisconn(p_drv);                           /* Notify disconnect event.                             */
        }
                                                                /* Clear all OTG interrupt sources.                     */
        DEF_BIT_SET(p_reg->GOTGINT, (STM32F_FS_GOTGINT_BIT_SEDET   |
                                     STM32F_FS_GOTGINT_BIT_SRSSCHG |
                                     STM32F_FS_GOTGINT_BIT_HNSSCHG |
                                     STM32F_FS_GOTGINT_BIT_HNGDET  |
                                     STM32F_FS_GOTGINT_BIT_ADTOCHG |
                                     STM32F_FS_GOTGINT_BIT_DBCDNE));
    }

                                                                /* ------------- EARLY SUSPEND DETECTION -------------- */
    if (DEF_BIT_IS_SET(int_stat, STM32F_FS_GINTSTS_BIT_ESUSP) == DEF_YES) {
                                                                /* Clear the Early Suspend interrupt                    */
        DEF_BIT_SET(p_reg->GINTSTS, STM32F_FS_GINTSTS_BIT_ESUSP);
    }

                                                                /* --------------- USB SUSPEND DETECTION -------------- */
    if (DEF_BIT_IS_SET(int_stat, STM32F_FS_GINTSTS_BIT_USBSUSP) == DEF_YES) {
        if (DEF_BIT_IS_SET(p_reg->DSTS, STM32F_FS_DSTS_BIT_SUSPSTS) == DEF_YES) {
            USBD_EventSuspend(p_drv);                           /* Notify Suspend Event                                 */
        }
                                                                /* Clear USB Suspend interrupt                          */
        DEF_BIT_SET(p_reg->GINTSTS, STM32F_FS_GINTSTS_BIT_USBSUSP);
    }

                                                                /* ----------------- WAKE-UP DETECTION ---------------- */
    if (DEF_BIT_IS_SET(int_stat, STM32F_FS_GINTSTS_BIT_WKUPINT) == DEF_YES) {
        USBD_EventResume(p_drv);                                /* Notify Resume Event                                  */

        DEF_BIT_CLR(p_reg->PCGCR, (STM32F_FS_PCGCCTL_BIT_GATEHCLK |
                                   STM32F_FS_PCGCCTL_BIT_STPPCLK));

        DEF_BIT_CLR(p_reg->DCTL, STM32F_FS_DCTL_BIT_RWUSIG);    /* Clear Remote wakeup signaling                        */
                                                                /* Clear Remote wakeup interrupt                        */
        DEF_BIT_SET(p_reg->GINTSTS, STM32F_FS_GINTSTS_BIT_WKUPINT);
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
*                                          STM32_RxFIFO_Rd()
*
* Description : Handle Rx FIFO non-empty interrupt generated when a data OUT packet has been received.
*
* Arguments   : p_drv       Pointer to device driver structure.
*
* Return(s)   : none.
*
* Note(s)     : (1) In the case where there is no user buffer to transfer the data to, discard the data
*                   in the FIFO and avoid being a packet behind on subsequent transfers.
*********************************************************************************************************
*/

static  void  STM32_RxFIFO_Rd (USBD_DRV  *p_drv)
{
    CPU_INT08U           ep_log_nbr;
    CPU_INT08U           ep_phy_nbr;
    CPU_INT08U           ep_type;
    CPU_INT08U           pkt_stat;
    CPU_INT08U           byte_rem;
    CPU_INT08U           word_cnt;
    CPU_INT16U           byte_cnt;
    CPU_INT32U          *p_data_buf;
    CPU_INT32U           reg_val;
    CPU_INT32U           i;
    CPU_INT32U           data;
    USBD_DRV_DATA_EP    *p_drv_data;
    USBD_STM32F_FS_REG  *p_reg;
    CPU_INT32U           ctl_reg;


    p_reg      = (USBD_STM32F_FS_REG *)p_drv->CfgPtr->BaseAddr;
    p_drv_data = (USBD_DRV_DATA_EP   *)p_drv->DataPtr;
    reg_val    =  p_reg->GRXSTSP;
    ep_log_nbr = (reg_val & STM32F_FS_GRXSTSx_EPNUM_MASK)  >>  0u;
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(USBD_EP_LOG_TO_ADDR_OUT(ep_log_nbr));
    byte_cnt   = (reg_val & STM32F_FS_GRXSTSx_BCNT_MASK)   >>  4u;
    pkt_stat   = (reg_val & STM32F_FS_GRXSTSx_PKTSTS_MASK) >> 17u;


    p_drv_data->EP_PktXferLen[ep_phy_nbr] += byte_cnt;

                                                                /* Disable Rx FIFO non-empty                            */
    DEF_BIT_CLR(p_reg->GINTMSK, STM32F_FS_GINTMSK_BIT_RXFLVLM);

    switch (pkt_stat) {
        case STM32F_FS_GRXSTSx_PKTSTS_SETUP_RX:
             p_data_buf = &p_drv_data->EP_SetupBuf[0u];
             if (byte_cnt == 8u) {                              /* Read Setup packet from Rx FIFO                        */
                 p_data_buf[0u] = *p_reg->DFIFO[ep_log_nbr].DATA;
                 p_data_buf[1u] = *p_reg->DFIFO[ep_log_nbr].DATA;
             }

             DEF_BIT_SET(p_reg->DOEP[0u].CTLx, STM32F_FS_DxEPCTLx_BIT_SNAK);
             break;


        case STM32F_FS_GRXSTSx_PKTSTS_OUT_RX:
             DEF_BIT_SET(p_reg->DOEP[ep_log_nbr].CTLx,          /* Put endpoint in NACK mode.                           */
                         STM32F_FS_DxEPCTLx_BIT_SNAK);

             if ((byte_cnt                             >   0u) &&
                 (p_drv_data->EP_AppBufPtr[ep_phy_nbr] != (CPU_INT08U *)0)) {

                 byte_cnt =  DEF_MIN(byte_cnt, p_drv_data->EP_AppBufLen[ep_phy_nbr]);
                 word_cnt = (byte_cnt / 4u);

                                                                /* Check app buffer alignement.                         */
                 if ((((CPU_INT32U)p_drv_data->EP_AppBufPtr[ep_phy_nbr]) % 4u) == 0u) {
                     p_data_buf = (CPU_INT32U *)p_drv_data->EP_AppBufPtr[ep_phy_nbr];
                 } else {
                     p_data_buf = &p_drv_data->DataBuf[0u];     /* Use drv internal aligned buf if app buf not aligned. */
                 }

                                                                /* Read OUT packet from Rx FIFO                         */
                 for (i = 0u; i < word_cnt; i++) {
                     *p_data_buf = *p_reg->DFIFO[ep_log_nbr].DATA;
                      p_data_buf++;
                 }

                 byte_rem = (byte_cnt - (word_cnt * 4u));
                 if (byte_rem > 0u) {                           /* Rd remaining data if byte_cnt not multiple of 4.     */
                     data = *p_reg->DFIFO[ep_log_nbr].DATA;

                     for (i = 0u; i < byte_rem; i++) {
                         ((CPU_INT08U *)p_data_buf)[i] = (CPU_INT08U)((data >> (8u * i)) & 0xFFu);
                     }
                 }

                                                                /* Copy data to app buf if not aligned.                 */
                 if ((((CPU_INT32U)p_drv_data->EP_AppBufPtr[ep_phy_nbr]) % 4u) != 0u) {
                     p_data_buf = &p_drv_data->DataBuf[0u];
                     for (i = 0u; i < byte_cnt; i++) {
                         p_drv_data->EP_AppBufPtr[ep_phy_nbr][i] = ((CPU_INT08U *)p_data_buf)[i];
                     }
                 }

                 if (p_drv_data->EP_AppBufBlk[ep_phy_nbr]) {    /* Multi-packet transfer.                               */
                     p_drv_data->EP_AppBufBlk[ep_phy_nbr]--;
                     p_drv_data->EP_AppBufPtr[ep_phy_nbr] += byte_cnt;
                                                                /* If more packets are expected, continue listening.    */
                     if (p_drv_data->EP_AppBufBlk[ep_phy_nbr] != 0) {
                       ctl_reg  = p_reg->DOEP[ep_log_nbr].CTLx;

                       ep_type = USBD_GET_EP_TYPE(ctl_reg);
                       if (ep_type == USBD_EP_TYPE_ISOC) {
                           if ((p_reg->DSTS & (1U << 8)) == 0U) {
                             ctl_reg |= STM32F_FS_DxEPCTLx_BIT_SODDFRM;

                           } else {
                             ctl_reg |= STM32F_FS_DxEPCTLx_BIT_SEVNFRM;
                           }
                       }

                                                                /* Clear EP NAK and enable EP for receiving             */
                       ctl_reg |= STM32F_FS_DxEPCTLx_BIT_CNAK | STM32F_FS_DxEPCTLx_BIT_EPENA;

                       p_reg->DOEP[ep_log_nbr].CTLx          = ctl_reg;

                                                                /* Clear output interrupt since more data is expected   */
                       DEF_BIT_CLR(p_reg->GINTMSK, STM32F_FS_GINTMSK_BIT_OEPINT);
                     }
                 }
                 if (p_drv_data->EP_AppBufBlk[ep_phy_nbr] == 0) {
                     p_drv_data->EP_AppBufPtr[ep_phy_nbr] = (CPU_INT08U *)0;
                 }
             } else {                                           /* See Note (1).                                        */
                 word_cnt = (byte_cnt + 3u) / 4u;
                 for (i = 0u; i < word_cnt; i++) {
                     data = *p_reg->DFIFO[ep_log_nbr].DATA;
                 }
             }
             break;


        case STM32F_FS_GRXSTSx_PKTSTS_OUT_COMPL:
        case STM32F_FS_GRXSTSx_PKTSTS_SETUP_COMPL:
        case STM32F_FS_GRXSTSx_PKTSTS_OUT_NAK:
        default:
             break;
    }
                                                                /* Enable Rx FIFO non-empty interrupt                   */
    DEF_BIT_SET(p_reg->GINTMSK, STM32F_FS_GINTMSK_BIT_RXFLVLM);

}

/*
*********************************************************************************************************
*                                          STM32_TxFIFO_Wr()
*
* Description : Writes a packet into the Tx FIFO associated with the Endpoint.
*
* Arguments   : p_drv       Pointer to device driver structure.
*
*               ep_log_nbr  Endpoint logical number.
*
*               p_buf       Pointer to buffer of data that will be transmitted.
*
*               ep_pkt_len  Number of octets to transmit.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  STM32_TxFIFO_Wr (USBD_DRV    *p_drv,
                               CPU_INT08U   ep_log_nbr,
                               CPU_INT08U  *p_buf,
                               CPU_INT16U   ep_pkt_len)
{
    CPU_INT32U           nbr_words;
    CPU_INT32U           i;
    CPU_INT32U           words_avail;
    CPU_INT32U          *p_data_buf;
    USBD_STM32F_FS_REG  *p_reg;
    USBD_DRV_DATA_EP    *p_drv_data;
    CPU_SR_ALLOC();


    p_drv_data = (USBD_DRV_DATA_EP   *)p_drv->DataPtr;
    p_reg      = (USBD_STM32F_FS_REG *)p_drv->CfgPtr->BaseAddr;
    nbr_words  = (ep_pkt_len + 3u) / 4u;

    do {
                                                                /* Read available words in the EP Tx FIFO               */
        words_avail = p_reg->DIEP[ep_log_nbr].DTXFSTSx & 0x0000FFFFu;
    } while (words_avail < nbr_words);                          /* Check if there are enough words to write into FIFO   */

    CPU_CRITICAL_ENTER();
    if (((CPU_INT32U)p_buf % 4u) == 0u) {                       /* Check app buffer alignement.                         */
        p_data_buf = (CPU_INT32U *)p_buf;
    } else {
        p_data_buf = &p_drv_data->DataBuf[0u];                  /* Use drv internal aligned buf if app buf not aligned. */

        for (i = 0u; i < ep_pkt_len; i++) {
            ((CPU_INT08U *)p_data_buf)[i] = p_buf[i];
        }
    }

    for (i = 0u; i < nbr_words; i++) {
       *p_reg->DFIFO[ep_log_nbr].DATA = *p_data_buf;
        p_data_buf++;
    }
    CPU_CRITICAL_EXIT();
}

/*
*********************************************************************************************************
*                                        STM32_EP_OutProcess()
*
* Description : Process OUT interrupts on associated EP.
*
* Arguments   : p_drv       Pointer to device driver structure.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  STM32_EP_OutProcess (USBD_DRV  *p_drv)
{

    CPU_INT32U           ep_int_stat;
    CPU_INT32U           dev_ep_int;
    CPU_INT32U           ep_log_nbr;
    CPU_INT08U           ep_phy_nbr;
    USBD_DRV_DATA_EP    *p_drv_data;
    USBD_STM32F_FS_REG  *p_reg;
    CPU_BOOLEAN          ep_rx_cmpl;


    p_reg      = (USBD_STM32F_FS_REG *)p_drv->CfgPtr->BaseAddr;
    p_drv_data = (USBD_DRV_DATA_EP   *)p_drv->DataPtr;
    dev_ep_int =  p_reg->DAINT >> 16u;                          /* Read all Device OUT Endpoint interrupt               */

    while (dev_ep_int != DEF_BIT_NONE) {
        ep_log_nbr  = (CPU_INT08U)(31u - CPU_CntLeadZeros32(dev_ep_int & 0x0000FFFFu));
        ep_phy_nbr  =  USBD_EP_ADDR_TO_PHY(USBD_EP_LOG_TO_ADDR_OUT(ep_log_nbr));
        ep_int_stat =  p_reg->DOEP[ep_log_nbr].INTx;            /* Read OUT EP interrupt status                         */
        ep_rx_cmpl  = p_drv_data->EP_AppBufBlk[ep_phy_nbr] == 0 ? DEF_TRUE : DEF_FALSE;

                                                                /* Check if EP transfer completed occurred              */
        if (DEF_BIT_IS_SET(ep_int_stat, STM32F_FS_DOEPINTx_BIT_XFRC)) {
            if (ep_rx_cmpl) {                                   /* Check if EP has more data to receive                 */
                USBD_EP_RxCmpl(p_drv, ep_log_nbr);
            }
                                                                /* Clear EP transfer complete interrupt                 */
            DEF_BIT_SET(p_reg->DOEP[ep_log_nbr].INTx, STM32F_FS_DOEPINTx_BIT_XFRC);
        }
                                                                /* Check if EP Setup phase done interrupt occurred      */
        if (DEF_BIT_IS_SET(ep_int_stat, STM32F_FS_DOEPINTx_BIT_STUP)) {
            USBD_EventSetup(p_drv, (void *)&p_drv_data->EP_SetupBuf[0u]);
                                                                /* Clear EP0 SETUP complete interrupt                   */
            DEF_BIT_SET(p_reg->DOEP[ep_log_nbr].INTx, STM32F_FS_DOEPINTx_BIT_STUP);

                                                                /* Re-enable EP for next setup pkt.                     */
            DEF_BIT_SET(p_reg->DOEP[0u].CTLx, STM32F_FS_DxEPCTLx_BIT_EPENA);
        }

        if (ep_rx_cmpl) {                                       /* Only set EP in NAK mode if no more data is expected  */
            DEF_BIT_SET(p_reg->DOEP[ep_log_nbr].CTLx, STM32F_FS_DxEPCTLx_BIT_SNAK);
        }

        dev_ep_int = p_reg->DAINT >> 16u;                       /* Read all Device OUT Endpoint interrupt               */
    }
}

/*
*********************************************************************************************************
*                                        STM32_EP_InProcess()
*
* Description : Process IN interrupts on associated EP.
*
* Arguments   : p_drv       Pointer to device driver structure.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  STM32_EP_InProcess (USBD_DRV  *p_drv)
{
    CPU_INT08U           ep_log_nbr;
    CPU_INT32U           ep_int_stat;
    CPU_INT32U           dev_ep_int;
    USBD_STM32F_FS_REG  *p_reg;


    p_reg      = (USBD_STM32F_FS_REG *)p_drv->CfgPtr->BaseAddr;
    dev_ep_int = (p_reg->DAINT & 0x0000FFFFu);                   /* Read all Device IN Endpoint interrupt                */

    while (dev_ep_int != DEF_BIT_NONE) {
        ep_log_nbr  = (CPU_INT08U)(31u - CPU_CntLeadZeros32(dev_ep_int & 0x0000FFFFu));
        ep_int_stat =  p_reg->DIEP[ep_log_nbr].INTx;             /* Read IN EP interrupt status                          */

                                                                /* Check if EP transfer completed interrupt occurred    */
        if (DEF_BIT_IS_SET(ep_int_stat, STM32F_FS_DIEPINTx_BIT_XFRC)) {
            USBD_EP_TxCmpl(p_drv, ep_log_nbr);
                                                                /* Clear EP transfer complete interrupt                 */
            DEF_BIT_SET(p_reg->DIEP[ep_log_nbr].INTx, STM32F_FS_DIEPINTx_BIT_XFRC);
        }

        dev_ep_int = (p_reg->DAINT & 0x0000FFFFu);             /* Read all Device IN Endpoint interrupt                */
    }
}
