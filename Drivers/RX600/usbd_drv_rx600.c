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
*                                           USB DEVICE DRIVER
*
*                                             Renesas RX600
*                                             Renesas RX100
*                                          Renesas V850E2/Jx4-L
*
* Filename : usbd_drv_rx600.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)  : (1) You can find specific information about this driver at:
*                https://doc.micrium.com/display/USBDDRV/RX600
*
*            (2) With an appropriate BSP, this device driver will support the USB device module on
*                the Renesas RX600, RX100 and V850E2/Jx4-L series.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                             INCLUDE FILES
*********************************************************************************************************
*/

#include  "../../Source/usbd_core.h"
#include  "usbd_drv_rx600.h"


/*
*********************************************************************************************************
*                                             LOCAL MACROS
*********************************************************************************************************
*/

#define  RX600_EP_PHY_TO_EP_TBL_IX(ep_phy)         (((ep_phy) <= 1u) ? (ep_phy) : (((ep_phy) >> 1u) + 1u))

#define  USBD_DBG_DRV(msg)                           USBD_DBG_GENERIC((msg),                            \
                                                                      USBD_EP_NBR_NONE,                 \
                                                                      USBD_IF_NBR_NONE)

#define  USBD_DBG_DRV_EP(msg, ep_addr)               USBD_DBG_GENERIC((msg),                            \
                                                                      (ep_addr),                        \
                                                                       USBD_IF_NBR_NONE)

#define  USBD_DBG_DRV_EP_ARG(msg, ep_addr, arg)      USBD_DBG_GENERIC_ARG((msg),                        \
                                                                          (ep_addr),                    \
                                                                           USBD_IF_NBR_NONE,            \
                                                                          (arg))

#define  RX600_DBG_STATS_EN                          DEF_DISABLED

#if (RX600_DBG_STATS_EN == DEF_ENABLED)
#define  USBD_DBG_STATS_RESET()                                 {                                                     \
                                                                    Mem_Clr((void     *)&USBD_DbgStats,               \
                                                                            (CPU_SIZE_T) sizeof(USBD_DBG_STATS_DRV)); \
                                                                }

#define  USBD_DBG_STATS_INC(stat)                               {                                                     \
                                                                    USBD_DbgStats.stat++;                             \
                                                                }

#define  USBD_DBG_STATS_INC_IF_TRUE(stat, bool)                 {                                                     \
                                                                    if (bool == DEF_TRUE) {                           \
                                                                        USBD_DBG_STATS_INC(stat);                     \
                                                                    }                                                 \
                                                                }
#else
#define  USBD_DBG_STATS_RESET()
#define  USBD_DBG_STATS_INC(stat)
#define  USBD_DBG_STATS_INC_IF_TRUE(stat, bool)
#endif


/*
*********************************************************************************************************
*                    RX600/RX100/V850E2Jx4L USB HARDWARE REGISTER BIT DEFINITIONS
*********************************************************************************************************
*/

                                                                /* ------ SYSTEM CONFIGURATION CONTROL REGISTER ------- */
#define  RX600_USB_SYSCFG_SCKE                    DEF_BIT_10
#define  RX600_USB_SYSCFG_DCFM                    DEF_BIT_06
#define  RX600_USB_SYSCFG_DRPD                    DEF_BIT_05
#define  RX600_USB_SYSCFG_DPRPU                   DEF_BIT_04
#define  RX600_USB_SYSCFG_USBE                    DEF_BIT_00

                                                                /* ------ SYSTEM CONFIGURATION STATUS REGISTER 0 ------ */
#define  RX600_USB_SYSSTS0_OCVMON                 DEF_BIT_FIELD(2, 14)
#define  RX600_USB_SYSSTS0_OVRCURA                DEF_BIT_15
#define  RX600_USB_SYSSTS0_OVRCURB                DEF_BIT_14
#define  RX600_USB_SYSSTS0_HTACT                  DEF_BIT_08
#define  RX600_USB_SYSSTS0_IDMON                  DEF_BIT_02
#define  RX600_USB_SYSSTS0_LNST                   DEF_BIT_FIELD(2, 0)

                                                                /* --------- DEVICE STATE CONTROL REGISTER 0 ---------- */
#define  RX600_USB_DVSTCTR0_HNPBTOA               DEF_BIT_11
#define  RX600_USB_DVSTCTR0_EXICEN                DEF_BIT_10
#define  RX600_USB_DVSTCTR0_VBUSEN                DEF_BIT_09
#define  RX600_USB_DVSTCTR0_WKUP                  DEF_BIT_08
#define  RX600_USB_DVSTCTR0_RWUPE                 DEF_BIT_07
#define  RX600_USB_DVSTCTR0_USBRST                DEF_BIT_06
#define  RX600_USB_DVSTCTR0_RESUME                DEF_BIT_05
#define  RX600_USB_DVSTCTR0_UACT                  DEF_BIT_04
#define  RX600_USB_DVSTCTR0_RHST                  DEF_BIT_FIELD(3, 0)

                                                                /* ------------ CFIFO PORT SELECT REGISTER ------------ */
#define  RX600_USB_CFIFOSEL_RCNT                  DEF_BIT_15
#define  RX600_USB_CFIFOSEL_REW                   DEF_BIT_14
#define  RX600_USB_CFIFOSEL_MBW                   DEF_BIT_10
#define  RX600_USB_CFIFOSEL_BIGEND                DEF_BIT_08
#define  RX600_USB_CFIFOSEL_ISEL                  DEF_BIT_05
#define  RX600_USB_CFIFOSEL_CURPIPE               DEF_BIT_FIELD(4, 0)

                                                                /* ----- D0FIFO AND D1FIFO PORT SELECT REGISTERS ------ */
#define  RX600_USB_DFIFOSEL_RCNT                  DEF_BIT_15
#define  RX600_USB_DFIFOSEL_REW                   DEF_BIT_14
#define  RX600_USB_DFIFOSEL_DCLRM                 DEF_BIT_13
#define  RX600_USB_DFIFOSEL_DREQE                 DEF_BIT_12
#define  RX600_USB_DFIFOSEL_MBW                   DEF_BIT_10
#define  RX600_USB_DFIFOSEL_BIGEND                DEF_BIT_08
#define  RX600_USB_DFIFOSEL_CURPIPE               DEF_BIT_FIELD(4, 0)

                                                                /* -- CFIFO D0FIFO AND D1FIFO PORT CONTROL REGISTERS -- */
#define  RX600_USB_FIFOCTR_BVAL                   DEF_BIT_15
#define  RX600_USB_FIFOCTR_BCLR                   DEF_BIT_14
#define  RX600_USB_FIFOCTR_FRDY                   DEF_BIT_13
#define  RX600_USB_FIFOCTR_DTLN                   DEF_BIT_FIELD(8, 0)

                                                                /* ----------- INTERRUPT ENABLE REGISTER 0 ------------ */
#define  RX600_USB_INTENB0_VBSE                   DEF_BIT_15
#define  RX600_USB_INTENB0_RSME                   DEF_BIT_14
#define  RX600_USB_INTENB0_SOFE                   DEF_BIT_13
#define  RX600_USB_INTENB0_DVSE                   DEF_BIT_12
#define  RX600_USB_INTENB0_CTRE                   DEF_BIT_11
#define  RX600_USB_INTENB0_BEMPE                  DEF_BIT_10
#define  RX600_USB_INTENB0_NRDYE                  DEF_BIT_09
#define  RX600_USB_INTENB0_BRDYE                  DEF_BIT_08

                                                                /* ----------- INTERRUPT ENABLE REGISTER 1 ------------ */
#define  RX600_USB_INTENB1_OVRCRE                 DEF_BIT_15
#define  RX600_USB_INTENB1_BCHGE                  DEF_BIT_14
#define  RX600_USB_INTENB1_DTCHE                  DEF_BIT_12
#define  RX600_USB_INTENB1_ATTCHE                 DEF_BIT_11
#define  RX600_USB_INTENB1_EOFERRE                DEF_BIT_06
#define  RX600_USB_INTENB1_SIGNE                  DEF_BIT_05
#define  RX600_USB_INTENB1_SACKE                  DEF_BIT_04

                                                                /* -------- SOF OUTPUT CONFIGURATION REGISTER --------- */
#define  RX600_USB_SOFCFG_TRNENSEL                DEF_BIT_08
#define  RX600_USB_SOFCFG_BRDYM                   DEF_BIT_06
#define  RX600_USB_SOFCFG_EDGESTS                 DEF_BIT_04

                                                                /* ----------- INTERRUPT STATUS REGISTER 0 ------------ */
#define  RX600_USB_INTSTS0_VBINT                  DEF_BIT_15
#define  RX600_USB_INTSTS0_RESM                   DEF_BIT_14
#define  RX600_USB_INTSTS0_SOFR                   DEF_BIT_13
#define  RX600_USB_INTSTS0_DVST                   DEF_BIT_12
#define  RX600_USB_INTSTS0_CTRT                   DEF_BIT_11
#define  RX600_USB_INTSTS0_BEMP                   DEF_BIT_10
#define  RX600_USB_INTSTS0_NRDY                   DEF_BIT_09
#define  RX600_USB_INTSTS0_BRDY                   DEF_BIT_08
#define  RX600_USB_INTSTS0_VBSTS                  DEF_BIT_07
#define  RX600_USB_INTSTS0_DVSQ                   DEF_BIT_FIELD(3, 4)
#define  RX600_USB_INTSTS0_VALID                  DEF_BIT_03
#define  RX600_USB_INTSTS0_CTSQ                   DEF_BIT_FIELD(3, 0)

                                                                /* ----------- INTERRUPT STATUS REGISTER 0 ------------ */
                                                                /* ------------------- DEVICE STATE ------------------- */
#define  RX600_USB_INTSTS0_DVSQ_PWR                  0x00u
#define  RX600_USB_INTSTS0_DVSQ_DFLT                 0x10u
#define  RX600_USB_INTSTS0_DVSQ_ADDR                 0x20u
#define  RX600_USB_INTSTS0_DVSQ_CFG                  0x30u
#define  RX600_USB_INTSTS0_DVSQ_SUSPEND_1            0x40u
#define  RX600_USB_INTSTS0_DVSQ_SUSPEND_2            0x50u
#define  RX600_USB_INTSTS0_DVSQ_SUSPEND_3            0x60u
#define  RX600_USB_INTSTS0_DVSQ_SUSPEND_4            0x70u


                                                                /* -------------- CONTROL TRANSFER STAGE -------------- */
#define  RX600_USB_INTSTS0_CTSQ_SETUP                0x00u
#define  RX600_USB_INTSTS0_CTSQ_CTRL_RD_DATA         0x01u
#define  RX600_USB_INTSTS0_CTSQ_CTRL_RD_STATUS       0x02u
#define  RX600_USB_INTSTS0_CTSQ_CTRL_WR_DATA         0x03u
#define  RX600_USB_INTSTS0_CTSQ_CTRL_WR_STATUS       0x04u
#define  RX600_USB_INTSTS0_CTSQ_CTRL_WR_ND_STATUS    0x05u
#define  RX600_USB_INTSTS0_CTSQ_CTRL_TX_SEQ_ERR      0x06u

                                                                /* ----------- INTERRUPT STATUS REGISTER 1 ------------ */
#define  RX600_USB_INTSTS1_OVRCR                  DEF_BIT_15
#define  RX600_USB_INTSTS1_BCHG                   DEF_BIT_14
#define  RX600_USB_INTSTS1_DTCH                   DEF_BIT_12
#define  RX600_USB_INTSTS1_ATTCH                  DEF_BIT_11
#define  RX600_USB_INTSTS1_EOFERR                 DEF_BIT_06
#define  RX600_USB_INTSTS1_SIGN                   DEF_BIT_05
#define  RX600_USB_INTSTS1_SACK                   DEF_BIT_04

                                                                /* -------------- FRAME NUMBER REGISTER --------------- */
#define  RX600_USB_FRMNUM_OVRN                    DEF_BIT_15
#define  RX600_USB_FRMNUM_CRCE                    DEF_BIT_14
#define  RX600_USB_FRMNUM_FRNM                    DEF_BIT_FIELD(11, 0)

                                                                /* ----------- DEVICE STATE CHANGE REGISTER ----------- */
#define  RX600_USB_DVCHGR_DVCHG                   DEF_BIT_15

                                                                /* --------------- USB ADDRESS REGISTER --------------- */
                                                                /* ----------------- STATUS RECOVERY ------------------ */
#define  RX600_USB_USBADDR_STSRECOV               DEF_BIT_FIELD(4, 8)

                                                                /* ------------------- USB ADDRESS -------------------- */
#define  RX600_USB_USBADDR_USBADDR                DEF_BIT_FIELD(7, 0)

                                                                /* --------------- USB ADDRESS REGISTER --------------- */
                                                                /* ----------------- STATUS RECOVERY ------------------ */
#define  RX600_USB_USBADDR_STSRECOV_DFLT           0x0900u
#define  RX600_USB_USBADDR_STSRECOV_ADDR           0x0A00u
#define  RX600_USB_USBADDR_STSRECOV_CFG            0x0B00u

                                                                /* ------------ USB REQUEST TYPE REGISTER ------------- */
                                                                /* --------------------- REQUEST ---------------------- */
#define  RX600_USB_USBREQ_BREQUEST                DEF_BIT_FIELD(8, 8)

                                                                /* ------------------- REQUEST TYPE ------------------- */
#define  RX600_USB_USBREQ_BMREQUESTTYPE           DEF_BIT_FIELD(8, 0)

                                                                /* ------------ DCP CONFIGURATION REGISTER ------------ */
#define  RX600_USB_DCPCFG_SHTNAK                  DEF_BIT_07    /* Should be modified only when PID == NAK state.       */
#define  RX600_USB_DCPCFG_DIR                     DEF_BIT_04    /* Should be modified only when PID == NAK state.       */

                                                                /* --------- DCP MAXIMUM PACKET SIZE REGISTER --------- */
                                                                /* ------------------ DEVICE SELECT ------------------- */
                                                                /* Should be modified only when PID == NAK state.       */
#define  RX600_USB_DCPMAXP_DEVSEL                 DEF_BIT_FIELD(4, 12)

                                                                /* --------------- MAXIMUM PACKET SIZE ---------------- */
                                                                /* Should be modified only when PID == NAK state.       */
#define  RX600_USB_DCPMAXP_MXPS                   DEF_BIT_FIELD(7, 0)

                                                                /* --------------- DCP CONTROL REGISTER --------------- */
#define  RX600_USB_DCPCTR_BSTS                    DEF_BIT_15
#define  RX600_USB_DCPCTR_SUREQ                   DEF_BIT_14
#define  RX600_USB_DCPCTR_SUREQCLR                DEF_BIT_11
#define  RX600_USB_DCPCTR_SQCLR                   DEF_BIT_08    /* Should be modified only when PID == NAK state.       */
#define  RX600_USB_DCPCTR_SQSET                   DEF_BIT_07    /* Should be modified only when PID == NAK state.       */
#define  RX600_USB_DCPCTR_SQMON                   DEF_BIT_06
#define  RX600_USB_DCPCTR_PBUSY                   DEF_BIT_05
#define  RX600_USB_DCPCTR_CCPL                    DEF_BIT_02
#define  RX600_USB_DCPCTR_PID                     DEF_BIT_FIELD(2, 0)

                                                                /* ------------------- RESPONSE PID ------------------- */
