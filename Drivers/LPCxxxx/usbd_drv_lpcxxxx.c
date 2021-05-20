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
*                                    USB DEVICE CONTROLLER DRIVER
*
*                                               LPCXXXX
*
* Filename : usbd_drv_lpcxxxx.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)  : (1) You can find specific information about this driver at:
*                https://doc.micrium.com/display/USBDDRV/LPCxxxx
*
*            (2) With an appropriate BSP, this device driver will support the OTG device module on
*                the NXP LPC MCUs & MPUs, including:
*
*                    NXP LPC23xx series.
*                    NXP LPC24xx series.
*                    NXP LPC17xx series.
*                    NXP LPC29xx series.
*                    NXP LPC32x0 series.
*                    NXP LPC318x series.
*
*            (3) This driver supports DMA and Slave (FIFO) in the following APIs.
*
*                    (a) 'USBD_DrvAPI_LPCXXXX_DMA'  API implementation uses the controller in DMA mode.
*
*                    (b) 'USBD_DrvAPI_LPCXXXX_FIFO' API implementation uses the controller in Slave mode.
*
*            (4) This driver does NOT support control OUT transfer of more than 64 bytes.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#include  "../../Source/usbd_core.h"
#include  "usbd_drv_lpcxxxx.h"

/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  USBD_DBG_DRV(msg)                           USBD_DBG_GENERIC((msg),                                           \
                                                                      USBD_EP_NBR_NONE,                                \
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
*                                   LPCXXXX USB DEVICE CONSTRAINTS
*********************************************************************************************************
*/

#define  LPCXXXX_REG_TO                               0x1Fu

#define  LPCXXXX_EP_RAM_SIZE_MAX                      1024u     /* Maximum Endpoint RAM size (in words)                 */
#define  LPCXXXX_UDCA_ALIGN                            128u     /* USB device comm area (UDCA) alignment.               */
#define  LPCXXXX_UDCA_ALIGN_MSK                       0x7Fu     /* USB device comm area (UDCA) alignment mask.          */


/*
*********************************************************************************************************
*                                        REGISTER BIT DEFINES
*********************************************************************************************************
*/
                                                                /* --------------- USB DEVICE INTERRUPTS -------------- */
#define  LPCXXXX_DEV_INT_ERR                    DEF_BIT_09      /* Error int. Any bus error int.                        */
#define  LPCXXXX_DEV_INT_EP_RLZED               DEF_BIT_08      /* EP realized.                                         */
#define  LPCXXXX_DEV_INT_TX_ENDPKT              DEF_BIT_07      /* End of Tx                                            */
#define  LPCXXXX_DEV_INT_RX_ENDPKT              DEF_BIT_06      /* End of Rx                                            */
#define  LPCXXXX_DEV_INT_CD_FULL                DEF_BIT_05      /* Cmd data reg is full (data can be read).             */
#define  LPCXXXX_DEV_INT_CD_EMPTY               DEF_BIT_04      /* Cmd code reg is empty.                               */
#define  LPCXXXX_DEV_INT_DEV_STAT               DEF_BIT_03      /* Set when USB Bus reset, suspend or connect chng      */
#define  LPCXXXX_DEV_INT_EP_SLOW                DEF_BIT_02      /* Slow int xfer for the EP.                            */
#define  LPCXXXX_DEV_INT_EP_FAST                DEF_BIT_01      /* Fast int xfer for the EP.                            */
#define  LPCXXXX_DEV_INT_FRAME                  DEF_BIT_00      /* Frame int. Frame int occurs every 1 ms.              */
                                                                /* All USB dev int.                                     */
#define  LPCXXXX_DEV_INT_ALL                   (LPCXXXX_DEV_INT_ERR        | \
                                                LPCXXXX_DEV_INT_EP_RLZED   | \
                                                LPCXXXX_DEV_INT_TX_ENDPKT  | \
                                                LPCXXXX_DEV_INT_RX_ENDPKT  | \
                                                LPCXXXX_DEV_INT_CD_FULL    | \
                                                LPCXXXX_DEV_INT_CD_EMPTY   | \
                                                LPCXXXX_DEV_INT_DEV_STAT   | \
                                                LPCXXXX_DEV_INT_EP_SLOW    | \
                                                LPCXXXX_DEV_INT_EP_FAST    | \
                                                LPCXXXX_DEV_INT_FRAME)

#define  LPCXXXX_DMA_INT_EOT                    DEF_BIT_00
#define  LPCXXXX_DMA_INT_NDDR                   DEF_BIT_01
#define  LPCXXXX_DMA_INT_ERR                    DEF_BIT_02

#define  LPCXXXX_EP_INT_ALL                      0xFFFFFFFFu

                                                                /* ----------- USB DEVICE INTERRUPT PRIORITY ----------- */
#define  LPCXXXX_DEV_INT_PRI_FRAME              DEF_BIT_00
#define  LPCXXXX_DEV_INT_PRI_EP_FAST            DEF_BIT_01

#define  LPCXXXX_EP_INT_STAT_EP0                DEF_BIT_00
#define  LPCXXXX_EP_INT_STAT_EP1                DEF_BIT_01
#define  LPCXXXX_EP_INT_STAT_EPs_CTRL          (LPCXXXX_EP_INT_STAT_EP0 | \
                                                LPCXXXX_EP_INT_STAT_EP1)

#define  LPCXXXX_PHY_NBR_EP0_OUT                       0x00u
#define  LPCXXXX_PHY_NBR_EP0_IN                        0x01u
                                                                /* ----- USB RX PACKET LENGTH REGISTER BIT DEFINES ---- */
#define  LPCXXXX_RX_PKT_LEN_PKT_RDY             DEF_BIT_11      /* Packet length field is ready for reading.            */
#define  LPCXXXX_RX_PKT_LEN_DV                  DEF_BIT_10      /* Data is valid.                                       */

#define  LPCXXXX_PKT_LEN_MASK                    0x000003FFu    /* Packet length field mask                             */

                                                                /* --------- USB CONTROL REGISTER BIT DEFINES --------- */
#define  LPCXXXX_CTRL_LOG_EP_MASK                0x0000003Cu
#define  LPCXXXX_CTRL_WR_EN                     DEF_BIT_01      /* Wr enable.                                           */
#define  LPCXXXX_CTRL_RD_EN                     DEF_BIT_00      /* Rd enable.                                           */

                                                                /* ------ USB ENDPOINT REGISTER INDEX BIT DEFINES ----- */
#define  LPCXXXX_EP_IX_PHY_NBR_MASK              0x0000001Fu    /* Physical EP nbr mask.                                */

                                                                /* --------- SERIAL INTERFACE ENGINE COMMANDS --------- */
#define  LPCXXXX_SIE_DATA_CODE                   0x00000100u    /* Data Phase coding 00 <Byte> 01 00                    */
                                                                /* ----------------- DEVICES COMMANDS ----------------- */
#define  LPCXXXX_SIE_CMD_WR_SET_ADDR             0x00D00500u    /* Set address.                                         */
#define  LPCXXXX_SIE_CMD_WR_CFG_DEV              0x00D80500u    /* Cfg dev.                                             */
#define  LPCXXXX_SIE_CMD_WR_SET_MODE             0x00F30500u    /* Set mode.                                            */

#define  LPCXXXX_SIE_CMD_RD_CUR_FRM_NBR          0x00F50500u    /* Read cur frame nbr (command phase).                  */
#define  LPCXXXX_SIE_DATA_RD_CUR_FRM_NBR         0x00F50200u    /* Read cur frame nbr (data    phase).                  */

#define  LPCXXXX_SIE_CMD_RD_TEST_REG             0x00FD0500u    /* Read test reg (command phase).                       */
#define  LPCXXXX_SIE_DATA_RD_TEST_REG            0x00FD0200u    /* Read test reg (data    phase).                       */

#define  LPCXXXX_SIE_CMD_WR_SET_DEV_STAT         0x00FE0500u    /* Set dev status.                                      */

#define  LPCXXXX_SIE_CMD_RD_GET_DEV_STAT         0x00FE0500u    /* Get dev status (command phase).                      */
#define  LPCXXXX_SIE_DATA_RD_GET_DEV_STAT        0x00FE0200u    /* Get dev status (data    phase).                      */

#define  LPCXXXX_SIE_CMD_RD_GET_ERR_CODE         0x00FF0500u    /* Get error code (command phase).                      */
#define  LPCXXXX_SIE_DATA_RD_GET_ERR_CODE        0x00FF0200u    /* Get error code (data    phase).                      */

#define  LPCXXXX_SIE_CMD_RD_ERR_STAT             0x00FB0500u    /* Read error status (command phase).                   */
#define  LPCXXXX_SIE_DATA_RD_ERR_STAT            0x00FB0200u    /* Read error status (data    phase).                   */

                                                                /* ----------------- ENDPOINT COMMANDS ---------------- */
#define  LPCXXXX_SIE_CMD_RD_SEL_EP_BASE          0x00000500u    /* Select EP (command phase).                           */
#define  LPCXXXX_SIE_DATA_RD_SEL_EP_BASE         0x00000200u    /* Select EP (data    phase).                           */

#define  LPCXXXX_SIE_CMD_RD_CLR_EP_BASE          0x00400500u    /* Select EP (command phase).                           */
#define  LPCXXXX_SIE_DATA_RD_CLR_EP_BASE         0x00400200u    /* Select EP (data    phase).                           */

#define  LPCXXXX_SIE_CMD_WR_SET_EP_STAT_BASE     0x00400500u    /* Set EP status (command phase).                       */
#define  LPCXXXX_SIE_DATA_WR_SET_EP_STAT_BASE    0x00400200u    /* Set EP status (data    phase).                       */

#define  LPCXXXX_SIE_CMD_RD_CLR_BUF              0x00F20500u    /* Clear buffer (command phase).                        */
#define  LPCXXXX_SIE_DATA_RD_CLR_BUF             0x00F20200u    /* Clear buffer (data    phase).                        */

#define  LPCXXXX_SIE_CMD_WR_VALIDATE_BUF         0x00FA0500u    /* Validate buffer (command phase).                     */

                                                                /* ------------ SET/GET DEVICE STATUS BITS ------------ */
#define  LPCXXXX_SIE_CMD_DEV_STAT_RST           DEF_BIT_04      /* Bus reset.                                           */
#define  LPCXXXX_SIE_CMD_DEV_SUS_CHNG           DEF_BIT_03      /* Suspend change.                                      */
#define  LPCXXXX_SIE_CMD_DEV_SUS                DEF_BIT_02      /* Suspend status.                                      */
#define  LPCXXXX_SIE_CMD_DEV_CON_CHNG           DEF_BIT_01      /* Connect change.                                      */
#define  LPCXXXX_SIE_CMD_DEV_CON                DEF_BIT_00      /* Connect status.                                      */

                                                                /* ------------- SET ADDRESS COMMAND BITS ------------- */
#define  LPCXXXX_SIE_CMD_SET_ADDR_DEV_EN        DEF_BIT_07      /* Dev enable.                                          */

                                                                /* ----------- CONFIGURE DEVICE COMMAND BITS ---------- */
#define  LPCXXXX_SIE_CMD_CFG_DEV_EN             DEF_BIT_00      /* Dev is configured.                                   */

                                                                /* ------------------- SET MODE BITS ------------------ */
#define  LPCXXXX_SIE_CMD_SET_MODE_AP_CLK        DEF_BIT_00      /* Always PLL clock enable.                             */
#define  LPCXXXX_SIE_CMD_SET_MODE_INAK_CI       DEF_BIT_01      /* Int on NAK for ctrl IN  EPs.                         */
#define  LPCXXXX_SIE_CMD_SET_MODE_INAK_CO       DEF_BIT_02      /* Int on NAK for ctrl OUT EPs.                         */
#define  LPCXXXX_SIE_CMD_SET_MODE_INAK_II       DEF_BIT_03      /* Int on NAK for intr IN  EPs.                         */
#define  LPCXXXX_SIE_CMD_SET_MODE_INAK_IO       DEF_BIT_04      /* Int on NAK for intr OUT EPs.                         */
#define  LPCXXXX_SIE_CMD_SET_MODE_INAK_BI       DEF_BIT_05      /* Int on NAK for Bulk IN  EPs.                         */
#define  LPCXXXX_SIE_CMD_SET_MODE_INAK_BO       DEF_BIT_06      /* Int on NAK for Bulk OUT EPs.                         */

                                                                /* ---------------- SEL EP COMMAND BITS --------------- */
#define  LPCXXXX_SIE_CMD_SEL_EPx_F_E            DEF_BIT_00      /* Buffer full/empty                                    */
#define  LPCXXXX_SIE_CMD_SEL_EPx_ST             DEF_BIT_01      /* EP is stalled                                        */
#define  LPCXXXX_SIE_CMD_SEL_EPx_STP            DEF_BIT_02      /* Setup packet.                                        */
#define  LPCXXXX_SIE_CMD_SEL_EPx_PO             DEF_BIT_03      /* Packet over-written.                                 */
#define  LPCXXXX_SIE_CMD_SEL_EPx_EPN            DEF_BIT_04      /* EP NAKed.                                            */
#define  LPCXXXX_SIE_CMD_SEL_EPx_B1_FULL        DEF_BIT_05      /* Buffer 1 full/empty.                                 */
#define  LPCXXXX_SIE_CMD_SEL_EPx_B2_FULL        DEF_BIT_06      /* Buffer 2 full/empty.                                 */

                                                                /* ------------ SET EP STATUS COMMAND BITS ------------ */
#define  LPCXXXX_SIE_CMD_SET_EPx_STAT_ST        DEF_BIT_00      /* EP stalled.                                          */
#define  LPCXXXX_SIE_CMD_SET_EPx_STAT_DA        DEF_BIT_05      /* EP disabled.                                         */
#define  LPCXXXX_SIE_CMD_SET_EPx_STAT_RF_MO     DEF_BIT_06      /* Rate feedback mode.                                  */
#define  LPCXXXX_SIE_CMD_SET_EPx_STAT_CND_ST    DEF_BIT_06      /* Conditional stall (only for control EPs).            */

                                                                /* ----------- GET ERROR COMMAND ERROR CODES ---------- */
#define  LPCXXXX_SIE_CMD_ERR_CODE_PID                  0x01u
#define  LPCXXXX_SIE_CMD_ERR_CODE_UNKOWN_PID           0x02u
#define  LPCXXXX_SIE_CMD_ERR_CODE_UNKOWN_PKT           0x03u
#define  LPCXXXX_SIE_CMD_ERR_CODE_TOKEN_CRC            0x04u
#define  LPCXXXX_SIE_CMD_ERR_CODE_DATA_CRC             0x05u
#define  LPCXXXX_SIE_CMD_ERR_CODE_DATA_TO              0x06u
#define  LPCXXXX_SIE_CMD_ERR_CODE_BABBLE               0x07u
#define  LPCXXXX_SIE_CMD_ERR_CODE_END_PKT              0x08u
#define  LPCXXXX_SIE_CMD_ERR_CODE_TX_RX_NAK            0x09u
#define  LPCXXXX_SIE_CMD_ERR_CODE_STALL                0x0Au
#define  LPCXXXX_SIE_CMD_ERR_CODE_BUF_OVR              0x0Bu
#define  LPCXXXX_SIE_CMD_ERR_CODE_EMPTY                0x0Cu
#define  LPCXXXX_SIE_CMD_ERR_CODE_BIT_STUFF            0x0Du
#define  LPCXXXX_SIE_CMD_ERR_CODE_SYNC                 0x0Eu
#define  LPCXXXX_SIE_CMD_ERR_CODE_TOGGLE               0x0Fu

                                                                /* ----------- GET ERROR STATUS COMMAND BITS ---------- */
#define  LPCXXXX_SIE_CMD_ERR_STAT_PID           DEF_BIT_00
#define  LPCXXXX_SIE_CMD_ERR_STAT_UEPKT         DEF_BIT_01
#define  LPCXXXX_SIE_CMD_ERR_STAT_DATA_CRC      DEF_BIT_02
#define  LPCXXXX_SIE_CMD_ERR_STAT_TO            DEF_BIT_03
#define  LPCXXXX_SIE_CMD_ERR_STAT_END_PKT       DEF_BIT_04
#define  LPCXXXX_SIE_CMD_ERR_STAT_BUF_OVR       DEF_BIT_05
#define  LPCXXXX_SIE_CMD_ERR_STAT_BIT_STUFF     DEF_BIT_06
#define  LPCXXXX_SIE_CMD_ERR_STAT_TOGGLE        DEF_BIT_07
                                                                /* ----------------- COMMAND CONSTANTS ---------------- */
#define  LPCXXXX_SIE_CMD_TEST_REG_VAL                0xA50Fu    /* Test value to determine if USB clocks is setup ...   */
                                                                /* ... correctly.                                       */

                                                                /* ------------------ DMA DESCRIPTOR ------------------ */
#define  LPCXXXX_DMA_STAT_NOT_SERV                        0u    /* No packet has been transferred yet.                  */
#define  LPCXXXX_DMA_STAT_BEING_SERV                      1u    /* At least one packet is transferred.                  */
#define  LPCXXXX_DMA_STAT_NORMAL                          2u    /* DD is retired because end of buffer is reached.      */
#define  LPCXXXX_DMA_STAT_UND                             3u    /* Data underrun.                                       */
#define  LPCXXXX_DMA_STAT_OVR                             8u    /* Data overrun.                                        */
#define  LPCXXXX_DMA_STAT_SYS_ERR                         9u    /* System error.                                        */
#define  LPCXXXX_DMA_STAT_MSK                           0xFu    /* DMA status mask.                                     */

#define  LPCXXXX_DMA_PKT_VALID_BIT              DEF_BIT_05      /* DMA descriptor valid field.                          */


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                    LPCXXXXX REGISTERS DATA TYPE
*********************************************************************************************************
*/

