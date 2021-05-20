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
*                                         USB DEVICE DRIVER
*                              SYNOPSYS (CHIPIDEA) CORE USB 2.0 OTG (HS)
*
* Filename : usbd_drv_synopsys_otg_hs.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)  : (1) You can find specific information about this driver at:
*                https://doc.micrium.com/display/USBDDRV/Synopsys_OTG_HS
*
*            (2) With an appropriate BSP, this device driver will support the Full-speed and
*                High-speed Device Interface module on the following MCUs:
*
*                    NXP LPC313x series
*                    NXP LPC185x series
*                    NXP LPC183x series
*                    NXP LPC182x series
*                    NXP LPC435x series
*                    NXP LPC433x series
*                    NXP LPC432x series
*                    Xilinx Zynq-7000
*
*            (3) This driver has not been tested with LPC18xx/LPC43xx USB1 controller in High-Speed
*                mode using an external ULPI PHY.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#include  <lib_mem.h>
#include  "../../Source/usbd_core.h"
#include  "usbd_drv_synopsys_otg_hs.h"
#include  <cpu_cache.h>


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*
* Note(s) : (1) The device controller has the following endpoint (EP) characteristics on these platforms:
*
*               Manufacturer | MCU or Soc   | Nbr of logical EPs | Nbr of physical EPs
*               -------------|--------------|--------------------|--------------------
*               NXP          | LPC313x      |       4            |         8
*               NXP          | LPC18xx      |       6            |        12
*               NXP          | LPC43xx      |       6            |        12
*               xilinx       | Zinq-7000    |      12            |        24
*
*               Each logical endpoint is bidirectional : direction IN and OUT.
*********************************************************************************************************
*/

                                                                /* ------------- USB CONTROLLER CONSTRAINS ------------ */
#define  USBD_OTGHS_REG_TO                           0x0000FFFFu
#define  USBD_OTGHS_MAX_RETRIES                             100u
#define  USBD_OTGHS_dTD_LST_INSERT_NBR_TRIES_MAX            100u
#define  USBD_OTGHS_dTD_NBR_PAGES                             5u
#define  USBD_OTGHS_EP_LOG_NBR_MAX                           12u/* Max of log EPs among all platforms supported by...   */
                                                                /* ...this drv (see Note #1).                           */
#define  USBD_OTGHS_EP_PHY_NBR_MAX                  (USBD_OTGHS_EP_LOG_NBR_MAX * 2u)

#define  USBD_OTGHS_dTD_EXT_ATTRIB_IS_COMPLETED     DEF_BIT_00

#define  USBD_OTGHS_ALIGN_OCTECTS_dQH               ( 2u * (1024u))
#define  USBD_OTGHS_ALIGN_OCTECTS_dTD               (64u * (   1u))
#define  USBD_OTGHS_ALIGN_OCTECTS_BUF               ( 1u * (   1u))

#define  USBD_OTGHS_MAX_NBR_EP_OPEN                 DEF_MIN(USBD_CFG_MAX_NBR_EP_OPEN, USBD_OTGHS_EP_PHY_NBR_MAX)
#define  USBD_OTGHS_dTD_NBR                        (USBD_CFG_MAX_NBR_URB_EXTRA + \
                                                    USBD_OTGHS_MAX_NBR_EP_OPEN )

                                                                /* ---------- USB DEVICE REGISTER BIT DEFINES --------- */
#define  USBD_OTGHS_DEV_ADDR_USBADDRA               DEF_BIT_24  /* Device Address Advance                               */
                                                                /* Device Address Mask                                  */
#define  USBD_OTGHS_DEV_ADDR_USBADDR_MASK           DEF_BIT_FIELD_32(7u, 25u)

#define  USBD_OTGHS_PORTSC1_HSP                     DEF_BIT_09  /* High-Speed port                                      */
#define  USBD_OTGHS_PORTSC1_FPR                     DEF_BIT_06  /* Force port resume.                                   */
#define  USBD_OTGHS_PORTSC1_PFSC                    DEF_BIT_24  /* Full-Speed port                                      */

                                                                /* ------ USB INTERRUPT AND STATUS REGISTER BITS ------ */
#define  USBD_OTGHS_USBSTS_NAKI                     DEF_BIT_16  /* NAK Interrupt Bit.                                   */
#define  USBD_OTGHS_USBSTS_AS                       DEF_BIT_15  /* Asynchronous Schedule Status.                        */
#define  USBD_OTGHS_USBSTS_PS                       DEF_BIT_14  /* Periodic Schedule Status.                            */
#define  USBD_OTGHS_USBSTS_RCL                      DEF_BIT_13  /* Reclamation.                                         */
#define  USBD_OTGHS_USBSTS_HCH                      DEF_BIT_12  /* HCHaIted.                                            */
#define  USBD_OTGHS_USBSTS_ULPI                     DEF_BIT_10  /* ULPI Interrupt.                                      */
#define  USBD_OTGHS_USBSTS_SLI                      DEF_BIT_08  /* DCSuspend.                                           */
#define  USBD_OTGHS_USBSTS_SRI                      DEF_BIT_07  /* SOF Received.                                        */
#define  USBD_OTGHS_USBSTS_URI                      DEF_BIT_06  /* USB Reset Received.                                  */
#define  USBD_OTGHS_USBSTS_AAI                      DEF_BIT_05  /* Interrupt on Async Advance.                          */
#define  USBD_OTGHS_USBSTS_SEI                      DEF_BIT_04  /* System Error.                                        */
#define  USBD_OTGHS_USBSTS_FRI                      DEF_BIT_03  /* Frame List Rollover.                                 */
#define  USBD_OTGHS_USBSTS_PCI                      DEF_BIT_02  /* Port Change Detect.                                  */
#define  USBD_OTGHS_USBSTS_UEI                      DEF_BIT_01  /* USB Error Interrupt.                                 */
#define  USBD_OTGHS_USBSTS_UI                       DEF_BIT_00  /* USB Interrupt.                                       */

                                                                /* ------ USB INTERRUPT REGISTER (USBINTR) BITS ------- */
#define  USBD_OTGHS_USBSTS_NAKE                     DEF_BIT_16  /* NAK Interrupt enable                                 */
#define  USBD_OTGHS_USBSTS_ULPIE                    DEF_BIT_10  /* ULPI enable                                          */
#define  USBD_OTGHS_USBSTS_SLE                      DEF_BIT_08  /* Sleep enable                                         */
#define  USBD_OTGHS_USBSTS_SRE                      DEF_BIT_07  /* SOF received enable                                  */
#define  USBD_OTGHS_USBSTS_URE                      DEF_BIT_06  /* USB reset enable                                     */
#define  USBD_OTGHS_USBSTS_AAE                      DEF_BIT_05  /* Interrupt on Async Advance enable                    */
#define  USBD_OTGHS_USBSTS_SEE                      DEF_BIT_04  /* System Error enable                                  */
#define  USBD_OTGHS_USBSTS_FRE                      DEF_BIT_03  /* Frame List Rollover enable                           */
#define  USBD_OTGHS_USBSTS_PCE                      DEF_BIT_02  /* Port Change Detect enable                            */
#define  USBD_OTGHS_USBSTS_UEE                      DEF_BIT_01  /* USB Error Interrupt enable                           */
#define  USBD_OTGHS_USBSTS_UE                       DEF_BIT_00  /* USB Interrupt enable                                 */

                                                                /* ------------ USB INTERRUPT ENABLE BITS ------------- */
#define  USBD_OTGHS_USB_INT_NAK                     DEF_BIT_16  /* NAK Interrupt enable                                 */
#define  USBD_OTGHS_USB_INT_ULP                     DEF_BIT_10  /* ULPI enable                                          */
#define  USBD_OTGHS_USB_INT_SL                      DEF_BIT_08  /* Sleep enable                                         */
#define  USBD_OTGHS_USB_INT_SR                      DEF_BIT_07  /* SOF received enable                                  */
#define  USBD_OTGHS_USB_INT_UR                      DEF_BIT_06  /* USB reset enable                                     */
#define  USBD_OTGHS_USB_INT_AA                      DEF_BIT_05  /* Interrupt on Async Advance enable                    */
#define  USBD_OTGHS_USB_INT_SE                      DEF_BIT_04  /* System Error enable                                  */
#define  USBD_OTGHS_USB_INT_FR                      DEF_BIT_03  /* Frame List Rollover enable                           */
#define  USBD_OTGHS_USB_INT_PC                      DEF_BIT_02  /* Port Change Detect enable                            */
#define  USBD_OTGHS_USB_INT_UE                      DEF_BIT_01  /* USB Error Interrupt enable                           */
#define  USBD_OTGHS_USB_INT_U                       DEF_BIT_00  /* USB Interrupt enable                                 */

#define  USBD_OTGHS_USB_INT_BUS                    (USBD_OTGHS_USB_INT_PC | \
                                                    USBD_OTGHS_USB_INT_UR | \
                                                    USBD_OTGHS_USB_INT_SL)


                                                                /* --------- USB COMMAND REGISTER BIT DEFINES --------- */
                                                                /* Interrupt Threshold Control                          */
#define  USBD_OTGHS_USBCMD_ITC_MASK                 DEF_BIT_FIELD_32(8u, 16u)
#define  USBD_OTGHS_USBCMD_ITC_MICRO_FRAME_1        DEF_BIT_MASK(0x01u,  16u)
#define  USBD_OTGHS_USBCMD_ITC_MICRO_FRAME_2        DEF_BIT_MASK(0x02u,  16u)
#define  USBD_OTGHS_USBCMD_ITC_MICRO_FRAME_3        DEF_BIT_MASK(0x04u,  16u)
#define  USBD_OTGHS_USBCMD_ITC_MICRO_FRAME_8        DEF_BIT_MASK(0x08u,  16u)
#define  USBD_OTGHS_USBCMD_ITC_MICRO_FRAME_16       DEF_BIT_MASK(0x10u,  16u)
#define  USBD_OTGHS_USBCMD_ITC_MICRO_FRAME_32       DEF_BIT_MASK(0x20u,  16u)
#define  USBD_OTGHS_USBCMD_ITC_MICRO_FRAME_40       DEF_BIT_MASK(0x40u,  16u)

                                                                /* Frame List size                                      */
#define  USBD_OTGHS_USBCMD_FS_MASK                  DEF_BIT_FIELD(2u, 14u)
#define  USBD_OTGHS_USBCMD_ATDTW                    DEF_BIT_14  /* Add dTD TripWire                                     */

#define  USBD_OTGHS_USBCMD_SUTW                     DEF_BIT_13  /* Setup Tripwire                                       */
#define  USBD_OTGHS_USBCMD_ASPE                     DEF_BIT_11  /* Asynchronous schedule park mode enable               */
                                                                /* Asynchronous schedule park mode count                */
#define  USBD_OTGHS_USBCMD_ASP_MASK                 DEF_BIT_FIELD(2u, 8u)
#define  USBD_OTGHS_USBCMD_LR                       DEF_BIT_07  /* Light Host/Device controller reset                   */
#define  USBD_OTGHS_USBCMD_IAA                      DEF_BIT_06  /* Interrupt on Async Advance Doorbell                  */
#define  USBD_OTGHS_USBCMD_ASE                      DEF_BIT_05  /* Asynchronous schedule enable                         */
#define  USBD_OTGHS_USBCMD_PSE                      DEF_BIT_04  /* Periodic schedule enable                             */

                                                                /* Frame List Size mask                                 */
#define  USBD_OTGHS_USBCMD_FS_SIZE_MASK             DEF_BIT_FIELD(3u, 2u)
#define  USBD_OTGHS_USBCMD_RST                      DEF_BIT_01  /* Controller Reset                                     */
#define  USBD_OTGHS_USBCMD_RUN                      DEF_BIT_00  /* Run Stop                                             */

                                                                /* ------ USB DEVICE ADDRESS REGISTER BIT DEFINES ----- */
                                                                /* Device Address mask                                  */
#define  USBD_OTGHS_DEV_ADDR_USBADR_MASK            DEF_BIT_FIELD(6u, 25u)
#define  USBD_OTGHS_DEV_ADDR_USBADRA                DEF_BIT_24  /* Device Address Advance.                              */

                                                                /* ---- ENDPOINT LIST ADDRESS REGISTER BIT DEFINES ---- */
#define  USBD_OTGHS_EP_LIST_ADDR_MASK               DEF_BIT_FIELD(21u, 11u)

                                                                /* ----------- USB MODE REGISTER BIT DEFINES ---------- */
#define  USBD_OTGHS_USBMODE_SDIS                    DEF_BIT_04  /* Stream Disable Mode                                  */
#define  USBD_OTGHS_USBMODE_SLOM                    DEF_BIT_03  /* Setup lockout mode                                   */
#define  USBD_OTGHS_USBMODE_ES                      DEF_BIT_02  /* Endianness selection                                 */
                                                                /* Controller Mode mask                                 */

#define  USBD_OTGHS_USBMODE_CM_MASK                 DEF_BIT_FIELD(2u, 0u)
                                                                /* Idle   mode                                          */
#define  USBD_OTGHS_USBMODE_CM_IDLE                 DEF_BIT_NONE
#define  USBD_OTGHS_USBMODE_CM_DEV                  DEF_BIT_01  /* Device mode                                          */
                                                                /* Host   mode                                          */
#define  USBD_OTGHS_USBMODE_CM_HOST                (DEF_BIT_01 | DEF_BIT_02)

                                                                /* ------- ENDPOINT CONTROL REGISTER BIT DEFINES ------ */
#define  USBD_OTGHS_ENDPTCTRL_TX_CFG_MASK           DEF_BIT_FIELD_32(8u, 16u)
#define  USBD_OTGHS_ENDPTCTRL_TX_EN                 DEF_BIT_23  /* Tx endpoint enable                                   */
#define  USBD_OTGHS_ENDPTCTRL_TX_TOGGLE_RST         DEF_BIT_22  /* Data Toggle reset                                    */
#define  USBD_OTGHS_ENDPTCTRL_TX_TOGGLE_DIS         DEF_BIT_21  /* Data Toggle inhibit                                  */
                                                                /* Tx endpoint type mask                                */