#define  RX600_USB_DCPCTR_PID_NAK                    0x00u
#define  RX600_USB_DCPCTR_PID_BUF                    0x01u
#define  RX600_USB_DCPCTR_PID_STALL_1                0x02u
#define  RX600_USB_DCPCTR_PID_STALL_2                0x03u
#define  RX600_USB_DCPCTR_PID_INVALID                0x04u

                                                                /* ----------- PIPE WINDOW SELECT REGISTER ------------ */
#define  RX600_USB_PIPESEL_MASK                   DEF_BIT_FIELD(4,0)

                                                                /* ----------- PIPE CONFIGURATION REGISTER ------------ */
                                                                /* Should be modified only when PID == NAK state.       */
#define  RX600_USB_PIPECFG_TYPE                   DEF_BIT_FIELD(2, 14)
#define  RX600_USB_PIPECFG_BFRE                   DEF_BIT_10
#define  RX600_USB_PIPECFG_DBLB                   DEF_BIT_09
#define  RX600_USB_PIPECFG_SHTNAK                 DEF_BIT_07
#define  RX600_USB_PIPECFG_DIR                    DEF_BIT_04
#define  RX600_USB_PIPECFG_EPNUM                  DEF_BIT_FIELD(4, 0)

                                                                /* ------------------ TRANSFER TYPE ------------------- */
#define  RX600_USB_PIPECFG_TYPE_BULK               0x4000u
#define  RX600_USB_PIPECFG_TYPE_INTR               0x8000u
#define  RX600_USB_PIPECFG_TYPE_ISOC               0xC000u

                                                                /* -------- PIPE MAXIMUM PACKET SIZE REGISTER --------- */
                                                                /* Should be modified only when PID == NAK state. OK.   */
#define  RX600_USB_PIPEMAXP_DEVSEL                DEF_BIT_FIELD(4, 0)
#define  RX600_USB_PIPEMAXP_MXPS                  DEF_BIT_FIELD(9, 0)

                                                                /* ----------- PIPE CYCLE CONTROL REGISTER ------------ */
                                                                /* Should be modified only when PID == NAK state. OK.   */
#define  RX600_USB_PIPEPERI_IFIS                  DEF_BIT_12
#define  RX600_USB_PIPEPERI_IITV                  DEF_BIT_FIELD(3, 0)

                                                                /* -------------- PIPE CONTROL REGISTER --------------- */
#define  RX600_USB_PIPECTR_BSTS                   DEF_BIT_15
#define  RX600_USB_PIPECTR_INBUFM                 DEF_BIT_14
#define  RX600_USB_PIPECTR_ATREPM                 DEF_BIT_10    /* Should be modified only when PID == NAK state.       */
#define  RX600_USB_PIPECTR_ACLRM                  DEF_BIT_09    /* Should be modified only when PID == NAK state.       */
#define  RX600_USB_PIPECTR_SQCLR                  DEF_BIT_08    /* Should be modified only when PID == NAK state.       */
#define  RX600_USB_PIPECTR_SQSET                  DEF_BIT_07    /* Should be modified only when PID == NAK state.       */
#define  RX600_USB_PIPECTR_SQMON                  DEF_BIT_06
#define  RX600_USB_PIPECTR_PBUSY                  DEF_BIT_05
#define  RX600_USB_PIPECTR_PID                    DEF_BIT_FIELD(2, 0)

                                                                /* ----- DEVICE ADDRESS N CONFIGURATION REGISTERS ----- */
#define  RX600_USB_DEVADD_SBSPD                   DEF_BIT_FIELD(2, 6)


/*
*********************************************************************************************************
*                                   OTHER DRIVER INTERNAL DEFINITIONS
*********************************************************************************************************
*/

#define  RX600_USB_PIPE_MAX_NBR                        10u      /* Max nbr of pipes.                                    */
#define  RX600_USB_PIPECTR_NBR                          9u      /* Nbr of pipes control.                                */

#if (CPU_CFG_ENDIAN_TYPE == CPU_ENDIAN_TYPE_BIG)
                                                                /* If CPU is in big endian, single byte is rd on ...    */
                                                                /* ... higher byte of CFIFO, D0FIFO and D1FIFO.         */
#define  RX600_USB_FIFO_SINGLE_BYTE_ACCESS(fifo)  (((CPU_INT08U *)(&(fifo))) + 1u)
#else
                                                                /* If CPU is in little endian, single byte is rd on ... */
                                                                /* ... lower byte of CFIFO, D0FIFO and D1FIFO.          */
#define  RX600_USB_FIFO_SINGLE_BYTE_ACCESS(fifo)  (((CPU_INT08U *)(&(fifo))) + 0u)
#endif


#define  RX600_USB_REG_TO                      0x0000FFFFu      /* Timeout val when actively pending.                   */

                                                                /* ----------------- VBUS DEFINITIONS ----------------- */
#define  RX600_USB_VBUS_RD_DLY                      10000u      /* Dly before reading the val of VBUS status.           */
#define  RX600_USB_VBUS_STABILIZATION_MAX_NBR_RD        5u      /* Nbr of consec rd for VBUS status to confirm state.   */


/*
*********************************************************************************************************
*                                           LOCAL DATA TYPES
*
* Note(s): (1) The RX600, RX100 and V850E2/Jx4-L series USB controller has FIFO buffer memory for data
*              transfer. The memory area used for each pipe is managed by the USB controller.
*
*          (2) The USB controller hardware registers as described in the Renesas RX600, RX100, and
*              V850E2/Jx4L series hardware manual.
*********************************************************************************************************
*/

                                                                /* --------------- RX600/RX100 DATA TYPES ------------- */
typedef  struct  usbd_rx600_transaction_ctr {
    CPU_REG16  PIPE_TRE;                                        /* Pipe transaction counter enable register.            */
    CPU_REG16  PIPE_TRN;                                        /* Pipe transaction counter register.                   */
} USBD_RX600_TRANSACTION_CTR;


                                                                /* -------- RX600/RX100 REGISTERS (SEE NOTE #2) ------- */
typedef  struct  usbd_rx600_reg {
    CPU_REG16                   SYSCFG;                         /* Configures USB module (host/dev, pull-up pins, etc). */
    CPU_REG08                   RSVD_00[2u];
    CPU_REG16                   SYSSTS0;                        /* Status of the D+ and D- lines of the USB bus.        */
    CPU_REG08                   RSVD_01[2u];
    CPU_REG16                   DVSTCTR0;                       /* Control and confirm the state of the USB data bus.   */
    CPU_REG08                   RSVD_02[10u];
    CPU_REG16                   CFIFO;                          /* Read from or write data to FIFO.                     */
    CPU_REG08                   RSVD_03[2u];
    CPU_REG16                   D0FIFO;                         /* Read from or write data to FIFO, DMA-capable.        */
    CPU_REG08                   RSVD_04[2u];
    CPU_REG16                   D1FIFO;                         /* Read from or write data to FIFO, DMA-capable.        */
    CPU_REG08                   RSVD_05[2u];
    CPU_REG16                   CFIFOSEL;                       /* Pipe and port settings for CFIFO.                    */
    CPU_REG16                   CFIFOCTR;                       /* Control register for CFIFO.                          */
    CPU_REG08                   RSVD_06[4u];
    CPU_REG16                   D0FIFOSEL;                      /* Pipe and port settings for D0FIFO.                   */
    CPU_REG16                   D0FIFOCTR;                      /* Control register for D0FIFO.                         */
    CPU_REG16                   D1FIFOSEL;                      /* Pipe and port settings for D1FIFO.                   */
    CPU_REG16                   D1FIFOCTR;                      /* Control register for D1FIFO.                         */
    CPU_REG16                   INTENB0;                        /* Interrupt enable register 0.                         */
    CPU_REG16                   INTENB1;                        /* Interrupt enable register 1.                         */
    CPU_REG08                   RSVD_07[2u];
    CPU_REG16                   BRDYENB;                        /* BRDY interrupt enable register.                      */
    CPU_REG16                   NRDYENB;                        /* NRDY interrupt enable register.                      */
    CPU_REG16                   BEMPENB;                        /* BEMP interrupt enable register.                      */
    CPU_REG16                   SOFCFG;                         /* BRDY interrupt status clear timing.                  */
    CPU_REG08                   RSVD_08[2u];
    CPU_REG16                   INTSTS0;                        /* Interrupt status register 0.                         */
    CPU_REG16                   INTSTS1;                        /* Interrupt status register 1.                         */
    CPU_REG08                   RSVD_09[2u];
    CPU_REG16                   BRDYSTS;                        /* BRDY interrupt status register.                      */
    CPU_REG16                   NRDYSTS;                        /* NRDY interrupt status register.                      */
    CPU_REG16                   BEMPSTS;                        /* BEMP interrupt status register.                      */
    CPU_REG16                   FRMNUM;                         /* Frame number register and isochronous errors status. */
    CPU_REG16                   DVCHGR;                         /* Device state change register.                        */
    CPU_REG16                   USBADDR;                        /* USB device address and wake-up recovery.             */
    CPU_REG08                   RSVD_10[2u];
    CPU_REG16                   USBREQ;                         /* USB request type register.                           */
    CPU_REG16                   USBVAL;                         /* USB request value register.                          */
    CPU_REG16                   USBINDX;                        /* USB request index register.                          */
    CPU_REG16                   USBLENG;                        /* USB request length register.                         */
    CPU_REG16                   DCPCFG;                         /* DCP configuration register.                          */
    CPU_REG16                   DCPMAXP;                        /* DCP maximum packet size register.                    */
    CPU_REG16                   DCPCTR;                         /* DCP control register.                                */
    CPU_REG08                   RSVD_11[2u];
    CPU_REG16                   PIPESEL;                        /* Pipe select register.                                */
    CPU_REG08                   RSVD_12[2u];
    CPU_REG16                   PIPECFG;                        /* Pipe configuration register.                         */
    CPU_REG08                   RSVD_13[2u];
    CPU_REG16                   PIPEMAXP;                       /* Pipe maximum packet size register.                   */
    CPU_REG16                   PIPEPERI;                       /* Pipe cycle control register.                         */
    CPU_REG16                   PIPE_CTR[RX600_USB_PIPECTR_NBR];/* Pipe control register.                               */
                                                                /* PIPEnCTR = PIPE_CTR[n - 1u].                         */
    CPU_REG08                   RSVD_14[2u];

    USBD_RX600_TRANSACTION_CTR  PIPE_TR[5u];                    /* Pipe transaction counter and enable registers.       */
                                                                /* PIPEnTRE = PIPE_TR[n - 1u].                          */
                                                                /* PIPEnTRN = PIPE_TR[n - 1u].                          */
    CPU_REG08                   RSVD_15[44u];
    CPU_REG16                   DEVADD[6u];                     /* Device address configuration registers.              */
                                                                /* DEVADDn  = DEVADD[n].                                */
}  USBD_RX600_REG;


                                                                /* -------------- V850E2/Jx4-L DATA TYPES ------------- */
typedef  struct  usbd_jx4l_transaction_ctr {
    CPU_REG16  TRE;                                             /* Pipe transaction counter enable register.            */
    CPU_REG16  RSVD_42;
    CPU_REG16  TRN;                                             /* Pipe transaction counter register.                   */
    CPU_REG16  RSVD_43;

} USBD_JX4L_TRANSACTION_CTR;


typedef  struct  usbd_jx4l_dev_addr {
    CPU_REG16  RSVD_45;
    CPU_REG16  NBR;
} USBD_JX4L_DEV_ADDR;

                                                                /* ------- V850E2/Jx4-L REGISTERS (SEE NOTE #2) ------- */
typedef  struct  usbd_jx4l_reg {
    CPU_REG16                  SYSCFG;                          /* Configures USB module (host/dev, pull-up pins, etc). */
    CPU_REG16                  RSVD_00[3u];
    CPU_REG16                  SYSSTS0;                         /* Status of the D+ and D- lines of the USB bus.        */
    CPU_REG16                  RSVD_01[3u];
    CPU_REG16                  DVSTCTR0;                        /* Control and confirm the state of the USB data bus.   */
    CPU_REG16                  RSVD_02[7u];
    CPU_REG16                  DMA0PCFG;
    CPU_REG16                  RSVD_03[1u];
    CPU_REG16                  DMA1PCFG;
    CPU_REG16                  RSVD_04[1u];
    CPU_REG16                  CFIFO;                           /* Read from or write data to FIFO.                     */
    CPU_REG16                  RSVD_05[3u];
    CPU_REG16                  D0FIFO;                          /* Read from or write data to FIFO, DMA-capable.        */
    CPU_REG16                  RSVD_06[3u];
    CPU_REG16                  D1FIFO;                          /* Read from or write data to FIFO, DMA-capable.        */
    CPU_REG16                  RSVD_07[3u];
    CPU_REG16                  CFIFOSEL;                        /* Pipe and port settings for CFIFO.                    */
    CPU_REG16                  RSVD_08;
    CPU_REG16                  CFIFOCTR;                        /* Control register for CFIFO.                          */
    CPU_REG16                  RSVD_09[5u];
    CPU_REG16                  D0FIFOSEL;                       /* Pipe and port settings for D0FIFO.                   */
    CPU_REG16                  RSVD_10;
    CPU_REG16                  D0FIFOCTR;                       /* Control register for D0FIFO.                         */
    CPU_REG16                  RSVD_11;
    CPU_REG16                  D1FIFOSEL;                       /* Pipe and port settings for D1FIFO.                   */
    CPU_REG16                  RSVD_12;
    CPU_REG16                  D1FIFOCTR;                       /* Control register for D1FIFO.                         */
    CPU_REG16                  RSVD_13;
    CPU_REG16                  INTENB0;                         /* Interrupt enable register 0.                         */
    CPU_REG16                  RSVD_14;
    CPU_REG16                  INTENB1;                         /* Interrupt enable register 1.                         */
    CPU_REG16                  RSVD_15[3u];
    CPU_REG16                  BRDYENB;                         /* BRDY interrupt enable register.                      */
    CPU_REG16                  RSVD_16;
    CPU_REG16                  NRDYENB;                         /* NRDY interrupt enable register.                      */
    CPU_REG16                  RSVD_17;
    CPU_REG16                  BEMPENB;                         /* BEMP interrupt enable register.                      */
    CPU_REG16                  RSVD_18;
    CPU_REG16                  SOFCFG;                          /* BRDY interrupt status clear timing.                  */
    CPU_REG16                  RSVD_19[3u];
    CPU_REG16                  INTSTS0;                         /* Interrupt status register 0.                         */
    CPU_REG16                  RSVD_20;
    CPU_REG16                  INTSTS1;                         /* Interrupt status register 1.                         */
    CPU_REG16                  RSVD_24[3u];
    CPU_REG16                  BRDYSTS;                         /* BRDY interrupt status register.                      */
    CPU_REG16                  RSVD_25;
    CPU_REG16                  NRDYSTS;                         /* NRDY interrupt status register.                      */
    CPU_REG16                  RSVD_26;
    CPU_REG16                  BEMPSTS;                         /* BEMP interrupt status register.                      */
    CPU_REG16                  RSVD_27;
    CPU_REG16                  FRMNUM;                          /* Frame number register and isochronous errors status. */
    CPU_REG16                  RSVD_28[3u];
    CPU_REG16                  USBADDR;                         /* USB device address and wake-up recovery.             */
    CPU_REG16                  RSVD_29[3u];
    CPU_REG16                  USBREQ;                          /* USB request type register.                           */
    CPU_REG16                  RSVD_30;
    CPU_REG16                  USBVAL;                          /* USB request value register.                          */
    CPU_REG16                  RSVD_31;
    CPU_REG16                  USBINDX;                         /* USB request index register.                          */
    CPU_REG16                  RSVD_32;
    CPU_REG16                  USBLENG;                         /* USB request length register.                         */
    CPU_REG16                  RSVD_33;
    CPU_REG16                  DCPCFG;                          /* DCP configuration register.                          */
    CPU_REG16                  RSVD_34;
    CPU_REG16                  DCPMAXP;                         /* DCP maximum packet size register.                    */
    CPU_REG16                  RSVD_35;
    CPU_REG16                  DCPCTR;                          /* DCP control register.                                */
    CPU_REG16                  RSVD_36[3u];
    CPU_REG16                  PIPESEL;                         /* Pipe select register.                                */
    CPU_REG16                  RSVD_37[3u];
    CPU_REG16                  PIPECFG;                         /* Pipe configuration register.                         */
    CPU_REG16                  RSVD_38[3u];
    CPU_REG16                  PIPEMAXP;                        /* Pipe maximum packet size register.                   */
    CPU_REG16                  RSVD_39;
    CPU_REG16                  PIPEPERI;                        /* Pipe cycle control register.                         */
    CPU_REG16                  RSV_40;
    CPU_REG32                  PIPE_CTR[RX600_USB_PIPECTR_NBR]; /* Pipe control register.                               */
                                                                /* PIPEnCTR = PIPE[n - 1u].CTR                          */
    CPU_REG16                  RSVD_41[15u];
    USBD_JX4L_TRANSACTION_CTR  PIPEx[5u];                       /* Pipe transaction counter and enable registers.       */
                                                                /* PIPEnTRE = PIPE_TR[n - 1u].TRE                       */
                                                                /* PIPEnTRN = PIPE_TR[n - 1u].TRN                       */
    CPU_REG16                  RSVD_44[43u];
    USBD_JX4L_DEV_ADDR         DEVADD[6u];                      /* Device address configuration register.               */
                                                                /* DEVADDn = DEVADD[n].NBR                              */
} USBD_JX4L_REG;


