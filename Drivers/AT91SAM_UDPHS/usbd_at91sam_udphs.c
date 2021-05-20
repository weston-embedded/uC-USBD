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
*                                            AT91SAM-UDPHS
*
* Filename : usbd_at91sam_udphs.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)  : (1) You can find specific information about this driver at:
*                https://doc.micrium.com/display/USBDDRV/AT91SAM_UDPHS
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../../Source/usbd_core.h"
#include  "usbd_at91sam_udphs.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  USBD_DBG_DRV(msg)                           USBD_DBG_GENERIC((msg),                                            \
                                                                      USBD_EP_NBR_NONE,                                 \
                                                                      USBD_IF_NBR_NONE)

#define  USBD_DBG_DRV_EP(msg, ep_addr)               USBD_DBG_GENERIC((msg),                                            \
                                                                      (ep_addr),                                        \
                                                                       USBD_IF_NBR_NONE)

#define  USBD_DBG_DRV_EP_ARG(msg, ep_addr, arg)      USBD_DBG_GENERIC_ARG((msg),                                        \
                                                                          (ep_addr),                                    \
                                                                           USBD_IF_NBR_NONE,                            \
                                                                          (arg))


/*
*********************************************************************************************************
*                     USB HIGH SPEED DEVICE PORT (UDPHS) USER INTERFACE CONSTRAINS
*********************************************************************************************************
*/

#define  USBD_AT91SAM_UDPHS_NBR_EPS                         7   /* Maximum number of endpoints.                         */
#define  USBD_AT91SAM_UDPHS_NBR_DMA_CH                      6   /* Number of DMA channels.                              */

#define  USBD_AT91SAM_UDPHS_EP_SIZE_MAX                  1024   /* Maximum EP size.                                     */
#define  USBD_AT91SAM_UDPHS_EP_BUF_DATA_SIZE       (64 * 1024)  /* 64KB logical address space.                          */

#define  USBD_AT91SAM_UDPHS_REG_TO                 0x0000FFFF


/*
*********************************************************************************************************
*                                       ENDPOINTS GENERIC DEFINES
*********************************************************************************************************
*/

#define  USBD_AT91SAM_UDPHS_EP_SIZE_8                       8
#define  USBD_AT91SAM_UDPHS_EP_SIZE_16                     16
#define  USBD_AT91SAM_UDPHS_EP_SIZE_32                     32
#define  USBD_AT91SAM_UDPHS_EP_SIZE_64                     64
#define  USBD_AT91SAM_UDPHS_EP_SIZE_128                   128
#define  USBD_AT91SAM_UDPHS_EP_SIZE_256                   256
#define  USBD_AT91SAM_UDPHS_EP_SIZE_512                   512
#define  USBD_AT91SAM_UDPHS_EP_SIZE_1024                 1024


/*
*********************************************************************************************************
*                                         REGISTER BIT DEFINES
*********************************************************************************************************
*/
                                                                /* --------- UDPHS ENPOINT CONTROL BIT DEFINES -------- */
#define  USBD_AT91SAM_UDPHS_EPTCTLx_EPT_ENABL      DEF_BIT_00   /* Endpoint enable.                                     */
#define  USBD_AT91SAM_UDPHS_EPTCTLx_AUTO_VALID     DEF_BIT_01   /* Packet   Auto-Valid.                                 */
#define  USBD_AT91SAM_UDPHS_EPTCTLx_INTDIS_DMA     DEF_BIT_03   /* Interrupts Disable DMA.                              */
#define  USBD_AT91SAM_UDPHS_EPTCTLx_NYET_DIS       DEF_BIT_04   /* NYET Disable.                                        */
#define  USBD_AT91SAM_UDPHS_EPTCTLx_DATA_RX        DEF_BIT_06   /* DATAx Interrupt Enable.                              */
#define  USBD_AT91SAM_UDPHS_EPTCTLx_MDATA_RX       DEF_BIT_07   /* MDATAx Interrupt Enable.                             */
#define  USBD_AT91SAM_UDPHS_EPTCTLx_ERR_OVFLW      DEF_BIT_08   /* Overflow Error  Interrupt Enable.                    */
#define  USBD_AT91SAM_UDPHS_EPTCTLx_RX_BK_RDY      DEF_BIT_09   /* Received OUT Data Interrupt Enable.                  */
#define  USBD_AT91SAM_UDPHS_EPTCTLx_TX_COMPLT      DEF_BIT_10   /* Transmitted IN Data Complete Interrupt Enable.       */
#define  USBD_AT91SAM_UDPHS_EPTCTLx_TX_PK_RDY      DEF_BIT_11   /* Tx Packet Ready/Transaction Error Interrupt Enable.  */
#define  USBD_AT91SAM_UDPHS_EPTCTLx_RX_SETUP       DEF_BIT_12   /* Received SETUP/Error Flow Interrupt Enable.          */
#define  USBD_AT91SAM_UDPHS_EPTCTLx_STALL_SNT      DEF_BIT_13   /* Stall Sent/CRC error/# trans. Interrupt Enable.      */
#define  USBD_AT91SAM_UDPHS_EPTCTLx_NAK_IN         DEF_BIT_14   /* NAKIN/Bank Flush Error Interrupt Enable.             */
#define  USBD_AT91SAM_UDPHS_EPTCTLx_NAK_OUT        DEF_BIT_15   /* NAK out Interrupt Enable.                            */
#define  USBD_AT91SAM_UDPHS_EPTCTLx_BUSY_BANK      DEF_BIT_18   /* Busy Bank Interrupt Enable.                          */
#define  USBD_AT91SAM_UDPHS_EPTCTLx_SHRT_PCKT      DEF_BIT_31   /* Short Packet Send/Short Packet Interrupt Enable.     */

#define  USBD_AT91SAM_UDPHS_EPTCTLx_ALL           (USBD_AT91SAM_UDPHS_EPTCTLx_EPT_ENABL  | \
                                                   USBD_AT91SAM_UDPHS_EPTCTLx_AUTO_VALID | \
                                                   USBD_AT91SAM_UDPHS_EPTCTLx_INTDIS_DMA | \
                                                   USBD_AT91SAM_UDPHS_EPTCTLx_NYET_DIS   | \
                                                   USBD_AT91SAM_UDPHS_EPTCTLx_DATA_RX    | \
                                                   USBD_AT91SAM_UDPHS_EPTCTLx_MDATA_RX   | \
                                                   USBD_AT91SAM_UDPHS_EPTCTLx_ERR_OVFLW  | \
                                                   USBD_AT91SAM_UDPHS_EPTCTLx_RX_BK_RDY  | \
                                                   USBD_AT91SAM_UDPHS_EPTCTLx_TX_COMPLT  | \
                                                   USBD_AT91SAM_UDPHS_EPTCTLx_TX_PK_RDY  | \
                                                   USBD_AT91SAM_UDPHS_EPTCTLx_RX_SETUP   | \
                                                   USBD_AT91SAM_UDPHS_EPTCTLx_STALL_SNT  | \
                                                   USBD_AT91SAM_UDPHS_EPTCTLx_NAK_IN     | \
                                                   USBD_AT91SAM_UDPHS_EPTCTLx_NAK_OUT    | \
                                                   USBD_AT91SAM_UDPHS_EPTCTLx_BUSY_BANK  | \
                                                   USBD_AT91SAM_UDPHS_EPTCTLx_SHRT_PCKT)

                                                                /* ------ UDPHS INTERRUPT REGISTERS BIT DEFINES ------- */