#define  USBD_OTGHS_ENDPTCTRL_TX_TYPE_MASK          DEF_BIT_FIELD_32(2u, 18u)
                                                                /* Tx endpoint type control                             */
#define  USBD_OTGHS_ENDPTCTRL_TX_TYPE_CTRL          DEF_BIT_NONE
#define  USBD_OTGHS_ENDPTCTRL_TX_TYPE_ISOC          DEF_BIT_18  /* Tx endpoint type isochronous                         */
#define  USBD_OTGHS_ENDPTCTRL_TX_TYPE_BULK          DEF_BIT_19  /* Tx endpoint type bulk                                */
                                                                /* Tx endpoint type interrupt                           */
#define  USBD_OTGHS_ENDPTCTRL_TX_TYPE_INT          (DEF_BIT_18 | DEF_BIT_19)
#define  USBD_OTGHS_ENDPTCTRL_TX_DATA_SRC           DEF_BIT_17  /* Tx endpoint data source                              */
#define  USBD_OTGHS_ENDPTCTRL_TX_STALL              DEF_BIT_16  /* Tx endpoint stall                                    */

#define  USBD_OTGHS_ENDPTCTRL_RX_CFG_MASK           DEF_BIT_FIELD_32(8u, 0u)
#define  USBD_OTGHS_ENDPTCTRL_RX_EN                 DEF_BIT_07  /* Rx endpoint enable                                   */
#define  USBD_OTGHS_ENDPTCTRL_RX_TOGGLE_RST         DEF_BIT_06  /* Rx Data Toggle reset                                 */
#define  USBD_OTGHS_ENDPTCTRL_RX_TOGGLE_DIS         DEF_BIT_05  /* Rx Data Toggle inhibit                               */
                                                                /* Rx endpoint type mask                                */
#define  USBD_OTGHS_ENDPTCTRL_RX_TYPE_MASK          DEF_BIT_FIELD_32(2u, 2u)
                                                                /* Rx endpoint type control                             */
#define  USBD_OTGHS_ENDPTCTRL_RX_TYPE_CTRL          DEF_BIT_NONE
#define  USBD_OTGHS_ENDPTCTRL_RX_TYPE_ISOC          DEF_BIT_02  /* Rx endpoint type isochronous                         */
#define  USBD_OTGHS_ENDPTCTRL_RX_TYPE_BULK          DEF_BIT_03  /* Rx endpoint type bulk                                */
                                                                /* Rx endpoint type interrupt                           */
#define  USBD_OTGHS_ENDPTCTRL_RX_TYPE_INT          (DEF_BIT_02 | DEF_BIT_03)
#define  USBD_OTGHS_ENDPTCTRL_RX_DATA_SRC           DEF_BIT_01  /* Rx endpoint data source                              */
#define  USBD_OTGHS_ENDPTCTRL_RX_STALL              DEF_BIT_00  /* Rx endpoint stall                                    */

#define  USBD_OTGHS_ENDPTCOMPLETE_TX_MASK           DEF_BIT_FIELD_32(USBD_OTGHS_EP_LOG_NBR_MAX, 16u)
#define  USBD_OTGHS_ENDPTCOMPLETE_RX_MASK           DEF_BIT_FIELD_32(USBD_OTGHS_EP_LOG_NBR_MAX,  0u)

#define  USBD_OTGHS_ENDPTxxxx_TX_MASK               DEF_BIT_FIELD_32(USBD_OTGHS_EP_LOG_NBR_MAX, 16u)
#define  USBD_OTGHS_ENDPTxxxx_RX_MASK               DEF_BIT_FIELD_32(USBD_OTGHS_EP_LOG_NBR_MAX,  0u)

                                                                /* -- ENDPOINT QUEUE HEAD EP CAPABILITIES BIT DEFINES - */
                                                                /* Number of packets executed per transaction.          */
#define  USBD_OTGHS_dQH_EP_CAP_MULT_MASK            DEF_BIT_FIELD_32(2u, 30u)
#define  USBD_OTGHS_dQH_EP_CAP_MULT_N               DEF_BIT_MASK(0u, 30u)
#define  USBD_OTGHS_dQH_EP_CAP_MULT_1               DEF_BIT_MASK(1u, 30u)
#define  USBD_OTGHS_dQH_EP_CAP_MULT_2               DEF_BIT_MASK(2u, 30u)
#define  USBD_OTGHS_dQH_EP_CAP_MULT_3               DEF_BIT_MASK(3u, 30u)

#define  USBD_OTGHS_dQH_EP_CAP_ZLTS                 DEF_BIT_29  /* Zero Length Termination Select                       */
                                                                /* EP maximum length mask                               */
#define  USBD_OTGHS_dQH_EP_CAP_MAX_LEN_MASK         DEF_BIT_FIELD_32(11u, 16u)
#define  USBD_OTGHS_dQH_EP_CAP_IOS                  DEF_BIT_15  /* Interrupt on setup                                   */

                                                                /* ------------ dTD_NEXT FIELD  BIT DEFINES ----------- */
                                                                /* Next transfer element pointer mask                   */
#define  USBD_OTGHS_dTD_dTD_NEXT_MASK               DEF_BIT_FIELD_32(27u, 5u)
#define  USBD_OTGHS_dTD_dTD_NEXT_TERMINATE          DEF_BIT_00  /* End of transfer list indicator                       */

                                                                /* -------------- TOKEN FILED BIT DEFINES ------------- */
#define  USBD_OTGHS_dTD_TOKEN_TOTAL_BYTES_MASK      DEF_BIT_FIELD_32(15u, 16u)
#define  USBD_OTGHS_dTD_TOKEN_IOC                   DEF_BIT_15  /* Interrupt on complete                                */
                                                                /* Multiplier override mask                             */
#define  USBD_OTGHS_dTD_TOKEN_MUL_OVER_MASK         DEF_BIT_FIELD_32(2u, 10u)

#define  USBD_OTGHS_dTD_TOKEN_TOTAL_BYTE_MAX        0x00004000u /* Maximum number of bytes                              */

#define  USBD_OTGHS_dTD_TOKEN_PAGE_SIZE             0x00001000u /* Page size 4K                                         */
                                                                /* Status mask                                          */
#define  USBD_OTGHS_dTD_TOKEN_STATUS_MASK           DEF_BIT_FIELD_32(8u, 0u)
#define  USBD_OTGHS_dTD_TOKEN_STATUS_ACTIVE         DEF_BIT_07
#define  USBD_OTGHS_dTD_TOKEN_STATUS_HALTED         DEF_BIT_06
#define  USBD_OTGHS_dTD_TOKEN_STATUS_DATA_ERR       DEF_BIT_05
#define  USBD_OTGHS_dTD_TOKEN_STATUS_TRAN_ERR       DEF_BIT_03

#define  USBD_OTGHS_dTD_TOKEN_STATUS_ANY           (USBD_OTGHS_dTD_TOKEN_STATUS_ACTIVE   | \
                                                    USBD_OTGHS_dTD_TOKEN_STATUS_HALTED   | \
                                                    USBD_OTGHS_dTD_TOKEN_STATUS_DATA_ERR | \
                                                    USBD_OTGHS_dTD_TOKEN_STATUS_TRAN_ERR)

#define  USBD_OTGHS_dTD_BUF_PTR_MASK                DEF_BIT_FIELD(20u, 12u)


                                                                /* ----------- IP_INFO REGISTER BIT DEFINES ----------- */
#define  USBD_OTGHS_IP_INFO_IP_MASK                 DEF_BIT_FIELD_32(16u, 16u)
#define  USBD_OTGHS_IP_INFO_MAJOR_MASK              DEF_BIT_FIELD_32(4u,  12u)
#define  USBD_OTGHS_IP_INFO_MINOR_MASK              DEF_BIT_FIELD_32(4u,   8u)

#define  USBD_LPC313X_IP_VER_2_0                             20u
#define  USBD_LPC313X_IP_VER_2_1                             21u
#define  USBD_LPC313X_IP_VER_2_2                             22u
#define  USBD_LPC313X_IP_VER_2_3                             23u
#define  USBD_LPC313X_IP_VER_2_4                             24u
#define  USBD_LPC313X_IP_VER_2_5                             24u
#define  USBD_LPC313X_IP_VER_2_6                             26u


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

#define  USBD_OTGHS_ENDPTxxx_GET_TX_BITS(ep_log_nbr)            (DEF_BIT32(ep_log_nbr) << 16u)
#define  USBD_OTGHS_ENDPTxxx_GET_RX_BITS(ep_log_nbr)             DEF_BIT32(ep_log_nbr)

#define  USBD_OTGHS_ENDPTxxx_GET_TX_NBR(ep_reg)                 (((ep_reg) & USBD_OTGHS_ENDPTxxxx_TX_MASK) >> 16u)
#define  USBD_OTGHS_ENDPTxxx_GET_RX_NBR(ep_reg)                  ((ep_reg) & USBD_OTGHS_ENDPTxxxx_RX_MASK)

#define  USBD_OTGHS_EP_PHY_NBR_IS_OUT(ep_phy_nbr)                DEF_BIT_IS_CLR((ep_phy_nbr), DEF_BIT_00)

#define  OTGHS_DBG_STATS_EN                                      DEF_ENABLED

#if (OTGHS_DBG_STATS_EN == DEF_ENABLED)
#define  OTGHS_DBG_STATS_RESET()                                {                                                      \
                                                                    Mem_Clr((void     *)&OTGHS_DbgStats,               \
                                                                            (CPU_SIZE_T) sizeof(OTGHS_DBG_STATS_DRV)); \
                                                                }

#define  OTGHS_DBG_STATS_INC(stat)                              {                                                      \
                                                                    OTGHS_DbgStats.stat++;                             \
                                                                }

#define  OTGHS_DBG_STATS_INC_IF_TRUE(stat, bool)                {                                                     \
                                                                    if (bool == DEF_TRUE) {                           \
                                                                        OTGHS_DBG_STATS_INC(stat);                    \
                                                                    }                                                 \
                                                                }
#else
#define  OTGHS_DBG_STATS_RESET()
#define  OTGHS_DBG_STATS_INC(stat)
#define  OTGHS_DBG_STATS_INC_IF_TRUE(stat, bool)
#endif


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*
* Note(s) : (1) The registers map structure is a merged of NXP LPC313x, LPC18xx, LPC43xx and Xilinx
*               Zynq-7000 USB device controllers' registers interfaces.
*               The registers in the table below are the registers interfaces differences between the
*               different platforms, that is the register listed belongs only to one of the platforms.
*               All other registers are common to all platforms.
*
*               Offset  | NXP LPC313x/LPC18xx/LPC43xx | Xilinx Zynq-7000
*
*               0x000                                   ID
*               0x004                                   HWGENERAL
*               0x008                                   HWHOST
*               0x00C                                   HWDEVICE
*               0x010                                   HWTXBUF
*               0x014                                   HWRXBUF
*               ...
*               0x020     IP_INFO
*               ...
*               0x080                                   GPTIMER0LD
*               0x084                                   GPTIMER0CTRL
*               0x088                                   GPTIMER1LD
*               0x08C                                   GPTIMER1CTRL
*               0x090                                   SBUSCFG
*               ...
*               0x16C                                   IC_USB
*               0x170                                   ULPI_VIEWPORT
*
*
*           (1) The endpoint's device Queue Head (dQH) is where all transfers are managed. The dQH is a 48 byte
*               data structure, but must be aligned on 64-byte boundaries. The remaining 16 bytes will be used
*               to store the device Transfer Descriptor (dTD) link list information.
*
*                                          0            1                 N
*        --     +-----------+  Current  +-----+      +-----+           +-----+
*         |     |           |---------->| dTD |----->| dTD |--- .... ->| dTD |----|
*      48 Bytes |   dQH     |           +-----+      +-----+           +-----+
*         |     |           |      Next    |             |                |
*         |     |           |--------------|-------------|                |
*        --     +-----------+              |                              |
*         |     |  Head     |--------------|                              |
*         |     +-----------+                                             |
*         |     |  Tail     |---------------------------------------------|
*      16 Bytes +-----------+
*         |     | #Entries  |-----> Number of elements of the dTD's link list
*         |     |-----------|
*         |     | #dTD Rdy  |-----> Number of completed dTDs
*        --     ------------+
*
*
*           (2) The size of the dTD is 28 bytes. The dTD must be be aligned to 8-DWord boundaries.
*               the remaining 36 bytes are used to store extra information.
*
*        -    --   +---------------+      +------+               +-----+
*        |     |   |   dTD_Next    |----->| dTD  |----- ... ---->| dTD |
*        |     |   +---------------+      +------+               +-----+
*        |    dTD  |     Token     |
*        |     |   +---------------+         +---------+
*     dTD_EXT  |   | BufPtrs[0..4] |----| |->|xxxxxxxxx| 0       - BufPtr[0]   : Always points to the first byte in the data
*        |    --   +---------------+    | |  +---------+                         buffer that is available
*        |         |   BufAddr     |----|-|  |xxxxxxxxx| 1
*        -         +---------------+    |    +---------+
*                                       |    |xxxxxxxxx| 2        - BufAddr    : Always points to the beginning of the data buffer.
*                                       |    +---------+
*                                       |    |    .    |            +--------+
*                                       |    |    .    |            |xxxxxxxx| = Used block
*                                       |    |    .    |            +--------+
*                                       |    +---------+
*                                       |--->|         | n - 1      +--------+
*                                            +---------+            |        | = Free block
*                                            |         |  n         +--------+
*                                            +---------+
*
*********************************************************************************************************
*/

