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
*                                              STR91xxx
*
* Filename : usbd_drv_str91xxx.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)  : (1) You can find specific information about this driver at:
*                https://doc.micrium.com/display/USBDDRV/STR91xxx
*
*            (2) With an appropriate BSP, this device driver will support the USB full-speed Device
*                Interface module on the following STMicroelectronics MCUs:
*
*                    STR91xxx series
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../../Source/usbd_core.h"
#include  "usbd_drv_str91xxx.h"

/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  STR91X_NBR_EPS                                10u      /* Maximum number of endpoints                          */
#define  STR91X_EPS_BUF_SIZE                           64u      /* Buffer size for each endpoint                        */
#define  STR91X_EPS_CRC_SIZE                            2u      /* Size of the CRC field for receive data               */
#define  STR91X_PKT_MEM_SIZE                         2048u      /* Size of the Packet memory 2K                         */
#define  STR91X_DESC_DATA_TYPE_SIZE                     8u      /* Size of the descriptor data type                     */
                                                                /* Size of the Total Paket buffer Area                  */
#define  STR91X_PKT_BUF_TOTAL_SIZE            (STR91X_PKT_MEM_SIZE        -                  \
                                              (STR91X_DESC_DATA_TYPE_SIZE *                  \
                                               STR91X_NBR_EPS))
                                                                /* Size of the Paket buffer Area used                   */
#define  STR91X_PKT_BUF_AREA_SIZE             (STR91X_NBR_EPS             *                  \
                                               STR91X_EPS_BUF_SIZE        *                  \
                                                        2u)

#define  STR91X_REG_BASE_ADDR                  0x70000000u      /* Base Register Address                                */

                                                                /* --------- USB CONTROL REGISTER BIT DEFINES --------- */
#define  STR91X_CNTR_FRES                      DEF_BIT_00       /* Force USB Reset                                      */
#define  STR91X_CNTR_PWDN                      DEF_BIT_01       /* Power Down                                           */
#define  STR91X_CNTR_LP_MODE                   DEF_BIT_02       /* Low-Power Mode                                       */
#define  STR91X_CNTR_FSUSP                     DEF_BIT_03       /* Force suspend                                        */
#define  STR91X_CNTR_RESUME                    DEF_BIT_04       /* Resume Request                                       */
#define  STR91X_CNTR_SZDPRM                    DEF_BIT_07       /* Short or Zero-Length Received Data Packet            */
#define  STR91X_CNTR_ESOFM                     DEF_BIT_08       /* Expected Start of Frame                              */
#define  STR91X_CNTR_SOFM                      DEF_BIT_09       /* Start of Frame                                       */
#define  STR91X_CNTR_RESETM                    DEF_BIT_10       /* USB Reset request                                    */
#define  STR91X_CNTR_SUSPM                     DEF_BIT_11       /* Suspend mode Request                                 */
#define  STR91X_CNTR_WKUPM                     DEF_BIT_12       /* Wake up                                              */
#define  STR91X_CNTR_ERRM                      DEF_BIT_13       /* Error                                                */
#define  STR91X_CNTR_DOVRM                     DEF_BIT_14       /* DMA Over/Underrun                                    */
#define  STR91X_CNTR_CTRM                      DEF_BIT_15       /* Correct Trasnfer                                     */

#define  STR91X_CNTR_INT_MASK                 (STR91X_CNTR_SZDPRM       |                    \
                                               STR91X_CNTR_ESOFM        |                    \
                                               STR91X_CNTR_SOFM         |                    \
                                               STR91X_CNTR_RESETM       |                    \
                                               STR91X_CNTR_SUSPM        |                    \
                                               STR91X_CNTR_WKUPM        |                    \
                                               STR91X_CNTR_ERRM         |                    \
                                               STR91X_CNTR_DOVRM        |                    \
                                               STR91X_CNTR_CTRM)

                                                                /* ---- USB INTERRUPT STATUS REGISTER BIT DEFINES ----- */
#define  STR91X_ISTR_EP_ID_MASK                0x0000000Fu      /* Endpoint Identifier                                  */
#define  STR91X_ISTR_DIR                       DEF_BIT_04       /* Direction of Transaction                             */
#define  STR91X_ISTR_SZDPR                     DEF_BIT_07       /* Short or Zero-Length Received Data Packet            */
#define  STR91X_ISTR_ESOF                      DEF_BIT_08       /* Expected Start of Frame                              */
#define  STR91X_ISTR_SOF                       DEF_BIT_09       /* Start of Frame                                       */
#define  STR91X_ISTR_RESET                     DEF_BIT_10       /* USB Reset request                                    */
#define  STR91X_ISTR_SUSP                      DEF_BIT_11       /* Suspend mode Request                                 */
#define  STR91X_ISTR_WKUP                      DEF_BIT_12       /* Wake up                                              */
#define  STR91X_ISTR_ERR                       DEF_BIT_13       /* Error                                                */
#define  STR91X_ISTR_DOVR                      DEF_BIT_14       /* DMA Over/Underrun                                    */
#define  STR91X_ISTR_CTR                       DEF_BIT_15       /* Correct Trasnfer                                     */

                                                                /* ------ USB INTERRUPT STATUS CLEAR BIT DEFINES ------ */
#define  STR91X_ISTR_RESET_CLR                 (CPU_INT16U)(~STR91X_ISTR_RESET)
#define  STR91X_ISTR_SUSP_CLR                  (CPU_INT16U)(~STR91X_ISTR_SUSP)
#define  STR91X_ISTR_WKUP_CLR                  (CPU_INT16U)(~STR91X_ISTR_WKUP)
#define  STR91X_ISTR_ERR_CLR                   (CPU_INT16U)(~STR91X_ISTR_ERR)
#define  STR91X_ISTR_DOVR_CLR                  (CPU_INT16U)(~STR91X_ISTR_DOVR)

                                                                /* --------- USB FRAME NUMBER BIT DEFINES ------------- */
#define  STR91X_FNR_RXDP                       DEF_BIT_15       /* Receive Data + Line Status                           */
#define  STR91X_FNR_RXDM                       DEF_BIT_14       /* Receive Data - Line Status                           */
#define  STR91X_FNR_LCK                        DEF_BIT_13       /* Locked bit                                           */
#define  STR91X_FNR_LSOF_MASK                  0x00000C00u      /* Lost SOF                                             */
#define  STR91X_FNR_FN_MAK                     0x000007FFu      /* Frame number                                         */

                                                                /* ----------------- USB WAKE UP EVENTS --------------- */
#define  STR91X_WKUP_ROOT_RST                  DEF_BIT_NONE
#define  STR91X_WKUP_ROOT_RESUME               DEF_BIT_14
#define  STR91X_WKUP_NOISE                     DEF_BIT_15
#define  STR91X_WKUP_UNDEF                     DEF_BIT_15 | DEF_BIT_14

                                                                /* --------- USB DEVICE ADDRESS BIT DEFINES ----------- */
#define  STR91X_DADDR_EA_MASK                  0x0000007Fu      /* Device Address                                       */
#define  STR91X_DADDR_EF                       DEF_BIT_07       /* Enable Function                                      */
#define  STR91X_DADDR_ADD_DFLT                 0x00000000u      /* USB Device Default Address                           */

                                                                /* ------- USB ENDPOINT n REGISTER BIT DEFINES -------- */