#define  USBD_AT91SAM_UDPHS_INT_SPEED              DEF_BIT_00   /* Speed status.                                        */
#define  USBD_AT91SAM_UDPHS_INT_DET_SUSPD          DEF_BIT_01   /* Suspend         Interrupt.                           */
#define  USBD_AT91SAM_UDPHS_INT_MICRO_SOF          DEF_BIT_02   /* Micro-SOF       Interrupt.                           */
#define  USBD_AT91SAM_UDPHS_INT_SOF                DEF_BIT_03   /* SOF             Interrupt.                           */
#define  USBD_AT91SAM_UDPHS_INT_ENDRESET           DEF_BIT_04   /* End of Reset    Interrupt.                           */
#define  USBD_AT91SAM_UDPHS_INT_WAKE_UP            DEF_BIT_05   /* Wake UP CPU     Interrupt.                           */
#define  USBD_AT91SAM_UDPHS_INT_ENDOFRSM           DEF_BIT_06   /* End of Resume   Interrupt.                           */
#define  USBD_AT91SAM_UDPHS_INT_UPSTR_RES          DEF_BIT_07   /* Upstream Resume Interrupt.                           */
#define  USBD_AT91SAM_UDPHS_INT_EPT_0              DEF_BIT_08   /* Endpoint 0      Interrupt.                           */
#define  USBD_AT91SAM_UDPHS_INT_EPT_1              DEF_BIT_09   /* Endpoint 1      Interrupt.                           */
#define  USBD_AT91SAM_UDPHS_INT_EPT_2              DEF_BIT_10   /* Endpoint 2      Interrupt.                           */
#define  USBD_AT91SAM_UDPHS_INT_EPT_3              DEF_BIT_11   /* Endpoint 3      Interrupt.                           */
#define  USBD_AT91SAM_UDPHS_INT_EPT_4              DEF_BIT_12   /* Endpoint 4      Interrupt.                           */
#define  USBD_AT91SAM_UDPHS_INT_EPT_5              DEF_BIT_13   /* Endpoint 5      Interrupt.                           */
#define  USBD_AT91SAM_UDPHS_INT_EPT_6              DEF_BIT_14   /* Endpoint 6      Interrupt.                           */

#define  USBD_AT91SAM_UDPHS_INT_EPT_x             (USBD_AT91SAM_UDPHS_INT_EPT_0     | \
                                                   USBD_AT91SAM_UDPHS_INT_EPT_1     | \
                                                   USBD_AT91SAM_UDPHS_INT_EPT_2     | \
                                                   USBD_AT91SAM_UDPHS_INT_EPT_3     | \
                                                   USBD_AT91SAM_UDPHS_INT_EPT_4     | \
                                                   USBD_AT91SAM_UDPHS_INT_EPT_5     | \
                                                   USBD_AT91SAM_UDPHS_INT_EPT_6)

#define  USBD_AT91SAM_UDPHS_INT_DMA_1              DEF_BIT_25   /* DMA Channel 1   Interrupt.                           */
#define  USBD_AT91SAM_UDPHS_INT_DMA_2              DEF_BIT_26   /* DMA Channel 2   Interrupt.                           */
#define  USBD_AT91SAM_UDPHS_INT_DMA_3              DEF_BIT_27   /* DMA Channel 3   Interrupt.                           */
#define  USBD_AT91SAM_UDPHS_INT_DMA_4              DEF_BIT_28   /* DMA Channel 4   Interrupt.                           */
#define  USBD_AT91SAM_UDPHS_INT_DMA_5              DEF_BIT_29   /* DMA Channel 5   Interrupt.                           */
#define  USBD_AT91SAM_UDPHS_INT_DMA_6              DEF_BIT_30   /* DMA Channel 6   Interrupt.                           */

#define  USBD_AT91SAM_UDPHS_INT_DMA_x             (USBD_AT91SAM_UDPHS_INT_DMA_1     | \
                                                   USBD_AT91SAM_UDPHS_INT_DMA_2     | \
                                                   USBD_AT91SAM_UDPHS_INT_DMA_3     | \
                                                   USBD_AT91SAM_UDPHS_INT_DMA_4     | \
                                                   USBD_AT91SAM_UDPHS_INT_DMA_5     | \
                                                   USBD_AT91SAM_UDPHS_INT_DMA_6)

#define  USBD_AT91SAM_UDPHS_INT_ALL               (USBD_AT91SAM_UDPHS_INT_DET_SUSPD | \
                                                   USBD_AT91SAM_UDPHS_INT_MICRO_SOF | \
                                                   USBD_AT91SAM_UDPHS_INT_SOF       | \
                                                   USBD_AT91SAM_UDPHS_INT_ENDRESET  | \
                                                   USBD_AT91SAM_UDPHS_INT_WAKE_UP   | \
                                                   USBD_AT91SAM_UDPHS_INT_ENDOFRSM  | \
                                                   USBD_AT91SAM_UDPHS_INT_UPSTR_RES | \
                                                   USBD_AT91SAM_UDPHS_INT_EPT_x     | \
                                                   USBD_AT91SAM_UDPHS_INT_DMA_x)

#define  USBD_AT91SAM_UDPHS_INT_BUS_STATUS        (USBD_AT91SAM_UDPHS_INT_DET_SUSPD | \
                                                   USBD_AT91SAM_UDPHS_INT_ENDRESET  | \
                                                   USBD_AT91SAM_UDPHS_INT_RESUME)

#define  USBD_AT91SAM_UDPHS_INT_RESUME            (USBD_AT91SAM_UDPHS_INT_WAKE_UP   | \
                                                   USBD_AT91SAM_UDPHS_INT_ENDOFRSM  | \
                                                   USBD_AT91SAM_UDPHS_INT_UPSTR_RES)

                                                                /* -------- UDPHS CONTROL REGISTER BIT DEFINES -------- */
#define  USBD_AT91SAM_UDPHS_CTRL_DEV_ADDR_MASK     0x0000007Fu  /* UDPHS Adress Mask.                                   */
#define  USBD_AT91SAM_UDPHS_CTRL_FADDR_EN          DEF_BIT_07   /* Function Addres Enable.                              */
#define  USBD_AT91SAM_UDPHS_CTRL_EN_UDPHS          DEF_BIT_08   /* UDPHS Enable.                                        */
#define  USBD_AT91SAM_UDPHS_CTRL_DETACH            DEF_BIT_09   /* Detach Command.                                      */
#define  USBD_AT91SAM_UDPHS_CTRL_REWAKEUP          DEF_BIT_10   /* Send Remote Wake Up.                                 */
#define  USBD_AT91SAM_UDPHS_CTRL_PULLD_DIS         DEF_BIT_11   /* Pull-Down Disable.                                   */


                                                                /* ---- UDPHS ENDPOINTS RESET REGISTER BIT DEFINES ---- */
#define  USBD_AT91SAM_UDPHSR64_UDPHS_EPTRST_EPT_0  DEF_BIT_00
#define  USBD_AT91SAM_UDPHSR64_UDPHS_EPTRST_EPT_1  DEF_BIT_01
#define  USBD_AT91SAM_UDPHSR64_UDPHS_EPTRST_EPT_2  DEF_BIT_02
#define  USBD_AT91SAM_UDPHSR64_UDPHS_EPTRST_EPT_3  DEF_BIT_03
#define  USBD_AT91SAM_UDPHSR64_UDPHS_EPTRST_EPT_4  DEF_BIT_04
#define  USBD_AT91SAM_UDPHSR64_UDPHS_EPTRST_EPT_5  DEF_BIT_05
#define  USBD_AT91SAM_UDPHSR64_UDPHS_EPTRST_EPT_6  DEF_BIT_06

#define  USBD_AT91SAM_UDPHSR64_UDPHS_EPTRST_EPT_ALL \
                                                  (USBD_AT91SAM_UDPHSR64_UDPHS_EPTRST_EPT_0    | \
                                                   USBD_AT91SAM_UDPHSR64_UDPHS_EPTRST_EPT_1    | \
                                                   USBD_AT91SAM_UDPHSR64_UDPHS_EPTRST_EPT_2    | \
                                                   USBD_AT91SAM_UDPHSR64_UDPHS_EPTRST_EPT_3    | \
                                                   USBD_AT91SAM_UDPHSR64_UDPHS_EPTRST_EPT_4    | \
                                                   USBD_AT91SAM_UDPHSR64_UDPHS_EPTRST_EPT_5    | \
                                                   USBD_AT91SAM_UDPHSR64_UDPHS_EPTRST_EPT_6)

                                                                /* ----------- UDPHS ENDPOINTS STATUS REGISTER -------- */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_FRCESTALL      DEF_BIT_05   /* Stall Handshake Request.                             */
                                                                /* Toggle Sequencing.                                   */