/*
*********************************************************************************************************
*                                     ENDPOINT TRANSFER STATE TYPE
* Note(s): (1) Since this controller cannot queue transfers, it must be able to know whether or not a
*              transfer is currently in progress and if it is possible to start a new one. This type is
*              used to indicate this state.
*********************************************************************************************************
*/

typedef  enum  usbd_drv_ep_xfer_state {
    USBD_DRV_EP_XFER_STATE_NONE = 0,                            /* No xfer in progress. A xfer can be started.          */
    USBD_DRV_EP_XFER_STATE_ABORTED,                             /* Xfer is being aborted, no xfer can be started.       */
    USBD_DRV_EP_XFER_STATE_IN_PROGRESS,                         /* Xfer is in progress, no other xfer can be started.   */
} USBD_DRV_EP_XFER_STATE;


/*
*********************************************************************************************************
*                                    SETUP PACKETS QUEUE DATA TYPES
*********************************************************************************************************
*/

typedef  struct  usbd_drv_setup_pkt_q {
                                                                /* Tbl containing the q'd setup pkts.                   */
                                                                /* Setup req buf: |Request|Value|Index|Length|          */
    CPU_INT16U  SetupPktTbl[USBD_DRV_SETUP_PKT_CIRCULAR_BUF_SIZE][4u];
    CPU_INT08U  IxIn;                                           /* Ix where to put the next rxd setup pkt.              */
    CPU_INT08U  IxOut;                                          /* Ix where to get the next setup pkt to give to core.  */
    CPU_INT08U  NbrSetupPkt;                                    /* Actual nbr of pkts in the buf.                       */
} USBD_DRV_SETUP_PKT_Q;


/*
*********************************************************************************************************
*                                           DRIVER DATA TYPE
*********************************************************************************************************
*/

typedef  struct  usbd_drv_reg {
    CPU_REG16            *SYSCFG;                               /* Configures USB module (host/dev, pull-up pins, etc). */
    CPU_REG16            *SYSSTS0;                              /* Status of the D+ and D- lines of the USB bus.        */
    CPU_REG16            *DVSTCTR0;                             /* Control and confirm the state of the USB data bus.   */
    CPU_REG16            *DMA0PCFG;
    CPU_REG16            *DMA1PCFG;
    CPU_REG16            *CFIFO;                                /* Read from or write data to FIFO.                     */
    CPU_REG16            *D0FIFO;                               /* Read from or write data to FIFO, DMA-capable.        */
    CPU_REG16            *D1FIFO;                               /* Read from or write data to FIFO, DMA-capable.        */
    CPU_REG16            *CFIFOSEL;                             /* Pipe and port settings for CFIFO.                    */
    CPU_REG16            *CFIFOCTR;                             /* Control register for CFIFO.                          */
    CPU_REG16            *D0FIFOSEL;                            /* Pipe and port settings for D0FIFO.                   */
    CPU_REG16            *D0FIFOCTR;                            /* Control register for D0FIFO.                         */
    CPU_REG16            *D1FIFOSEL;                            /* Pipe and port settings for D1FIFO.                   */
    CPU_REG16            *D1FIFOCTR;                            /* Control register for D1FIFO.                         */
    CPU_REG16            *INTENB0;                              /* Interrupt enable register 0.                         */
    CPU_REG16            *INTENB1;                              /* Interrupt enable register 1.                         */
    CPU_REG16            *BRDYENB;                              /* BRDY interrupt enable register.                      */
    CPU_REG16            *NRDYENB;                              /* NRDY interrupt enable register.                      */
    CPU_REG16            *BEMPENB;                              /* BEMP interrupt enable register.                      */
    CPU_REG16            *SOFCFG;                               /* BRDY interrupt status clear timing.                  */
    CPU_REG16            *INTSTS0;                              /* Interrupt status register 0.                         */
    CPU_REG16            *INTSTS1;                              /* Interrupt status register 1.                         */
    CPU_REG16            *BRDYSTS;                              /* BRDY interrupt status register.                      */
    CPU_REG16            *NRDYSTS;                              /* NRDY interrupt status register.                      */
    CPU_REG16            *BEMPSTS;                              /* BEMP interrupt status register.                      */
    CPU_REG16            *FRMNUM;                               /* Frame number register and isochronous errors status. */
    CPU_REG16            *USBADDR;                              /* USB device address and wake-up recovery.             */
    CPU_REG16            *USBREQ;                               /* USB request type register.                           */
    CPU_REG16            *USBVAL;                               /* USB request value register.                          */
    CPU_REG16            *USBINDX;                              /* USB request index register.                          */
    CPU_REG16            *USBLENG;                              /* USB request length register.                         */
    CPU_REG16            *DCPCFG;                               /* DCP configuration register.                          */
    CPU_REG16            *DCPMAXP;                              /* DCP maximum packet size register.                    */
    CPU_REG16            *DCPCTR;                               /* DCP control register.                                */
    CPU_REG16            *PIPESEL;                              /* Pipe select register.                                */
    CPU_REG16            *PIPECFG;                              /* Pipe configuration register.                         */
    CPU_REG16            *PIPEMAXP;                             /* Pipe maximum packet size register.                   */
    CPU_REG16            *PIPEPERI;                             /* Pipe cycle control register.                         */
    CPU_REG16            *PIPE_CTR[RX600_USB_PIPECTR_NBR];      /* Pipe control register.                               */
}  USBD_DRV_REG;


typedef  struct  usbd_drv_data {                                /* --------- DRIVER'S INTERNAL DATA STRUCTURE --------- */
    CPU_BOOLEAN              ZLP_TxDis;                         /* Indicates a ZLP should not be sent.                  */
    USBD_DRV_SETUP_PKT_Q     SetupPktQ;                         /* Setup pkt queuing buf.                               */
    CPU_BOOLEAN              CallTxCmpl;                        /* Flag indicating that USBD_EP_TxCmpl must be called.  */
    CPU_BOOLEAN              CallRxCmpl;                        /* Flag indicating that USBD_EP_RxCmpl must be called.  */
                                                                /* Max pkt size info tbl.                               */
    CPU_INT16U               MaxPktSize[(RX600_USB_PIPE_MAX_NBR + 1u)];
                                                                /* State of the xfer on the ep.                         */
    USBD_DRV_EP_XFER_STATE   EP_XferState[(RX600_USB_PIPE_MAX_NBR + 1u)];
    USBD_DRV_REG            *RegPtr;                            /* Driver register addresses                            */
} USBD_DRV_DATA;


#if (RX600_DBG_STATS_EN == DEF_ENABLED)
typedef  struct  usbd_dbg_stats_drv {
    CPU_INT08U  ISR_BRDY_Cnt[(RX600_USB_PIPE_MAX_NBR + 1u)];
    CPU_INT08U  ISR_BEMP_Cnt[(RX600_USB_PIPE_MAX_NBR + 1u)];

    CPU_INT08U  ISR_CTSQ_Cnt[RX600_USB_INTSTS0_CTSQ_CTRL_TX_SEQ_ERR + 1u];

    CPU_INT08U  InvalidXferStateRxStart;
    CPU_INT08U  InvalidXferStateTx;
    CPU_INT08U  InvalidXferStateTxZLP;
    CPU_INT08U  InvalidXferStateISR_CTSQ;

    CPU_INT08U  EP0_TxCmplCallFromTxZLP;
    CPU_INT08U  EP0_TxCmplCallFromISR_CTSQ;
    CPU_INT08U  EP0_TxCmplCallFromISR_BEMP;
} USBD_DBG_STATS_DRV;
#endif


/*
*********************************************************************************************************
*                                           GLOBAL VARIABLES
*********************************************************************************************************
*/

#if (RX600_DBG_STATS_EN == DEF_ENABLED)
    USBD_DBG_STATS_DRV  USBD_DbgStats;
#endif


/*
*********************************************************************************************************
*                              USB DEVICE CONTROLLER DRIVER API PROTOTYPES
*********************************************************************************************************
*/

static  void         USBD_DrvInitRX600  (USBD_DRV     *p_drv,
                                         USBD_ERR     *p_err);