typedef  struct usbd_otgfs_usb_reg {                            /* See Note #1.                                         */
                                                                /* ------ IDENTIFICATION CONFIGURATION CONSTANTS ------ */
    CPU_REG32  ID;                                              /* R 0x000 Identification register                      */
    CPU_REG32  HWGENERAL;                                       /* R 0x004 General hardware parameters                  */
    CPU_REG32  HWHOST;                                          /* R 0x008 Host hardware parameters                     */
    CPU_REG32  HWDEVICE;                                        /* R 0x00C Device hardware parameters                   */
    CPU_REG32  HWTXBUF;                                         /* R 0x010 TX Buffer hardware parameters                */
    CPU_REG32  HWRXBUF;                                         /* R 0x014 RX Buffer hardware parameters                */
    CPU_REG08  RESERVED0[6u];
    CPU_REG32  IP_INFO;                                         /* R 0x020 IP number and version number                 */
    CPU_REG32  RESERVED1[23u];
                                                                /* ------------ GENERAL PURPOSE TIMERS ---------------  */
    CPU_REG32  GPTIMER0LD;                                      /* R/W 0x080 General-purpose timer 0 load value         */
    CPU_REG32  GPTIMER0CTRL;                                    /* R/W 0x084 General-purpose timer 0 control            */
    CPU_REG32  GPTIMER1LD;                                      /* R/W 0x088 General-purpose timer 1 load value         */
    CPU_REG32  GPTIMER1CTRL;                                    /* R/W 0x08C General-purpose timer 1 control            */
                                                                /* --------------- AXI INTERCONNECT ------------------- */
    CPU_REG32  SBUSCFG;                                         /* R/W 0x090 DMA Master AHB burst mode                  */
    CPU_REG32  RESERVED2[27u];
                                                                /* -------- CONTROLLER CAPABILITIES CONSTANTS --------- */
    CPU_REG32  CAPLENGTH;                                       /* R 0x100 Capability register length                   */
    CPU_REG32  HCSPARAMS;                                       /* R 0x104 Host controller structural parameters        */
    CPU_REG32  HCCPARAMS;                                       /* R 0x108 Host controller capability parameters        */
    CPU_REG32  RESERVED3[5u];
    CPU_REG32  DCIVERSION;                                      /* R 0x120 Device interface version number              */
    CPU_REG32  DCCPARAMS;                                       /* R 0x124 Device controller capability parameters      */
    CPU_REG32  RESERVED4[6u];
                                                                /* -------- INTERRUPTS AND ENDPOINT POINTERS ---------- */
    CPU_REG32  USBCMD;                                          /* R/W 0x140 USB command                                */
    CPU_REG32  USBSTS;                                          /* R/W 0x144 USB status                                 */
    CPU_REG32  USBINTR;                                         /* R/W 0x148 USB interrupt enable                       */
    CPU_REG32  FRINDEX;                                         /* R/W 0x14C USB frame index                            */
    CPU_REG32  RESERVED5;
    CPU_REG32  DEV_ADDR;                                        /* R/W 0x154 USB Device Address                         */
    CPU_REG32  EP_LST_ADDR;                                     /* R/W 0x158 Next asynchronous list addr/addr of  ...   */
                                                                /*           ... of endpoint list in memory             */
    CPU_REG32  TTCTRL;                                          /* R/W 0x15C Asynchronous buffer stat for embedded TT   */
                                                                /* ------------------ MISCELLANEOUS ------------------- */
    CPU_REG32  BURSTSIZE;                                       /* R/W 0x160 Programmable burst size                    */
    CPU_REG32  TXFILLTUNING;                                    /* R/W 0x164 Host transmit pre-buffer packet tuning     */
    CPU_REG32  TXTTFILLTUNING;                                  /* R/W 0x168 Host TT tx pre-buffer packet tuning        */
    CPU_REG32  IC_USB;                                          /* R/W 0x16C Low and fast speed control                 */
    CPU_REG32  ULPI_VIEWPORT;                                   /* R/W 0x170 ULPI viewport.                             */
    CPU_REG32  RESERVED6;                                       /* Reserved bits.                                       */
                                                                /* ---------------- ENDPOINT CONTROL ------------------ */
    CPU_REG32  ENDPTNAK;                                        /* R/W 0x178 Endpoint NAK                               */
    CPU_REG32  ENDPTNAKEN;                                      /* R/W 0x17C Endpoint NAK Enable                        */
    CPU_REG32  CONFIGFLAG;                                      /* R   0x180 Configured flag register                   */
    CPU_REG32  PORTSC1;                                         /* R/W 0x184 Port status/control 1                      */
    CPU_REG32  RESERVED7[7u];
                                                                /* ------------------ MODE CONTROL -------------------- */
    CPU_REG32  OTGSC;                                           /* R/W 0x1A4 OTG status and control                     */
    CPU_REG32  USBMODE;                                         /* R/W 0x1A8 USB device mode                            */
                                                                /* -------- ENDPOINT CONFIGURATION AND CONTROL -------- */
    CPU_REG32  ENDPTSETUPSTAT;                                  /* R/W 0x1AC Endpoint setup status                      */
    CPU_REG32  ENDPTPRIME;                                      /* R/W 0x1B0 Endpoint initialization                    */
    CPU_REG32  ENDPTFLUSH;                                      /* R/W 0x1B4 Endpoint de-initialization                 */
    CPU_REG32  ENDPTSTATUS;                                     /* R   0x1B8 Endpoint status                            */
    CPU_REG32  ENDPTCOMPLETE;                                   /* R/W 0x1BC Endpoint complete                          */
    CPU_REG32  ENDPTCTRLx[USBD_OTGHS_EP_LOG_NBR_MAX];           /*           Endpoint control registers                 */
}  USBD_OTGHS_REG;

                                                                /* --- ENDPOINT TRANSFER DESCRIPTOR (dTD) DATA TYPE --- */
typedef  struct  usbd_otgfs_dtd {
    CPU_REG32  dTD_NextPtr;                                     /* Next Link Pointer                                    */
    CPU_REG32  Token;                                           /* DTD token                                            */
    CPU_REG32  BufPtrs[5u];                                     /* Buffer pointer (Page n, n = [0..4])                  */
} USBD_OTGHS_dTD;

                                                                /* -------------- dTD EXTENDED DATA TYPE -------------- */
typedef  struct  usbd_otgfs_dtd_ext {
    CPU_REG32   dTD_NextPtr;                                    /* Next link pointer.                                   */
    CPU_REG32   Token;                                          /* dTD token.                                           */
    CPU_REG32   BufPtrs[5u];                                    /* Buffer pointer (Page n, n = [0..4]).                 */
    CPU_INT32U  BufAddr;                                        /* Buffer address.                                      */
    CPU_INT32U  BufLen;                                         /* Buffer length.                                       */
    CPU_REG32   Attrib;                                         /* Attributes of the dTD.                               */
} USBD_OTGHS_dTD_EXT;

                                                                /* -------- ENDPOINT QUEUE HEAD (dQH) DATA TYPE ------- */
typedef  struct  usbd_otgfs_dqh {
    CPU_REG32            EpCap;                                 /* Endpoint capabilities                                */
    CPU_REG32            dTD_CurrPtr;                           /* Current dTD pointer used by HW only.                 */
    USBD_OTGHS_dTD       OverArea;                              /* Overlay Area                                         */
    CPU_INT32U           Reserved0;
    CPU_REG32            SetupBuf[2u];                          /* Setup buffer                                         */
                                                                /* ------------------ dTD's LINK LIST ----------------- */
    USBD_OTGHS_dTD_EXT  *dTD_LstHeadPtr;                        /* dTD's link list head pointer                         */
    USBD_OTGHS_dTD_EXT  *dTD_LstTailPtr;                        /* dTD's link list tail pointer                         */
    CPU_REG32            dTD_LstNbrEntries;                     /* dTD's link list number of entries                    */
    CPU_INT32U           Unused;                                /* Unused space                                         */
} USBD_OTGHS_dQH;

                                                                /* --------------- OTGHS CTRLR VARIANCE --------------- */
typedef  enum  usbd_otghs_ctrlr {
    USBD_NXP_OTGHS_CTRLR_LPCXX_USBX = 0u,
    USBD_XILINX_OTGHS_CTRLR_ZYNQ
} USBD_OTGHS_CTRLR;

                                                                /* ---------- DRIVER INTERNAL DATA DATA TYPE ---------- */
typedef  struct  usbd_drv_data {
    USBD_OTGHS_dQH    *dQH_Tbl;
    MEM_POOL           dTD_MemPool;
    CPU_INT08U         hw_rev;
#if (USBD_CFG_MAX_NBR_URB_EXTRA > 0u)
    CPU_INT08U        *dTD_UsageTbl;
#endif
    CPU_BOOLEAN        Suspend;
    USBD_OTGHS_CTRLR   Ctrlr;
} USBD_DRV_DATA;

#if (OTGHS_DBG_STATS_EN == DEF_ENABLED)
typedef  struct  dbg_ep {
    CPU_INT32U   AbortCnt;
    CPU_INT32U   StallCnt;
#if (CPU_CFG_CACHE_MGMT_EN == DEF_ENABLED)
    CPU_INT32U   BufNotAlignedOnCacheLine;
#endif

    CPU_INT32U   TxStartedCnt;
    CPU_INT32U   TxEpPrimedCnt;
    CPU_INT32U   TxEndptcmplRegCnt;
    CPU_INT32U   TxCmplFromIsrCnt;

    CPU_INT32U   RxStartedCnt;
    CPU_INT32U   RxEpPrimedCnt;
    CPU_INT32U   RxEndptcmplRegCnt;
    CPU_INT32U   RxCmplFromIsrCnt;
    CPU_INT32U   RxCompletedCnt;

    CPU_INT32U   dTD_LstInsert_BufSpan4KBoundaryCnt;
    CPU_INT32U   dTD_LstInsert_LstEmptyCnt;
    CPU_INT32U   dTD_LstInsert_LstNotEmptyCnt;
    CPU_INT32U   dTD_LstInsert_ListNonEmpty_PrimeAlreadyDoneByHW_InsertionOK;
    CPU_INT32U   dTD_LstInsert_LstNonEmpty_MaxRetriesReached_InsertionNOK;
    CPU_INT32U   dTD_LstInsert_LstNonEmpty_GoodStatusInEndptstatusReg_InsertionOK;
    CPU_INT32U   dTD_LstInsert_LstNonEmpty_PrimeDoneBySW_InsertionOK;

    CPU_INT32U   dTD_LstRemove_LstLastElementCnt;
    CPU_INT32U   dTD_LstRemove_LstNonEmptyCnt;
} OTGHS_DBG_EP;

typedef  struct  otgfs_dbg_stats_drv {
    CPU_INT32U    ISR_SetupProcess_VersionLessThan2_3Cnt;
    CPU_INT32U    ISR_SetupProcess_VersionGreaterThan2_3Cnt;

    OTGHS_DBG_EP  EP_Tbl[USBD_OTGHS_EP_PHY_NBR_MAX];
} OTGHS_DBG_STATS_DRV;
#endif

/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

#if (CPU_CFG_CACHE_MGMT_EN == DEF_ENABLED)
extern  CPU_INT32U  CPU_Cache_Linesize;
#endif

#if (OTGHS_DBG_STATS_EN == DEF_ENABLED)
OTGHS_DBG_STATS_DRV  OTGHS_DbgStats;
#endif


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

                                                                /* ------------- dTD's LINK LIST FUNCTIONS ------------ */
static  void         USBD_OTGHS_dTD_LstInsert(USBD_DRV    *p_drv,
                                              CPU_INT08U   ep_phy_nbr,
                                              CPU_INT08U  *p_data,
                                              CPU_INT16U   len,
                                              USBD_ERR    *p_err);

static  CPU_BOOLEAN  USBD_OTGHS_dTD_LstRemove(USBD_DRV    *p_drv,
                                              CPU_INT08U   ep_phy_nbr);

static  CPU_BOOLEAN  USBD_OTGHS_dTD_LstEmpty (USBD_DRV    *p_drv,
                                              CPU_INT08U   ep_phy_nbr);

static  void         USBD_OTGHS_SetupProcess (USBD_DRV    *p_drv,
                                              CPU_INT08U   ep_log_nbr);

static  void         USBD_OTGHS_SoftRst      (USBD_DRV    *p_drv);


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