#define  USBD_AT91SAM_UDPHS_EPTSTAx_TOGGLE_MASK    DEF_BIT_06 | \
                                                   DEF_BIT_07)

#define  USBD_AT91SAM_UDPHS_EPTSTAx_ERR_OVFLW      DEF_BIT_08   /* Overflow Error.                                      */

#define  USBD_AT91SAM_UDPHS_EPTSTAx_RX_BK_RDY      DEF_BIT_09   /* Received OUT Data.                                   */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_KILL_BANK      DEF_BIT_09   /* KILL Bank.                                           */

#define  USBD_AT91SAM_UDPHS_EPTSTAx_TX_COMPLT      DEF_BIT_10   /* Transmitted IN data complete.                        */

#define  USBD_AT91SAM_UDPHS_EPTSTAx_TX_PK_RDY      DEF_BIT_11   /* Tx packet ready.                                     */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_ERR_TRANS      DEF_BIT_11   /* Transaction Error.                                   */

#define  USBD_AT91SAM_UDPHS_EPTSTAx_RX_SETUP       DEF_BIT_12   /* Received Setup.                                      */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_ERR_FL_ISO     DEF_BIT_12   /* Error Flow.                                          */

#define  USBD_AT91SAM_UDPHS_EPTSTAx_STALL_SNT      DEF_BIT_13   /* STALL sent.                                          */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_ERR_CRISO      DEF_BIT_13   /* CRC ISO error.                                       */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_ERR_NBTRA      DEF_BIT_13   /* Number of Transaction.                               */

#define  USBD_AT91SAM_UDPHS_EPTSTAx_NAK_IN         DEF_BIT_14   /* NAK IN.                                              */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_ERR_FLUSH      DEF_BIT_14   /* Bank Flush Error.                                    */

#define  USBD_AT91SAM_UDPHS_EPTSTAx_NAK_OUT        DEF_BIT_15   /* NAK OUT.                                             */

#define  USBD_AT91SAM_UDPHS_EPTSTAx_CTRLDIR_RD     DEF_BIT_16   /* A Control Read is requested by the host.             */
                                                                /* Current Bank Mask.                                   */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_CURBANK_MASK  (DEF_BIT_16 | \
                                                   DEF_BIT_17)
                                                                /* Bank 0 or Single Bank.                               */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_CURBANK_0      DEF_BIT_NONE

#define  USBD_AT91SAM_UDPHS_EPTSTAx_CURBANK_1      DEF_BIT_16   /* Bank 1.                                              */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_CURBANK_2      DEF_BIT_17   /* Bank 2.                                              */

                                                                /* Busy Bank Mask.                                      */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_BUSYBANK_MASK (DEF_BIT_18 | \
                                                   DEF_BIT_19)

#define  USBD_AT91SAM_UDPHS_EPTSTAx_BUSYBANK_0     DEF_BIT_NONE
#define  USBD_AT91SAM_UDPHS_EPTSTAx_BUSYBANK_1     DEF_BIT_18   /* 1 Busy Bank.                                         */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_BUSYBANK_2     DEF_BIT_19   /* 2 Busy Banks.                                        */
                                                                /* 3 Busy Banks.                                        */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_BUSYBANK_3    (DEF_BIT_18 | \
                                                   DEF_BIT_19)

#define  USBD_AT91SAM_UDPHS_EPTSTAx_BYTE_CNT_MASK  0x7FF00000   /* UDPHS Byte Count.                                    */

#define  USBD_AT91SAM_UDPHS_EPTSTAx_SHRT_PCKT      DEF_BIT_31   /* Short Packet.                                        */

                                                                /* -------- UDPHS ENDPOINTS SET STATUS REGISTER ------- */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_SET_FRCESTALL  DEF_BIT_05   /* Stall Handshake Request.                             */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_SET_KILL_BANK  DEF_BIT_09   /* Kill the last written bank.                          */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_SET_TX_PK_RDY  DEF_BIT_11   /* Tx Packet Ready set.                                 */

                                                                /* ------- UDPHS ENDPOINTS CLEAR STATUS REGISTER ------ */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_CLR_FRCESTALL  DEF_BIT_05   /* Clear the STALL request.                             */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_CLR_TOGGLESQ   DEF_BIT_06   /* Data Toggle Clear.                                   */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_CLR_RX_BK_RDY  DEF_BIT_09   /* Receive OUT Data clear.                              */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_CLR_TX_COMPLT  DEF_BIT_10   /* Transmitted IN Data Complete clear.                  */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_CLR_RX_SETUP   DEF_BIT_12   /* Receive setup clear.                                 */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_CLR_STALL_SNT  DEF_BIT_13   /* Stall Sent clear.                                    */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_CLR_NAK_IN     DEF_BIT_14   /* NAK IN  clear.                                       */
#define  USBD_AT91SAM_UDPHS_EPTSTAx_CLR_NAK_OUT    DEF_BIT_15   /* NAK OUT clear.                                       */

                                                                /* - UDPHS ENDPOINT CONFIGURATION REGISTER BIT DEFINES  */
#define  USBD_AT91SAM_UDPHS_EPTCFGx_EPT_SIZE_8           0x00
#define  USBD_AT91SAM_UDPHS_EPTCFGx_EPT_SIZE_16          0x01
#define  USBD_AT91SAM_UDPHS_EPTCFGx_EPT_SIZE_32          0x02
#define  USBD_AT91SAM_UDPHS_EPTCFGx_EPT_SIZE_64          0x03
#define  USBD_AT91SAM_UDPHS_EPTCFGx_EPT_SIZE_128         0x04
#define  USBD_AT91SAM_UDPHS_EPTCFGx_EPT_SIZE_256         0x05
#define  USBD_AT91SAM_UDPHS_EPTCFGx_EPT_SIZE_512         0x06
#define  USBD_AT91SAM_UDPHS_EPTCFGx_EPT_SIZE_1024        0x07

#define  USBD_AT91SAM_UDPHS_EPTCFGx_EPT_DIR        DEF_BIT_03
#define  USBD_AT91SAM_UDPHS_EPTCFGx_EPT_TYPE_MASK \
                                                  (DEF_BIT_04 | \
                                                   DEF_BIT_05)
#define  USBD_AT91SAM_UDPHS_EPTCFGx_EPT_TYPE_CTRL  DEF_BIT_NONE
#define  USBD_AT91SAM_UDPHS_EPTCFGx_EPT_TYPE_ISO   DEF_BIT_04
#define  USBD_AT91SAM_UDPHS_EPTCFGx_EPT_TYPE_BULK  DEF_BIT_05
#define  USBD_AT91SAM_UDPHS_EPTCFGx_EPT_TYPE_INT  (DEF_BIT_04 | \
                                                   DEF_BIT_05)

#define  USBD_AT91SAM_UDPHS_EPTCFGx_BK_NUMBER_MASK \
                                                  (DEF_BIT_06 | \
                                                   DEF_BIT_07)
#define  USBD_AT91SAM_UDPHS_EPTCFGx_BK_NUMBER_0    DEF_BIT_NONE
#define  USBD_AT91SAM_UDPHS_EPTCFGx_BK_NUMBER_1    DEF_BIT_06
#define  USBD_AT91SAM_UDPHS_EPTCFGx_BK_NUMBER_2    DEF_BIT_07
#define  USBD_AT91SAM_UDPHS_EPTCFGx_BK_NUMBER_3   (DEF_BIT_06 | \
                                                   DEF_BIT_07)

#define  USBD_AT91SAM_UDPHS_EPTCFGx_EPT_MAPD       DEF_BIT_31


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

typedef struct usbd_at91sam_udphs_ep_reg {                      /* ---------------- ENDPOINT REGISTERS  --------------- */
    CPU_REG32  EPTCFGx;                                         /* Endpoint Configuration.                              */
    CPU_REG32  EPTCTLENBx;                                      /* Endpoint Enable.                                     */
    CPU_REG32  EPTCTLDISx;                                      /* Endpoint Disable.                                    */
    CPU_REG32  EPTCTLx;                                         /* Endpoint Control.                                    */
    CPU_REG32  RESERVED;
    CPU_REG32  EPTSETSTAx;                                      /* Endpoint Status.                                     */
    CPU_REG32  EPTCLRSTAx;                                      /* Endpoint Clear Status.                               */
    CPU_REG32  EPTSTAx;                                         /* Endpoint Set   Status.                               */
}  USBD_AT91SAM_UDPHS_EP_REG;