static  void         USBD_DrvInitJX4L   (USBD_DRV     *p_drv,
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
*                                       LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_RX600_DrvEP_SetRespPID     (USBD_DRV_REG   *p_reg,
                                                      CPU_INT08U      ep_log_nbr,
                                                      CPU_INT08U      resp_pid);

static  void         USBD_RX600_DrvSetupPktClr       (USBD_DRV_DATA  *p_drv_data);

static  void         USBD_RX600_DrvSetupPktEnq       (USBD_DRV_DATA  *p_drv_data,
                                                      USBD_DRV_REG   *p_reg);

static  void         USBD_RX600_DrvSetupPktEnqSetAddr(USBD_DRV_DATA  *p_drv_data);

static  CPU_INT16U  *USBD_RX600_DrvSetupPktGet       (USBD_DRV_DATA  *p_drv_data);

static  void         USBD_RX600_DrvSetupPktDeq       (USBD_DRV_DATA  *p_drv_data);


/*
*********************************************************************************************************
*                                   USB DEVICE CONTROLLER DRIVER API
*********************************************************************************************************
*/

USBD_DRV_API  USBD_DrvAPI_RX600 = { USBD_DrvInitRX600,
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


USBD_DRV_API  USBD_DrvAPI_V850E2JX4L = { USBD_DrvInitJX4L,
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
*                                      DRIVER INTERFACE FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         USBD_DrvInitJX4L()
*
* Description : Initialize the device for the V850E2/Jx4L series.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE            Device successfully initialized.
*                               USBD_ERR_ALLOC           Memory allocation failed.
*
* Return(s)   : none.
*
* Note(s)     : (1) Since the CPU frequency could be higher than OTG module clock, a timeout is needed
*                   to reset the OTG controller successfully.
*********************************************************************************************************
*/

static  void  USBD_DrvInitJX4L (USBD_DRV  *p_drv,
                                USBD_ERR  *p_err)
{
    USBD_DRV_REG      *p_reg;
    USBD_JX4L_REG     *p_v850_reg;
    USBD_DRV_DATA     *p_drv_data;
    USBD_DRV_BSP_API  *p_bsp_api;
    CPU_INT08U         i;
    CPU_SIZE_T         reqd_octets;
    LIB_ERR            err_lib;


                                                                /* Alloc drv's internal data.                           */
    p_drv_data = (USBD_DRV_DATA *)Mem_HeapAlloc(sizeof(USBD_DRV_DATA),
                                                sizeof(CPU_DATA),
                                               &reqd_octets,
                                               &err_lib);
    if (p_drv_data == (void *)0) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

    p_reg = (USBD_DRV_REG *)Mem_HeapAlloc(sizeof(USBD_DRV_REG),
                                          sizeof(CPU_DATA),
                                          &reqd_octets,
                                          &err_lib);
    if (p_reg == (void *)0) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

    p_v850_reg = (USBD_JX4L_REG *)p_drv->CfgPtr->BaseAddr;     /* Get a ref to the USB hw reg.                         */

    p_reg->SYSCFG    = &p_v850_reg->SYSCFG;
    p_reg->SYSSTS0   = &p_v850_reg->SYSSTS0;
    p_reg->DVSTCTR0  = &p_v850_reg->DVSTCTR0;
    p_reg->DMA0PCFG  = &p_v850_reg->DMA0PCFG;
    p_reg->DMA1PCFG  = &p_v850_reg->DMA1PCFG;
    p_reg->CFIFO     = &p_v850_reg->CFIFO;
    p_reg->D0FIFO    = &p_v850_reg->D0FIFO;
    p_reg->D1FIFO    = &p_v850_reg->D1FIFO;
    p_reg->CFIFOSEL  = &p_v850_reg->CFIFOSEL;
    p_reg->CFIFOCTR  = &p_v850_reg->CFIFOCTR;
    p_reg->D0FIFOSEL = &p_v850_reg->D0FIFOSEL;
    p_reg->D0FIFOCTR = &p_v850_reg->D0FIFOCTR;
    p_reg->D1FIFOSEL = &p_v850_reg->D1FIFOSEL;
    p_reg->D1FIFOCTR = &p_v850_reg->D1FIFOCTR;
    p_reg->INTENB0   = &p_v850_reg->INTENB0;
    p_reg->INTENB1   = &p_v850_reg->INTENB1;
    p_reg->BRDYENB   = &p_v850_reg->BRDYENB;
    p_reg->NRDYENB   = &p_v850_reg->NRDYENB;
    p_reg->BEMPENB   = &p_v850_reg->BEMPENB;
    p_reg->SOFCFG    = &p_v850_reg->SOFCFG;
    p_reg->INTSTS0   = &p_v850_reg->INTSTS0;
    p_reg->INTSTS1   = &p_v850_reg->INTSTS1;
    p_reg->BRDYSTS   = &p_v850_reg->BRDYSTS;
    p_reg->NRDYSTS   = &p_v850_reg->NRDYSTS;
    p_reg->BEMPSTS   = &p_v850_reg->BEMPSTS;
    p_reg->FRMNUM    = &p_v850_reg->FRMNUM;
    p_reg->USBADDR   = &p_v850_reg->USBADDR;
    p_reg->USBREQ    = &p_v850_reg->USBREQ;
    p_reg->USBVAL    = &p_v850_reg->USBVAL;
    p_reg->USBINDX   = &p_v850_reg->USBINDX;
    p_reg->USBLENG   = &p_v850_reg->USBLENG;
    p_reg->DCPCFG    = &p_v850_reg->DCPCFG;
    p_reg->DCPMAXP   = &p_v850_reg->DCPMAXP;
    p_reg->DCPCTR    = &p_v850_reg->DCPCTR;
    p_reg->PIPESEL   = &p_v850_reg->PIPESEL;
    p_reg->PIPECFG   = &p_v850_reg->PIPECFG;
    p_reg->PIPEMAXP  = &p_v850_reg->PIPEMAXP;
    p_reg->PIPEPERI  = &p_v850_reg->PIPEPERI;

    for( i = 0u; i < RX600_USB_PIPECTR_NBR; i++) {
         p_reg->PIPE_CTR[i] = (CPU_REG16 *)&p_v850_reg->PIPE_CTR[i];
    }

    p_drv_data->RegPtr = p_reg;

    p_drv->DataPtr = p_drv_data;                                /* Store drv's internal data ptr.                       */

    p_bsp_api = (USBD_DRV_BSP_API *)p_drv->BSP_API_Ptr;         /* Get a ref to the drv's BSP API.                      */

    if (p_bsp_api->Init != (void *)0) {
        p_bsp_api->Init(p_drv);                                 /* Call board/chip specific dev ctrlr init fnct.        */
    }

    p_drv_data->ZLP_TxDis  = DEF_FALSE;                         /* Init internal data structures.                       */
    p_drv_data->CallTxCmpl = DEF_FALSE;
    p_drv_data->CallRxCmpl = DEF_FALSE;

    USBD_RX600_DrvSetupPktClr(p_drv_data);
    Mem_Clr((void     *)&(p_drv_data->MaxPktSize[0u]),          /* Clr EP max pkt size info tbl.                        */
                   sizeof(p_drv_data->MaxPktSize));

    Mem_Clr((void     *)&(p_drv_data->EP_XferState[0u]),        /* Clr EP xfer state info tbl.                          */
                   sizeof(p_drv_data->EP_XferState));

    USBD_DBG_STATS_RESET();

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        USBD_DrvInitRX600()
*
* Description : Initialize the device for the RX600/RX100 series.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE            Device successfully initialized.
*                               USBD_ERR_ALLOC           Memory allocation failed.
*
* Return(s)   : none.
*
* Note(s)     : (1) Since the CPU frequency could be higher than OTG module clock, a timeout is needed
*                   to reset the OTG controller successfully.
*********************************************************************************************************
*/

static  void  USBD_DrvInitRX600 (USBD_DRV  *p_drv,
                                 USBD_ERR  *p_err)
{
    USBD_DRV_REG      *p_reg;
    USBD_RX600_REG    *p_rx_reg;
    USBD_DRV_DATA     *p_drv_data;
    USBD_DRV_BSP_API  *p_bsp_api;
    CPU_INT08U         i;
    CPU_SIZE_T         reqd_octets;
    LIB_ERR            err_lib;


                                                                /* Alloc drv's internal data.                           */
    p_drv_data = (USBD_DRV_DATA *)Mem_HeapAlloc(sizeof(USBD_DRV_DATA),
                                                sizeof(CPU_DATA),
                                               &reqd_octets,
                                               &err_lib);
    if (p_drv_data == (void *)0) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

    p_reg = (USBD_DRV_REG *)Mem_HeapAlloc(sizeof(USBD_DRV_REG),
                                          sizeof(CPU_DATA),
                                          &reqd_octets,
                                          &err_lib);
    if (p_reg == (void *)0) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

    p_rx_reg = (USBD_RX600_REG *)p_drv->CfgPtr->BaseAddr;       /* Get a ref to the USB hw reg.                         */

    p_reg->SYSCFG    = &p_rx_reg->SYSCFG;
    p_reg->SYSSTS0   = &p_rx_reg->SYSSTS0;
    p_reg->DVSTCTR0  = &p_rx_reg->DVSTCTR0;
    p_reg->CFIFO     = &p_rx_reg->CFIFO;
    p_reg->D0FIFO    = &p_rx_reg->D0FIFO;
    p_reg->D1FIFO    = &p_rx_reg->D1FIFO;
    p_reg->CFIFOSEL  = &p_rx_reg->CFIFOSEL;
    p_reg->CFIFOCTR  = &p_rx_reg->CFIFOCTR;
    p_reg->D0FIFOSEL = &p_rx_reg->D0FIFOSEL;
    p_reg->D0FIFOCTR = &p_rx_reg->D0FIFOCTR;
    p_reg->D1FIFOSEL = &p_rx_reg->D1FIFOSEL;
    p_reg->D1FIFOCTR = &p_rx_reg->D1FIFOCTR;
    p_reg->INTENB0   = &p_rx_reg->INTENB0;
    p_reg->INTENB1   = &p_rx_reg->INTENB1;
    p_reg->BRDYENB   = &p_rx_reg->BRDYENB;
    p_reg->NRDYENB   = &p_rx_reg->NRDYENB;
    p_reg->BEMPENB   = &p_rx_reg->BEMPENB;
    p_reg->SOFCFG    = &p_rx_reg->SOFCFG;
    p_reg->INTSTS0   = &p_rx_reg->INTSTS0;
    p_reg->INTSTS1   = &p_rx_reg->INTSTS1;
    p_reg->BRDYSTS   = &p_rx_reg->BRDYSTS;
    p_reg->NRDYSTS   = &p_rx_reg->NRDYSTS;
    p_reg->BEMPSTS   = &p_rx_reg->BEMPSTS;
    p_reg->FRMNUM    = &p_rx_reg->FRMNUM;
    p_reg->USBADDR   = &p_rx_reg->USBADDR;
    p_reg->USBREQ    = &p_rx_reg->USBREQ;
    p_reg->USBVAL    = &p_rx_reg->USBVAL;
    p_reg->USBINDX   = &p_rx_reg->USBINDX;
    p_reg->USBLENG   = &p_rx_reg->USBLENG;
    p_reg->DCPCFG    = &p_rx_reg->DCPCFG;
    p_reg->DCPMAXP   = &p_rx_reg->DCPMAXP;
    p_reg->DCPCTR    = &p_rx_reg->DCPCTR;
    p_reg->PIPESEL   = &p_rx_reg->PIPESEL;
    p_reg->PIPECFG   = &p_rx_reg->PIPECFG;
    p_reg->PIPEMAXP  = &p_rx_reg->PIPEMAXP;
    p_reg->PIPEPERI  = &p_rx_reg->PIPEPERI;

    for( i = 0u; i < RX600_USB_PIPECTR_NBR; i++) {
         p_reg->PIPE_CTR[i] = (CPU_REG16 *)&p_rx_reg->PIPE_CTR[i];
    }

    p_drv_data->RegPtr = p_reg;

    p_drv->DataPtr = p_drv_data;                                /* Store drv's internal data ptr.                       */

    p_bsp_api = (USBD_DRV_BSP_API *)p_drv->BSP_API_Ptr;         /* Get a ref to the drv's BSP API.                      */

    if (p_bsp_api->Init != (void *)0) {
        p_bsp_api->Init(p_drv);                                 /* Call board/chip specific dev ctrlr init fnct.        */
    }

    p_drv_data->ZLP_TxDis  = DEF_FALSE;                         /* Init internal data structures.                       */
    p_drv_data->CallTxCmpl = DEF_FALSE;
    p_drv_data->CallRxCmpl = DEF_FALSE;

    USBD_RX600_DrvSetupPktClr(p_drv_data);
    Mem_Clr((void     *)&(p_drv_data->MaxPktSize[0u]),          /* Clr EP max pkt size info tbl.                        */
                   sizeof(p_drv_data->MaxPktSize));

    Mem_Clr((void     *)&(p_drv_data->EP_XferState[0u]),        /* Clr EP xfer state info tbl.                          */
                   sizeof(p_drv_data->EP_XferState));

    USBD_DBG_STATS_RESET();

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            USBD_DrvStart()
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
*                               USBD_ERR_NONE            Device successfully connected.
*
* Return(s)   : none.
*
* Note(s)     : Typically, the start function activates the pull-down on the D- pin to simulate
*               attachment to host. Some MCUs/MPUs have an internal pull-down that is activated by
*               a device controller register; for others, this may be a GPIO pin. Additionally,
*               interrupts for reset and suspend are activated.
*********************************************************************************************************
*/

static  void  USBD_DrvStart (USBD_DRV  *p_drv,
                             USBD_ERR  *p_err)
{
    USBD_DRV_REG      *p_reg;
    USBD_DRV_DATA     *p_drv_data;
    USBD_DRV_BSP_API  *p_bsp_api;


    p_drv_data = (USBD_DRV_DATA    *)p_drv->DataPtr;            /* Get a ref to the drv's internal data.                */
    p_reg      = (USBD_DRV_REG     *)p_drv_data->RegPtr;        /* Get a ref to the USB hw reg.                         */
    p_bsp_api  = (USBD_DRV_BSP_API *)p_drv->BSP_API_Ptr;        /* Get a ref to the drv's BSP API.                      */

    DEF_BIT_CLR(*p_reg->SYSCFG, RX600_USB_SYSCFG_USBE);         /* Dis USB module.                                      */

   *p_reg->SYSCFG = RX600_USB_SYSCFG_SCKE;                      /* Supply 48-MHz clk signal to the USB module.          */

    DEF_BIT_SET(*p_reg->SYSCFG, RX600_USB_SYSCFG_USBE);         /* En USB module.                                       */

   *p_reg->D0FIFOSEL = 0x00u;
   *p_reg->D1FIFOSEL = 0x00u;

    if (p_bsp_api->Conn != (void *)0) {
        p_bsp_api->Conn();                                      /* Call board/chip specific conn fnct.                  */
    }

                                                                /* Conn to host by activating the pull-up on D+ pin.    */
    if (DEF_BIT_IS_SET(*p_reg->SYSCFG, RX600_USB_SYSCFG_DPRPU) == DEF_TRUE) {
        *p_reg->SYSCFG &= ~(RX600_USB_SYSCFG_DPRPU);            /* If D+ pin is en, dis D+ pin.                         */
    }

    DEF_BIT_SET(*p_reg->SYSCFG, RX600_USB_SYSCFG_DPRPU);        /* En pulling up of the D+ line.                        */

   *p_reg->INTENB0 = (RX600_USB_INTENB0_VBSE  |                 /* En  'VBUS'                              int.         */
                      RX600_USB_INTENB0_RSME  |                 /* En  'Resume'                            int.         */
                      RX600_USB_INTENB0_DVSE  |                 /* En  'Device State Transition'           int.         */
                      RX600_USB_INTENB0_CTRE  |                 /* En  'Control Transfer Stage Transition' int.         */
                      RX600_USB_INTENB0_BEMPE |                 /* En  'Buffer Empty'                      int.         */
                      RX600_USB_INTENB0_BRDYE);                 /* En  'Buffer Ready'                      int.         */
   *p_reg->BRDYENB =  DEF_BIT_NONE;                             /* Clr 'Buffer Ready'                      int reg.     */
   *p_reg->NRDYENB =  DEF_BIT_NONE;                             /* Clr 'Buffer Not Ready'                  int reg.     */
   *p_reg->BEMPENB =  DEF_BIT_NONE;                             /* Clr 'Buffer Empty'                      int reg.     */

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            USBD_DrvStop()
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
    USBD_DRV_REG      *p_reg;
    USBD_DRV_DATA     *p_drv_data;
    USBD_DRV_BSP_API  *p_bsp_api;


    p_drv_data = (USBD_DRV_DATA    *)p_drv->DataPtr;            /* Get a ref to the drv's internal data.                */
    p_reg      = (USBD_DRV_REG     *)p_drv_data->RegPtr;        /* Get a ref to the USB hw reg.                         */
    p_bsp_api  = (USBD_DRV_BSP_API *)p_drv->BSP_API_Ptr;        /* Get a ref to the drv's BSP API.                      */

    if (p_bsp_api->Disconn != (void *)0) {
        p_bsp_api->Disconn();
    }

   *p_reg->INTENB0 = (RX600_USB_INTENB0_VBSE |                  /* En 'VBUS'                    int.                    */
                      RX600_USB_INTENB0_RSME |                  /* En 'Resume'                  int.                    */
                      RX600_USB_INTENB0_DVSE);                  /* En 'Device State Transition' int.                    */
    DEF_BIT_CLR(*p_reg->SYSCFG, RX600_USB_SYSCFG_DPRPU);        /* Dis pulling up of the D+ line.                       */
}


/*
*********************************************************************************************************
*                                           USBD_DrvAddrSet()
*
* Description : Assign an address to device.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               dev_addr    Device address assigned by the host.
*
* Return(s)   : DEF_OK,   if NO error(s),
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

    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;               /* Get a ref to the drv's internal data.                */
    p_drv_data->ZLP_TxDis = DEF_TRUE;                           /* No ZLP to Tx for SET_ADDRESS (see Note #3).          */

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                           USBD_DrvAddrEn()
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

    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;               /* Get a ref to the drv's internal data.                */
    p_drv_data->ZLP_TxDis = DEF_FALSE;                          /* Re-en Tx ZLP (see Note #3).                          */
}


/*
*********************************************************************************************************
*                                           USBD_DrvCfgSet()
*
* Description : Bring device into configured state.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               cfg_val     Configuration value.
*
* Return(s)   : DEF_OK,   if NO error(s),
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
*                                           USBD_DrvCfgClr()
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
*                                         USBD_DrvGetFrameNbr()
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
    USBD_DRV_REG   *p_reg;
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT16U      frame_nbr;


    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;               /* Get a ref to the drv's internal data.                */
    p_reg      = (USBD_DRV_REG  *)p_drv_data->RegPtr;           /* Get a ref to the USB hw reg.                         */
    frame_nbr  = (CPU_INT16U     )(*p_reg->FRMNUM & RX600_USB_FRMNUM_FRNM);

    return (frame_nbr);
}


/*
*********************************************************************************************************
*                                           USBD_DrvEP_Open()
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
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                       USBD_ERR_NONE            Endpoint successfully opened.
*                                       USND_ERR_FAIL            Endpoint open failed.
*                                       USBD_ERR_EP_INVALID_ARG  Invalid endpoint type.
*
* Return(s)   : none.
*
* Note(s)     : (1) Typically, the endpoint open function performs the following operations:
*
*                   (a) Validate endpoint address, type and maximum packet size.
*                   (b) Configure endpoint information in the device controller. This may include not
*                       only assigning the type and maximum packet size, but also making certain that
*                       the endpoint is successfully configured (or realized or mapped). For some
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
    USBD_DRV_REG   *p_reg;
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT16U      reg_val;
    CPU_INT08U      ep_log_nbr;
    CPU_INT08U      ep_ix_nbr;
    CPU_REG16       reg_to;
    CPU_BOOLEAN     valid;
    CPU_SR_ALLOC();


    (void)transaction_frame;

    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;               /* Get a ref to the drv's internal data.                */
    p_reg      = (USBD_DRV_REG  *)p_drv_data->RegPtr;           /* Get a ref to the USB hw reg.                         */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    ep_ix_nbr  =  RX600_EP_PHY_TO_EP_TBL_IX(USBD_EP_ADDR_TO_PHY(ep_addr));

    if (ep_log_nbr == 0u) {
        reg_val = 0x00u;
        DEF_BIT_SET(reg_val, RX600_USB_DCPCTR_SQCLR);
        DEF_BIT_SET(reg_val, RX600_USB_DCPCTR_CCPL);
    } else {
        switch (ep_type) {                                      /* Cfg pipe settings depending on pipe type.            */
            case USBD_EP_TYPE_ISOC:
                 reg_val = RX600_USB_PIPECFG_TYPE_ISOC;         /* Set pipe as isoc.                                    */
                 break;


            case USBD_EP_TYPE_INTR:
                 reg_val = RX600_USB_PIPECFG_TYPE_INTR;         /* Set pipe as intr.                                    */
                 break;


            case USBD_EP_TYPE_BULK:
                 reg_val = RX600_USB_PIPECFG_TYPE_BULK;         /* Set pipe as bulk.                                    */
                 break;


            case USBD_EP_TYPE_CTRL:
            default:
                *p_err = USBD_ERR_INVALID_ARG;
                 return;
        }

        if (USBD_EP_IS_IN(ep_addr) == DEF_TRUE) {
            DEF_BIT_SET(reg_val, RX600_USB_PIPECFG_DIR);
        }

                                                                /* Set EP nbr.                                          */
        reg_val |= (ep_log_nbr & RX600_USB_PIPECFG_EPNUM);

        if ((ep_addr & USBD_EP_DIR_MASK) == USBD_EP_DIR_OUT) {
            DEF_BIT_SET(reg_val, RX600_USB_PIPECFG_SHTNAK);     /* Dis pipe on EOT for OUT EP.                          */
        } else {
            DEF_BIT_SET(reg_val, RX600_USB_PIPECFG_BFRE);       /* Don't generate BRDY int on tx completion.            */
        }
    }

    CPU_CRITICAL_ENTER();
   *p_reg->PIPESEL = (ep_log_nbr & RX600_USB_PIPESEL_MASK);     /* Sel pipe nbr to apply settings to.                   */

    valid = USBD_RX600_DrvEP_SetRespPID(p_reg,                  /* Set EP resp PID.                                     */
                                        ep_log_nbr,
                                        RX600_USB_DCPCTR_PID_NAK);
    if (valid != DEF_OK) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_FAIL;
        return;
    }

    if (ep_log_nbr == 0u) {
       *p_reg->DCPCFG   = 0x00u;
       *p_reg->DCPMAXP &= max_pkt_size;
       *p_reg->DCPCTR   = reg_val;
        DEF_BIT_SET(*p_reg->CFIFOCTR, RX600_USB_FIFOCTR_BCLR);
        DEF_BIT_SET(*p_reg->BRDYENB, 0x01u);                    /* En 'Buffer Ready' int for ctrl pipe.                 */
        DEF_BIT_SET(*p_reg->BEMPENB, 0x01u);                    /* En 'Buffer Empty' int for ctrl pipe.                 */
    } else {
        reg_to = RX600_USB_REG_TO;
        do {
           *p_reg->CFIFOSEL &= (0x00 & RX600_USB_CFIFOSEL_CURPIPE);
            reg_to--;
        } while (((*p_reg->CFIFOSEL & RX600_USB_CFIFOSEL_CURPIPE) != 0x00) &&
                  (reg_to > 0u));
        if (reg_to == 0u) {
            CPU_CRITICAL_EXIT();
           *p_err = USBD_ERR_FAIL;
            return;
        }

       *p_reg->PIPEPERI =  DEF_CLR;
       *p_reg->PIPECFG  =  reg_val;
       *p_reg->PIPEMAXP =  max_pkt_size;                        /* Set max pkt size.                                    */
       *p_reg->PIPESEL  =  0x00u;                               /* Sel no pipe.                                         */
        if ((ep_addr & USBD_EP_DIR_MASK) == USBD_EP_DIR_OUT) {

            DEF_BIT_SET(*p_reg->BRDYENB, DEF_BIT(ep_log_nbr));  /* En rx int.                                           */
        } else {
            DEF_BIT_SET(*p_reg->BEMPENB, DEF_BIT(ep_log_nbr));  /* En tx int.                                           */
        }
    }

    p_drv_data->MaxPktSize[ep_ix_nbr]   = max_pkt_size;
    p_drv_data->EP_XferState[ep_ix_nbr] = USBD_DRV_EP_XFER_STATE_NONE;

    CPU_CRITICAL_EXIT();

   *p_err = USBD_ERR_NONE;
    return;
}


/*
*********************************************************************************************************
*                                          USBD_DrvEP_Close()
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
    USBD_DRV_REG   *p_reg;
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT08U      ep_log_nbr;
    CPU_INT08U      ep_ix_nbr;
    CPU_SR_ALLOC();


    p_drv_data = (USBD_DRV_DATA  *)p_drv->DataPtr;              /* Get a ref to the drv's internal data.                */
    p_reg      = (USBD_DRV_REG   *)p_drv_data->RegPtr;          /* Get a ref to the USB hw reg.                         */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    ep_ix_nbr  =  RX600_EP_PHY_TO_EP_TBL_IX(USBD_EP_ADDR_TO_PHY(ep_addr));

    CPU_CRITICAL_ENTER();
    (void)USBD_RX600_DrvEP_SetRespPID(p_reg,                    /* Set the EP resp PID.                                 */
                                      ep_log_nbr,
                                      RX600_USB_DCPCTR_PID_NAK);

    if (ep_log_nbr == 0u) {
        DEF_BIT_SET(*p_reg->DCPCTR, RX600_USB_DCPCTR_SQCLR);
    } else {
        DEF_BIT_SET(*p_reg->PIPE_CTR[ep_log_nbr - 1u], RX600_USB_PIPECTR_SQCLR);
    }

   *p_reg->PIPESEL = ep_log_nbr;                                /* Sel pipe to be closed.                               */

    if (ep_log_nbr > 0u) {
        if ((*p_reg->PIPE_CTR[ep_log_nbr - 1u] & RX600_USB_PIPECTR_PID) > 0u) {
            DEF_BIT_CLR(*p_reg->PIPE_CTR[ep_log_nbr - 1u], RX600_USB_PIPECTR_PID);
        }
       *p_reg->PIPE_CTR[ep_log_nbr - 1u] = DEF_CLR;
    }

   *p_reg->PIPECFG  = DEF_CLR;                                  /* Set all vals to dflt vals.                           */
   *p_reg->PIPEMAXP = DEF_CLR;
   *p_reg->PIPEPERI = DEF_CLR;
   *p_reg->PIPESEL  = 0x00u;                                    /* Set pipe sel to no selected pipe.                    */

    DEF_BIT_CLR(*p_reg->BRDYENB, DEF_BIT(ep_log_nbr));
    DEF_BIT_CLR(*p_reg->BEMPENB, DEF_BIT(ep_log_nbr));

    p_drv_data->MaxPktSize[ep_ix_nbr]   = 0u;
    p_drv_data->EP_XferState[ep_ix_nbr] = USBD_DRV_EP_XFER_STATE_NONE;

    CPU_CRITICAL_EXIT();

    USBD_DBG_DRV_EP("  Drv EP FIFO Close", ep_addr);
}