typedef  struct  usbd_lpcxxxx_reg {
                                                                /* ------------ DEVICE INTERRUPT REGISTERS ------------ */
    CPU_REG32  DEV_INT_STAT;                                    /* Dev int status.                                      */
    CPU_REG32  DEV_INT_EN;                                      /* Dev int enable.                                      */
    CPU_REG32  DEV_INT_CLR;                                     /* Dev int clear.                                       */
    CPU_REG32  DEV_INT_SET;                                     /* Dev int set.                                         */

                                                                /* -------------- DATA TRANSFER REGISTERS ------------- */
    CPU_REG32  CMD_CODE;                                        /* Command code.                                        */
    CPU_REG32  CMD_DATA;                                        /* Command data.                                        */

                                                                /* -------------- DATA TRANSFER REGISTERS ------------- */
    CPU_REG32  RX_DATA;                                         /* Receive  data.                                       */
    CPU_REG32  TX_DATA;                                         /* Transmit data.                                       */
    CPU_REG32  RX_PKT_LEN;                                      /* Receive  pkt len.                                    */
    CPU_REG32  TX_PKT_LEN;                                      /* Transmit pkt len.                                    */
    CPU_REG32  CTRL;                                            /* USB control.                                         */
    CPU_REG32  DEV_INT_PRI;                                     /* Dev int priority.                                    */

                                                                /* ----------- ENDPOINT INTERRUPT REGISTERS ----------- */
    CPU_REG32  EP_INT_STAT;                                     /* EP int status.                                       */
    CPU_REG32  EP_INT_EN;                                       /* EP int enable.                                       */
    CPU_REG32  EP_INT_CLR;                                      /* EP int clear.                                        */
    CPU_REG32  EP_INT_SET;                                      /* EP int set.                                          */
    CPU_REG32  EP_INT_PRIO;                                     /* EP int priority.                                     */

                                                                /* --------------- REALIZATION REGISTERS -------------- */
    CPU_REG32  RE_EP;                                           /* EP realize.                                          */
    CPU_REG32  EP_IX;                                           /* EP index.                                            */
    CPU_REG32  EP_MAX_PKT_SIZE;                                 /* EP maximum packet size.                              */

                                                                /* ------------------- DMA REGISTERS ------------------ */
    CPU_REG32  DMA_R_STAT;                                      /* DMA request status.                                  */
    CPU_REG32  DMA_R_CLR;                                       /* DMA request clear.                                   */
    CPU_REG32  DMA_R_SET;                                       /* DMA request set.                                     */
    CPU_REG32  RSVD0[9];
    CPU_REG32  UDCA_H;                                          /* DMA UDCA head.                                       */
    CPU_REG32  EP_DMA_STAT;                                     /* EP DMA status.                                       */
    CPU_REG32  EP_DMA_EN;                                       /* EP DMA enable.                                       */
    CPU_REG32  EP_DMA_DIS;                                      /* EP DMA disable.                                      */
    CPU_REG32  DMA_INT_STAT;                                    /* DMA int status.                                      */
    CPU_REG32  DMA_INT_EN;                                      /* DMA int enable.                                      */
    CPU_REG32  RSVD1[2];
    CPU_REG32  EO_INT_STA;                                      /* End of xfer    int status.                           */
    CPU_REG32  EO_INT_CLR;                                      /* End of xfer    int clear.                            */
    CPU_REG32  EO_INT_SET;                                      /* End of xfer    int set.                              */
    CPU_REG32  DD_INT_STA;                                      /* New DD request int status.                           */
    CPU_REG32  DD_INT_CLR;                                      /* New DD request int clear.                            */
    CPU_REG32  DD_INT_SET;                                      /* New DD request int set.                              */
    CPU_REG32  SYS_INT_STA;                                     /* System error   int status.                           */
    CPU_REG32  SYS_INT_CLR;                                     /* System error   int clear.                            */
    CPU_REG32  SYS_INT_SET;                                     /* System error   int set.                              */
}  USBD_LPCXXXX_REG;


/*
*********************************************************************************************************
*                                       LPCXXXXX ERROR COUNTERS
*********************************************************************************************************
*/

typedef  struct  usbd_lpcxx_err_ctrs {
    CPU_INT32U   PID;                                           /* PID encoding     error counter.                      */
    CPU_INT32U   UnPkt;                                         /* Unexpected pkt   error counter.                      */
    CPU_INT32U   DataCRC;                                       /* Data CRC         error counter.                      */
    CPU_INT32U   To;                                            /* Timeout          error counter.                      */
    CPU_INT32U   EndPkt;                                        /* End of packet    error counter.                      */
    CPU_INT32U   BufOvr;                                        /* Buffer overrun   error counter.                      */
    CPU_INT32U   BitStuff;                                      /* Bitstuff         error counter.                      */
    CPU_INT32U   Toggle;                                        /* Wrong toggle bit error counter.                      */
} USBD_LPCXXXX_ERR_CTRS;


/*
*********************************************************************************************************
*                                      DMA DESCRIPTOR DATA TYPE
*********************************************************************************************************
*/


typedef  struct  usbd_dma_desc {
    CPU_REG32   NextPtr;                                        /* Next descriptor pointer.                             */
    CPU_REG16   Ctrl;
    CPU_REG16   BufLen;
    CPU_REG32   BufPtr;
    CPU_REG32   Stat;                                           /* Descriptor status.                                   */
#if 0
    CPU_REG32   IsocPktMemAddr;
#endif
    CPU_REG32   BufMemPtr;                                      /* Dedicated memory temporary buffer ptr.               */
} USBD_DMA_DESC;


/*
*********************************************************************************************************
*                                          DRIVER DATA TYPE
*********************************************************************************************************
*/

typedef  struct  usbd_drv_data {                                /* ---------- COMMON FIFO/DMA DATA STRUCTURE ---------- */
    USBD_LPCXXXX_ERR_CTRS      ErrCtrs;                         /* Errors counters.                                     */
    CPU_INT32U                 EP_Prio;                         /* EP int priority.                                     */
    CPU_INT08U                 EP_SieStat[32];                  /* EP status.                                           */
    CPU_INT16U                 EP_PktSize[32];
    CPU_INT32U                 EP_IntEn;                        /* Bitmap with EP with interrupt enable.                */
    CPU_BOOLEAN                CtrlOutPktOverwritten;
    CPU_BOOLEAN                ConnectStat;
    CPU_INT32U                 CtrlOutDataBuf[64u / 4u];        /* Buffer that contains ctrl OUT data.                  */
    CPU_INT16U                 CtrlOutDataLen;                  /* Len of ctrl OUT data rxd.                            */
    CPU_BOOLEAN                CtrlOutDataRxd;                  /* Flag indicates data was rxd on ctrl OUT EP.          */
    CPU_BOOLEAN                CtrlOutDataIntEn;                /* Flag indicates data is expected on ctrl OUT EP.      */
    CPU_BOOLEAN                CtrlZLP_Rxd;                     /* Flag indicates ZLP was rxd on ctrl OUT EP.           */
} USBD_DRV_DATA;


typedef  struct  usbd_drv_data_fifo {
    USBD_LPCXXXX_ERR_CTRS      ErrCtrs;
    CPU_INT32U                 EP_Prio;
    CPU_INT08U                 EP_SieStat[32];
    CPU_INT16U                 EP_PktSize[32];
    CPU_INT32U                 EP_IntEn;
    CPU_BOOLEAN                CtrlOutPktOverwritten;
    CPU_BOOLEAN                ConnectStat;
    CPU_INT32U                 CtrlOutDataBuf[64u / 4u];
    CPU_INT16U                 CtrlOutDataLen;
    CPU_BOOLEAN                CtrlOutDataRxd;
    CPU_BOOLEAN                CtrlOutDataIntEn;
    CPU_BOOLEAN                CtrlZLP_Rxd;

    CPU_INT16U                 EP_RAM_Used;                     /* Local RAM used.                                      */
    CPU_INT32U                 EP_DoubleBuf;                    /* Bitmap with EP with double buffer.                   */
} USBD_DRV_DATA_FIFO;


typedef  struct  usbd_drv_data_dma {
    USBD_LPCXXXX_ERR_CTRS      ErrCtrs;
    CPU_INT32U                 EP_Prio;
    CPU_INT08U                 EP_SieStat[32];
    CPU_INT16U                 EP_PktSize[32];
    CPU_INT32U                 EP_IntEn;
    CPU_BOOLEAN                CtrlOutPktOverwritten;
    CPU_BOOLEAN                ConnectStat;
    CPU_INT32U                 CtrlOutDataBuf[64u / 4u];
    CPU_INT16U                 CtrlOutDataLen;
    CPU_BOOLEAN                CtrlOutDataRxd;
    CPU_BOOLEAN                CtrlOutDataIntEn;
    CPU_BOOLEAN                CtrlZLP_Rxd;

    USBD_DMA_DESC            **UDHCA_Ptr;                       /* USB dev comm area descriptors ptr.                   */
    USBD_DMA_DESC             *DescPtr;                         /* Descriptor array start ptr.                          */
} USBD_DRV_DATA_DMA;


/*
*********************************************************************************************************
*                                            LOCAL MACRO'S
*********************************************************************************************************
*/



/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

                                                                /* --------------- FIFO MODE DRIVER API  -------------- */
