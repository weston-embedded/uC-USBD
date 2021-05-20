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
*                                              STM32xxx
*                                        USB Full-speed Device
*
* Filename : usbd_drv_stm32xxx.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)  : (1) You can find specific information about this driver at:
*                https://doc.micrium.com/display/USBDDRV/STM32xxx
*
*            (2) With an appropriate BSP, this device driver will support the USB full-speed Device
*                Interface module on the following STMicroelectronics MCUs:
*
*                    STM32L151xxx
*                    STM32L152xxx
*                    STM32L162xxx
*                    STM32F103xxx
*                    STM32F102xxx
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../../Source/usbd_core.h"
#include  "usbd_drv_stm32xxx.h"


/*
*********************************************************************************************************
*                                            LOCAL MACROS
*********************************************************************************************************
*/

#define  USBD_DBG_DRV(msg)                           USBD_DBG_GENERIC((msg),                            \
                                                                       USBD_EP_NBR_NONE,                \
                                                                       USBD_IF_NBR_NONE)

#define  USBD_DBG_DRV_EP(msg, ep_addr)               USBD_DBG_GENERIC((msg),                            \
                                                                      (ep_addr),                        \
                                                                       USBD_IF_NBR_NONE)

#define  USBD_DBG_DRV_EP_ARG(msg, ep_addr, arg)      USBD_DBG_GENERIC_ARG((msg),                        \
                                                                          (ep_addr),                    \
                                                                           USBD_IF_NBR_NONE,            \
                                                                          (arg))


#define  STM32XXX_EP_TX_ADDR(ep_nbr)        ((CPU_INT32U *)(((p_reg->BTABLE) + (ep_nbr) * 8u     ) * 2u + STM32XXX_BASE_PMA))
#define  STM32XXX_EP_TX_CNT(ep_nbr)         ((CPU_INT32U *)(((p_reg->BTABLE) + (ep_nbr) * 8u + 2u) * 2u + STM32XXX_BASE_PMA))
#define  STM32XXX_EP_RX_ADDR(ep_nbr)        ((CPU_INT32U *)(((p_reg->BTABLE) + (ep_nbr) * 8u + 4u) * 2u + STM32XXX_BASE_PMA))
#define  STM32XXX_EP_RX_CNT(ep_nbr)         ((CPU_INT32U *)(((p_reg->BTABLE) + (ep_nbr) * 8u + 6u) * 2u + STM32XXX_BASE_PMA))


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  STM32XXX_BASE_PMA                     0x40006000L      /* USB_IP Packet Memory Area base address               */
#define  STM32XXX_NBR_EPS                               8u      /* Maximum number of endpoints                          */

/*
*********************************************************************************************************
*                                       USB RAM ADDRESS DEFINES
*********************************************************************************************************
*/

#define  STM32XXX_ENDPn_RXADDR(n)                   (0x040u + ((n) * 0x80u))
#define  STM32XXX_ENDPn_TXADDR(n)                   (0x080u + ((n) * 0x80u))


/*
*********************************************************************************************************
*                                     STM32 REGISTER BIT DEFINES
*********************************************************************************************************
*/
                                                                /* ----------------- ISTR BIT DEFINES ----------------- */
#define  STM32XXX_ISTR_CTR                DEF_BIT_15            /* Correct TRansfer (clear-only bit)                    */
#define  STM32XXX_ISTR_DOVR               DEF_BIT_14            /* DMA OVeR/underrun (clear-only bit)                   */
#define  STM32XXX_ISTR_ERR                DEF_BIT_13            /* ERRor (clear-only bit)                               */
#define  STM32XXX_ISTR_WKUP               DEF_BIT_12            /* WaKe UP (clear-only bit)                             */
#define  STM32XXX_ISTR_SUSP               DEF_BIT_11            /* SUSPend (clear-only bit)                             */
#define  STM32XXX_ISTR_RESET              DEF_BIT_10            /* RESET (clear-only bit)                               */
#define  STM32XXX_ISTR_SOF                DEF_BIT_09            /* Start Of Frame (clear-only bit)                      */
#define  STM32XXX_ISTR_ESOF               DEF_BIT_08            /* Expected Start Of Frame (clear-only bit)             */
#define  STM32XXX_ISTR_DIR                DEF_BIT_04            /* DIRection of transaction (read-only bit)             */
#define  STM32XXX_ISTR_EP_ID              DEF_BIT_FIELD(4u, 0u) /* EndPoint IDentifier (read-only bit)                  */

#define  STM32XXX_ISTR_CLR_CTR   (CPU_INT16U)(~STM32XXX_ISTR_CTR)     /* Clear Correct TRansfer bit                     */
#define  STM32XXX_ISTR_CLR_DOVR  (CPU_INT16U)(~STM32XXX_ISTR_DOVR)    /* Clear DMA OVeR/underrun bit                    */
#define  STM32XXX_ISTR_CLR_ERR   (CPU_INT16U)(~STM32XXX_ISTR_ERR)     /* Clear ERRor bit                                */
#define  STM32XXX_ISTR_CLR_WKUP  (CPU_INT16U)(~STM32XXX_ISTR_WKUP)    /* Clear WaKe UP bit                              */
#define  STM32XXX_ISTR_CLR_SUSP  (CPU_INT16U)(~STM32XXX_ISTR_SUSP)    /* Clear SUSPend bit                              */
#define  STM32XXX_ISTR_CLR_RESET (CPU_INT16U)(~STM32XXX_ISTR_RESET)   /* Clear RESET bit                                */
#define  STM32XXX_ISTR_CLR_SOF   (CPU_INT16U)(~STM32XXX_ISTR_SOF)     /* Clear Start Of Frame bit                       */
#define  STM32XXX_ISTR_CLR_ESOF  (CPU_INT16U)(~STM32XXX_ISTR_ESOF)    /* Clear Expected Start Of Frame bit              */

                                                                /* ----------------- CNTR BIT DEFINES ----------------- */