/*
*********************************************************************************************************
*                                         USBD_DrvEP_RxStart()
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
*                               USBD_ERR_NONE            Receive successfully configured.
*                               USBD_ERR_RX              Generic Rx error.
*                               USBD_ERR_DRV_EP_BUSY     A transfer is already in progress on this endpoint.
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
    USBD_DRV_REG   *p_reg;
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT08U      ep_log_nbr;
    CPU_INT08U      ep_ix_nbr;
    CPU_INT16U      max_pkt_size;
    CPU_INT32U      xfer_size;
    CPU_BOOLEAN     valid;
    CPU_SR_ALLOC();


    (void)p_buf;

    p_drv_data   = (USBD_DRV_DATA  *)p_drv->DataPtr;            /* Get a ref to the drv's internal data.                */
    p_reg        = (USBD_DRV_REG   *)p_drv_data->RegPtr;        /* Get a ref to the USB hw reg.                         */
    ep_log_nbr   =  USBD_EP_ADDR_TO_LOG(ep_addr);
    ep_ix_nbr    =  RX600_EP_PHY_TO_EP_TBL_IX(USBD_EP_ADDR_TO_PHY(ep_addr));

    max_pkt_size =  p_drv_data->MaxPktSize[ep_ix_nbr];
    xfer_size    =  DEF_MIN(buf_len, max_pkt_size);
   *p_err        =  USBD_ERR_EP_QUEUING;

    CPU_CRITICAL_ENTER();
    if ((p_drv_data->EP_XferState[ep_ix_nbr] == USBD_DRV_EP_XFER_STATE_NONE) ||
        (p_drv_data->EP_XferState[ep_ix_nbr] == USBD_DRV_EP_XFER_STATE_ABORTED)) {

        valid = USBD_RX600_DrvEP_SetRespPID(p_reg,              /* Set the EP resp PID.                                 */
                                            ep_log_nbr,
                                            RX600_USB_DCPCTR_PID_BUF);
        if (valid == DEF_OK) {
           *p_err = USBD_ERR_NONE;
            p_drv_data->EP_XferState[ep_ix_nbr] = USBD_DRV_EP_XFER_STATE_IN_PROGRESS;
        } else {
           *p_err = USBD_ERR_RX;
        }
    } else {
        USBD_DBG_STATS_INC(InvalidXferStateRxStart);
    }

    if ((ep_log_nbr             == 0u) &&
        (p_drv_data->CallRxCmpl == DEF_TRUE)) {
        USBD_EP_RxCmpl(p_drv, 0u);
        p_drv_data->CallRxCmpl = DEF_FALSE;
    }

    CPU_CRITICAL_EXIT();

    return (xfer_size);
}


/*
*********************************************************************************************************
*                                            USBD_DrvEP_Rx()
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
*                               USBD_ERR_NONE            Data successfully received.
*                               USBD_ERR_RX              Generic Rx error.
*
* Return(s)   : Number of bytes received, if NO error(s),
*
*               0,                        otherwise.
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
    USBD_DRV_REG   *p_reg;
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT16U     *pbuf16;
    CPU_INT16U      max_pkt_size;
    CPU_INT16U      bytes_rxd;
    CPU_INT16U      pkt_len;
    CPU_INT16U      pipe_ctrl;
    CPU_INT16U      fifo_sel;
    CPU_INT16U      fifo_sel_masked;
    CPU_INT08U      ep_log_nbr;
    CPU_INT08U      ep_ix_nbr;
    CPU_BOOLEAN     rd_byte;
    CPU_REG16       reg_to;
    CPU_BOOLEAN     valid;
    CPU_SR_ALLOC();


    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;               /* Get a ref to the drv's internal data.                */
    p_reg      = (USBD_DRV_REG  *)p_drv_data->RegPtr;           /* Get a ref to the USB hw reg.                         */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    ep_ix_nbr  =  RX600_EP_PHY_TO_EP_TBL_IX(USBD_EP_ADDR_TO_PHY(ep_addr));
    bytes_rxd  =  0u;
    pbuf16     = (CPU_INT16U *)p_buf;
    rd_byte    =  DEF_FALSE;
   *p_err      =  USBD_ERR_NONE;
    fifo_sel   = (ep_log_nbr & RX600_USB_CFIFOSEL_CURPIPE);     /* Sel pipe to be rd from.                              */

    DEF_BIT_SET(fifo_sel, RX600_USB_CFIFOSEL_MBW);              /* Port access 16-bit width.                            */
    DEF_BIT_SET(fifo_sel, RX600_USB_CFIFOSEL_RCNT);             /* Dec data len on rd.                                  */
#if (CPU_CFG_ENDIAN_TYPE == CPU_ENDIAN_TYPE_BIG)
    DEF_BIT_SET(fifo_sel, RX600_USB_CFIFOSEL_BIGEND);
#endif

    CPU_CRITICAL_ENTER();
    valid = USBD_RX600_DrvEP_SetRespPID(p_reg,                  /* Set EP resp PID.                                     */
                                        ep_log_nbr,
                                        RX600_USB_DCPCTR_PID_NAK);
    if (valid != DEF_OK) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_RX;
        return (0u);
    }

    reg_to          =  RX600_USB_REG_TO;
    fifo_sel_masked = (fifo_sel & RX600_USB_CFIFOSEL_CURPIPE);
    do {                                                        /* Wait until pipe is correctly selected.               */
       *p_reg->CFIFOSEL = fifo_sel;
        reg_to--;
    } while (((*p_reg->CFIFOSEL & RX600_USB_CFIFOSEL_CURPIPE) != fifo_sel_masked) &&
              (reg_to > 0u));
    if (reg_to == 0u) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_RX;
        return (0u);
    }

    if (buf_len > 0u) {
        reg_to = RX600_USB_REG_TO;
        if (ep_log_nbr == 0u) {
            do {
                pipe_ctrl = *p_reg->DCPCTR;                     /* Chk that FIFO is rdy to be accessed.                 */
                reg_to--;
            } while (((pipe_ctrl & RX600_USB_DCPCTR_BSTS) != RX600_USB_DCPCTR_BSTS) &&
                      (reg_to > 0u));
        } else {
            do {
                pipe_ctrl = *p_reg->PIPE_CTR[ep_log_nbr - 1u];  /* Chk that FIFO is rdy to be accessed.                 */
                reg_to--;
            } while (((pipe_ctrl & RX600_USB_PIPECTR_BSTS) != RX600_USB_PIPECTR_BSTS) &&
                      (reg_to > 0u));
        }
        if (reg_to == 0u) {
            CPU_CRITICAL_EXIT();
           *p_err = USBD_ERR_RX;
            return (0u);
        }

        pkt_len = (*p_reg->CFIFOCTR & RX600_USB_FIFOCTR_DTLN);  /* Get pkt len.                                         */

        if (pkt_len == 0u) {                                    /* Clr FIFO for zero pkt rd.                            */
            DEF_BIT_SET(*p_reg->CFIFOCTR, RX600_USB_FIFOCTR_BCLR);
        } else {
            max_pkt_size = p_drv_data->MaxPktSize[ep_ix_nbr];
            buf_len      = DEF_MIN(buf_len, max_pkt_size);
            if (pkt_len <= buf_len) {
               *p_err = USBD_ERR_NONE;
            } else {
               *p_err = USBD_ERR_RX;                            /* Unable to rd the whole pkt.                          */
            }

            buf_len = DEF_MIN(buf_len, pkt_len);

            if ((buf_len % 2u) != 0u) {                         /* Chk if len is a multiple of a word len.              */
                buf_len--;                                      /* An extra single byte needs to be rd.                 */
                rd_byte = DEF_TRUE;
            }
            while (bytes_rxd < buf_len) {
               *pbuf16++   = *p_reg->CFIFO;                     /* Rd data from the FIFO one word at a time.            */
                bytes_rxd += 2u;
            }

            if (rd_byte == DEF_TRUE) {                          /* If an extra byte is to be rd.                        */
                p_buf = (CPU_INT08U *)pbuf16;
               *p_buf =  MEM_VAL_GET_INT08U(RX600_USB_FIFO_SINGLE_BYTE_ACCESS(*p_reg->CFIFO));
                bytes_rxd++;
            }

            if (*p_err != USBD_ERR_NONE) {
                                                                /* Clr pipe's FIFO.                                     */
                DEF_BIT_SET(*p_reg->CFIFOCTR, RX600_USB_FIFOCTR_BCLR);
            }
        }
    } else {
        DEF_BIT_SET(*p_reg->CFIFOCTR, RX600_USB_FIFOCTR_BCLR);  /* Clr FIFO for zero pkt rd.                            */
    }
                                                                /* No xfer is in progress.                              */
    p_drv_data->EP_XferState[ep_ix_nbr] = USBD_DRV_EP_XFER_STATE_NONE;
    CPU_CRITICAL_EXIT();

    return (bytes_rxd);
}


