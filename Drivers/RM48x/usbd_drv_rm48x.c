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
*                                                RM48X
*
* Filename : usbd_drv_rm48x.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)  : (1) You can find specific information about this driver at:
*                https://doc.micrium.com/display/USBDDRV/RM48x
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#include  "../../Source/usbd_core.h"
#include  "usbd_drv_rm48x.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  USBD_DRV_SETUP_PKT_CIRCULAR_BUF_SIZE        3u
#define  RM48X_EP_MAX_NBR                           15u


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
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                        REGISTER BIT DEFINES
*********************************************************************************************************
*/
                                                                /* --------------- USB DEVICE INTERRUPTS -------------- */
#define  RM48X_IRQ_EN_SOF              DEF_BIT_07               /* Start of frame interrupt enable.                     */
#define  RM48X_IRQ_EN_EPN_RX           DEF_BIT_05               /* Receive endpoint n interrupt enable(non-ISO).        */
#define  RM48X_IRQ_EN_EPN_TX           DEF_BIT_04               /* Transmit enpoint n interrupt enable(non-ISO).        */
#define  RM48X_IRQ_EN_DS_CHG           DEF_BIT_03               /* Device state changed interrupt enable.               */
#define  RM48X_IRQ_EN_EP0              DEF_BIT_00               /* EP0 transactions interrupt enable.                   */

#define  RM48X_IRQ_EN_ALL             (RM48X_IRQ_EN_SOF     |    \
                                       RM48X_IRQ_EN_EPN_RX  |    \
                                       RM48X_IRQ_EN_EPN_TX  |    \
                                       RM48X_IRQ_EN_DS_CHG  |    \
                                       RM48X_IRQ_EN_EP0)

                                                                /* --------- INTERRUPT SOURCE REGISTER DEFINES -------- */
#define  RM48X_IRQ_SRC_EPN_RX          DEF_BIT_05               /* EPN OUT interrupt flag bit.                          */
#define  RM48X_IRQ_SRC_EPN_TX          DEF_BIT_04               /* EPN IN interrupt flag bit.                           */
#define  RM48X_IRQ_SRC_DS_CHG          DEF_BIT_03               /* Device state change interrupt flag bit.              */
#define  RM48X_IRQ_SRC_SETUP           DEF_BIT_02               /* Setup interrupt flag bit.                            */
#define  RM48X_IRQ_SRC_EP0_RX          DEF_BIT_01               /* EP0 OUT interrupt flag bit.                          */
#define  RM48X_IRQ_SRC_EP0_TX          DEF_BIT_00               /* EP0 IN interrupt flag bit.                           */

#define  RM48X_IRQ_SRC_ALL             DEF_BIT_FIELD(16u, 0u)

                                                                /* ------------ ENDPOINT REGISTER DEFINES ------------- */
#define  RM48X_EP_SIZE_08BYTE          DEF_BIT_NONE
#define  RM48X_EP_SIZE_16BYTE          DEF_BIT_MASK(1u, 12u)
#define  RM48X_EP_SIZE_32BYTE          DEF_BIT_MASK(2u, 12u)
#define  RM48X_EP_SIZE_64BYTE          DEF_BIT_MASK(3u, 12u)
#define  RM48X_EP_VALID                DEF_BIT_15

                                                                /* ------------ SYS CFG1 REGISTER DEFINES ------------- */
#define  RM48X_SYSCON1_CFG_LOCK        DEF_BIT_08               /* Device configuration locked bit.                     */
#define  RM48X_SYSCON1_DATA_ENDIAN     DEF_BIT_07               /* Select little- or big-endian format on data access.  */
#define  RM48X_SYSCON1_NAK_EN          DEF_BIT_04               /* NAK enable bit.                                      */
#define  RM48X_SYSCON1_AUTODEC_DIS     DEF_BIT_03               /* Autodecode process disable bit.                      */
#define  RM48X_SYSCON1_SELF_PWR        DEF_BIT_02               /* Self-powered bit.                                    */
#define  RM48X_SYSCON1_SOFF_DIS        DEF_BIT_01               /* Shutoff disable bit.                                 */
#define  RM48X_SYSCON1_PULLUP_EN       DEF_BIT_00               /* External pullup allows device to disconnect itself.  */

                                                                /* ------------ SYS CFG2 REGISTER DEFINES ------------- */
#define  RM48X_SYSCON2_STALL_CMD       DEF_BIT_05               /* Stall command for EP0 bit.                           */
#define  RM48X_SYSCON2_DEV_CFG         DEF_BIT_03               /* Device configured bit.                               */
#define  RM48X_SYSCON2_CLR_CFG         DEF_BIT_02               /* Device deconfigured bit.                             */

                                                                /* --------- ENDPOINT CONTROL REGISTER DEFINES -------- */
#define  RM48X_CTRL_CLR_HALT           DEF_BIT_07               /* Clear halt bit.                                      */
#define  RM48X_CTRL_SET_HALT           DEF_BIT_06               /* Set halt bit.                                        */
#define  RM48X_CTRL_SET_FIFO_EN        DEF_BIT_02               /* FIFO enable bit.                                     */
#define  RM48X_CTRL_CLR_EP             DEF_BIT_01               /* Clear endpoint bit.                                  */
#define  RM48X_CTRL_RESET_EP           DEF_BIT_00               /* Reset endpoint bit.                                  */

                                                                /* --------- ENDPOINT CONTROL REGISTER DEFINES -------- */
#define  RM48X_EP_NUM_SETUP_SEL        DEF_BIT_06               /* EP0 setup access status bit.                         */
#define  RM48X_EP_NUM_EP_SEL           DEF_BIT_05               /* EP access status bit.                                */
#define  RM48X_EP_NUM_EP_DIR           DEF_BIT_04               /* IN EP direction bit.                                 */
#define  RM48X_EP_NUM_MASK             DEF_BIT_FIELD(4u, 0u)

                                                                /* ----------- DEVICE STATUS REGISTER DEFINES --------- */
#define  RM48X_DEVSTAT_USB_RESET       DEF_BIT_05               /* Reset state bit.                                     */
#define  RM48X_DEVSTAT_SUS             DEF_BIT_04               /* Suspend state bit.                                   */
#define  RM48X_DEVSTAT_ADD             DEF_BIT_02               /* Addressed state bit.                                 */
#define  RM48X_DEVSTAT_ATT             DEF_BIT_00               /* Attached state bit.                                  */
#define  RM48X_DEVSTAT_ALL            (RM48X_DEVSTAT_USB_RESET  |    \
                                       RM48X_DEVSTAT_SUS        |    \
                                       RM48X_DEVSTAT_ADD        |    \
                                       RM48X_DEVSTAT_ATT)

                                                                /* -------- RECEIVE FIFO STATUS REGISTER DEFINES ------ */
#define  RM48X_RXF_COUNT               DEF_BIT_FIELD(10u, 0u)