USBD_DRV_API  USBD_DrvAPI_Synopsys_OTG_HS = { USBD_DrvInit,
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
*                   1) Allocate software resources.
*                   2) Call BSP Init() function.
*
* Argument(s) : p_drv   Pointer to device driver structure.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE       Device successfully initialized.
*                               USBD_ERR_ALLOC      Memory allocation failed.
*                               USBD_ERR_FAIL       No maximum physical endpoint number.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_DrvInit (USBD_DRV    *p_drv,
                            USBD_ERR    *p_err)
{
    USBD_DRV_BSP_API  *p_bsp_api;
    USBD_DRV_DATA     *p_drv_data;
    CPU_INT08U         ep_phy_nbr_max;
    LIB_ERR            err_lib;


                                                                /* Allocate driver internal data.                       */
    p_drv->DataPtr = Mem_HeapAlloc(              sizeof(USBD_DRV_DATA),
                                                 sizeof(CPU_DATA),
                                   (CPU_SIZE_T *)0,
                                                &err_lib);
    if (p_drv->DataPtr == (void *)0) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

    Mem_Clr((void *)p_drv->DataPtr,
                   (sizeof(USBD_DRV_DATA)));

    p_drv_data = (USBD_DRV_DATA  *)p_drv->DataPtr;

    Mem_PoolCreate(             &p_drv_data->dTD_MemPool,       /* Create EP device transfer descriptor memory pool.    */
                   (void       *)0,                             /* From heap.                                           */
                                 0u,
                                 USBD_OTGHS_dTD_NBR,            /* Take into account extra URBs if used.                */
                                (sizeof(USBD_OTGHS_dTD_EXT)),
                                 USBD_OTGHS_ALIGN_OCTECTS_dTD,
                   (CPU_SIZE_T *)0,
                                &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

    ep_phy_nbr_max = USBD_EP_MaxPhyNbrGet(p_drv->DevNbr);       /* Get max phy EP used by the device.                   */
    if (ep_phy_nbr_max == USBD_EP_PHY_NONE) {
        *p_err = USBD_ERR_FAIL;
         return;
    }
    ep_phy_nbr_max++;                                           /* Inc because max nbf or phy EP returned is 0-based.   */
                                                                /* Allocate EP Queue Head data.                         */
    p_drv_data->dQH_Tbl = (USBD_OTGHS_dQH *)Mem_HeapAlloc(              (sizeof(USBD_OTGHS_dQH) * ep_phy_nbr_max),
                                                                         USBD_OTGHS_ALIGN_OCTECTS_dQH,
                                                          (CPU_SIZE_T  *)0,
                                                                        &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

    Mem_Clr((void  *) p_drv_data->dQH_Tbl,
                     (sizeof(USBD_OTGHS_dQH) * (ep_phy_nbr_max)));

#if (USBD_CFG_MAX_NBR_URB_EXTRA > 0u)
                                                                /* Alloc tbl tracking nbr of dTD used by ongoing... */
                                                                /* ...xfers for all open EP.                        */
    p_drv_data->dTD_UsageTbl = (CPU_INT08U *)Mem_HeapAlloc(             (sizeof(CPU_INT08U) * ep_phy_nbr_max),
                                                                        (sizeof(CPU_DATA)),
                                                           (CPU_SIZE_T *)0,
                                                                        &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

    Mem_Clr((void *)p_drv_data->dTD_UsageTbl,
                   (sizeof(CPU_INT08U) * ep_phy_nbr_max));
#endif

    p_drv_data->Suspend = DEF_FALSE;

    p_bsp_api = p_drv->BSP_API_Ptr;                             /* Get driver BSP API reference.                        */
    if (p_bsp_api->Init != (void *)0) {
        p_bsp_api->Init(p_drv);                                 /* Call board/chip specific device ctrlr init fnct.     */
    }

    OTGHS_DBG_STATS_RESET();

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
* Note(s)     : (1) Typically, the start function activates the pull-down on the D- pin to simulate
*                   attachment to host. Some MCUs/MPUs have an internal pull-down that is activated by a
*                   device controller register; for others, this may be a GPIO pin. Additionally, interrupts
*                   for reset and suspend are activated.
*
*               (2) Since the CPU frequency could be higher than OTG module clock, a timeout is needed
*                   to reset the OTG controller successfully.
*
*               (3) The NXP MCUs implementing the Synopsys USB IP defines a specific register at offset
*                   0x20 giving information about the IP version. This register does NOT exist on Xilinx
*                   Zynq-7000 SoC and Freescale K6x, K7x, iMx6. This register allows a different
*                   Setup packet processing for some specific versions of the USB IP for NXP MCUs.
*********************************************************************************************************
*/

static  void  USBD_DrvStart (USBD_DRV  *p_drv,
                             USBD_ERR  *p_err)
{
    USBD_DRV_BSP_API  *p_bsp_api;
    USBD_DRV_DATA     *p_drv_data;
    USBD_OTGHS_REG    *p_reg;
    CPU_INT16U         reg_to;
    CPU_INT32U         reg_ip_info;


    p_bsp_api  =  p_drv->BSP_API_Ptr;                           /* Get driver BSP API reference.                        */
    p_drv_data = (USBD_DRV_DATA  *)(p_drv->DataPtr);
    p_reg      = (USBD_OTGHS_REG *)(p_drv->CfgPtr->BaseAddr);

    p_reg->USBCMD = USBD_OTGHS_USBCMD_RST;                      /* Reset controller (see Note #2)                       */
    reg_to        = USBD_OTGHS_REG_TO;
    while ((reg_to                                  > 0) &&
           ((p_reg->USBCMD & USBD_OTGHS_USBCMD_RST) != 0)) {
        reg_to--;
    }

    reg_ip_info = p_reg->IP_INFO;
    if (reg_ip_info != 0u) {                                    /* See Note #3.                                         */
        p_drv_data->Ctrlr   = USBD_NXP_OTGHS_CTRLR_LPCXX_USBX;
        p_drv_data->hw_rev  = (CPU_INT08U)(CPU_INT32U)(((p_reg->IP_INFO & USBD_OTGHS_IP_INFO_MAJOR_MASK) >> 12u) * 10u);
        p_drv_data->hw_rev += (CPU_INT08U)(CPU_INT32U)(((p_reg->IP_INFO & USBD_OTGHS_IP_INFO_MINOR_MASK) >>  8u));
    } else {
        p_drv_data->Ctrlr = USBD_XILINX_OTGHS_CTRLR_ZYNQ;
    }

    p_reg->USBMODE = USBD_OTGHS_USBMODE_CM_DEV;                 /* Set device mode                                      */

    p_reg->USBINTR     = DEF_BIT_NONE;                          /* Disable all interrupts                               */
    p_reg->EP_LST_ADDR = (CPU_INT32U)p_drv_data->dQH_Tbl;

    DEF_BIT_SET(p_reg->USBMODE, USBD_OTGHS_USBMODE_SLOM);       /* Disable setup lockout                                */
    DEF_BIT_CLR(p_reg->USBMODE, USBD_OTGHS_USBMODE_SDIS);       /* Enable double priming on both Rx and Tx.             */

    USBD_OTGHS_SoftRst(p_drv);                                  /* Perform software reset                               */

    DEF_BIT_CLR(p_reg->USBCMD, USBD_OTGHS_USBCMD_ITC_MASK);     /* Dev ctrlr fires USB intr immediately.                */

    p_reg->USBINTR = USBD_OTGHS_USB_INT_UR                      /* Enable the Reset and Suspend interrupt               */
                   | USBD_OTGHS_USB_INT_SL;

    if (p_drv->CfgPtr->Spd == USBD_DEV_SPD_FULL) {
        DEF_BIT_SET(p_reg->PORTSC1, USBD_OTGHS_PORTSC1_PFSC);   /* Force FS.                                            */
    }

    DEF_BIT_SET(p_reg->USBCMD, USBD_OTGHS_USBCMD_RUN);          /* Set the RUN bit                                      */

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
    USBD_DRV_BSP_API  *p_bsp_api;
    USBD_OTGHS_REG    *p_reg;


    p_bsp_api =  p_drv->BSP_API_Ptr;                            /* Get driver BSP API reference.                        */
    p_reg     = (USBD_OTGHS_REG *)(p_drv->CfgPtr->BaseAddr);

    p_reg->USBINTR = DEF_BIT_NONE;

    if (p_bsp_api->Disconn != (void *)0) {
        p_bsp_api->Disconn();
    }

    DEF_BIT_CLR(p_reg->USBCMD, USBD_OTGHS_USBCMD_RUN);          /* Clear the RUN bit                                    */
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
    USBD_OTGHS_REG  *p_reg;


    p_reg           = (USBD_OTGHS_REG *)(p_drv->CfgPtr->BaseAddr);
    p_reg->DEV_ADDR = ((dev_addr << 25u) & USBD_OTGHS_DEV_ADDR_USBADDR_MASK) |
                                           USBD_OTGHS_DEV_ADDR_USBADDRA;

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
    (void)p_drv;
    (void)dev_addr;
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
    CPU_INT16U       frame_nbr;
    USBD_OTGHS_REG  *p_reg;


    p_reg = (USBD_OTGHS_REG *)(p_drv->CfgPtr->BaseAddr);

    frame_nbr = (p_reg->FRINDEX >> 3u);                         /* Lower 3 bits are used only for micro-frames.         */

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
*                       the endpoint is successfully configured (or 'realized' or 'mapped'). For some
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
    USBD_OTGHS_dQH  *p_dqh;
    USBD_OTGHS_REG  *p_reg;
    USBD_DRV_DATA   *p_drv_data;
    CPU_BOOLEAN      ep_dir_in;
    CPU_INT08U       ep_log_nbr;
    CPU_INT08U       ep_phy_nbr;
    CPU_INT32U       reg_val;
    CPU_SR_ALLOC();


    ep_dir_in  =  USBD_EP_IS_IN(ep_addr);                       /* Get EP direction.                                    */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical  number.                              */
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);                 /* Get EP physical number.                              */
    p_reg      = (USBD_OTGHS_REG *)(p_drv->CfgPtr->BaseAddr);
    p_drv_data = (USBD_DRV_DATA  *)(p_drv->DataPtr);
    p_dqh      = &p_drv_data->dQH_Tbl[ep_phy_nbr];

                                                                /* -------------- ENDPOINT CONFIGURATION -------------- */
                                                                /* Prepare locally the EP cfg.                          */
    reg_val = USBD_OTGHS_ENDPTCTRL_TX_EN |                      /* Enable EP Tx (i.e. IN direction).                    */
              USBD_OTGHS_ENDPTCTRL_RX_EN;                       /* Enable EP Rx (i.e. OUT direction).                   */

    switch (ep_type) {                                          /* Set the EP Tx and Rx type.                           */
        case USBD_EP_TYPE_CTRL:
             DEF_BIT_SET(reg_val, USBD_OTGHS_ENDPTCTRL_TX_TYPE_CTRL |
                                  USBD_OTGHS_ENDPTCTRL_RX_TYPE_CTRL);
             break;


        case USBD_EP_TYPE_ISOC:
             DEF_BIT_SET(reg_val, USBD_OTGHS_ENDPTCTRL_TX_TYPE_ISOC |
                                  USBD_OTGHS_ENDPTCTRL_RX_TYPE_ISOC);
             break;


        case USBD_EP_TYPE_INTR:
             DEF_BIT_SET(reg_val, USBD_OTGHS_ENDPTCTRL_TX_TYPE_INT |
                                  USBD_OTGHS_ENDPTCTRL_RX_TYPE_INT);
             break;


        case USBD_EP_TYPE_BULK:
             DEF_BIT_SET(reg_val, USBD_OTGHS_ENDPTCTRL_TX_TYPE_BULK |
                                  USBD_OTGHS_ENDPTCTRL_RX_TYPE_BULK);
             break;


        default:                                                /* 'default' case intentionally empty.                  */
             break;
    }

    if (ep_type != USBD_EP_TYPE_CTRL) {                         /* Reset Tx & Rx data toggle.                           */
        DEF_BIT_SET(reg_val, USBD_OTGHS_ENDPTCTRL_TX_TOGGLE_RST);
        DEF_BIT_SET(reg_val, USBD_OTGHS_ENDPTCTRL_RX_TOGGLE_RST);
    }

    CPU_CRITICAL_ENTER();
    if (ep_dir_in == DEF_FALSE) {                               /* OUT Endpoints                                        */
                                                                /* Reset reg upper half to keep only Rx related bits.   */
        reg_val &= USBD_OTGHS_ENDPTCTRL_RX_CFG_MASK;
        DEF_BIT_CLR(p_reg->ENDPTCTRLx[ep_log_nbr], USBD_OTGHS_ENDPTCTRL_RX_CFG_MASK);
    } else {                                                    /* IN Endpoints                                         */
                                                                /* Reset reg lower half to keep only Tx related bits.   */
        reg_val &= USBD_OTGHS_ENDPTCTRL_TX_CFG_MASK;
        DEF_BIT_CLR(p_reg->ENDPTCTRLx[ep_log_nbr], USBD_OTGHS_ENDPTCTRL_TX_CFG_MASK);
    }

    DEF_BIT_SET(p_reg->ENDPTCTRLx[ep_log_nbr], reg_val);        /* Apply the local EP cfg to EP ctrl register.          */

                                                                /* --------- ENDPOINT QUEUE HEAD CONFIGURATION -------- */
    reg_val = ((max_pkt_size << 16u) & USBD_OTGHS_dQH_EP_CAP_MAX_LEN_MASK);

    if ((ep_type   == USBD_EP_TYPE_CTRL) &&
        (ep_dir_in == DEF_FALSE)) {
        DEF_BIT_SET(reg_val              , USBD_OTGHS_dQH_EP_CAP_IOS);
        DEF_BIT_SET(p_reg->ENDPTSETUPSTAT, DEF_BIT32(ep_log_nbr));
    }

    if ((ep_type == USBD_EP_TYPE_CTRL) ||
        (ep_type == USBD_EP_TYPE_BULK) ||
        (ep_type == USBD_EP_TYPE_INTR)) {
        DEF_BIT_SET(reg_val, USBD_OTGHS_dQH_EP_CAP_ZLTS);
    }

    if (ep_type == USBD_EP_TYPE_ISOC) {
        if (transaction_frame == 1u) {
            DEF_BIT_SET(reg_val, USBD_OTGHS_dQH_EP_CAP_MULT_1);
        } else if (transaction_frame == 2u) {                   /* #### This case must be tested.                       */
            DEF_BIT_SET(reg_val, USBD_OTGHS_dQH_EP_CAP_MULT_2);
        } else if (transaction_frame == 3u) {                   /* #### This case must be tested.                       */
            DEF_BIT_SET(reg_val, USBD_OTGHS_dQH_EP_CAP_MULT_3);
        }
    }

    p_dqh->EpCap                = reg_val;
    p_dqh->OverArea.dTD_NextPtr = USBD_OTGHS_dTD_dTD_NEXT_TERMINATE;

    CPU_DCACHE_RANGE_FLUSH(p_dqh, sizeof(USBD_OTGHS_dQH));      /* Write cached memory block back to RAM.               */

    CPU_CRITICAL_EXIT();

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
    USBD_OTGHS_REG  *p_reg;
    CPU_INT08U       ep_phy_nbr;
    CPU_INT08U       ep_log_nbr;


    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_reg      = (USBD_OTGHS_REG *)(p_drv->CfgPtr->BaseAddr);

                                                                /* Disable EP to avoid undefined behavior.              */
    if (USBD_EP_IS_IN(ep_addr)) {
        DEF_BIT_CLR(p_reg->ENDPTCTRLx[ep_log_nbr], USBD_OTGHS_ENDPTCTRL_TX_EN);
    } else {
        DEF_BIT_CLR(p_reg->ENDPTCTRLx[ep_log_nbr], USBD_OTGHS_ENDPTCTRL_RX_EN);
    }

    (void)USBD_OTGHS_dTD_LstEmpty(p_drv, ep_phy_nbr);           /*Remove any left dTDs.                                 */
                                                                /* Reset EP ctrl reg.                                   */
    if (USBD_EP_IS_IN(ep_addr)) {
        DEF_BIT_CLR(p_reg->ENDPTCTRLx[ep_log_nbr], USBD_OTGHS_ENDPTCTRL_TX_CFG_MASK);
    } else {
        DEF_BIT_CLR(p_reg->ENDPTCTRLx[ep_log_nbr], USBD_OTGHS_ENDPTCTRL_RX_CFG_MASK);
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
*                               USBD_ERR_NONE           Receive successfully configured.
*                               USBD_ERR_RX             Generic Rx error.
*                               USBD_ERR_EP_QUEUING     Unable to enqueue xfer.
*
* Return(s)   : Maximum number of octets that will be received, if NO error(s).
*
*               0,                                              otherwise.
*
* Note(s)     : (1) When the CPU cache is enabled, this code ensures that the buffer start address is
*                   aligned on the cache line size. If not, the nearest address from the initial buffer
*                   start address is computed. This address is aligned on the cache line. The number of
*                   octets to flush or invalidate will be increased accordingly to take into account
*                   the buffer size plus the address adjustment.
*
*               (2) When performing a OUT transfer, the received data will have to be cache invalidated
*                   so that the CPU can read the most up-to-date data from the cache. Before letting the
*                   USB device controller process the IN transfer and write the received data into the
*                   receive buffer, cache flushing the buffer is required. If the buffer comes from the
*                   stack and its start address is not aligned on a cache line, the flush operation will
*                   flush from the nearest cache line aligned address. It will ensure that when
*                   invalidating the receive buffer in the function USBD_DrvEP_Rx(), there is no
*                   corruption of other memory locations close to the receive buffer.
*********************************************************************************************************
*/

static  CPU_INT32U  USBD_DrvEP_RxStart (USBD_DRV    *p_drv,
                                        CPU_INT08U   ep_addr,
                                        CPU_INT08U  *p_buf,
                                        CPU_INT32U   buf_len,
                                        USBD_ERR    *p_err)
{
    USBD_OTGHS_REG  *p_reg;
    CPU_INT08U       ep_phy_nbr;
    CPU_INT32U       ep_pkt_len;


    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    p_reg      = (USBD_OTGHS_REG *)(p_drv->CfgPtr->BaseAddr);

                                                                /* Force one transaction.                               */
    ep_pkt_len =  DEF_MIN(buf_len, USBD_OTGHS_dTD_TOKEN_TOTAL_BYTE_MAX);

    DEF_BIT_CLR(p_reg->USBINTR, USBD_OTGHS_USB_INT_U);          /* Disable interrupts.                                  */

#if (CPU_CFG_CACHE_MGMT_EN == DEF_ENABLED)                      /* See Note #1.                                         */
    {
        CPU_INT08U  *p_cache_aligned_buf_addr = DEF_NULL;
        CPU_INT32U   len;
        CPU_INT08U   remainder;


        remainder = (CPU_INT08U)(((CPU_INT32U)p_buf) % CPU_Cache_Linesize);
        if (remainder != 0u) {
            p_cache_aligned_buf_addr = p_buf - remainder;
            len                      = ep_pkt_len + remainder;
            OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].BufNotAlignedOnCacheLine);
        } else {
            p_cache_aligned_buf_addr = p_buf;
            len                      = ep_pkt_len;
        }

        if (len != 0) {
                                                                /* See Note #2.                                         */
            CPU_DCACHE_RANGE_FLUSH(p_cache_aligned_buf_addr, len);
        }
    }
#endif

    USBD_OTGHS_dTD_LstInsert(            p_drv,
                                         ep_phy_nbr,
                                         p_buf,
                             (CPU_INT16U)ep_pkt_len,
                                         p_err);

    OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].RxStartedCnt);

    DEF_BIT_SET(p_reg->USBINTR, USBD_OTGHS_USB_INT_U);          /* Enable interrupts.                                   */

    if (*p_err == USBD_ERR_FAIL) {
       *p_err = USBD_ERR_RX;
    }

    return (ep_pkt_len);
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
* Note(s)     : (1) See Note #1 in function 'USBD_DrvEP_RxStart()'.
*********************************************************************************************************
*/

static  CPU_INT32U  USBD_DrvEP_Rx (USBD_DRV    *p_drv,
                                   CPU_INT08U   ep_addr,
                                   CPU_INT08U  *p_buf,
                                   CPU_INT32U   buf_len,
                                   USBD_ERR    *p_err)
{
    USBD_OTGHS_REG  *p_reg;
    USBD_DRV_DATA   *p_drv_data;
    USBD_OTGHS_dQH  *p_dqh;
    CPU_INT08U       ep_phy_nbr;
    CPU_INT16U       xfer_len_rxd;
    CPU_INT32U       ep_buf_len;
    CPU_INT32U       ep_token;
    CPU_SR_ALLOC();

    (void)p_buf;

    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    p_reg      = (USBD_OTGHS_REG *)(p_drv->CfgPtr->BaseAddr);
    p_drv_data = (USBD_DRV_DATA  *)(p_drv->DataPtr);
    p_dqh      = &p_drv_data->dQH_Tbl[ep_phy_nbr];
    ep_buf_len =  DEF_MIN(buf_len, USBD_OTGHS_dTD_TOKEN_TOTAL_BYTE_MAX);

    DEF_BIT_CLR(p_reg->USBINTR, USBD_OTGHS_USB_INT_U);          /* Disable interrupts.                                  */

#if (CPU_CFG_CACHE_MGMT_EN == DEF_ENABLED)                      /* See Note #1.                                         */
    {
        CPU_INT08U  *p_cache_aligned_buf_addr = DEF_NULL;
        CPU_INT32U   len;
        CPU_INT08U   remainder;


        remainder = (CPU_INT08U)(((CPU_INT32U)p_buf) % CPU_Cache_Linesize);
        if (remainder != 0u) {
            p_cache_aligned_buf_addr = p_buf - remainder;
            len                      = ep_buf_len + remainder;
        } else {
            p_cache_aligned_buf_addr = p_buf;
            len                      = ep_buf_len;
        }

        if (len != 0) {
                                                                /* Invalidate (clear) the cached RAM block, so that the */
            CPU_DCACHE_RANGE_INV(p_cache_aligned_buf_addr, len);
                                                                /* next CPU read will be from RAM again.                */
        }
    }
#endif
    CPU_DCACHE_RANGE_INV(p_dqh, sizeof(USBD_OTGHS_dQH));
    CPU_DCACHE_RANGE_INV(p_dqh->dTD_LstHeadPtr, sizeof(USBD_OTGHS_dTD_EXT));

                                                                /* Chk for err.                                         */
    ep_token = p_dqh->dTD_LstHeadPtr->Token;

    if (DEF_BIT_IS_SET_ANY(ep_token, USBD_OTGHS_dTD_TOKEN_STATUS_ANY) == DEF_YES) {
        if (DEF_BIT_IS_SET(ep_token, USBD_OTGHS_dTD_TOKEN_STATUS_DATA_ERR) == DEF_YES) {
           *p_err = USBD_ERR_DRV_BUF_OVERFLOW;                  /* Buf ovrf err can happen on any type of EP.           */
        } else if (DEF_BIT_IS_SET(ep_token, USBD_OTGHS_dTD_TOKEN_STATUS_TRAN_ERR) == DEF_YES) {
           *p_err = USBD_ERR_DRV_INVALID_PKT;                   /* Pkt err or fulfillment err is only on isoc EP.       */
        } else {
           *p_err = USBD_ERR_RX;                                /* Signal err even if no particular err is defined.     */
        }
     } else {
        *p_err = USBD_ERR_NONE;
    }

                                                                /* Calc rx'd len.                                       */
    xfer_len_rxd = (CPU_INT16U)(p_dqh->dTD_LstHeadPtr->BufLen -
                              ((p_dqh->dTD_LstHeadPtr->Token  & USBD_OTGHS_dTD_TOKEN_TOTAL_BYTES_MASK) >> 16u));

    if (xfer_len_rxd != 0u) {
                                                                /* Chk rem data to read.                                */
        if (xfer_len_rxd > ep_buf_len) {
            CPU_CRITICAL_ENTER();
                                                                /* Update size & pointer to memory buffer.              */
            p_dqh->dTD_LstHeadPtr->Token = ((p_dqh->dTD_LstHeadPtr->Token) & USBD_OTGHS_dTD_TOKEN_TOTAL_BYTES_MASK)
                                         + ((xfer_len_rxd << 16u)          & USBD_OTGHS_dTD_TOKEN_TOTAL_BYTES_MASK);
                                                                /* Write cached memory block back to RAM.               */
            CPU_DCACHE_RANGE_FLUSH(p_dqh->dTD_LstHeadPtr, sizeof(USBD_OTGHS_dTD_EXT));
            CPU_CRITICAL_EXIT();
        }
    }

    USBD_OTGHS_dTD_LstRemove(p_drv, ep_phy_nbr);
    OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].RxCompletedCnt);

    DEF_BIT_SET(p_reg->USBINTR, USBD_OTGHS_USB_INT_U);          /* Enable interrupts.                                   */

    return (xfer_len_rxd);
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
    (void)USBD_DrvEP_Rx(              p_drv,
                                      ep_addr,
                        (CPU_INT08U *)0u,
                                      0u,
                                      p_err);
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
    CPU_INT32U  ep_pkt_len;


    (void)p_drv;
    (void)ep_addr;
    (void)p_buf;

    ep_pkt_len = DEF_MIN(buf_len, USBD_OTGHS_dTD_TOKEN_TOTAL_BYTE_MAX);
   *p_err      = USBD_ERR_NONE;

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
*                               USBD_ERR_NONE           Data successfully transmitted.
*                               USBD_ERR_TX             Generic Tx error.
*                               USBD_ERR_EP_QUEUING     Unable to enqueue xfer.
*
* Return(s)   : none.
*
* Note(s)     : (1) See Note #1 in function 'USBD_DrvEP_RxStart()'.
*********************************************************************************************************
*/

static  void  USBD_DrvEP_TxStart (USBD_DRV    *p_drv,
                                  CPU_INT08U   ep_addr,
                                  CPU_INT08U  *p_buf,
                                  CPU_INT32U   buf_len,
                                  USBD_ERR    *p_err)
{
    USBD_OTGHS_REG  *p_reg;
    CPU_INT08U       ep_phy_nbr;
    CPU_INT32U       ep_pkt_len;


    p_reg      = (USBD_OTGHS_REG *)(p_drv->CfgPtr->BaseAddr);
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    ep_pkt_len =  DEF_MIN(buf_len, USBD_OTGHS_dTD_TOKEN_TOTAL_BYTE_MAX);

    DEF_BIT_CLR(p_reg->USBINTR, USBD_OTGHS_USB_INT_U);          /* Disable interrupts.                                  */

#if (CPU_CFG_CACHE_MGMT_EN == DEF_ENABLED)                      /* See Note #1.                                         */
    {
        CPU_INT08U  *p_cache_aligned_buf_addr = DEF_NULL;
        CPU_INT32U   len;
        CPU_INT08U   remainder;


        remainder = (CPU_INT08U)(((CPU_INT32U)p_buf) % CPU_Cache_Linesize);
        if (remainder != 0u) {
            p_cache_aligned_buf_addr = p_buf - remainder;
            len                      = ep_pkt_len + remainder;
            OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].BufNotAlignedOnCacheLine);
        } else {
            p_cache_aligned_buf_addr = p_buf;
            len                      = ep_pkt_len;
        }

        if (len != 0) {
                                                                /* Write the cached memory block back to RAM, before... */
            CPU_DCACHE_RANGE_FLUSH(p_cache_aligned_buf_addr, len);
                                                                /* ...initiating the DMA transfer.                      */
        }
    }