#define  STM32XXX_CNTR_CTRM               DEF_BIT_15            /* Correct TRansfer Mask                                */
#define  STM32XXX_CNTR_DOVRM              DEF_BIT_14            /* DMA OVeR/underrun Mask                               */
#define  STM32XXX_CNTR_ERRM               DEF_BIT_13            /* ERRor Mask                                           */
#define  STM32XXX_CNTR_WKUPM              DEF_BIT_12            /* WaKe UP Mask                                         */
#define  STM32XXX_CNTR_SUSPM              DEF_BIT_11            /* SUSPend Mask                                         */
#define  STM32XXX_CNTR_RESETM             DEF_BIT_10            /* RESET Mask                                           */
#define  STM32XXX_CNTR_SOFM               DEF_BIT_09            /* Start Of Frame Mask                                  */
#define  STM32XXX_CNTR_ESOFM              DEF_BIT_08            /* Expected Start Of Frame Mask                         */
#define  STM32XXX_CNTR_RESUME             DEF_BIT_04            /* RESUME request                                       */
#define  STM32XXX_CNTR_FSUSP              DEF_BIT_03            /* Force SUSPend                                        */
#define  STM32XXX_CNTR_LPMODE             DEF_BIT_02            /* Low-power MODE                                       */
#define  STM32XXX_CNTR_PDWN               DEF_BIT_01            /* Power DoWN                                           */
#define  STM32XXX_CNTR_FRES               DEF_BIT_00            /* Force USB RESet                                      */

                                                                /* ------------------ FMR BIT DEFINES ----------------- */
#define  STM32XXX_FNR_RXDP                DEF_BIT_15            /* status of D+ data line                               */
#define  STM32XXX_FNR_RXDM                DEF_BIT_14            /* status of D- data line                               */
#define  STM32XXX_FNR_LCK                 DEF_BIT_13            /* LoCKed                                               */
#define  STM32XXX_FNR_LSOF                DEF_BIT_FIELD(2u, 11u)/* Lost SOF                                             */
#define  STM32XXX_FNR_FN                  DEF_BIT_FIELD(11u, 0u)/* Frame Number                                         */

                                                                /* ----------------- DADDR BIT DEFINES ---------------- */
#define  STM32XXX_DADDR_EF                DEF_BIT_07            /* Enable Function.                                     */
#define  STM32XXX_DADDR_ADD               DEF_BIT_FIELD(7u, 0u) /* ADDress.                                             */
#define  STM32XXX_DADDR_ADD_DFLT          DEF_BIT_NONE          /* USB Device Default Address                           */

                                                                /* --------------- ENDPOINT BIT DEFINES --------------- */
#define  STM32XXX_EP_CTRRX                DEF_BIT_15            /* EndPoint Correct TRansfer RX                         */
#define  STM32XXX_EP_DTOGRX               DEF_BIT_14            /* EndPoint Data TOGGLE RX                              */
#define  STM32XXX_EP_RXSTAT               DEF_BIT_FIELD(2u, 12u)/* EndPoint RX STATus bit field                         */
#define  STM32XXX_EP_SETUP                DEF_BIT_11            /* EndPoint SETUP                                       */

#define  STM32XXX_EP_TYPE                 DEF_BIT_FIELD(2u, 9u) /* EndPoint TYPE                                        */
#define  STM32XXX_EP_KIND                 DEF_BIT_08            /* EndPoint KIND                                        */
#define  STM32XXX_EP_CTRTX                DEF_BIT_07            /* EndPoint Correct TRansfer TX                         */
#define  STM32XXX_EP_DTOGTX               DEF_BIT_06            /* EndPoint Data TOGGLE TX                              */
#define  STM32XXX_EP_TXSTAT               DEF_BIT_FIELD(2u, 4u) /* EndPoint TX STATus bit field                         */
#define  STM32XXX_EP_ADDR                 DEF_BIT_FIELD(4u, 0u) /* EndPoint ADDRess FIELD                               */

                                                                /* EndPoint REGister MASK (no toggle fields)            */
#define  STM32XXX_EP_MASK                (STM32XXX_EP_CTRRX |              \
                                          STM32XXX_EP_SETUP |              \
                                          STM32XXX_EP_TYPE  |              \
                                          STM32XXX_EP_KIND  |              \
                                          STM32XXX_EP_CTRTX |              \
                                          STM32XXX_EP_ADDR)

                                                                /* ------------ EP_TYPE[1:0] ENDPOINT TYPE ------------ */
#define  STM32XXX_EPTYPE_MASK             DEF_BIT_FIELD(2u, 9u) /* EndPoint TYPE Mask                                   */
#define  STM32XXX_EPTYPE_BULK             DEF_BIT_NONE          /* EndPoint BULK                                        */
#define  STM32XXX_EPTYPE_CONTROL          DEF_BIT_MASK(1u, 9u)  /* EndPoint CONTROL                                     */
#define  STM32XXX_EPTYPE_ISOCHRONOUS      DEF_BIT_MASK(2u, 9u)  /* EndPoint ISOCHRONOUS                                 */
#define  STM32XXX_EPTYPE_INTERRUPT        DEF_BIT_MASK(3u, 9u)  /* EndPoint INTERRUPT                                   */
#define  STM32XXX_EPTYPE_MASK_OFF      (~STM32XXX_EP_TYPE & STM32XXX_EP_MASK)

                                                                /* --------------- EP_KIND ENDPOINT KIND -------------- */
#define  STM32XXX_EPKIND_MASK_OFF      (~STM32XXX_EP_KIND & STM32XXX_EP_MASK)

                                                                /* -------- STAT_RX[1:0] STATUS FOR RX TRANSFER ------- */