typedef struct usbd_at91sam_udphs_dma_reg {                     /* ------------------- DMA REGISTERS  ----------------- */
    CPU_REG32  DMANXTDSCx;                                      /* DMA Next Descriptor.                                 */
    CPU_REG32  DMAADDRESSx;                                     /* DMA Address.                                         */
    CPU_REG32  DMACONTROLx;                                     /* DMA Control.                                         */
    CPU_REG32  DMASTATUSx;                                      /* DMA Status.                                          */
}  USBD_AT91SAM_UDPHS_EP_DMA_REG;

typedef  struct  usbd_at91sam_udphs_reg {                       /* ---- USB HIGH SPEED DEVICE PORT(UDPHS) USER IF  ---- */
    CPU_REG32                      UDPHS_CTRL;                  /* UDPHS Control.                                       */
    CPU_REG32                      UDPHS_FNUM;                  /* Frame Number.                                        */
    CPU_REG32                      RESERVED0[2];
    CPU_REG32                      UDPHS_IEN;                   /* Interrupt Enable.                                    */
    CPU_REG32                      UDPHS_INTSTA;                /* Interrupt Status.                                    */
    CPU_REG32                      UDPHS_CLRINT;                /* Clear Interrupt.                                     */
    CPU_REG32                      UDPHS_EPTRST;                /* Enpoint Reset.                                       */
    CPU_REG32                      RESERVED1[48];
    CPU_REG32                      UDPHS_TST;                   /* Test.                                                */
    CPU_REG32                      RESERVED2[2];
    CPU_REG32                      UDPHS_IPADDRSIZE;            /* PADDRSIZE.                                           */
    CPU_REG32                      UDPHS_IPNAME1;               /* Name 2.                                              */
    CPU_REG32                      UDPHS_IPNAME2;               /* Name 2.                                              */
    CPU_REG32                      UDPHS_IPFEAUTURES;           /* Features.                                            */
    CPU_REG32                      RESERVED3;
                                                                /* Endpoint 0 to 6 Register.                            */
    USBD_AT91SAM_UDPHS_EP_REG      UDPHS_EP_REG[USBD_AT91SAM_UDPHS_NBR_EPS];
    CPU_REG32                      RESERVED4[76];
                                                                /* DMA channel 1 to 5 Registers.                        */
    USBD_AT91SAM_UDPHS_EP_DMA_REG  UDPHS_DMA_REG[USBD_AT91SAM_UDPHS_NBR_DMA_CH];
}  USBD_AT91SAM_UDPHS_REG;

typedef struct  usbd_at91sam_udphs_ep_buf {
    CPU_INT08U                     EpBufPhy[USBD_AT91SAM_UDPHS_EP_BUF_DATA_SIZE];
} USBD_AT91SAM_UDPHS_EP_BUF;

typedef struct  usbd_at91sam_udphs_ep_dpram {
    USBD_AT91SAM_UDPHS_EP_BUF      EpBufLog[USBD_AT91SAM_UDPHS_NBR_EPS];
} USBD_AT91SAM_UDPHS_EP_DPRAM;


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

static  CPU_INT08U   BulkRxEP_LogNbr;
static  CPU_BOOLEAN  BulkRxClrFlagPending;


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

static  void         USBD_AT91SAM_UDPHS_EP_Process(USBD_DRV    *p_drv,
                                                   CPU_INT08U   ep_log_nbr);


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