static  void         USBD_DrvInitFIFO      (USBD_DRV     *p_drv,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvStopFIFO      (USBD_DRV     *p_drv);

static  void         USBD_DrvEP_OpenFIFO   (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U    ep_type,
                                            CPU_INT16U    max_pkt_size,
                                            CPU_INT08U    transaction_frame,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvEP_CloseFIFO  (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr);

static  CPU_INT32U   USBD_DrvEP_RxStartFIFO(USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U   *p_buf,
                                            CPU_INT32U    buf_len,
                                            USBD_ERR     *p_err);

static  CPU_INT32U   USBD_DrvEP_TxFIFO     (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U   *p_buf,
                                            CPU_INT32U    buf_len,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvEP_TxStartFIFO(USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U   *p_buf,
                                            CPU_INT32U    buf_len,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvEP_TxZLP_FIFO (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            USBD_ERR     *p_err);

static  CPU_INT32U   USBD_DrvEP_RxFIFO     (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U   *p_buf,
                                            CPU_INT32U    buf_len,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvEP_RxZLP_FIFO (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            USBD_ERR     *p_err);

static  CPU_BOOLEAN  USBD_DrvEP_AbortFIFO  (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr);

                                                                /* --------------- DMA MODE DRIVER API  --------------- */
static  void         USBD_DrvInitDMA       (USBD_DRV     *p_drv,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvStopDMA       (USBD_DRV     *p_drv);

static  void         USBD_DrvEP_OpenDMA    (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U    ep_type,
                                            CPU_INT16U    max_pkt_size,
                                            CPU_INT08U    transaction_frame,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvEP_CloseDMA   (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr);

static  CPU_INT32U   USBD_DrvEP_RxStartDMA (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U   *p_buf,
                                            CPU_INT32U    buf_len,
                                            USBD_ERR     *p_err);

static  CPU_INT32U   USBD_DrvEP_TxDMA      (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U   *p_buf,
                                            CPU_INT32U    buf_len,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvEP_TxStartDMA (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U   *p_buf,
                                            CPU_INT32U    buf_len,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvEP_TxZLP_DMA  (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            USBD_ERR     *p_err);

static  CPU_INT32U   USBD_DrvEP_RxDMA      (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U   *p_buf,
                                            CPU_INT32U    buf_len,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvEP_RxZLP_DMA  (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            USBD_ERR     *p_err);

static  CPU_BOOLEAN  USBD_DrvEP_AbortDMA   (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr);

                                                                /* --------------- COMMON FIFO/DMA API  --------------- */

static  void         USBD_DrvStart         (USBD_DRV     *p_drv,
                                            USBD_ERR     *p_err);

static  CPU_BOOLEAN  USBD_DrvAddrSet       (USBD_DRV     *p_drv,
                                            CPU_INT08U    dev_addr);

static  void         USBD_DrvAddrEn        (USBD_DRV     *p_drv,
                                            CPU_INT08U    dev_addr);

static  CPU_BOOLEAN  USBD_DrvCfgSet        (USBD_DRV     *p_drv,
                                            CPU_INT08U    cfg_val);

static  void         USBD_DrvCfgClr        (USBD_DRV     *p_drv,
                                            CPU_INT08U    cfg_val);

static  CPU_INT16U   USBD_DrvGetFrameNbr   (USBD_DRV     *p_drv);

static  CPU_BOOLEAN  USBD_DrvEP_StallDMA   (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_BOOLEAN   state);

static  CPU_BOOLEAN  USBD_DrvEP_StallFIFO  (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_BOOLEAN   state);

static  void         USBD_DrvISR_Handler   (USBD_DRV     *p_drv);

                                                                /* ----- SERIAL INTERFACE ENGINE (SIE) FUNCTIONS  ----- */
static  CPU_BOOLEAN  LPCXXXX_SIE_RdCmd     (USBD_DRV     *p_drv,
                                            CPU_INT32U    sie_cmd,
                                            CPU_INT32U    sie_data,
                                            CPU_INT08U   *p_sie_rd_data);

static  CPU_BOOLEAN  LPCXXXX_SIE_RdTestReg (USBD_DRV     *p_drv);

static  CPU_BOOLEAN  LPCXXXX_SIE_WrCmd     (USBD_DRV     *p_drv,
                                            CPU_INT32U    sie_cmd,
                                            CPU_INT08U    sie_data);

                                                                /* ------------- DEVICE STATUS FUNCTIONS  ------------- */
static  void         LPCXXXX_DevErr        (USBD_DRV     *p_drv);

static  void         LPCXXXX_DevRegRst     (USBD_DRV     *p_drv);

static  void         LPCXXXX_DevStat       (USBD_DRV     *p_drv);

                                                                /* ------------------- EP FUNCTIONS ------------------- */
static  void         LPCXXXX_EP_Process    (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_phy_nbr);

static  CPU_INT16U   LPCXXXX_EP_SlaveRd    (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_phy_nbr,
                                            CPU_INT08U   *p_buf,
                                            CPU_INT16U    buf_len,
                                            CPU_INT08U   *p_sie_data);

static  void         LPCXXXX_EP_SlaveWr    (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_phy_nbr,
                                            CPU_INT08U   *p_buf,
                                            CPU_INT16U    ep_pkt_len);


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

USBD_DRV_API  USBD_DrvAPI_LPCXXXX_FIFO = { USBD_DrvInitFIFO,
                                           USBD_DrvStart,
                                           USBD_DrvStopFIFO,
                                           USBD_DrvAddrSet,
                                           USBD_DrvAddrEn,
                                           USBD_DrvCfgSet,
                                           USBD_DrvCfgClr,
                                           USBD_DrvGetFrameNbr,
                                           USBD_DrvEP_OpenFIFO,
                                           USBD_DrvEP_CloseFIFO,
                                           USBD_DrvEP_RxStartFIFO,
                                           USBD_DrvEP_RxFIFO,
                                           USBD_DrvEP_RxZLP_FIFO,
                                           USBD_DrvEP_TxFIFO,
                                           USBD_DrvEP_TxStartFIFO,
                                           USBD_DrvEP_TxZLP_FIFO,
                                           USBD_DrvEP_AbortFIFO,
                                           USBD_DrvEP_StallFIFO,
                                           USBD_DrvISR_Handler,
                                         };


USBD_DRV_API  USBD_DrvAPI_LPCXXXX_DMA = { USBD_DrvInitDMA,
                                          USBD_DrvStart,
                                          USBD_DrvStopDMA,
                                          USBD_DrvAddrSet,
                                          USBD_DrvAddrEn,
                                          USBD_DrvCfgSet,
                                          USBD_DrvCfgClr,
                                          USBD_DrvGetFrameNbr,
                                          USBD_DrvEP_OpenDMA,
                                          USBD_DrvEP_CloseDMA,
                                          USBD_DrvEP_RxStartDMA,
                                          USBD_DrvEP_RxDMA,
                                          USBD_DrvEP_RxZLP_DMA,
                                          USBD_DrvEP_TxDMA,
                                          USBD_DrvEP_TxStartDMA,
                                          USBD_DrvEP_TxZLP_DMA,
                                          USBD_DrvEP_AbortDMA,
                                          USBD_DrvEP_StallDMA,
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
*                                         USBD_DrvInitFIFO()
*
* Description : Initialize the device for FIFO mode operation.
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
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_DrvInitFIFO (USBD_DRV  *p_drv,
                                USBD_ERR  *p_err)
{
    USBD_LPCXXXX_REG    *p_reg;
    USBD_DRV_DATA_FIFO  *p_drv_data;
    USBD_DRV_BSP_API    *p_bsp_api;
    CPU_BOOLEAN          valid;
    LIB_ERR              lib_mem_err;
    CPU_SIZE_T           reqd_octets;
    CPU_SR_ALLOC();


                                                                /* Alloc drv internal data.                             */
    p_drv_data = (USBD_DRV_DATA_FIFO *)Mem_HeapAlloc(sizeof(USBD_DRV_DATA_FIFO),
                                                     sizeof(CPU_DATA),
                                                    &reqd_octets,
                                                    &lib_mem_err);
    if (p_drv_data == (void *)0) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

    CPU_CRITICAL_ENTER();
    p_drv->DataPtr = p_drv_data;                                /* Store drv internal data ptr.                         */
    CPU_CRITICAL_EXIT();

    p_bsp_api = p_drv->BSP_API_Ptr;                             /* Get drv BSP API ref.                                 */
    p_reg     = (USBD_LPCXXXX_REG *)p_drv->CfgPtr->BaseAddr;    /* Get USB ctrl reg ref.                                */

    p_bsp_api->Init(p_drv);                                     /* Init board/chip specific components.                 */

    DEF_BIT_CLR(p_reg->DEV_INT_EN, LPCXXXX_DEV_INT_ALL);        /* Disable all int.                                     */
    p_reg->DEV_INT_CLR = LPCXXXX_DEV_INT_ALL;                   /* Clr pending int.                                     */

    p_bsp_api->Disconn();                                       /* Disconnect USB at board/chip level.                  */
    p_drv_data->EP_RAM_Used = 32u;
    p_reg->EP_INT_CLR = 0xFFFFFFFFu;
    LPCXXXX_DevRegRst(p_drv);                                   /* Reset all reg.                                       */

    valid = LPCXXXX_SIE_RdTestReg(p_drv);                       /* Validate comm in the SIE.                            */
    if (valid == DEF_FAIL) {
        *p_err = USBD_ERR_FAIL;
         return;
    }

    valid = LPCXXXX_SIE_WrCmd(p_drv,
                              LPCXXXX_SIE_CMD_WR_SET_DEV_STAT,
                              DEF_BIT_NONE);
    if (valid == DEF_FAIL) {
        *p_err = USBD_ERR_FAIL;
         return;
    }

    valid = LPCXXXX_SIE_WrCmd(p_drv,
                              LPCXXXX_SIE_CMD_WR_SET_MODE,
                              LPCXXXX_SIE_CMD_SET_MODE_AP_CLK);
    if (valid == DEF_FAIL) {
        *p_err = USBD_ERR_FAIL;
         return;
    }

    p_drv_data->CtrlOutPktOverwritten = DEF_FALSE;
    p_drv_data->ConnectStat           = DEF_FALSE;

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          USBD_DrvInitDMA()
*
* Description : Initialize the device for DMA mode.
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
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_DrvInitDMA (USBD_DRV  *p_drv,
                               USBD_ERR  *p_err)
{
    USBD_LPCXXXX_REG   *p_reg;
    USBD_DRV_DATA_DMA  *p_drv_data;
    USBD_DRV_BSP_API   *p_bsp_api;
    USBD_DRV_CFG       *p_cfg;
    USBD_DRV_EP_INFO   *p_ep_tbl;
    USBD_DMA_DESC      *p_desc;
    CPU_ADDR            ded_mem_cur;
    CPU_ADDR            ded_mem_end;
    CPU_INT08U          ep_phy_nbr;
    LIB_ERR             lib_mem_err;
    CPU_SIZE_T          reqd_octets;
    CPU_BOOLEAN         valid;
    CPU_INT08U          ep_phy_nbr_max;
    CPU_SR_ALLOC();


    p_drv_data = (USBD_DRV_DATA_DMA *)Mem_HeapAlloc( sizeof(USBD_DRV_DATA_DMA),      /* Alloc drv internal data.                             */
                                                     sizeof(CPU_DATA),
                                                    &reqd_octets,
                                                    &lib_mem_err);
    if (p_drv_data == (void *)0) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

    ep_phy_nbr_max = USBD_EP_MaxPhyNbrGet(p_drv->DevNbr);       /* Get max phy EP used by the stack.                    */
    if (ep_phy_nbr_max == USBD_EP_PHY_NONE) {
        *p_err = USBD_ERR_FAIL;
         return;
    }

    ep_phy_nbr_max++;
    p_cfg = p_drv->CfgPtr;

    if ((p_cfg->MemAddr != 0x00000000u) &&                      /* Chk if ded mem is used.                              */
        (p_cfg->MemSize !=          0u)) {

        ded_mem_cur = p_cfg->MemAddr;
        ded_mem_end = p_cfg->MemAddr + p_cfg->MemSize;          /* ... Calc ded mem end addr.                           */

        if (ded_mem_cur %  LPCXXXX_UDCA_ALIGN != 0u) {          /* ... Align USB dev comm area.                         */
            ded_mem_cur += LPCXXXX_UDCA_ALIGN;
            ded_mem_cur &= LPCXXXX_UDCA_ALIGN_MSK;
        }

        p_drv_data->UDHCA_Ptr  = (USBD_DMA_DESC **)ded_mem_cur;
        ded_mem_cur           +=  LPCXXXX_UDCA_ALIGN;

        p_drv_data->DescPtr    = (USBD_DMA_DESC *)ded_mem_cur;  /* ... Alloc space for EP desc.                         */
        ded_mem_cur           += (ep_phy_nbr_max - 2u) * sizeof(USBD_DMA_DESC);

        Mem_Clr((void *)p_drv_data->DescPtr,
                       (ep_phy_nbr_max - 2u) * sizeof(USBD_DMA_DESC));

        if (ded_mem_cur %  CPU_WORD_SIZE_32 != 0u) {
            ded_mem_cur += CPU_WORD_SIZE_32 - (ded_mem_cur % CPU_WORD_SIZE_32);
        }

        p_ep_tbl   = p_cfg->EP_InfoTbl;                         /* Alloc buf area for non-ctrl EPs based on cfg tbl.    */
        ep_phy_nbr = 2u;

        if (ded_mem_cur < ded_mem_end) {
            while ((p_ep_tbl[ep_phy_nbr].Attrib != DEF_BIT_NONE)   &&
                   (ep_phy_nbr                  <  ep_phy_nbr_max) &&
                   (ded_mem_cur                 <  ded_mem_end)) {

                p_desc            = &p_drv_data->DescPtr[ep_phy_nbr - 2u];
                p_desc->BufMemPtr =  ded_mem_cur;

                ded_mem_cur                       += p_ep_tbl[ep_phy_nbr].MaxPktSize;
                p_drv_data->EP_PktSize[ep_phy_nbr] = p_ep_tbl[ep_phy_nbr].MaxPktSize;

                                                                /* ... Alloc buf to word-aligned addr.                  */
                if (ded_mem_cur %  CPU_WORD_SIZE_32) {
                    ded_mem_cur += CPU_WORD_SIZE_32 - (ded_mem_cur % CPU_WORD_SIZE_32);
                }
                ep_phy_nbr++;
            }
        }

    } else {

       p_drv_data->UDHCA_Ptr = (USBD_DMA_DESC **)Mem_HeapAlloc( ep_phy_nbr_max * sizeof(USBD_DMA_DESC *),
                                                                LPCXXXX_UDCA_ALIGN,
                                                               &reqd_octets,
                                                               &lib_mem_err);
        if (lib_mem_err != LIB_MEM_ERR_NONE) {
            *p_err = USBD_ERR_ALLOC;
             return;
        }

        p_drv_data->DescPtr = (USBD_DMA_DESC *)Mem_HeapAlloc((ep_phy_nbr_max - 2u) * sizeof(USBD_DMA_DESC),
                                                              CPU_WORD_SIZE_32,
                                                             &reqd_octets,
                                                             &lib_mem_err);
        if (lib_mem_err != LIB_MEM_ERR_NONE) {
            *p_err = USBD_ERR_ALLOC;
             return;
        }
    }

    Mem_Clr((void *)p_drv_data->UDHCA_Ptr,
                    ep_phy_nbr_max * sizeof(USBD_DMA_DESC *));

    CPU_CRITICAL_ENTER();
    p_drv->DataPtr = p_drv_data;                                /* Store drv local data in drv struct.                  */
    CPU_CRITICAL_EXIT();

    p_bsp_api = p_drv->BSP_API_Ptr;                             /* Get drv BSP API ref.                                 */
    p_reg     = (USBD_LPCXXXX_REG *)p_drv->CfgPtr->BaseAddr;    /* Get dev ctrl reg ref.                                */
    p_bsp_api->Init(p_drv);                                     /* Init board/chip specific components.                 */

    DEF_BIT_CLR(p_reg->DEV_INT_EN, LPCXXXX_DEV_INT_ALL);        /* Disable all dev int.                                 */
    p_reg->DEV_INT_CLR = LPCXXXX_DEV_INT_ALL;                   /* Clear pending int.                                   */
    p_reg->DMA_INT_EN  = DEF_BIT_NONE;                          /* Disable DMA int.                                     */
    p_reg->EP_INT_CLR  = 0xFFFFFFFFu;
    p_reg->EP_DMA_DIS  = 0xFFFFFFFFu;                           /* Disable EP DMA.                                      */

    LPCXXXX_DevRegRst(p_drv);                                   /* Reset all reg.                                       */

    valid = LPCXXXX_SIE_RdTestReg(p_drv);                       /* Validate comm in SIE.                                */
    if (valid == DEF_FAIL) {
        *p_err = USBD_ERR_FAIL;
         return;
    }

    valid = LPCXXXX_SIE_WrCmd(p_drv,
                              LPCXXXX_SIE_CMD_WR_SET_DEV_STAT,
                              DEF_BIT_NONE);
    if (valid == DEF_FAIL) {
        *p_err = USBD_ERR_FAIL;
         return;
    }

    valid = LPCXXXX_SIE_WrCmd(p_drv,
                              LPCXXXX_SIE_CMD_WR_SET_MODE,
                              LPCXXXX_SIE_CMD_SET_MODE_AP_CLK  |
                              LPCXXXX_SIE_CMD_SET_MODE_INAK_II |
                              LPCXXXX_SIE_CMD_SET_MODE_INAK_BI);
    if (valid == DEF_FAIL) {
        *p_err = USBD_ERR_FAIL;
         return;
    }

    p_reg->UDCA_H                     = (CPU_INT32U)p_drv_data->UDHCA_Ptr;
    p_drv_data->CtrlOutPktOverwritten =  DEF_FALSE;
    p_drv_data->ConnectStat           =  DEF_FALSE;

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
    USBD_LPCXXXX_REG  *p_reg;
    USBD_DRV_BSP_API  *p_bsp_api;
    CPU_BOOLEAN        valid;


    p_bsp_api = p_drv->BSP_API_Ptr;                             /* Get drv BSP API ref.                                 */
    p_reg     = (USBD_LPCXXXX_REG *)p_drv->CfgPtr->BaseAddr;    /* Get USB ctrl reg ref.                                */


    valid = LPCXXXX_SIE_WrCmd(p_drv,
                              LPCXXXX_SIE_CMD_WR_SET_DEV_STAT,
                              LPCXXXX_SIE_CMD_DEV_CON);
    if (valid == DEF_FAIL) {
        *p_err = USBD_ERR_FAIL;
         return;
    }

    p_bsp_api->Conn();

    p_reg->DEV_INT_PRI = LPCXXXX_DEV_INT_PRI_EP_FAST;

    DEF_BIT_SET(p_reg->DEV_INT_EN, LPCXXXX_DEV_INT_DEV_STAT |
                                   LPCXXXX_DEV_INT_ERR);

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         USBD_DrvStopFIFO()
*
* Description : Stop device operation.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_DrvStopFIFO (USBD_DRV  *p_drv)
{
    USBD_DRV_BSP_API  *p_bsp_api;
    USBD_LPCXXXX_REG  *p_reg;


    p_reg     = (USBD_LPCXXXX_REG *)p_drv->CfgPtr->BaseAddr;    /* Get USB ctrl reg ref.                                */
    p_bsp_api = p_drv->BSP_API_Ptr;                             /* Get drv BSP API ref.                                 */


    DEF_BIT_CLR(p_reg->DEV_INT_EN, LPCXXXX_DEV_INT_ALL);        /* Disable all int.                                     */
    p_reg->DEV_INT_CLR = LPCXXXX_DEV_INT_ALL;                   /* Clr pending int.                                     */

    p_bsp_api->Disconn();

    (void)LPCXXXX_SIE_WrCmd(p_drv,
                            LPCXXXX_SIE_CMD_WR_SET_DEV_STAT,
                            0);
}


/*
*********************************************************************************************************
*                                          USBD_DrvStopDMA()
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

static  void  USBD_DrvStopDMA (USBD_DRV  *p_drv)
{
    USBD_DRV_BSP_API   *p_bsp_api;
    USBD_LPCXXXX_REG   *p_reg;
    USBD_DRV_DATA_DMA  *p_drv_data;
    USBD_DMA_DESC      *p_desc;
    CPU_INT08U          ep_phy_nbr;
    CPU_INT08U          ep_phy_nbr_max;


    p_reg     = (USBD_LPCXXXX_REG *)p_drv->CfgPtr->BaseAddr;    /* Get USB ctrl reg ref.                                */
    p_bsp_api = p_drv->BSP_API_Ptr;                             /* Get drv BSP API ref.                                 */


    DEF_BIT_CLR(p_reg->DEV_INT_EN, LPCXXXX_DEV_INT_ALL);        /* Disable all int.                                     */
    p_reg->DEV_INT_CLR = LPCXXXX_DEV_INT_ALL;                   /* Clr pending int.                                     */

    p_bsp_api->Disconn();

    (void)LPCXXXX_SIE_WrCmd(p_drv,
                            LPCXXXX_SIE_CMD_WR_SET_DEV_STAT,
                            0);

    p_drv_data        = (USBD_DRV_DATA_DMA *)p_drv->DataPtr;
    ep_phy_nbr_max    =  USBD_EP_MaxPhyNbrGet(p_drv->DevNbr);
    p_reg->EO_INT_CLR =  0xFFFFFFFFu;
    p_reg->EP_DMA_DIS =  0xFFFFFFFFu;

    if (ep_phy_nbr_max != USBD_EP_PHY_NONE) {
        for (ep_phy_nbr = 2u; ep_phy_nbr < ep_phy_nbr_max; ep_phy_nbr++) {
            p_desc         =  p_drv_data->UDHCA_Ptr[ep_phy_nbr];
            if (p_desc != (USBD_DMA_DESC *)0) {
                p_desc->BufPtr = 0x00000000u;
                p_desc->BufLen = 0x0000u;
            }
        }
    }
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
    CPU_BOOLEAN  valid;


    valid = LPCXXXX_SIE_WrCmd(p_drv,
                              LPCXXXX_SIE_CMD_WR_SET_ADDR,
                              LPCXXXX_SIE_CMD_SET_ADDR_DEV_EN | dev_addr);

    return (valid);
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
    CPU_BOOLEAN  valid;


    (void)cfg_val;

    valid = LPCXXXX_SIE_WrCmd(p_drv,
                              LPCXXXX_SIE_CMD_WR_CFG_DEV,
                              LPCXXXX_SIE_CMD_CFG_DEV_EN);

    return (valid);
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

    (void)LPCXXXX_SIE_WrCmd(p_drv,
                            LPCXXXX_SIE_CMD_WR_CFG_DEV,
                            0u);
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
    CPU_INT16U   frm_nbr;
    CPU_INT08U   temp;
    CPU_BOOLEAN  valid;
    CPU_SR_ALLOC();


    (void)p_drv;

    CPU_CRITICAL_ENTER();
    valid = LPCXXXX_SIE_RdCmd( p_drv,
                               LPCXXXX_SIE_CMD_RD_CUR_FRM_NBR,
                               LPCXXXX_SIE_DATA_RD_CUR_FRM_NBR,
                              &temp);
    CPU_CRITICAL_EXIT();
    if (valid != DEF_OK) {
        return (0u);
    }

    frm_nbr = temp;

    CPU_CRITICAL_ENTER();
    valid = LPCXXXX_SIE_RdCmd( p_drv,
                               LPCXXXX_SIE_CMD_RD_CUR_FRM_NBR,
                               LPCXXXX_SIE_DATA_RD_CUR_FRM_NBR,
                              &temp);
    CPU_CRITICAL_EXIT();
    if (valid != DEF_OK) {
        return (0u);
    }

    frm_nbr |= temp << 8u;

    return (frm_nbr);
}


/*
*********************************************************************************************************
*                                        USBD_DrvEP_OpenFIFO()
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

static  void  USBD_DrvEP_OpenFIFO (USBD_DRV    *p_drv,
                                   CPU_INT08U   ep_addr,
                                   CPU_INT08U   ep_type,
                                   CPU_INT16U   max_pkt_size,
                                   CPU_INT08U   transaction_frame,
                                   USBD_ERR    *p_err)
{
    USBD_LPCXXXX_REG    *p_reg;
    USBD_DRV_DATA_FIFO  *p_drv_data;
    CPU_INT08U           ep_log_nbr;
    CPU_INT08U           ep_phy_nbr;
    CPU_INT16U           reg_to;
    CPU_INT16U           ram_used;
    CPU_BOOLEAN          double_buf;
    CPU_BOOLEAN          valid;
    CPU_SR_ALLOC();


    (void)transaction_frame;

    p_reg      = (USBD_LPCXXXX_REG   *)p_drv->CfgPtr->BaseAddr;
    p_drv_data = (USBD_DRV_DATA_FIFO *)p_drv->DataPtr;
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    double_buf =  DEF_NO;
    reg_to     =  LPCXXXX_REG_TO;

    CPU_CRITICAL_ENTER();
    ram_used = p_drv_data->EP_RAM_Used;


    switch (ep_type) {
        case USBD_EP_TYPE_CTRL:
        case USBD_EP_TYPE_INTR:
             ram_used += ((max_pkt_size + 3u) / 4u) + 1u;
             break;

        case USBD_EP_TYPE_BULK:
        case USBD_EP_TYPE_ISOC:
             double_buf = DEF_YES;
             ram_used += 2u * (((max_pkt_size + 3u) / 4u) + 1u);
             break;

        default:
             CPU_CRITICAL_EXIT();
            *p_err = USBD_ERR_INVALID_ARG;
             return;
    }

    if (ram_used > LPCXXXX_EP_RAM_SIZE_MAX) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_ALLOC;
        return;
    }

    DEF_BIT_SET(p_reg->RE_EP, DEF_BIT32(ep_phy_nbr));
    p_reg->EP_IX           = ep_phy_nbr;
    p_reg->EP_MAX_PKT_SIZE = max_pkt_size;

    while ((DEF_BIT_IS_CLR(p_reg->DEV_INT_STAT, LPCXXXX_DEV_INT_EP_RLZED)) &&
           (reg_to > 0u)) {
        reg_to--;
    }

    if (reg_to == 0u) {
        CPU_CRITICAL_EXIT();
        USBD_DBG_DRV_EP("  Drv EP FIFO Open failed (timeout ep rlzed)", ep_addr);
        *p_err = USBD_ERR_FAIL;
        return;
    }

    p_drv_data->EP_RAM_Used = ram_used;

    if (double_buf == DEF_YES) {
        DEF_BIT_SET(p_drv_data->EP_DoubleBuf, DEF_BIT32(ep_phy_nbr));
    } else {
        DEF_BIT_CLR(p_drv_data->EP_DoubleBuf, DEF_BIT32(ep_phy_nbr));
    }

    p_drv_data->EP_PktSize[ep_phy_nbr] = max_pkt_size;
    p_reg->DEV_INT_CLR                 = LPCXXXX_DEV_INT_EP_RLZED;
                                                                /* ----- ENABLE INTERRUPTS FOR CONTROL AND OUT EP ----- */
    if (ep_log_nbr == 0) {
        DEF_BIT_SET(p_drv_data->EP_Prio, DEF_BIT32(ep_phy_nbr));
    } else {
        DEF_BIT_CLR(p_drv_data->EP_Prio, DEF_BIT32(ep_phy_nbr));
    }

#if 0
    if (ep_phy_nbr == 0u) {
        DEF_BIT_SET(p_reg->EP_INT_EN, DEF_BIT_00);
    }
#else
    if (( ep_log_nbr       == 0u) ||
        ((ep_phy_nbr % 2u) == 0u)) {
        DEF_BIT_SET(p_reg->EP_INT_EN, DEF_BIT32(ep_phy_nbr));
    }
#endif

    p_reg->EP_INT_PRIO = p_drv_data->EP_Prio;

    DEF_BIT_CLR(p_drv_data->EP_SieStat[ep_phy_nbr], LPCXXXX_SIE_CMD_SET_EPx_STAT_DA);

    valid = LPCXXXX_SIE_WrCmd(p_drv,
                              LPCXXXX_SIE_CMD_WR_SET_EP_STAT_BASE | (ep_phy_nbr << 16u),
                              p_drv_data->EP_SieStat[ep_phy_nbr]);

    CPU_CRITICAL_EXIT();

    if (valid == DEF_FAIL) {
        USBD_DBG_DRV_EP("  Drv EP FIFO Open failed (set ep stat)", ep_addr);
       *p_err =  USBD_ERR_FAIL;
    } else {
        USBD_DBG_DRV_EP("  Drv EP FIFO Open", ep_addr);
       *p_err =  USBD_ERR_NONE;
    }
}


/*
*********************************************************************************************************
*                                        USBD_DrvEP_OpenDMA()
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

static  void  USBD_DrvEP_OpenDMA (USBD_DRV    *p_drv,
                                  CPU_INT08U   ep_addr,
                                  CPU_INT08U   ep_type,
                                  CPU_INT16U   max_pkt_size,
                                  CPU_INT08U   transaction_frame,
                                  USBD_ERR    *p_err)
{
    USBD_LPCXXXX_REG   *p_reg;
    USBD_DRV_DATA_DMA  *p_drv_data;
    USBD_DMA_DESC      *p_desc;
    CPU_INT08U          ep_phy_nbr;
    CPU_INT08U          sie_data;
    CPU_INT16U          reg_to;
    CPU_BOOLEAN         valid;
    CPU_SR_ALLOC();


    (void)ep_type;
    (void)transaction_frame;

    p_reg      = (USBD_LPCXXXX_REG   *)p_drv->CfgPtr->BaseAddr;
    p_drv_data = (USBD_DRV_DATA_DMA  *)p_drv->DataPtr;
    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
    reg_to     = LPCXXXX_REG_TO;

    if ((ep_phy_nbr != 0u) &&
        (ep_phy_nbr != 1u)) {
                                                                /* Get desc for non-ctrl EPs.                           */
        p_desc          = &p_drv_data->DescPtr[ep_phy_nbr - 2u];
        p_desc->NextPtr =  0x00000000;                          /* Init desc.                                           */
        p_desc->Ctrl    = (max_pkt_size << 5u);
        p_desc->BufLen  =  0u;
        p_desc->Stat    =  DEF_BIT_NONE;
    }

    CPU_CRITICAL_ENTER();

    DEF_BIT_SET(p_reg->RE_EP, DEF_BIT32(ep_phy_nbr));
    p_reg->EP_IX           = ep_phy_nbr;
    p_reg->EP_MAX_PKT_SIZE = max_pkt_size;

    while ((DEF_BIT_IS_CLR(p_reg->DEV_INT_STAT, LPCXXXX_DEV_INT_EP_RLZED)) &&
           (reg_to > 0u)) {
        reg_to--;
    }

    if (reg_to == 0u) {
        CPU_CRITICAL_EXIT();
        USBD_DBG_DRV_EP("  Drv EP DMA Open failed (timeout ep rlzed)", ep_addr);
        *p_err = USBD_ERR_FAIL;
        return;
    }

    p_reg->DEV_INT_CLR = LPCXXXX_DEV_INT_EP_RLZED;
                                                                /* ----- ENABLE INTERRUPTS FOR CONTROL AND OUT EP ----- */
    if ((ep_phy_nbr != 0u) &&
        (ep_phy_nbr != 1u)) {
        p_drv_data->UDHCA_Ptr[ep_phy_nbr] = p_desc;
    } else {
        DEF_BIT_SET(p_drv_data->EP_Prio, DEF_BIT32(ep_phy_nbr));
        p_drv_data->EP_PktSize[ep_phy_nbr] = max_pkt_size;
    }

    p_reg->EP_INT_PRIO = p_drv_data->EP_Prio;

    if (ep_phy_nbr == 0u) {
        DEF_BIT_SET(p_reg->EP_INT_EN, DEF_BIT_00);
    }

                                                                /* En EP with Set Endpoint Status cmd.                  */
    DEF_BIT_CLR(p_drv_data->EP_SieStat[ep_phy_nbr], LPCXXXX_SIE_CMD_SET_EPx_STAT_DA);
    valid = LPCXXXX_SIE_WrCmd(p_drv,
                              LPCXXXX_SIE_CMD_WR_SET_EP_STAT_BASE | (ep_phy_nbr << 16u),
                              p_drv_data->EP_SieStat[ep_phy_nbr]);
                                                                /* Clr EP int in USBEpIntSt reg with Select Endpoint/...*/
                                                                /* ...Clear Interrupt cmd.                              */
    (void)LPCXXXX_SIE_RdCmd(p_drv,
                            LPCXXXX_SIE_CMD_RD_CLR_EP_BASE  | (ep_phy_nbr << 16),
                            LPCXXXX_SIE_DATA_RD_CLR_EP_BASE | (ep_phy_nbr << 16),
                           &sie_data);
    CPU_CRITICAL_EXIT();

    if (valid == DEF_FAIL) {
        USBD_DBG_DRV_EP("  Drv EP DMA Open failed (set ep stat)", ep_addr);
       *p_err  = USBD_ERR_FAIL;
    } else {
        USBD_DBG_DRV_EP("  Drv EP DMA Open", ep_addr);
       *p_err  = USBD_ERR_NONE;
    }
}


/*
*********************************************************************************************************
*                                       USBD_DrvEP_CloseFIFO()
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

static  void  USBD_DrvEP_CloseFIFO (USBD_DRV    *p_drv,
                                    CPU_INT08U   ep_addr)
{
    USBD_LPCXXXX_REG    *p_reg;
    USBD_DRV_DATA_FIFO  *p_drv_data;
    CPU_INT08U           ep_phy_nbr;
    CPU_INT16U           reg_to;
    CPU_INT16U           ram_used;
    CPU_SR_ALLOC();


    p_reg      = (USBD_LPCXXXX_REG   *)p_drv->CfgPtr->BaseAddr;
    p_drv_data = (USBD_DRV_DATA_FIFO *)p_drv->DataPtr;
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    ram_used   = ((p_drv_data->EP_PktSize[ep_phy_nbr] + 3u) / 4u) + 1u;


    CPU_CRITICAL_ENTER();

    if (DEF_BIT_IS_SET(p_drv_data->EP_DoubleBuf, DEF_BIT32(ep_phy_nbr)) == DEF_YES) {
        ram_used *= 2u;
    }

    if (ram_used > p_drv_data->EP_RAM_Used) {
        CPU_CRITICAL_EXIT();
        USBD_DBG_DRV_EP("  Drv EP FIFO Close failed (out of ram)", ep_addr);
        return;
    }

    DEF_BIT_CLR(p_reg->RE_EP, DEF_BIT32(ep_phy_nbr));

    DEF_BIT_CLR(p_drv_data->EP_Prio, DEF_BIT32(ep_phy_nbr));
    p_reg->EP_INT_PRIO = p_drv_data->EP_Prio;

    if (ep_phy_nbr > 1u) {
        DEF_BIT_CLR(p_reg->EP_INT_EN, DEF_BIT32(ep_phy_nbr));
    }

    reg_to = LPCXXXX_REG_TO;

    while ((DEF_BIT_IS_CLR(p_reg->DEV_INT_STAT, LPCXXXX_DEV_INT_EP_RLZED)) &&
           (reg_to > 0)) {
        reg_to--;
    }

    if (reg_to == 0u) {
        CPU_CRITICAL_EXIT();
        USBD_DBG_DRV_EP("  Drv EP FIFO Close failed (timeout ep rlzed)", ep_addr);
        return;
    }

    DEF_BIT_CLR(p_drv_data->EP_DoubleBuf, DEF_BIT32(ep_phy_nbr));
    p_drv_data->EP_RAM_Used -= ram_used;

    p_reg->DEV_INT_CLR = LPCXXXX_DEV_INT_EP_RLZED;
    CPU_CRITICAL_EXIT();

    USBD_DBG_DRV_EP("  Drv EP FIFO Close", ep_addr);
}


/*
*********************************************************************************************************
*                                        USBD_DrvEP_CloseDMA()
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

static  void  USBD_DrvEP_CloseDMA (USBD_DRV    *p_drv,
                                   CPU_INT08U   ep_addr)
{
    USBD_LPCXXXX_REG   *p_reg;
    USBD_DRV_DATA_DMA  *p_drv_data;
    CPU_INT08U          ep_phy_nbr;
    CPU_INT16U          reg_to;
    CPU_SR_ALLOC();


    p_reg      = (USBD_LPCXXXX_REG   *)p_drv->CfgPtr->BaseAddr;
    p_drv_data = (USBD_DRV_DATA_DMA  *)p_drv->DataPtr;
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);

    CPU_CRITICAL_ENTER();

    DEF_BIT_CLR(p_reg->RE_EP, DEF_BIT32(ep_phy_nbr));
    reg_to = LPCXXXX_REG_TO;

    while ((DEF_BIT_IS_CLR(p_reg->DEV_INT_STAT, LPCXXXX_DEV_INT_EP_RLZED)) &&
           (reg_to > 0)) {
        reg_to--;
    }

    CPU_CRITICAL_EXIT();

    if (reg_to == 0u) {
        USBD_DBG_DRV_EP("  Drv EP DMA Close failed (timeout ep rlzed)", ep_addr);
        return;
    }
    p_reg->DEV_INT_CLR = LPCXXXX_DEV_INT_EP_RLZED;

    if ((ep_phy_nbr != 0u) &&
        (ep_phy_nbr != 1u)) {
        CPU_CRITICAL_ENTER();
        p_drv_data->UDHCA_Ptr[ep_phy_nbr] = (USBD_DMA_DESC *)0;
        CPU_CRITICAL_EXIT();
        if ((ep_phy_nbr % 2) == 0u) {
            p_reg->DMA_R_CLR  = DEF_BIT32(ep_phy_nbr);
            p_reg->EP_DMA_DIS = DEF_BIT32(ep_phy_nbr);
        }
    } else {
        CPU_CRITICAL_ENTER();
        p_drv_data->EP_PktSize[ep_phy_nbr] = 0u;
        DEF_BIT_CLR(p_reg->EP_INT_EN, DEF_BIT32(0u));
        CPU_CRITICAL_EXIT();
    }

    USBD_DBG_DRV_EP("  Drv EP DMA Close", ep_addr);
}


/*
*********************************************************************************************************
*                                      USBD_DrvEP_RxStartFIFO()
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

static  CPU_INT32U  USBD_DrvEP_RxStartFIFO (USBD_DRV    *p_drv,
                                            CPU_INT08U   ep_addr,
                                            CPU_INT08U  *p_buf,
                                            CPU_INT32U   buf_len,
                                            USBD_ERR    *p_err)
{
    USBD_DRV_DATA_FIFO  *p_drv_data;
    CPU_INT08U           ep_phy_nbr;
    CPU_BOOLEAN          valid;
    CPU_INT16U           ep_max_pkt_size;
    CPU_INT08U           sie_data;
    CPU_SR_ALLOC();


    (void)p_buf;

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
    p_drv_data = p_drv->DataPtr;

    ep_max_pkt_size = USBD_EP_MaxPktSizeGet(p_drv->DevNbr,
                                            ep_addr,
                                            p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (0u);
    }

    CPU_CRITICAL_ENTER();
    valid = LPCXXXX_SIE_RdCmd(p_drv,
                              LPCXXXX_SIE_CMD_RD_SEL_EP_BASE  | (ep_phy_nbr << 16),
                              LPCXXXX_SIE_DATA_RD_SEL_EP_BASE | (ep_phy_nbr << 16),
                             &sie_data);
    if (valid != DEF_OK) {
        CPU_CRITICAL_EXIT();

       *p_err = USBD_ERR_RX;
        USBD_DBG_DRV_EP("  Drv EP FIFO Rx Start failed (sel EP)", ep_addr);
        return (0u);
    }

    if ((p_drv_data->CtrlZLP_Rxd == DEF_YES) &&                 /* If ZLP was already recevied on control endpoint.     */
        (buf_len                 == 0u) &&
        (ep_phy_nbr              == 0u)) {

        USBD_DBG_DRV_EP_ARG("  Drv EP FIFO Rx Start Pkt (Ctrl ZLP Rdy)", ep_addr, buf_len);

        p_drv_data->CtrlZLP_Rxd = DEF_NO;

        USBD_EP_RxCmpl(p_drv, USBD_EP_PHY_TO_LOG(ep_phy_nbr));

    } else if ((ep_phy_nbr                 == 0u) &&            /* If data was already received on ctrl OUT EP.         */
               (p_drv_data->CtrlOutDataRxd == DEF_YES)) {

        USBD_DBG_DRV_EP_ARG("  Drv EP FIFO Rx Start Pkt (Ctrl OUT data Rdy)", ep_addr, buf_len);

        p_drv_data->CtrlOutDataRxd = DEF_NO;

        USBD_EP_RxCmpl(p_drv, USBD_EP_PHY_TO_LOG(ep_phy_nbr));
                                                                /* if data was already received on any other EPs.       */
    } else if (DEF_BIT_IS_SET(sie_data, LPCXXXX_SIE_CMD_SEL_EPx_F_E) == DEF_YES) {

        USBD_DBG_DRV_EP_ARG("  Drv EP FIFO Rx Start Pkt (Rdy)", ep_addr, buf_len);

        USBD_EP_RxCmpl(p_drv, USBD_EP_PHY_TO_LOG(ep_phy_nbr));
    } else {                                                    /* Nothing received yet, prepare for reception.         */
        USBD_DBG_DRV_EP_ARG("  Drv EP FIFO Rx Start Pkt (Int en)", ep_addr, buf_len);

        if ((ep_phy_nbr != 0u) ||
            (buf_len    == 0u)) {
            DEF_BIT_SET(p_drv_data->EP_IntEn, DEF_BIT32(ep_phy_nbr));
        } else {
            p_drv_data->CtrlOutDataIntEn = DEF_YES;
        }
    }
    CPU_CRITICAL_EXIT();

   *p_err = USBD_ERR_NONE;

    return (DEF_MIN(buf_len, ep_max_pkt_size));
}


/*
*********************************************************************************************************
*                                       USBD_DrvEP_RxStartDMA()
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

static  CPU_INT32U  USBD_DrvEP_RxStartDMA (USBD_DRV    *p_drv,
                                           CPU_INT08U   ep_addr,
                                           CPU_INT08U  *p_buf,
                                           CPU_INT32U   buf_len,
                                           USBD_ERR    *p_err)
{
    USBD_LPCXXXX_REG   *p_reg;
    USBD_DRV_DATA_DMA  *p_drv_data;
    USBD_DRV_CFG       *p_cfg;
    USBD_DMA_DESC      *p_desc;
    CPU_INT16U          ep_max_pkt_size;
    CPU_INT08U          ep_phy_nbr;
    CPU_INT32U          rtn_len;


    p_reg      = (USBD_LPCXXXX_REG  *)(p_drv->CfgPtr->BaseAddr);
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);

    if ((ep_phy_nbr == 0u) ||
        (ep_phy_nbr == 1u)) {

        rtn_len = USBD_DrvEP_RxStartFIFO(p_drv,
                                         ep_addr,
                                         p_buf,
                                         buf_len,
                                         p_err);

    } else {
        p_drv_data      = (USBD_DRV_DATA_DMA *)p_drv->DataPtr;
        p_cfg           =  p_drv->CfgPtr;
        p_desc          =  p_drv_data->UDHCA_Ptr[ep_phy_nbr];
        p_desc->NextPtr =  0x00000000;                          /* Init desc.                                           */

        if ((p_cfg->MemAddr != 0x00000000u) &&
            (p_cfg->MemSize !=          0u)) {
            ep_max_pkt_size = p_drv_data->EP_PktSize[ep_phy_nbr];
            if (buf_len > ep_max_pkt_size) {
                p_desc->BufLen = ep_max_pkt_size;
            } else {
                p_desc->BufLen = (CPU_INT16U)buf_len;
            }
            p_desc->BufPtr = p_desc->BufMemPtr;
        } else {
            p_desc->BufLen = (CPU_INT16U)buf_len;
            p_desc->BufPtr = (CPU_INT32U)p_buf;
        }

        rtn_len = p_desc->BufLen;

        p_desc->Stat = DEF_BIT_NONE;

        USBD_DBG_DRV_EP_ARG("  Drv EP DMA Rx Start Len:", ep_addr, p_desc->BufLen);

        p_reg->EP_DMA_EN = DEF_BIT32(ep_phy_nbr);

       *p_err = USBD_ERR_NONE;
   }

    return (rtn_len);
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
*                               USBD_ERR_SHORT_XFER     Short transfer successfully received.
*                               USBD_ERR_RX             Generic Rx error.
*
* Return(s)   : Number of octets received, if NO error(s).
*
*               0,                         otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT32U  USBD_DrvEP_RxFIFO (USBD_DRV    *p_drv,
                                       CPU_INT08U   ep_addr,
                                       CPU_INT08U  *p_buf,
                                       CPU_INT32U   buf_len,
                                       USBD_ERR    *p_err)
{
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT08U      ep_phy_nbr;
    CPU_INT16U      ep_pkt_len;
    CPU_INT08U      sie_data;
    CPU_SR_ALLOC();


    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);;
    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;

    DEF_BIT_CLR(p_drv_data->EP_IntEn, DEF_BIT32(ep_phy_nbr));

    if (ep_phy_nbr != 0u) {
        ep_pkt_len = LPCXXXX_EP_SlaveRd(p_drv, ep_phy_nbr, p_buf, buf_len, &sie_data);
    } else {
        CPU_CRITICAL_ENTER();
        ep_pkt_len = p_drv_data->CtrlOutDataLen;

        if (ep_pkt_len != 0u) {
            Mem_Copy(p_buf, &p_drv_data->CtrlOutDataBuf[0u], ep_pkt_len);
        }
        CPU_CRITICAL_EXIT();

        USBD_DBG_DRV_EP_ARG("  Drv EP FIFO Rx 0 Len:", ep_addr, ep_pkt_len);
    }

    if (ep_pkt_len <= buf_len) {
       *p_err = USBD_ERR_NONE;
    } else {
        ep_pkt_len = buf_len;
       *p_err      = USBD_ERR_DRV_BUF_OVERFLOW;
    }

    return (ep_pkt_len);
}


/*
*********************************************************************************************************
*                                         USBD_DrvEP_RxDMA()
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

static  CPU_INT32U  USBD_DrvEP_RxDMA (USBD_DRV    *p_drv,
                                      CPU_INT08U   ep_addr,
                                      CPU_INT08U  *p_buf,
                                      CPU_INT32U   buf_len,
                                      USBD_ERR    *p_err)
{
    USBD_DRV_DATA_DMA  *p_drv_data;
    USBD_DRV_CFG       *p_cfg;
    USBD_DMA_DESC      *p_desc;
    CPU_INT32U          xfer_len;
    CPU_INT08U          ep_phy_nbr;
    CPU_INT08U          dma_stat;
    CPU_BOOLEAN         pkt_valid;


    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
    xfer_len   = 0u;

    if ((ep_phy_nbr == 0u) ||
        (ep_phy_nbr == 1u)) {

        xfer_len = USBD_DrvEP_RxFIFO(p_drv,
                                     ep_addr,
                                     p_buf,
                                     buf_len,
                                     p_err);

    } else {
        p_drv_data = (USBD_DRV_DATA_DMA *)p_drv->DataPtr;
        p_cfg      =  p_drv->CfgPtr;
        p_desc     =  p_drv_data->UDHCA_Ptr[ep_phy_nbr];
        dma_stat   = (p_desc->Stat >> 1u) & LPCXXXX_DMA_STAT_MSK;
        pkt_valid  =  DEF_BIT_IS_SET(p_desc->Stat, LPCXXXX_DMA_PKT_VALID_BIT);

        switch (dma_stat) {
            case LPCXXXX_DMA_STAT_NORMAL:
            case LPCXXXX_DMA_STAT_UND:
                 if (pkt_valid == DEF_YES) {
                     xfer_len = p_desc->Stat >> 16u;
                     USBD_DBG_DRV_EP_ARG("  Drv EP DMA Rx Len:", ep_addr, xfer_len);
                    *p_err    = USBD_ERR_NONE;
                 } else {
                     xfer_len = p_desc->Stat >> 16u;
                     if (dma_stat == LPCXXXX_DMA_STAT_UND) {
                         USBD_DBG_DRV_EP_ARG("  Drv EP DMA Rx underrun:", ep_addr, p_desc->Stat);
                     } else {
                         USBD_DBG_DRV_EP_ARG("  Drv EP DMA Rx invalid:", ep_addr, xfer_len);
                     }
                     xfer_len = 0;
                    *p_err = USBD_ERR_RX;
                 }
                 break;

            case LPCXXXX_DMA_STAT_NOT_SERV:
            case LPCXXXX_DMA_STAT_BEING_SERV:
            case LPCXXXX_DMA_STAT_OVR:
            case LPCXXXX_DMA_STAT_SYS_ERR:
            default:
                 USBD_DBG_DRV_EP_ARG("  Drv EP DMA Rx failed:", ep_addr, dma_stat);
                *p_err = USBD_ERR_RX;
                 break;
        }

        if ((p_cfg->MemAddr != 0x00000000u) &&
            (p_cfg->MemSize !=          0u)) {

            if (xfer_len != 0u) {
                Mem_Copy((void *)p_buf,
                         (void *)p_desc->BufMemPtr,
                                 xfer_len);
            }
        } else {
            p_desc->BufPtr = 0x00000000;
        }
   }

   return (xfer_len);
}


/*
*********************************************************************************************************
*                                       USBD_DrvEP_RxZLP_FIFO()
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

static  void  USBD_DrvEP_RxZLP_FIFO (USBD_DRV    *p_drv,
                                     CPU_INT08U   ep_addr,
                                     USBD_ERR    *p_err)
{
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT08U      ep_phy_nbr;
    CPU_INT08U      sie_data;
    CPU_INT08U      zero_pkt;


    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;

    DEF_BIT_CLR(p_drv_data->EP_IntEn, DEF_BIT32(ep_phy_nbr));

    if (ep_phy_nbr != 0u) {
        (void)LPCXXXX_EP_SlaveRd(p_drv, ep_phy_nbr, &zero_pkt, 0, &sie_data);
    }

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       USBD_DrvEP_RxZLP_DMA()
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

static  void  USBD_DrvEP_RxZLP_DMA (USBD_DRV    *p_drv,
                                    CPU_INT08U   ep_addr,
                                    USBD_ERR    *p_err)
{
    USBD_LPCXXXX_REG   *p_reg;
    USBD_DRV_DATA_DMA  *p_drv_data;
    USBD_DMA_DESC      *p_desc;
    CPU_INT08U          ep_phy_nbr;


    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);

    if ((ep_phy_nbr == 0u) ||
        (ep_phy_nbr == 1u)) {
        USBD_DrvEP_RxZLP_FIFO(p_drv, ep_addr, p_err);
    } else {
        p_reg            = (USBD_LPCXXXX_REG  *)(p_drv->CfgPtr->BaseAddr);
        p_drv_data       = (USBD_DRV_DATA_DMA *)p_drv->DataPtr;
        p_desc           = p_drv_data->UDHCA_Ptr[ep_phy_nbr];
        p_desc->NextPtr  = 0x00000000;
        p_desc->BufPtr   = 0x00000000u;
        p_desc->BufLen   = 0x0000u;
        p_desc->Stat     = DEF_BIT_NONE;

        USBD_DBG_DRV_EP("  Drv EP FIFO RxZLP", ep_addr);

        p_reg->EP_DMA_EN = DEF_BIT32(ep_phy_nbr);

       *p_err = USBD_ERR_NONE;
    }
}

/*
*********************************************************************************************************
*                                         USBD_DrvEP_TxFIFO()
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

static  CPU_INT32U  USBD_DrvEP_TxFIFO (USBD_DRV    *p_drv,
                                       CPU_INT08U   ep_addr,
                                       CPU_INT08U  *p_buf,
                                       CPU_INT32U   buf_len,
                                       USBD_ERR    *p_err)
{
    USBD_DRV_DATA  *p_drv_data;
    CPU_BOOLEAN     valid;
    CPU_INT08U      ep_phy_nbr;
    CPU_INT16U      ep_pkt_len;
    CPU_INT08U      sie_data;
    CPU_SR_ALLOC();


    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;

    CPU_CRITICAL_ENTER();
                                                                /* Select EP cmd.                                       */
    valid = LPCXXXX_SIE_RdCmd( p_drv,
                               LPCXXXX_SIE_CMD_RD_SEL_EP_BASE  | (ep_phy_nbr << 16u),
                               LPCXXXX_SIE_DATA_RD_SEL_EP_BASE | (ep_phy_nbr << 16u),
                              &sie_data);

    if (valid != DEF_OK) {
        CPU_CRITICAL_EXIT();
        USBD_DBG_DRV_EP("  Drv EP FIFO Tx failed (sel EP)", ep_addr);
       *p_err = USBD_ERR_TX;
        return (0u);
    }
                                                                /* Chk if next wr buf is full.                          */
    if (DEF_BIT_IS_SET(sie_data, LPCXXXX_SIE_CMD_SEL_EPx_F_E)) {
        CPU_CRITICAL_EXIT();
        USBD_DBG_DRV_EP("  Drv EP FIFO Tx failed (wr buf full)", ep_addr);
       *p_err = USBD_ERR_TX;
        return (0u);
    }

    ep_pkt_len = (CPU_INT16U)DEF_MIN(p_drv_data->EP_PktSize[ep_phy_nbr], buf_len);

    LPCXXXX_EP_SlaveWr(p_drv, ep_phy_nbr, p_buf, ep_pkt_len);

    CPU_CRITICAL_EXIT();

    USBD_DBG_DRV_EP_ARG("  Drv EP FIFO Tx Len:", ep_addr, ep_pkt_len);

   *p_err = USBD_ERR_NONE;

    return (ep_pkt_len);
}


/*
*********************************************************************************************************
*                                         USBD_DrvEP_TxDMA()
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

static  CPU_INT32U  USBD_DrvEP_TxDMA (USBD_DRV    *p_drv,
                                      CPU_INT08U   ep_addr,
                                      CPU_INT08U  *p_buf,
                                      CPU_INT32U   buf_len,
                                      USBD_ERR    *p_err)
{
    USBD_DRV_DATA_DMA  *p_drv_data;
    USBD_DRV_CFG       *p_cfg;
    USBD_DMA_DESC      *p_desc;
    CPU_INT16U          ep_max_pkt_size;
    CPU_INT16U          xfer_len;
    CPU_INT08U          ep_phy_nbr;


    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    p_drv_data = (USBD_DRV_DATA_DMA *)p_drv->DataPtr;

    if ((ep_phy_nbr == 0u) ||
        (ep_phy_nbr == 1u)) {
           xfer_len = (CPU_INT16U)USBD_DrvEP_TxFIFO(p_drv,
                                                    ep_addr,
                                                    p_buf,
                                                    buf_len,
                                                    p_err);
    } else {
        p_cfg  = p_drv->CfgPtr;
        p_desc = p_drv_data->UDHCA_Ptr[ep_phy_nbr];

        if ((p_cfg->MemAddr != 0x00000000u) &&
            (p_cfg->MemSize !=          0u)) {
            ep_max_pkt_size = p_drv_data->EP_PktSize[ep_phy_nbr];
            if (buf_len  > ep_max_pkt_size) {
                xfer_len = ep_max_pkt_size;
            } else {
                xfer_len = (CPU_INT16U)buf_len;
            }

            Mem_Copy((void *)p_desc->BufMemPtr,
                     (void *)p_buf,
                             xfer_len);
            p_desc->BufPtr  =  p_desc->BufMemPtr;
        } else {
            xfer_len        = (CPU_INT16U)buf_len;
            p_desc->BufPtr  = (CPU_INT32U)p_buf;
        }
        p_desc->BufLen = xfer_len;
        p_desc->Stat   = DEF_BIT_NONE;

        USBD_DBG_DRV_EP_ARG("  Drv EP DMA Tx Len:", ep_addr, xfer_len);

       *p_err = USBD_ERR_NONE;
    }

   return (xfer_len);
}


/*
*********************************************************************************************************
*                                      USBD_DrvEP_TxStartFIFO()
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

static  void  USBD_DrvEP_TxStartFIFO (USBD_DRV    *p_drv,
                                      CPU_INT08U   ep_addr,
                                      CPU_INT08U  *p_buf,
                                      CPU_INT32U   buf_len,
                                      USBD_ERR    *p_err)
{
    USBD_LPCXXXX_REG  *p_reg;
    CPU_BOOLEAN        valid;
    CPU_INT08U         ep_phy_nbr;
    CPU_INT08U         sie_data;
    CPU_SR_ALLOC();


    (void)p_buf;
    (void)buf_len;

    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    p_reg      = (USBD_LPCXXXX_REG *)p_drv->CfgPtr->BaseAddr;

    USBD_DBG_DRV_EP_ARG("  Drv EP FIFO Tx Start Len:", ep_addr, buf_len);

    CPU_CRITICAL_ENTER();
    valid = LPCXXXX_SIE_RdCmd( p_drv,
                               LPCXXXX_SIE_CMD_RD_SEL_EP_BASE  | (ep_phy_nbr << 16),
                               LPCXXXX_SIE_DATA_RD_SEL_EP_BASE | (ep_phy_nbr << 16),
                              &sie_data);
    if (valid != DEF_OK) {
        CPU_CRITICAL_EXIT();
        USBD_DBG_DRV_EP("  Drv EP FIFO Tx Start failed (sel EP)", ep_addr);
       *p_err = USBD_ERR_TX;
        return;
    }

    DEF_BIT_SET(p_reg->EP_INT_EN, DEF_BIT32(ep_phy_nbr));

                                                                /* Select validate buf cmd.                             */
    valid = LPCXXXX_SIE_WrCmd(p_drv,
                              LPCXXXX_SIE_CMD_WR_VALIDATE_BUF,
                              0);
    if (valid != DEF_OK) {
        CPU_CRITICAL_EXIT();
        USBD_DBG_DRV_EP("  Drv EP FIFO Tx Start failed (val buf)", ep_addr);
       *p_err = USBD_ERR_TX;
        return;
    }

    CPU_CRITICAL_EXIT();

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       USBD_DrvEP_TxStartDMA()
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

static  void  USBD_DrvEP_TxStartDMA (USBD_DRV    *p_drv,
                                     CPU_INT08U   ep_addr,
                                     CPU_INT08U  *p_buf,
                                     CPU_INT32U   buf_len,
                                     USBD_ERR    *p_err)
{
    USBD_LPCXXXX_REG  *p_reg;
    CPU_INT08U         ep_phy_nbr;


    p_reg      = (USBD_LPCXXXX_REG  *)(p_drv->CfgPtr->BaseAddr);
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);

    if ((ep_phy_nbr == 0u) ||
        (ep_phy_nbr == 1u)) {
       USBD_DrvEP_TxStartFIFO(p_drv,
                              ep_addr,
                              p_buf,
                              buf_len,
                              p_err);
    } else {
       USBD_DBG_DRV_EP_ARG("  Drv EP DMA Tx Start Len:", ep_addr, buf_len);

       p_reg->EP_DMA_EN = DEF_BIT32(ep_phy_nbr);

      *p_err = USBD_ERR_NONE;
   }
}


/*
*********************************************************************************************************
*                                       USBD_DrvEP_TxZLP_FIFO()
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

static  void  USBD_DrvEP_TxZLP_FIFO (USBD_DRV    *p_drv,
                                     CPU_INT08U   ep_addr,
                                     USBD_ERR    *p_err)
{
    USBD_LPCXXXX_REG  *p_reg;
    CPU_INT08U         ep_phy_nbr;
    CPU_INT08U         sie_data;
    CPU_INT08U         zero_pkt;
    CPU_BOOLEAN        valid;
    CPU_SR_ALLOC();


    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);

    LPCXXXX_EP_SlaveWr(p_drv, ep_phy_nbr, &zero_pkt, 0);

                                                                /* Select EP cmd.                                       */
    CPU_CRITICAL_ENTER();
    valid = LPCXXXX_SIE_RdCmd( p_drv,
                               LPCXXXX_SIE_CMD_RD_SEL_EP_BASE  | (ep_phy_nbr << 16),
                               LPCXXXX_SIE_DATA_RD_SEL_EP_BASE | (ep_phy_nbr << 16),
                              &sie_data);
    CPU_CRITICAL_EXIT();
    if (valid != DEF_OK) {
        USBD_DBG_DRV_EP("  Drv EP FIFO TxZLP failed (sel EP)", ep_addr);
       *p_err  = USBD_ERR_TX;
        return;
    }

                                                                /* Select validate buf cmd.                             */
    valid = LPCXXXX_SIE_WrCmd(p_drv,
                              LPCXXXX_SIE_CMD_WR_VALIDATE_BUF,
                              0);
    if (ep_phy_nbr == 1u) {
        p_reg  = (USBD_LPCXXXX_REG *)p_drv->CfgPtr->BaseAddr;
        CPU_CRITICAL_ENTER();
        DEF_BIT_SET(p_reg->EP_INT_EN, DEF_BIT32(1u));
        CPU_CRITICAL_EXIT();
    }

    if (valid != DEF_OK) {
        USBD_DBG_DRV_EP("  Drv EP FIFO TxZLP failed (val buf)", ep_addr);
       *p_err  = USBD_ERR_TX;
    } else {
        USBD_DBG_DRV_EP("  Drv EP FIFO TxZLP", ep_addr);
       *p_err  = USBD_ERR_NONE;
    }
}


/*
*********************************************************************************************************
*                                      USBD_DrvEP_TxZLP_DMA()
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

static  void  USBD_DrvEP_TxZLP_DMA (USBD_DRV    *p_drv,
                                    CPU_INT08U   ep_addr,
                                    USBD_ERR    *p_err)
{
    USBD_LPCXXXX_REG   *p_reg;
    USBD_DRV_DATA_DMA  *p_drv_data;
    USBD_DMA_DESC      *p_desc;
    CPU_INT08U          ep_phy_nbr;


    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);

    if ((ep_phy_nbr == 0u) ||
        (ep_phy_nbr == 1u)) {
        USBD_DrvEP_TxZLP_FIFO(p_drv, ep_addr, p_err);

    } else {
        p_drv_data       = (USBD_DRV_DATA_DMA *)p_drv->DataPtr;
        p_reg            = (USBD_LPCXXXX_REG  *)p_drv->CfgPtr->BaseAddr;
        p_desc           = p_drv_data->UDHCA_Ptr[ep_phy_nbr];
        p_desc->BufPtr   = 0x00000000u;
        p_desc->BufLen   = 0x0000u;
        p_desc->Stat     = DEF_BIT_NONE;

        USBD_DBG_DRV_EP("  Drv EP DMA TxZLP", ep_addr);

        p_reg->EP_DMA_EN = DEF_BIT32(ep_phy_nbr);

       *p_err = USBD_ERR_NONE;
   }
}


/*
*********************************************************************************************************
*                                       USBD_DrvEP_AbortFIFO()
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

static  CPU_BOOLEAN  USBD_DrvEP_AbortFIFO (USBD_DRV    *p_drv,
                                           CPU_INT08U   ep_addr)
{
    CPU_INT08U   ep_phy_nbr;
    CPU_INT16U   reg_to;
    CPU_INT08U   sie_data = 0u;
    CPU_BOOLEAN  buf_is_full;
    CPU_BOOLEAN  valid;
    CPU_SR_ALLOC();


    valid       = DEF_OK;
    reg_to      = LPCXXXX_REG_TO;
    buf_is_full = DEF_TRUE;
    ep_phy_nbr  = USBD_EP_ADDR_TO_PHY(ep_addr);

    while ((valid       == DEF_OK) &&
           (reg_to      >  0u)     &&
           (buf_is_full == DEF_TRUE)) {
        USBD_DBG_DRV_EP("  Drv EP FIFO Abort", ep_addr);

                                                                /* Select EP cmd.                                       */
        CPU_CRITICAL_ENTER();
        valid = LPCXXXX_SIE_RdCmd( p_drv,
                                   LPCXXXX_SIE_CMD_RD_CLR_EP_BASE  | (ep_phy_nbr << 16),
                                   LPCXXXX_SIE_DATA_RD_CLR_EP_BASE | (ep_phy_nbr << 16),
                                  &sie_data);
        CPU_CRITICAL_EXIT();

        buf_is_full = DEF_BIT_IS_SET(sie_data, LPCXXXX_SIE_CMD_SEL_EPx_F_E);
        USBD_DBG_DRV_EP_ARG("  Drv EP FIFO Abort", ep_addr, sie_data);

        if ((valid       == DEF_OK) &&
            (buf_is_full == DEF_TRUE)) {
                                                                /* Clr buf cmd.                                         */
           CPU_CRITICAL_ENTER();
           valid = LPCXXXX_SIE_RdCmd( p_drv,
                                      LPCXXXX_SIE_CMD_RD_CLR_BUF,
                                      LPCXXXX_SIE_DATA_RD_CLR_BUF,
                                     &sie_data);
           CPU_CRITICAL_EXIT();
        }
        reg_to--;
    }

    if (reg_to == 0u) {
        USBD_DBG_DRV_EP("  Drv EP FIFO Abort failed (clr buf)", ep_addr);
        return (DEF_FAIL);
    }

    USBD_DBG_DRV_EP("  Drv EP FIFO Abort", ep_addr);
    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                       USBD_DrvEP_AbortFIFO()
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

static  CPU_BOOLEAN  USBD_DrvEP_AbortDMA (USBD_DRV    *p_drv,
                                          CPU_INT08U   ep_addr)
{
    USBD_LPCXXXX_REG   *p_reg;
    USBD_DRV_DATA_DMA  *p_drv_data;
    USBD_DMA_DESC      *p_desc;
    CPU_INT08U          ep_phy_nbr;
    CPU_BOOLEAN         ep_abort;


    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);

    if ((ep_phy_nbr == 0u) ||
        (ep_phy_nbr == 1u)) {
        ep_abort = USBD_DrvEP_AbortFIFO(p_drv, ep_addr);
    } else {
        p_drv_data        = (USBD_DRV_DATA_DMA *)p_drv->DataPtr;
        p_reg             = (USBD_LPCXXXX_REG  *)p_drv->CfgPtr->BaseAddr;
        p_reg->EO_INT_CLR = DEF_BIT32(ep_phy_nbr);
        p_reg->EP_DMA_DIS = DEF_BIT32(ep_phy_nbr);
        p_desc            = p_drv_data->UDHCA_Ptr[ep_phy_nbr];
        p_desc->BufPtr    = 0x00000000u;
        p_desc->BufLen    = 0x0000u;
        ep_abort          = DEF_OK;

        USBD_DBG_DRV_EP("  Drv EP DMA Abort", ep_addr);
   }
   return (ep_abort);
}