#endif

    USBD_OTGHS_dTD_LstInsert(            p_drv,
                                         ep_phy_nbr,
                                         p_buf,
                             (CPU_INT16U)ep_pkt_len,
                                         p_err);

    OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].TxStartedCnt);
    DEF_BIT_SET(p_reg->USBINTR, USBD_OTGHS_USB_INT_U);          /* Enable interrupts.                                   */

    if (*p_err == USBD_ERR_FAIL) {
       *p_err = USBD_ERR_TX;
    }
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
    USBD_DrvEP_TxStart (              p_drv,
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
    USBD_OTGHS_REG  *p_reg;
    USBD_DRV_DATA   *p_drv_data;
    CPU_INT32U       ix;
    CPU_INT32U       entries;
    CPU_BOOLEAN      ok;
    CPU_INT08U       ep_phy_nbr;
    CPU_INT08U       ep_log_nbr;
    CPU_INT32U       ep_flush;
    CPU_BOOLEAN      flush_done;
    CPU_INT32U       nbr_retries;
    CPU_INT32U       reg_to;
    CPU_BOOLEAN      valid;


    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_reg      = (USBD_OTGHS_REG *)(p_drv->CfgPtr->BaseAddr);
    p_drv_data = (USBD_DRV_DATA  *)(p_drv->DataPtr);
    ok         =  DEF_OK;
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    valid      =  DEF_OK;

    CPU_DCACHE_RANGE_INV(&(p_drv_data->dQH_Tbl[ep_phy_nbr]), sizeof(USBD_OTGHS_dQH));

    entries = p_drv_data->dQH_Tbl[ep_phy_nbr].dTD_LstNbrEntries;

    if (USBD_EP_IS_IN(ep_addr) == DEF_YES) {
        ep_flush = USBD_OTGHS_ENDPTxxx_GET_TX_BITS(ep_log_nbr);
    } else {
        ep_flush = USBD_OTGHS_ENDPTxxx_GET_RX_BITS(ep_log_nbr);
    }

    nbr_retries = USBD_OTGHS_MAX_RETRIES;
    flush_done  = DEF_FALSE;

    while ((flush_done  == DEF_FALSE) &&
           (nbr_retries >  0u)) {

        p_reg->ENDPTFLUSH = ep_flush;

        reg_to = USBD_OTGHS_REG_TO;
        while (((p_reg->ENDPTFLUSH & ep_flush) != 0u) &&
               (reg_to                          > 0u)) {
            reg_to--;
        }

        flush_done = DEF_BIT_IS_CLR(p_reg->ENDPTSTATUS, ep_flush);
        nbr_retries--;
    }

    for (ix = 0u; ix < entries; ix++) {
        ok = USBD_OTGHS_dTD_LstRemove(p_drv, ep_phy_nbr);
        if (ok == DEF_FAIL) {
            break;
        }
    }

    if (nbr_retries == 0u) {
        valid = DEF_FAIL;
    }

    OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].AbortCnt);
    return (valid);
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
    USBD_OTGHS_REG  *p_reg;
    CPU_INT08U       ep_log_nbr;


    p_reg      = (USBD_OTGHS_REG *)(p_drv->CfgPtr->BaseAddr);
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);

    if (state == DEF_SET) {
        if (USBD_EP_IS_IN(ep_addr) == DEF_YES) {
            DEF_BIT_SET(p_reg->ENDPTCTRLx[ep_log_nbr],
                        USBD_OTGHS_ENDPTCTRL_TX_STALL);
        } else {
            DEF_BIT_SET(p_reg->ENDPTCTRLx[ep_log_nbr],
                        USBD_OTGHS_ENDPTCTRL_RX_STALL);
        }
    } else {
        if (ep_log_nbr > 0u) {
            if (USBD_EP_IS_IN(ep_addr) == DEF_YES) {
                DEF_BIT_CLR(p_reg->ENDPTCTRLx[ep_log_nbr],
                            USBD_OTGHS_ENDPTCTRL_TX_STALL);

                DEF_BIT_SET(p_reg->ENDPTCTRLx[ep_log_nbr],
                            USBD_OTGHS_ENDPTCTRL_TX_TOGGLE_RST);
            } else {
                DEF_BIT_CLR(p_reg->ENDPTCTRLx[ep_log_nbr],
                            USBD_OTGHS_ENDPTCTRL_RX_STALL);

                DEF_BIT_SET(p_reg->ENDPTCTRLx[ep_log_nbr],
                            USBD_OTGHS_ENDPTCTRL_RX_TOGGLE_RST);
            }
        }
    }

    OTGHS_DBG_STATS_INC(EP_Tbl[USBD_EP_ADDR_TO_PHY(ep_addr)].StallCnt);
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
* Note(s)     : (1) The bit DCSuspend in the register USBSTS generates an interrupt each time it
*                   transitions. From 0 to 1, the interrupt generated is for a Suspend event.
*                   From 1 to 0, the interrupt generated is for the device controller exiting
*                   from a suspend state.
*********************************************************************************************************
*/