/*
*********************************************************************************************************
*                                          USBD_DrvEP_RxZLP()
*
* Description : Receive zero-length packet from endpoint.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_addr     Endpoint address.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE            Zero-length packet successfully received.
*                               USBD_ERR_RX              Generic Rx error.
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
    USBD_DRV_REG   *p_reg;
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT16U     *p_setup_buf;
    CPU_INT08U      ep_log_nbr;
    CPU_INT08U      ep_ix_nbr;
    CPU_SR_ALLOC();


    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;               /* Get a ref to the drv's internal data.                */
    p_reg      = (USBD_DRV_REG  *)p_drv_data->RegPtr;           /* Get a ref to the USB hw reg.                         */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    ep_ix_nbr  =  RX600_EP_PHY_TO_EP_TBL_IX(USBD_EP_ADDR_TO_PHY(ep_addr));

    if (ep_log_nbr == 0u) {
        CPU_CRITICAL_ENTER();

        USBD_RX600_DrvSetupPktDeq(p_drv_data);

        USBD_RX600_DrvEP_SetRespPID(p_reg,                      /* Set EP resp PID.                                     */
                                    ep_log_nbr,
                                    RX600_USB_DCPCTR_PID_BUF);

        DEF_BIT_SET(*p_reg->DCPCTR, RX600_USB_DCPCTR_CCPL);     /* En the status stage of the ctrl xfer to be cmpl.     */

        if (p_drv_data->SetupPktQ.NbrSetupPkt != 0u) {
                                                                /* Deq the next setup pkt and post it to the core.      */
            p_setup_buf = USBD_RX600_DrvSetupPktGet(p_drv_data);
            USBD_EventSetup(        p_drv,                      /* Post setup pkt to the core.                          */
                            (void *)p_setup_buf);
        }

        p_drv_data->EP_XferState[ep_ix_nbr] = USBD_DRV_EP_XFER_STATE_NONE;
        CPU_CRITICAL_EXIT();

       *p_err = USBD_ERR_NONE;
    } else {
                                                                /* Rd ZLP.                                              */
        USBD_DrvEP_Rx(        p_drv,
                              ep_addr,
                      (void *)0,
                              0u,
                              p_err);
    }
}


/*
*********************************************************************************************************
*                                            USBD_DrvEP_Tx()
*
* Description : Configure endpoint with buffer to transmit data.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_addr     Endpoint address.
*
*               p_buf       Pointer to buffer of data that will be transmitted.
*
*               buf_len     Number of bytes to transmit.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE            Transmit successfully configured.
*                               USBD_ERR_DRV_EP_BUSY     A transfer is already in progress on this endpoint.
*
* Return(s)   : Number of bytes transmitted, if NO error(s),
*
*               0,                           otherwise.
*
* Note(s)     : (1) Control transfers are terminated by setting the DCPCTR.CCPL bit to 1 while the
*                   DCPCTR.PID[1:0] bits are set to BUF.
*********************************************************************************************************
*/

static  CPU_INT32U  USBD_DrvEP_Tx (USBD_DRV    *p_drv,
                                   CPU_INT08U   ep_addr,
                                   CPU_INT08U  *p_buf,
                                   CPU_INT32U   buf_len,
                                   USBD_ERR    *p_err)
{
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT16U      tx_len;
    CPU_INT08U      ep_ix_nbr;
    CPU_SR_ALLOC();

    (void)p_buf;

    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;               /* Get a ref to the drv's internal data.                */
    ep_ix_nbr  =  RX600_EP_PHY_TO_EP_TBL_IX(USBD_EP_ADDR_TO_PHY(ep_addr));

    tx_len     =  DEF_MIN(p_drv_data->MaxPktSize[ep_ix_nbr], buf_len);
   *p_err      =  USBD_ERR_EP_QUEUING;

    CPU_CRITICAL_ENTER();
    if ((p_drv_data->EP_XferState[ep_ix_nbr] == USBD_DRV_EP_XFER_STATE_NONE) ||
        (p_drv_data->EP_XferState[ep_ix_nbr] == USBD_DRV_EP_XFER_STATE_ABORTED)) {
       *p_err = USBD_ERR_NONE;
        p_drv_data->EP_XferState[ep_ix_nbr] = USBD_DRV_EP_XFER_STATE_IN_PROGRESS;
    } else {
        USBD_DBG_STATS_INC(InvalidXferStateTx);
    }
    CPU_CRITICAL_EXIT();

    return (tx_len);
}


/*
*********************************************************************************************************
*                                         USBD_DrvEP_TxStart()
*
* Description : Transmit the specified amount of data to device endpoint.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_addr     Endpoint address.
*
*               p_buf       Pointer to buffer of data that will be transmitted.
*
*               buf_len     Number of bytes to transmit.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE            Data successfully transmitted.
*                               USBD_ERR_TX              Generic Tx error.
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
    USBD_DRV_REG   *p_reg;
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT32U      bytes_txd;
    CPU_INT16U     *pbuf16;
    CPU_INT16U      max_pkt_size;
    CPU_INT16U      pipe_ctrl;
    CPU_INT16U      fifo_sel;
    CPU_INT16U      fifo_sel_masked;
    CPU_INT08U      ep_log_nbr;
    CPU_INT08U      ep_ix_nbr;
    CPU_BOOLEAN     wr_byte;
    CPU_REG16       reg_to;
    CPU_BOOLEAN     valid;
    CPU_SR_ALLOC();


    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;               /* Get a ref to the drv's internal data.                */
    p_reg      = (USBD_DRV_REG  *)p_drv_data->RegPtr;           /* Get a ref to the USB hw reg.                         */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    ep_ix_nbr  =  RX600_EP_PHY_TO_EP_TBL_IX(USBD_EP_ADDR_TO_PHY(ep_addr));
    bytes_txd  =  0u;
    pbuf16     = (CPU_INT16U *)p_buf;
    wr_byte    =  DEF_FALSE;

    fifo_sel = 0x00u;
    DEF_BIT_SET(fifo_sel, RX600_USB_CFIFOSEL_MBW);              /* Port access 16-bit width.                            */
    if (ep_log_nbr == 0u) {
        DEF_BIT_SET(fifo_sel, RX600_USB_CFIFOSEL_ISEL);
    }
#if (CPU_CFG_ENDIAN_TYPE == CPU_ENDIAN_TYPE_BIG)
    DEF_BIT_SET(fifo_sel, RX600_USB_CFIFOSEL_BIGEND);
#endif
                                                                /* Sel pipe to be rd from.                              */
    DEF_BIT_SET(fifo_sel, (ep_log_nbr & RX600_USB_CFIFOSEL_CURPIPE));

    reg_to          =  RX600_USB_REG_TO;
    fifo_sel_masked = (fifo_sel & (RX600_USB_CFIFOSEL_CURPIPE | RX600_USB_CFIFOSEL_ISEL));

    CPU_CRITICAL_ENTER();
    p_drv_data->EP_XferState[ep_ix_nbr] = USBD_DRV_EP_XFER_STATE_IN_PROGRESS;

    valid = USBD_RX600_DrvEP_SetRespPID(p_reg,                  /* Set EP resp PID.                                     */
                                        ep_log_nbr,
                                        RX600_USB_DCPCTR_PID_NAK);
    if (valid != DEF_OK) {
        p_drv_data->EP_XferState[ep_ix_nbr] = USBD_DRV_EP_XFER_STATE_NONE;
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_TX;
        return;
    }

    do {
       *p_reg->CFIFOSEL = fifo_sel;
        reg_to--;
    } while (((*p_reg->CFIFOSEL & (RX600_USB_CFIFOSEL_CURPIPE | RX600_USB_CFIFOSEL_ISEL)) != fifo_sel_masked) &&
              (reg_to > 0u));
    if (reg_to == 0u) {
        p_drv_data->EP_XferState[ep_ix_nbr] = USBD_DRV_EP_XFER_STATE_NONE;
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_TX;
        return;
    }

    if (ep_log_nbr == 0u) {
        reg_to = RX600_USB_REG_TO;
        do {
            pipe_ctrl = *p_reg->DCPCTR;                         /* Chk that FIFO is rdy to be accessed.                 */
            reg_to--;
        } while (((pipe_ctrl & RX600_USB_DCPCTR_BSTS) != RX600_USB_DCPCTR_BSTS) &&
                  (reg_to > 0u));
        if (reg_to == 0u) {
            p_drv_data->EP_XferState[ep_ix_nbr] = USBD_DRV_EP_XFER_STATE_NONE;
            CPU_CRITICAL_EXIT();
           *p_err = USBD_ERR_TX;
            return;
        }
    }

    max_pkt_size = p_drv_data->MaxPktSize[ep_ix_nbr];
    buf_len      = DEF_MIN(buf_len, max_pkt_size);

    if (buf_len == 0u) {
       *p_reg->CFIFOCTR |= (RX600_USB_FIFOCTR_BVAL |
                            RX600_USB_FIFOCTR_BCLR);
    }

    if ((buf_len % 2u) != 0u) {                                 /* Chk if pkt len is a multiple of a word len.          */
        buf_len--;
        wr_byte = DEF_TRUE;                                     /* An extra single byte needs to be written.            */
    }

    while (bytes_txd < buf_len) {
       *p_reg->CFIFO = *pbuf16++;                               /* Wr data into the FIFO one word at a time.            */
        bytes_txd += 2u;
    }

    if (wr_byte == DEF_TRUE) {
        DEF_BIT_CLR(*p_reg->CFIFOSEL, RX600_USB_CFIFOSEL_MBW);  /* Port access 8-bit width.                             */
        p_buf = (CPU_INT08U *)pbuf16;
       *p_reg->CFIFO = *p_buf;
        bytes_txd++;
    }

    if (bytes_txd != max_pkt_size) {
        DEF_BIT_SET(*p_reg->CFIFOCTR, RX600_USB_FIFOCTR_BVAL);  /* Data has been completely written to the FIFO buf.    */
    }
    valid = USBD_RX600_DrvEP_SetRespPID(p_reg,                  /* Set EP resp PID.                                     */
                                        ep_log_nbr,
                                        RX600_USB_DCPCTR_PID_BUF);
    CPU_CRITICAL_EXIT();

    if (valid == DEF_OK) {
       *p_err = USBD_ERR_NONE;
    } else {
       *p_err = USBD_ERR_TX;
    }

    USBD_DBG_DRV_EP_ARG("  Drv EP FIFO Tx Len:", ep_addr, bytes_txd);

    return;
}


/*
*********************************************************************************************************
*                                          USBD_DrvEP_TxZLP()
*
* Description : Transmit zero-length packet from endpoint.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_addr     Endpoint address.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE            Zero-length packet successfully transmitted.
*                               USBD_ERR_TX              Generic Tx error.
*
* Return(s)   : none.
*
* Note(s)     : (1) The ZLP_TxDis variable is used to prevent the transmission of a zero-length packet
*                   when handling a SET_ADDRESS standard request since the USB module already sent one.
*                   ZLP_TxDis is set in AddrSet(), called before EP_TxZLP(), and cleared in AddrEn(),
*                   called after EP_TxZLP().
*********************************************************************************************************
*/

static  void  USBD_DrvEP_TxZLP (USBD_DRV    *p_drv,
                                CPU_INT08U   ep_addr,
                                USBD_ERR    *p_err)
{
    USBD_DRV_REG   *p_reg;
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT16U     *p_setup_buf;
    CPU_INT08U      ep_log_nbr;
    CPU_INT08U      ep_ix_nbr;
    CPU_SR_ALLOC();


    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;               /* Get a ref to the drv's internal data.                */
    p_reg      = (USBD_DRV_REG  *)p_drv_data->RegPtr;           /* Get a ref to the USB hw reg.                         */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    ep_ix_nbr  =  RX600_EP_PHY_TO_EP_TBL_IX(USBD_EP_ADDR_TO_PHY(ep_addr));

    if (ep_log_nbr == 0u) {
        CPU_CRITICAL_ENTER();

        USBD_RX600_DrvSetupPktDeq(p_drv_data);
        if (p_drv_data->ZLP_TxDis == DEF_FALSE) {               /* Do not send a ZLP if the flag is set (see Note #1).  */
            if ((p_drv_data->EP_XferState[(ep_ix_nbr)] == USBD_DRV_EP_XFER_STATE_NONE) ||
                (p_drv_data->EP_XferState[(ep_ix_nbr)] == USBD_DRV_EP_XFER_STATE_ABORTED)) {
                p_drv_data->EP_XferState[ep_ix_nbr] = USBD_DRV_EP_XFER_STATE_IN_PROGRESS;

                (void)USBD_RX600_DrvEP_SetRespPID(p_reg,            /* Set EP resp PID.                                     */
                                                  ep_log_nbr,
                                                  RX600_USB_DCPCTR_PID_BUF);

                DEF_BIT_SET(*p_reg->DCPCTR, RX600_USB_DCPCTR_CCPL);
            } else {
                USBD_DBG_STATS_INC(InvalidXferStateTxZLP);
            }
        } else {
            p_drv_data->EP_XferState[ep_ix_nbr] = USBD_DRV_EP_XFER_STATE_NONE;

            USBD_EP_TxCmpl(p_drv, 0u);
            USBD_DBG_STATS_INC(EP0_TxCmplCallFromTxZLP)
        }
        if (p_drv_data->SetupPktQ.NbrSetupPkt != 0u) {
                                                                /* Deq the next setup pkt and post it to the core.      */
            p_setup_buf = USBD_RX600_DrvSetupPktGet(p_drv_data);
            USBD_EventSetup(        p_drv,                      /* Post setup pkt to the core.                          */
                            (void *)p_setup_buf);
        }
        CPU_CRITICAL_EXIT();

       *p_err = USBD_ERR_NONE;
    } else {
        USBD_DrvEP_TxStart(        p_drv,
                                   ep_addr,
                           (void *)0,
                                   0u,
                                   p_err);
    }
}