/*
*********************************************************************************************************
*                                       USBD_DrvEP_StallFIFO()
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

static  CPU_BOOLEAN  USBD_DrvEP_StallFIFO (USBD_DRV     *p_drv,
                                           CPU_INT08U    ep_addr,
                                           CPU_BOOLEAN   state)
{
    USBD_DRV_DATA     *p_drv_data;
    USBD_LPCXXXX_REG  *p_reg;
    CPU_INT08U         ep_phy_nbr;
    CPU_BOOLEAN        valid;
    CPU_SR_ALLOC();


    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;

    CPU_CRITICAL_ENTER();

    if (state == DEF_SET) {
        DEF_BIT_SET(p_drv_data->EP_SieStat[ep_phy_nbr], LPCXXXX_SIE_CMD_SET_EPx_STAT_ST);
    } else {
        DEF_BIT_CLR(p_drv_data->EP_SieStat[ep_phy_nbr], LPCXXXX_SIE_CMD_SET_EPx_STAT_ST);
    }

    p_reg             = (USBD_LPCXXXX_REG *)p_drv->CfgPtr->BaseAddr;
    p_reg->EP_INT_CLR = DEF_BIT(ep_phy_nbr);


    valid = LPCXXXX_SIE_WrCmd(p_drv,
                              LPCXXXX_SIE_CMD_WR_SET_EP_STAT_BASE | (ep_phy_nbr << 16u),
                              p_drv_data->EP_SieStat[ep_phy_nbr]);
    CPU_CRITICAL_EXIT();

    if (valid == DEF_TRUE) {
        if (state == DEF_SET) {
            USBD_DBG_DRV_EP("  Drv EP FIFO Stall (set)", ep_addr);
        } else {
            USBD_DBG_DRV_EP("  Drv EP FIFO Stall (clr)", ep_addr);
        }
    } else {
        if (state == DEF_SET) {
            USBD_DBG_DRV_EP("  Drv EP FIFO Stall failed (set)", ep_addr);
        } else {
            USBD_DBG_DRV_EP("  Drv EP FIFO Stall failed (clr)", ep_addr);
        }
    }

    return (valid);
}


/*
*********************************************************************************************************
*                                       USBD_DrvEP_StallDMA()
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

static  CPU_BOOLEAN  USBD_DrvEP_StallDMA (USBD_DRV     *p_drv,
                                          CPU_INT08U    ep_addr,
                                          CPU_BOOLEAN   state)
{
    USBD_DRV_DATA     *p_drv_data;
    USBD_LPCXXXX_REG  *p_reg;
    CPU_INT08U         ep_phy_nbr;
    CPU_BOOLEAN        valid;
    CPU_INT08U         sie_data;
    CPU_SR_ALLOC();


    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;

    CPU_CRITICAL_ENTER();

    if (state == DEF_SET) {
        DEF_BIT_SET(p_drv_data->EP_SieStat[ep_phy_nbr], LPCXXXX_SIE_CMD_SET_EPx_STAT_ST);
    } else {
        DEF_BIT_CLR(p_drv_data->EP_SieStat[ep_phy_nbr], LPCXXXX_SIE_CMD_SET_EPx_STAT_ST);
    }

    valid = LPCXXXX_SIE_WrCmd(p_drv,
                              LPCXXXX_SIE_CMD_WR_SET_EP_STAT_BASE | (ep_phy_nbr << 16u),
                              p_drv_data->EP_SieStat[ep_phy_nbr]);
    CPU_CRITICAL_EXIT();

    if ((ep_phy_nbr % 2) == 0u) {
        if (valid == DEF_TRUE) {
            if (state == DEF_SET) {
                USBD_DBG_DRV_EP("  Drv EP Stall DMA (set)", ep_addr);

                p_reg = (USBD_LPCXXXX_REG *)p_drv->CfgPtr->BaseAddr;

                CPU_CRITICAL_ENTER();
                valid = LPCXXXX_SIE_RdCmd(p_drv,
                                          LPCXXXX_SIE_CMD_RD_CLR_EP_BASE  | (ep_phy_nbr << 16),
                                          LPCXXXX_SIE_DATA_RD_CLR_EP_BASE | (ep_phy_nbr << 16),
                                          &sie_data);
                if (valid != DEF_OK) {
                    USBD_DBG_DRV_EP("  Drv EP FIFO Rd failed (sel EP)", USBD_EP_PHY_TO_ADDR(ep_phy_nbr));
                }

                valid = LPCXXXX_SIE_RdCmd(p_drv,
                                          LPCXXXX_SIE_CMD_RD_CLR_BUF,
                                          LPCXXXX_SIE_DATA_RD_CLR_BUF,
                                          &sie_data);
                if (valid != DEF_OK) {
                    USBD_DBG_DRV_EP("  Drv EP FIFO Rd failed (clr buf)", USBD_EP_PHY_TO_ADDR(ep_phy_nbr));
                }

                valid = LPCXXXX_SIE_RdCmd(p_drv,
                                          LPCXXXX_SIE_CMD_RD_CLR_EP_BASE  | (ep_phy_nbr << 16),
                                          LPCXXXX_SIE_DATA_RD_CLR_EP_BASE | (ep_phy_nbr << 16),
                                          &sie_data);

                valid = LPCXXXX_SIE_RdCmd(p_drv,
                                          LPCXXXX_SIE_CMD_RD_CLR_BUF,
                                          LPCXXXX_SIE_DATA_RD_CLR_BUF,
                                          &sie_data);

                CPU_CRITICAL_EXIT();


                p_reg->DMA_R_CLR = DEF_BIT32(ep_phy_nbr);
            } else {
                USBD_DBG_DRV_EP("  Drv EP Stall DMA (clr)", ep_addr);
            }
        } else {
            if (state == DEF_SET) {
                USBD_DBG_DRV_EP("  Drv EP Stall DMA failed (set)", ep_addr);
            } else {
                USBD_DBG_DRV_EP("  Drv EP Stall DMA failed (clr)", ep_addr);
            }
        }
    }

    return (valid);
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

    USBD_LPCXXXX_REG   *p_reg;
    USBD_DRV_DATA      *p_drv_data;
    CPU_INT08U          ep_phy_nbr;
    CPU_INT08U          sie_data;
    CPU_INT32U          ep_int_stat;
    CPU_INT32U          dev_int_stat;
    CPU_INT32U          dev_int_en;
#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
    USBD_DRV_DATA_DMA  *p_drv_data_dma;
    USBD_DMA_DESC      *p_desc;
#endif


    p_reg              = (USBD_LPCXXXX_REG  *)p_drv->CfgPtr->BaseAddr;
    dev_int_stat       = p_reg->DEV_INT_STAT;                   /* Dev int stat reg should be clr at beginning of ISR.  */
    p_reg->DEV_INT_CLR = dev_int_stat;


    p_drv_data     = (USBD_DRV_DATA *)p_drv->DataPtr;
    dev_int_en     =  p_reg->DEV_INT_EN;
    dev_int_stat  &=  dev_int_en;