#define  STR91X_EPnR_EA_MASK                   0x0000000Fu      /* Endpoint Address Mask                                */
#define  STR91X_EPnR_STAT_TX_DIS            (0x00u <<  4u)      /* All transmission requests addressed are ignored      */
#define  STR91X_EPnR_STAT_TX_STALL          (0x01u <<  4u)      /* The endpoint is stalled. STALL handshake             */
#define  STR91X_EPnR_STAT_TX_NAK            (0x02u <<  4u)      /* The endpoint is naked.   NAK Handshake               */
#define  STR91X_EPnR_STAT_TX_VALID          (0x03u <<  4u)      /* The endpoint is enabled for transmission             */
#define  STR91X_EPnR_STAT_TX_MASK           (0x03u <<  4u)      /* Transmission status mask                             */
#define  STR91X_EPnR_DTOG_TX                   DEF_BIT_06       /* Datta toggle for transmission transfers              */
#define  STR91X_EPnR_CTR_TX                    DEF_BIT_07       /* Correct transfer for Transmission                    */
#define  STR91X_EPnR_EP_KIND                   DEF_BIT_08       /* Endpoint Kind                                        */
#define  STR91X_EPnR_EP_TYPE_MASK            (0x03u << 9u)      /* Endpoint type Mask                                   */
#define  STR91X_EPnR_EP_TYPE_BULK            (0x00u << 9u)      /* Bulk type                                            */
#define  STR91X_EPnR_EP_TYPE_CTRL            (0x01u << 9u)      /* Control type                                         */
#define  STR91X_EPnR_EP_TYPE_ISOC            (0x02u << 9u)      /* Isochronos type                                      */
#define  STR91X_EPnR_EP_TYPE_INT             (0x03u << 9u)      /* Interrupt type                                       */
#define  STR91X_EPnR_SETUP                     DEF_BIT_11       /* Setup transaction completed                          */
#define  STR91X_EPnR_STAT_RX_DIS            (0x00u << 12u)      /* All reception requests addressed are ignored         */
#define  STR91X_EPnR_STAT_RX_STALL          (0x01u << 12u)      /* The endpoint is stalled. STALL handshake             */
#define  STR91X_EPnR_STAT_RX_NAK            (0x02u << 12u)      /* The endpoint is naked.   NAK Handshake               */
#define  STR91X_EPnR_STAT_RX_VALID          (0x03u << 12u)      /* The endpoint is enabled for reception                */
#define  STR91X_EPnR_STAT_RX_MASK           (0x03u << 12u)      /* Reception status mask                                */
#define  STR91X_EPnR_DTOG_RX                  DEF_BIT_14        /* Datta toggle for reception transfers                 */
#define  STR91X_EPnR_CTR_RX                   DEF_BIT_15        /* Correct transfer for reception                       */
                                                                /* Bits the are not toggle-bits                         */
#define  STR91X_USB_EPnR_NO_TOGGLE_MASK       (STR91X_EPnR_CTR_RX        |                   \
                                               STR91X_EPnR_SETUP         |                   \
                                               STR91X_EPnR_EP_TYPE_MASK  |                   \
                                               STR91X_EPnR_EP_KIND       |                   \
                                               STR91X_EPnR_CTR_TX        |                   \
                                               STR91X_EPnR_EA_MASK)

                                                                /* ---------- USB ENDPOINT n REGISTER ACTIONS --------- */
#define  STR91X_EPnR_CTR_RX_CLR                        1u       /* Clear   CTR_RX   bit                                 */
#define  STR91X_EPnR_DTOG_RX_CLR                       2u       /* Clear   DTOG_RX  bit                                 */
#define  STR91X_EPnR_DTOG_RX_SET                       3u       /* Set     DTOG_RX  bit                                 */
#define  STR91X_EPnR_DTOG_RX_TOGGLE                    4u       /* Toggle  DTOG_RX  bit                                 */
#define  STR91X_EPnR_STAT_RX_SET                       5u       /* Set     STAT_RX  field                               */
#define  STR91X_EPnR_EP_KIND_SET                       6u       /* Set     EP_KIND  bit                                 */
#define  STR91X_EPnR_EP_KIND_CLR                       7u       /* Clear   EP_KIND  bit                                 */
#define  STR91X_EPnR_EP_TYPE_SET                       8u       /* Clear   EP_TYPE  field                               */
#define  STR91X_EPnR_CTR_TX_CLR                        9u       /* Clear   CTR_RX   bit                                 */
#define  STR91X_EPnR_DTOG_TX_CLR                      10u       /* Clear   DTOG_TX  bit                                 */
#define  STR91X_EPnR_DTOG_TX_SET                      11u       /* Set     DTOG_TX  bit                                 */
#define  STR91X_EPnR_DTOG_TX_TOGGLE                   12u       /* Toggle  DTOG_RX  bit                                 */
#define  STR91X_EPnR_STAT_TX_SET                      13u       /* Set     STAT_TX  field                               */

                                                                /* ---------- USB ADDRn REGISTER BIT DEFINES ---------- */
#define  STR91X_ADDRn_ADDR_RX_MASK            0xFFFF0000u       /* Reception Buffer Address Mask                        */
#define  STR91X_ADDRn_ADDR_TX_MASK            0x0000FFFFu       /* Transmission Buffer Address Mask                     */
#define  STR91X_COUNTn_BL_SIZE                DEF_BIT_31        /* Block Size                                           */

                                                                /* --------- USB COUNTn REGISTER BIT DEFINES ---------- */
#define  STR91X_COUNTn_NUM_BLOCK_MASK         0x7C000000u       /* Number of Blocks                                     */
#define  STR91X_COUNTn_COUNT_RX_MASK          0x03FF0000u       /* Reception byte count                                 */
#define  STR91X_COUNTn_COUNT_TX_MASK          0x000003FFu       /* Trasnmission byte count                              */
#define  STR91X_COUNTn_MEM_BLK_MASK           0xFC000000u       /* Memory Buffer definition (Blk size + Nbr_blk)        */

                                                                /* --------- USB ENDPOINT DIRECTION DEFINE ------------ */
#define  STR91X_EP_DIR_IN                               0u      /* EP IN  (Tx)                                          */
#define  STR91X_EP_DIR_OUT                              1u      /* EP OUT (Rx)                                          */


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
*                                            LOCAL DATA TYPES
*
* Note(s) : (1) The buffer descriptor is defined as :
*
*           +------------+
*           | COUNTn_TX  |----------------------------|
*           +------------+              +--------+    |
*           | ADDRn_TX   |------|       | COUNTn | <---
*           +------------+      |       +--------+    |
*           | COUNTn_TX  |--|   ------->| ADDRn  |    |
*           +------------+  |   |       +--------+    |
*           | ADDRn_RX   |--|---|   (STR91x Register) |
*           +------------+  |-------------------------|
*
*          (2) The Endpoint Buffer is defined  as:
*
*           +---------------+
*           | RX Buffer EPn |
*           +---------------+
*           | TX Buffer EPn |
*           +---------------+
*           | . . . . . .   |
*           +---------------+
*           | RX Buffer EP0 |
*           +---------------+
*           | TX Buffer EP0 |
*           +---------------+

*          (3) The Buffer Descriptor Table is defined  as:
*
*           +------------+
*           | COUNTn_TX  |
*           +------------+           +---------------+
*           | ADDRn_RX   |---------->| RX Buffer EPn |
*           +------------+           +---------------+
*           | COUNTn_TX  |
*           +------------+           +---------------+
*           | ADDRn_RX   |---------->| RX Buffer EPn |
*           +------------+           +---------------+
*
*********************************************************************************************************
*/