/*
*********************************************************************************************************
*                                          USBD_DrvEP_Abort()
*
* Description : Abort any pending transfer on endpoint.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_addr     Endpoint address.
*
* Return(s)   : DEF_OK,   if NO error(s),
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_DrvEP_Abort (USBD_DRV    *p_drv,
                                       CPU_INT08U   ep_addr)
{
    USBD_DRV_REG   *p_reg;
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT16U      fifo_sel;
    CPU_INT16U      fifo_sel_masked;
    CPU_INT08U      ep_log_nbr;
    CPU_INT08U      ep_ix_nbr;
    CPU_REG16       reg_to;
    CPU_BOOLEAN     valid;
    CPU_SR_ALLOC();


    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;               /* Get a ref to the drv's internal data.                */
    p_reg      = (USBD_DRV_REG  *)p_drv_data->RegPtr;           /* Get a ref to the USB hw reg.                         */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    ep_ix_nbr  =  RX600_EP_PHY_TO_EP_TBL_IX(USBD_EP_ADDR_TO_PHY(ep_addr));
    fifo_sel   =  0x00u;

    if (USBD_EP_IS_IN(ep_addr) == DEF_YES) {
        if (ep_log_nbr == 0u) {
            DEF_BIT_SET(fifo_sel, RX600_USB_CFIFOSEL_ISEL);
        }
    } else {
        DEF_BIT_CLR(fifo_sel, RX600_USB_CFIFOSEL_ISEL);
    }

    DEF_BIT_SET(fifo_sel, (ep_log_nbr & RX600_USB_CFIFOSEL_CURPIPE));

    reg_to          =  RX600_USB_REG_TO;
    fifo_sel_masked = (fifo_sel & (RX600_USB_CFIFOSEL_CURPIPE | RX600_USB_CFIFOSEL_ISEL));

    CPU_CRITICAL_ENTER();
    if (p_drv_data->EP_XferState[ep_ix_nbr] == USBD_DRV_EP_XFER_STATE_IN_PROGRESS) {
        p_drv_data->EP_XferState[ep_ix_nbr] = USBD_DRV_EP_XFER_STATE_ABORTED;
    }
    do {
       *p_reg->CFIFOSEL = fifo_sel_masked;
        reg_to--;
    } while (((*p_reg->CFIFOSEL & (RX600_USB_CFIFOSEL_CURPIPE | RX600_USB_CFIFOSEL_ISEL)) != fifo_sel_masked) &&
             (reg_to > 0u));
    if (reg_to == 0u) {
        CPU_CRITICAL_EXIT();
        return (DEF_FAIL);
    }

    valid = USBD_RX600_DrvEP_SetRespPID(p_reg,                  /* Set EP resp PID.                                     */
                                        ep_log_nbr,
                                        RX600_USB_DCPCTR_PID_NAK);
    if (valid != DEF_OK) {
        CPU_CRITICAL_EXIT();
        return (DEF_FAIL);
    }
    DEF_BIT_SET(*p_reg->CFIFOCTR, RX600_USB_FIFOCTR_BCLR);      /* Clr pipe's FIFO.                                     */

    CPU_CRITICAL_EXIT();

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                          USBD_DrvEP_Stall()
*
* Description : Set or clear stall condition on endpoint.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               ep_addr     Endpoint address.
*
*               state       Endpoint stall state:
*                               DEF_SET: set   stall condition
*                               DEF_CLR: clear stall condition
*
* Return(s)   : DEF_OK,   if NO error(s),
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
    USBD_DRV_REG   *p_reg;
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT16U     *p_setup_buf;
    CPU_INT16U      fifo_sel;
    CPU_INT16U      fifo_sel_masked;
    CPU_INT08U      ep_log_nbr;
    CPU_REG16       reg_to;
    CPU_BOOLEAN     valid;
    CPU_SR_ALLOC();


    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;               /* Get a ref to the drv's internal data.                */
    p_reg      = (USBD_DRV_REG  *)p_drv_data->RegPtr;           /* Get a ref to the USB hw reg.                         */
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);

    if (state == DEF_SET) {
        CPU_CRITICAL_ENTER();

        if (ep_addr == 0x00u) {                                 /* If EP is OUT and is EP0.                             */
            USBD_RX600_DrvSetupPktDeq(p_drv_data);
        }

        valid = USBD_RX600_DrvEP_SetRespPID(p_reg,              /* Set EP resp PID.                                     */
                                            ep_log_nbr,
                                            RX600_USB_DCPCTR_PID_NAK);
        if (valid != DEF_OK) {
            CPU_CRITICAL_EXIT();
            return (DEF_FAIL);
        }

        (void)USBD_RX600_DrvEP_SetRespPID(p_reg,                /* Set EP resp PID.                                     */
                                          ep_log_nbr,
                                          RX600_USB_DCPCTR_PID_STALL_1);

        if ((ep_addr                             == 0x00u) &&
            (p_drv_data->SetupPktQ.NbrSetupPkt != 0u)) {
                                                                /* Deq the next setup pkt and post it to the core.      */
            p_setup_buf = USBD_RX600_DrvSetupPktGet(p_drv_data);
            USBD_EventSetup(        p_drv,                      /* Post setup pkt to the core.                          */
                            (void *)p_setup_buf);
        }
        CPU_CRITICAL_EXIT();

        USBD_DBG_DRV_EP("  Drv EP FIFO Stall (set)", ep_addr);
    } else {
        fifo_sel = ep_log_nbr & RX600_USB_CFIFOSEL_CURPIPE;
        if (USBD_EP_IS_IN(ep_addr) == DEF_YES) {
            if (ep_log_nbr == 0u) {
                DEF_BIT_SET(fifo_sel, RX600_USB_CFIFOSEL_ISEL);
            }
        } else {
            DEF_BIT_CLR(fifo_sel, RX600_USB_CFIFOSEL_ISEL);
        }
        reg_to          =  RX600_USB_REG_TO;
        fifo_sel_masked = (fifo_sel & (RX600_USB_CFIFOSEL_CURPIPE | RX600_USB_CFIFOSEL_ISEL));

        CPU_CRITICAL_ENTER();

        (void)USBD_RX600_DrvEP_SetRespPID(p_reg,                /* Set EP resp PID.                                     */
                                          ep_log_nbr,
                                          RX600_USB_DCPCTR_PID_STALL_1);
        valid = USBD_RX600_DrvEP_SetRespPID(p_reg,              /* Set EP resp PID.                                     */
                                            ep_log_nbr,
                                            RX600_USB_DCPCTR_PID_NAK);
        if (valid != DEF_OK) {
            CPU_CRITICAL_EXIT();
            return (DEF_FAIL);
        }

        if (ep_log_nbr == 0u) {
            DEF_BIT_SET(*p_reg->DCPCTR, RX600_USB_DCPCTR_SQCLR);
        } else if (ep_log_nbr <= RX600_USB_PIPE_MAX_NBR) {
            DEF_BIT_SET(*p_reg->PIPE_CTR[ep_log_nbr - 1u], RX600_USB_PIPECTR_SQCLR);
        }

        do {
           *p_reg->CFIFOSEL = fifo_sel_masked;
            reg_to--;
        } while (((*p_reg->CFIFOSEL & (RX600_USB_CFIFOSEL_CURPIPE | RX600_USB_CFIFOSEL_ISEL)) != fifo_sel_masked) &&
                 (reg_to > 0u));
        if (reg_to == 0u) {
            CPU_CRITICAL_EXIT();
            return (DEF_FAIL);
        }

        DEF_BIT_SET(*p_reg->CFIFOCTR, RX600_USB_FIFOCTR_BCLR);  /* Clr pipe's FIFO.                                     */

        (void)USBD_RX600_DrvEP_SetRespPID(p_reg,                /* Set EP resp PID.                                     */
                                          ep_log_nbr,
                                          RX600_USB_DCPCTR_PID_BUF);
        CPU_CRITICAL_EXIT();

        USBD_DBG_DRV_EP("  Drv EP FIFO Stall (clr)", ep_addr);
    }

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                         USBD_DrvISR_Handler()
*
* Description : USB device Interrupt Service Routine (ISR) handler.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
* Return(s)   : none.
*
* Note(s)     : (1) The interrupts registers (INTSTS0, BRDYSTS and BEMPSTS) must be cleared precisely
*                   this way :
*
*                   (a) INTSTS0 must be cleared by writing 0 only to the status bits indicating 1. It
*                       must not write 0 to status bits indicating 0. Write 0 also to the CTSQ and DVSQ
*                       bit-fields.
*                   (b) BRDYSTS and BEMPSTS must be cleared right after they have been handled, not
*                       before.
*
*               (2) The RX600 USB module automatically handles any SET_ADDRESS standard request received.
*                   For the driver and the USB-Device stack to operate correctly together, it is required
*                   to :
*
*                   (a) Enqueue every setup packet to maintain sequentiality of the control transfers,
*                       since a zero-length packet is sent before the stack processes the SET_ADDRESS
*                       standard request.
*                   (b) Since the SET_ADDRESS standard request is handled only by the USB module, a
*                       "dummy" SET_ADDRESS standard request must be created and enqueued when a device
*                       state change to 'Addressed' is detected.
*
*               (3) User Manual indicates: "When the VBUS interrupt is generated, use software to
*                   repeat reading the VBSTS bit until the same value is read three or more times, and
*                   eliminate chattering.". It ensures that VBUS signal is stable. They also recommend to
*                   wait for VBUS stabilization before doing this.
*
*               (4) During Setup transfers, the RX600 controller raises a Control Transfer Stage
*                   Transition (CTRT) interrupt when receiving the first token (SETUP, OUT or IN). In
*                   some cases (i.e. high-speed hub is used, variation in timings from the host, etc.),
*                   the appropriate transfer will not yet have started when the CTRT interrupt happens.
*                   This can lead to problematic cases:
*
*                   (a) When device must receive a zero-length packet after a successful IN transaction
*                       (Get Descriptor standard request, for example), if the first OUT token is
*                       received and the interrupt is triggered BEFORE the USB-Device core has called
*                       USBD_DrvEP_RxStart(), a flag is set, indicating that USBD_EP_RxCmpl() will need
*                       to be called when the USB-Device core will call USBD_DrvEP_RxStart(), since the
*                       OUT token was already received.
*
*                   (b) To avoid the same problem as with OUT token, the flag CallTxCmpl is used, to
*                       indicate that USBD_EP_TxCmpl() must be called. It is called from another CTRT
*                       interrupt source instead of being called when receiving the first IN token. This
*                       guarantees that the timing will be correct.
*********************************************************************************************************
*/