#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
    p_drv_data_dma = (USBD_DRV_DATA_DMA *)p_drv->DataPtr;
#endif

                                                                /* --------------- USB STATUS INTERRUPT --------------- */
    if (DEF_BIT_IS_SET(dev_int_stat, LPCXXXX_DEV_INT_DEV_STAT)) {
        LPCXXXX_DevStat(p_drv);
    }
                                                                /* ---------------- USB ERROR INTERRUPT --------------- */
    if (DEF_BIT_IS_SET(dev_int_stat, LPCXXXX_DEV_INT_ERR)) {
        LPCXXXX_DevErr(p_drv);
        USBD_DBG_DRV("  Drv ISR USB Error");
    }

                                                                /* ---------------- FAST EP INTERRUPTS ---------------- */
    if (DEF_BIT_IS_SET(dev_int_stat, LPCXXXX_DEV_INT_EP_FAST)) {
        USBD_DBG_DRV_EP("  Drv ISR EP (Fast)", USBD_EP_ADDR_NONE);

        ep_int_stat  = p_reg->EP_INT_STAT;
        ep_int_stat &= p_reg->EP_INT_EN;
        ep_int_stat &= p_drv_data->EP_Prio;

        while (ep_int_stat != DEF_BIT_NONE) {
                                                                /* Process EP0 int first.                               */
            if (DEF_BIT_IS_SET_ANY(ep_int_stat, LPCXXXX_EP_INT_STAT_EPs_CTRL)) {
                ep_phy_nbr = (CPU_INT08U)(31u - CPU_CntLeadZeros32(ep_int_stat & LPCXXXX_EP_INT_STAT_EPs_CTRL));
            } else {
                ep_phy_nbr = (CPU_INT08U)(31u - CPU_CntLeadZeros32(ep_int_stat));
            }
            LPCXXXX_EP_Process(p_drv, ep_phy_nbr);

            ep_int_stat  = p_reg->EP_INT_STAT;
            ep_int_stat &= p_reg->EP_INT_EN;
            ep_int_stat &= p_drv_data->EP_Prio;
        }
    }

                                                                /* ---------------- SLOW EP INTERRUPTS ---------------- */
    if (DEF_BIT_IS_SET(dev_int_stat, LPCXXXX_DEV_INT_EP_SLOW)) {

        ep_int_stat  =  p_reg->EP_INT_STAT;
        ep_int_stat &=  p_reg->EP_INT_EN;
        ep_int_stat &= ~p_drv_data->EP_Prio;

        while (ep_int_stat != DEF_BIT_NONE) {
            if (DEF_BIT_IS_SET_ANY(ep_int_stat, LPCXXXX_EP_INT_STAT_EPs_CTRL)) {
                ep_phy_nbr = (CPU_INT08U)(31u - CPU_CntLeadZeros32(ep_int_stat & LPCXXXX_EP_INT_STAT_EPs_CTRL));
            } else {
                ep_phy_nbr = (CPU_INT08U)(31u - CPU_CntLeadZeros32(ep_int_stat));
            }

            LPCXXXX_EP_Process(p_drv, ep_phy_nbr);

            ep_int_stat  =  p_reg->EP_INT_STAT;
            ep_int_stat &=  p_reg->EP_INT_EN;
            ep_int_stat &= ~p_drv_data->EP_Prio;
        }
    }

    if (DEF_BIT_IS_SET(p_reg->DMA_INT_STAT, LPCXXXX_DMA_INT_EOT)) {
        ep_int_stat  = p_reg->EO_INT_STA;

        while (ep_int_stat != DEF_BIT_NONE) {
            ep_phy_nbr        = (CPU_INT08U)(31u - CPU_CntLeadZeros32(ep_int_stat));

            p_reg->EO_INT_CLR = DEF_BIT32(ep_phy_nbr);
            if ((ep_phy_nbr % 2u) == 0u) {
#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
                p_desc = p_drv_data_dma->UDHCA_Ptr[ep_phy_nbr];
                USBD_DBG_DRV_EP_ARG("  Drv ISR Rx DMA Cmpl", USBD_EP_PHY_TO_LOG(ep_phy_nbr), p_desc->Stat);
#endif
                USBD_EP_RxCmpl(p_drv, USBD_EP_PHY_TO_LOG(ep_phy_nbr));
            } else {
#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
                p_desc = p_drv_data_dma->UDHCA_Ptr[ep_phy_nbr];
                USBD_DBG_DRV_EP_ARG("  Drv ISR Tx DMA Cmpl", USBD_EP_PHY_TO_LOG(ep_phy_nbr) | 0x80, p_desc->Stat);
#endif
                USBD_EP_TxCmpl(p_drv, USBD_EP_PHY_TO_LOG(ep_phy_nbr));

                p_reg->DMA_R_CLR = DEF_BIT32(ep_phy_nbr);
            }
            ep_int_stat = p_reg->EO_INT_STA;

            (void)LPCXXXX_SIE_RdCmd(p_drv,                      /* Clr EP state.                                        */
                                    LPCXXXX_SIE_CMD_RD_CLR_EP_BASE  | (ep_phy_nbr << 16),
                                    LPCXXXX_SIE_DATA_RD_CLR_EP_BASE | (ep_phy_nbr << 16),
                                   &sie_data);
        }
    }

    if (DEF_BIT_IS_SET(p_reg->DMA_INT_STAT, LPCXXXX_DMA_INT_ERR)) {
        ep_int_stat  = p_reg->SYS_INT_STA;

        while (ep_int_stat != DEF_BIT_NONE) {
            ep_phy_nbr         = (CPU_INT08U)(31u - CPU_CntLeadZeros32(ep_int_stat));

            p_reg->SYS_INT_CLR = DEF_BIT32(ep_phy_nbr);
            if ((ep_phy_nbr % 2u) == 0u) {
                USBD_DBG_DRV_EP("  Drv ISR Rx Err", USBD_EP_PHY_TO_LOG(ep_phy_nbr));
                USBD_EP_RxCmpl(p_drv, USBD_EP_PHY_TO_LOG(ep_phy_nbr));
            } else {
#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
                p_desc = p_drv_data_dma->UDHCA_Ptr[ep_phy_nbr];
                USBD_DBG_DRV_EP_ARG("  Drv ISR Tx Err", USBD_EP_PHY_TO_ADDR(ep_phy_nbr), p_desc->Stat);
#endif
                USBD_EP_TxCmpl(p_drv, USBD_EP_PHY_TO_LOG(ep_phy_nbr));

                p_reg->DMA_R_CLR  = DEF_BIT32(ep_phy_nbr);
            }
            ep_int_stat  = p_reg->SYS_INT_STA;

            (void)LPCXXXX_SIE_RdCmd(p_drv,                      /* Clr EP state.                                        */
                                    LPCXXXX_SIE_CMD_RD_CLR_EP_BASE  | (ep_phy_nbr << 16),
                                    LPCXXXX_SIE_DATA_RD_CLR_EP_BASE | (ep_phy_nbr << 16),
                                   &sie_data);
        }
    }

    if (DEF_BIT_IS_SET(p_reg->DMA_INT_STAT, LPCXXXX_DMA_INT_NDDR)) {
        ep_int_stat  = p_reg->DD_INT_STA;

        while (ep_int_stat != DEF_BIT_NONE) {
            ep_phy_nbr         = (CPU_INT08U)(31u - CPU_CntLeadZeros32(ep_int_stat));

            p_reg->DD_INT_CLR  = DEF_BIT32(ep_phy_nbr);
            ep_int_stat        = p_reg->DD_INT_STA;
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
*                                         LPCXXXX_DevRegRst()
*
* Description : Reset all the USB registers.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  LPCXXXX_DevRegRst (USBD_DRV  *p_drv)
{
    USBD_LPCXXXX_REG  *p_reg;
    USBD_DRV_DATA     *p_drv_data;
    CPU_INT08U         ep_phy_nbr;
    CPU_INT16U         reg_to;
    CPU_INT08U         sie_data;
    CPU_SR_ALLOC();


    p_reg      = (USBD_LPCXXXX_REG *)p_drv->CfgPtr->BaseAddr;
    p_drv_data = (USBD_DRV_DATA    *)p_drv->DataPtr;



    p_reg->EP_INT_EN  = DEF_BIT_00;
    p_reg->RE_EP      = DEF_BIT_NONE;                           /* Unrealize all EPs.                                   */
    p_reg->CTRL       = DEF_BIT_NONE;                           /* Init all Tx/Rx related reg.                          */
    p_reg->RX_PKT_LEN = DEF_BIT_NONE;
    p_reg->TX_PKT_LEN = DEF_BIT_NONE;

                                                                /* Disable all EPs                                      */
    for (ep_phy_nbr = 0; ep_phy_nbr < 32u; ep_phy_nbr++) {
        p_drv_data->EP_SieStat[ep_phy_nbr] = LPCXXXX_SIE_CMD_SET_EPx_STAT_DA;
                                                                /* Select clr EP cmd.                                   */
        CPU_CRITICAL_ENTER();
       (void)LPCXXXX_SIE_RdCmd( p_drv,
                                LPCXXXX_SIE_CMD_RD_CLR_EP_BASE  | (ep_phy_nbr << 16),
                                LPCXXXX_SIE_DATA_RD_CLR_EP_BASE | (ep_phy_nbr << 16),
                               &sie_data);
        CPU_CRITICAL_EXIT();

                                                                /* Set EP status.                                       */
       (void)LPCXXXX_SIE_WrCmd(p_drv,
                               LPCXXXX_SIE_CMD_WR_SET_EP_STAT_BASE | (ep_phy_nbr << 16),
                               p_drv_data->EP_SieStat[ep_phy_nbr]);
    }

    reg_to = LPCXXXX_REG_TO;
                                                                 /* Wait until EP_RLZED bit is set.                     */
    while ((DEF_BIT_IS_CLR(p_reg->DEV_INT_STAT, LPCXXXX_DEV_INT_EP_RLZED)) &&
           (reg_to > 0u)) {
        reg_to--;
    }

    p_reg->DEV_INT_CLR = LPCXXXX_DEV_INT_EP_RLZED;

    (void)LPCXXXX_SIE_WrCmd(p_drv,
                            LPCXXXX_SIE_CMD_WR_SET_ADDR,
                            LPCXXXX_SIE_CMD_SET_ADDR_DEV_EN);

    (void)LPCXXXX_SIE_WrCmd(p_drv,
                            LPCXXXX_SIE_CMD_WR_SET_ADDR,
                            LPCXXXX_SIE_CMD_SET_ADDR_DEV_EN);

    p_drv_data->ErrCtrs.PID      = 0u;
    p_drv_data->ErrCtrs.UnPkt    = 0u;                          /* Unexpected packet err ctr.                           */
    p_drv_data->ErrCtrs.DataCRC  = 0u;                          /* Data CRC          err ctr.                           */
    p_drv_data->ErrCtrs.To       = 0u;                          /* Timeout           err ctr.                           */
    p_drv_data->ErrCtrs.EndPkt   = 0u;                          /* End of packet     err ctr.                           */
    p_drv_data->ErrCtrs.BufOvr   = 0u;                          /* Buffer overrun    err ctr.                           */
    p_drv_data->ErrCtrs.BitStuff = 0u;                          /* Bitstuff          err ctr.                           */
    p_drv_data->ErrCtrs.Toggle   = 0u;                          /* wrong toggle bit  err ctr.                           */
    p_drv_data->CtrlOutDataLen   = 0u;
    p_drv_data->CtrlOutDataRxd   = DEF_NO;
    p_drv_data->CtrlOutDataIntEn = DEF_NO;
    p_drv_data->EP_IntEn         = 0u;
    p_drv_data->CtrlZLP_Rxd      = DEF_NO;
}


/*
*********************************************************************************************************
*                                          LPCXXXX_DevStat()
*
* Description : Process the device status change event.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
* Return(s)   : none.
*
* Note(s)     : (1) The device connection to the host is detected while processing the bus reset. The
*                   USB device controller provides the CON bit to know the current connect status. But
*                   this bit is not associated to any interrupt. The DEV_STAT interrupt is only generated
*                   when a disconnection occurs. As soon as the device connects to a host's port, the
*                   host will reset the device. That's the reset processing allows to detect also the
*                   connection event.
*
*               (2) A host can do other resets after the first reset following the device connection.
*                   To avoid notifying several connect events to the stack, a flag is used to ensure
*                   that only one connect event is notified.
*********************************************************************************************************
*/

static  void  LPCXXXX_DevStat (USBD_DRV  *p_drv)
{
    USBD_LPCXXXX_REG  *p_reg;
    USBD_DRV_DATA     *p_drv_data;
    CPU_INT08U         sie_data;
    CPU_BOOLEAN        valid;


    p_reg       = (USBD_LPCXXXX_REG *)p_drv->CfgPtr->BaseAddr;
    p_drv_data  = (USBD_DRV_DATA    *)p_drv->DataPtr;

    valid = LPCXXXX_SIE_RdCmd( p_drv,
                               LPCXXXX_SIE_CMD_RD_GET_DEV_STAT,
                               LPCXXXX_SIE_DATA_RD_GET_DEV_STAT,
                              &sie_data);
    if (valid == DEF_FAIL) {
        return;
    }

                                                                /* --------------- USB BUS RESET STATUS --------------- */
    if (DEF_BIT_IS_SET(sie_data, LPCXXXX_SIE_CMD_DEV_STAT_RST) == DEF_YES) {
                                                                /* Detect connect event (see Note #1).                  */
        if ((DEF_BIT_IS_SET(sie_data, LPCXXXX_SIE_CMD_DEV_CON) == DEF_YES) &&
            (p_drv_data->ConnectStat                           == DEF_FALSE)) {

            USBD_EventConn(p_drv);                              /* Notify about dev connection.                         */
            p_drv_data->ConnectStat = DEF_TRUE;                 /* See Note #2.                                         */
            USBD_DBG_DRV("  Drv ISR Connect");
        }

        LPCXXXX_DevRegRst(p_drv);
        USBD_EventReset(p_drv);                                 /* Notify bus reset.                                    */
        DEF_BIT_SET(p_reg->DEV_INT_EN, LPCXXXX_DEV_INT_DEV_STAT |
                                       LPCXXXX_DEV_INT_ERR      |
                                       LPCXXXX_DEV_INT_EP_FAST  |
                                       LPCXXXX_DEV_INT_EP_SLOW);
        DEF_BIT_SET(p_reg->DMA_INT_EN, LPCXXXX_DMA_INT_EOT  |
                                       LPCXXXX_DMA_INT_NDDR |
                                       LPCXXXX_DMA_INT_ERR);
        USBD_DBG_DRV("  Drv ISR Reset");
                                                                /* ---------------- USB CONNECT CHANGE ---------------- */
    } else if (DEF_BIT_IS_SET(sie_data, LPCXXXX_SIE_CMD_DEV_CON_CHNG) == DEF_YES) {
                                                                /* Verify if disconnect event detected.                 */
        if ((DEF_BIT_IS_CLR(sie_data, LPCXXXX_SIE_CMD_DEV_CON) == DEF_YES) &&
            (p_drv_data->ConnectStat                           == DEF_TRUE)) {
            USBD_EventDisconn(p_drv);                           /* Notify about dev disconnection.                      */
            p_drv_data->ConnectStat = DEF_FALSE;
            USBD_DBG_DRV("  Drv ISR Disconnect");
        }

    } else {                                                    /* ---------------- USB SUSPEND CHANGE ---------------- */
                                                                /* Chk if Suspend change bit changed.                   */
        if (DEF_BIT_IS_SET(sie_data, LPCXXXX_SIE_CMD_DEV_SUS_CHNG) == DEF_YES) {
                                                                /* If Suspend bit is set, dev is suspended.             */
            if (DEF_BIT_IS_SET(sie_data, LPCXXXX_SIE_CMD_DEV_SUS) == DEF_YES) {
                USBD_EventSuspend(p_drv);                       /* Notify bus suspend.                                  */
                DEF_BIT_CLR(p_reg->DEV_INT_EN, LPCXXXX_DEV_INT_EP_FAST |
                                               LPCXXXX_DEV_INT_EP_SLOW);
                USBD_DBG_DRV("  Drv ISR Suspend");
            } else {
                USBD_EventResume(p_drv);                        /* Notify bus resume.                                   */
                DEF_BIT_SET(p_reg->DEV_INT_EN, LPCXXXX_DEV_INT_EP_FAST |
                                               LPCXXXX_DEV_INT_EP_SLOW);
                USBD_DBG_DRV("  Drv ISR Resume");
            }
        }
    }
}


/*
*********************************************************************************************************
*                                          LPCXXXX_DevErr()
*
* Description : Process the device error interrupt.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  LPCXXXX_DevErr (USBD_DRV  *p_drv)
{
    CPU_INT08U      sie_data;
    USBD_DRV_DATA  *p_drv_data;
    CPU_BOOLEAN     valid;


    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;
    valid      = LPCXXXX_SIE_RdCmd( p_drv,
                                    LPCXXXX_SIE_CMD_RD_ERR_STAT,
                                    LPCXXXX_SIE_DATA_RD_ERR_STAT,
                                   &sie_data);
    if (valid == DEF_FAIL) {
        return;
    }

    if (DEF_BIT_IS_SET(sie_data, LPCXXXX_SIE_CMD_ERR_STAT_PID)) {
        p_drv_data->ErrCtrs.PID++;
        USBD_DBG_DRV("    Drv ISR USB Error PID");
    }

    if (DEF_BIT_IS_SET(sie_data, LPCXXXX_SIE_CMD_ERR_STAT_UEPKT)) {
        p_drv_data->ErrCtrs.UnPkt++;
        USBD_DBG_DRV("    Drv ISR USB Error UEPKT");
    }

    if (DEF_BIT_IS_SET(sie_data, LPCXXXX_SIE_CMD_ERR_STAT_DATA_CRC)) {
        p_drv_data->ErrCtrs.DataCRC++;
        USBD_DBG_DRV("    Drv ISR USB Error data CRC");
    }

    if (DEF_BIT_IS_SET(sie_data, LPCXXXX_SIE_CMD_ERR_STAT_TO)) {
        p_drv_data->ErrCtrs.To++;
        USBD_DBG_DRV("    Drv ISR USB Error TO");
    }

    if (DEF_BIT_IS_SET(sie_data, LPCXXXX_SIE_CMD_ERR_STAT_END_PKT)) {
        p_drv_data->ErrCtrs.EndPkt++;
        USBD_DBG_DRV("    Drv ISR USB Error end pkt");
    }

    if (DEF_BIT_IS_SET(sie_data, LPCXXXX_SIE_CMD_ERR_STAT_BUF_OVR)) {
        p_drv_data->ErrCtrs.BufOvr++;
        USBD_DBG_DRV("    Drv ISR USB Error OVR");
    }

    if (DEF_BIT_IS_SET(sie_data, LPCXXXX_SIE_CMD_ERR_STAT_BIT_STUFF)) {
        p_drv_data->ErrCtrs.BitStuff++;
        USBD_DBG_DRV("    Drv ISR USB Error bit stuff");
    }

    if (DEF_BIT_IS_SET(sie_data, LPCXXXX_SIE_CMD_ERR_STAT_TOGGLE)) {
        p_drv_data->ErrCtrs.Toggle++;
        USBD_DBG_DRV("    Drv ISR USB Error toggle");
    }
}


/*
*********************************************************************************************************
*                                        LPCXXXX_EP_Process()
*
* Description : Process IN, OUT and SETUP packets for a specific endpoint.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_phy_nbr  Physical endpoint number.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  LPCXXXX_EP_Process (USBD_DRV    *p_drv,
                                  CPU_INT08U   ep_phy_nbr)
{
    USBD_LPCXXXX_REG    *p_reg;
#if 1
    USBD_DRV_DATA_FIFO  *p_data_fifo;
#endif
    CPU_INT08U           ep_log_nbr;
    CPU_INT16U           reg_to;
    CPU_INT08U           setup_pkt[8];
    CPU_INT08U           sie_data;


    p_reg             = (USBD_LPCXXXX_REG *)p_drv->CfgPtr->BaseAddr;
    reg_to            =  LPCXXXX_REG_TO;
    ep_log_nbr        =  USBD_EP_PHY_TO_LOG(ep_phy_nbr);
    p_data_fifo       = (USBD_DRV_DATA_FIFO *)p_drv->DataPtr;

    p_reg->EP_INT_CLR =  DEF_BIT32(ep_phy_nbr);

    while ((reg_to > 0) &&
           (DEF_BIT_IS_CLR(p_reg->DEV_INT_STAT, LPCXXXX_DEV_INT_CD_FULL))) {
        reg_to--;
    }

    if (reg_to == 0u) {
        return;
    }

    sie_data           = (CPU_INT08U)p_reg->CMD_DATA;
    p_reg->DEV_INT_CLR =  LPCXXXX_DEV_INT_CD_FULL;

    if (ep_phy_nbr == 0u) {

        if (DEF_BIT_IS_SET(sie_data, LPCXXXX_SIE_CMD_SEL_EPx_STP)) {
                                                                /* Last setup pkt rxd overwrite last pkt.               */
            if (DEF_BIT_IS_SET(sie_data, LPCXXXX_SIE_CMD_SEL_EPx_PO)) {

                USBD_DBG_DRV_EP("  Drv ISR Setup PO", USBD_EP_PHY_TO_ADDR(ep_phy_nbr));

                if (DEF_BIT_IS_SET(p_data_fifo->EP_IntEn, DEF_BIT32(ep_phy_nbr))) {
                    USBD_EP_RxCmpl(p_drv, ep_log_nbr);

                    DEF_BIT_CLR(p_data_fifo->EP_IntEn, DEF_BIT32(ep_phy_nbr));
                } else {
                    p_data_fifo->CtrlZLP_Rxd = DEF_YES;         /* Assume a ZLP was overwritten...                      */
                }
            }

            (void)LPCXXXX_EP_SlaveRd(p_drv, ep_phy_nbr, &setup_pkt[0], 8, &sie_data);

            USBD_DBG_DRV("  Drv ISR Setup");

            USBD_EventSetup(p_drv, (void *)&setup_pkt[0u]);

        } else if (DEF_BIT_IS_SET(sie_data, LPCXXXX_SIE_CMD_SEL_EPx_F_E)) {
                                                                /* Data was received on ctrl OUT endpoint. ...          */
                                                                /* ... Data is read in ISR to prevent pkt overwrite.    */
            p_data_fifo->CtrlOutDataLen = LPCXXXX_EP_SlaveRd(               p_drv,
                                                                            ep_phy_nbr,
                                                             (CPU_INT08U *)&p_data_fifo->CtrlOutDataBuf[0u],
                                                                            64u,
                                                                           &sie_data);
            if (p_data_fifo->CtrlOutDataLen > 0u) {
                if (p_data_fifo->CtrlOutDataIntEn == DEF_YES) { /* If data was received and expected.                   */

                    USBD_DBG_DRV_EP("  Drv ISR Rx Cmpl w/data", USBD_EP_PHY_TO_ADDR(ep_phy_nbr));

                    p_data_fifo->CtrlOutDataIntEn = DEF_NO;
                    USBD_EP_RxCmpl(p_drv, ep_log_nbr);
                } else {                                        /* Data received but not yet expected.                  */
                    p_data_fifo->CtrlOutDataRxd = DEF_YES;
                }
                                                                /* ZLP pkt received.                                    */
            } else if (DEF_BIT_IS_SET(p_data_fifo->EP_IntEn, DEF_BIT32(ep_phy_nbr)) == DEF_YES) {

                USBD_DBG_DRV_EP("  Drv ISR Rx Cmpl", USBD_EP_PHY_TO_ADDR(ep_phy_nbr));

                DEF_BIT_CLR(p_data_fifo->EP_IntEn, DEF_BIT32(ep_phy_nbr));

                USBD_EP_RxCmpl(p_drv, ep_log_nbr);
            } else {                                            /* Not yet expected ZLP received.                       */
                USBD_DBG_DRV_EP("  Drv ISR Rx Cmpl Ctrl ZLP", USBD_EP_PHY_TO_ADDR(ep_phy_nbr));

                p_data_fifo->CtrlZLP_Rxd = DEF_YES;
            }
        }
    } else {
        if ((ep_phy_nbr % 2u) == 0u) {
            if (DEF_BIT_IS_SET(p_data_fifo->EP_IntEn, DEF_BIT32(ep_phy_nbr))) {
                DEF_BIT_CLR(p_data_fifo->EP_IntEn, DEF_BIT32(ep_phy_nbr));
                USBD_DBG_DRV_EP("  Drv ISR Rx Cmpl (Fast)", USBD_EP_PHY_TO_ADDR(ep_phy_nbr));
                USBD_EP_RxCmpl(p_drv, ep_log_nbr);
            }
        } else {
            USBD_DBG_DRV_EP("  Drv ISR Tx Cmpl (Fast)", USBD_EP_PHY_TO_ADDR(ep_phy_nbr));
            USBD_EP_TxCmpl(p_drv, ep_log_nbr);
        }
    }
}