typedef  struct  usbf_str91x_desc {
    CPU_REG32  USB_ADDRn;                                       /* Packet buffer address                                */
    CPU_REG32  USB_COUNTn;                                      /* Packet byte counter                                  */
}  USBD_STR91X_DESC;

typedef  struct  usbf_str91x_buf {
    CPU_INT32U  TxBuf[STR91X_EPS_BUF_SIZE / 4u];                /* Packet buffer address                                */
    CPU_INT32U  RxBuf[STR91X_EPS_BUF_SIZE / 4u];                /* Packet byte counter                                  */
}  USBD_STR91X_BUF;

typedef  struct  usbf_str91x_reg {
    USBD_STR91X_DESC  USB_BUF_DESC_TBL[STR91X_NBR_EPS];         /* Buffer Description table                             */
                                                                /* Packet Buffer Memory                                 */
    CPU_REG08         USB_BUF_PKT[STR91X_PKT_BUF_TOTAL_SIZE];
                                                                /* ---------------- REGISTER DEFINITION --------------- */
    CPU_REG32         USB_EPnR[STR91X_NBR_EPS];                 /* Endpoints-specific register                          */
    CPU_REG32         RSVD0[6u];
    CPU_REG32         USB_CNTR;                                 /* USB Control Register                                 */
    CPU_REG32         USB_ISTR;                                 /* USB Interrupt Status Register                        */
    CPU_REG32         USB_FNR;                                  /* USB Frame Number register                            */
    CPU_REG32         USB_DADDR;                                /* USB Device Address                                   */
    CPU_REG32         USB_BTABLE;                               /* Buffer Table Address                                 */
    CPU_REG32         USB_DMACR1;                               /* DMA Control Register 1                               */
    CPU_REG32         USB_DMACR2;                               /* DMA Control Register 2                               */
    CPU_REG32         USB_DMACR3;                               /* DMA Control Register 3                               */
    CPU_REG32         USB_DMABSIZE;                             /* DMA burst size register                              */
    CPU_REG32         USB_DMADALLI;                             /* DMA LLI register                                     */
}  USBD_STR91X_REG;


/*
*********************************************************************************************************
*                                          DRIVER DATA TYPE
*********************************************************************************************************
*/