/*
*********************************************************************************************************
*                                    SETUP PACKETS QUEUE DATA TYPES
*********************************************************************************************************
*/

typedef struct  usbd_drv_setup_pkt_q {
                                                                /* Tbl containing the q'd setup pkts.                   */
                                                                /* Setup req buf: |Request|Value|Index|Length|          */
    CPU_INT16U  SetupPktTbl[USBD_DRV_SETUP_PKT_CIRCULAR_BUF_SIZE][4u];
    CPU_INT08U  IxIn;                                           /* Ix where to put the next rxd setup pkt.              */
    CPU_INT08U  IxOut;                                          /* Ix where to get the next setup pkt to give to core.  */
    CPU_INT08U  NbrSetupPkt;                                    /* Actual nbr of pkts in the buf.                       */
} USBD_DRV_SETUP_PKT_Q;


/*
*********************************************************************************************************
*                                      RM48X REGISTERS DATA TYPE
*********************************************************************************************************
*/

typedef  struct  usbd_rm48x_reg {
                                                                /* ----------------- REVISION REGISTER ---------------- */
    CPU_REG16  REV;                                             /* Revision.                                            */

                                                                /* ---------------- ENDPOINT REGISTERS ---------------- */
    CPU_REG16  EP_NUM;                                          /* Endpoint selection.                                  */
    CPU_REG16  DATA;                                            /* Data.                                                */
    CPU_REG16  CTRL;                                            /* Control.                                             */
    CPU_REG16  STAT_FLG;                                        /* Status.                                              */
    CPU_REG16  RXFSTAT;                                         /* Receive FIFO status.                                 */
    CPU_REG16  SYSCON1;                                         /* System configuration 1.                              */
    CPU_REG16  SYSCON2;                                         /* System configuration 2.                              */
    CPU_REG16  DEVSTAT;                                         /* Device status.                                       */
    CPU_REG16  SOF;                                             /* Start of frame.                                      */
    CPU_REG16  IRQ_EN;                                          /* Interrupt enable.                                    */
    CPU_REG16  DMA_IRQ_EN;                                      /* DMA interrupt enable.                                */
    CPU_REG16  IRQ_SRC;                                         /* Interrupt source.                                    */
    CPU_REG16  EPN_STAT;                                        /* Non-ISO endpoint interrupt enable.                   */
    CPU_REG16  DMAN_STAT;                                       /* Non-ISO DMA interrupt enable.                        */
    CPU_REG16  RSVD0;

                                                                /* ----------- DMA CONFIGURATION REGISTERS ------------ */
    CPU_REG16  RXDMA_CFG;                                       /* DMA receive channels configuration.                  */
    CPU_REG16  TXDMA_CFG;                                       /* DMA transmit channels configuration.                 */
    CPU_REG16  DATA_DMA;                                        /* DMA FIFO data.                                       */
    CPU_REG16  TXDMA0;                                          /* Transmit DMA control 0.                              */
    CPU_REG16  TXDMA1;                                          /* Transmit DMA control 1.                              */
    CPU_REG16  TXDMA2;                                          /* Transmit DMA control 2.                              */
    CPU_REG16  RSVD1[2u];
    CPU_REG16  RXDMA0;                                          /* Receive DMA control 0.                               */
    CPU_REG16  RXDMA1;                                          /* Receive DMA control 1.                               */
    CPU_REG16  RXDMA2;                                          /* Receive DMA control 2.                               */
    CPU_REG16  RSVD2[5u];
                                                                /* --------- ENDPOINT CONFIGURATION REGISTERS --------- */
    CPU_REG16  EP0;                                             /* Endpoint configuration 0.                            */
    CPU_REG16  EP_RX[RM48X_EP_MAX_NBR];                         /* Receive endpoint configuration 1.                    */
    CPU_REG16  RSVD3;
    CPU_REG16  EP_TX[RM48X_EP_MAX_NBR];                         /* Transmit endpoint configuration 1.                   */
}  USBD_RM48X_REG;


/*
*********************************************************************************************************
*                                          DRIVER DATA TYPE
*********************************************************************************************************
*/

typedef  struct  usbd_drv_data {                                /* ---------- COMMON FIFO/DMA DATA STRUCTURE ---------- */
    CPU_INT16U            DevStatChg;                           /* Device state change.                                 */
    CPU_BOOLEAN           ZLP_TxDis;                            /* Indicate a ZLP should not be sent.                   */
    USBD_DRV_SETUP_PKT_Q  SetupPktQ;                            /* Setup pkt queuing buf.                               */
    CPU_INT16U            EP_MaxPktSize[RM48X_EP_MAX_NBR * 2u];
} USBD_DRV_DATA;


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

static  void         USBD_RM48X_DrvSetupPktClr       (USBD_DRV_DATA   *p_drv_data);

static  void         USBD_RM48X_DrvSetupPktEnq       (USBD_DRV_DATA   *p_drv_data,
                                                      USBD_RM48X_REG  *p_reg);

static  void         USBD_RM48X_DrvSetupPktEnqSetAddr(USBD_DRV_DATA   *p_drv_data);

static  CPU_INT16U  *USBD_RM48X_DrvSetupPktGet       (USBD_DRV_DATA   *p_drv_data);

static  void         USBD_RM48X_DrvSetupPktDeq       (USBD_DRV_DATA   *p_drv_data);


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