/*
*********************************************************************************************************
*                                        LPCXXXX_EP_SlaveRd()
*
* Description : Read data from a endpoint in slave mode.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_phy_nbr  Endpoint physical number
*
*               p_buf       Pointer to the memory are where the data will be saved.
*
*               nbr_bytes   Number of bytes to be read.
*
*               p_sie_data  Pointer to the variable that will receive the data from SIE.
*
* Return(s)   : Number of bytes available in EP's internal FIFO.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT16U  LPCXXXX_EP_SlaveRd (USBD_DRV    *p_drv,
                                        CPU_INT08U   ep_phy_nbr,
                                        CPU_INT08U  *p_buf,
                                        CPU_INT16U   buf_len,
                                        CPU_INT08U  *p_sie_data)
{
    USBD_LPCXXXX_REG  *p_reg;
    CPU_INT32U         i;
    CPU_INT16U         nbr_words;
    CPU_INT32U         reg_val;
    CPU_INT16U         ep_pkt_len;
    CPU_INT16U         rx_len;
    CPU_INT16U         reg_to;
    CPU_BOOLEAN        valid;
    CPU_SR_ALLOC();


    p_reg  = (USBD_LPCXXXX_REG *)p_drv->CfgPtr->BaseAddr;
    reg_to =  LPCXXXX_REG_TO;

    CPU_CRITICAL_ENTER();

    p_reg->CTRL = (USBD_EP_PHY_TO_LOG(ep_phy_nbr) << 2u)
                | LPCXXXX_CTRL_RD_EN;

    while (DEF_BIT_IS_CLR(p_reg->RX_PKT_LEN, LPCXXXX_RX_PKT_LEN_PKT_RDY) &&
           (reg_to > 0u)) {
        reg_to--;
    }

    if ((reg_to > 0u) &&
        (DEF_BIT_IS_SET(p_reg->RX_PKT_LEN, LPCXXXX_RX_PKT_LEN_DV))) {
        rx_len = p_reg->RX_PKT_LEN & LPCXXXX_PKT_LEN_MASK;
    } else {
        rx_len= 0u;
    }

    ep_pkt_len = DEF_MIN(rx_len, buf_len);

    if (ep_pkt_len == 0u) {
        reg_val = p_reg->RX_DATA;
    } else {
        nbr_words = ep_pkt_len / 4u;

        for (i = 0u; i < nbr_words; i++) {
            reg_val  = p_reg->RX_DATA;
            p_buf[0] = (CPU_INT08U)((reg_val       ) & 0xFFu);
            p_buf[1] = (CPU_INT08U)((reg_val >>  8u) & 0xFFu);
            p_buf[2] = (CPU_INT08U)((reg_val >> 16u) & 0xFFu);
            p_buf[3] = (CPU_INT08U)((reg_val >> 24u) & 0xFFu);

            p_buf += 4u;
        }

        if ((ep_pkt_len % 4) != 0u) {
            reg_val = p_reg->RX_DATA;

            switch (ep_pkt_len % 4) {
                case 1u:
                     p_buf[0] = (CPU_INT08U)(reg_val & 0xFF);
                     break;

                case 2u:
                     p_buf[0] = (CPU_INT08U)((reg_val       ) & 0xFFu);
                     p_buf[1] = (CPU_INT08U)((reg_val >>  8u) & 0xFFu);
                     break;

                case 3u:
                     p_buf[0] = (CPU_INT08U)((reg_val       ) & 0xFFu);
                     p_buf[1] = (CPU_INT08U)((reg_val >>  8u) & 0xFFu);
                     p_buf[2] = (CPU_INT08U)((reg_val >> 16u) & 0xFFu);
                     break;

                default:
                    break;
            }
        }
    }

    p_reg->DEV_INT_CLR = LPCXXXX_DEV_INT_RX_ENDPKT;
    p_reg->CTRL        = DEF_BIT_NONE;

    valid = LPCXXXX_SIE_RdCmd(p_drv,
                              LPCXXXX_SIE_CMD_RD_SEL_EP_BASE  | (ep_phy_nbr << 16),
                              LPCXXXX_SIE_DATA_RD_SEL_EP_BASE | (ep_phy_nbr << 16),
                              p_sie_data);
    if (valid != DEF_OK) {
        USBD_DBG_DRV_EP("  Drv EP FIFO Rd failed (sel EP)", USBD_EP_PHY_TO_ADDR(ep_phy_nbr));
    }

    valid = LPCXXXX_SIE_RdCmd(p_drv,
                              LPCXXXX_SIE_CMD_RD_CLR_BUF,
                              LPCXXXX_SIE_DATA_RD_CLR_BUF,
                              p_sie_data);
    if (valid != DEF_OK) {
        USBD_DBG_DRV_EP("  Drv EP FIFO Rd failed (clr buf)", USBD_EP_PHY_TO_ADDR(ep_phy_nbr));
    }
    CPU_CRITICAL_EXIT();

    return (rx_len);
}


/*
*********************************************************************************************************
*                                        LPCXXXX_EP_SlaveWr()
*
* Description : Write data to an endpoint in slave mode.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_phy_nbr  Endpoint physical number
*
*               p_buf       Pointer to the memory are where the data will be saved.
*
*               ep_pkt_len  Number of bytes to written.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  LPCXXXX_EP_SlaveWr (USBD_DRV    *p_drv,
                                  CPU_INT08U   ep_phy_nbr,
                                  CPU_INT08U  *p_buf,
                                  CPU_INT16U   ep_pkt_len)
{
    USBD_LPCXXXX_REG  *p_reg;
    CPU_INT32U         i;
    CPU_INT16U         nbr_words;
    CPU_SR_ALLOC();


    nbr_words = (ep_pkt_len + 3u) / 4u;
    p_reg     = (USBD_LPCXXXX_REG *)p_drv->CfgPtr->BaseAddr;

    CPU_CRITICAL_ENTER();

    p_reg->CTRL       = (USBD_EP_PHY_TO_LOG(ep_phy_nbr) << 2u)
                      | LPCXXXX_CTRL_WR_EN;

    p_reg->TX_PKT_LEN = ep_pkt_len & LPCXXXX_PKT_LEN_MASK;


    if (ep_pkt_len == 0u) {                                     /* Tx zero len pkt.                                     */
        p_reg->TX_DATA = 0u;
        p_reg->CTRL    = DEF_BIT_NONE;
        CPU_CRITICAL_EXIT();
        return;
    }

    for (i = 0; i < nbr_words; i++) {
        p_reg->TX_DATA = (CPU_INT32U)((p_buf[0]       ) & 0x000000FF)
                       | (CPU_INT32U)((p_buf[1] <<  8u) & 0x0000FF00)
                       | (CPU_INT32U)((p_buf[2] << 16u) & 0x00FF0000)
                       | (CPU_INT32U)((p_buf[3] << 24u) & 0xFF000000);
        p_buf += 4u;
    }

    switch (ep_pkt_len % 4) {
        case 1u:
             p_reg->TX_DATA = (CPU_INT32U)((p_buf[0]       ) & 0x000000FF);
             break;

        case 2u:
             p_reg->TX_DATA = (CPU_INT32U)((p_buf[0]       ) & 0x000000FF)
                            | (CPU_INT32U)((p_buf[1] <<  8u) & 0x0000FF00);
             break;

        case 3u:
             p_reg->TX_DATA = (CPU_INT32U)((p_buf[0]       ) & 0x000000FF)
                            | (CPU_INT32U)((p_buf[1] <<  8u) & 0x0000FF00)
                            | (CPU_INT32U)((p_buf[2] << 16u) & 0x00FF0000);
             break;

        default:
            break;
    }

    p_reg->CTRL = DEF_BIT_NONE;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                               SERIAL INTERFACE ENGINE (SIE) FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         LPCXXXX_SIE_WrCmd()