USBD_DRV_API  USBD_DrvAPI_AT91SAM_UDPHS = { USBD_DrvInit,
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
    USBD_AT91SAM_UDPHS_REG  *p_reg;
    USBD_DRV_BSP_API        *p_bsp_api;
    CPU_INT08U               i;


                                                                /* Get device controller registers reference.           */
    p_reg     = (USBD_AT91SAM_UDPHS_REG *)p_drv->CfgPtr->BaseAddr;
    p_bsp_api =  p_drv->BSP_API_Ptr;                            /* Get driver BSP API reference.                        */

    if (p_bsp_api->Init != (void *)0) {
        p_bsp_api->Init(p_drv);                                 /* Call board/chip specific device controller ...       */
                                                                /* ... initialization function.                         */
    }
                                                                /* Disable and reset the UDPHS controller.              */
    p_reg->UDPHS_CTRL = DEF_BIT_NONE;
                                                                /* Enable the UDPHS controller.                         */
    p_reg->UDPHS_CTRL = USBD_AT91SAM_UDPHS_CTRL_EN_UDPHS |
                        USBD_AT91SAM_UDPHS_CTRL_DETACH;

    DEF_BIT_CLR(p_reg->UDPHS_IEN, USBD_AT91SAM_UDPHS_INT_ALL);  /* Disable all interrupts.                              */
    p_reg->UDPHS_CLRINT = USBD_AT91SAM_UDPHS_INT_ALL;           /* Clear all pending interrupts.                        */

                                                                /* Restart the Endpoint registers.                      */
    p_reg->UDPHS_EPTRST = USBD_AT91SAM_UDPHSR64_UDPHS_EPTRST_EPT_ALL;

    for (i = 0; i < USBD_AT91SAM_UDPHS_NBR_EPS; i++) {
        p_reg->UDPHS_EP_REG[i].EPTCTLDISx = USBD_AT91SAM_UDPHS_EPTCTLx_ALL;
        p_reg->UDPHS_EP_REG[i].EPTCLRSTAx = USBD_AT91SAM_UDPHS_EPTSTAx_CLR_TOGGLESQ;
    }
                                                                /* Disable DMA.                                         */
    for (i = 0; i < USBD_AT91SAM_UDPHS_NBR_DMA_CH; i++) {
        p_reg->UDPHS_DMA_REG[i].DMACONTROLx = 0;
        p_reg->UDPHS_DMA_REG[i].DMACONTROLx = 0x02;
        p_reg->UDPHS_DMA_REG[i].DMACONTROLx = 0;
        p_reg->UDPHS_DMA_REG[i].DMASTATUSx  = p_reg->UDPHS_DMA_REG[i].DMASTATUSx;
    }

    BulkRxClrFlagPending = DEF_FALSE;

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
* Note(s)     : Typically, the start function activates the pull-up on the D+ or D- pin to simulate
*               attachment to host. Some MCUs/MPUs have an internal pull-down that is activated by a
*               device controller register; for others, this may be a GPIO pin. Additionally, interrupts
*               for reset and suspend are activated.
*********************************************************************************************************
*/

static  void  USBD_DrvStart (USBD_DRV  *p_drv,
                             USBD_ERR  *p_err)
{
    USBD_AT91SAM_UDPHS_REG  *p_reg;
    USBD_DRV_BSP_API        *p_bsp_api;


    p_reg     = (USBD_AT91SAM_UDPHS_REG *)p_drv->CfgPtr->BaseAddr;
    p_bsp_api =  p_drv->BSP_API_Ptr;                            /* Get driver BSP API reference.                        */

    if (p_bsp_api->Conn != (void *)0) {
        p_bsp_api->Conn();                                      /* Call board/chip specific connect function.           */
    }
                                                                /* Activate interrupts for reset and suspend.           */
    DEF_BIT_SET(p_reg->UDPHS_IEN, USBD_AT91SAM_UDPHS_INT_BUS_STATUS);
                                                                /* Attach the device and disable pulldown.              */
    DEF_BIT_CLR(p_reg->UDPHS_CTRL, USBD_AT91SAM_UDPHS_CTRL_DETACH);
    DEF_BIT_SET(p_reg->UDPHS_CTRL, USBD_AT91SAM_UDPHS_CTRL_PULLD_DIS);

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
    USBD_AT91SAM_UDPHS_REG  *p_reg;
    USBD_DRV_BSP_API        *p_bsp_api;


    p_reg     = (USBD_AT91SAM_UDPHS_REG *)p_drv->CfgPtr->BaseAddr;
    p_bsp_api =  p_drv->BSP_API_Ptr;                            /* Get driver BSP API reference.                        */

    if (p_bsp_api->Disconn != (void *)0) {
        p_bsp_api->Disconn();
    }
                                                                /* Clear and disable USB interrupts.                    */
    DEF_BIT_CLR(p_reg->UDPHS_IEN, USBD_AT91SAM_UDPHS_INT_BUS_STATUS);
                                                                /* Detach the device and enable pulldown.               */
    DEF_BIT_SET(p_reg->UDPHS_CTRL, USBD_AT91SAM_UDPHS_CTRL_DETACH);
    DEF_BIT_CLR(p_reg->UDPHS_CTRL, USBD_AT91SAM_UDPHS_CTRL_PULLD_DIS);
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
    USBD_AT91SAM_UDPHS_REG  *p_reg;


    p_reg = (USBD_AT91SAM_UDPHS_REG *)p_drv->CfgPtr->BaseAddr;

                                                                /* Set the device address.                              */
    DEF_BIT_CLR(p_reg->UDPHS_CTRL,  USBD_AT91SAM_UDPHS_CTRL_DEV_ADDR_MASK);
    DEF_BIT_SET(p_reg->UDPHS_CTRL, (dev_addr & USBD_AT91SAM_UDPHS_CTRL_DEV_ADDR_MASK));

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
    USBD_AT91SAM_UDPHS_REG  *p_reg;


    (void)dev_addr;

    p_reg = (USBD_AT91SAM_UDPHS_REG *)p_drv->CfgPtr->BaseAddr;  /* Get dev ctrl reg ref.                                */
                                                                /* Enable Function Address.                             */
    DEF_BIT_SET(p_reg->UDPHS_CTRL, USBD_AT91SAM_UDPHS_CTRL_FADDR_EN);
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
    USBD_AT91SAM_UDPHS_REG  *p_reg;
    CPU_INT16U               frm_nbr;


    p_reg   = (USBD_AT91SAM_UDPHS_REG *)p_drv->CfgPtr->BaseAddr;
    frm_nbr = (p_reg->UDPHS_FNUM >> 3) & 0xFF;                  /* Retrieve current frame number.                       */

    return (frm_nbr);
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
*                       the endpoint is successfully configured (or  realized?or  mapped?. For some
*                       device controllers, this may not be necessary.
*
*               (2) If the endpoint address is valid, then the endpoint open function should validate
*                   the attributes allowed by the hardware endpoint :
*
*                   (a) The maximum packet size 'max_pkt_size' should be validated to match hardware
*                       capabilities.
*
*               (3) If the endpoint configuration doesn't meet the hardware requirements   decrease
*                   the endpoint's number of banks and reconfigure.
*********************************************************************************************************
*/

static  void  USBD_DrvEP_Open (USBD_DRV    *p_drv,
                               CPU_INT08U   ep_addr,
                               CPU_INT08U   ep_type,
                               CPU_INT16U   max_pkt_size,
                               CPU_INT08U   transaction_frame,
                               USBD_ERR    *p_err)
{
    USBD_AT91SAM_UDPHS_REG  *p_reg;
    CPU_INT32U               ep_reg;
    CPU_BOOLEAN              ep_dir;
    CPU_INT08U               ep_log_nbr;
    CPU_INT32U               reg_to;


    (void)transaction_frame;

    p_reg      = (USBD_AT91SAM_UDPHS_REG *)p_drv->CfgPtr->BaseAddr;
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    ep_dir     =  DEF_BIT_IS_SET(ep_addr, DEF_BIT_07);          /* ep_addr bit 7. 1 IN (Tx) - 0 OUT (Rx).               */
                                                                /* ---------------- ARGUMENTS CHECKING ---------------- */
    if (ep_log_nbr >= USBD_AT91SAM_UDPHS_NBR_EPS) {
       *p_err = USBD_ERR_EP_NONE_AVAIL;
        return;
    }

    if ((ep_type      != USBD_EP_TYPE_ISOC) &&
        (max_pkt_size == USBD_AT91SAM_UDPHS_EP_SIZE_MAX)) {
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }
                                                                /* -------------- EP SIZE CONFIGURATION --------------- */
    switch (max_pkt_size) {
        case USBD_AT91SAM_UDPHS_EP_SIZE_8:
             ep_reg = USBD_AT91SAM_UDPHS_EPTCFGx_EPT_SIZE_8;
             break;

        case USBD_AT91SAM_UDPHS_EP_SIZE_16:
             ep_reg = USBD_AT91SAM_UDPHS_EPTCFGx_EPT_SIZE_16;
             break;

        case USBD_AT91SAM_UDPHS_EP_SIZE_32:
             ep_reg = USBD_AT91SAM_UDPHS_EPTCFGx_EPT_SIZE_32;
             break;

        case USBD_AT91SAM_UDPHS_EP_SIZE_64:
             ep_reg = USBD_AT91SAM_UDPHS_EPTCFGx_EPT_SIZE_64;
             break;

        case USBD_AT91SAM_UDPHS_EP_SIZE_128:
             ep_reg = USBD_AT91SAM_UDPHS_EPTCFGx_EPT_SIZE_128;
             break;

        case USBD_AT91SAM_UDPHS_EP_SIZE_256:
             ep_reg = USBD_AT91SAM_UDPHS_EPTCFGx_EPT_SIZE_256;
             break;

        case USBD_AT91SAM_UDPHS_EP_SIZE_512:
             ep_reg = USBD_AT91SAM_UDPHS_EPTCFGx_EPT_SIZE_512;
             break;

        case USBD_AT91SAM_UDPHS_EP_SIZE_1024:
             ep_reg = USBD_AT91SAM_UDPHS_EPTCFGx_EPT_SIZE_1024;
             break;

        default:
            *p_err = USBD_ERR_EP_NONE_AVAIL;                    /* EP size no valid.                                    */
             return;
    }
                                                                /* ----------- EP DIRECTION CONFIGURATION ------------- */
    if ((ep_dir     == DEF_TRUE) &&
        (ep_log_nbr != 0)) {
        DEF_BIT_SET(ep_reg, USBD_AT91SAM_UDPHS_EPTCFGx_EPT_DIR);
    }
                                                                /* --------------- EP TYPE CONFIGURATION -------------- */
    switch (ep_type) {
        case USBD_EP_TYPE_CTRL:
             break;

        case USBD_EP_TYPE_ISOC:
             DEF_BIT_SET(ep_reg, USBD_AT91SAM_UDPHS_EPTCFGx_EPT_TYPE_ISO);
             break;

        case USBD_EP_TYPE_BULK:
             DEF_BIT_SET(ep_reg, USBD_AT91SAM_UDPHS_EPTCFGx_EPT_TYPE_BULK);
             break;

        case USBD_EP_TYPE_INTR:
             DEF_BIT_SET(ep_reg, USBD_AT91SAM_UDPHS_EPTCFGx_EPT_TYPE_INT);
             break;

        default:
            *p_err =  USBD_ERR_EP_INVALID_TYPE;
             return;
    }
                                                                /* ----------- EP NUMBER BANKS CONFIGURATION ---------- */
                                                                /* If EP# 0 set #banks to 1, otherwise to 2.            */
    if (ep_log_nbr == 0) {
        DEF_BIT_SET(ep_reg, USBD_AT91SAM_UDPHS_EPTCFGx_BK_NUMBER_1);
    } else {
        if (ep_type == USBD_EP_TYPE_BULK) {
            DEF_BIT_SET(ep_reg, USBD_AT91SAM_UDPHS_EPTCFGx_BK_NUMBER_1);
        } else {
            DEF_BIT_SET(ep_reg, USBD_AT91SAM_UDPHS_EPTCFGx_BK_NUMBER_2);
        }
    }
                                                                /* Set EP configuration in the EP configuration reg.    */
    p_reg->UDPHS_EP_REG[ep_log_nbr].EPTCFGx = ep_reg;

    reg_to = USBD_AT91SAM_UDPHS_REG_TO;

    while (DEF_BIT_IS_CLR(p_reg->UDPHS_EP_REG[ep_log_nbr].EPTCFGx, USBD_AT91SAM_UDPHS_EPTCFGx_EPT_MAPD) &&
           (reg_to > 0)) {
        reg_to--;
    }
                                                                /* See Note #3.                                         */
    if (reg_to == 0)  {
        DEF_BIT_CLR(ep_reg, USBD_AT91SAM_UDPHS_EPTCFGx_BK_NUMBER_MASK);
        DEF_BIT_SET(ep_reg, USBD_AT91SAM_UDPHS_EPTCFGx_BK_NUMBER_1);
        p_reg->UDPHS_EP_REG[ep_log_nbr].EPTCFGx = ep_reg;

        reg_to = USBD_AT91SAM_UDPHS_REG_TO;

        while (DEF_BIT_IS_CLR(p_reg->UDPHS_EP_REG[ep_log_nbr].EPTCFGx, USBD_AT91SAM_UDPHS_EPTCFGx_EPT_MAPD) &&
              (reg_to > 0)) {
            reg_to--;
        }
    }

    if (reg_to == 0) {
       *p_err = USBD_ERR_EP_NONE_AVAIL;
        return;
    }
                                                                /* -------------- NON-DMA EP CONFIGURATION ------------ */
    ep_reg = 0;

    if (ep_type == USBD_EP_TYPE_CTRL) {
        ep_reg = USBD_AT91SAM_UDPHS_EPTCTLx_RX_SETUP;           /* Enable Received SETUP Interrupt.                     */
    }

    if (ep_dir == DEF_FALSE) {                                  /* Enable Received OUT Data Interrupt.                  */
        DEF_BIT_SET(ep_reg, USBD_AT91SAM_UDPHS_EPTCTLx_RX_BK_RDY);
    }

    DEF_BIT_SET(ep_reg, USBD_AT91SAM_UDPHS_EPTCTLx_EPT_ENABL);  /* Enable endpoint.                                     */

    DEF_BIT_SET(p_reg->UDPHS_IEN, DEF_BIT(ep_log_nbr + 8));     /* Enable the interrupts for this endpoint.             */

    p_reg->UDPHS_EP_REG[ep_log_nbr].EPTCTLENBx = ep_reg;

    return;
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
    USBD_AT91SAM_UDPHS_REG  *p_reg;
    CPU_INT08U               ep_log_nbr;


    p_reg      = (USBD_AT91SAM_UDPHS_REG *)p_drv->CfgPtr->BaseAddr;
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
                                                                /* Clear FORCESTALL flag.                               */
    p_reg->UDPHS_EP_REG[ep_log_nbr].EPTCLRSTAx = USBD_AT91SAM_UDPHS_EPTSTAx_CLR_TOGGLESQ |
                                                 USBD_AT91SAM_UDPHS_EPTSTAx_CLR_FRCESTALL;
                                                                /* Reset Endpoint Fifos.                                */
    p_reg->UDPHS_EPTRST                        = DEF_BIT(ep_log_nbr);

    DEF_BIT_CLR(p_reg->UDPHS_EP_REG[ep_log_nbr].EPTCFGx,
                USBD_AT91SAM_UDPHS_EPTCFGx_BK_NUMBER_MASK);
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
* Return(s)   : Maximum length in bytes of data that can be received in one iteration.
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
    USBD_AT91SAM_UDPHS_REG  *p_reg;
    CPU_INT08U               ep_log_nbr;
    CPU_INT32U               ep_max_pkt_size;
    CPU_SR_ALLOC();


    (void)p_buf;
    (void)buf_len;

    p_reg      = (USBD_AT91SAM_UDPHS_REG *)p_drv->CfgPtr->BaseAddr;
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);

    CPU_CRITICAL_ENTER();

    if (BulkRxClrFlagPending == DEF_TRUE) {
        p_reg->UDPHS_EP_REG[BulkRxEP_LogNbr].EPTCLRSTAx = USBD_AT91SAM_UDPHS_EPTSTAx_CLR_RX_BK_RDY;
        BulkRxClrFlagPending                            = DEF_FALSE;
    }
                                                                /* Enable Received OUT Data Interrupt.                  */
    p_reg->UDPHS_EP_REG[ep_log_nbr].EPTCTLENBx = USBD_AT91SAM_UDPHS_EPTCTLx_RX_BK_RDY;

    CPU_CRITICAL_EXIT();

    ep_max_pkt_size = USBD_EP_MaxPktSizeGet(p_drv->DevNbr,
                                            ep_addr,
                                            p_err);

    return (DEF_MIN(buf_len, ep_max_pkt_size));
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
    USBD_AT91SAM_UDPHS_REG       *p_reg;
    USBD_AT91SAM_UDPHS_EP_DPRAM  *p_ep_dpram;
    CPU_INT08U                    ep_log_nbr;
    CPU_INT16U                    ep_pkt_len;
    CPU_SR_ALLOC();


    p_reg      = (USBD_AT91SAM_UDPHS_REG *)p_drv->CfgPtr->BaseAddr;
    p_ep_dpram = (USBD_AT91SAM_UDPHS_EP_DPRAM *)p_drv->CfgPtr->MemAddr;
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);

    CPU_CRITICAL_ENTER();
                                                                /* Get the number of bytes received.                    */
    ep_pkt_len = (p_reg->UDPHS_EP_REG[ep_log_nbr].EPTSTAx >> 20) & 0x000007FF;
    ep_pkt_len = (CPU_INT16U)DEF_MIN(ep_pkt_len, buf_len);

    if (ep_pkt_len > 0) {
        Mem_Copy ((void     *) (p_buf),                         /* Copy received bytes form the DPRAM.                  */
                  (void     *)&(p_ep_dpram->EpBufLog[ep_log_nbr].EpBufPhy[0]),
                  (CPU_SIZE_T) (ep_pkt_len));
    }
                                                                /* Clear the RX_BK_RDY flag.                            */
    if ((p_reg->UDPHS_EP_REG[ep_log_nbr].EPTCFGx & USBD_AT91SAM_UDPHS_EPTCFGx_EPT_TYPE_MASK) != USBD_AT91SAM_UDPHS_EPTCFGx_EPT_TYPE_BULK) {
        p_reg->UDPHS_EP_REG[ep_log_nbr].EPTCLRSTAx = USBD_AT91SAM_UDPHS_EPTSTAx_CLR_RX_BK_RDY;
    } else {
        BulkRxEP_LogNbr      = ep_log_nbr;
        BulkRxClrFlagPending = DEF_TRUE;
    }

    USBD_DBG_DRV_EP_ARG("  Drv EP Rx Len:", ep_addr, ep_pkt_len);

    CPU_CRITICAL_EXIT();

   *p_err = USBD_ERR_NONE;

    return (ep_pkt_len);
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
    USBD_AT91SAM_UDPHS_REG  *p_reg;
    CPU_INT08U               ep_log_nbr;


    p_reg      = (USBD_AT91SAM_UDPHS_REG *)p_drv->CfgPtr->BaseAddr;
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);

                                                                /* Clear the RX_BK_RDY flag.                            */
    p_reg->UDPHS_EP_REG[ep_log_nbr].EPTCLRSTAx = USBD_AT91SAM_UDPHS_EPTSTAx_CLR_RX_BK_RDY;

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
    USBD_AT91SAM_UDPHS_REG       *p_reg;
    USBD_AT91SAM_UDPHS_EP_DPRAM  *p_ep_dpram;
    CPU_INT08U                    ep_log_nbr;
    CPU_INT16U                    ep_pkt_len;
    CPU_INT16U                    ep_max_pkt_size;
    CPU_INT32U                    reg_to;
    CPU_SR_ALLOC();


    p_reg      = (USBD_AT91SAM_UDPHS_REG *)p_drv->CfgPtr->BaseAddr;
    p_ep_dpram = (USBD_AT91SAM_UDPHS_EP_DPRAM *)p_drv->CfgPtr->MemAddr;
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);


    reg_to = USBD_AT91SAM_UDPHS_REG_TO;
                                                                /* Wait until TX_PK_RDY is clear.                       */
    while ((reg_to > 0) &&
           (DEF_BIT_IS_SET(p_reg->UDPHS_EP_REG[ep_log_nbr].EPTSTAx, USBD_AT91SAM_UDPHS_EPTSTAx_TX_PK_RDY) == DEF_YES)) {
        reg_to--;
    }

    if (reg_to == 0) {
       *p_err = USBD_ERR_TX;
        return (0u);
    }

    CPU_CRITICAL_ENTER();
                                                                /* If the packet length is greather than the max.       */
    ep_max_pkt_size =  DEF_BIT((p_reg->UDPHS_EP_REG[ep_log_nbr].EPTCFGx & 0x07) + 3);
    ep_pkt_len      = (CPU_INT16U)DEF_MIN(ep_max_pkt_size, buf_len);

                                                                /* Copy data to the DPRAM.                              */
    Mem_Copy ((void     *)&(p_ep_dpram->EpBufLog[ep_log_nbr].EpBufPhy[0]),
              (void     *) (p_buf),
              (CPU_SIZE_T) (ep_pkt_len));

    CPU_CRITICAL_EXIT();

    USBD_DBG_DRV_EP_ARG("  Drv EP Tx Len:", ep_addr, ep_pkt_len);

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
* Note(s)     : (1) Set TX_PK_RDY status flag after a packet has been written into the endpoint FIFO
*                   for IN data transfers.
*********************************************************************************************************
*/