#define  STM32XXX_EPRXSTAT_DIS            DEF_BIT_NONE          /* EndPoint RX DISabled                                 */
#define  STM32XXX_EPRXSTAT_STALL          DEF_BIT_MASK(1u, 12u) /* EndPoint RX STALLed                                  */
#define  STM32XXX_EPRXSTAT_NAK            DEF_BIT_MASK(2u, 12u) /* EndPoint RX NAKed                                    */
#define  STM32XXX_EPRXSTAT_VALID          DEF_BIT_MASK(3u, 12u) /* EndPoint RX VALID                                    */
#define  STM32XXX_EPRXSTAT_DTOG1          DEF_BIT_12            /* EndPoint RX Data TOGgle bit1                         */
#define  STM32XXX_EPRXSTAT_DTOG2          DEF_BIT_13            /* EndPoint RX Data TOGgle bit1                         */
#define  STM32XXX_EPRXSTAT_DTOGMASK      (STM32XXX_EP_RXSTAT |             \
                                          STM32XXX_EP_MASK)

                                                                /* -------- STAT_TX[1:0] STATUS FOR TX TRANSFER ------- */
#define  STM32XXX_EPTXSTAT_DIS            DEF_BIT_NONE          /* EndPoint TX DISabled                                 */
#define  STM32XXX_EPTXSTAT_STALL          DEF_BIT_MASK(1u, 4u)  /* EndPoint TX STALLed                                  */
#define  STM32XXX_EPTXSTAT_NAK            DEF_BIT_MASK(2u, 4u)  /* EndPoint TX NAKed                                    */
#define  STM32XXX_EPTXSTAT_VALID          DEF_BIT_MASK(3u, 4u)  /* EndPoint TX VALID                                    */
#define  STM32XXX_EPTXSTAT_DTOG1          DEF_BIT_04            /* EndPoint TX Data TOGgle bit1                         */
#define  STM32XXX_EPTXSTAT_DTOG2          DEF_BIT_05            /* EndPoint TX Data TOGgle bit2                         */
#define  STM32XXX_EPTXSTAT_DTOGMASK      (STM32XXX_EP_TXSTAT |             \
                                          STM32XXX_EP_MASK)

                                                                /* ------------- BUFFER DESCRIPTOR TABLE -------------- */
#define  STM32XXX_COUNTn_RX_BL_SIZE        DEF_BIT_15           /* Rx memory block size.                                */

                                                                /* --------- USB ENDPOINT n REGISTER ACTIONS ---------- */
#define  STM32XXX_EPnR_CTR_RX_CLR                        1u     /* Clear   CTR_RX   bit                                 */
#define  STM32XXX_EPnR_DTOG_RX_CLR                       2u     /* Clear   DTOG_RX  bit                                 */
#define  STM32XXX_EPnR_STAT_RX_SET                       3u     /* Set     STAT_RX  field                               */
#define  STM32XXX_EPnR_CTR_TX_CLR                        4u     /* Clear   CTR_RX   bit                                 */
#define  STM32XXX_EPnR_DTOG_TX_CLR                       5u     /* Clear   DTOG_TX  bit                                 */
#define  STM32XXX_EPnR_STAT_TX_SET                       6u     /* Set     STAT_TX  field                               */


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

typedef  struct  usbd_stm32_reg {
    CPU_REG32  EPxR[STM32XXX_NBR_EPS];                          /* Endpoint-Specific Registers.                         */
    CPU_REG32  RSVD0[8u];
    CPU_REG32  CNTR;                                            /* USB control register.                                */
    CPU_REG32  ISTR;                                            /* USB interrupt status register.                       */
    CPU_REG32  FNR;                                             /* USB frame number register.                           */
    CPU_REG32  DADDR;                                           /* USB device address.                                  */
    CPU_REG32  BTABLE;                                          /* Buffer table address.                                */
} USBD_STM32XXX_REG;


/*
*********************************************************************************************************
*                                          DRIVER DATA TYPE
*********************************************************************************************************
*/