static  void  USBD_DrvISR_Handler (USBD_DRV  *p_drv)
{
    USBD_OTGHS_REG      *p_reg;
    USBD_DRV_DATA       *p_drv_data;
    USBD_OTGHS_dTD_EXT  *p_dtd;
    USBD_OTGHS_dTD_EXT  *p_dtd_next;
    CPU_INT32U           ep_complete;
    CPU_INT32U           ep_setup;
    CPU_INT08U           ep_log_nbr;
    CPU_INT08U           ep_phy_nbr;
    CPU_INT32U           int_status;
    CPU_INT32U           int_en;


    p_drv_data  = (USBD_DRV_DATA  *)(p_drv->DataPtr);
    p_reg       = (USBD_OTGHS_REG *)(p_drv->CfgPtr->BaseAddr);

    int_status  = p_reg->USBSTS;
    int_en      = p_reg->USBINTR;
    int_status &= int_en;

    if (int_status != DEF_BIT_NONE) {
                                                                /* ------------- HIGH-FREQUENCY INTERRUPTS ------------ */
        if (DEF_BIT_IS_SET(int_status, USBD_OTGHS_USB_INT_U) == DEF_YES) {
            p_reg->USBSTS = USBD_OTGHS_USB_INT_U;

            ep_setup = p_reg->ENDPTSETUPSTAT;                   /* (1) Process all setup transactions                   */
            while (ep_setup != DEF_BIT_NONE) {
                ep_log_nbr = (CPU_INT08U)(31u - CPU_CntLeadZeros32(ep_setup));
                USBD_OTGHS_SetupProcess(p_drv,
                                        ep_log_nbr);
                ep_setup = p_reg->ENDPTSETUPSTAT;
            }

            ep_complete          = p_reg->ENDPTCOMPLETE;        /* (2) Process all IN/OUT  transactions                 */
            p_reg->ENDPTCOMPLETE = ep_complete;
            while (ep_complete != DEF_BIT_NONE) {

                if (DEF_BIT_IS_SET_ANY(ep_complete, USBD_OTGHS_ENDPTCOMPLETE_TX_MASK) == DEF_YES) {
                    ep_log_nbr = (CPU_INT08U)(31u - CPU_CntLeadZeros32(USBD_OTGHS_ENDPTxxx_GET_TX_NBR(ep_complete)));
                    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(USBD_EP_LOG_TO_ADDR_IN(ep_log_nbr));
                    OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].TxEndptcmplRegCnt);

                    ep_complete &= ~USBD_OTGHS_ENDPTxxx_GET_TX_BITS(ep_log_nbr);
                    CPU_DCACHE_RANGE_INV(&p_drv_data->dQH_Tbl[ep_phy_nbr], sizeof(USBD_OTGHS_dQH));
                    p_dtd        =  p_drv_data->dQH_Tbl[ep_phy_nbr].dTD_LstHeadPtr;
                    CPU_DCACHE_RANGE_INV(p_dtd, sizeof(USBD_OTGHS_dTD_EXT));
                                                                /* Loop if many dTDs have completed in a single int.    */
                    if (p_dtd != ((USBD_OTGHS_dTD_EXT *)USBD_OTGHS_dTD_dTD_NEXT_TERMINATE)) {
                        while (DEF_BIT_IS_CLR(p_dtd->Token, USBD_OTGHS_dTD_TOKEN_STATUS_ACTIVE) == DEF_YES) {
                            p_dtd_next = (USBD_OTGHS_dTD_EXT *)p_dtd->dTD_NextPtr;

                            USBD_EP_TxCmpl(p_drv, ep_log_nbr);
                            OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].TxCmplFromIsrCnt);

                            USBD_OTGHS_dTD_LstRemove(p_drv, ep_phy_nbr);

                            if ((((CPU_REG32)p_dtd_next)                                                  == ((CPU_REG32) 1)) ||
                                (DEF_BIT_IS_SET((CPU_REG32)p_dtd_next, USBD_OTGHS_dTD_dTD_NEXT_TERMINATE) == DEF_YES)) {
                                break;
                            }

                            p_dtd = p_dtd_next;
                            CPU_DCACHE_RANGE_INV(p_dtd, sizeof(USBD_OTGHS_dTD_EXT));
                        }
                    }
                }

                if (DEF_BIT_IS_SET_ANY(ep_complete, USBD_OTGHS_ENDPTCOMPLETE_RX_MASK) == DEF_YES) {
                    ep_log_nbr = (CPU_INT08U)(31u - CPU_CntLeadZeros32(USBD_OTGHS_ENDPTxxx_GET_RX_NBR(ep_complete)));
                    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(USBD_EP_LOG_TO_ADDR_OUT(ep_log_nbr));
                    OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].RxEndptcmplRegCnt);

                    ep_complete &= ~USBD_OTGHS_ENDPTxxx_GET_RX_BITS(ep_log_nbr);
                    CPU_DCACHE_RANGE_INV(&p_drv_data->dQH_Tbl[ep_phy_nbr], sizeof(USBD_OTGHS_dQH));
                    p_dtd        =  p_drv_data->dQH_Tbl[ep_phy_nbr].dTD_LstHeadPtr;
                    CPU_DCACHE_RANGE_INV(p_dtd, sizeof(USBD_OTGHS_dTD_EXT));
                                                                /* Loop if many dTDs have completed in a single int.    */
                    if (p_dtd != ((USBD_OTGHS_dTD_EXT *)USBD_OTGHS_dTD_dTD_NEXT_TERMINATE)) {
                        while (DEF_BIT_IS_CLR(p_dtd->Token, USBD_OTGHS_dTD_TOKEN_STATUS_ACTIVE) == DEF_YES) {
                            p_dtd_next = (USBD_OTGHS_dTD_EXT *)p_dtd->dTD_NextPtr;

                            if (DEF_BIT_IS_CLR(p_dtd->Attrib, USBD_OTGHS_dTD_EXT_ATTRIB_IS_COMPLETED) == DEF_YES) {
                                USBD_EP_RxCmpl(p_drv, ep_log_nbr);
                                OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].RxCmplFromIsrCnt);
                                DEF_BIT_SET(p_dtd->Attrib, USBD_OTGHS_dTD_EXT_ATTRIB_IS_COMPLETED);
                                CPU_DCACHE_RANGE_FLUSH((void *)&p_dtd->Attrib, sizeof(CPU_REG32));
                            }
                                                                    /* End of dTD list attached to this dQH.                */
                            if ((((CPU_REG32)p_dtd_next)                                                  == ((CPU_REG32) 1)) ||
                                (DEF_BIT_IS_SET((CPU_REG32)p_dtd_next, USBD_OTGHS_dTD_dTD_NEXT_TERMINATE) == DEF_YES)) {
                                break;
                            }

                            p_dtd = p_dtd_next;
                            CPU_DCACHE_RANGE_INV(p_dtd, sizeof(USBD_OTGHS_dTD_EXT));
                        }
                    }
                }
            }
        }

        if (DEF_BIT_IS_SET(int_status, USBD_OTGHS_USB_INT_UE) == DEF_YES) {
            p_reg->USBSTS = USBD_OTGHS_USB_INT_UE;
        }

                                                                /* ------------- LOW-FREQUENCY INTERRUPTS ------------- */
                                                                /* (3) Process the bus change interrupts                */
                                                                /* ---------------- USB RESET INTERRUPT --------------- */
        if (DEF_BIT_IS_SET(int_status, USBD_OTGHS_USB_INT_UR) == DEF_YES) {

            USBD_OTGHS_SoftRst(p_drv);                          /* Perform a soft reset                                 */

            USBD_EventReset(p_drv);                             /* Notify bus reset.                                    */

            p_reg->USBSTS  = USBD_OTGHS_USB_INT_UR;             /* Clear the interrupt                                  */
            p_reg->USBINTR = USBD_OTGHS_USB_INT_SL
                           | USBD_OTGHS_USB_INT_UR
                           | USBD_OTGHS_USB_INT_PC
                           | USBD_OTGHS_USB_INT_U
                           | USBD_OTGHS_USB_INT_UE;
        }

                                                                /* --------------- USB SUSPEND INTERRUPT -------------- */
                                                                /* See Note #1.                                         */
        if ((DEF_BIT_IS_SET(int_status, USBD_OTGHS_USB_INT_SL) == DEF_YES) &&
            (p_drv_data->Suspend                               == DEF_FALSE)) {

            USBD_EventSuspend(p_drv);

            p_reg->USBSTS  = USBD_OTGHS_USB_INT_SL;             /* Clear the suspend interrupt                          */
            p_reg->USBINTR = USBD_OTGHS_USB_INT_UR              /* Enable the Reset and Port change interrupt           */
                           | USBD_OTGHS_USB_INT_U
                           | USBD_OTGHS_USB_INT_PC
                           | USBD_OTGHS_USB_INT_UE;
            p_drv_data->Suspend = DEF_TRUE;

        } else if ((DEF_BIT_IS_SET(int_status, USBD_OTGHS_USB_INT_SL) == DEF_NO) &&
                   (p_drv_data->Suspend                               == DEF_TRUE)) {

            USBD_EventResume(p_drv);
            p_drv_data->Suspend = DEF_FALSE;
        }


                                                                /* ------------- USB PORT CHANGE INTERRUPT ------------ */
        if (DEF_BIT_IS_SET(int_status, USBD_OTGHS_USB_INT_PC) == DEF_YES) {
                                                                /* Detect the speed of the device                       */
            if (DEF_BIT_IS_SET(p_reg->PORTSC1, USBD_OTGHS_PORTSC1_HSP) == DEF_YES) {
                USBD_EventHS(p_drv);                            /* Notify high-speed event.                             */
            }
                                                                /* Clear the Port change interrupt                       */
            p_reg->USBSTS = USBD_OTGHS_USB_INT_PC;
        }
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
*                                      USBD_OTGHS_dTD_LstInsert()
*
* Description : Insert a new dTD at the end of the link list.
*               (1) Get a dTD from the memory pool.
*               (2) Build the transfer descriptor.
*               (3) Insert the dTD at the end of the link list
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_phy_nbr  Endpoint logical number.
*
*               p_data      Pointer to the data buffer; ignored for OUT endpoints.
*
*               len         Transfer length.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           dTd successfully obtained, filled and queued.
*                               USBD_ERR_FAIL           Generic failure error.
*                               USBD_ERR_EP_QUEUING     No more dTD remaining to queue.
*
* Return(s)   : none.
*
* Note(s)     : (1) If the endpoint is not a control endpoint, it is required to make sure that there is
*                   a dTD left. This is not required for control endpoints, since they cannot have more
*                   than one dTD queued at any time.
*
*               (2) 1st condition in the IF statement ensures that a dTD can be allocated for an open
*                   endpoint having already some ongoing dTD. (-1u) guarantees that there is always 1 dTD
*                   available for the control endpoint.
*                   2nd and 3rd conditions allows a dTD to be allocated if the considered endpoint is
*                   an empty endpoint and there are some dTD available.
*********************************************************************************************************
*/