static  void  USBD_DrvISR_Handler (USBD_DRV  *p_drv)
{
    USBD_DRV_REG   *p_reg;
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT16U     *p_setup_buf;
    CPU_INT16U      int_reg0;
    CPU_INT16U      brdy_reg;
    CPU_INT16U      bemp_reg;
    CPU_INT16U      reg_bits;
    CPU_INT08U      ep_log_nbr;
    CPU_INT08U      ep_ix_nbr;
    CPU_REG32       i;
    CPU_INT16U      vbus_sts_initial;
    CPU_INT16U      vbus_sts_cur;
    CPU_INT08U      vbus_count;


    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;               /* Get a ref to the drv's internal data.                */
    p_reg      = (USBD_DRV_REG  *)p_drv_data->RegPtr;           /* Get a ref to the USB hw reg.                         */
    int_reg0   = *p_reg->INTSTS0;                               /* Rd  'Interrupt Status Register 0' (INTSTS0).         */

                                                                /* Process each of the interrupt flags.                 */
                                                                /* --------------------- VBUS INT --------------------- */
    if (DEF_BIT_IS_SET(int_reg0, RX600_USB_INTSTS0_VBINT) == DEF_YES) {
        DEF_BIT_CLR(*p_reg->INTENB0, RX600_USB_INTENB0_VBSE);   /* Dis VBUS int.                                        */

        for (i = 0u; i < RX600_USB_VBUS_RD_DLY; i++) {          /* Wait short dly for VBUS stabilization (see Note #3). */
            ;
        }
                                                                /* Rd VBUS state several times to avoid chattering...   */
                                                                /* ... (see Note #3).                                   */
        do {
            vbus_sts_initial = *p_reg->INTSTS0 & RX600_USB_INTSTS0_VBSTS;

            for (vbus_count = RX600_USB_VBUS_STABILIZATION_MAX_NBR_RD; vbus_count > 0u; vbus_count--) {
                vbus_sts_cur = *p_reg->INTSTS0 & RX600_USB_INTSTS0_VBSTS;
                if (vbus_sts_cur != vbus_sts_initial) {
                    break;                                      /* VBUS signal not stable, rd it again several times.   */
                }
            }
        } while (vbus_count != 0u);

        if (DEF_BIT_IS_SET(vbus_sts_cur, RX600_USB_INTSTS0_VBSTS) == DEF_YES) {
            USBD_EventConn(p_drv);
        } else {
            USBD_EventDisconn(p_drv);
        }
        DEF_BIT_SET(*p_reg->INTENB0, RX600_USB_INTENB0_VBSE);   /* En VBUS int.                                         */
        DEF_BIT_CLR(*p_reg->INTSTS0, RX600_USB_INTSTS0_VBINT);
    }

                                                                /* -------------------- RESUME INT -------------------- */
    if (DEF_BIT_IS_SET(int_reg0, RX600_USB_INTSTS0_RESM) == DEF_YES) {
        USBD_EventResume(p_drv);                                /* Resume event detected: post event to the core.       */
        USBD_DBG_DRV("  Drv ISR Resume");
        DEF_BIT_CLR(*p_reg->INTSTS0, RX600_USB_INTSTS0_RESM);
    }

                                                                /* ------------- DEV STATE TRANSITION INT ------------- */
    if (DEF_BIT_IS_SET(int_reg0, RX600_USB_INTSTS0_DVST) == DEF_YES) {
        reg_bits = (int_reg0 & RX600_USB_INTSTS0_DVSQ);         /* Mask all non relevant bits.                          */

        switch (reg_bits) {
            case (RX600_USB_INTSTS0_DVSQ_PWR):                  /* Dev has entered the powered state.                   */
                  break;


            case (RX600_USB_INTSTS0_DVSQ_DFLT):                 /* Dev has entered the default state.                   */
                 USBD_RX600_DrvSetupPktClr(p_drv_data);         /* Free up any setup req buf in the q.                  */
                 USBD_EventReset(p_drv);                        /* Post bus reset event to the core.                    */
                 USBD_DBG_DRV("  Drv ISR Reset");
                 break;


            case (RX600_USB_INTSTS0_DVSQ_ADDR):                 /* Dev has entered the addressed state.                 */
                 USBD_RX600_DrvSetupPktEnqSetAddr(p_drv_data);  /* Enq dummy SET_ADDRESS setup pkt (see Note #2b).      */
                 if (p_drv_data->SetupPktQ.NbrSetupPkt == 1u) { /* If no other pkt in buf, post event to the core.      */
                     p_setup_buf = USBD_RX600_DrvSetupPktGet(p_drv_data);
                     USBD_EventSetup(        p_drv,
                                     (void *)p_setup_buf);
                 }
                 break;


            case (RX600_USB_INTSTS0_DVSQ_CFG):                  /* Dev has entered the configured state.                */
                 break;


            case (RX600_USB_INTSTS0_DVSQ_SUSPEND_1):
            case (RX600_USB_INTSTS0_DVSQ_SUSPEND_2):
            case (RX600_USB_INTSTS0_DVSQ_SUSPEND_3):
            case (RX600_USB_INTSTS0_DVSQ_SUSPEND_4):
                                                                /* Dev has entered the suspended state.                 */
                 USBD_EventSuspend(p_drv);                      /* Post bus suspend event to the core.                  */
                 USBD_DBG_DRV("  Drv ISR Suspend");
                 break;


            default:
                 break;
        }
        DEF_BIT_CLR(*p_reg->INTSTS0, RX600_USB_INTSTS0_DVST);
    }

                                                                /* ---------- CTRL XFER STAGE TRANSITION INT ---------- */
    if (DEF_BIT_IS_SET(int_reg0, RX600_USB_INTSTS0_CTRT) == DEF_YES) {
        reg_bits = (int_reg0 & RX600_USB_INTSTS0_CTSQ);         /* Mask all non relevant bits.                          */

        USBD_DBG_STATS_INC(ISR_CTSQ_Cnt[reg_bits]);

        switch (reg_bits) {
            case RX600_USB_INTSTS0_CTSQ_CTRL_RD_STATUS:         /* Ctrl rd status stage.                                */
                 if (p_drv_data->EP_XferState[RX600_EP_PHY_TO_EP_TBL_IX(0u)] == USBD_DRV_EP_XFER_STATE_IN_PROGRESS) {
                     USBD_EP_RxCmpl(p_drv, 0u);
                 } else {
                     p_drv_data->CallRxCmpl = DEF_TRUE;         /* USBD_EP_RxCmpl should be called later (see Note #4a).*/
                 }
                 break;


            case RX600_USB_INTSTS0_CTSQ_CTRL_WR_STATUS:         /* Ctrl wr status stage.                                */
                 p_drv_data->CallTxCmpl = DEF_TRUE;
                 break;


            case RX600_USB_INTSTS0_CTSQ_CTRL_WR_ND_STATUS:      /* Ctrl wr (no data) status stage.                      */
                 p_drv_data->CallTxCmpl = DEF_TRUE;             /* USBD_EP_TxCmpl will need to be called (see Note #4b).*/
            case RX600_USB_INTSTS0_CTSQ_CTRL_WR_DATA:           /* Ctrl wr data stage.                                  */
            case RX600_USB_INTSTS0_CTSQ_CTRL_RD_DATA:           /* Ctrl rd data stage.                                  */
                 USBD_RX600_DrvSetupPktEnq(p_drv_data,          /* Setup pkt has been rxd, enq setup pkt (see Note #2a).*/
                                           p_reg);

                 if (p_drv_data->SetupPktQ.NbrSetupPkt == 1u) { /* If no other pkt in buf, post event to the core.      */
                     p_setup_buf  =  USBD_RX600_DrvSetupPktGet(p_drv_data);
                     USBD_EventSetup(        p_drv,
                                     (void *)p_setup_buf);
                 }
                 DEF_BIT_CLR(*p_reg->INTSTS0, RX600_USB_INTSTS0_VALID);
                 break;


            case RX600_USB_INTSTS0_CTSQ_CTRL_TX_SEQ_ERR:        /* Ctrl xfer sequence err.                              */
            case RX600_USB_INTSTS0_CTSQ_SETUP:                  /* Idle or setup stage.                                 */
            default:
                 if (p_drv_data->CallTxCmpl == DEF_TRUE) {      /* If CallTxCmpl flag is set ...                        */
                                                                /* ... call USBD_EP_TxCmpl (see Note #4b).              */
                     if (p_drv_data->EP_XferState[RX600_EP_PHY_TO_EP_TBL_IX(1u)] == USBD_DRV_EP_XFER_STATE_IN_PROGRESS) {
                         p_drv_data->CallTxCmpl = DEF_FALSE;
                         p_drv_data->EP_XferState[RX600_EP_PHY_TO_EP_TBL_IX(1u)] = USBD_DRV_EP_XFER_STATE_NONE;
                         USBD_DBG_STATS_INC(EP0_TxCmplCallFromISR_CTSQ);

                         USBD_EP_TxCmpl(p_drv, 0u);
                     } else {
                         USBD_DBG_STATS_INC(InvalidXferStateISR_CTSQ);
                     }
                 }
                 break;
        }
        DEF_BIT_CLR(*p_reg->INTSTS0, RX600_USB_INTSTS0_CTRT);
    }

                                                                /* ---------- BUFFER READY INTERRUPT STATUS ----------- */
    brdy_reg  = *p_reg->BRDYSTS;                                /* BRDY int status reg (BRDYSTS).                       */
    brdy_reg &= *p_reg->BRDYENB;                                /* BRDY int en reg (BRDYENB).                           */
    while (brdy_reg != 0u) {
        ep_log_nbr = CPU_CntTrailZeros(brdy_reg);
        ep_ix_nbr  = RX600_EP_PHY_TO_EP_TBL_IX(ep_log_nbr * 2u);

        USBD_DBG_STATS_INC(ISR_BRDY_Cnt[ep_ix_nbr]);

        if (p_drv_data->EP_XferState[ep_ix_nbr] == USBD_DRV_EP_XFER_STATE_IN_PROGRESS) {
            USBD_EP_RxCmpl(p_drv, ep_log_nbr);                  /* Post successful completion to the core.              */
        }

        DEF_BIT_CLR(brdy_reg,        DEF_BIT(ep_log_nbr));
        DEF_BIT_CLR(*p_reg->BRDYSTS, DEF_BIT(ep_log_nbr));      /* Clr BRDYSTS (see Note #1b).                          */

        brdy_reg  = *p_reg->BRDYSTS;                            /* BRDY int status reg (BRDYSTS).                       */
        brdy_reg &= *p_reg->BRDYENB;                            /* BRDY int en reg (BRDYENB).                           */
    }

                                                                /* ---------- BUFFER EMPTY INTERRUPT STATUS ----------- */
    bemp_reg  = *p_reg->BEMPSTS;                                /* BEMP int status reg (BEMPSTS).                       */
    bemp_reg &= *p_reg->BEMPENB;                                /* BEMP int en reg (BEMPENB).                           */
    while(bemp_reg != 0u) {
        ep_log_nbr = CPU_CntTrailZeros(bemp_reg);
        ep_ix_nbr  = RX600_EP_PHY_TO_EP_TBL_IX((1u + (ep_log_nbr * 2u)));

        USBD_DBG_STATS_INC(ISR_BEMP_Cnt[ep_ix_nbr]);

        if (p_drv_data->EP_XferState[ep_ix_nbr] == USBD_DRV_EP_XFER_STATE_IN_PROGRESS) {
            p_drv_data->EP_XferState[ep_ix_nbr] = USBD_DRV_EP_XFER_STATE_NONE;
            USBD_EP_TxCmpl(p_drv, ep_log_nbr);

            USBD_DBG_STATS_INC_IF_TRUE(EP0_TxCmplCallFromISR_BEMP, (ep_log_nbr == 0u));
        }

        DEF_BIT_CLR(bemp_reg,        DEF_BIT(ep_log_nbr));
        DEF_BIT_CLR(*p_reg->BEMPSTS, DEF_BIT(ep_log_nbr));      /* Clr BEMPSTS (see Note #1b).                          */

        bemp_reg  = *p_reg->BEMPSTS;                            /* BEMP int status reg (BEMPSTS).                       */
        bemp_reg &= *p_reg->BEMPENB;                            /* BEMP int en reg (BEMPENB).                           */
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
*                                     USBD_RX600_DrvEP_SetRespPID()
*
* Description : This function is used to set the state of the selected endpoint.
*
* Arguments   : p_reg       Pointer to RX600 registers structure.
*
*               ep_log_nbr  Endpoint logical number.
*
*               resp_pid    Response PID to set the endpoint to.
*                               RX600_USB_DCPCTR_PID_NAK
*                               RX600_USB_DCPCTR_PID_BUF
*                               RX600_USB_DCPCTR_PID_STALL_1
*                               RX600_USB_DCPCTR_PID_STALL_2
*                               RX600_USB_DCPCTR_PID_INVALID
*
* Return(s)   : DEF_OK,   if NO error(s),
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_RX600_DrvEP_SetRespPID (USBD_DRV_REG  *p_reg,
                                                  CPU_INT08U     ep_log_nbr,
                                                  CPU_INT08U     resp_pid)
{
    CPU_INT16U  pipe_ctr;
    CPU_REG16   reg_to;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if ((resp_pid != RX600_USB_DCPCTR_PID_NAK) &&
        (resp_pid != RX600_USB_DCPCTR_PID_BUF) &&
        (resp_pid != RX600_USB_DCPCTR_PID_STALL_1) &&
        (resp_pid != RX600_USB_DCPCTR_PID_STALL_2) &&
        (resp_pid != RX600_USB_DCPCTR_PID_INVALID)) {

        return (DEF_FAIL);
    }
#endif

    reg_to = RX600_USB_REG_TO;

    if (ep_log_nbr == 0u) {
        pipe_ctr = *p_reg->DCPCTR;

        if ((pipe_ctr & RX600_USB_DCPCTR_PID) != resp_pid) {
            DEF_BIT_CLR(pipe_ctr, RX600_USB_DCPCTR_PID);
            DEF_BIT_SET(pipe_ctr, resp_pid);
           *p_reg->DCPCTR = pipe_ctr;
        }
    } else if (ep_log_nbr <= RX600_USB_PIPE_MAX_NBR) {
        pipe_ctr = *p_reg->PIPE_CTR[ep_log_nbr - 1u];

        if ((pipe_ctr & RX600_USB_PIPECTR_PID) != resp_pid) {
            DEF_BIT_CLR(pipe_ctr, RX600_USB_PIPECTR_PID);
            DEF_BIT_SET(pipe_ctr, resp_pid);
           *p_reg->PIPE_CTR[ep_log_nbr - 1u] = pipe_ctr;
        }
    }

    if (resp_pid == RX600_USB_DCPCTR_PID_NAK) {
        if (ep_log_nbr == 0u) {
            do {
               pipe_ctr = *p_reg->DCPCTR;
               reg_to--;
            } while ((DEF_BIT_IS_SET(pipe_ctr, RX600_USB_DCPCTR_PBUSY) == DEF_TRUE) &&
                     (reg_to > 0u));
        } else if (ep_log_nbr <= RX600_USB_PIPE_MAX_NBR) {
            do {
               pipe_ctr = *p_reg->PIPE_CTR[ep_log_nbr - 1u];
               reg_to--;
            } while ((DEF_BIT_IS_SET(pipe_ctr, RX600_USB_PIPECTR_PBUSY) == DEF_TRUE) &&
                     (reg_to > 0u));
        }
    }
    if (reg_to > 0u) {
        return (DEF_OK);
    } else {
        return (DEF_FAIL);
    }
}


/*
*********************************************************************************************************
*                                      USBD_RX600_DrvSetupPktClr()
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

static  void  USBD_RX600_DrvSetupPktClr(USBD_DRV_DATA  *p_drv_data)
{
    USBD_DRV_SETUP_PKT_Q  *p_setup_pkt_buf;


    p_setup_pkt_buf = &(p_drv_data->SetupPktQ);

    p_setup_pkt_buf->NbrSetupPkt = 0u;                          /* Clr internal data and ix.                            */
    p_setup_pkt_buf->IxIn        = 0u;
    p_setup_pkt_buf->IxOut       = 0u;

    Mem_Clr((void     *)&(p_setup_pkt_buf->SetupPktTbl[0u]),    /* Clr every buf's content.                             */
                   sizeof(p_setup_pkt_buf->SetupPktTbl));
}


/*
*********************************************************************************************************
*                                      USBD_RX600_DrvSetupPktEnq()
*
* Description : Enqueue a setup packet in the circular buffer.
*
* Argument(s) : p_drv_data  Pointer to device driver data structure.
*
*               p_reg       Pointer to RX600 registers structure.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_RX600_DrvSetupPktEnq (USBD_DRV_DATA  *p_drv_data,
                                         USBD_DRV_REG   *p_reg)
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
    dest_buf[0u] = MEM_VAL_GET_INT16U(&(*p_reg->USBREQ));
    dest_buf[1u] = MEM_VAL_GET_INT16U(&(*p_reg->USBVAL));
    dest_buf[2u] = MEM_VAL_GET_INT16U(&(*p_reg->USBINDX));
    dest_buf[3u] = MEM_VAL_GET_INT16U(&(*p_reg->USBLENG));

                                                                /* Adjust ix and internal data.                         */
    p_setup_pkt_buf->IxIn++;
    if (p_setup_pkt_buf->IxIn >= USBD_DRV_SETUP_PKT_CIRCULAR_BUF_SIZE) {
        p_setup_pkt_buf->IxIn  = 0u;
    }
    p_setup_pkt_buf->NbrSetupPkt++;
}


/*
*********************************************************************************************************
*                                  USBD_RX600_DrvSetupPktEnqSetAddr()
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

static  void  USBD_RX600_DrvSetupPktEnqSetAddr (USBD_DRV_DATA  *p_drv_data)
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
*                                      USBD_RX600_DrvSetupPktGet()
*
* Description : Get a setup packet from the circular buffer.
*
* Argument(s) : p_drv_data  Pointer to device driver data structure.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function only gets the first setup packet from the buffer. The function
*                   USBD_RX600_DrvSetupPktDeq() must be called after processing the setup packet obtained
*                   to remove the setup packet from the circular buffer.
*
*               (2) This function should be called from a CRITICAL_SECTION.
*********************************************************************************************************
*/

static  CPU_INT16U*  USBD_RX600_DrvSetupPktGet (USBD_DRV_DATA  *p_drv_data)
{
    USBD_DRV_SETUP_PKT_Q  *p_setup_pkt_buf;
    CPU_INT16U            *actual_buf;


    p_setup_pkt_buf = &(p_drv_data->SetupPktQ);

    actual_buf = &(p_setup_pkt_buf->SetupPktTbl[p_setup_pkt_buf->IxOut][0u]);

    return (actual_buf);
}


/*
*********************************************************************************************************
*                                      USBD_RX600_DrvSetupPktDeq()
*
* Description : Dequeue a setup packet from the circular buffer.
*
* Argument(s) : p_drv_data  Pointer to device driver data structure.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function only removes the first setup packet from the buffer. The function
*                   USBD_RX600_DrvSetupPktGet() must be called before this function to get a pointer on
*                   the first setup packet and process the setup packet before dequeuing it.
*
*               (2) This function should be called from a CRITICAL_SECTION.
*********************************************************************************************************
*/

static  void  USBD_RX600_DrvSetupPktDeq(USBD_DRV_DATA  *p_drv_data)
{
    USBD_DRV_SETUP_PKT_Q  *p_setup_pkt_buf;


    p_setup_pkt_buf = &(p_drv_data->SetupPktQ);

    p_setup_pkt_buf->IxOut++;
    if (p_setup_pkt_buf->IxOut >= USBD_DRV_SETUP_PKT_CIRCULAR_BUF_SIZE) {
        p_setup_pkt_buf->IxOut  = 0u;
    }
    p_setup_pkt_buf->NbrSetupPkt--;
}