typedef  struct  usbd_drv_data_ep {                             /* ---------- DEVICE ENDPOINT DATA STRUCTURE ---------- */
    CPU_INT16U  EP_MaxPktSize[STM32XXX_NBR_EPS * 2u];
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

static  void  STM32_EPnR_SetReg  (USBD_STM32XXX_REG  *p_reg,
                                  CPU_INT08U          ep_nbr,
                                  CPU_INT32U          ep_reg_val,
                                  CPU_INT08U          ep_action);

static  void  STM32_ProcessEP    (USBD_STM32XXX_REG  *p_reg,
                                  USBD_DRV           *p_drv);

static  void  STM32_Process_Setup(USBD_STM32XXX_REG  *p_reg,
                                  USBD_DRV           *p_drv,
                                  CPU_INT08U          ep_nbr);

static  void  STM32_Reset        (USBD_STM32XXX_REG  *p_reg);


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

USBD_DRV_API  USBD_DrvAPI_STM32XXX = { USBD_DrvInit,
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
    p_bsp_api      = p_drv->BSP_API_Ptr;                        /* Get driver BSP API reference.                        */

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
    USBD_STM32XXX_REG  *p_reg;
    USBD_DRV_BSP_API   *p_bsp_api;


    p_bsp_api =  p_drv->BSP_API_Ptr;                            /* Get driver BSP API reference.                        */
    p_reg     = (USBD_STM32XXX_REG *)p_drv->CfgPtr->BaseAddr;   /* Get ref to USB HW reg.                               */

    p_reg->CNTR = STM32XXX_CNTR_FRES;                           /* Force reset.                                         */
    p_reg->CNTR = 0x00000000u;                                  /* Clear force reset.                                   */
    p_reg->ISTR = 0x00000000u;                                  /* Clear all interrupts.                                */
    p_reg->CNTR = STM32XXX_CNTR_CTRM | STM32XXX_CNTR_RESETM;

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
    USBD_STM32XXX_REG  *p_reg;
    USBD_DRV_BSP_API   *p_bsp_api;


    p_bsp_api =  p_drv->BSP_API_Ptr;                            /* Get driver BSP API reference.                        */
    p_reg     = (USBD_STM32XXX_REG *)p_drv->CfgPtr->BaseAddr;   /* Get ref to USB HW reg.                               */

    if (p_bsp_api->Disconn != (void *)0) {
        p_bsp_api->Disconn();
    }

    p_reg->ISTR = 0x00000000u;                                  /* Clear all interrupts.                                */
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
    USBD_STM32XXX_REG  *p_reg;


    p_reg = (USBD_STM32XXX_REG *)p_drv->CfgPtr->BaseAddr;       /* Get ref to USB HW reg.                               */

    p_reg->DADDR = (dev_addr | STM32XXX_DADDR_EF);              /* Enable the USB Device & set the dftl USB dev Addr.   */
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
    USBD_STM32XXX_REG  *p_reg;
    CPU_INT16U          frame_nbr;


    p_reg     = (USBD_STM32XXX_REG *)p_drv->CfgPtr->BaseAddr;
    frame_nbr = (CPU_INT16U         )p_reg->FNR & STM32XXX_FNR_FN;

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
    USBD_STM32XXX_REG  *p_reg;
    USBD_DRV_DATA_EP   *p_drv_data;
    CPU_BOOLEAN         is_out;
    CPU_INT08U          ep_nbr;
    CPU_INT08U          ep_phy_nbr;

    (void)transaction_frame;

    p_reg      = (USBD_STM32XXX_REG *)p_drv->CfgPtr->BaseAddr;  /* Get ref to USB HW reg.                               */
    p_drv_data = (USBD_DRV_DATA_EP  *)(p_drv->DataPtr);
    ep_nbr     =  USBD_EP_ADDR_TO_LOG(ep_addr);
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    is_out     = ((ep_addr & 0x80u) == 0x80u) ? DEF_NO : DEF_YES;

    if (ep_nbr == 0u) {                                         /* -------------------- ENDPOINT 0 -------------------- */
        p_reg->EPxR[0u] = (((p_reg->EPxR[0u]) & STM32XXX_EPTYPE_MASK_OFF) | (STM32XXX_EPTYPE_CONTROL));

        STM32_EPnR_SetReg(p_reg,
                          ep_nbr,
                          STM32XXX_EPTXSTAT_NAK,
                          STM32XXX_EPnR_STAT_TX_SET);

       *STM32XXX_EP_RX_ADDR(0u) = STM32XXX_ENDPn_RXADDR(ep_nbr);
       *STM32XXX_EP_RX_CNT(0u)  = STM32XXX_COUNTn_RX_BL_SIZE + (((64u / 32u) - 1u) << 10);
       *STM32XXX_EP_TX_ADDR(0u) = STM32XXX_ENDPn_TXADDR(ep_nbr);

        p_reg->EPxR[ep_nbr] = ((p_reg->EPxR[ep_nbr]) & STM32XXX_EPKIND_MASK_OFF);

        STM32_EPnR_SetReg(p_reg,
                          ep_nbr,
                          STM32XXX_EPRXSTAT_VALID,
                          STM32XXX_EPnR_STAT_RX_SET);

    } else {                                                    /* ------------------ OTHER ENDPOINT ------------------ */
        switch (ep_type) {
            case USBD_EP_TYPE_CTRL:
                 p_reg->EPxR[ep_nbr] = (((p_reg->EPxR[ep_nbr]) & STM32XXX_EPTYPE_MASK_OFF) | (STM32XXX_EPTYPE_CONTROL));
                 break;

            case USBD_EP_TYPE_ISOC:
                 p_reg->EPxR[ep_nbr] = (((p_reg->EPxR[ep_nbr]) & STM32XXX_EPTYPE_MASK_OFF) | (STM32XXX_EPTYPE_ISOCHRONOUS));
                 break;

            case USBD_EP_TYPE_BULK:
                 p_reg->EPxR[ep_nbr] = (((p_reg->EPxR[ep_nbr]) & STM32XXX_EPTYPE_MASK_OFF) | (STM32XXX_EPTYPE_BULK));
                 break;

            case USBD_EP_TYPE_INTR:
                 p_reg->EPxR[ep_nbr] = (((p_reg->EPxR[ep_nbr]) & STM32XXX_EPTYPE_MASK_OFF) | (STM32XXX_EPTYPE_INTERRUPT));
                 break;

            default:
                 break;
        }

        if (is_out == DEF_YES) {
           *STM32XXX_EP_RX_ADDR(ep_nbr) = STM32XXX_ENDPn_RXADDR(ep_nbr);

            if (max_pkt_size > 64u) {
               *p_err = USBD_ERR_EP_INVALID_TYPE;
                return;
            }

           *STM32XXX_EP_RX_CNT(ep_nbr) = STM32XXX_COUNTn_RX_BL_SIZE + (((64u / 32u) - 1u) << 10);

            STM32_EPnR_SetReg(p_reg,
                              ep_nbr,
                              STM32XXX_EPRXSTAT_VALID,
                              STM32XXX_EPnR_STAT_RX_SET);

        } else {
            STM32_EPnR_SetReg(p_reg,
                              ep_nbr,
                              DEF_BIT_NONE,
                              STM32XXX_EPnR_DTOG_TX_CLR);

           *STM32XXX_EP_TX_ADDR(ep_nbr) = STM32XXX_ENDPn_TXADDR(ep_nbr);
            STM32_EPnR_SetReg(p_reg,
                              ep_nbr,
                              STM32XXX_EPTXSTAT_NAK,
                              STM32XXX_EPnR_STAT_TX_SET);
        }
    }

    p_drv_data->EP_MaxPktSize[ep_phy_nbr] = max_pkt_size;

    USBD_DBG_DRV_EP("  Drv EP Open", ep_addr);
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
    USBD_STM32XXX_REG  *p_reg;
    USBD_DRV_DATA_EP   *p_drv_data;
    CPU_INT08U          ep_nbr;
    CPU_INT08U          ep_phy_nbr;
    CPU_BOOLEAN         ep_dir_in;


    p_reg      = (USBD_STM32XXX_REG *)p_drv->CfgPtr->BaseAddr;  /* Get ref to USB HW reg.                               */
    p_drv_data = (USBD_DRV_DATA_EP  *)p_drv->DataPtr;
    ep_nbr     =  USBD_EP_ADDR_TO_LOG(ep_addr);
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    ep_dir_in  =  USBD_EP_IS_IN(ep_addr);

    if (ep_dir_in == DEF_YES) {
       *STM32XXX_EP_TX_ADDR(ep_nbr) = 0u;
       *STM32XXX_EP_TX_CNT(ep_nbr)  = 0u;

        STM32_EPnR_SetReg(p_reg,
                          ep_nbr,
                          STM32XXX_EPTXSTAT_DIS,
                          STM32XXX_EPnR_STAT_TX_SET);

    } else {
       *STM32XXX_EP_RX_ADDR(ep_nbr) = 0u;
       *STM32XXX_EP_RX_CNT(ep_nbr)  = 0u;

        STM32_EPnR_SetReg(p_reg,
                          ep_nbr,
                          STM32XXX_EPRXSTAT_DIS,
                          STM32XXX_EPnR_STAT_RX_SET);
    }

    p_drv_data->EP_MaxPktSize[ep_phy_nbr] = 0u;

    USBD_DBG_DRV_EP("  Drv EP Close", ep_addr);
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
    USBD_STM32XXX_REG  *p_reg;
    CPU_INT08U          ep_nbr;
    CPU_INT08U          ep_phy_nbr;
    CPU_INT16U          ep_rxstat;
    USBD_DRV_DATA_EP   *p_drv_data;


    (void)p_buf;
    (void)buf_len;

    p_drv_data = (USBD_DRV_DATA_EP  *)p_drv->DataPtr;
    p_reg      = (USBD_STM32XXX_REG *)p_drv->CfgPtr->BaseAddr;  /* Get ref to USB HW reg.                               */
    ep_nbr     =  USBD_EP_ADDR_TO_LOG(ep_addr);
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    ep_rxstat  =  p_reg->EPxR[ep_nbr] & STM32XXX_EP_RXSTAT;


    if (ep_rxstat != STM32XXX_EPRXSTAT_STALL) {
        STM32_EPnR_SetReg(p_reg,
                          ep_nbr,
                          STM32XXX_EPRXSTAT_VALID,
                          STM32XXX_EPnR_STAT_RX_SET);
    }

    USBD_DBG_DRV_EP("  Drv EP Rx Start Pkt (Int en)", ep_addr);

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
    USBD_STM32XXX_REG  *p_reg;
    CPU_INT32U          len;
    CPU_INT32U          len_tot;
    CPU_INT16U          datum;
    CPU_INT08U          ep_nbr;
    CPU_INT32U         *p_buf_usb;


    p_reg  = (USBD_STM32XXX_REG *)p_drv->CfgPtr->BaseAddr;      /* Get ref to USB HW reg.                               */
    ep_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    len    = (*STM32XXX_EP_RX_CNT(ep_nbr)) & 0x03FFu;

    if (len > buf_len) {
       *p_err = USBD_ERR_RX;
        len   = buf_len;
    } else {
       *p_err = USBD_ERR_NONE;
    }

    len_tot   =  len;
    p_buf_usb = (CPU_INT32U *)(STM32XXX_ENDPn_RXADDR(ep_nbr) * 2 + STM32XXX_BASE_PMA);

    while (len >= 2u) {
        datum = *p_buf_usb;
       *p_buf =  datum       & 0xFFu;
        p_buf++;
       *p_buf = (datum >> 8) & 0xFFu;
        p_buf++;
        p_buf_usb++;
        len  -=  2u;
    }

    if (len > 0u) {
        datum = *p_buf_usb;
       *p_buf =  datum & 0xFFu;
        p_buf_usb++;
    }

    USBD_DBG_DRV_EP_ARG("  Drv EP Rx Len:", ep_addr, len_tot);
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
    USBD_STM32XXX_REG  *p_reg;
    USBD_DRV_DATA_EP   *p_drv_data;
    CPU_INT32U          len;
    CPU_INT32U          len_tot;
    CPU_INT16U          datum;
    CPU_INT08U          ep_nbr;
    CPU_INT08U          ep_phy_nbr;
    CPU_INT32U         *p_buf_usb;


    p_reg      = (USBD_STM32XXX_REG *)p_drv->CfgPtr->BaseAddr;      /* Get ref to USB HW reg.                               */
    p_drv_data = (USBD_DRV_DATA_EP  *)p_drv->DataPtr;
    ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
    len_tot    = (CPU_INT16U)DEF_MIN(p_drv_data->EP_MaxPktSize[ep_phy_nbr], buf_len);
    ep_nbr     =  USBD_EP_ADDR_TO_LOG(ep_addr);

    if (len_tot > 64u) {
        len = 64u;
    } else {
        len = len_tot;
    }

    len_tot   =  len;
    p_buf_usb = (CPU_INT32U *)(STM32XXX_ENDPn_TXADDR(ep_nbr) * 2u + STM32XXX_BASE_PMA);

    while (len >= 2u) {
        datum     =  *p_buf;
        p_buf++;
        datum    |= (*p_buf << 8);
        p_buf++;
       *p_buf_usb = datum;
        p_buf_usb++;
        len      -= 2u;
    }

    if (len > 0u) {
        datum     = *p_buf;
       *p_buf_usb =  datum;
    }

   *STM32XXX_EP_TX_CNT(ep_nbr) = len_tot;

    USBD_DBG_DRV_EP_ARG("  Drv EP Tx Len:", ep_addr, len_tot);
   *p_err = USBD_ERR_NONE;

    return (len_tot);
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
    USBD_STM32XXX_REG  *p_reg;
    CPU_INT08U          ep_nbr;


    (void)p_buf;
    (void)buf_len;

    p_reg  = (USBD_STM32XXX_REG *)p_drv->CfgPtr->BaseAddr;      /* Get ref to USB HW reg.                               */
    ep_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);

    STM32_EPnR_SetReg(p_reg,
                      ep_nbr,
                      STM32XXX_EPTXSTAT_VALID,
                      STM32XXX_EPnR_STAT_TX_SET);

    USBD_DBG_DRV_EP_ARG("  Drv EP Tx Start Len:", ep_addr, buf_len);

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
    USBD_STM32XXX_REG  *p_reg;
    CPU_INT08U          ep_nbr;


    p_reg  = (USBD_STM32XXX_REG *)p_drv->CfgPtr->BaseAddr;      /* Get ref to USB HW reg.                               */
    ep_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);

   *STM32XXX_EP_TX_CNT(ep_nbr) = 0u;
    STM32_EPnR_SetReg(p_reg,
                      ep_nbr,
                      STM32XXX_EPTXSTAT_VALID,
                      STM32XXX_EPnR_STAT_TX_SET);

    USBD_DBG_DRV_EP("  Drv EP FIFO TxZLP", ep_addr);
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
        USBD_DBG_DRV_EP("  Drv EP Abort", ep_addr);
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
    USBD_STM32XXX_REG  *p_reg;
    CPU_INT08U          ep_nbr;
    CPU_BOOLEAN         is_out;


    p_reg  = (USBD_STM32XXX_REG *)p_drv->CfgPtr->BaseAddr;      /* Get ref to USB HW reg.                               */
    ep_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    is_out = ((ep_addr & 0x80u) == 0x80u) ? DEF_NO : DEF_YES;

    switch(state) {
        case DEF_SET:
             if (ep_nbr == 0u) {
                 STM32_EPnR_SetReg(p_reg,
                                   ep_nbr,
                                   STM32XXX_EPTXSTAT_STALL,
                                   STM32XXX_EPnR_STAT_TX_SET);

                 STM32_EPnR_SetReg(p_reg,
                                   ep_nbr,
                                   STM32XXX_EPRXSTAT_STALL,
                                   STM32XXX_EPnR_STAT_RX_SET);

                 return (DEF_OK);
             }

             if (is_out == DEF_YES) {
                 STM32_EPnR_SetReg(p_reg,
                                   ep_nbr,
                                   STM32XXX_EPRXSTAT_STALL,
                                   STM32XXX_EPnR_STAT_RX_SET);
             } else {
                 STM32_EPnR_SetReg(p_reg,
                                   ep_nbr,
                                   STM32XXX_EPTXSTAT_STALL,
                                   STM32XXX_EPnR_STAT_TX_SET);
             }
             break;

        case DEF_CLR:
             if (is_out == DEF_YES) {
                 if (ep_nbr == 0u) {
                    *STM32XXX_EP_RX_CNT(0u) = 64u;
                 } else {
                     STM32_EPnR_SetReg(p_reg,
                                       ep_nbr,
                                       DEF_BIT_NONE,
                                       STM32XXX_EPnR_DTOG_RX_CLR);
                 }

                 STM32_EPnR_SetReg(p_reg,
                                   ep_nbr,
                                   STM32XXX_EPRXSTAT_VALID,
                                   STM32XXX_EPnR_STAT_RX_SET);
             } else {
                 STM32_EPnR_SetReg(p_reg,
                                   ep_nbr,
                                   DEF_BIT_NONE,
                                   STM32XXX_EPnR_DTOG_TX_CLR);

                 STM32_EPnR_SetReg(p_reg,
                                   ep_nbr,
                                   STM32XXX_EPTXSTAT_NAK,
                                   STM32XXX_EPnR_STAT_TX_SET);
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
    USBD_STM32XXX_REG  *p_reg;
    CPU_INT16U          istr;


    p_reg = (USBD_STM32XXX_REG *)p_drv->CfgPtr->BaseAddr;       /* Get ref to USB HW reg.                               */

                                                                /* --------------- GET INTERRUPT STATUS --------------- */
    istr = p_reg->ISTR;
                                                                /* ------------ HANDLE BUS RESET INTERRUPT ------------ */
    if (DEF_BIT_IS_SET(istr, STM32XXX_ISTR_RESET) == DEF_YES) {
        p_reg->ISTR = STM32XXX_ISTR_CLR_RESET;                  /* Clr interrupt.                                       */

        STM32_Reset(p_reg);                                     /* Reset the STM32 EP's registers                       */
        USBD_EventReset(p_drv);
        p_reg->CNTR = STM32XXX_CNTR_CTRM | STM32XXX_CNTR_RESETM | STM32XXX_CNTR_SUSPM;
    }

                                                                /* -------------- HANDLE WAKEUP INTERRUPT ------------- */
    if (DEF_BIT_IS_SET(istr, STM32XXX_ISTR_WKUP) == DEF_YES) {
        p_reg->ISTR = STM32XXX_ISTR_CLR_WKUP;
        USBD_EventResume(p_drv);
    }

                                                                /* ------------- HANDLE SUSPEND INTERRUPT ------------- */
    if (DEF_BIT_IS_SET(istr, STM32XXX_ISTR_SUSP) == DEF_YES) {
        p_reg->ISTR = STM32XXX_ISTR_CLR_SUSP;
        USBD_EventSuspend(p_drv);
        p_reg->CNTR = STM32XXX_CNTR_RESETM;                     /* En reset int & dis suspend int.                      */
    }

                                                                /* ------------- HANDLE TRANSFER INTERRUPT ------------ */
    if (DEF_BIT_IS_SET(istr, STM32XXX_ISTR_CTR) == DEF_YES) {
        STM32_ProcessEP(p_reg,
                        p_drv);
    }
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
**************************************************************************************************************
*                                         STM32_Reset()
*
* Description : Reset the STM32xxx EP register to the default value
*
* Argument(s) : p_reg       Pointer to register structure.
*
* Return(s)   : none.
*
* Note(s)     : None.
**************************************************************************************************************
*/

static  void  STM32_Reset (USBD_STM32XXX_REG  *p_reg)
{
    CPU_INT08U  ep_nbr;


    p_reg->BTABLE =  0x0000u;                                   /* Set the buffer table address to 0x0000               */
                                                                /* Set the ep addr in the USB_EPnR register             */
    p_reg->DADDR  = (STM32XXX_DADDR_EF  |                       /* Enable the USB Device & set the dftl USB dev Addr.   */
                     STM32XXX_DADDR_ADD_DFLT);

    for (ep_nbr = 0u; ep_nbr < 8u; ep_nbr++) {
        p_reg->EPxR[ep_nbr] = ((p_reg->EPxR[ep_nbr]) & STM32XXX_EP_MASK) | ep_nbr;
    }
}


/*
*********************************************************************************************************
*                                           STM32_ProcessEP()
*
* Description : Process IN, OUT and SETUP packets for a specific endpoint.
*
* Argument(s) : p_reg           Pointer to register structure.
*
*               p_drv           Pointer to device driver structure.
*
* Return(s)   : none.
*
* Note(s)     : (1) If DIR  bit=1, CTR_RX bit or both CTR_TX/CTR_RX are set in the USB_EPnR register related
*                   to the interrupt endpoint. The interrupt transaction is of OUT type (data received by the
*                   USB peripheral from the host PC) or two pending transactions are waiting to be processed.
*
*               (2) The SETUP bit must be examined, in the case of a successful receive transaction (CTR_RX
*                   event), to determine the type of transaction occured. To protect the interrupt service
*                   routine from the changes in SETUP bits due to next incoming tokens, this bit is kept
*                   frozen while CTR_RX bit is at 1; its state changes when CTR_RX is at 0.
*********************************************************************************************************
*/

static  void  STM32_ProcessEP (USBD_STM32XXX_REG  *p_reg,
                               USBD_DRV           *p_drv)
{
    CPU_INT16U          istr;
    CPU_INT08U          ep_ix;
    CPU_INT16U          ep_val;

                                                                 /* --------------- GET INTERRUPT STATUS --------------- */
    istr = p_reg->ISTR;

    while ((istr & STM32XXX_ISTR_CTR) != 0) {                   /* Stay in loop while pending ints.                     */
        p_reg->ISTR = STM32XXX_ISTR_CLR_CTR;                    /* Clear ctr flag.                                      */

                                                                /* Extract highest priority endpoint number.            */
        ep_ix = (CPU_INT08U)(istr & STM32XXX_ISTR_EP_ID);

        if (ep_ix == 0u) {                                      /* ----------------- CONTROL ENDPOINT ----------------  */
                                                                /* IN transaction.                                      */
            if (DEF_BIT_IS_CLR(istr, STM32XXX_ISTR_DIR) == DEF_YES) {
                STM32_EPnR_SetReg(p_reg,
                                  ep_ix,
                                  DEF_BIT_NONE,
                                  STM32XXX_EPnR_CTR_TX_CLR);

                USBD_EP_TxCmpl(p_drv, ep_ix);                   /* Notify core about Tx transfer complete.              */
                return;

            } else {                                            /* See Note #1.                                         */

                ep_val = p_reg->EPxR[ep_ix];
                if ((ep_val & STM32XXX_EP_CTRTX) != 0) {
                    STM32_EPnR_SetReg(p_reg,
                                      ep_ix,
                                      DEF_BIT_NONE,
                                      STM32XXX_EPnR_CTR_TX_CLR);

                    USBD_EP_TxCmpl(p_drv, ep_ix);               /* Notify core about Tx transfer complete.              */
                    return;

                } else if ((ep_val & STM32XXX_EP_SETUP) != 0) {
                                                                /* See Note #2.                                         */
                    STM32_EPnR_SetReg(p_reg,
                                      ep_ix,
                                      DEF_BIT_NONE,
                                      STM32XXX_EPnR_CTR_RX_CLR);

                    STM32_Process_Setup(p_reg,
                                        p_drv,
                                        ep_ix);
                    return;

                } else if ((ep_val & STM32XXX_EP_CTRRX) != 0) {
                    STM32_EPnR_SetReg(p_reg,
                                      ep_ix,
                                      DEF_BIT_NONE,
                                      STM32XXX_EPnR_CTR_RX_CLR);

                    USBD_EP_RxCmpl(p_drv, ep_ix);               /* Notify core about Rx transfer complete.              */
                    return;
                }
            }

        } else {                                                /* --------------- NON-CONTROL ENDPOINT --------------- */
            ep_val = p_reg->EPxR[ep_ix];                        /* Process related endpoint register.                   */
            if ((ep_val & STM32XXX_EP_CTRRX) != 0) {
                                                                /* Clear int flag.                                      */
                STM32_EPnR_SetReg(p_reg,
                                  ep_ix,
                                  DEF_BIT_NONE,
                                  STM32XXX_EPnR_CTR_RX_CLR);

                USBD_EP_RxCmpl(p_drv, ep_ix);                   /* Notify core about Rx transfer complete.              */
            }

            if ((ep_val & STM32XXX_EP_CTRTX) != 0) {
                                                                /* Clear int flag.                                      */
                STM32_EPnR_SetReg(p_reg,
                                  ep_ix,
                                  DEF_BIT_NONE,
                                  STM32XXX_EPnR_CTR_TX_CLR);

                USBD_EP_TxCmpl(p_drv, ep_ix);                   /* Notify core about Tx transfer complete.              */
            }
        }
                                                                /* --------------- GET INTERRUPT STATUS --------------- */
        istr = p_reg->ISTR;
    }
}


/*
*********************************************************************************************************
*                                           STM32_Process_Setup()
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
*********************************************************************************************************
*/

static  void  STM32_Process_Setup (USBD_STM32XXX_REG  *p_reg,
                                   USBD_DRV           *p_drv,
                                   CPU_INT08U          ep_nbr)
{
    CPU_INT16U   buf_len;
    CPU_INT16U   setup_buf[4u];
    CPU_INT32U  *p_usb_buf;
    CPU_INT08U   i;


    p_usb_buf = (CPU_INT32U *)(STM32XXX_ENDPn_RXADDR(ep_nbr) * 2u + STM32XXX_BASE_PMA);
    buf_len   = (*STM32XXX_EP_RX_CNT(ep_nbr)) & 0x03FFu;

    for (i = 0u; i < buf_len; i++) {
        setup_buf[i] = *p_usb_buf;
        p_usb_buf++;
    }

    USBD_EventSetup(p_drv, (void *)&setup_buf[0]);
}


/*
**************************************************************************************************************
*                                            STM32_EPnR_SetReg()
*
* Description : Set a value in the EPnR register.
*
* Argument(s) : p_reg           Pointer to register structure.
*
*               ep_nbr          The enpoint number.
*
*               ep_reg_val      value to be set in the enpoint register.
*
*               ep_action       Action to be performed in the EPnP register (where m can be T(Transmission) or R(reception):
*
*                               STM32XXX_EPnR_CTR_mX_CLR    Clear the CTR_RX/CTR_TX bit in the EPnPR register.
*
*                               STM32XXX_EPnR_DTOG_mX_CLR   Clear the Data Toggle bit (0 = DATA0 PID)
*
*                               STM32XXX_EPnR_STAT_mX_SET   Set the Endpoint Status.
*
* Return(s)   : none.
*
* Note(s)     : none
**************************************************************************************************************
*/

static  void  STM32_EPnR_SetReg (USBD_STM32XXX_REG  *p_reg,
                                 CPU_INT08U          ep_nbr,
                                 CPU_INT32U          ep_reg_val,
                                 CPU_INT08U          ep_action)
{
    CPU_INT16U  reg_val;


    switch (ep_action) {
        case STM32XXX_EPnR_CTR_RX_CLR:
             p_reg->EPxR[ep_nbr] = ((p_reg->EPxR[ep_nbr]) & (0x7FFFu & STM32XXX_EP_MASK));
             break;


        case STM32XXX_EPnR_DTOG_RX_CLR:
             if (((p_reg->EPxR[ep_nbr]) & STM32XXX_EP_DTOGRX) != 0u) {
                                                                /* Toggle DTOG RX.                                      */
                 p_reg->EPxR[ep_nbr] = (STM32XXX_EP_DTOGRX | ((p_reg->EPxR[ep_nbr]) & STM32XXX_EP_MASK));
             }
             break;


        case STM32XXX_EPnR_STAT_RX_SET:
             reg_val = (p_reg->EPxR[ep_nbr]) & STM32XXX_EPRXSTAT_DTOGMASK;

             if ((STM32XXX_EPRXSTAT_DTOG1 & (ep_reg_val)) != 0u) {
                 reg_val ^= STM32XXX_EPRXSTAT_DTOG1;
             }

             if ((STM32XXX_EPRXSTAT_DTOG2 & (ep_reg_val)) != 0u) {
                 reg_val ^= STM32XXX_EPRXSTAT_DTOG2;
             }

             p_reg->EPxR[ep_nbr] = reg_val;
             break;


        case STM32XXX_EPnR_CTR_TX_CLR:
             p_reg->EPxR[ep_nbr] = ((p_reg->EPxR[ep_nbr]) & (0xFF7Fu & STM32XXX_EP_MASK));
             break;


        case STM32XXX_EPnR_DTOG_TX_CLR:
             if (((p_reg->EPxR[ep_nbr]) & STM32XXX_EP_DTOGTX) != 0u) {
                                                                /* Toggle DTOG TX.                                      */
                 p_reg->EPxR[ep_nbr] = (STM32XXX_EP_DTOGTX | ((p_reg->EPxR[ep_nbr]) & STM32XXX_EP_MASK));
             }
             break;


        case STM32XXX_EPnR_STAT_TX_SET:
             reg_val = (p_reg->EPxR[ep_nbr]) & STM32XXX_EPTXSTAT_DTOGMASK;

             if ((STM32XXX_EPTXSTAT_DTOG1 & (ep_reg_val)) != 0u) {
                 reg_val ^= STM32XXX_EPTXSTAT_DTOG1;
             }

             if ((STM32XXX_EPTXSTAT_DTOG2 & (ep_reg_val)) != 0u) {
                 reg_val ^= STM32XXX_EPTXSTAT_DTOG2;
             }

             p_reg->EPxR[ep_nbr] = reg_val;
             break;


        default:
             break;
    }
}