static  void  USBD_OTGHS_dTD_LstInsert (USBD_DRV    *p_drv,
                                        CPU_INT08U   ep_phy_nbr,
                                        CPU_INT08U  *p_data,
                                        CPU_INT16U   len,
                                        USBD_ERR    *p_err)
{
    USBD_OTGHS_REG      *p_reg;
    USBD_DRV_DATA       *p_drv_data;
    USBD_OTGHS_dTD_EXT  *p_dtd;
    USBD_OTGHS_dTD_EXT  *p_dtd_last;
    USBD_OTGHS_dQH      *p_dqh;
    CPU_INT08U          *p_buf_page;
    CPU_INT08U           i;
    CPU_INT08U           insert_nbr_tries;
    CPU_BOOLEAN          insert_complete;
    CPU_BOOLEAN          valid_bit;
    CPU_INT32U           ep_status;
    CPU_INT08U           ep_log_nbr;
    LIB_ERR              err_lib;
#if (USBD_CFG_MAX_NBR_URB_EXTRA > 0u)
    CPU_INT08U           dtd_used;
    CPU_INT08U           ep_empty;
    CPU_INT08U           ep_phy_nbr_max;
    CPU_INT16U           dTD_avail;
#endif
    CPU_SR_ALLOC();


    p_drv_data = (USBD_DRV_DATA  *)(p_drv->DataPtr);
    p_reg      = (USBD_OTGHS_REG *)(p_drv->CfgPtr->BaseAddr);
    p_dqh      = &p_drv_data->dQH_Tbl[ep_phy_nbr];
    ep_log_nbr =  USBD_EP_PHY_TO_LOG(ep_phy_nbr);

    CPU_DCACHE_RANGE_INV(p_dqh, sizeof(USBD_OTGHS_dQH));        /* Invalidate (clear) the cached RAM block, so that the */
                                                                /* next CPU read will be from RAM again.                */
    p_dtd_last = p_dqh->dTD_LstTailPtr;

#if (USBD_CFG_MAX_NBR_URB_EXTRA > 0u)
                                                                /* If EP type is bulk, intr or isoc.                    */
    if (((p_reg->ENDPTCTRLx[ep_log_nbr] & USBD_OTGHS_ENDPTCTRL_TX_TYPE_MASK) != USBD_OTGHS_ENDPTCTRL_TX_TYPE_CTRL) ||
        ((p_reg->ENDPTCTRLx[ep_log_nbr] & USBD_OTGHS_ENDPTCTRL_RX_TYPE_MASK) != USBD_OTGHS_ENDPTCTRL_RX_TYPE_CTRL)) {

        ep_empty       = 0u;                                    /* Chk if a dTD is avail (see Note #1).                 */
        dtd_used       = 0u;
        ep_phy_nbr_max =  USBD_EP_MaxPhyNbrGet(p_drv->DevNbr);
        CPU_CRITICAL_ENTER();
        for (i = 0u; i < ep_phy_nbr_max; ++i) {
            ep_empty += (p_drv_data->dTD_UsageTbl[i] == 0u) ? 1u : 0u;
            dtd_used +=  p_drv_data->dTD_UsageTbl[i];
        }

        dTD_avail = USBD_OTGHS_dTD_NBR - dtd_used;
                                                                /* See Note #2.                                         */
        if ((((CPU_INT16U)(dtd_used + ep_empty))    < (USBD_OTGHS_dTD_NBR - 1u)) ||
            ((p_drv_data->dTD_UsageTbl[ep_phy_nbr] == 0u)                        &&
             (dTD_avail                            != 0u))) {

            (p_drv_data->dTD_UsageTbl[ep_phy_nbr])++;

        } else {
            CPU_CRITICAL_EXIT();
           *p_err = USBD_ERR_EP_QUEUING;
            return;
        }
        CPU_CRITICAL_EXIT();
    } else {                                                    /* Chk not required for ctrl EP (see Note #1).          */
        CPU_CRITICAL_ENTER();
        (p_drv_data->dTD_UsageTbl[ep_phy_nbr])++;
        CPU_CRITICAL_EXIT();
    }
#endif
                                                                /* (1) Get a dTD from the memory pool                   */
    p_dtd = (USBD_OTGHS_dTD_EXT *)Mem_PoolBlkGet(&p_drv_data->dTD_MemPool,
                                                  sizeof(USBD_OTGHS_dTD_EXT),
                                                 &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = USBD_ERR_FAIL;
        return;
    }

    Mem_Clr((void *)p_dtd,                                      /* ... Initialize the dTD to 0x00                       */
                    sizeof(USBD_OTGHS_dTD_EXT));

                                                                /* (2) Build the transfer descriptor                    */
    p_dtd->dTD_NextPtr = USBD_OTGHS_dTD_dTD_NEXT_TERMINATE;     /* ... Set the terminate bit to 1                       */
                                                                /* ... Fill in the total transfer len.                  */
    p_dtd->Token       = ((len << 16u) & USBD_OTGHS_dTD_TOKEN_TOTAL_BYTES_MASK)
                                       | USBD_OTGHS_dTD_TOKEN_IOC
                                       | USBD_OTGHS_dTD_TOKEN_STATUS_ACTIVE;

    p_buf_page = p_data;

    p_dtd->BufPtrs[0] = (CPU_INT32U)p_buf_page;                 /* Init Buffer Pointer (Page 0) + Current Offset        */
                                                                /* Init Buffer Pointer List if buffer spans more ...    */
                                                                /* ... than one physical page (see Note #3).            */
    for (i = 1u; i <= 4u; i++) {                                /* Init Buffer Pointer (Page 1 to 4)                    */
                                                                /* Find the next closest 4K-page boundary ahead.        */
        p_buf_page = (CPU_INT08U *)(((CPU_INT32U)p_buf_page + 0x1000u) & 0xFFFFF000u);

        if (p_buf_page < (p_data + len)) {                      /* If buffer spans a new 4K-page boundary.              */
                                                                /* Set page ptr to ref start of the subsequent 4K page. */
            p_dtd->BufPtrs[i]  = (CPU_INT32U)p_buf_page;
            OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].dTD_LstInsert_BufSpan4KBoundaryCnt);
        } else {                                                /* All the transfer size has been described...          */
            break;                                              /* ... quit the loop.                                   */
        }
    }

    p_dtd->BufAddr = (CPU_INT32U)p_data;                        /* Save the buffer address                              */
    p_dtd->BufLen  =             len;
    p_dtd->Attrib  =             0u;                            /* Reset the field's value.                             */
    CPU_DCACHE_RANGE_FLUSH(p_dtd, sizeof(USBD_OTGHS_dTD_EXT));

    CPU_CRITICAL_ENTER();
                                                                /* (3) Insert the dTD at the end of the link list       */
    if (p_dqh->dTD_LstNbrEntries == 0u) {                       /* ... Case 1: Link list is empty                       */
        OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].dTD_LstInsert_LstEmptyCnt);

        p_dqh->dTD_LstHeadPtr = p_dtd;
        p_dqh->dTD_LstTailPtr = p_dtd;
        p_dqh->dTD_LstNbrEntries++;
                                                                /* (a) Write dQH next pointer and dQH terminate ...     */
                                                                /* ... bit to '0' as a single operation                 */
        p_dqh->OverArea.dTD_NextPtr = (CPU_INT32U)p_dtd;

                                                                /* (b) Clear the Status bits                            */
        DEF_BIT_CLR(p_dqh->OverArea.Token, USBD_OTGHS_dTD_TOKEN_STATUS_MASK);

        CPU_DCACHE_RANGE_FLUSH(p_dqh, sizeof(USBD_OTGHS_dQH));

                                                                /* (c) Prime the endpoint                               */
        if (USBD_OTGHS_EP_PHY_NBR_IS_OUT(ep_phy_nbr) == DEF_YES) {
            p_reg->ENDPTPRIME = USBD_OTGHS_ENDPTxxx_GET_RX_BITS(ep_log_nbr);
            OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].RxEpPrimedCnt);
        } else {
            p_reg->ENDPTPRIME = USBD_OTGHS_ENDPTxxx_GET_TX_BITS(ep_log_nbr);
            OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].TxEpPrimedCnt);
        }
    } else {
        OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].dTD_LstInsert_LstNotEmptyCnt);
                                                                /* ... Case 2: Link list is not empty                   */
                                                                /* (a) Add dTD to end of linked list                    */
        p_dqh->dTD_LstTailPtr->dTD_NextPtr = (CPU_INT32U)p_dtd;
        CPU_DCACHE_RANGE_FLUSH(p_dqh->dTD_LstTailPtr, sizeof(USBD_OTGHS_dTD_EXT));
        p_dqh->dTD_LstTailPtr              =  p_dtd;
        p_dqh->dTD_LstNbrEntries++;
        CPU_DCACHE_RANGE_FLUSH(p_dqh, sizeof(USBD_OTGHS_dQH));

                                                                /* (b) Read correct prime bit. IF '1' DONE.             */
        if (USBD_OTGHS_EP_PHY_NBR_IS_OUT(ep_phy_nbr) == DEF_YES) {
            valid_bit = DEF_BIT_IS_SET(USBD_OTGHS_ENDPTxxx_GET_RX_NBR(p_reg->ENDPTPRIME),
                                      (DEF_BIT32(ep_log_nbr)));
        } else {
            valid_bit = DEF_BIT_IS_SET(USBD_OTGHS_ENDPTxxx_GET_TX_NBR(p_reg->ENDPTPRIME),
                                      (DEF_BIT32(ep_log_nbr)));
        }
        if (valid_bit == DEF_YES) {
            OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].dTD_LstInsert_ListNonEmpty_PrimeAlreadyDoneByHW_InsertionOK);
            CPU_CRITICAL_EXIT();
           *p_err = USBD_ERR_NONE;
            return;
        }

        insert_nbr_tries = USBD_OTGHS_dTD_LST_INSERT_NBR_TRIES_MAX;
        insert_complete  = DEF_FALSE;
        ep_status        = DEF_BIT_NONE;

        while ((insert_complete  == DEF_FALSE) &&
               (insert_nbr_tries >  0u)) {


            DEF_BIT_SET(p_reg->USBCMD, USBD_OTGHS_USBCMD_ATDTW);/* (c) Set ATDTW (HW sem) bit in USBCMD register to '1' */

            ep_status = p_reg->ENDPTSTATUS;                     /* (d) Read correct status bits in ENDPSTATUS           */

                                                                /* (e) Read ATDTW bit in USBCMD register ...            */
            if (DEF_BIT_IS_SET(p_reg->USBCMD, USBD_OTGHS_USBCMD_ATDTW) == DEF_YES){
                insert_complete = DEF_TRUE;                     /* ... If '1' continue to (f)                           */
            } else {
                insert_nbr_tries--;                             /* ... If '0' goto (c)                                  */
            }
        }

        if (insert_complete == DEF_FALSE) {

            Mem_PoolBlkFree(       &p_drv_data->dTD_MemPool,
                            (void *)p_dtd,
                                   &err_lib);

            p_dqh->dTD_LstTailPtr              = p_dtd_last;
            p_dqh->dTD_LstTailPtr->dTD_NextPtr = USBD_OTGHS_dTD_dTD_NEXT_TERMINATE;
            p_dqh->dTD_LstNbrEntries--;
            CPU_DCACHE_RANGE_FLUSH(p_dqh, sizeof(USBD_OTGHS_dQH));
            CPU_DCACHE_RANGE_FLUSH(p_dqh->dTD_LstTailPtr, sizeof(USBD_OTGHS_dTD_EXT));

            CPU_CRITICAL_EXIT();
           *p_err = USBD_ERR_FAIL;
            OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].dTD_LstInsert_LstNonEmpty_MaxRetriesReached_InsertionNOK);
            return;
        }

        DEF_BIT_CLR(p_reg->USBCMD, USBD_OTGHS_USBCMD_ATDTW);    /* (f) Set ATDTW (HW sem) bit in USBCMD register to '0' */

        if (USBD_OTGHS_EP_PHY_NBR_IS_OUT(ep_phy_nbr) == DEF_YES) {
            valid_bit = DEF_BIT_IS_SET(USBD_OTGHS_ENDPTxxx_GET_RX_NBR(ep_status),
                                      (DEF_BIT32(ep_log_nbr)));
        } else {
            valid_bit = DEF_BIT_IS_SET(USBD_OTGHS_ENDPTxxx_GET_TX_NBR(ep_status),
                                      (DEF_BIT32(ep_log_nbr)));
        }

        if (valid_bit == DEF_YES) {
            CPU_CRITICAL_EXIT();
           *p_err = USBD_ERR_NONE;
            OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].dTD_LstInsert_LstNonEmpty_GoodStatusInEndptstatusReg_InsertionOK);
            return;                                             /* (g) If status bit read in (d) is '1' DONE            */
        } else {                                                /* DCD must be re-primed EP.                            */
                                                                /* (h) If status bit read in (d) is '0' goto Case 1     */
            p_dqh->OverArea.dTD_NextPtr = (CPU_INT32U)p_dqh->dTD_LstHeadPtr;

            DEF_BIT_CLR(p_dqh->OverArea.Token,
                        USBD_OTGHS_dTD_TOKEN_STATUS_MASK);

            CPU_DCACHE_RANGE_FLUSH(p_dqh, sizeof(USBD_OTGHS_dQH));
                                                                /* (e) Prime the endpoint                               */
            if (USBD_OTGHS_EP_PHY_NBR_IS_OUT(ep_phy_nbr) == DEF_YES) {
                p_reg->ENDPTPRIME = USBD_OTGHS_ENDPTxxx_GET_RX_BITS(ep_log_nbr);
                OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].RxEpPrimedCnt);
            } else {
                p_reg->ENDPTPRIME = USBD_OTGHS_ENDPTxxx_GET_TX_BITS(ep_log_nbr);
                OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].TxEpPrimedCnt);
            }
            OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].dTD_LstInsert_LstNonEmpty_PrimeDoneBySW_InsertionOK);
        }
    }
    CPU_CRITICAL_EXIT();

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       USBD_OTGHS_dTD_LstEmpty()
*
* Description : This function flush the dTD list. All the dTD and Rx buffers are returned to the memory pool.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_phy_nbr  Endpoint physical number.
*
* Return(s)   : DEF_OK,   if NO error(s).
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_OTGHS_dTD_LstEmpty (USBD_DRV    *p_drv,
                                              CPU_INT08U   ep_phy_nbr)
{
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT32U      ix;
    CPU_INT32U      entries;
    CPU_BOOLEAN     ok;
    CPU_INT08U      ep_addr;


    p_drv_data = (USBD_DRV_DATA *)(p_drv->DataPtr);
    CPU_DCACHE_RANGE_INV(&(p_drv_data->dQH_Tbl[ep_phy_nbr]), sizeof(USBD_OTGHS_dQH));
    entries    =  p_drv_data->dQH_Tbl[ep_phy_nbr].dTD_LstNbrEntries;
    ok         =  DEF_OK;

    for (ix = 0u; ix < entries; ix++) {
        ok = USBD_OTGHS_dTD_LstRemove(p_drv, ep_phy_nbr);
        if (ok == DEF_FAIL) {
            break;
        }
    }

    ep_addr = USBD_EP_PHY_TO_ADDR(ep_phy_nbr);

    if (entries > 0u) {
        USBD_DrvEP_Abort(p_drv, ep_addr);
    }

    return (ok);
}