*
* Description : Write a command to serial interface engine (SIE).
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               sie_cmd     Command to be written in the SIE.
*
*               sie_val     Value to be written in the data phase of the SIE.
*
* Return(s)   : DEF_OK,   if NO error(s).
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  LPCXXXX_SIE_WrCmd (USBD_DRV    *p_drv,
                                        CPU_INT32U   sie_cmd,
                                        CPU_INT08U   sie_data)
{
    USBD_LPCXXXX_REG  *p_reg;
    CPU_INT16U         sie_retry;
    CPU_SR_ALLOC();


    sie_retry = LPCXXXX_REG_TO;
    p_reg     = (USBD_LPCXXXX_REG *)p_drv->CfgPtr->BaseAddr;

    CPU_CRITICAL_ENTER();
                                                                /* ------------------- COMMAND PHASE ------------------ */
                                                                /* Clr both CCEMPTY and CDFULL int.                     */
    p_reg->DEV_INT_CLR = LPCXXXX_DEV_INT_CD_FULL
                       | LPCXXXX_DEV_INT_CD_EMPTY;


    p_reg->CMD_CODE = sie_cmd;
    while (DEF_BIT_IS_CLR(p_reg->DEV_INT_STAT, LPCXXXX_DEV_INT_CD_EMPTY) &&
          (sie_retry > 0u)) {
        sie_retry--;
    }

    p_reg->DEV_INT_CLR = LPCXXXX_DEV_INT_CD_EMPTY;
    if (sie_retry == 0u) {                                      /* Chk if max nbr of retries reached.                   */
        CPU_CRITICAL_EXIT();
        return (DEF_FAIL);
    }

                                                                /* -------------------- DATA PHASE -------------------- */
    if (sie_cmd != LPCXXXX_SIE_CMD_WR_VALIDATE_BUF) {
        sie_retry       = LPCXXXX_REG_TO;
        p_reg->CMD_CODE = LPCXXXX_SIE_DATA_CODE
                        | (sie_data  << 16);

        while ((DEF_BIT_IS_CLR(p_reg->DEV_INT_STAT, LPCXXXX_DEV_INT_CD_EMPTY)) &&
               (sie_retry > 0u)) {
            sie_retry--;
        }

        p_reg->DEV_INT_CLR = LPCXXXX_DEV_INT_CD_EMPTY;

        if (sie_retry == 0u) {
            CPU_CRITICAL_EXIT();
            return (DEF_FAIL);
        }
    }

    CPU_CRITICAL_EXIT();

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                         LPCXXXX_SIE_RdCmd()
*
* Description : Execute a read command with optional data phase in the serial interface engine (SIE).
*
* Argument(s) : p_drv           Pointer to device driver structure.
*
*               sie_cmd         Read command (command phase).
*
*               sie_data        Read command (data    phase).
*
*               p_sie_rd_data   Pointer to the variable that will receive the data.
*
* Return(s)   : DEF_OK,   if NO error(s).
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) This function MUST be called within a critical section.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  LPCXXXX_SIE_RdCmd (USBD_DRV    *p_drv,
                                        CPU_INT32U   sie_cmd,
                                        CPU_INT32U   sie_data,
                                        CPU_INT08U  *p_sie_rd_data)
{
    USBD_LPCXXXX_REG  *p_reg;
    CPU_BOOLEAN        valid;
    CPU_INT16U         sie_retry;
    CPU_SR_ALLOC();


    sie_retry =  LPCXXXX_REG_TO;
    valid     =  DEF_YES;
    p_reg     = (USBD_LPCXXXX_REG *)p_drv->CfgPtr->BaseAddr;

                                                                /* ------------------- COMMAND PHASE ------------------ */
                                                                /* Clr both CCEMPTY and CDFULL int.                     */
    p_reg->DEV_INT_CLR = LPCXXXX_DEV_INT_CD_FULL
                       | LPCXXXX_DEV_INT_CD_EMPTY;

    p_reg->CMD_CODE = sie_cmd;

    while ((DEF_BIT_IS_CLR(p_reg->DEV_INT_STAT, LPCXXXX_DEV_INT_CD_EMPTY)) &&
           (sie_retry > 0u)) {
        sie_retry--;
    }

    p_reg->DEV_INT_CLR = LPCXXXX_DEV_INT_CD_EMPTY;

    if (sie_retry == 0u) {                                      /* Chk if max nbr of retries reached.                   */
        CPU_CRITICAL_EXIT();
        return (DEF_FAIL);
    }
                                                                /* -------------------- DATA PHASE -------------------- */

    sie_retry       = LPCXXXX_REG_TO;
    p_reg->CMD_CODE = sie_data;

    while ((DEF_BIT_IS_CLR(p_reg->DEV_INT_STAT, LPCXXXX_DEV_INT_CD_EMPTY)) &&
           (sie_retry > 0u)) {
        sie_retry--;
    }

    if (sie_retry > 0u) {
       *p_sie_rd_data = (CPU_INT08U)p_reg->CMD_DATA;
    }

    p_reg->DEV_INT_CLR = LPCXXXX_DEV_INT_CD_EMPTY
                       | LPCXXXX_DEV_INT_CD_FULL;

    if (sie_retry == 0u) {
        valid = DEF_FAIL;
    }

    return (valid);
}