typedef  struct  usbd_drv_data_ep {                             /* ---------- DEVICE ENDPOINT DATA STRUCTURE ---------- */
    CPU_INT16U  EP_MaxPktSize[STR91X_NBR_EPS * 2u];
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

static  void  STR91X_ADDRn_SetReg (USBD_STR91X_REG  *p_reg,
                                   CPU_INT08U        ep_nbr,
                                   CPU_BOOLEAN       ep_dir,
                                   CPU_INT32U        ep_reg_val);

static  void  STR91X_COUNTn_SetReg(USBD_STR91X_REG  *p_reg,
                                   CPU_INT08U        ep_nbr,
                                   CPU_BOOLEAN       ep_dir,
                                   CPU_INT32U        ep_reg_val);

static  void  STR91X_EPnR_SetReg  (USBD_STR91X_REG  *p_reg,
                                   CPU_INT08U        ep_nbr,
                                   CPU_INT32U        ep_reg_val,
                                   CPU_INT08U        ep_action);

static  void  STR91X_ProcessEP    (USBD_STR91X_REG  *p_reg,
                                   USBD_DRV         *p_drv,
                                   CPU_INT08U        ep_nbr,
                                   CPU_BOOLEAN       ep_dir_bit);

static  void  STR91X_ProcessSetup (USBD_STR91X_REG  *p_reg,
                                   USBD_DRV         *p_drv,
                                   CPU_INT08U        ep_nbr);

static  void  STR91X_Reset        (USBD_STR91X_REG  *p_reg);

static  void  STR91X_TxBufWr      (CPU_INT32U       *p_dest,
                                   CPU_INT08U       *p_src,
                                   CPU_INT32U        len);


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

USBD_DRV_API  USBD_DrvAPI_STR91XXX = { USBD_DrvInit,
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
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_DrvInit (USBD_DRV  *p_drv,
                            USBD_ERR  *p_err)
{
    USBD_DRV_BSP_API  *p_bsp_api;
    USBD_DRV_DATA_EP  *p_drv_data;
    LIB_ERR            lib_mem_err;
    CPU_SIZE_T         reqd_octets;

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

    p_drv->DataPtr = p_drv_data;                                /* Store drv internal data ptr.                         */

    p_bsp_api = p_drv->BSP_API_Ptr;                             /* Get driver BSP API reference.                        */

    if (p_bsp_api->Init != (void *)0) {
        p_bsp_api->Init(p_drv);                                 /* Call board/chip specific device controller ...       */
                                                                /* ... initialization function.                         */
    }

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
    USBD_STR91X_REG   *p_reg;
    USBD_DRV_BSP_API  *p_bsp_api;


    p_bsp_api =  p_drv->BSP_API_Ptr;                            /* Get driver BSP API reference.                        */
    p_reg     = (USBD_STR91X_REG *)p_drv->CfgPtr->BaseAddr;     /* Get ref to USB HW reg.                               */

    p_reg->USB_CNTR = STR91X_CNTR_FRES;                         /* Force reset.                                         */
    p_reg->USB_CNTR = 0x0000u;                                  /* Disable all interrupts                               */
    p_reg->USB_ISTR = 0x0000u;                                  /* Clear   all pending interrupts                       */
    p_reg->USB_CNTR = STR91X_CNTR_RESETM;                       /* Enable USB Reset Interrupt                           */

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
    USBD_STR91X_REG   *p_reg;
    USBD_DRV_BSP_API  *p_bsp_api;


    p_bsp_api =  p_drv->BSP_API_Ptr;                            /* Get driver BSP API reference.                        */
    p_reg     = (USBD_STR91X_REG *)p_drv->CfgPtr->BaseAddr;     /* Get ref to USB HW reg.                               */

    if (p_bsp_api->Disconn != (void *)0) {
        p_bsp_api->Disconn();
    }

    p_reg->USB_CNTR = 0x0000u;                                  /* Disable all interrupts                               */
    p_reg->USB_ISTR = 0x0000u;                                  /* Clear   all pending interrupts                       */
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
    USBD_STR91X_REG  *p_reg;


    p_reg = (USBD_STR91X_REG *)p_drv->CfgPtr->BaseAddr;         /* Get ref to USB HW reg.                               */

    p_reg->USB_DADDR = (dev_addr | STR91X_DADDR_EF);            /* Enable the USB Device & set the dftl USB dev Addr.   */

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
    USBD_STR91X_REG  *p_reg;
    CPU_INT16U        frame_nbr;


    p_reg     = (USBD_STR91X_REG *)p_drv->CfgPtr->BaseAddr;
    frame_nbr = (CPU_INT16U       )p_reg->USB_FNR & STR91X_FNR_FN_MAK;

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
    CPU_INT08U         ep_nbr;
    CPU_INT08U         ep_phy_nbr;
    CPU_BOOLEAN        ep_dir_in;
    CPU_INT08U         nbr_blks;
    USBD_STR91X_BUF   *p_ep_buf;
    USBD_STR91X_REG   *p_reg;
    USBD_DRV_DATA_EP  *p_drv_data;

    (void)transaction_frame;


    p_reg      = (USBD_STR91X_REG  *)p_drv->CfgPtr->BaseAddr;   /* Get ref to USB HW reg.                               */
    p_drv_data = (USBD_DRV_DATA_EP *)(p_drv->DataPtr);
    ep_nbr     =  USBD_EP_ADDR_TO_LOG(ep_addr);
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    ep_dir_in  =  USBD_EP_IS_IN(ep_addr);                       /* bEndpointAddress bit 7. 1 IN (Tx) - 0 OUT (Rx).      */
    p_ep_buf   = (USBD_STR91X_BUF *)(&p_reg->USB_BUF_PKT[0u]);

                                                                /* ---------------- SET ENDPOINT TYPE ----------------- */
    switch (ep_type) {
        case USBD_EP_TYPE_CTRL:
             STR91X_EPnR_SetReg(p_reg,
                                ep_nbr,
                                STR91X_EPnR_EP_TYPE_CTRL,
                                STR91X_EPnR_EP_TYPE_SET);
             break;


        case USBD_EP_TYPE_ISOC:
             STR91X_EPnR_SetReg(p_reg,
                                ep_nbr,
                                STR91X_EPnR_EP_TYPE_ISOC,
                                STR91X_EPnR_EP_TYPE_SET);
             break;


        case USBD_EP_TYPE_BULK:
             STR91X_EPnR_SetReg(p_reg,
                                ep_nbr,
                                STR91X_EPnR_EP_TYPE_BULK,
                                STR91X_EPnR_EP_TYPE_SET);
             break;


        case USBD_EP_TYPE_INTR:
             STR91X_EPnR_SetReg(p_reg,
                                ep_nbr,
                                STR91X_EPnR_EP_TYPE_INT,
                                STR91X_EPnR_EP_TYPE_SET);
             break;


        default:
             break;
    }

                                                                /* Set the Allocated buffer memory                      */
    nbr_blks = ((STR91X_EPS_BUF_SIZE * 2u ) / 32u) - 1u;
    p_reg->USB_BUF_DESC_TBL[ep_nbr].USB_COUNTn = (STR91X_COUNTn_BL_SIZE     )
                                               | ((nbr_blks << 26u)
                                               &  STR91X_COUNTn_MEM_BLK_MASK);

    if (ep_type != USBD_EP_TYPE_CTRL) {
        if (ep_dir_in == DEF_TRUE) {                            /* ------ ENDPOINT IN - SETUP DEVICE TRANSMISSION ----- */
                                                                /* Initializes the DATA0/DATA1 Tx toggle bit to '0'     */
            STR91X_EPnR_SetReg(p_reg,
                               ep_nbr,
                               DEF_BIT_NONE,
                               STR91X_EPnR_DTOG_TX_CLR);

                                                                /* Set the Tx buffer address in the desc. table         */
            STR91X_ADDRn_SetReg(              p_reg,
                                              ep_nbr,
                                (CPU_BOOLEAN) STR91X_EP_DIR_IN,
                                (CPU_INT32U )&(p_ep_buf[ep_nbr].TxBuf[0u]));

                                                                /* Set the packet length                                */
            STR91X_COUNTn_SetReg(             p_reg,
                                              ep_nbr,
                                 (CPU_BOOLEAN)STR91X_EP_DIR_IN,
                                              0u);

                                                                /* NAK Tx request                                       */
            STR91X_EPnR_SetReg(p_reg,
                               ep_nbr,
                               STR91X_EPnR_STAT_TX_NAK,
                               STR91X_EPnR_STAT_TX_SET);

        } else {                                                /* ------- ENDPOINT OUT - SETUP DEVICE RECEPTION ------ */
                                                                /* Set the Rx buffer address in the desc. table         */
            STR91X_ADDRn_SetReg(              p_reg,
                                              ep_nbr,
                                (CPU_BOOLEAN) STR91X_EP_DIR_OUT,
                                (CPU_INT32U )&(p_ep_buf[ep_nbr].RxBuf[0u]));

                                                                /* Enable Rx requests                                   */
            STR91X_EPnR_SetReg(p_reg,
                               ep_nbr,
                               STR91X_EPnR_STAT_RX_VALID,
                               STR91X_EPnR_STAT_RX_SET);

        }
    } else {
                                                                /* Initializes the DATA0/DATA1 Tx toggle bit to '1'     */
        STR91X_EPnR_SetReg(p_reg,
                           ep_nbr,
                           DEF_BIT_NONE,
                           STR91X_EPnR_DTOG_TX_SET);

                                                                /* Initializes the DATA0/DATA1 Rx toggle bit to '0'     */
        STR91X_EPnR_SetReg(p_reg,
                           ep_nbr,
                           DEF_BIT_NONE,
                           STR91X_EPnR_DTOG_RX_CLR);

                                                                /* Initializes the the STAT_TX and STAT_RX to NAK       */
        STR91X_EPnR_SetReg(p_reg,
                           ep_nbr,
                           STR91X_EPnR_STAT_TX_NAK,
                           STR91X_EPnR_STAT_TX_SET);

        STR91X_EPnR_SetReg(p_reg,
                           ep_nbr,
                           STR91X_EPnR_STAT_RX_NAK,
                           STR91X_EPnR_STAT_RX_SET);

                                                                /* Set the Rx, Tx buffer address in the desc. table     */
        STR91X_ADDRn_SetReg(              p_reg,
                                          ep_nbr,
                            (CPU_BOOLEAN) STR91X_EP_DIR_IN,
                            (CPU_INT32U )&(p_ep_buf[ep_nbr].TxBuf[0u]));

        STR91X_ADDRn_SetReg(              p_reg,
                                          ep_nbr,
                            (CPU_BOOLEAN) STR91X_EP_DIR_OUT,
                            (CPU_INT32U )&(p_ep_buf[ep_nbr].RxBuf[0u]));
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
    USBD_STR91X_REG   *p_reg;
    USBD_DRV_DATA_EP  *p_drv_data;
    CPU_INT08U         ep_nbr;
    CPU_INT08U         ep_phy_nbr;
    CPU_BOOLEAN        ep_dir_in;


    p_reg      = (USBD_STR91X_REG *)p_drv->CfgPtr->BaseAddr;     /* Get ref to USB HW reg.                               */
    p_drv_data = (USBD_DRV_DATA_EP  *)p_drv->DataPtr;
    ep_nbr     =  USBD_EP_ADDR_TO_LOG(ep_addr);
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    ep_dir_in  =  DEF_BIT_IS_SET(ep_addr, DEF_BIT_07);

    if (ep_dir_in) {
        STR91X_ADDRn_SetReg(             p_reg,
                                         ep_nbr,
                            (CPU_BOOLEAN)STR91X_EP_DIR_IN,
                                         0u);

        STR91X_COUNTn_SetReg(            p_reg,
                                         ep_nbr,
                            (CPU_BOOLEAN)STR91X_EP_DIR_IN,
                                         0u);

        STR91X_EPnR_SetReg(p_reg,
                           ep_nbr,
                           STR91X_EPnR_STAT_TX_DIS,
                           STR91X_EPnR_STAT_TX_SET);

    } else {

        STR91X_ADDRn_SetReg(             p_reg,
                                         ep_nbr,
                            (CPU_BOOLEAN)STR91X_EP_DIR_OUT,
                                         0u);

        STR91X_COUNTn_SetReg(            p_reg,
                                         ep_nbr,
                            (CPU_BOOLEAN)STR91X_EP_DIR_OUT,
                                         0u);

        STR91X_EPnR_SetReg(p_reg,
                           ep_nbr,
                           STR91X_EPnR_STAT_RX_DIS,
                           STR91X_EPnR_STAT_RX_SET);
    }

    p_drv_data->EP_MaxPktSize[ep_phy_nbr] = 0u;
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
    USBD_STR91X_REG  *p_reg;
    CPU_INT08U        ep_nbr;


    (void)p_buf;

    p_reg  = (USBD_STR91X_REG *)p_drv->CfgPtr->BaseAddr;        /* Get ref to USB HW reg.                               */
    ep_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);

    STR91X_EPnR_SetReg(p_reg,
                       ep_nbr,
                       STR91X_EPnR_STAT_RX_VALID,
                       STR91X_EPnR_STAT_RX_SET);

   *p_err = USBD_ERR_NONE;

    return (DEF_MIN(STR91X_EPS_BUF_SIZE, buf_len));
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
    CPU_INT08U        ep_nbr;
    CPU_INT16U        len_tot;
    CPU_INT32U       *p_buf_addr;
    USBD_STR91X_BUF  *p_ep_buf;
    CPU_INT32U        cntr_flags;
    USBD_STR91X_REG  *p_reg;


    p_reg  = (USBD_STR91X_REG *)p_drv->CfgPtr->BaseAddr;        /* Get ref to USB HW reg.                               */
    ep_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);

                                                                /* Disable control interrupts.                          */
    cntr_flags = (p_reg->USB_CNTR & STR91X_CNTR_INT_MASK);
    DEF_BIT_CLR(p_reg->USB_CNTR, STR91X_CNTR_INT_MASK);

    len_tot    = (p_reg->USB_BUF_DESC_TBL[ep_nbr].USB_COUNTn >> 16u) & 0x03FFu;
    p_ep_buf   = (USBD_STR91X_BUF *)(&p_reg->USB_BUF_PKT[0u]);
    p_buf_addr =  &(p_ep_buf[ep_nbr].RxBuf[0u]);

    if (len_tot > buf_len) {
       *p_err   = USBD_ERR_RX;
        len_tot = buf_len;
    } else {
       *p_err   = USBD_ERR_NONE;
    }

    Mem_Copy((void *)p_buf,
             (void *)p_buf_addr,
                     len_tot);

    DEF_BIT_SET(p_reg->USB_CNTR, cntr_flags);                   /* Enable control interrupts.                           */

    return (len_tot);
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
    USBD_STR91X_REG   *p_reg;
    USBD_STR91X_BUF   *p_ep_buf;
    USBD_DRV_DATA_EP  *p_drv_data;
    CPU_INT08U         ep_nbr;
    CPU_INT08U         ep_phy_nbr;
    CPU_INT16U         ep_pkt_len;
    CPU_INT32U         cntr_flags;


    p_reg      = (USBD_STR91X_REG  *)p_drv->CfgPtr->BaseAddr;   /* Get ref to USB HW reg.                               */
    p_drv_data = (USBD_DRV_DATA_EP *)p_drv->DataPtr;
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    ep_pkt_len = (CPU_INT16U)DEF_MIN(p_drv_data->EP_MaxPktSize[ep_phy_nbr], buf_len);
    ep_nbr     =  USBD_EP_ADDR_TO_LOG(ep_addr);

    if (ep_pkt_len > STR91X_EPS_BUF_SIZE) {                     /* If the packet length is greather that the max        */
        ep_pkt_len = STR91X_EPS_BUF_SIZE;
    }
                                                                /* Disable USB control interrupts.                      */
    cntr_flags = p_reg->USB_CNTR & STR91X_CNTR_INT_MASK;
    DEF_BIT_CLR(p_reg->USB_CNTR, STR91X_CNTR_INT_MASK);

                                                                /* Point to the Packet buffer memory                    */
    p_ep_buf = (USBD_STR91X_BUF *)(&p_reg->USB_BUF_PKT[0u]);

    STR91X_TxBufWr((CPU_INT32U *)(&(p_ep_buf[ep_nbr].TxBuf[0u])),
                                    p_buf,
                                    ep_pkt_len);

                                                                /* Set the packet length                                */
    STR91X_COUNTn_SetReg(             p_reg,
                                      ep_nbr,
                         (CPU_BOOLEAN)STR91X_EP_DIR_IN,
                                      ep_pkt_len);

    DEF_BIT_SET(p_reg->USB_CNTR, cntr_flags);                   /* Enable control interrupts.                           */

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
    USBD_STR91X_REG  *p_reg;
    CPU_INT08U        ep_nbr;

    (void)p_buf;
    (void)buf_len;

    p_reg  = (USBD_STR91X_REG *)p_drv->CfgPtr->BaseAddr;        /* Get ref to USB HW reg.                               */
    ep_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);

    STR91X_EPnR_SetReg(p_reg,
                       ep_nbr,
                       STR91X_EPnR_STAT_TX_VALID,
                       STR91X_EPnR_STAT_TX_SET);

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
    USBD_STR91X_REG  *p_reg;
    CPU_INT08U        ep_nbr;


    p_reg  = (USBD_STR91X_REG *)p_drv->CfgPtr->BaseAddr;        /* Get ref to USB HW reg.                               */
    ep_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);

                                                                /* Set the packet length                                */
    STR91X_COUNTn_SetReg(             p_reg,
                                      ep_nbr,
                         (CPU_BOOLEAN)STR91X_EP_DIR_IN,
                                      0u);
                                                                /* Set the Tx enpoint status valid                      */
    STR91X_EPnR_SetReg(p_reg,
                       ep_nbr,
                       STR91X_EPnR_STAT_TX_VALID,
                       STR91X_EPnR_STAT_TX_SET);

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
    CPU_INT08U   ep_nbr;
    CPU_BOOLEAN  ep_stall;


    ep_nbr   = USBD_EP_ADDR_TO_LOG(ep_addr);
    ep_stall = DEF_OK;

    if (ep_nbr > 0u) {
        ep_stall = USBD_DrvEP_Stall(p_drv, ep_addr, DEF_CLR);
    }

    return (ep_stall);
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
    USBD_STR91X_REG  *p_reg;
    CPU_INT08U        ep_nbr;
    CPU_BOOLEAN       ep_dir_in;
    CPU_INT32U        ep_type;


    p_reg     = (USBD_STR91X_REG *)p_drv->CfgPtr->BaseAddr;     /* Get ref to USB HW reg.                               */
    ep_nbr    =  USBD_EP_ADDR_TO_LOG(ep_addr);
    ep_type   = (p_reg->USB_EPnR[ep_nbr] & STR91X_EPnR_EP_TYPE_MASK);
    ep_dir_in =  USBD_EP_IS_IN(ep_addr);

    switch(state) {
        case DEF_SET:
             if (ep_dir_in) {
                                                                /* Set the Tx enpoint STALL state                       */
                 STR91X_EPnR_SetReg(p_reg,
                                    ep_nbr,
                                    STR91X_EPnR_STAT_TX_STALL,
                                    STR91X_EPnR_STAT_TX_SET);
             } else {
                 STR91X_EPnR_SetReg(p_reg,
                                    ep_nbr,
                                    STR91X_EPnR_STAT_RX_STALL,
                                    STR91X_EPnR_STAT_RX_SET);
             }
             break;


        case DEF_CLR:
             if (ep_type == STR91X_EPnR_EP_TYPE_CTRL) {
                                                                /* Initializes the DATA0/DATA1 Tx toggle bit to '1'     */
                 STR91X_EPnR_SetReg(p_reg,
                                    ep_nbr,
                                    DEF_BIT_NONE,
                                    STR91X_EPnR_DTOG_TX_SET);

                                                                /* Initializes the DATA0/DATA1 Rx toggle bit to '0'     */
                 STR91X_EPnR_SetReg(p_reg,
                                    ep_nbr,
                                    DEF_BIT_NONE,
                                    STR91X_EPnR_DTOG_RX_CLR);

                                                                /* Initializes the the STAT_TX and STAT_RX to NAK       */
                 STR91X_EPnR_SetReg(p_reg,
                                    ep_nbr,
                                    STR91X_EPnR_STAT_TX_NAK,
                                    STR91X_EPnR_STAT_TX_SET);

                 STR91X_EPnR_SetReg(p_reg,
                                    ep_nbr,
                                    STR91X_EPnR_STAT_RX_NAK,
                                    STR91X_EPnR_STAT_RX_SET);

             } else {

                 if (ep_dir_in) {
                     STR91X_EPnR_SetReg(p_reg,
                                        ep_nbr,
                                        STR91X_EPnR_STAT_TX_NAK,
                                        STR91X_EPnR_STAT_TX_SET);

                     STR91X_EPnR_SetReg(p_reg,
                                        ep_nbr,
                                        DEF_BIT_NONE,
                                        STR91X_EPnR_DTOG_TX_CLR);

                 } else {
                     STR91X_EPnR_SetReg(p_reg,
                                        ep_nbr,
                                        STR91X_EPnR_STAT_RX_VALID,
                                        STR91X_EPnR_STAT_RX_SET);

                     STR91X_EPnR_SetReg(p_reg,
                                        ep_nbr,
                                        DEF_BIT_NONE,
                                        STR91X_EPnR_DTOG_RX_CLR);
                  }
             }
             break;


        default:
             return(DEF_FAIL);
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
    USBD_STR91X_REG  *p_reg;
    CPU_INT08U        ep_nbr;
    CPU_INT16U        istr;


    p_reg = (USBD_STR91X_REG *)p_drv->CfgPtr->BaseAddr;         /* Get ref to USB HW reg.                               */

                                                                /* --------------- GET INTERRUPT STATUS --------------- */
    istr  =  p_reg->USB_ISTR;
                                                                /* ---------------- USB RESET INTERRUPT --------------- */
    if (DEF_BIT_IS_SET(istr, STR91X_ISTR_RESET) == DEF_YES) {
        p_reg->USB_ISTR = STR91X_ISTR_RESET_CLR;                /* Clear the reset interrupt                            */
        DEF_BIT_SET(p_reg->USB_CNTR, (STR91X_CNTR_CTRM  |
                                      STR91X_CNTR_SUSPM |
                                      STR91X_CNTR_WKUPM));
        STR91X_Reset(p_reg);                                    /* Reset the STR91X EP's registers                      */
        USBD_EventReset(p_drv);                                 /* Notify the USB stack of a reset event                */
    }

                                                                /* --------------- USB SUSPEND INTERRUPT -------------- */
    if (DEF_BIT_IS_SET(istr, STR91X_ISTR_SUSP) == DEF_YES) {
        USBD_EventSuspend(p_drv);
        DEF_BIT_SET(p_reg->USB_CNTR, STR91X_CNTR_FSUSP);
        p_reg->USB_ISTR = STR91X_ISTR_SUSP_CLR;
    }

                                                                /* --------------- USB WAKEUP INTERRUPT --------------- */
    if (DEF_BIT_IS_SET(istr, STR91X_ISTR_WKUP) == DEF_YES) {
        USBD_EventResume(p_drv);
        DEF_BIT_CLR(p_reg->USB_CNTR, STR91X_CNTR_FSUSP);
        p_reg->USB_ISTR = STR91X_ISTR_WKUP_CLR;
    }

                                                                /* ----------- CORRECT TRANSFER INTERRUPT ------------- */
    if (DEF_BIT_IS_SET(istr, STR91X_ISTR_CTR) == DEF_YES) {
        while (DEF_BIT_IS_SET(p_reg->USB_ISTR, STR91X_ISTR_CTR) == DEF_TRUE) {
            ep_nbr = p_reg->USB_ISTR & STR91X_ISTR_EP_ID_MASK;

            if (DEF_BIT_IS_SET(p_reg->USB_ISTR, STR91X_ISTR_DIR) == DEF_TRUE) {
                STR91X_ProcessEP(p_reg,
                                 p_drv,
                                 ep_nbr,
                                 DEF_TRUE);
            } else {
                STR91X_ProcessEP(p_reg,
                                 p_drv,
                                 ep_nbr,
                                 DEF_FALSE);
            }
        }
    }

    if (DEF_BIT_IS_SET(istr, STR91X_ISTR_ERR) == DEF_YES) {
        p_reg->USB_ISTR = STR91X_ISTR_ERR_CLR;
    }
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
**************************************************************************************************************
*                                         STR91X_Reset()
*
* Description : Reset the STR91X EP register to the default value
*
* Argument(s) : p_reg       Pointer to register structure.
*
* Return(s)   : none.
*
* Note(s)     : None.
**************************************************************************************************************
*/

static  void  STR91X_Reset (USBD_STR91X_REG  *p_reg)
{
    CPU_INT08U  ep_nbr;


    p_reg->USB_BTABLE =  0x0000u;                               /* Set the buffer table address to 0x0000               */
                                                                /* Set the ep addr in the USB_EPnR register             */
    p_reg->USB_DADDR  = (STR91X_DADDR_EF |                      /* Enable the USB Device & set the dftl USB dev Addr.   */
                         STR91X_DADDR_ADD_DFLT);

    for (ep_nbr = 0u; ep_nbr < STR91X_NBR_EPS; ep_nbr++) {
        p_reg->USB_EPnR[ep_nbr]                    = (STR91X_EPnR_EA_MASK & ep_nbr);
        p_reg->USB_BUF_DESC_TBL[ep_nbr].USB_COUNTn =  0u;
        p_reg->USB_BUF_DESC_TBL[ep_nbr].USB_ADDRn  =  0u;
    }
}


/*
**************************************************************************************************************
*                                         STR91X_ProcessEP()
*
* Description : Process IN, OUT and SETUP packets for a specific endpoint.
*
* Argument(s) : p_reg           Pointer to register structure.
*
*               p_drv           Pointer to device driver structure.
*
*               ep_nbr          Endpoint number 0, 1 , 2  ... STR91X_NBR_EPS.
*
*               ep_dir_bit      Endpoint direction
*                               if DEF_TRUE  the enpoint is a OUT endpoint.
*                               if DEF_FALSE the endpoint is a IN endpoint
*
* Return(s)   : none.
*
* Note(s)     : None.
**************************************************************************************************************
*/

static  void  STR91X_ProcessEP (USBD_STR91X_REG  *p_reg,
                                USBD_DRV         *p_drv,
                                CPU_INT08U        ep_nbr,
                                CPU_BOOLEAN       ep_dir_bit)
{
    CPU_BOOLEAN  ctr_rx;
    CPU_BOOLEAN  ctr_tx;
    CPU_BOOLEAN  setup;


    ctr_tx =  DEF_BIT_IS_SET(p_reg->USB_EPnR[ep_nbr],
                             STR91X_EPnR_CTR_TX);
    ctr_rx =  DEF_BIT_IS_SET(p_reg->USB_EPnR[ep_nbr],
                             STR91X_EPnR_CTR_RX);
    setup  =  DEF_BIT_IS_SET(p_reg->USB_EPnR[ep_nbr],
                             STR91X_EPnR_SETUP);

    if (((ep_dir_bit == DEF_TRUE) &&
         (ctr_rx     == DEF_TRUE) &&
         (ctr_tx     == DEF_TRUE)) ||
        ((ep_dir_bit == DEF_FALSE) &&
         (ctr_tx     == DEF_TRUE))) {
                                                                /* Process IN transaction                               */
        STR91X_EPnR_SetReg(p_reg,
                           ep_nbr,
                           DEF_BIT_NONE,
                           STR91X_EPnR_CTR_TX_CLR);

        USBD_EP_TxCmpl(p_drv, ep_nbr);
    }

    if ((ep_dir_bit == DEF_TRUE) &&
        (ctr_rx     == DEF_TRUE)) {

        if (setup == DEF_TRUE) {
                                                                /* Process the Setup package                            */
            STR91X_ProcessSetup(p_reg,
                                p_drv,
                                ep_nbr);

            STR91X_EPnR_SetReg(p_reg,
                               ep_nbr,
                               DEF_BIT_NONE,
                               STR91X_EPnR_CTR_RX_CLR);

        } else {                                                /* Process the OUT transaction                          */

            STR91X_EPnR_SetReg(p_reg,
                               ep_nbr,
                               DEF_BIT_NONE,
                               STR91X_EPnR_CTR_RX_CLR);

            USBD_EP_RxCmpl(p_drv, ep_nbr);
        }
    }
}


/*
**************************************************************************************************************
*                                      STR91X_ProcessSetup()
*
* Description : Process SETUP packets.
*
* Argument(s) : p_reg       Pointer to register structure.
*
*               p_drv       Pointer to device driver structure.
*
*               ep_nbr      The enpoint number.
*
* Return(s)   : none.
*
* Note(s)     : None.
**************************************************************************************************************
*/

static  void  STR91X_ProcessSetup (USBD_STR91X_REG  *p_reg,
                                   USBD_DRV         *p_drv,
                                   CPU_INT08U        ep_nbr)
{
    CPU_INT16U        buf_len;
    CPU_INT32U       *p_buf_addr;
    CPU_INT08U        setup_pkt[8u];
    USBD_STR91X_BUF  *p_ep_buf;


    buf_len    = (p_reg->USB_BUF_DESC_TBL[ep_nbr].USB_COUNTn >> 16u) & 0x03FFu;
    p_ep_buf   = (USBD_STR91X_BUF *) &(p_reg->USB_BUF_PKT[0u]);
    p_buf_addr =  &(p_ep_buf[ep_nbr].RxBuf[0]);

    if (buf_len > 8u) {
        buf_len = 8u;
    }

    Mem_Copy((void *)&setup_pkt[0u],
             (void *) p_buf_addr,
                      buf_len);

    USBD_EventSetup(p_drv, (void *)&setup_pkt[0u]);
}


/*
**************************************************************************************************************
*                                     STR91X_EPnR_SetReg()
*
* Description : Set a value in the EPnR register.
*
* Argument(s) : p_reg           Pointer to register structure.
*
*               ep_nbr          The enpoint number.
*
*               ep_reg_val      value to be set in the enpoint register.
*
*               ep_action       Action to be perform in the EPnP register (where m can be T(Transmission) or R(reception):
*
*                               STR91X_USB_EPnR_CTR_mX_CLR       Clear the CTR_RX/CTR_TX bit in the EPnPR register.
*
*                               STR91X_USB_EPnR_DTOG_mX_CLR      Clear the Data Toggle bit (0 = DATA0 PID)
*
*                               STR91X_USB_EPnR_DTOG_mX_SET      Set the Data Toggle bit   (1 = DATA1 PID)
*
*                               STR91X_USB_EPnR_DTOG_mX_TOGGLE   Toggles the Data Toggle data  bit(0->1 or 1->0)
*
*                               STR91X_USB_EPnR_STAT_mX_SET      Set the Endpoint Status.
*
*                               STR91X_USB_EPnR_EP_KIND_SET      Set the endpoind kind bit.
*
*                               STR91X_USB_EPnR_EP_TYPE_SET      Set the Endpoint type.
*
* Return(s)   : none.
*
* Note(s)     : (1) Read-modify-write cycle on the EPnR registers should be avoided because between the read and
*                   write operations some bits could be set by the hardware and the next write would modify
*                   them before the CPU has the time to detect the change.
*
*               (2) Since the are some toogle-bits the values in these registers need to be read first
*                   interpretated and later written in the register:
*                   e.g. Current Value       '01'
*                        Desire  Value       '11'
*                        Value to be Written '10'
*
*               (3) CTR_RX and CTR_TX bits in USB_EPnR register indicate respectively IN transaction successful
*                   completion and OUT transaction successful completion. Once one of these bits or both are
*                   set by the device controller, an interrupt will be generated. It is important to not clear
*                   these bits accidently if there are set to '1'. Otherwise, interrupt for transfer completion
*                   could be missed. This line of code prevent an accidental clear of CTR_RX and CTR_TX bits
*                   for all actions except with STR91X_USB_EPnR_CTR_mX_CLR which is the explicit clearing
*                   operation for CTR_RX or CTR_TX bits
**************************************************************************************************************
*/

static  void  STR91X_EPnR_SetReg (USBD_STR91X_REG  *p_reg,
                                  CPU_INT08U        ep_nbr,
                                  CPU_INT32U        ep_reg_val,
                                  CPU_INT08U        ep_action)
{
    CPU_INT32U  reg_val;
    CPU_INT32U  reg_mask;


    reg_val  =  p_reg->USB_EPnR[ep_nbr];
    reg_mask =  STR91X_USB_EPnR_NO_TOGGLE_MASK;
                                                                /* See Note #3.                                         */
    DEF_BIT_SET(reg_val, (STR91X_EPnR_CTR_RX |
                          STR91X_EPnR_CTR_TX));

    switch (ep_action) {
        case STR91X_EPnR_CTR_RX_CLR:
             DEF_BIT_CLR(reg_val, STR91X_EPnR_CTR_RX);
             break;


        case STR91X_EPnR_DTOG_RX_CLR:
             if (DEF_BIT_IS_SET(reg_val, STR91X_EPnR_DTOG_RX) == DEF_TRUE) {
                 DEF_BIT_SET(reg_val,  STR91X_EPnR_DTOG_RX);
                 DEF_BIT_SET(reg_mask, STR91X_EPnR_DTOG_RX);
             }
             break;


        case STR91X_EPnR_DTOG_RX_SET:
             if (DEF_BIT_IS_CLR(reg_val, STR91X_EPnR_DTOG_RX) == DEF_TRUE) {
                 DEF_BIT_SET(reg_val,  STR91X_EPnR_DTOG_RX);
                 DEF_BIT_SET(reg_mask, STR91X_EPnR_DTOG_RX);
             }
             break;


        case STR91X_EPnR_DTOG_RX_TOGGLE:
             DEF_BIT_SET(reg_val, STR91X_EPnR_DTOG_RX);
             break;


        case STR91X_EPnR_STAT_RX_SET:
             ep_reg_val ^= (reg_val & STR91X_EPnR_STAT_RX_MASK);
             DEF_BIT_CLR(reg_val,  STR91X_EPnR_STAT_RX_MASK);
             DEF_BIT_SET(reg_val,  ep_reg_val);
             DEF_BIT_SET(reg_mask, STR91X_EPnR_STAT_RX_MASK);
             break;


        case STR91X_EPnR_EP_KIND_SET:
        case STR91X_EPnR_EP_TYPE_SET:
             DEF_BIT_SET(reg_val, ep_reg_val);
             break;


        case STR91X_EPnR_EP_KIND_CLR:
             DEF_BIT_CLR(reg_val, ep_reg_val);
             break;


        case STR91X_EPnR_CTR_TX_CLR:
             DEF_BIT_CLR(reg_val, STR91X_EPnR_CTR_TX);
             break;


        case STR91X_EPnR_DTOG_TX_CLR:
             if (DEF_BIT_IS_SET(reg_val, STR91X_EPnR_DTOG_TX) == DEF_TRUE) {
                 DEF_BIT_SET(reg_val,  STR91X_EPnR_DTOG_TX);
                 DEF_BIT_SET(reg_mask, STR91X_EPnR_DTOG_TX);
             }
             break;


        case STR91X_EPnR_DTOG_TX_SET:
             if (DEF_BIT_IS_CLR(reg_val, STR91X_EPnR_DTOG_TX) == DEF_TRUE) {
                 DEF_BIT_SET(reg_val,  STR91X_EPnR_DTOG_TX);
                 DEF_BIT_SET(reg_mask, STR91X_EPnR_DTOG_TX);
             }
             break;


        case STR91X_EPnR_DTOG_TX_TOGGLE:
             DEF_BIT_SET(reg_val,  STR91X_EPnR_DTOG_TX);
             DEF_BIT_SET(reg_mask, STR91X_EPnR_DTOG_TX);
             break;


        case STR91X_EPnR_STAT_TX_SET:
             ep_reg_val ^= (reg_val & STR91X_EPnR_STAT_TX_MASK);
             DEF_BIT_CLR(reg_val,  STR91X_EPnR_STAT_TX_MASK);
             DEF_BIT_SET(reg_val,  ep_reg_val);
             DEF_BIT_SET(reg_mask, STR91X_EPnR_STAT_TX_MASK);
             break;


        default:
             break;
    }

    p_reg->USB_EPnR[ep_nbr] = (reg_val & reg_mask);
}


/*
**************************************************************************************************************
*                                      STR91X_ADDRn_SetReg()
*
* Description : Set a value in the ADDRn register.
*
* Argument(s) : p_reg           Pointer to register structure.
*
*               ep_nbr          The endpoint number.
*
*               ep_dir          Direction of the Endpoint:
*                               STR91X_EP_DIR_IN
*                               STR91X_EP_DIR_OUT
*
*               ep_reg_val      Value to be set.
*
* Return(s)   : none.
*
* Note(s)     : none.
**************************************************************************************************************
*/

static  void  STR91X_ADDRn_SetReg (USBD_STR91X_REG  *p_reg,
                                   CPU_INT08U        ep_nbr,
                                   CPU_BOOLEAN       ep_dir,
                                   CPU_INT32U        ep_reg_val)
{
    CPU_INT32U  reg_val;


    reg_val =  p_reg->USB_BUF_DESC_TBL[ep_nbr].USB_ADDRn;

    if (ep_dir == STR91X_EP_DIR_OUT) {
        ep_reg_val = ((ep_reg_val << 16u) & STR91X_ADDRn_ADDR_RX_MASK);
        DEF_BIT_CLR(reg_val, STR91X_ADDRn_ADDR_RX_MASK);
    } else {
        ep_reg_val &= STR91X_ADDRn_ADDR_TX_MASK;
        DEF_BIT_CLR(reg_val, STR91X_ADDRn_ADDR_TX_MASK);
    }

    DEF_BIT_SET(reg_val, ep_reg_val);

    p_reg->USB_BUF_DESC_TBL[ep_nbr].USB_ADDRn = reg_val;
}


/*
**************************************************************************************************************
*                                      STR91X_COUNTn_SetReg()
*
* Description : Set a value in the COUNTn register.
*
* Argument(s) : p_reg           Pointer to register structure.
*
*               ep_nbr          The endpoint number.
*
*               ep_dir          Direction of the Endpoint:
*                               STR91X_EP_DIR_IN
*                               STR91X_EP_DIR_OUT
*
*               ep_reg_val      Value to be set.
*
* Return(s)   : none.
*
* Note(s)     : none.
**************************************************************************************************************
*/

static  void  STR91X_COUNTn_SetReg (USBD_STR91X_REG  *p_reg,
                                    CPU_INT08U        ep_nbr,
                                    CPU_BOOLEAN       ep_dir,
                                    CPU_INT32U        ep_reg_val)
{
    CPU_INT32U  reg_val;


    reg_val =  p_reg->USB_BUF_DESC_TBL[ep_nbr].USB_COUNTn;

    if (ep_dir == STR91X_EP_DIR_OUT) {
        ep_reg_val = ((ep_reg_val << 16u) & STR91X_COUNTn_COUNT_RX_MASK);
        DEF_BIT_CLR(reg_val, STR91X_COUNTn_COUNT_RX_MASK);
    } else {
        ep_reg_val &= STR91X_COUNTn_COUNT_TX_MASK;
        DEF_BIT_CLR(reg_val, STR91X_COUNTn_COUNT_TX_MASK);
    }

    DEF_BIT_SET(reg_val, ep_reg_val);
    p_reg->USB_BUF_DESC_TBL[ep_nbr].USB_COUNTn = reg_val;
}


/*
**************************************************************************************************************
*                                      STR91X_TxBufWr()
*
* Description : Writes to the TX Packet buffer memory.
*
* Argument(s) : p_dest          Pointer to destination memory buffer.
*
*               p_src           Pointer to source memory buffer
*
*               len             Number of octets to write.
*
* Return(s)   : none.
*
* Note(s)     : none.
**************************************************************************************************************
*/

static  void  STR91X_TxBufWr (CPU_INT32U  *p_dest,
                              CPU_INT08U  *p_src,
                              CPU_INT32U   len)
{
    CPU_INT32U  i;
    CPU_INT32U  j;

    CPU_INT32U  nbr_dwords;
    CPU_INT32U  nbr_bytes;


    nbr_dwords = (len / CPU_WORD_SIZE_32);
    nbr_bytes  = (len % CPU_WORD_SIZE_32);
    j          =  0u;
    i          =  0u;

    while (i < nbr_dwords) {
         p_dest[i] = (CPU_INT32U)((p_src[j + 0u] <<  0u) |
                                  (p_src[j + 1u] <<  8u) |
                                  (p_src[j + 2u] << 16u) |
                                  (p_src[j + 3u] << 24u));

        j += CPU_WORD_SIZE_32;
        i++;
    }

    switch (nbr_bytes) {
        case 1u:
             p_dest[i] = (CPU_INT32U)(p_src[j] <<  0u);
             break;


        case 2u:
             p_dest[i] = (CPU_INT32U)((p_src[j]      <<  0u) |
                                      (p_src[j + 1u] <<  8u));
             break;


        case 3u:
             p_dest[i] = (CPU_INT32U)((p_src[j + 0u] <<  0u) |
                                      (p_src[j + 1u] <<  8u) |
                                      (p_src[j + 2u] << 16u));
             break;


        case 0u:
        default:
             break;
    }
}