/*
*********************************************************************************************************
*                                      USBD_OTGHS_dTD_LstRemove()
*
* Description : Remove a dTD from the link list.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_phy_nbr  Endpoint physical number.
*
* Return(s)   : DEF_OK,   if NO error(s).
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) The link list before the Remove function will look like:
*
*                           +-----+      +-----+          +-----+
*     dTD_LstHeadPtr -----> | dTD |----->| dTD |---....-->| dTD |---->
*                           +--|--+      +--|--+          +--|--+
*                              |            |                |
*                              V            V                V
*                          +--------+    +--------+       +--------+
*                          |  dTD   |    |  dTD   |       |  dTD   |
*                          | Buffer |    | Buffer |       | Buffer |
*                          +--------+    +--------+       +--------+
*                              0             1               N
*
*               after the USBD_OTGHS_dTD_LstRemove() is called the link list will look like:
*
*                      |--------------------|
*                      | |            |     V
*                      | |  +-----+   |  +-----+          +-----+
*     dTD_LstHeadPtr --| |  | dTD |---|->| dTD |---....-->| dTD |---->
*                        |  +--|--+   |  +--|--+          +--|--+
*                        |     |      |     |                |
*                        |     V      |     V                V
*                        | +--------+ |  +--------+       +--------+
*                        | |  dTD   | |  |  dTD   |       |  dTD   |
*                        | | Buffer | |  | Buffer |       | Buffer |
*                        | +--------+ |  +--------+       +--------+
*                        |     0'     |     0                N -1
*                        | return to  |
*                        | memory pool|
*
*                                    +-----------+   +----------+
*                p_ep_data --------->| EP Buffer | = |   dTD    |  ; *p_ep_trans_size = size of the dTD buffer.
*                                    |           |   |  Buffer  |
*                                    +-----------+   +----------+
*                                                          0'
*
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_OTGHS_dTD_LstRemove (USBD_DRV    *p_drv,
                                               CPU_INT08U   ep_phy_nbr)
{
    USBD_DRV_DATA       *p_drv_data;
    USBD_OTGHS_dQH      *p_dqh;
    USBD_OTGHS_dTD_EXT  *p_dtdlst_head;
    LIB_ERR              err_lib;
    CPU_SR_ALLOC();


    p_drv_data = (USBD_DRV_DATA *)(p_drv->DataPtr);
    p_dqh      = &p_drv_data->dQH_Tbl[ep_phy_nbr];

    CPU_DCACHE_RANGE_INV(p_dqh, sizeof(USBD_OTGHS_dQH));        /* Invalidate (clear) the cached RAM block, so that the */
                                                                /* next CPU read will be from RAM again.                */
    p_dtdlst_head = p_dqh->dTD_LstHeadPtr;

    CPU_CRITICAL_ENTER();
    if (p_dqh->dTD_LstNbrEntries == 1u) {
        p_dqh->dTD_LstHeadPtr    = (USBD_OTGHS_dTD_EXT *)USBD_OTGHS_dTD_dTD_NEXT_TERMINATE;
        p_dqh->dTD_LstTailPtr    = (USBD_OTGHS_dTD_EXT *)USBD_OTGHS_dTD_dTD_NEXT_TERMINATE;
        p_dqh->dTD_LstNbrEntries = 0u;
        OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].dTD_LstRemove_LstLastElementCnt);
    } else if (p_dqh->dTD_LstNbrEntries > 0u) {
        CPU_DCACHE_RANGE_INV(p_dqh->dTD_LstHeadPtr, sizeof(USBD_OTGHS_dTD_EXT));
        p_dqh->dTD_LstHeadPtr = (USBD_OTGHS_dTD_EXT *)p_dqh->dTD_LstHeadPtr->dTD_NextPtr;
        p_dqh->dTD_LstNbrEntries--;
        OTGHS_DBG_STATS_INC(EP_Tbl[ep_phy_nbr].dTD_LstRemove_LstNonEmptyCnt);
    }
#if (USBD_CFG_MAX_NBR_URB_EXTRA > 0u)
    (p_drv_data->dTD_UsageTbl[ep_phy_nbr])--;
#endif

    CPU_DCACHE_RANGE_FLUSH(p_dqh, sizeof(USBD_OTGHS_dQH));
    CPU_CRITICAL_EXIT();

    Mem_PoolBlkFree(       &p_drv_data->dTD_MemPool,            /* Return the head dTD to the memory pool               */
                    (void *)p_dtdlst_head,
                           &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
        return (DEF_FAIL);
    }

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                       USBD_OTGHS_SetupProcess()
*
* Description : Process setup request.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_log_nbr  Endpoint logical number.
*
* Return(s)   : none.
*
* Note(s)     : (1) NXP LPC313x/18xx/43xx USB device controller defines a register giving the IP version
*                   of the controller. According to the IP version, a different processing must be done
*                   for handling the setup packet. This is not required for the Xilinx Zynq-7000.
*********************************************************************************************************
*/

static  void  USBD_OTGHS_SetupProcess (USBD_DRV    *p_drv,
                                       CPU_INT08U   ep_log_nbr)
{
    USBD_DRV_DATA   *p_drv_data;
    USBD_OTGHS_REG  *p_reg;
    USBD_OTGHS_dQH  *p_dqh;
    CPU_INT08U       ep_phy_nbr;
    CPU_INT32U       reg_to;
    CPU_INT32U       setup_pkt[2u];
    CPU_BOOLEAN      sutw_set;


    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(USBD_EP_LOG_TO_ADDR_OUT(ep_log_nbr));
    sutw_set   =  DEF_FALSE;
    reg_to     =  USBD_OTGHS_REG_TO;
    p_drv_data = (USBD_DRV_DATA  *)(p_drv->DataPtr);
    p_reg      = (USBD_OTGHS_REG *)(p_drv->CfgPtr->BaseAddr);
    p_dqh      = &p_drv_data->dQH_Tbl[ep_phy_nbr];
    CPU_DCACHE_RANGE_INV(p_dqh, sizeof(USBD_OTGHS_dQH));

    if (p_drv_data->Ctrlr == USBD_NXP_OTGHS_CTRLR_LPCXX_USBX) { /* See Note #1.                                         */

        if (p_drv_data->hw_rev < USBD_LPC313X_IP_VER_2_3) {     /* Pre 2.3 hardware setup handling                      */
            OTGHS_DBG_STATS_INC(ISR_SetupProcess_VersionLessThan2_3Cnt);

            setup_pkt[0u] = p_dqh->SetupBuf[0u];                /* (1) Duplicate contents of dQH.SetupBuf               */
            setup_pkt[1u] = p_dqh->SetupBuf[1u];

            p_reg->ENDPTSETUPSTAT = DEF_BIT32(ep_log_nbr);      /* (2) Write '1' to clear the bit in ENDPSETUPSTAT      */
            USBD_EventSetup(         p_drv,
                            (void *)&setup_pkt);
            return;
        } else {
            OTGHS_DBG_STATS_INC(ISR_SetupProcess_VersionGreaterThan2_3Cnt);
        }
    }

    p_reg->ENDPTSETUPSTAT = DEF_BIT32(ep_log_nbr);              /* (1) Write '1' to clear the bit in ENDPSETUPSTAT      */

    while ((sutw_set == DEF_FALSE) &&
           (reg_to   >  0u)) {

        DEF_BIT_SET(p_reg->USBCMD, USBD_OTGHS_USBCMD_SUTW);     /* (2) Write '1' to setup Tripwire                      */

        setup_pkt[0u] = p_dqh->SetupBuf[0u];                    /* (3) Duplicate contents of dQH.SetupBuf               */
        setup_pkt[1u] = p_dqh->SetupBuf[1u];
                                                                /* (4) Read Setup TripWire. If the bi is set ....    */
                                                                /*     ... continue otherwise go to (2)              */
        sutw_set = DEF_BIT_IS_SET(p_reg->USBCMD, USBD_OTGHS_USBCMD_SUTW);
        reg_to--;
    }
    if (reg_to == 0u) {
        return;
    }

    reg_to = USBD_OTGHS_REG_TO;

    DEF_BIT_CLR(p_reg->USBCMD, USBD_OTGHS_USBCMD_SUTW);         /* (5) Write '0' to clear setup Tripwire                */

    while ((DEF_BIT_IS_SET(p_reg->ENDPTSETUPSTAT, DEF_BIT32(ep_log_nbr)) == DEF_YES) &&
           (reg_to > 0u)) {
        reg_to--;
    }

    if (reg_to == 0u) {
        return;
    }

    USBD_EventSetup(         p_drv,
                    (void *)&setup_pkt);
}


/*
*********************************************************************************************************
*                                         USBD_OTGHS_SoftRst()
*
* Description : Perform a soft reset :
*                   (1) Initializes the dQH data structure for all endpoints.
*                   (2) Creates dTD link list for endpoint 0
*                   (3) Initializes the dTD data structure for all endpoints.
*                   (4) Disable all endpoints except endpoint 0
*                   (5) Clear all setup token semaphores by reading ENDPTSETUPSTAT register and writing
*                       the same value back to the ENDPTSETUPSTAT register.
*                   (6) Clear all endpoint complete status bits by reading the ENDPTCOMPLETE register
*                       and writing the same value back to the ENDPTCOMPLETE register.
*                   (7) Cancel all prime status by waiting until all bits in the ENDPRIME are 0 and the writing
*                       0xFFFFFFFF to ENDPTFLUSH.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_OTGHS_SoftRst (USBD_DRV  *p_drv)
{
    USBD_DRV_DATA   *p_drv_data;
    USBD_OTGHS_REG  *p_reg;
    CPU_INT32U       reg_to;
    CPU_INT08U       ep_phy_nbr;
    CPU_INT08U       ep_phy_nbr_max;
    CPU_INT08U       ep_log_nbr;


    p_reg          = (USBD_OTGHS_REG *)(p_drv->CfgPtr->BaseAddr);
    p_drv_data     = (USBD_DRV_DATA  *)(p_drv->DataPtr);
    ep_phy_nbr_max =  USBD_EP_MaxPhyNbrGet(p_drv->DevNbr);

    if (ep_phy_nbr_max != USBD_EP_PHY_NONE) {
        for (ep_phy_nbr = 0u; ep_phy_nbr < ep_phy_nbr_max; ep_phy_nbr++) {

            USBD_OTGHS_dTD_LstEmpty(p_drv, ep_phy_nbr);

            p_drv_data->dQH_Tbl[ep_phy_nbr].EpCap                = 0u;
            p_drv_data->dQH_Tbl[ep_phy_nbr].OverArea.dTD_NextPtr = USBD_OTGHS_dTD_dTD_NEXT_TERMINATE;

            Mem_Clr((void *)&p_drv_data->dQH_Tbl[ep_phy_nbr].OverArea.Token,
                           (sizeof(USBD_OTGHS_dQH) - 2u));

            CPU_DCACHE_RANGE_FLUSH(&(p_drv_data->dQH_Tbl[ep_phy_nbr]), sizeof(USBD_OTGHS_dQH));

            ep_log_nbr = USBD_EP_PHY_TO_LOG(ep_phy_nbr);
            if ((ep_log_nbr != 0u) &&
                (DEF_BIT_IS_SET(p_reg->USBCMD, USBD_OTGHS_USBCMD_RUN) == DEF_YES)) {
                p_reg->ENDPTCTRLx[ep_log_nbr] = USBD_OTGHS_ENDPTCTRL_TX_TOGGLE_RST |
                                                USBD_OTGHS_ENDPTCTRL_RX_TOGGLE_RST;
            }
        }
    }

    reg_to = USBD_OTGHS_REG_TO;
    while ((p_reg->ENDPTPRIME != DEF_BIT_NONE) &&
           (reg_to            >  0u)) {
        reg_to--;
    }

    p_reg->ENDPTFLUSH = USBD_OTGHS_ENDPTxxxx_TX_MASK |
                        USBD_OTGHS_ENDPTxxxx_RX_MASK;

    reg_to = USBD_OTGHS_REG_TO;
    while ((p_reg->ENDPTSTATUS != DEF_BIT_NONE) &&
           (reg_to             >  0u)) {
        p_reg->ENDPTFLUSH = USBD_OTGHS_ENDPTxxxx_TX_MASK |
                            USBD_OTGHS_ENDPTxxxx_RX_MASK;
        reg_to--;
    }

    if (DEF_BIT_IS_SET(p_reg->USBCMD,  USBD_OTGHS_USBCMD_RUN) == DEF_YES) {
        p_reg->DEV_ADDR = DEF_BIT_NONE;
    }
}