/*
*********************************************************************************************************
*                                       LPCXXXX_SIE_RdTestReg()
*
* Description : Read and verify the value of the test register.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
* Return(s)   : DEF_OK,   if test register has the expected value LPCXXXX_SIE_CMD_TEST_REG_VAL.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : Issuing a Read Test Register command to the SIE helps to verify that the USB clocks
*               (usbclk and AHB slave clock) are properly running. If both clocks are correct, the
*               value 0xA50F should be returned when reading the test register.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  LPCXXXX_SIE_RdTestReg (USBD_DRV  *p_drv)
{
    USBD_LPCXXXX_REG  *p_reg;
    CPU_INT16U         sie_retry;
    CPU_INT16U         test_reg;
    CPU_SR_ALLOC();


    test_reg  =  DEF_BIT_NONE;
    sie_retry =  LPCXXXX_REG_TO;
    p_reg     = (USBD_LPCXXXX_REG *)p_drv->CfgPtr->BaseAddr;


    CPU_CRITICAL_ENTER();
                                                                /* ------------------- COMMAND PHASE ------------------ */
    p_reg->DEV_INT_CLR = LPCXXXX_DEV_INT_CD_FULL
                       | LPCXXXX_DEV_INT_CD_EMPTY;

    p_reg->CMD_CODE = LPCXXXX_SIE_CMD_RD_TEST_REG;


    while ((DEF_BIT_IS_CLR(p_reg->DEV_INT_STAT, LPCXXXX_DEV_INT_CD_EMPTY)) &&
           (sie_retry > 0u)) {
        sie_retry--;
    }

    p_reg->DEV_INT_CLR = LPCXXXX_DEV_INT_CD_EMPTY;

                                                                /* -------------------- DATA PHASE -------------------- */
    sie_retry       = LPCXXXX_REG_TO;
    p_reg->CMD_CODE = LPCXXXX_SIE_DATA_RD_TEST_REG;

    while ((DEF_BIT_IS_CLR(p_reg->DEV_INT_STAT, LPCXXXX_DEV_INT_CD_EMPTY)) &&
           (sie_retry > 0u)) {
        sie_retry--;
    }

    if (sie_retry > 0u) {
        test_reg = (CPU_INT16U)(p_reg->CMD_DATA & 0x000000FFu);
    }

    sie_retry       = LPCXXXX_REG_TO;
    p_reg->CMD_CODE = LPCXXXX_SIE_DATA_RD_TEST_REG;

    while ((DEF_BIT_IS_CLR(p_reg->DEV_INT_STAT, LPCXXXX_DEV_INT_CD_EMPTY)) &&
           (sie_retry > 0u)) {
        sie_retry--;
    }

    if (sie_retry > 0u) {
        test_reg |= (CPU_INT16U)((p_reg->CMD_DATA & 0x000000FFu) << 8u);
    }

    CPU_CRITICAL_EXIT();

    if (test_reg == LPCXXXX_SIE_CMD_TEST_REG_VAL) {             /* See Note #1.                                         */
        return (DEF_OK);
    }

    return (DEF_FAIL);
}