static  void  USBD_DrvEP_TxStart (USBD_DRV    *p_drv,
                                  CPU_INT08U   ep_addr,
                                  CPU_INT08U  *p_buf,
                                  CPU_INT32U   buf_len,
                                  USBD_ERR    *p_err)
{
    USBD_AT91SAM_UDPHS_REG  *p_reg;
    CPU_INT08U               ep_log_nbr;
    CPU_SR_ALLOC();


    (void)p_buf;
    (void)buf_len;

    p_reg      = (USBD_AT91SAM_UDPHS_REG *)p_drv->CfgPtr->BaseAddr;
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);

    CPU_CRITICAL_ENTER();
                                                                /* Enable Transmitted IN Data Complete Interrupt.       */
    p_reg->UDPHS_EP_REG[ep_log_nbr].EPTCTLENBx = USBD_AT91SAM_UDPHS_EPTCTLx_TX_COMPLT;
                                                                /* See Note #1.                                         */
    p_reg->UDPHS_EP_REG[ep_log_nbr].EPTSETSTAx = USBD_AT91SAM_UDPHS_EPTSTAx_SET_TX_PK_RDY;

    CPU_CRITICAL_EXIT();

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
    USBD_AT91SAM_UDPHS_REG  *p_reg;
    CPU_INT08U               ep_log_nbr;
    CPU_INT32U               reg_to;


    p_reg      = (USBD_AT91SAM_UDPHS_REG *)p_drv->CfgPtr->BaseAddr;
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    reg_to     =  USBD_AT91SAM_UDPHS_REG_TO;
                                                                /* Set TX_PK_RDY status flag.                           */
    p_reg->UDPHS_EP_REG[ep_log_nbr].EPTSETSTAx = USBD_AT91SAM_UDPHS_EPTSTAx_SET_TX_PK_RDY;

                                                                /* Wait until TX_COMPLT status flag is clr.             */
    while ((reg_to > 0) &&
           (DEF_BIT_IS_CLR(p_reg->UDPHS_EP_REG[ep_log_nbr].EPTSTAx, USBD_AT91SAM_UDPHS_EPTSTAx_TX_COMPLT) == DEF_YES)) {
        reg_to--;
    }

    if (reg_to == 0) {
       *p_err = USBD_ERR_TX;
        return;
    }
                                                                /* Clear TX_COMPLT status flag.                         */
    p_reg->UDPHS_EP_REG[ep_log_nbr].EPTCLRSTAx = USBD_AT91SAM_UDPHS_EPTCTLx_TX_COMPLT;

    if ((p_reg->UDPHS_CTRL & USBD_AT91SAM_UDPHS_CTRL_DEV_ADDR_MASK) != 0x00) {
                                                                /* Enable Function Address.                             */
        DEF_BIT_SET(p_reg->UDPHS_CTRL, USBD_AT91SAM_UDPHS_CTRL_FADDR_EN);
    } else {
                                                                /* Use the default function address 0.                  */
        DEF_BIT_CLR(p_reg->UDPHS_CTRL, USBD_AT91SAM_UDPHS_CTRL_FADDR_EN);
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
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_DrvEP_Abort (USBD_DRV    *p_drv,
                                       CPU_INT08U   ep_addr)
{
    USBD_AT91SAM_UDPHS_REG  *p_reg;
    CPU_INT08U               ep_log_nbr;


    p_reg      = (USBD_AT91SAM_UDPHS_REG *)p_drv->CfgPtr->BaseAddr;
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);

                                                                /* Kill the last written bank.                          */
    p_reg->UDPHS_EP_REG[ep_log_nbr].EPTSETSTAx = USBD_AT91SAM_UDPHS_EPTSTAx_SET_KILL_BANK;

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
    USBD_AT91SAM_UDPHS_REG  *p_reg;
    CPU_INT08U               ep_log_nbr;


    p_reg      = (USBD_AT91SAM_UDPHS_REG *)p_drv->CfgPtr->BaseAddr;
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);

    if (state == DEF_SET) {
                                                                /* Set FRCESTALL flag to request a STALL answer to ...  */
                                                                /* ...the host for the next handshake.                  */
        p_reg->UDPHS_EP_REG[ep_log_nbr].EPTSETSTAx = USBD_AT91SAM_UDPHS_EPTSTAx_SET_FRCESTALL;

        if (BulkRxClrFlagPending == DEF_TRUE) {
            p_reg->UDPHS_EP_REG[BulkRxEP_LogNbr].EPTCLRSTAx = USBD_AT91SAM_UDPHS_EPTSTAx_CLR_RX_BK_RDY;
            BulkRxClrFlagPending                            = DEF_FALSE;
        }

        USBD_DBG_DRV_EP("  Drv EP Stall (set)", ep_addr);
    } else {
                                                                /* Clear FORCESTALL flag.                               */
        p_reg->UDPHS_EP_REG[ep_log_nbr].EPTCLRSTAx = USBD_AT91SAM_UDPHS_EPTSTAx_CLR_TOGGLESQ |
                                                     USBD_AT91SAM_UDPHS_EPTSTAx_CLR_FRCESTALL;
                                                                /* Reset Endpoint Fifos.                                */
        p_reg->UDPHS_EPTRST                        = DEF_BIT(ep_log_nbr);

        USBD_DBG_DRV_EP("  Drv EP Stall (clr)", ep_addr);
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
    USBD_AT91SAM_UDPHS_REG  *p_reg;
    CPU_INT32U               int_stat;
    CPU_INT32U               int_en;
    CPU_INT08U               ep_log_nbr;
    CPU_INT08U               i;


    p_reg     = (USBD_AT91SAM_UDPHS_REG *)p_drv->CfgPtr->BaseAddr;
                                                                /* Get interrupts status.                               */
    int_stat  = p_reg->UDPHS_INTSTA;
    int_en    = p_reg->UDPHS_IEN;
    int_stat &= int_en;

    while (int_stat != DEF_BIT_NONE) {

        if (DEF_BIT_IS_SET_ANY(int_stat, USBD_AT91SAM_UDPHS_INT_ENDRESET) == DEF_YES) {
                                                                /* ---------------- USB RESET INTERRUPT --------------- */
                                                                /* Clear device address.                                */
            DEF_BIT_CLR(p_reg->UDPHS_CTRL, (USBD_AT91SAM_UDPHS_CTRL_DEV_ADDR_MASK |
                                            USBD_AT91SAM_UDPHS_CTRL_FADDR_EN));

                                                                /* Reset the UDPHS EP's registers.                      */
            p_reg->UDPHS_EPTRST = USBD_AT91SAM_UDPHSR64_UDPHS_EPTRST_EPT_ALL;

            for (i = 0; i < USBD_AT91SAM_UDPHS_NBR_EPS; i++) {
                p_reg->UDPHS_EP_REG[i].EPTCTLDISx = USBD_AT91SAM_UDPHS_EPTCTLx_ALL;
                p_reg->UDPHS_EP_REG[i].EPTCLRSTAx = USBD_AT91SAM_UDPHS_EPTSTAx_CLR_TOGGLESQ;
            }

            USBD_EventReset(p_drv);                             /* Notify bus reset.                                    */

            if (DEF_BIT_IS_SET(p_reg->UDPHS_INTSTA, USBD_AT91SAM_UDPHS_INT_SPEED) == DEF_YES) {
                USBD_EventHS(p_drv);                            /* Process High-Speed Handshake Event.                  */
            }
                                                                /* Enable the UDPHS.                                    */
            DEF_BIT_SET(p_reg->UDPHS_CTRL, USBD_AT91SAM_UDPHS_CTRL_EN_UDPHS);
                                                                /* Clear the Interrupt.                                 */
            p_reg->UDPHS_CLRINT = USBD_AT91SAM_UDPHS_INT_WAKE_UP |
                                  USBD_AT91SAM_UDPHS_INT_DET_SUSPD;

            p_reg->UDPHS_CLRINT = USBD_AT91SAM_UDPHS_INT_ENDRESET;
                                                                /* Enable Suspend Interrupt.                            */
            DEF_BIT_SET(p_reg->UDPHS_IEN, USBD_AT91SAM_UDPHS_INT_DET_SUSPD);

        } else if (DEF_BIT_IS_SET(int_stat, USBD_AT91SAM_UDPHS_INT_DET_SUSPD) == DEF_YES) {
                                                                /* --------------- USB SUSPEND INTERRUPT -------------- */
            USBD_EventSuspend(p_drv);                           /* Notify bus suspend.                                  */
                                                                /* Clear the suspend interrupt.                         */
            p_reg->UDPHS_CLRINT = USBD_AT91SAM_UDPHS_INT_DET_SUSPD |
                                  USBD_AT91SAM_UDPHS_INT_WAKE_UP;
                                                                /* Disable Suspend Interrupt.                           */
            DEF_BIT_CLR(p_reg->UDPHS_IEN, USBD_AT91SAM_UDPHS_INT_DET_SUSPD);
                                                                /* Enable Wake Up CPU Interrupt.                        */
            DEF_BIT_SET(p_reg->UDPHS_IEN, USBD_AT91SAM_UDPHS_INT_WAKE_UP);

        } else if (DEF_BIT_IS_SET_ANY(int_stat, USBD_AT91SAM_UDPHS_INT_RESUME) == DEF_YES) {
                                                                /* ---------------- USB RESUME INTERRUPT -------------- */
            USBD_EventResume(p_drv);                            /* Notify bus resume.                                   */
                                                                /* Clear the Resume Interrupt.                          */
            p_reg->UDPHS_CLRINT =  USBD_AT91SAM_UDPHS_INT_RESUME;
                                                                /* Disable Wake Up CPU Interrupt.                       */
            DEF_BIT_CLR(p_reg->UDPHS_IEN, USBD_AT91SAM_UDPHS_INT_RESUME);
                                                                /* Enable Suspend Interrupt.                            */
            DEF_BIT_SET(p_reg->UDPHS_IEN, USBD_AT91SAM_UDPHS_INT_DET_SUSPD);

        } else if (DEF_BIT_IS_SET_ANY(int_stat, USBD_AT91SAM_UDPHS_INT_EPT_x) == DEF_YES) {
                                                                /* ------------------ ENDPOINT INTERRUPT -------------- */
            for (ep_log_nbr = 0; ep_log_nbr < USBD_AT91SAM_UDPHS_NBR_EPS; ep_log_nbr++) {
                if (DEF_BIT_IS_SET(int_stat, DEF_BIT(ep_log_nbr + 8)) == DEF_YES) {
                                                                /* Process packets for a specific endpoint.             */
                    USBD_AT91SAM_UDPHS_EP_Process(p_drv, ep_log_nbr);
                }
            }
        }
                                                                /* Get interrupts status again.                         */
        int_en    = p_reg->UDPHS_IEN;
        int_stat  = p_reg->UDPHS_INTSTA;
        int_stat &= int_en;
    }
}