USBD_DRV_API  USBD_DrvAPI_RM48X = { USBD_DrvInit,
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
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  USBD_DrvInit (USBD_DRV  *p_drv,
                            USBD_ERR  *p_err)
{
    USBD_DRV_BSP_API  *p_bsp_api;
    USBD_DRV_DATA     *p_drv_data;
    CPU_SIZE_T         reqd_octets;
    LIB_ERR            err_lib;

                                                                /* Alloc drv's internal data.                           */
    p_drv_data = (USBD_DRV_DATA *)Mem_HeapAlloc(sizeof(USBD_DRV_DATA),
                                                sizeof(CPU_ALIGN),
                                               &reqd_octets,
                                               &err_lib);
    if (p_drv_data == (void *)0) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

    Mem_Clr(p_drv_data, sizeof(USBD_DRV_DATA));

    p_drv_data->DevStatChg = 0u;                                /* Initialize global device state change variable.      */
    p_drv_data->ZLP_TxDis  = DEF_FALSE;                         /* Initialize internal data variable.                   */

    USBD_RM48X_DrvSetupPktClr(p_drv_data);
    Mem_Clr((void *)&(p_drv_data->EP_MaxPktSize[0u]),           /* Clr EP max pkt size info tbl.                        */
               sizeof(p_drv_data->EP_MaxPktSize));

    p_drv->DataPtr = p_drv_data;                                /* Store drv internal data ptr.                         */

    p_bsp_api = p_drv->BSP_API_Ptr;                             /* Get driver BSP API reference.                        */

    if (p_bsp_api->Init != (void *)0) {
        p_bsp_api->Init(p_drv);                                 /* Call board/chip specific dev ctrlr init fnct.        */
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
* Note(s)     : Typically, the start function activates the pull-down on the D- pin to simulate
*               attachment to host. Some MCUs/MPUs have an internal pull-down that is activated by a
*               device controller register; for others, this may be a GPIO pin. Additionally, interrupts
*               for reset and suspend are activated.
*********************************************************************************************************
*/

static  void  USBD_DrvStart (USBD_DRV  *p_drv,
                             USBD_ERR  *p_err)
{
    USBD_RM48X_REG    *p_reg;
    USBD_DRV_BSP_API  *p_bsp_api;


    p_reg     = (USBD_RM48X_REG *)p_drv->CfgPtr->BaseAddr;      /* Get USB ctrl reg ref.                                */
    p_bsp_api =  p_drv->BSP_API_Ptr;                            /* Get driver BSP API reference.                        */

    DEF_BIT_CLR(p_reg->SYSCON1, RM48X_SYSCON1_CFG_LOCK);        /* Disable configuration lock.                          */
    DEF_BIT_CLR(p_reg->IRQ_EN , RM48X_IRQ_EN_ALL);              /* Disable all device interrupts.                       */

    p_reg->IRQ_SRC = RM48X_IRQ_SRC_ALL;                         /* Clear all pending interrupts.                        */

    if (DEF_BIT_IS_SET(p_reg->SYSCON1, RM48X_SYSCON1_PULLUP_EN) == DEF_YES) {
        DEF_BIT_CLR(p_reg->SYSCON1, RM48X_SYSCON1_PULLUP_EN);   /* Disable Pull-up.                                     */
    }

    DEF_BIT_SET(p_reg->SYSCON1, RM48X_SYSCON1_PULLUP_EN);       /* Enable Pull-up.                                      */

    if (p_bsp_api->Conn != (void *)0) {
        p_bsp_api->Conn();                                      /* Call board/chip specific connect function.           */
    }

    DEF_BIT_SET(p_reg->IRQ_EN, RM48X_IRQ_EN_DS_CHG);            /* Enable device state changed interrupts.              */

    DEF_BIT_SET(p_reg->SYSCON1,(RM48X_SYSCON1_AUTODEC_DIS |     /* Allow software to handle all EP0 transactions.       */
                                RM48X_SYSCON1_SELF_PWR    |     /* Set device as self powered.                          */
                                RM48X_SYSCON1_SOFF_DIS));       /* Power shutoff circuitry disabled.                    */

    p_reg->SYSCON1 |= RM48X_SYSCON1_CFG_LOCK;                   /* Enable configuration lock                            */

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
    USBD_RM48X_REG    *p_reg;
    USBD_DRV_BSP_API  *p_bsp_api;


    p_reg     = (USBD_RM48X_REG *)p_drv->CfgPtr->BaseAddr;      /* Get USB ctrl reg ref.                                */
    p_bsp_api =  p_drv->BSP_API_Ptr;                            /* Get driver BSP API reference.                        */

    if (p_bsp_api->Disconn != (void *)0) {
        p_bsp_api->Disconn();                                   /* Disconnect USB at board/chip level.                  */
    }

    DEF_BIT_CLR(p_reg->SYSCON1, RM48X_SYSCON1_PULLUP_EN);       /* Disable Pull-up.                                     */

    DEF_BIT_CLR(p_reg->IRQ_EN, RM48X_IRQ_EN_ALL);               /* Disable all device interrupts.                       */

    p_reg->IRQ_SRC = RM48X_IRQ_SRC_ALL;                         /* Clear all pending interrupts.                        */
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
*
*               (3) The ZLP_TxDis variable is used to prevent the transmission of a zero-length packet
*                   when handling a SET_ADDRESS standard request since the USB module already sent one.
*                   ZLP_TxDis is set in AddrSet(), called before EP_TxZLP(), and cleared in AddrEn(),
*                   called after EP_TxZLP().
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_DrvAddrSet (USBD_DRV    *p_drv,
                                      CPU_INT08U   dev_addr)
{
    USBD_DRV_DATA  *p_drv_data;


   (void)dev_addr;

    p_drv_data            = (USBD_DRV_DATA *)p_drv->DataPtr;    /* Get ref to the drv's internal data.                  */
    p_drv_data->ZLP_TxDis =  DEF_TRUE;                          /* No ZLP to Tx for SET_ADDRESS                         */

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
*
*               (3) The ZLP_TxDis variable is used to prevent the transmission of a zero-length packet
*                   when handling a SET_ADDRESS standard request since the USB module already sent one.
*                   ZLP_TxDis is set in AddrSet(), called before EP_TxZLP(), and cleared in AddrEn(),
*                   called after EP_TxZLP().
*********************************************************************************************************
*/

static  void  USBD_DrvAddrEn (USBD_DRV    *p_drv,
                              CPU_INT08U   dev_addr)
{
    USBD_DRV_DATA  *p_drv_data;


   (void)dev_addr;

    p_drv_data            = (USBD_DRV_DATA *)p_drv->DataPtr;    /* Get ref to the drv's internal data.                  */
    p_drv_data->ZLP_TxDis =  DEF_FALSE;                         /* Re-en Tx ZLP                                         */
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
    USBD_RM48X_REG  *p_reg;


   (void)cfg_val;
                                                                /* Get USB ctrl reg ref.                                */
    p_reg           = (USBD_RM48X_REG *)p_drv->CfgPtr->BaseAddr;
    p_reg->SYSCON2 |=  RM48X_SYSCON2_DEV_CFG;                   /* Set the device configured bit                        */

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
    USBD_RM48X_REG  *p_reg;


   (void)cfg_val;
                                                                /* Get USB ctrl reg ref.                                */
    p_reg           = (USBD_RM48X_REG *)p_drv->CfgPtr->BaseAddr;
    p_reg->SYSCON2 |=  RM48X_SYSCON2_CLR_CFG;                   /* Clear the device configured bit                      */
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
*               (3) EPn_TX_PTR and EPn_RX_PTR are evenly partioned for enpoints 1 - 15 in the USB
*                   dedicated memory since ISO endpoint types are not available at this time. To use
*                   ISO endpoints, the memory partitioning will need to be modified.
*********************************************************************************************************
*/

static  void  USBD_DrvEP_Open (USBD_DRV    *p_drv,
                               CPU_INT08U   ep_addr,
                               CPU_INT08U   ep_type,
                               CPU_INT16U   max_pkt_size,
                               CPU_INT08U   transaction_frame,
                               USBD_ERR    *p_err)
{
    USBD_RM48X_REG    *p_reg;
    USBD_DRV_DATA     *p_drv_data;
    USBD_DRV_EP_INFO  *p_ep_tbl;
    CPU_BOOLEAN        ep_dir;
    CPU_INT08U         ep_log_nbr;
    CPU_INT08U         ep_phy_nbr;
    CPU_INT08U         ix;
    CPU_INT16U         reg_val;
    CPU_INT32U         ep_ptr_val;


    (void)transaction_frame;

    p_reg      = (USBD_RM48X_REG *)p_drv->CfgPtr->BaseAddr;     /* Get USB ctrl reg ref.                                */
    p_drv_data = (USBD_DRV_DATA  *)p_drv->DataPtr;              /* Get ref to the drv's internal data.                  */
    ep_dir     =  USBD_EP_IS_IN(ep_addr);                       /* Get EP direction.                                    */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical number.                               */
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);                 /* Get EP physical number.                              */
    p_ep_tbl   =  p_drv->CfgPtr->EP_InfoTbl;
    ep_ptr_val =  DEF_MAX(p_ep_tbl[0u].MaxPktSize,
                          p_ep_tbl[1u].MaxPktSize);
   *p_err      =  USBD_ERR_FAIL;

                                                                /* -------------- ENDPOINT CONFIGURATION -------------- */
    DEF_BIT_CLR(p_reg->SYSCON1, RM48X_SYSCON1_CFG_LOCK);        /* Clear register configuration lock.                   */

    if (ep_type != USBD_EP_TYPE_CTRL) {
        for (ix = 2u; ix <= ep_phy_nbr; ix++) {                 /* See note #3.                                         */
            ep_ptr_val += p_ep_tbl[ix].MaxPktSize;
        }
    }

    switch (max_pkt_size) {
        case 8u:
             reg_val = (RM48X_EP_SIZE_08BYTE | (ep_ptr_val / 8u));
             break;


        case 16u:
             reg_val = (RM48X_EP_SIZE_16BYTE | (ep_ptr_val / 8u));
             break;


        case 32u:
             reg_val = (RM48X_EP_SIZE_32BYTE | (ep_ptr_val / 8u));
             break;


        case 64u:
             reg_val = (RM48X_EP_SIZE_64BYTE | (ep_ptr_val / 8u));
             break;


        default:
              return;
    }

    switch (ep_type) {
        case USBD_EP_TYPE_CTRL:
             p_reg->EP0 = reg_val;
             DEF_BIT_SET(p_reg->IRQ_EN, RM48X_IRQ_EN_EP0);      /* Enable EP0 transactions interrupt.                   */
             break;


        case USBD_EP_TYPE_BULK:
        case USBD_EP_TYPE_INTR:
             if (ep_dir == DEF_YES) {
                 p_reg->EP_TX[ep_log_nbr - 1u] |= (RM48X_EP_VALID | reg_val);
             } else {
                 p_reg->EP_RX[ep_log_nbr - 1u] |= (RM48X_EP_VALID | reg_val);
             }
             p_reg->EP_NUM = ((ep_dir << 4u) | ep_log_nbr);
             p_reg->CTRL   = RM48X_CTRL_RESET_EP;
             break;


        case USBD_EP_TYPE_ISOC:
        default:
             *p_err = USBD_ERR_EP_INVALID_TYPE;
              return;
    }

    p_drv_data->EP_MaxPktSize[ep_phy_nbr] = max_pkt_size;

    DEF_BIT_SET(p_reg->SYSCON1, RM48X_SYSCON1_CFG_LOCK);        /* Set reg cfg lock.                                    */

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
    USBD_RM48X_REG  *p_reg;
    USBD_DRV_DATA   *p_drv_data;
    CPU_BOOLEAN      ep_dir;
    CPU_INT08U       ep_log_nbr;
    CPU_INT08U       ep_phy_nbr;
    CPU_SR_ALLOC();


    p_reg      = (USBD_RM48X_REG *)p_drv->CfgPtr->BaseAddr;     /* Get USB ctrl reg ref.                                */
    p_drv_data = (USBD_DRV_DATA  *)p_drv->DataPtr;              /* Get ref to the drv's internal data.                  */
    ep_dir     =  USBD_EP_IS_IN(ep_addr);                       /* Get EP direction.                                    */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical  number.                              */
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);                 /* Get EP physical number.                              */

                                                                /* -------------- ENDPOINT CONFIGURATION -------------- */
    DEF_BIT_CLR(p_reg->SYSCON1, RM48X_SYSCON1_CFG_LOCK);        /* Clear register configuration lock.                   */

    if (ep_log_nbr == 0u) {
        p_reg->EP0 = DEF_CLR;
        DEF_BIT_CLR(p_reg->IRQ_EN, RM48X_IRQ_EN_EP0);           /* Dis EP0 transactions interrupt.                      */

    } else {
        if (ep_dir == DEF_YES) {
            p_reg->EP_TX[ep_log_nbr - 1u] = DEF_CLR;
        } else {
            p_reg->EP_RX[ep_log_nbr - 1u] = DEF_CLR;
        }

        CPU_CRITICAL_ENTER();
        p_reg->EP_NUM = ((ep_dir << 4u) | ep_log_nbr);
        p_reg->CTRL   = RM48X_CTRL_CLR_EP;                      /* Clear FIFO, EP flags, and enable bit.                */
        CPU_CRITICAL_EXIT();
    }

    p_drv_data->EP_MaxPktSize[ep_phy_nbr] = 0u;

    DEF_BIT_SET(p_reg->SYSCON1, RM48X_SYSCON1_CFG_LOCK);        /* Set register configuration lock.                     */
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
    USBD_DRV_DATA   *p_drv_data;
    USBD_RM48X_REG  *p_reg;
    CPU_INT08U       ep_log_nbr;
    CPU_INT08U       ep_phy_nbr;


    (void)p_buf;

    p_reg      = (USBD_RM48X_REG *)p_drv->CfgPtr->BaseAddr;     /* Get USB ctrl reg ref.                                */
    p_drv_data = (USBD_DRV_DATA  *)p_drv->DataPtr;              /* Get ref to the drv's internal data.                  */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical  number.                              */
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);                 /* Get EP physical number.                              */

    p_reg->EP_NUM = ep_log_nbr;                                 /* Specify EP to configure for receiving.               */
    p_reg->CTRL   = RM48X_CTRL_CLR_EP;                          /* Clear FIFO, EP flags, and enable bit.                */
    p_reg->CTRL   = RM48X_CTRL_SET_FIFO_EN;                     /* Re - enable the FIFO for receiving.                  */

    if (ep_log_nbr != 0u) {
        DEF_BIT_SET(p_reg->IRQ_EN, RM48X_IRQ_EN_EPN_RX);        /* Enable Receive EP n interrupt(non-ISO).              */
    }

   *p_err = USBD_ERR_NONE;

    return (DEF_MIN(p_drv_data->EP_MaxPktSize[ep_phy_nbr], buf_len));
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
    USBD_RM48X_REG  *p_reg;
    CPU_INT16U      *p_buf16;
    CPU_INT08U       ep_log_nbr;
    CPU_INT16U       nbr_half_words;
    CPU_INT16U       pkt_len;
    CPU_INT16U       bytes_rxd;
    CPU_INT16U       i;
    CPU_SR_ALLOC();


    p_reg      = (USBD_RM48X_REG *)p_drv->CfgPtr->BaseAddr;     /* Get USB ctrl reg ref.                                */
    p_buf16    = (CPU_INT16U     *)p_buf;
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical number.                               */

    CPU_CRITICAL_ENTER();

    p_reg->EP_NUM  = (RM48X_EP_NUM_EP_SEL | ep_log_nbr);        /* Specify EP to configure for receiving.               */
    pkt_len        = (p_reg->RXFSTAT & RM48X_RXF_COUNT);        /* Mask out number of bytes in FIFO value.              */
    bytes_rxd      =  DEF_MIN(pkt_len, buf_len);
    nbr_half_words = (bytes_rxd / 2u);                          /* Determine the number of half words in the transfer.  */

    for (i = 0u; i < nbr_half_words; i++) {                     /* Copy transfered data into buffer.                    */
       *p_buf16 = p_reg->DATA;
        p_buf16++;
    }

    if ((bytes_rxd % 2u) != 0u) {                               /* Read an extra byte.                                  */
        p_buf = (CPU_INT08U *) p_buf16;
       *p_buf = (CPU_INT08U  )(p_reg->DATA & 0xFF);
    }

    if (pkt_len > buf_len) {
       *p_err = USBD_ERR_RX;
    } else {
       *p_err = USBD_ERR_NONE;
    }

    p_reg->EP_NUM = ep_log_nbr;                                 /* Specify EP to configure for receiving.               */
    p_reg->CTRL   = 0u;

    CPU_CRITICAL_EXIT();

    return (bytes_rxd);
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
    USBD_RM48X_REG  *p_reg;
    USBD_DRV_DATA   *p_drv_data;
    CPU_INT16U      *p_setup_buf;
    CPU_INT08U       ep_log_nbr;
    CPU_SR_ALLOC();


    p_reg      = (USBD_RM48X_REG *)p_drv->CfgPtr->BaseAddr;     /* Get USB ctrl reg ref.                                */
    p_drv_data = (USBD_DRV_DATA  *)p_drv->DataPtr;              /* Get ref to the drv's internal data.                  */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical  number.                              */

    CPU_CRITICAL_ENTER();
    if (ep_log_nbr == 0u) {
        USBD_RM48X_DrvSetupPktDeq(p_drv_data);                  /* Deq current setup pkt just processed.                */
        if (p_drv_data->SetupPktQ.NbrSetupPkt != 0u) {
                                                                /* Get nxt setup pkt.                                   */
            p_setup_buf = USBD_RM48X_DrvSetupPktGet(p_drv_data);
            USBD_EventSetup(        p_drv,                      /* Post setup pkt to the core.                          */
                            (void *)p_setup_buf);
        }
    }
    p_reg->EP_NUM = (RM48X_EP_NUM_EP_SEL | ep_log_nbr);         /* Specify EP to configure for receiving.               */
    p_reg->CTRL   =  RM48X_CTRL_CLR_EP;                         /* Clear FIFO, EP flags, and enable bit.                */
    p_reg->CTRL   =  RM48X_CTRL_SET_FIFO_EN;                    /* Re - enable the FIFO for receiving.                  */
    DEF_BIT_CLR(p_reg->EP_NUM, RM48X_EP_NUM_EP_SEL);            /* Clear FIFO access bit.                               */

    CPU_CRITICAL_EXIT();

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
    USBD_RM48X_REG  *p_reg;
    USBD_DRV_DATA   *p_drv_data;
    CPU_REG08       *reg_fifo08;
    CPU_INT08U       ep_log_nbr;
    CPU_INT08U       ep_phy_nbr;
    CPU_INT16U       nbr_half_words;
    CPU_INT16U       tx_len;
    CPU_INT16U       i;
    CPU_INT16U      *p_buf16;
    CPU_SR_ALLOC();


    p_reg          = (USBD_RM48X_REG *)p_drv->CfgPtr->BaseAddr; /* Get USB ctrl reg ref.                                */
    p_drv_data     = (USBD_DRV_DATA  *)p_drv->DataPtr;          /* Get ref to the drv's internal data.                  */
    p_buf16        = (CPU_INT16U     *)p_buf;
    ep_log_nbr     =  USBD_EP_ADDR_TO_LOG(ep_addr);             /* Get EP logical  number.                              */
    ep_phy_nbr     =  USBD_EP_ADDR_TO_PHY(ep_addr);             /* Get EP physical number.                              */
    tx_len         =  DEF_MIN(p_drv_data->EP_MaxPktSize[ep_phy_nbr],
                              buf_len);
    nbr_half_words = (tx_len / 2u);                             /* Determine the number of half words in the transfer.  */

    CPU_CRITICAL_ENTER();

    p_reg->EP_NUM = (RM48X_EP_NUM_EP_SEL |                      /* Specify EP to use for transmitting.                  */
                     RM48X_EP_NUM_EP_DIR |
                     ep_log_nbr);

    p_reg->CTRL   =  RM48X_CTRL_CLR_EP;                         /* Clear endpoint FIFO                                  */

    for (i = 0u; i < nbr_half_words; i++) {                     /* Copy data to transfer into buffer.                   */
        p_reg->DATA = *p_buf16;
        p_buf16++;
    }

    if ((tx_len % 2u) != 0u) {                                  /* Write extra byte.                                    */
        p_buf      = (CPU_INT08U *) p_buf16;
        reg_fifo08 = (CPU_REG08  *)&p_reg->DATA;
       *reg_fifo08 = *p_buf;
    }

    p_reg->CTRL = RM48X_CTRL_SET_FIFO_EN;
    DEF_BIT_CLR(p_reg->EP_NUM, RM48X_EP_NUM_EP_SEL);            /* Clear FIFO access bit.                               */

    CPU_CRITICAL_EXIT();

   *p_err = USBD_ERR_NONE;

    return (tx_len);
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
    USBD_RM48X_REG  *p_reg;
    CPU_INT08U       ep_log_nbr;


    (void)p_buf;
    (void)buf_len;

    p_reg      = (USBD_RM48X_REG *)p_drv->CfgPtr->BaseAddr;     /* Get USB ctrl reg ref.                                */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical  number.                              */

    if (ep_log_nbr != 0u) {
        DEF_BIT_SET(p_reg->IRQ_EN, RM48X_IRQ_EN_EPN_TX);        /* Enable transmit EP n interrupt(non-ISO).             */
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
* Note(s)     : (1) The ZLP_TxDis variable is used to prevent the transmission of a zero-length packet
*                   when handling a SET_ADDRESS standard request since the USB module already sent one.
*                   ZLP_TxDis is set in USBD_DrvAddrSet(), called before USBD_EP_TxZLP(), and cleared
*                   in USBD_DrvAddrEn(), called after USBD_EP_TxZLP().
*********************************************************************************************************
*/

static  void  USBD_DrvEP_TxZLP (USBD_DRV    *p_drv,
                                CPU_INT08U   ep_addr,
                                USBD_ERR    *p_err)
{
    USBD_RM48X_REG  *p_reg;
    USBD_DRV_DATA   *p_drv_data;
    CPU_INT16U      *p_setup_buf;
    CPU_INT08U       ep_log_nbr;
    CPU_SR_ALLOC();


    p_reg      = (USBD_RM48X_REG *)p_drv->CfgPtr->BaseAddr;     /* Get USB ctrl reg ref.                                */
    p_drv_data = (USBD_DRV_DATA  *)p_drv->DataPtr;              /* Get ref to the drv's internal data.                  */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical  number.                              */

    if (ep_log_nbr == 0u) {
        CPU_CRITICAL_ENTER();
        USBD_RM48X_DrvSetupPktDeq(p_drv_data);                  /* Deq current setup pkt just processed.                */
        if (p_drv_data->SetupPktQ.NbrSetupPkt != 0u) {
                                                                /* Get nxt setup pkt.                                   */
            p_setup_buf = USBD_RM48X_DrvSetupPktGet(p_drv_data);
            USBD_EventSetup(        p_drv,                      /* Post setup pkt to the core.                          */
                            (void *)p_setup_buf);
        }
        CPU_CRITICAL_EXIT();
    }

    if (p_drv_data->ZLP_TxDis == DEF_FALSE) {
        CPU_CRITICAL_ENTER();
        p_reg->EP_NUM = (RM48X_EP_NUM_EP_SEL |                  /* Specify EP to use for transmitting.                  */
                         RM48X_EP_NUM_EP_DIR |
                         ep_log_nbr);

        p_reg->CTRL   =  RM48X_CTRL_CLR_EP;
        p_reg->CTRL   =  RM48X_CTRL_SET_FIFO_EN;
        DEF_BIT_CLR(p_reg->EP_NUM, RM48X_EP_NUM_EP_SEL);        /* Clear FIFO access bit.                               */
        CPU_CRITICAL_EXIT();
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
    USBD_RM48X_REG  *p_reg;
    CPU_INT08U       ep_log_nbr;
    CPU_BOOLEAN      ep_dir;
    CPU_SR_ALLOC();


    p_reg      = (USBD_RM48X_REG *)p_drv->CfgPtr->BaseAddr;     /* Get USB ctrl reg ref.                                */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical  number.                              */
    ep_dir     =  USBD_EP_IS_IN(ep_addr);                       /* Get EP direction.                                    */

    CPU_CRITICAL_ENTER();
    p_reg->EP_NUM  = ((ep_dir << 4) | ep_log_nbr);              /* Specify EP to use for transmiting.                   */
    p_reg->CTRL    = RM48X_CTRL_CLR_EP;                         /* Clear FIFO, EP flags, and enable bit.                */
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
    USBD_RM48X_REG  *p_reg;
    USBD_DRV_DATA   *p_drv_data;
    CPU_INT16U      *p_setup_buf;
    CPU_INT08U       ep_log_nbr;
    CPU_BOOLEAN      ep_dir;
    CPU_SR_ALLOC();


    p_reg      = (USBD_RM48X_REG *)p_drv->CfgPtr->BaseAddr;     /* Get USB ctrl reg ref.                                */
    p_drv_data = (USBD_DRV_DATA  *)p_drv->DataPtr;              /* Get ref to the drv's internal data.                  */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);                 /* Get EP logical  number.                              */
    ep_dir     =  USBD_EP_IS_IN(ep_addr);                       /* Get EP direction.                                    */

    CPU_CRITICAL_ENTER();
    p_reg->EP_NUM = ((ep_dir << 4u) | ep_log_nbr);              /* Specify EP to use for stalling.                      */

    if (state == DEF_SET) {
        if (ep_log_nbr > 0u) {
            p_reg->CTRL = RM48X_CTRL_SET_HALT;                  /* Set the halt bit to stall the endpoint.              */
        } else {
            if (ep_addr == 0x00u) {                             /* If EP is OUT and is EP0.                             */
                USBD_RM48X_DrvSetupPktDeq(p_drv_data);
            }

            p_reg->SYSCON2 = RM48X_SYSCON2_STALL_CMD;           /* Set specific stall command for EP0.                  */

            if ((ep_addr                           == 0x00u) &&
                (p_drv_data->SetupPktQ.NbrSetupPkt !=    0u)) {
                                                                /* Deq the next setup pkt and post it to the core.      */
                p_setup_buf = USBD_RM48X_DrvSetupPktGet(p_drv_data);
                USBD_EventSetup(        p_drv,                  /* Post setup pkt to the core.                          */
                                (void *)p_setup_buf);
            }
        }
    } else {
        p_reg->CTRL = RM48X_CTRL_RESET_EP;                      /* Forces data PID to DATA0                             */
        p_reg->CTRL = RM48X_CTRL_CLR_HALT;                      /* Clear the halt bit of stall endpoint.                */
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
* Note(s)     : (1) Interrupt bits 2 and 1 are reserved in IRQ_EN register, but are sources in the
*                   status register that can trigger the EP0 bit 0 interrupt enable.
*
*               (2) The RM48x USB module automatically handles any SET_ADDRESS standard request received.
*                   For the driver and the USB-Device stack to operate correctly together, it is required
*                   to :
*
*                   (a) Enqueue every setup packet to maintain sequentiality of the control transfers,
*                       since a zero-length packet is sent before the stack processes the SET_ADDRESS
*                       standard request.
*
*                   (b) Since the SET_ADDRESS standard request is handled only by the USB module, a
*                       "dummy" SET_ADDRESS standard request must be created and enqueued when a device
*                       state change to 'Addressed' is detected.
*********************************************************************************************************
*/

static  void  USBD_DrvISR_Handler (USBD_DRV  *p_drv)
{
    USBD_RM48X_REG  *p_reg;
    USBD_DRV_DATA   *p_drv_data;
    CPU_INT16U      *p_setup_buf;
    CPU_INT08U       ep_log_nbr_rx;
    CPU_INT08U       ep_log_nbr_tx;
    CPU_INT16U       int_status;
    CPU_INT16U       int_en;
    CPU_INT16U       devstat_new;
    CPU_INT16U       devstat_curr;


    p_reg      = (USBD_RM48X_REG *)p_drv->CfgPtr->BaseAddr;     /* Get USB ctrl reg ref.                                */
    p_drv_data = (USBD_DRV_DATA  *)p_drv->DataPtr;              /* Get ref to the drv's internal data.                  */

    int_status    =  p_reg->IRQ_SRC;
    int_en        = (p_reg->IRQ_EN        |                     /* See Note #1.                                         */
                     RM48X_IRQ_SRC_SETUP  |
                     RM48X_IRQ_SRC_EP0_RX);

    devstat_new   =  p_reg->DEVSTAT;
    devstat_curr  =  devstat_new;
    devstat_new  ^=  p_drv_data->DevStatChg;                    /* See which state changed since previous interrupt.    */

    ep_log_nbr_rx = (0xFu & (p_reg->EPN_STAT >> 8u));           /* See which EP caused a Rx or Tx interrupt.            */
    ep_log_nbr_tx = (0xFu &  p_reg->EPN_STAT);

    int_status   &= int_en;

                                                                /* ---------- DEVICE STATE CHANGE INTERRUPTS ---------- */
    if (DEF_BIT_IS_SET(int_status, RM48X_IRQ_SRC_DS_CHG) == DEF_YES) {
                                                                /* --------- DEVICE ATTACHED STATE INTERRUPT ---------- */
        if (DEF_BIT_IS_SET(devstat_new, RM48X_DEVSTAT_ATT) == DEF_YES) {
            if (DEF_BIT_IS_SET(devstat_curr, RM48X_DEVSTAT_ATT) == DEF_YES) {
                USBD_EventConn(p_drv);                          /* Notify connect    event.                             */
            } else {
                USBD_EventDisconn(p_drv);                       /* Notify disconnect event.                             */
            }
        }

                                                                /* ---------- USB RESET SIGNALING INTERRUPT ----------- */
        if (DEF_BIT_IS_SET(devstat_new, RM48X_DEVSTAT_USB_RESET) == DEF_YES) {
            if (DEF_BIT_IS_CLR(devstat_curr, RM48X_DEVSTAT_USB_RESET) == DEF_YES) {
                USBD_RM48X_DrvSetupPktClr(p_drv_data);          /* Free up any setup req buf in the q.                  */
                USBD_EventReset(p_drv);                         /* Notify bus reset event.                              */
            }
        }

                                                                /* ----------- USB SUSPEND STATE INTERRUPT ------------ */
        if (DEF_BIT_IS_SET(devstat_new, RM48X_DEVSTAT_SUS) == DEF_YES) {
            if (DEF_BIT_IS_SET(devstat_curr, RM48X_DEVSTAT_SUS) == DEF_YES) {
                USBD_EventSuspend(p_drv);                       /* Notify suspend event.                                */
            } else {
                USBD_EventResume(p_drv);                        /* Notify resume  event.                                */
            }
        }

                                                                /* -------------- ADDRESS STATE INTERRUPT ------------- */
        if (DEF_BIT_IS_SET(devstat_new, RM48X_DEVSTAT_ADD) == DEF_YES) {
            if (DEF_BIT_IS_SET(devstat_curr, RM48X_DEVSTAT_ADD) == DEF_YES) {
                USBD_RM48X_DrvSetupPktEnqSetAddr(p_drv_data);   /* Enq dummy SET_ADDRESS setup pkt (see Note #2b).      */

                if (p_drv_data->SetupPktQ.NbrSetupPkt == 1u) {  /* If no other pkt in buf, post event to the core.      */
                    p_setup_buf = USBD_RM48X_DrvSetupPktGet(p_drv_data);
                    USBD_EventSetup(        p_drv,
                                    (void *)p_setup_buf);
                }
            }
        }

        p_reg->IRQ_SRC         = RM48X_IRQ_SRC_DS_CHG;          /* Clear DS_CHG interrupts.                             */
        p_drv_data->DevStatChg = devstat_curr;                  /* Update global device state change value.             */
    }

                                                                /* --------- PROCESS OUT ENDPOINT 0 INTERRUPT --------- */
    if (DEF_BIT_IS_SET(int_status, RM48X_IRQ_SRC_EP0_RX) == DEF_YES) {
        p_reg->IRQ_SRC = RM48X_IRQ_SRC_EP0_RX;                  /* Clear EP0 Rx interrupt.                              */
        USBD_EP_RxCmpl(p_drv, 0u);                              /* Notify RX successful completion to the core.         */
    }

                                                                /* ---------- PROCESS IN ENDPOINT 0 INTERRUPT --------- */
    if (DEF_BIT_IS_SET(int_status, RM48X_IRQ_SRC_EP0_TX) == DEF_YES) {
        p_reg->IRQ_SRC = RM48X_IRQ_SRC_EP0_TX;                  /* Clear EP0 Tx interrupt.                              */
        USBD_EP_TxCmpl(p_drv, 0u);                              /* Notify TX successful completion to the core.         */
    }

                                                                /* --------- PROCESS OUT ENDPOINT N INTERRUPT --------- */
    if (DEF_BIT_IS_SET(int_status, RM48X_IRQ_SRC_EPN_RX) == DEF_YES) {
        p_reg->IRQ_SRC = RM48X_IRQ_SRC_EPN_RX;                  /* Clear EPn Rx interrupt.                              */
        USBD_EP_RxCmpl(p_drv, ep_log_nbr_rx);                   /* Notify RX successful completion to the core.         */
    }

                                                                /* ---------- PROCESS IN ENDPOINT N INTERRUPT --------- */
    if (DEF_BIT_IS_SET(int_status, RM48X_IRQ_SRC_EPN_TX) == DEF_YES) {
        p_reg->IRQ_SRC =  RM48X_IRQ_SRC_EPN_TX;                 /* Clear EPn Tx interrupt.                              */
        p_reg->EP_NUM  = (RM48X_EP_NUM_EP_DIR |                 /* Reset CTRL reg for next IN transfer of EP n.         */
                          ep_log_nbr_tx);
        p_reg->CTRL    =  0u;
        USBD_EP_TxCmpl(p_drv, ep_log_nbr_tx);                   /* Notify TX successful completion to the core.         */
    }

                                                                /* ------------- PROCESS SETUP INTERRUPT -------------- */
    if (DEF_BIT_IS_SET(int_status, RM48X_IRQ_SRC_SETUP) == DEF_YES) {
        p_reg->EP_NUM = RM48X_EP_NUM_SETUP_SEL;                 /* Select EP0 and allow access to EP0 setup FIFO.       */

        USBD_RM48X_DrvSetupPktEnq(p_drv_data,                   /* Setup pkt has been rxd, enq setup pkt (see Note #2a).*/
                                  p_reg);

        p_reg->EP_NUM = DEF_BIT_NONE;                           /* Clear setup fifo access bit.                         */
        p_reg->CTRL   = RM48X_CTRL_RESET_EP;

        if (p_drv_data->SetupPktQ.NbrSetupPkt == 1u) {          /* If no other pkt in buf, post event to the core.      */
            p_setup_buf = USBD_RM48X_DrvSetupPktGet(p_drv_data);
            USBD_EventSetup(        p_drv,
                            (void *)p_setup_buf);
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
*                                   USBD_RM48X_DrvSetupPktClr()
*
* Description : Clear the circular buffer used to queue the setup packet and reset the indexes.
*
* Argument(s) : p_drv_data  Pointer to device driver data structure.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_RM48X_DrvSetupPktClr (USBD_DRV_DATA  *p_drv_data)
{
    USBD_DRV_SETUP_PKT_Q  *p_setup_pkt_buf;


    p_setup_pkt_buf = &(p_drv_data->SetupPktQ);

    p_setup_pkt_buf->NbrSetupPkt = 0u;                          /* Clr internal data and ix.                            */
    p_setup_pkt_buf->IxIn        = 0u;
    p_setup_pkt_buf->IxOut       = 0u;

    Mem_Clr((void *)&(p_setup_pkt_buf->SetupPktTbl[0u]),        /* Clr every buf's content.                             */
               sizeof(p_setup_pkt_buf->SetupPktTbl));
}


/*
*********************************************************************************************************
*                                   USBD_RM48X_DrvSetupPktEnq()
*
* Description : Enqueue a setup packet in the circular buffer.
*
* Argument(s) : p_drv_data  Pointer to device driver data structure.
*
*               p_reg       Pointer to RM48X registers structure.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_RM48X_DrvSetupPktEnq (USBD_DRV_DATA   *p_drv_data,
                                         USBD_RM48X_REG  *p_reg)
{
    USBD_DRV_SETUP_PKT_Q  *p_setup_pkt_buf;
    CPU_INT16U            *dest_buf;


    p_setup_pkt_buf = &(p_drv_data->SetupPktQ);

    if (p_setup_pkt_buf->NbrSetupPkt >= USBD_DRV_SETUP_PKT_CIRCULAR_BUF_SIZE) {
        return;
    }

                                                                /* Get next avail buf from circular buf.                */
    dest_buf = &(p_setup_pkt_buf->SetupPktTbl[p_setup_pkt_buf->IxIn][0u]);

                                                                /* Store setup pkt in obtained buf.                     */
    dest_buf[0u] = p_reg->DATA;
    dest_buf[1u] = p_reg->DATA;
    dest_buf[2u] = p_reg->DATA;
    dest_buf[3u] = p_reg->DATA;

                                                                /* Adjust ix and internal data.                         */
    p_setup_pkt_buf->IxIn++;
    if (p_setup_pkt_buf->IxIn >= USBD_DRV_SETUP_PKT_CIRCULAR_BUF_SIZE) {
        p_setup_pkt_buf->IxIn  = 0u;
    }
    p_setup_pkt_buf->NbrSetupPkt++;
}


/*
*********************************************************************************************************
*                                USBD_RM48X_DrvSetupPktEnqSetAddr()
*
* Description : Enqueue a dummy SET_ADDRESS setup packet in the circular buffer.
*
* Argument(s) : p_drv_data  Pointer to device driver data structure.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_RM48X_DrvSetupPktEnqSetAddr (USBD_DRV_DATA  *p_drv_data)
{
    USBD_DRV_SETUP_PKT_Q  *p_setup_pkt_buf;
    CPU_INT16U            *dest_buf;


    p_setup_pkt_buf = &(p_drv_data->SetupPktQ);

    if (p_setup_pkt_buf->NbrSetupPkt >= USBD_DRV_SETUP_PKT_CIRCULAR_BUF_SIZE) {
        return;
    }
                                                                /* Get next avail buf from circular buf.                */
    dest_buf = &(p_setup_pkt_buf->SetupPktTbl[p_setup_pkt_buf->IxIn][0u]);

                                                                /* Emulate a SET_ADDRESS pkt and store it in buf.       */
    dest_buf[0u] = MEM_VAL_BIG_TO_HOST_16(USBD_REQ_SET_ADDRESS);
    dest_buf[1u] = MEM_VAL_BIG_TO_HOST_16(0x0100u);             /* Specify any addr that is not 0 in the MSB.           */
    dest_buf[2u] = 0x0000u;
    dest_buf[3u] = 0x0000u;

                                                                /* Adjust ix and internal data.                         */
    p_setup_pkt_buf->IxIn++;
    if (p_setup_pkt_buf->IxIn >= USBD_DRV_SETUP_PKT_CIRCULAR_BUF_SIZE) {
        p_setup_pkt_buf->IxIn  = 0u;
    }
    p_setup_pkt_buf->NbrSetupPkt++;
}


/*
*********************************************************************************************************
*                                   USBD_RM48X_DrvSetupPktGet()
*
* Description : Get a setup packet from the circular buffer.
*
* Argument(s) : p_drv_data  Pointer to device driver data structure.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function only gets the first setup packet from the buffer. The function
*                   USBD_RM48X_DrvSetupPktDeq() must be called after processing the setup packet
*                   obtained to remove the setup packet from the circular buffer.
*
*               (2) This function should be called from a CRITICAL_SECTION.
*********************************************************************************************************
*/

static  CPU_INT16U*  USBD_RM48X_DrvSetupPktGet (USBD_DRV_DATA  *p_drv_data)
{
    USBD_DRV_SETUP_PKT_Q  *p_setup_pkt_buf;
    CPU_INT16U            *actual_buf;


    p_setup_pkt_buf = &(p_drv_data->SetupPktQ);

    actual_buf      = &(p_setup_pkt_buf->SetupPktTbl[p_setup_pkt_buf->IxOut][0u]);

    return (actual_buf);
}


/*
*********************************************************************************************************
*                                   USBD_RM48X_DrvSetupPktDeq()
*
* Description : Dequeue a setup packet from the circular buffer.
*
* Argument(s) : p_drv_data  Pointer to device driver data structure.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function only removes the first setup packet from the buffer. The function
*                   USBD_RM48X_DrvSetupPktGet() must be called before this function to get a pointer
*                   on the first setup packet and process the setup packet before dequeuing it.
*
*               (2) This function should be called from a CRITICAL_SECTION.
*********************************************************************************************************
*/

static  void  USBD_RM48X_DrvSetupPktDeq (USBD_DRV_DATA  *p_drv_data)
{
    USBD_DRV_SETUP_PKT_Q  *p_setup_pkt_buf;


    p_setup_pkt_buf = &(p_drv_data->SetupPktQ);

    p_setup_pkt_buf->IxOut++;
    if (p_setup_pkt_buf->IxOut >= USBD_DRV_SETUP_PKT_CIRCULAR_BUF_SIZE) {
        p_setup_pkt_buf->IxOut  = 0u;
    }
    p_setup_pkt_buf->NbrSetupPkt--;
}