/*
*********************************************************************************************************
*                                        USBD_AT91SAM_UDPHS_EP_Process()
*
* Description : Process IN, OUT and SETUP packets for a specific endpoint.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_log_nbr  Logical endpoint number.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/
static  void  USBD_AT91SAM_UDPHS_EP_Process(USBD_DRV    *p_drv,
                                            CPU_INT08U   ep_log_nbr)
{
    USBD_AT91SAM_UDPHS_REG       *p_reg;
    USBD_AT91SAM_UDPHS_EP_DPRAM  *p_ep_dpram;
    CPU_INT32U                    ep_status;
    CPU_INT32U                    ep_ctrl_en;
    CPU_INT08U                    setup_pkt[8];
    CPU_INT16U                    setup_len;

    p_reg      = (USBD_AT91SAM_UDPHS_REG *)p_drv->CfgPtr->BaseAddr;
    p_ep_dpram = (USBD_AT91SAM_UDPHS_EP_DPRAM *)p_drv->CfgPtr->MemAddr;
                                                                /* Get the status for a specific endpoint.              */
    ep_status  = p_reg->UDPHS_EP_REG[ep_log_nbr].EPTSTAx;
    ep_ctrl_en = p_reg->UDPHS_EP_REG[ep_log_nbr].EPTCTLx;
    ep_status &= ep_ctrl_en;
                                                                /* ---------------- SETUP TRANSACTION ----------------- */
    if (DEF_BIT_IS_SET(ep_status, USBD_AT91SAM_UDPHS_EPTSTAx_RX_SETUP) == DEF_YES) {
                                                                /* Get the number of bytes received.                    */
        setup_len = (p_reg->UDPHS_EP_REG[ep_log_nbr].EPTSTAx >> 20) & 0x000007FF;

        if (setup_len == 8) {
            Mem_Copy ((void     *)&(setup_pkt[0]),              /* Copy received bytes form the DPRAM.                  */
                      (void     *)&(p_ep_dpram->EpBufLog[ep_log_nbr].EpBufPhy[0]),
                      (CPU_SIZE_T) (setup_len));

                                                                /* Clear the RX_SETUP flag.                             */
            p_reg->UDPHS_EP_REG[ep_log_nbr].EPTCLRSTAx = USBD_AT91SAM_UDPHS_EPTSTAx_CLR_RX_SETUP;

            USBD_EventSetup(p_drv, (void *)&setup_pkt[0]);      /* Send a USB setup event to the core task.             */

        } else {
            p_reg->UDPHS_EP_REG[ep_log_nbr].EPTCLRSTAx = USBD_AT91SAM_UDPHS_EPTSTAx_CLR_RX_SETUP;
        }
                                                                /* ------------------ IN TRANSACTION ------------------ */
    } else if (DEF_BIT_IS_SET(ep_status, USBD_AT91SAM_UDPHS_EPTSTAx_TX_COMPLT) == DEF_YES) {
                                                                /* Clear the TX_COMPLT flag.                            */
        p_reg->UDPHS_EP_REG[ep_log_nbr].EPTCLRSTAx = USBD_AT91SAM_UDPHS_EPTSTAx_TX_COMPLT;
        USBD_EP_TxCmpl(p_drv, ep_log_nbr);                      /* Notify USB stack that packet transmit has completed. */
                                                                /* ------------------ OUT TRANSACTION ----------------- */
    } else if (DEF_BIT_IS_SET(ep_status, USBD_AT91SAM_UDPHS_EPTSTAx_RX_BK_RDY) == DEF_YES) {
                                                                /* Disable Received OUT Data Interrupt.                 */
        p_reg->UDPHS_EP_REG[ep_log_nbr].EPTCTLDISx = USBD_AT91SAM_UDPHS_EPTCTLx_RX_BK_RDY;
        USBD_EP_RxCmpl(p_drv, ep_log_nbr);                      /* Notify USB stack that packet receive has completed.  */
    }
}
