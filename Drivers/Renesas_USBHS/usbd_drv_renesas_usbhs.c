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
*                                       RENESAS USB HIGH-SPEED
*
* Filename : usbd_drv_renesas_usbhs.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)  : (1) You can find specific information about this driver at:
*                https://doc.micrium.com/display/USBDDRV/RENESAS_USBHS
*
*            (2) With an appropriate BSP, this device driver will support the USB device module on
*                the Renesas RZ.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "usbd_drv_renesas_usbhs.h"
#include  "../../Source/usbd_core.h"
#include  "../drv_lib/usbd_drv_lib.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  RENESAS_USBHS_PIPE_QTY_MAX                       16u   /* Max nbr of pipes driver supports.                    */
#define  RENESAS_USBHS_DFIFO_QTY_MAX                       2u   /* Max nbr of DFIFO channel driver supports.            */


                                                                /* Bit field that represents the avail DFIFO ch.        */
#define  RENESAS_USBHS_DFIFO_MASK     DEF_BIT_FIELD(USBD_DrvDFIFO_Qty[p_drv_data->Ctrlr], 0u)
#define  RENESAS_USBHS_CFIFO                       DEF_BIT_07   /* Bit that represents CFIFO.                           */

#define  RENESAS_USBHS_BUF_STARTING_IX                     8u   /* FIFO buf start ix. Prev buf used by ctrl/intr pipes. */
#define  RENESAS_USBHS_BUF_LEN                            64u   /* Length of single buf in FIFO.                        */
#define  RENESAS_USBHS_BUF_QTY_AVAIL                     128u   /* FIFO is 8K long -> 128 buf avail (128 * 64).         */

#define  RENESAS_USBHS_SETUP_PKT_Q_SIZE                    3u   /* Size of setup pkt circular buf.                      */

#define  RENESAS_USBHS_RX_Q_SIZE                           4u   /* Size of the RX Q (useful in dbl buf mode).           */

#define  RENESAS_USBHS_PIPETRN_IX_NONE                  0xFFu   /* No PIPETRN ix for this pipe.                         */


/*
*********************************************************************************************************
*                                USB HARDWARE REGISTER BIT DEFINITIONS
*********************************************************************************************************
*/

                                                                /* ------ SYSTEM CONFIGURATION CONTROL REGISTER ------- */
#define  RENESAS_USBHS_SYSCFG0_USBE                   DEF_BIT_00
#define  RENESAS_USBHS_SYSCFG0_UPLLE                  DEF_BIT_01
#define  RENESAS_USBHS_SYSCFG0_UCKSEL                 DEF_BIT_02
#define  RENESAS_USBHS_SYSCFG0_DPRPU                  DEF_BIT_04
#define  RENESAS_USBHS_SYSCFG0_DRPD                   DEF_BIT_05
#define  RENESAS_USBHS_SYSCFG0_DCFM                   DEF_BIT_06
#define  RENESAS_USBHS_SYSCFG0_HSE                    DEF_BIT_07
#define  RENESAS_USBHS_SYSCFG0_CNEN                   DEF_BIT_08/* Only avail on RX64M.                                 */


                                                                /* -------------- CPU BUS WAIT REGISTER --------------- */
#define  RENESAS_USBHS_BUSWAIT_BWAIT_MASK             DEF_BIT_FIELD(5u, 0u)


                                                                /* ------- SYSTEM CONFIGURATION STATUS REGISTER ------- */
#define  RENESAS_USBHS_SYSSTS0_LNST_MASK              DEF_BIT_FIELD(2u, 0u)


                                                                /* ---------- DEVICE STATE CONTROL REGISTER ----------- */
#define  RENESAS_USBHS_DVSCTR_RHST_MASK               DEF_BIT_FIELD(3u, 0u)
#define  RENESAS_USBHS_DVSCTR_UACT                    DEF_BIT_04
#define  RENESAS_USBHS_DVSCTR_RESUME                  DEF_BIT_05
#define  RENESAS_USBHS_DVSCTR_USBRST                  DEF_BIT_06
#define  RENESAS_USBHS_DVSCTR_RWUPE                   DEF_BIT_07
#define  RENESAS_USBHS_DVSCTR_WKUP                    DEF_BIT_08

#define  RENESAS_USBHS_DVSCTR_RHST_NONE               DEF_BIT_NONE
#define  RENESAS_USBHS_DVSCTR_RHST_RESET              DEF_BIT_02
#define  RENESAS_USBHS_DVSCTR_RHST_FS                 DEF_BIT_01
#define  RENESAS_USBHS_DVSCTR_RHST_HS                (DEF_BIT_01 | DEF_BIT_00)


                                                                /* ------------ DMA-FIFO BUS CFG REGISTER ------------- */
#define  RENESAS_USBHS_DxFBCFG_TENDE                  DEF_BIT_04


                                                                /* ------------ CFIFO PORT SELECT REGISTER ------------ */
#define  RENESAS_USBHS_CFIFOSEL_CURPIPE_MASK          DEF_BIT_FIELD(4u, 0u)
#define  RENESAS_USBHS_CFIFOSEL_ISEL                  DEF_BIT_05
#define  RENESAS_USBHS_CFIFOSEL_BIGEND                DEF_BIT_08
#define  RENESAS_USBHS_CFIFOSEL_MBW_MASK              DEF_BIT_FIELD(2u, 10u)
#define  RENESAS_USBHS_CFIFOSEL_REW                   DEF_BIT_14
#define  RENESAS_USBHS_CFIFOSEL_RCNT                  DEF_BIT_15

#define  RENESAS_USBHS_CFIFOSEL_MBW_8                 DEF_BIT_NONE
#define  RENESAS_USBHS_CFIFOSEL_MBW_16                DEF_BIT_10
#define  RENESAS_USBHS_CFIFOSEL_MBW_32                DEF_BIT_11


                                                                /* ------------ DFIFO PORT SELECT REGISTER ------------ */
#define  RENESAS_USBHS_DxFIFOSEL_CURPIPE_MASK         DEF_BIT_FIELD(4u, 0u)
#define  RENESAS_USBHS_DxFIFOSEL_BIGEND               DEF_BIT_08
#define  RENESAS_USBHS_DxFIFOSEL_MBW_MASK             DEF_BIT_FIELD(2u, 10u)
#define  RENESAS_USBHS_DxFIFOSEL_DREQE                DEF_BIT_12
#define  RENESAS_USBHS_DxFIFOSEL_DCLRM                DEF_BIT_13
#define  RENESAS_USBHS_DxFIFOSEL_REW                  DEF_BIT_14
#define  RENESAS_USBHS_DxFIFOSEL_RCNT                 DEF_BIT_15


                                                                /* ----------- CFIFO PORT CONTROL REGISTER ------------ */
#define  RENESAS_USBHS_FIFOCTR_DTLN_MASK              DEF_BIT_FIELD(12u, 0u)
#define  RENESAS_USBHS_FIFOCTR_FRDY                   DEF_BIT_13
#define  RENESAS_USBHS_FIFOCTR_BCLR                   DEF_BIT_14
#define  RENESAS_USBHS_FIFOCTR_BVAL                   DEF_BIT_15


                                                                /* ----------- INTERRUPT ENABLE REGISTER 0 ------------ */
#define  RENESAS_USBHS_INTENB0_BRDYE                  DEF_BIT_08
#define  RENESAS_USBHS_INTENB0_NRDYE                  DEF_BIT_09
#define  RENESAS_USBHS_INTENB0_BEMPE                  DEF_BIT_10
#define  RENESAS_USBHS_INTENB0_CTRE                   DEF_BIT_11
#define  RENESAS_USBHS_INTENB0_DVSE                   DEF_BIT_12
#define  RENESAS_USBHS_INTENB0_SOFE                   DEF_BIT_13
#define  RENESAS_USBHS_INTENB0_RSME                   DEF_BIT_14
#define  RENESAS_USBHS_INTENB0_VBSE                   DEF_BIT_15

                                                                /* ----------- INTERRUPT ENABLE REGISTER 1 ------------ */
#define  RENESAS_USBHS_INTENB1_SACKE                  DEF_BIT_04
#define  RENESAS_USBHS_INTENB1_SIGNE                  DEF_BIT_05
#define  RENESAS_USBHS_INTENB1_EOFERRE                DEF_BIT_06
#define  RENESAS_USBHS_INTENB1_ATTCHE                 DEF_BIT_11
#define  RENESAS_USBHS_INTENB1_DTCHE                  DEF_BIT_12
#define  RENESAS_USBHS_INTENB1_BCHGE                  DEF_BIT_14

                                                                /* ------------- SOF OUTPUT CFG REGISTER -------------- */
#define  RENESAS_USBHS_SOFCFG_BRDYM                   DEF_BIT_06
#define  RENESAS_USBHS_SOFCFG_TRNENSEL                DEF_BIT_08

                                                                /* ----------- INTERRUPT STATUS REGISTER 0 ------------ */
#define  RENESAS_USBHS_INTSTS0_CTSQ_MASK              DEF_BIT_FIELD(3u, 0u)
#define  RENESAS_USBHS_INTSTS0_VALID                  DEF_BIT_03
#define  RENESAS_USBHS_INTSTS0_DVSQ_MASK              DEF_BIT_FIELD(2u, 4u)
#define  RENESAS_USBHS_INTSTS0_DVSQ_SUSP              DEF_BIT_06
#define  RENESAS_USBHS_INTSTS0_VBSTS                  DEF_BIT_07
#define  RENESAS_USBHS_INTSTS0_BRDY                   DEF_BIT_08
#define  RENESAS_USBHS_INTSTS0_NRDY                   DEF_BIT_09
#define  RENESAS_USBHS_INTSTS0_BEMP                   DEF_BIT_10
#define  RENESAS_USBHS_INTSTS0_CTRT                   DEF_BIT_11
#define  RENESAS_USBHS_INTSTS0_DVST                   DEF_BIT_12
#define  RENESAS_USBHS_INTSTS0_SOFR                   DEF_BIT_13
#define  RENESAS_USBHS_INTSTS0_RESM                   DEF_BIT_14
#define  RENESAS_USBHS_INTSTS0_VBINT                  DEF_BIT_15

#define  RENESAS_USBHS_INTSTS0_CTSQ_SETUP             DEF_BIT_NONE
#define  RENESAS_USBHS_INTSTS0_CTSQ_RD_DATA           DEF_BIT_00
#define  RENESAS_USBHS_INTSTS0_CTSQ_RD_STATUS         DEF_BIT_01
#define  RENESAS_USBHS_INTSTS0_CTSQ_WR_DATA          (DEF_BIT_00 | DEF_BIT_01)
#define  RENESAS_USBHS_INTSTS0_CTSQ_WR_STATUS         DEF_BIT_02
#define  RENESAS_USBHS_INTSTS0_CTSQ_WR_STATUS_NDATA  (DEF_BIT_00 | DEF_BIT_02)
#define  RENESAS_USBHS_INTSTS0_CTSQ_SEQ_ERR          (DEF_BIT_01 | DEF_BIT_02)

#define  RENESAS_USBHS_INTSTS0_DVSQ_POWERED           DEF_BIT_NONE
#define  RENESAS_USBHS_INTSTS0_DVSQ_DFLT              DEF_BIT_04
#define  RENESAS_USBHS_INTSTS0_DVSQ_ADDR              DEF_BIT_05
#define  RENESAS_USBHS_INTSTS0_DVSQ_CONFIG           (DEF_BIT_04 | DEF_BIT_05)


                                                                /* ----------- INTERRUPT STATUS REGISTER 1 ------------ */
#define  RENESAS_USBHS_INTSTS1_SACK                   DEF_BIT_04
#define  RENESAS_USBHS_INTSTS1_SIGN                   DEF_BIT_05
#define  RENESAS_USBHS_INTSTS1_EOFERR                 DEF_BIT_06
#define  RENESAS_USBHS_INTSTS1_ATTCH                  DEF_BIT_11
#define  RENESAS_USBHS_INTSTS1_DTCH                   DEF_BIT_12
#define  RENESAS_USBHS_INTSTS1_BCHG                   DEF_BIT_14


                                                                /* -------------- FRAME NUMBER REGISTER --------------- */
#define  RENESAS_USBHS_FRMNUM_FRNM_MASK               DEF_BIT_FIELD(11u, 0u)
#define  RENESAS_USBHS_FRMNUM_CRCE                    DEF_BIT_14
#define  RENESAS_USBHS_FRMNUM_OVRN                    DEF_BIT_15


                                                                /* -------------- UFRAME NUMBER REGISTER -------------- */
#define  RENESAS_USBHS_UFRMNUM_UFRNM_MASK             DEF_BIT_FIELD(3u, 0u)


                                                                /* --------------- USB ADDRESS REGISTER --------------- */
#define  RENESAS_USBHS_USBADDR_USBADDR_MASK           DEF_BIT_FIELD(7u, 0u)


                                                                /* ------------ USB REQUEST TYPE REGISTER ------------- */
#define  RENESAS_USBHS_USBREQ_BMREQUESTTYPE_MASK      DEF_BIT_FIELD(8u, 0u)
#define  RENESAS_USBHS_USBREQ_BREQUEST_MASK           DEF_BIT_FIELD(8u, 8u)


                                                                /* ------------ DCP CONFIGURATION REGISTER ------------ */
#define  RENESAS_USBHS_DCPCFG_DIR                     DEF_BIT_04


                                                                /* ------------ DCP MAX PKT SIZE REGISTER ------------- */
#define  RENESAS_USBHS_DCPMAXP_MXPS_MASK              DEF_BIT_FIELD(7u, 0u)
#define  RENESAS_USBHS_DCPMAXP_DEVSEL_MASK            DEF_BIT_FIELD(4u, 12u)


                                                                /* --------------- DCP CONTROL REGISTER --------------- */
#define  RENESAS_USBHS_DCPCTR_PID_MASK                DEF_BIT_FIELD(2u, 0u)
#define  RENESAS_USBHS_DCPCTR_CCPL                    DEF_BIT_02
#define  RENESAS_USBHS_DCPCTR_PINGE                   DEF_BIT_04
#define  RENESAS_USBHS_DCPCTR_PBUSY                   DEF_BIT_05
#define  RENESAS_USBHS_DCPCTR_SQMON                   DEF_BIT_06
#define  RENESAS_USBHS_DCPCTR_SQSET                   DEF_BIT_07
#define  RENESAS_USBHS_DCPCTR_SQCLR                   DEF_BIT_08
#define  RENESAS_USBHS_DCPCTR_SUREQCLR                DEF_BIT_11
#define  RENESAS_USBHS_DCPCTR_CSSTS                   DEF_BIT_12
#define  RENESAS_USBHS_DCPCTR_CSCLR                   DEF_BIT_13
#define  RENESAS_USBHS_DCPCTR_SUREQ                   DEF_BIT_14
#define  RENESAS_USBHS_DCPCTR_BSTS                    DEF_BIT_15


                                                                /* ----------- PIPE WINDOW SELECT REGISTER ------------ */
#define  RENESAS_USBHS_PIPESEL_PIPESEL_MASK           DEF_BIT_FIELD(4u, 0u)


                                                                /* ----------- PIPE CONFIGURATION REGISTER ------------ */
#define  RENESAS_USBHS_PIPCFG_EPNUM_MASK              DEF_BIT_FIELD(4u, 0u)
#define  RENESAS_USBHS_PIPCFG_DIR                     DEF_BIT_04
#define  RENESAS_USBHS_PIPCFG_SHTNAK                  DEF_BIT_07
#define  RENESAS_USBHS_PIPCFG_CNTMD                   DEF_BIT_08
#define  RENESAS_USBHS_PIPCFG_DBLB                    DEF_BIT_09
#define  RENESAS_USBHS_PIPCFG_BFRE                    DEF_BIT_10

#define  RENESAS_USBHS_PIPCFG_TYPE_MASK               DEF_BIT_FIELD(2u, 14u)
#define  RENESAS_USBHS_PIPCFG_TYPE_NONE               DEF_BIT_NONE
#define  RENESAS_USBHS_PIPCFG_TYPE_BULK               DEF_BIT_14
#define  RENESAS_USBHS_PIPCFG_TYPE_INTR               DEF_BIT_15
#define  RENESAS_USBHS_PIPCFG_TYPE_ISOC              (DEF_BIT_14 | DEF_BIT_15)


                                                                /* ----------- PIPE BUFFER SETTING REGISTER ----------- */
#define  RENESAS_USBHS_PIPEBUF_BUFNMB_MASK            DEF_BIT_FIELD(8u, 0u)
#define  RENESAS_USBHS_PIPEBUF_BUFSIZE_MASK           DEF_BIT_FIELD(5u, 10u)


                                                                /* ------------ PIPE MAX PKT SIZE REGISTER ------------ */
#define  RENESAS_USBHS_PIPEMAXP_MXPS_MASK             DEF_BIT_FIELD(11u, 0u)
#define  RENESAS_USBHS_PIPEMAXP_DEVSEL_MASK           DEF_BIT_FIELD(4u, 12u)


                                                                /* ----------- PIPE TIMING CONTROL REGISTER ----------- */
#define  RENESAS_USBHS_PIPERI_IITV_MASK               DEF_BIT_FIELD(3u, 0u)
#define  RENESAS_USBHS_PIPERI_IFIS                    DEF_BIT_12


                                                                /* ------------- PIPE N CONTROL REGISTER -------------- */
#define  RENESAS_USBHS_PIPExCTR_PID_MASK              DEF_BIT_FIELD(2u, 0u)
#define  RENESAS_USBHS_PIPExCTR_PBUSY                 DEF_BIT_05
#define  RENESAS_USBHS_PIPExCTR_SQMON                 DEF_BIT_06
#define  RENESAS_USBHS_PIPExCTR_SQSET                 DEF_BIT_07
#define  RENESAS_USBHS_PIPExCTR_SQCLR                 DEF_BIT_08
#define  RENESAS_USBHS_PIPExCTR_ACLRM                 DEF_BIT_09
#define  RENESAS_USBHS_PIPExCTR_ATREPM                DEF_BIT_10
#define  RENESAS_USBHS_PIPExCTR_CSSTS                 DEF_BIT_12
#define  RENESAS_USBHS_PIPExCTR_CSCLR                 DEF_BIT_13
#define  RENESAS_USBHS_PIPExCTR_INBUFM                DEF_BIT_14
#define  RENESAS_USBHS_PIPExCTR_BSTS                  DEF_BIT_15

#define  RENESAS_USBHS_PIPExCTR_PID_NAK               DEF_BIT_NONE
#define  RENESAS_USBHS_PIPExCTR_PID_BUF               DEF_BIT_00
#define  RENESAS_USBHS_PIPExCTR_PID_STALL1            DEF_BIT_01
#define  RENESAS_USBHS_PIPExCTR_PID_STALL2           (DEF_BIT_00 | DEF_BIT_01)


                                                                /* -------- PIPE N TRANSACTION CNT EN REGISTER -------- */
#define  RENESAS_USBHS_PIPExTRE_TRCLR                 DEF_BIT_08
#define  RENESAS_USBHS_PIPExTRE_TRENB                 DEF_BIT_09


                                                                /* ---------- DEVICE ADDRESS N CFG REGISTER ----------- */
#define  RENESAS_USBHS_DEVADDx_USBSPD_MASK            DEF_BIT_FIELD(2u, 6u)
#define  RENESAS_USBHS_DEVADDx_HUBPORT_MASK           DEF_BIT_FIELD(3u, 8u)
#define  RENESAS_USBHS_DEVADDx_UPPHUB_MASK            DEF_BIT_FIELD(4u, 11u)

#define  RENESAS_USBHS_DEVADDx_USBSPD_NONE            DEF_BIT_NONE
#define  RENESAS_USBHS_DEVADDx_USBSPD_LOW             DEF_BIT_06
#define  RENESAS_USBHS_DEVADDx_USBSPD_FULL            DEF_BIT_07
#define  RENESAS_USBHS_DEVADDx_USBSPD_HIGH           (DEF_BIT_06 | DEF_BIT_07)


                                                                /* ------------ UTMI SUSPEND MODE REGISTER ------------ */
#define  RENESAS_USBHS_SUSPMODE_SUSPM                 DEF_BIT_14


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/

                                                                /* ---------- PIPTRN REG INDEX LOOK-UP TABLE ---------- */
static  const  CPU_INT08U  USBD_DrvPIPETRN_LUT[][RENESAS_USBHS_PIPE_QTY_MAX] =
{
                                                                /* ------------------- TABLE FOR RZ ------------------- */
{
    RENESAS_USBHS_PIPETRN_IX_NONE,                              /* PIPE 0 (DCP) -> No PIPTRN reg.                       */
    0u,                                                         /* PIPE 1       -> PIPTRN reg offset 0.                 */
    1u,                                                         /* PIPE 2       -> PIPTRN reg offset 1.                 */
    2u,                                                         /* PIPE 3       -> PIPTRN reg offset 2.                 */
    3u,                                                         /* PIPE 4       -> PIPTRN reg offset 3.                 */
    4u,                                                         /* PIPE 5       -> PIPTRN reg offset 4.                 */
    RENESAS_USBHS_PIPETRN_IX_NONE,                              /* PIPE 6       -> No PIPTRN reg.                       */
    RENESAS_USBHS_PIPETRN_IX_NONE,                              /* PIPE 7       -> No PIPTRN reg.                       */
    RENESAS_USBHS_PIPETRN_IX_NONE,                              /* PIPE 8       -> No PIPTRN reg.                       */
    10u,                                                        /* PIPE 9       -> PIPTRN reg offset 10.                */
    11u,                                                        /* PIPE A       -> PIPTRN reg offset 11.                */
    5u,                                                         /* PIPE B       -> PIPTRN reg offset 5.                 */
    6u,                                                         /* PIPE C       -> PIPTRN reg offset 6.                 */
    7u,                                                         /* PIPE D       -> PIPTRN reg offset 7.                 */
    8u,                                                         /* PIPE E       -> PIPTRN reg offset 8.                 */
    9u,                                                         /* PIPE F       -> PIPTRN reg offset 9.                 */
},

                                                                /* ----------------- TABLE FOR RX64M ------------------ */
{
    RENESAS_USBHS_PIPETRN_IX_NONE,                              /* PIPE 0 (DCP) -> No PIPTRN reg.                       */
    0u,                                                         /* PIPE 1       -> PIPTRN reg offset 0.                 */
    1u,                                                         /* PIPE 2       -> PIPTRN reg offset 1.                 */
    2u,                                                         /* PIPE 3       -> PIPTRN reg offset 2.                 */
    3u,                                                         /* PIPE 4       -> PIPTRN reg offset 3.                 */
    4u,                                                         /* PIPE 5       -> PIPTRN reg offset 4.                 */
    RENESAS_USBHS_PIPETRN_IX_NONE,                              /* PIPE 6       -> No PIPTRN reg.                       */
    RENESAS_USBHS_PIPETRN_IX_NONE,                              /* PIPE 7       -> No PIPTRN reg.                       */
    RENESAS_USBHS_PIPETRN_IX_NONE,                              /* PIPE 8       -> No PIPTRN reg.                       */
    RENESAS_USBHS_PIPETRN_IX_NONE,                              /* No more pipes on RX64M.                              */
    RENESAS_USBHS_PIPETRN_IX_NONE,
    RENESAS_USBHS_PIPETRN_IX_NONE,
    RENESAS_USBHS_PIPETRN_IX_NONE,
    RENESAS_USBHS_PIPETRN_IX_NONE,
    RENESAS_USBHS_PIPETRN_IX_NONE,
    RENESAS_USBHS_PIPETRN_IX_NONE,
},
};

static  const  CPU_INT08U  USBD_DrvPipeQty[]    = {16u, 10u};   /* Quantity of pipes.                                   */
static  const  CPU_INT08U  USBD_DrvDFIFO_Qty[]  = { 2u,  2u};   /* Quantity of DFIFO.                                   */


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

                                                                /* --------------- DFIFO SEL & CTR REG ---------------- */
typedef  struct  usbd_renesas_usbhs_dxfifo {
    CPU_REG16  SEL;
    CPU_REG16  CTR;
} USBD_RENESAS_USBHS_DxFIFO;

                                                                /* ----------- PIPE TRANSACTION COUNTER REG ----------- */
typedef  struct  usbd_renesas_usbhs_transaction_ctr {
    CPU_REG16  TRE;
    CPU_REG16  TRN;
} USBD_RENESAS_USBHS_TRANSACTION_CTR;

                                                                /* ---------------- RENESAS USBHS REG ----------------- */
typedef  struct  usbd_renesas_usbhs_reg {
    CPU_REG16                           SYSCFG0;
    CPU_REG16                           BUSWAIT;
    CPU_REG16                           SYSSTS0;
    CPU_REG16                           PLLSTAT;

    CPU_REG16                           DVSTCTR0;
    CPU_REG16                           RSVD_01;

    CPU_REG16                           TESTMODE;
    CPU_REG16                           RSVD_02;

    CPU_REG16                           DxFBCFG[RENESAS_USBHS_DFIFO_QTY_MAX];

    CPU_REG32                           CFIFO;
    CPU_REG32                           DxFIFO[RENESAS_USBHS_DFIFO_QTY_MAX];

    CPU_REG16                           CFIFOSEL;
    CPU_REG16                           CFIFOCTR;
    CPU_REG32                           RSVD_03;

    USBD_RENESAS_USBHS_DxFIFO           DxFIFOn[RENESAS_USBHS_DFIFO_QTY_MAX];

    CPU_REG16                           INTENB0;
    CPU_REG16                           INTENB1;
    CPU_REG16                           RSVD_04;

    CPU_REG16                           BRDYENB;
    CPU_REG16                           NRDYENB;
    CPU_REG16                           BEMPENB;
    CPU_REG16                           SOFCFG;
    CPU_REG16                           PHYSET;

    CPU_REG16                           INTSTS0;
    CPU_REG16                           INTSTS1;
    CPU_REG16                           RSVD_06;

    CPU_REG16                           BRDYSTS;
    CPU_REG16                           NRDYSTS;
    CPU_REG16                           BEMPSTS;

    CPU_REG16                           FRMNUM;
    CPU_REG16                           UFRMNUM;

    CPU_REG16                           USBADDR;
    CPU_REG16                           RSVD_07;

    CPU_REG16                           USBREQ;
    CPU_REG16                           USBVAL;
    CPU_REG16                           USBINDX;
    CPU_REG16                           USBLENG;

    CPU_REG16                           DCPCFG;
    CPU_REG16                           DCPMAXP;
    CPU_REG16                           DCPCTR;
    CPU_REG16                           RSVD_08;

    CPU_REG16                           PIPESEL;
    CPU_REG16                           RSVD_09;
    CPU_REG16                           PIPECFG;
    CPU_REG16                           PIPEBUF;
    CPU_REG16                           PIPEMAXP;
    CPU_REG16                           PIPEPERI;

    CPU_REG16                           PIPExCTR[15u];
    CPU_REG16                           RSVD_10;

    USBD_RENESAS_USBHS_TRANSACTION_CTR  PIPExTR[12u];
    CPU_REG16                           RSVD_11[8u];

    CPU_REG16                           DEVADDx[11u];
    CPU_REG16                           RSVD_12[14u];

    CPU_REG16                           SUSPMODE;
} USBD_RENESAS_USBHS_REG;

                                                                /* ----------- RENESAS USBHS CTRLR VARIANCE ----------- */
typedef  enum  usbd_renesas_usbhs_ctrlr {
    USBD_RENESAS_USBHS_CTRLR_RZ = 0u,
    USBD_RENESAS_USBHS_CTRLR_RX64M,
} USBD_RENESAS_USBHS_CTRLR;


/*
*********************************************************************************************************
*                                         PIPE INFO DATA TYPE
*********************************************************************************************************
*/

typedef  struct  usbd_drv_pipe_info {
    CPU_INT16U    TotBufLen;                                    /* Indicates the total len alloc for this pipe in FIFO. */
    CPU_INT16U    MaxBufLen;                                    /* Max len of a single buf.                             */
    CPU_INT08U    PipebufStartIx;                               /* Buf start ix in FIFO for this pipe.                  */

    CPU_BOOLEAN   UseDblBuf;                                    /* Indicates if pipe use double buffering.              */
    CPU_BOOLEAN   UseContinMode;                                /* Indicates if pipe use continuous mode (xfer based).  */

    CPU_INT16U    MaxPktSize;                                   /* Max pkt size of EP associated to this pipe.          */

    CPU_INT08U    FIFO_IxUsed;                                  /* Ix of the FIFO channel used with this pipe.          */
} USBD_DRV_PIPE_INFO;


/*
*********************************************************************************************************
*                                         DFIFO INFO DATA TYPE
*********************************************************************************************************
*/

typedef  struct  usbd_drv_dfifo_info {
    CPU_INT08U    EP_LogNbr;                                    /* Log nbr of EP currently using this FIFO channel.     */
    CPU_BOOLEAN   XferIsRd;                                     /* Direction of current xfer.                           */
    CPU_INT08U   *BufPtr;                                       /* Ptr to buf currently processed by this fifo ch.      */
    CPU_INT32U    BufLen;                                       /* Len of buf currently processed by this fifo ch.      */

                                                                /* ---------- CNTS & FLAGS USED WITH TX XFER ---------- */
    CPU_INT32U    CopyDataCnt;                                  /* Cnt of data copied and ready to be transmitted.      */
    CPU_INT32U    CurDmaTxLen;                                  /* Len of cur DMA xfer.                                 */
    CPU_INT08U    RemByteCnt;                                   /* Remaining byte cnt to copy to FIFO manually.         */

                                                                /* ---------- CNTS & FLAGS USED WITH RX XFER ---------- */
    CPU_INT08U    DMA_XferNewestIx;                             /* Index of newest xfer len added to list.              */
    CPU_INT08U    DMA_XferOldestIx;                             /* Index of oldest xfer len added to list.              */
    CPU_INT32U    DMA_XferTbl[RENESAS_USBHS_RX_Q_SIZE];         /* Table that contains list of xfer len for DMA.        */
    CPU_BOOLEAN   XferEndFlag;                                  /* Flag that indicates if xfer is complete.             */
    CPU_INT32U    RxLen;                                        /* Receive len (for OUT EP).                            */

                                                                /* --------- CNTS & FLAGS USED WITH ALL XFER ---------- */
    USBD_ERR      Err;                                          /* Xfer error code.                                     */
    CPU_INT32U    USB_XferByteCnt;                              /* Cur byte cnt of USB xfer.                            */
    CPU_INT32U    DMA_XferByteCnt;                              /* Cur byte cnt of DMA xfer.                            */
} USBD_DRV_DFIFO_INFO;


/*
*********************************************************************************************************
*                                   DRIVER INTERNAL DATA DATA TYPE
*********************************************************************************************************
*/

typedef  struct  usbd_drv_data {
    CPU_BOOLEAN               DMA_En;                           /* Indicates if DMA should be used when possible.       */
    CPU_INT08U                AvailDFIFO;                       /* Bitmap indicates available DFIFO ch.                 */
    CPU_BOOLEAN               NoZLP;                            /* Indicates a ZLP should not be sent.                  */
    CPU_BOOLEAN               IssueSetAddr;                     /* If SetAddress setup pkt should be sent to core.      */
    CPU_BOOLEAN               CtrlRdStatusStart;                /* Flags that indicate that ZLP was issued.             */
    CPU_BOOLEAN               CtrlWrStatusStart;

    USBD_DRV_LIB_SETUP_PKT_Q  SetupPktQ;                        /* Setup packet queue.                                  */

                                                                /* Array of pipe info.                                  */
    USBD_DRV_PIPE_INFO        PipeInfoTbl[RENESAS_USBHS_PIPE_QTY_MAX];

                                                                /* Array of DFIFO info.                                 */
    USBD_DRV_DFIFO_INFO       DFIFO_InfoTbl[RENESAS_USBHS_DFIFO_QTY_MAX];

    USBD_RENESAS_USBHS_CTRLR  Ctrlr;
} USBD_DRV_DATA;


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

static  void         USBD_DrvInitRZ_DMA    (USBD_DRV     *p_drv,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvInitRZ_FIFO   (USBD_DRV     *p_drv,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvInitRX64M_DMA (USBD_DRV     *p_drv,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvInitRX64M_FIFO(USBD_DRV     *p_drv,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvStart         (USBD_DRV     *p_drv,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvStop          (USBD_DRV     *p_drv);

static  CPU_INT16U   USBD_DrvFrameNbrGet   (USBD_DRV     *p_drv);

static  void         USBD_DrvEP_Open       (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U    ep_type,
                                            CPU_INT16U    max_pkt_size,
                                            CPU_INT08U    transaction_frame,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvEP_Close      (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr);

static  CPU_INT32U   USBD_DrvEP_RxStartDMA (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U   *p_buf,
                                            CPU_INT32U    buf_len,
                                            USBD_ERR     *p_err);

static  CPU_INT32U   USBD_DrvEP_RxStartFIFO(USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U   *p_buf,
                                            CPU_INT32U    buf_len,
                                            USBD_ERR     *p_err);

static  CPU_INT32U   USBD_DrvEP_RxDMA      (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U   *p_buf,
                                            CPU_INT32U    buf_len,
                                            USBD_ERR     *p_err);

static  CPU_INT32U   USBD_DrvEP_RxFIFO     (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U   *p_buf,
                                            CPU_INT32U    buf_len,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvEP_RxZLP      (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            USBD_ERR     *p_err);

static  CPU_INT32U   USBD_DrvEP_TxDMA      (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U   *p_buf,
                                            CPU_INT32U    buf_len,
                                            USBD_ERR     *p_err);

static  CPU_INT32U   USBD_DrvEP_TxFIFO     (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U   *p_buf,
                                            CPU_INT32U    buf_len,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvEP_TxStartDMA (USBD_DRV     *p_drv,
                                            CPU_INT08U    ep_addr,
                                            CPU_INT08U   *p_buf,
                                            CPU_INT32U    buf_len,
                                            USBD_ERR     *p_err);

static  void         USBD_DrvEP_TxStartFIFO(USBD_DRV     *p_drv,
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

static  void         USBD_RenesasUSBHS_Init            (USBD_DRV                *p_drv,
                                                        USBD_ERR                *p_err);

static  CPU_BOOLEAN  USBD_RenesasUSBHS_CFIFO_Rd        (USBD_RENESAS_USBHS_REG  *p_reg,
                                                        CPU_INT08U               ep_log_nbr,
                                                        CPU_INT08U              *p_buf,
                                                        CPU_INT32U               buf_len,
                                                        CPU_INT32U              *p_rx_len);

static  CPU_BOOLEAN  USBD_RenesasUSBHS_CFIFO_Wr        (USBD_RENESAS_USBHS_REG  *p_reg,
                                                        USBD_DRV_PIPE_INFO      *p_pipe_info,
                                                        CPU_INT08U               ep_log_nbr,
                                                        CPU_INT08U              *p_buf,
                                                        CPU_INT32U               buf_len);

static  CPU_BOOLEAN  USBD_RenesasUSBHS_DFIFO_Rd        (USBD_DRV                *p_drv,
                                                        USBD_RENESAS_USBHS_REG  *p_reg,
                                                        USBD_DRV_PIPE_INFO      *p_pipe_info,
                                                        USBD_DRV_DFIFO_INFO     *p_dfifo_info,
                                                        CPU_INT08U               dev_nbr,
                                                        CPU_INT08U               dfifo_nbr,
                                                        CPU_INT08U               ep_log_nbr);

static  CPU_BOOLEAN  USBD_RenesasUSBHS_DFIFO_Wr        (USBD_RENESAS_USBHS_REG  *p_reg,
                                                        USBD_DRV_PIPE_INFO      *p_pipe_info,
                                                        USBD_DRV_DFIFO_INFO     *p_dfifo_info,
                                                        CPU_INT08U               dev_nbr,
                                                        CPU_INT08U               dfifo_nbr,
                                                        CPU_INT08U               ep_log_nbr);

static  void         USBD_RenesasUSBHS_DFIFO_RemBytesWr(USBD_RENESAS_USBHS_REG  *p_reg,
                                                        CPU_INT08U              *p_buf,
                                                        CPU_INT08U               rem_bytes_cnt,
                                                        CPU_INT08U               dfifo_ch_nbr);

static  void         USBD_RenesasUSBHS_DFIFO_RemBytesRd(USBD_RENESAS_USBHS_REG  *p_reg,
                                                        CPU_INT08U              *p_buf,
                                                        CPU_INT08U               rem_bytes_cnt,
                                                        CPU_INT08U               dfifo_ch_nbr);

static  CPU_INT08U   USBD_RenesasUSBHS_FIFO_Acquire    (USBD_DRV_DATA           *p_drv_data,
                                                        CPU_INT08U               ep_log_nbr);

static  CPU_BOOLEAN  USBD_RenesasUSBHS_EP_PID_Set      (USBD_RENESAS_USBHS_REG  *p_reg,
                                                        CPU_INT08U               ep_log_nbr,
                                                        CPU_INT08U               resp_pid);

static  CPU_BOOLEAN  USBD_RenesasUSBHS_CurPipeSet      (CPU_REG16               *p_fifosel_reg,
                                                        CPU_INT08U               ep_log_nbr,
                                                        CPU_BOOLEAN              dir_in);

static  void         USBD_RenesasUSBHS_ResetEvent      (USBD_DRV_DATA           *p_drv_data);

static  void         USBD_RenesasUSBHS_BRDY_Event      (USBD_DRV                *p_drv,
                                                        CPU_INT08U               ep_log_nbr);

static  void         USBD_RenesasUSBHS_BEMP_Event      (USBD_DRV                *p_drv,
                                                        CPU_INT08U               ep_log_nbr);

static  void         USBD_RenesasUSBHS_DFIFO_Event     (USBD_DRV                *p_drv,
                                                        CPU_INT08U               dfifo_nbr);


/*
*********************************************************************************************************
*                                  EXTERNAL FUNCTIONS DEFINED IN BSP
*********************************************************************************************************
*/

void         USBD_BSP_DlyUs         (CPU_INT32U    us);

CPU_BOOLEAN  USBD_BSP_DMA_CopyStart (CPU_INT08U    dev_nbr,
                                     CPU_INT08U    dfifo_nbr,
                                     CPU_BOOLEAN   is_rd,
                                     void         *buf_ptr,
                                     CPU_REG32    *dfifo_addr,
                                     CPU_INT32U    xfer_len);

CPU_INT08U  USBD_BSP_DMA_ChStatusGet(CPU_INT08U    dev_nbr,
                                     CPU_INT08U    dfifo_nbr);

void        USBD_BSP_DMA_ChStatusClr(CPU_INT08U    dev_nbr,
                                     CPU_INT08U    dfifo_nbr);


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

/*
*********************************************************************************************************
*                           USB DEVICE CONTROLLER DRIVER API FOR RENESAS RZ
*********************************************************************************************************
*/

                                                                /* ----- RENESAS USBHS DRIVER DMA IMPLEMENTATION ------ */
USBD_DRV_API  USBD_DrvAPI_RenesasRZ_DMA = { USBD_DrvInitRZ_DMA,
                                            USBD_DrvStart,
                                            USBD_DrvStop,
                                            DEF_NULL,
                                            DEF_NULL,
                                            DEF_NULL,
                                            DEF_NULL,
                                            USBD_DrvFrameNbrGet,
                                            USBD_DrvEP_Open,
                                            USBD_DrvEP_Close,
                                            USBD_DrvEP_RxStartDMA,
                                            USBD_DrvEP_RxDMA,
                                            USBD_DrvEP_RxZLP,
                                            USBD_DrvEP_TxDMA,
                                            USBD_DrvEP_TxStartDMA,
                                            USBD_DrvEP_TxZLP,
                                            USBD_DrvEP_Abort,
                                            USBD_DrvEP_Stall,
                                            USBD_DrvISR_Handler,
};

                                                                /* ----- RENESAS USBHS DRIVER FIFO IMPLEMENTATION ----- */
USBD_DRV_API  USBD_DrvAPI_RenesasRZ_FIFO = { USBD_DrvInitRZ_FIFO,
                                             USBD_DrvStart,
                                             USBD_DrvStop,
                                             DEF_NULL,
                                             DEF_NULL,
                                             DEF_NULL,
                                             DEF_NULL,
                                             USBD_DrvFrameNbrGet,
                                             USBD_DrvEP_Open,
                                             USBD_DrvEP_Close,
                                             USBD_DrvEP_RxStartFIFO,
                                             USBD_DrvEP_RxFIFO,
                                             USBD_DrvEP_RxZLP,
                                             USBD_DrvEP_TxFIFO,
                                             USBD_DrvEP_TxStartFIFO,
                                             USBD_DrvEP_TxZLP,
                                             USBD_DrvEP_Abort,
                                             USBD_DrvEP_Stall,
                                             USBD_DrvISR_Handler,
};


/*
*********************************************************************************************************
*                         USB DEVICE CONTROLLER DRIVER API FOR RENESAS RX64M
*********************************************************************************************************
*/

                                                                /* ----- RENESAS USBHS DRIVER DMA IMPLEMENTATION ------ */
USBD_DRV_API  USBD_DrvAPI_RenesasRX64M_DMA = { USBD_DrvInitRX64M_DMA,
                                               USBD_DrvStart,
                                               USBD_DrvStop,
                                               DEF_NULL,
                                               DEF_NULL,
                                               DEF_NULL,
                                               DEF_NULL,
                                               USBD_DrvFrameNbrGet,
                                               USBD_DrvEP_Open,
                                               USBD_DrvEP_Close,
                                               USBD_DrvEP_RxStartDMA,
                                               USBD_DrvEP_RxDMA,
                                               USBD_DrvEP_RxZLP,
                                               USBD_DrvEP_TxDMA,
                                               USBD_DrvEP_TxStartDMA,
                                               USBD_DrvEP_TxZLP,
                                               USBD_DrvEP_Abort,
                                               USBD_DrvEP_Stall,
                                               USBD_DrvISR_Handler,
};

                                                                /* ----- RENESAS USBHS DRIVER FIFO IMPLEMENTATION ----- */
USBD_DRV_API  USBD_DrvAPI_RenesasRX64M_FIFO = { USBD_DrvInitRX64M_FIFO,
                                                USBD_DrvStart,
                                                USBD_DrvStop,
                                                DEF_NULL,
                                                DEF_NULL,
                                                DEF_NULL,
                                                DEF_NULL,
                                                USBD_DrvFrameNbrGet,
                                                USBD_DrvEP_Open,
                                                USBD_DrvEP_Close,
                                                USBD_DrvEP_RxStartFIFO,
                                                USBD_DrvEP_RxFIFO,
                                                USBD_DrvEP_RxZLP,
                                                USBD_DrvEP_TxFIFO,
                                                USBD_DrvEP_TxStartFIFO,
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
*                                        USBD_DrvInitRZ_DMA()
*
* Description : Initialize the device. Allows driver to use DMA data transfers.
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

static  void  USBD_DrvInitRZ_DMA (USBD_DRV  *p_drv,
                                  USBD_ERR  *p_err)
{
    USBD_DRV_DATA  *p_drv_data;


    USBD_RenesasUSBHS_Init(p_drv, p_err);

    if (*p_err == USBD_ERR_NONE) {
        p_drv_data         = (USBD_DRV_DATA *)p_drv->DataPtr;
        p_drv_data->DMA_En =  DEF_ENABLED;                      /* Indicates that driver can use DMA.                   */
        p_drv_data->Ctrlr  =  USBD_RENESAS_USBHS_CTRLR_RZ;      /* Indicates that driver used on RZ.                    */
    }
}


/*
*********************************************************************************************************
*                                         USBD_DrvInitRZ_FIFO()
*
* Description : Initialize the device. Forces driver to use FIFO mode for data transfers.
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

static  void  USBD_DrvInitRZ_FIFO (USBD_DRV  *p_drv,
                                   USBD_ERR  *p_err)
{
    USBD_DRV_DATA  *p_drv_data;


    USBD_RenesasUSBHS_Init(p_drv, p_err);

    if (*p_err == USBD_ERR_NONE) {
        p_drv_data         = (USBD_DRV_DATA *)p_drv->DataPtr;
        p_drv_data->DMA_En =  DEF_DISABLED;                     /* Indicate to driver that DMA is disabled.             */
        p_drv_data->Ctrlr  =  USBD_RENESAS_USBHS_CTRLR_RZ;      /* Indicates that driver used on RZ.                    */
    }
}


/*
*********************************************************************************************************
*                                       USBD_DrvInitRX64M_DMA()
*
* Description : Initialize the device. Allows driver to use DMA data transfers.
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

static  void  USBD_DrvInitRX64M_DMA (USBD_DRV  *p_drv,
                                     USBD_ERR  *p_err)
{
    USBD_DRV_DATA  *p_drv_data;


    USBD_RenesasUSBHS_Init(p_drv, p_err);

    if (*p_err == USBD_ERR_NONE) {
        p_drv_data         = (USBD_DRV_DATA *)p_drv->DataPtr;
        p_drv_data->DMA_En =  DEF_ENABLED;                      /* Indicates that driver can use DMA.                   */
        p_drv_data->Ctrlr  =  USBD_RENESAS_USBHS_CTRLR_RX64M;   /* Indicates that driver used on RX64M.                 */
    }
}


/*
*********************************************************************************************************
*                                      USBD_DrvInitRX64M_FIFO()
*
* Description : Initialize the device. Forces driver to use FIFO mode for data transfers.
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

static  void  USBD_DrvInitRX64M_FIFO (USBD_DRV  *p_drv,
                                      USBD_ERR  *p_err)
{
    USBD_DRV_DATA  *p_drv_data;


    USBD_RenesasUSBHS_Init(p_drv, p_err);

    if (*p_err == USBD_ERR_NONE) {
        p_drv_data         = (USBD_DRV_DATA *)p_drv->DataPtr;
        p_drv_data->DMA_En =  DEF_DISABLED;                     /* Indicate to driver that DMA is disabled.             */
        p_drv_data->Ctrlr  =  USBD_RENESAS_USBHS_CTRLR_RX64M;   /* Indicates that driver used on RX64M.                 */
    }
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
* Note(s)     : (1) Typically, the start function activates the pull-up on the D+ or D- pin to simulate
*                   attachment to host. Some MCUs/MPUs have an internal pull-down that is activated by a
*                   device controller register; for others, this may be a GPIO pin. Additionally,
*                   interrupts for reset and suspend are activated.
*
*               (2) This platform typically has 2 USB controllers (USB0 and USB1). Only USB0 can enable
*                   the PLL for both USB controllers. If USB1 is used without USB0, the PLL must be
*                   enabled in USB0 from the BSP init() function.
*********************************************************************************************************
*/

static  void  USBD_DrvStart (USBD_DRV  *p_drv,
                             USBD_ERR  *p_err)
{
    CPU_INT08U               dfifo_cnt;
    USBD_DRV_BSP_API        *p_bsp_api;
    USBD_RENESAS_USBHS_REG  *p_reg;
    USBD_DRV_DATA           *p_drv_data;


    p_bsp_api  =  p_drv->BSP_API_Ptr;                            /* Get driver BSP API reference.                        */
    p_reg      = (USBD_RENESAS_USBHS_REG *)p_drv->CfgPtr->BaseAddr;
    p_drv_data = (USBD_DRV_DATA          *)p_drv->DataPtr;

    if (p_drv_data->Ctrlr == USBD_RENESAS_USBHS_CTRLR_RZ) {
        DEF_BIT_CLR(p_reg->SUSPMODE,                            /* Suspend UTMI.                                        */
                    RENESAS_USBHS_SUSPMODE_SUSPM);

        p_reg->SYSCFG0 = RENESAS_USBHS_SYSCFG0_UPLLE;           /* Enable PLL. See note (2).                            */

        USBD_OS_DlyMs(1u);                                      /* Apply short delay to suspend UTMI.                   */

        p_reg->SUSPMODE = RENESAS_USBHS_SUSPMODE_SUSPM;         /* Resume UTMI.                                         */

        USBD_OS_DlyMs(50u);                                     /* Apply short delay so UTMI can resume.                */
    }

    DEF_BIT_CLR(p_reg->SYSCFG0,                                 /* Force dev mode.                                      */
                RENESAS_USBHS_SYSCFG0_DCFM);

    if (p_drv->CfgPtr->Spd == USBD_DEV_SPD_HIGH) {
        DEF_BIT_SET(p_reg->SYSCFG0, RENESAS_USBHS_SYSCFG0_HSE); /* Enable High-Speed.                                   */
    }


    if (p_bsp_api->Conn != DEF_NULL) {
        p_bsp_api->Conn();                                      /* Call board/chip specific connect function.           */
    }

    DEF_BIT_SET(p_reg->SYSCFG0, RENESAS_USBHS_SYSCFG0_USBE);    /* Enable USB controller.                               */

    p_reg->INTENB0 = (RENESAS_USBHS_INTENB0_VBSE  |             /* En VBUS int.                                         */
                      RENESAS_USBHS_INTENB0_RSME  |             /* En resume int.                                       */
                      RENESAS_USBHS_INTENB0_DVSE  |             /* En dev state transition int.                         */
                      RENESAS_USBHS_INTENB0_CTRE  |             /* En ctrl xfer stage Transition int.                   */
                      RENESAS_USBHS_INTENB0_BEMPE |             /* En buffer empty int.                                 */
                      RENESAS_USBHS_INTENB0_BRDYE);             /* En buffer ready int.                                 */

    p_reg->BRDYENB = DEF_BIT_NONE;                              /* Clr buffer     ready enable.                         */
    p_reg->NRDYENB = DEF_BIT_NONE;                              /* Clr buffer not ready enable.                         */
    p_reg->BEMPENB = DEF_BIT_NONE;                              /* Clr buffer empty     enable.                         */

    for (dfifo_cnt = 0u; dfifo_cnt < USBD_DrvDFIFO_Qty[p_drv_data->Ctrlr]; dfifo_cnt++) {
        p_reg->DxFIFOn[dfifo_cnt].SEL = DEF_BIT_NONE;
    }

    USBD_OS_DlyMs(10u);                                         /* Apply short delay before enabling pull up on D+.     */

                                                                /* Conn to host by activating the pull-up on D+ pin.    */
    DEF_BIT_SET(p_reg->SYSCFG0, RENESAS_USBHS_SYSCFG0_DPRPU);   /* En pulling up of the D+ line.                        */

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
    USBD_DRV_BSP_API        *p_bsp_api;
    USBD_RENESAS_USBHS_REG  *p_reg;


    p_bsp_api =  p_drv->BSP_API_Ptr;                            /* Get driver BSP API reference.                        */
    p_reg     = (USBD_RENESAS_USBHS_REG *)p_drv->CfgPtr->BaseAddr;


    p_reg->BRDYENB = DEF_BIT_NONE;                              /* Clr 'Buffer Ready'     int reg.                      */
    p_reg->NRDYENB = DEF_BIT_NONE;                              /* Clr 'Buffer Not Ready' int reg.                      */
    p_reg->BEMPENB = DEF_BIT_NONE;                              /* Clr 'Buffer Empty'     int reg.                      */

    DEF_BIT_CLR(p_reg->SYSCFG0, RENESAS_USBHS_SYSCFG0_USBE);    /* Disable USB controller.                              */

    USBD_OS_DlyMs(1u);

    DEF_BIT_CLR(p_reg->SUSPMODE, RENESAS_USBHS_SUSPMODE_SUSPM); /* Suspend UTMI.                                        */

    p_reg->SYSCFG0 = 0u;

    if (p_bsp_api->Disconn != DEF_NULL) {
        p_bsp_api->Disconn();
    }
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
    CPU_INT16U               frm_nbr;
    USBD_RENESAS_USBHS_REG  *p_reg;


    p_reg = (USBD_RENESAS_USBHS_REG *)p_drv->CfgPtr->BaseAddr;

                                                                /* Bit 0..10  represent frame number.                   */
    frm_nbr  = (p_reg->FRMNUM  & RENESAS_USBHS_FRMNUM_FRNM_MASK);

                                                                /* Bit 11..13 represent micro frame number.             */
    frm_nbr |= (p_reg->UFRMNUM & RENESAS_USBHS_UFRMNUM_UFRNM_MASK) << 11u;

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
*                       the endpoint is successfully configured (or  realized  or  mapped ). For some
*                       device controllers, this may not be necessary.
*
*               (2) If the endpoint address is valid, then the endpoint open function should validate
*                   the attributes allowed by the hardware endpoint :
*
*                   (a) The maximum packet size 'max_pkt_size' should be validated to match hardware
*                       capabilities.
*
*               (3) Depending on the buffer length available for the given (isochronous or bulk) pipe,
*                   this function will enable double buffering and continuous mode (in this order).
*
*                   (a) If buffer length >= (2 * max_pkt_size), then double buffering is enabled.
*
*                       (1) If each buffer length is >= (2 * max_pkt_size), then continuous mode is
*                           enabled
*
*                   (b) buffer length MUST be a multiple of the max_pkt_size.
*
*                   (c) Double buffering and continuous mode will ae enabled ONLY if DMA is used.
*********************************************************************************************************
*/

static  void  USBD_DrvEP_Open (USBD_DRV    *p_drv,
                               CPU_INT08U   ep_addr,
                               CPU_INT08U   ep_type,
                               CPU_INT16U   max_pkt_size,
                               CPU_INT08U   transaction_frame,
                               USBD_ERR    *p_err)
{
    CPU_INT08U               ep_log_nbr;
    CPU_INT08U               ep_buf_qty;
    CPU_INT16U               single_buf_len;
    CPU_INT16U               pipecfg_val;
    CPU_INT16U               pipebuf_val;
    CPU_INT16U               rounded_up_max_pkt_size;
    USBD_RENESAS_USBHS_REG  *p_reg;
    USBD_DRV_PIPE_INFO      *p_pipe_info;
    USBD_DRV_DATA           *p_drv_data;
    CPU_SR_ALLOC();


    p_reg       = (USBD_RENESAS_USBHS_REG *)p_drv->CfgPtr->BaseAddr;
    p_drv_data  = (USBD_DRV_DATA          *)p_drv->DataPtr;
    ep_log_nbr  =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_pipe_info = &p_drv_data->PipeInfoTbl[ep_log_nbr];

    if (transaction_frame > 1u) {                               /* High bandwidth not supported by ctrlr.               */
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }

    p_pipe_info->MaxBufLen = p_pipe_info->TotBufLen;

    if (ep_type == USBD_EP_TYPE_CTRL) {
        p_reg->DCPCFG  =  DEF_BIT_NONE;
        p_reg->DCPMAXP = (max_pkt_size & RENESAS_USBHS_PIPEMAXP_MXPS_MASK);
    } else {
        p_pipe_info->UseDblBuf     = DEF_NO;
        p_pipe_info->UseContinMode = DEF_NO;

        if (ep_type != USBD_EP_TYPE_INTR) {
                                                                /* Round up max pkt size on buf size base.              */
            rounded_up_max_pkt_size = (((max_pkt_size - 1u) & (~(RENESAS_USBHS_BUF_LEN - 1u))) + RENESAS_USBHS_BUF_LEN);

                                                                /* Determine configuration for this pipe.               */
            if (p_drv_data->DMA_En == DEF_ENABLED) {
                single_buf_len = p_pipe_info->TotBufLen;
                if ((single_buf_len / 2u) >= rounded_up_max_pkt_size) {
                                                                /* Use double buffering.                                */
                    single_buf_len /= 2u;
                    p_pipe_info->UseDblBuf = DEF_YES;
                    p_pipe_info->MaxBufLen = single_buf_len;

                    if (((single_buf_len / 2u) >= rounded_up_max_pkt_size) &&
                         (ep_type              == USBD_EP_TYPE_BULK)) {
                        p_pipe_info->UseContinMode = DEF_YES;   /* Use continuous mode.                                 */
                    }
                }
            } else {
                single_buf_len         = rounded_up_max_pkt_size;
                p_pipe_info->MaxBufLen = single_buf_len;
            }

            if (p_drv_data->PipeInfoTbl[ep_log_nbr].PipebufStartIx == 0u) {
               *p_err = USBD_ERR_EP_ALLOC;
                return;
            }

            ep_buf_qty  = ( single_buf_len / RENESAS_USBHS_BUF_LEN) - 1u;
            pipebuf_val = ((ep_buf_qty << 10u) & RENESAS_USBHS_PIPEBUF_BUFSIZE_MASK) |
                          (p_drv_data->PipeInfoTbl[ep_log_nbr].PipebufStartIx & RENESAS_USBHS_PIPEBUF_BUFNMB_MASK);
        } else {
            pipebuf_val = 0u;
        }

        pipecfg_val = (ep_log_nbr & RENESAS_USBHS_PIPCFG_EPNUM_MASK);

        switch (ep_type) {
            case USBD_EP_TYPE_INTR:
                 DEF_BIT_SET(pipecfg_val, RENESAS_USBHS_PIPCFG_TYPE_INTR);
                 break;


            case USBD_EP_TYPE_BULK:
                 DEF_BIT_SET(pipecfg_val, RENESAS_USBHS_PIPCFG_TYPE_BULK);
                 break;


            case USBD_EP_TYPE_CTRL:
            case USBD_EP_TYPE_ISOC:
            default:
                *p_err = USBD_ERR_INVALID_ARG;
                 return;
        }

        if (p_drv_data->DMA_En == DEF_ENABLED) {
            if (p_pipe_info->UseDblBuf == DEF_YES) {                /* Enable double buffering.                             */
                DEF_BIT_SET(pipecfg_val, RENESAS_USBHS_PIPCFG_DBLB);
            }

            if (p_pipe_info->UseContinMode == DEF_YES) {            /* Enable continuous mode.                              */
                DEF_BIT_SET(pipecfg_val, RENESAS_USBHS_PIPCFG_CNTMD);
            }
        }

        if (USBD_EP_IS_IN(ep_addr) == DEF_YES) {
            DEF_BIT_SET(pipecfg_val, RENESAS_USBHS_PIPCFG_DIR);
        } else {
            DEF_BIT_SET(pipecfg_val, RENESAS_USBHS_PIPCFG_SHTNAK);
        }

        CPU_CRITICAL_ENTER();
        p_reg->PIPESEL  = (ep_log_nbr   & RENESAS_USBHS_PIPESEL_PIPESEL_MASK);
        p_reg->PIPEMAXP = (max_pkt_size & RENESAS_USBHS_PIPEMAXP_MXPS_MASK);
        p_reg->PIPECFG  =  pipecfg_val;
        p_reg->PIPEPERI =  DEF_BIT_NONE;
        p_reg->PIPEBUF  =  pipebuf_val;
        p_reg->PIPESEL  =  0u;
        CPU_CRITICAL_EXIT();

        DEF_BIT_SET(p_reg->PIPExCTR[ep_log_nbr - 1u],           /* Reset data toggle.                                   */
                    RENESAS_USBHS_PIPExCTR_SQCLR);

        DEF_BIT_SET(p_reg->PIPExCTR[ep_log_nbr - 1u],           /* Clr FIFO.                                            */
                    RENESAS_USBHS_PIPExCTR_ACLRM);
        DEF_BIT_CLR(p_reg->PIPExCTR[ep_log_nbr - 1u],
                    RENESAS_USBHS_PIPExCTR_ACLRM);
    }

    p_pipe_info->MaxPktSize = max_pkt_size;

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
    CPU_INT08U               ep_log_nbr;
    USBD_RENESAS_USBHS_REG  *p_reg;
    CPU_SR_ALLOC();


    p_reg      = (USBD_RENESAS_USBHS_REG *)p_drv->CfgPtr->BaseAddr;
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);

    if (ep_log_nbr != 0u) {
        CPU_CRITICAL_ENTER();                                   /* Disable pipe.                                        */
        p_reg->PIPESEL  = (ep_log_nbr & RENESAS_USBHS_PIPESEL_PIPESEL_MASK);
        p_reg->PIPEMAXP =  0u;
        p_reg->PIPECFG  =  DEF_BIT_NONE;
        p_reg->PIPEPERI =  DEF_BIT_NONE;
        p_reg->PIPESEL  =  0u;
        CPU_CRITICAL_EXIT();
    }

    DEF_BIT_CLR(p_reg->BEMPENB, DEF_BIT(ep_log_nbr));           /* Disable int.                                         */
    DEF_BIT_CLR(p_reg->BRDYENB, DEF_BIT(ep_log_nbr));
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
* Return(s)   : Maximum number of octets controller can receive.
*
* Note(s)     : (1) The Renesas USBHS driver will, in order to improve the performances and benefit of
*                   some features of the controller (double buffering, continuous mode), handle the
*                   entire transfer without involving the core. When double buffering is enabled, the
*                   synchronisation between USB transfers and DMA transfers must be kept. The processing
*                   flow will be as following:
*
*                   (a) This function will mark PIPE as ready to receive data.
*
*                   (b) When one of the buffer is full, controller will raise a buffer ready (BRDY)
*                       interrupt.
*
*                       (1) If no DMA transfer is currently being processed on the DFIFO channel,
*                           a DMA transfer will be triggered at that moment to copy the received data.
*                           Otherwise, the data transfer will be queued.
*
*                       (2) End of transfer will also be evaluated at this moment. If end of
*                           transfer has occurred and a DMA transfer is being processed, a end of transfer
*                           flag will be raised. Otherwise, the transfer will complete at that moment.
*
*                   (c) When a DMA transfer completes, the DFIFO event ISR will be triggered.
*
*                       (1) Next data transfer in the queue will be submitted, if any.
*
*                       (2) Otherwise, if the end of transfer flag is set, transfer will complete.
*********************************************************************************************************
*/

static  CPU_INT32U  USBD_DrvEP_RxStartDMA (USBD_DRV    *p_drv,
                                           CPU_INT08U   ep_addr,
                                           CPU_INT08U  *p_buf,
                                           CPU_INT32U   buf_len,
                                           USBD_ERR    *p_err)
{
    CPU_BOOLEAN              valid;
    CPU_INT08U               ep_log_nbr;
    CPU_INT32U               xfer_len;
    USBD_RENESAS_USBHS_REG  *p_reg;
    USBD_DRV_DATA           *p_drv_data;
    USBD_DRV_PIPE_INFO      *p_pipe_info;
    USBD_DRV_DFIFO_INFO     *p_dfifo_info;
    CPU_SR_ALLOC();


    p_reg       = (USBD_RENESAS_USBHS_REG *)p_drv->CfgPtr->BaseAddr;
    p_drv_data  = (USBD_DRV_DATA          *)p_drv->DataPtr;
    ep_log_nbr  =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_pipe_info = &p_drv_data->PipeInfoTbl[ep_log_nbr];

    if (ep_log_nbr == 0u) {                                     /* Use CFIFO for ctrl EP.                               */
        xfer_len = USBD_DrvEP_RxStartFIFO(p_drv,
                                          ep_addr,
                                          p_buf,
                                          buf_len,
                                          p_err);

        return (xfer_len);
    }

    if (buf_len != 0u) {
                                                                /* Acquire an available DFIFO ch.                       */
        p_pipe_info->FIFO_IxUsed = USBD_RenesasUSBHS_FIFO_Acquire(p_drv_data, ep_log_nbr);
    } else {
        p_pipe_info->FIFO_IxUsed = RENESAS_USBHS_CFIFO;           /* Use CFIFO for ZLP.                                   */
    }

    if (p_pipe_info->FIFO_IxUsed  == RENESAS_USBHS_CFIFO) {
       if (p_pipe_info->UseDblBuf == DEF_YES) {
            CPU_CRITICAL_ENTER();                               /* Disable double buffering.                            */
            p_reg->PIPESEL = (ep_log_nbr & RENESAS_USBHS_PIPESEL_PIPESEL_MASK);
            DEF_BIT_CLR(p_reg->PIPECFG, RENESAS_USBHS_PIPCFG_DBLB);
            p_reg->PIPESEL =  0u;
            CPU_CRITICAL_EXIT();
        }

        xfer_len = USBD_DrvEP_RxStartFIFO(p_drv,
                                          ep_addr,
                                          p_buf,
                                          buf_len,
                                          p_err);

        return (xfer_len);
    }

    valid = USBD_RenesasUSBHS_CurPipeSet(&p_reg->DxFIFOn[p_pipe_info->FIFO_IxUsed].SEL,
                                          ep_log_nbr,
                                          DEF_NO);
    if (valid != DEF_OK) {
        goto end_err;
    }


    p_dfifo_info = &p_drv_data->DFIFO_InfoTbl[p_pipe_info->FIFO_IxUsed];

    DEF_BIT_SET(p_reg->PIPExCTR[ep_log_nbr - 1u],               /* Clr FIFO.                                            */
                RENESAS_USBHS_PIPExCTR_ACLRM);
    DEF_BIT_CLR(p_reg->PIPExCTR[ep_log_nbr - 1u],
                RENESAS_USBHS_PIPExCTR_ACLRM);

                                                                /* Init transaction counter.                            */
    if (USBD_DrvPIPETRN_LUT[p_drv_data->Ctrlr][ep_log_nbr] != RENESAS_USBHS_PIPETRN_IX_NONE) {
        p_reg->PIPExTR[USBD_DrvPIPETRN_LUT[p_drv_data->Ctrlr][ep_log_nbr]].TRN = ((buf_len - 1u) / p_pipe_info->MaxPktSize) + 1u;
        p_reg->PIPExTR[USBD_DrvPIPETRN_LUT[p_drv_data->Ctrlr][ep_log_nbr]].TRE =  RENESAS_USBHS_PIPExTRE_TRENB;
    }

    p_dfifo_info->BufPtr           = p_buf;                     /* Init DFIFO xfer flags.                               */
    p_dfifo_info->BufLen           = buf_len;
    p_dfifo_info->USB_XferByteCnt  = 0u;
    p_dfifo_info->DMA_XferByteCnt  = 0u;
    p_dfifo_info->EP_LogNbr        = ep_log_nbr;
    p_dfifo_info->XferIsRd         = DEF_YES;
    p_dfifo_info->Err              = USBD_ERR_NONE;
    p_dfifo_info->DMA_XferNewestIx = 0u;
    p_dfifo_info->DMA_XferOldestIx = 0u;
    p_dfifo_info->XferEndFlag      = DEF_NO;
    p_dfifo_info->RemByteCnt       = 0u;

    DEF_BIT_SET(p_reg->BRDYENB, DEF_BIT(ep_log_nbr));

    valid = USBD_RenesasUSBHS_EP_PID_Set(p_reg,                 /* Enable reception of data on pipe.                    */
                                         ep_log_nbr,
                                         RENESAS_USBHS_PIPExCTR_PID_BUF);
    if (valid != DEF_OK) {
        goto end_err;
    }

   *p_err    = USBD_ERR_NONE;
    xfer_len = buf_len;

    return (xfer_len);

end_err:
    DEF_BIT_SET(p_drv_data->AvailDFIFO,                         /* Free FIFO channel.                                   */
                DEF_BIT(p_pipe_info->FIFO_IxUsed));

   *p_err = USBD_ERR_RX;

    return (0u);
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
* Return(s)   : Maximum number of octets controller can receive.
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
    CPU_BOOLEAN              valid;
    CPU_INT08U               ep_log_nbr;
    CPU_INT32U               rx_len;
    USBD_RENESAS_USBHS_REG  *p_reg;
    USBD_DRV_DATA           *p_drv_data;
    USBD_DRV_PIPE_INFO      *p_pipe_info;


    (void)p_buf;

    ep_log_nbr  =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_reg       = (USBD_RENESAS_USBHS_REG *)p_drv->CfgPtr->BaseAddr;
    p_drv_data  = (USBD_DRV_DATA          *)p_drv->DataPtr;
    p_pipe_info = &p_drv_data->PipeInfoTbl[ep_log_nbr];

    p_pipe_info->FIFO_IxUsed = RENESAS_USBHS_CFIFO;

    if (p_pipe_info->UseContinMode == DEF_NO) {
        rx_len = DEF_MIN(p_pipe_info->MaxPktSize, buf_len);
    } else {
        rx_len = DEF_MIN(p_pipe_info->MaxBufLen, buf_len);
    }

    if ((ep_log_nbr == 0u) &&
        (buf_len    == 0u)) {

        valid = USBD_RenesasUSBHS_EP_PID_Set(p_reg,
                                             ep_log_nbr,
                                             RENESAS_USBHS_PIPExCTR_PID_BUF);
        if (valid != DEF_OK) {
           *p_err = USBD_ERR_RX;
            return (0u);
        }

        DEF_BIT_SET(p_reg->DCPCTR, RENESAS_USBHS_DCPCTR_CCPL);  /* Complete control xfer status stage.                  */

       *p_err = USBD_ERR_NONE;
        return (rx_len);
    }

    if (ep_log_nbr != 0u) {
        DEF_BIT_SET(p_reg->PIPExCTR[ep_log_nbr - 1u],           /* Clr FIFO.                                            */
                    RENESAS_USBHS_PIPExCTR_ACLRM);
        DEF_BIT_CLR(p_reg->PIPExCTR[ep_log_nbr - 1u],
                    RENESAS_USBHS_PIPExCTR_ACLRM);
    }

    DEF_BIT_SET(p_reg->BRDYENB, DEF_BIT(ep_log_nbr));

    valid = USBD_RenesasUSBHS_EP_PID_Set(p_reg,
                                         ep_log_nbr,
                                         RENESAS_USBHS_PIPExCTR_PID_BUF);
    if (valid == DEF_OK) {
       *p_err = USBD_ERR_NONE;
    } else {
       *p_err = USBD_ERR_RX;
    }

    return (rx_len);
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
    CPU_INT08U               ep_log_nbr;
    CPU_INT32U               rx_len;
    USBD_DRV_DATA           *p_drv_data;
    USBD_DRV_PIPE_INFO      *p_pipe_info;
    USBD_DRV_DFIFO_INFO     *p_dfifo_info;
    USBD_RENESAS_USBHS_REG  *p_reg;
    CPU_SR_ALLOC();


    ep_log_nbr   =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_drv_data   = (USBD_DRV_DATA *)p_drv->DataPtr;
    p_pipe_info  = &p_drv_data->PipeInfoTbl[ep_log_nbr];
    p_dfifo_info = &p_drv_data->DFIFO_InfoTbl[p_pipe_info->FIFO_IxUsed];
    p_reg        = (USBD_RENESAS_USBHS_REG *)p_drv->CfgPtr->BaseAddr;

    if (p_pipe_info->FIFO_IxUsed != RENESAS_USBHS_CFIFO) {
        rx_len = p_dfifo_info->USB_XferByteCnt;

       *p_err = p_dfifo_info->Err;

        CPU_CRITICAL_ENTER();
        DEF_BIT_SET(p_drv_data->AvailDFIFO,                     /* Free FIFO channel.                                   */
                    DEF_BIT(p_pipe_info->FIFO_IxUsed));
        CPU_CRITICAL_EXIT();
    } else {
        rx_len = USBD_DrvEP_RxFIFO(p_drv,
                                   ep_addr,
                                   p_buf,
                                   buf_len,
                                   p_err);

        if (p_pipe_info->UseDblBuf == DEF_YES) {
            CPU_CRITICAL_ENTER();                               /* Re-enable double buffering.                          */
            p_reg->PIPESEL = (ep_log_nbr & RENESAS_USBHS_PIPESEL_PIPESEL_MASK);
            DEF_BIT_SET(p_reg->PIPECFG, RENESAS_USBHS_PIPCFG_DBLB);
            p_reg->PIPESEL =  0u;
            CPU_CRITICAL_EXIT();
        }

        if (rx_len > buf_len) {
           *p_err  = USBD_ERR_RX;
            rx_len = buf_len;
        }
    }

    return (rx_len);
}


/*
*********************************************************************************************************
*                                         USBD_DrvEP_RxFIFO()
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

static  CPU_INT32U  USBD_DrvEP_RxFIFO (USBD_DRV    *p_drv,
                                       CPU_INT08U   ep_addr,
                                       CPU_INT08U  *p_buf,
                                       CPU_INT32U   buf_len,
                                       USBD_ERR    *p_err)
{
    CPU_BOOLEAN              valid;
    CPU_INT08U               ep_log_nbr;
    CPU_INT32U               rx_len;
    USBD_RENESAS_USBHS_REG  *p_reg;
    CPU_SR_ALLOC();


    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_reg      = (USBD_RENESAS_USBHS_REG *)p_drv->CfgPtr->BaseAddr;
    rx_len     =  0u;

    CPU_CRITICAL_ENTER();
    valid = USBD_RenesasUSBHS_CFIFO_Rd(p_reg,
                                       ep_log_nbr,
                                       p_buf,
                                       buf_len,
                                      &rx_len);
    CPU_CRITICAL_EXIT();
    if ((valid  == DEF_OK) &&
        (rx_len <= buf_len)) {
       *p_err  = USBD_ERR_NONE;
    } else if (rx_len > buf_len) {
       *p_err  = USBD_ERR_RX;
        rx_len = buf_len;
    } else {
       *p_err  = USBD_ERR_RX;
    }

    return (rx_len);
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
    USBD_DRV_DATA  *p_drv_data;
    CPU_INT08U      ep_log_nbr;


    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);

    if (ep_log_nbr == 0u) {
        USBD_DrvLib_SetupPktQSubmitNext(&p_drv_data->SetupPktQ, p_drv);
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

static  CPU_INT32U  USBD_DrvEP_TxDMA (USBD_DRV    *p_drv,
                                      CPU_INT08U   ep_addr,
                                      CPU_INT08U  *p_buf,
                                      CPU_INT32U   buf_len,
                                      USBD_ERR    *p_err)
{
    CPU_BOOLEAN              valid;
    CPU_INT08U               ep_log_nbr;
    CPU_INT32U               tx_len;
    USBD_DRV_DATA           *p_drv_data;
    USBD_DRV_PIPE_INFO      *p_pipe_info;
    USBD_RENESAS_USBHS_REG  *p_reg;
    CPU_SR_ALLOC();


    (void)p_buf;

    p_drv_data  = (USBD_DRV_DATA *)p_drv->DataPtr;
    ep_log_nbr  =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_pipe_info = &p_drv_data->PipeInfoTbl[ep_log_nbr];
    p_reg       = (USBD_RENESAS_USBHS_REG *)p_drv->CfgPtr->BaseAddr;

                                                                /* Acquire an available FIFO channel.                   */
    p_pipe_info->FIFO_IxUsed = USBD_RenesasUSBHS_FIFO_Acquire(p_drv_data,
                                                              ep_log_nbr);
    if (p_pipe_info->FIFO_IxUsed == RENESAS_USBHS_CFIFO) {
        if (p_pipe_info->UseDblBuf == DEF_YES) {
            CPU_CRITICAL_ENTER();                               /* Disable double buffering.                            */
            p_reg->PIPESEL = (ep_log_nbr & RENESAS_USBHS_PIPESEL_PIPESEL_MASK);
            DEF_BIT_CLR(p_reg->PIPECFG, RENESAS_USBHS_PIPCFG_DBLB);
            p_reg->PIPESEL =  0u;
            CPU_CRITICAL_EXIT();
        }

        tx_len = USBD_DrvEP_TxFIFO(p_drv,
                                   ep_addr,
                                   p_buf,
                                   buf_len,
                                   p_err);

        return (tx_len);
    }

                                                                /* Set current pipe to alloc DFIFO channel.             */
    valid = USBD_RenesasUSBHS_CurPipeSet(&p_reg->DxFIFOn[p_pipe_info->FIFO_IxUsed].SEL,
                                          ep_log_nbr,
                                          DEF_NO);
    if (valid == DEF_OK) {
        tx_len = buf_len;                                       /* Xfer is handle completely by driver in DMA mode.     */

       *p_err = USBD_ERR_NONE;
    } else {
        CPU_CRITICAL_ENTER();                                   /* Free FIFO channel.                                   */
        DEF_BIT_SET(p_drv_data->AvailDFIFO,
                    DEF_BIT(p_pipe_info->FIFO_IxUsed));
        CPU_CRITICAL_EXIT();

        tx_len = 0u;

       *p_err = USBD_ERR_TX;
    }

    return (tx_len);
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

static  CPU_INT32U  USBD_DrvEP_TxFIFO (USBD_DRV    *p_drv,
                                       CPU_INT08U   ep_addr,
                                       CPU_INT08U  *p_buf,
                                       CPU_INT32U   buf_len,
                                       USBD_ERR    *p_err)
{
    CPU_INT08U           ep_log_nbr;
    CPU_INT16U           tx_len;
    USBD_DRV_DATA       *p_drv_data;
    USBD_DRV_PIPE_INFO  *p_pipe_info;


    (void)p_buf;

    p_drv_data             = (USBD_DRV_DATA *)p_drv->DataPtr;
    ep_log_nbr             =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_pipe_info            = &p_drv_data->PipeInfoTbl[ep_log_nbr];
    p_pipe_info->FIFO_IxUsed =  RENESAS_USBHS_CFIFO;

    if (p_pipe_info->UseContinMode == DEF_NO) {
        tx_len = DEF_MIN(p_pipe_info->MaxPktSize, buf_len);
    } else {
        tx_len = DEF_MIN(p_pipe_info->MaxBufLen, buf_len);
    }

   *p_err = USBD_ERR_NONE;

    return (tx_len);
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
* Note(s)     : (1) The Renesas USBHS driver will, in order to improve the performances and benefit of
*                   some features of the controller (double buffering, continuous mode), handle the
*                   entire transfer without involving the core. The processing flow will be as following:
*
*                   (a) This function will initiate the first DMA transfer to the first buffer.
*
*                   (b) When a DMA transfer completes, the DFIFO event ISR will be triggered. The pipe will
*                       be marked as ready to transmit data at that moment.
*
*                   (c) At the moment one of the buffer is ready to be written, the buffer ready (BRDY)
*                       is triggered. A DMA transfer is then submitted for this buffer. If it is the
*                       last buffer to be submitted, BRDY interrupts will be disabled.
*
*                   (d) When one or both buffers (if they were both full when data transmission occurred)
*                       have been transmitted, the buffer empty interrupt is triggered. If buffer
*                       transmission is complete, the transfer will be completed at that moment.
*********************************************************************************************************
*/

static  void  USBD_DrvEP_TxStartDMA (USBD_DRV    *p_drv,
                                     CPU_INT08U   ep_addr,
                                     CPU_INT08U  *p_buf,
                                     CPU_INT32U   buf_len,
                                     USBD_ERR    *p_err)
{
    CPU_BOOLEAN              valid;
    CPU_INT08U               ep_log_nbr;
    USBD_DRV_DATA           *p_drv_data;
    USBD_DRV_PIPE_INFO      *p_pipe_info;
    USBD_DRV_DFIFO_INFO     *p_dfifo_info;
    USBD_RENESAS_USBHS_REG  *p_reg;


    ep_log_nbr  =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_reg       = (USBD_RENESAS_USBHS_REG *)p_drv->CfgPtr->BaseAddr;
    p_drv_data  = (USBD_DRV_DATA          *)p_drv->DataPtr;
    p_pipe_info = &p_drv_data->PipeInfoTbl[ep_log_nbr];

    if (p_pipe_info->FIFO_IxUsed == RENESAS_USBHS_CFIFO) {
        USBD_DrvEP_TxStartFIFO(p_drv,
                               ep_addr,
                               p_buf,
                               buf_len,
                               p_err);

        return;
    }

    p_dfifo_info = &p_drv_data->DFIFO_InfoTbl[p_pipe_info->FIFO_IxUsed];

                                                                /* Clr FIFO.                                            */
    DEF_BIT_SET(p_reg->PIPExCTR[ep_log_nbr - 1u],
                RENESAS_USBHS_PIPExCTR_ACLRM);
    DEF_BIT_CLR(p_reg->PIPExCTR[ep_log_nbr - 1u],
                RENESAS_USBHS_PIPExCTR_ACLRM);

    p_dfifo_info->BufPtr          = p_buf;
    p_dfifo_info->BufLen          = buf_len;
    p_dfifo_info->USB_XferByteCnt = 0u;
    p_dfifo_info->DMA_XferByteCnt = 0u;
    p_dfifo_info->EP_LogNbr       = ep_log_nbr;
    p_dfifo_info->XferIsRd        = DEF_NO;
    p_dfifo_info->Err             = USBD_ERR_NONE;
    p_dfifo_info->CopyDataCnt     = 0u;

    DEF_BIT_SET(p_reg->BEMPENB, DEF_BIT(ep_log_nbr));           /* Enable int.                                          */
    DEF_BIT_SET(p_reg->BRDYENB, DEF_BIT(ep_log_nbr));


                                                                /* ---------------- WRITE DATA TO FIFO ---------------- */
    valid = USBD_RenesasUSBHS_DFIFO_Wr(p_reg,
                                       p_pipe_info,
                                      &p_drv_data->DFIFO_InfoTbl[p_pipe_info->FIFO_IxUsed],
                                       p_drv->DevNbr,
                                       p_pipe_info->FIFO_IxUsed,
                                       ep_log_nbr);
    if (valid == DEF_OK) {
       *p_err = USBD_ERR_NONE;
    } else {
       *p_err = USBD_ERR_TX;
    }
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
    CPU_BOOLEAN              valid;
    CPU_INT08U               ep_log_nbr;
    USBD_RENESAS_USBHS_REG  *p_reg;
    USBD_DRV_DATA           *p_drv_data;
    CPU_SR_ALLOC();


    p_reg       = (USBD_RENESAS_USBHS_REG *)p_drv->CfgPtr->BaseAddr;
    p_drv_data  = (USBD_DRV_DATA          *)p_drv->DataPtr;
    ep_log_nbr  =  USBD_EP_ADDR_TO_LOG(ep_addr);

    if (ep_log_nbr != 0u) {
        DEF_BIT_SET(p_reg->PIPExCTR[ep_log_nbr - 1u],           /* Clr FIFO.                                            */
                    RENESAS_USBHS_PIPExCTR_ACLRM);
        DEF_BIT_CLR(p_reg->PIPExCTR[ep_log_nbr - 1u],
                    RENESAS_USBHS_PIPExCTR_ACLRM);
    }

    DEF_BIT_SET(p_reg->BEMPENB, DEF_BIT(ep_log_nbr));           /* Enable int.                                          */

                                                                /* ---------------- WRITE DATA TO FIFO ---------------- */
    CPU_CRITICAL_ENTER();
    valid = USBD_RenesasUSBHS_CFIFO_Wr(p_reg,
                                      &p_drv_data->PipeInfoTbl[ep_log_nbr],
                                       ep_log_nbr,
                                       p_buf,
                                       buf_len);
    CPU_CRITICAL_EXIT();
    if (valid != DEF_OK) {
       *p_err = USBD_ERR_TX;
        return;
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
    CPU_BOOLEAN              valid;
    CPU_INT08U               ep_log_nbr;
    USBD_DRV_DATA           *p_drv_data;
    USBD_RENESAS_USBHS_REG  *p_reg;
    CPU_SR_ALLOC();


    p_reg      = (USBD_RENESAS_USBHS_REG *)p_drv->CfgPtr->BaseAddr;
    p_drv_data = (USBD_DRV_DATA          *)p_drv->DataPtr;
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);


    if (ep_log_nbr == 0u) {
        CPU_CRITICAL_ENTER();
        if (p_drv_data->NoZLP == DEF_YES) {                     /* No ZLP for SetAddress req as they are handled by hw. */
            p_drv_data->NoZLP = DEF_NO;
            USBD_EP_TxCmpl(p_drv, 0u);
        } else {
            DEF_BIT_SET(p_reg->BEMPENB, DEF_BIT(ep_log_nbr));

            valid = USBD_RenesasUSBHS_EP_PID_Set(p_reg,
                                                 ep_log_nbr,
                                                 RENESAS_USBHS_PIPExCTR_PID_BUF);

                                                                /* Enable status phase completion.                      */
            DEF_BIT_SET(p_reg->DCPCTR, RENESAS_USBHS_DCPCTR_CCPL);

            if (valid != DEF_OK) {
                CPU_CRITICAL_EXIT();

               *p_err = USBD_ERR_TX;
                return;
            }
        }
        CPU_CRITICAL_EXIT();

        USBD_DrvLib_SetupPktQSubmitNext(&p_drv_data->SetupPktQ, p_drv);
    } else {
        (void)USBD_DrvEP_TxFIFO(              p_drv,
                                              ep_addr,
                                (CPU_INT08U *)0,
                                              0u,
                                              p_err);
        if (*p_err != USBD_ERR_NONE) {
            return;
        }

        USBD_DrvEP_TxStartFIFO(              p_drv,
                                             ep_addr,
                               (CPU_INT08U *)0,
                                             0u,
                                             p_err);
        if (*p_err != USBD_ERR_NONE) {
            return;
        }
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
    CPU_BOOLEAN              valid;
    CPU_INT08U               ep_log_nbr;
    USBD_RENESAS_USBHS_REG  *p_reg;
    USBD_DRV_DATA           *p_drv_data;
    USBD_DRV_PIPE_INFO      *p_pipe_info;
    USBD_DRV_DFIFO_INFO     *p_dfifo_info;
    CPU_SR_ALLOC();


    p_reg      = (USBD_RENESAS_USBHS_REG *)p_drv->CfgPtr->BaseAddr;
    p_drv_data = (USBD_DRV_DATA *)p_drv->DataPtr;
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);


    valid = USBD_RenesasUSBHS_EP_PID_Set(p_reg,
                                         ep_log_nbr,
                                         RENESAS_USBHS_PIPExCTR_PID_NAK);

    DEF_BIT_CLR(p_reg->BEMPENB, DEF_BIT(ep_log_nbr));
    DEF_BIT_CLR(p_reg->BRDYENB, DEF_BIT(ep_log_nbr));

    p_pipe_info = &p_drv_data->PipeInfoTbl[ep_log_nbr];
    if (p_pipe_info->FIFO_IxUsed < USBD_DrvDFIFO_Qty[p_drv_data->Ctrlr]) {   /* Free FIFO if any used.                               */

        p_dfifo_info = &p_drv_data->DFIFO_InfoTbl[p_pipe_info->FIFO_IxUsed];
        if (p_dfifo_info->EP_LogNbr == ep_log_nbr) {

            CPU_CRITICAL_ENTER();
            DEF_BIT_SET(p_drv_data->AvailDFIFO,
                        DEF_BIT(p_pipe_info->FIFO_IxUsed));
            CPU_CRITICAL_EXIT();
        }
    }

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
    CPU_BOOLEAN              valid;
    CPU_INT08U               ep_log_nbr;
    USBD_DRV_DATA           *p_drv_data;
    USBD_RENESAS_USBHS_REG  *p_reg;


    p_reg      = (USBD_RENESAS_USBHS_REG *)p_drv->CfgPtr->BaseAddr;
    ep_log_nbr =  USBD_EP_ADDR_TO_LOG(ep_addr);
    p_drv_data = (USBD_DRV_DATA          *)p_drv->DataPtr;

    if (state == DEF_SET) {
        valid = USBD_RenesasUSBHS_EP_PID_Set(p_reg,
                                             ep_log_nbr,
                                             RENESAS_USBHS_PIPExCTR_PID_STALL1);

        if (ep_addr == 0x00u){
            USBD_DrvLib_SetupPktQSubmitNext(&p_drv_data->SetupPktQ, p_drv);
        }
    } else {
        valid = USBD_RenesasUSBHS_EP_PID_Set(p_reg,
                                             ep_log_nbr,
                                             RENESAS_USBHS_PIPExCTR_PID_NAK);

        if (ep_log_nbr != 0u) {                                 /* Reset data toggle.                                   */
            DEF_BIT_SET(p_reg->PIPExCTR[ep_log_nbr - 1u], RENESAS_USBHS_PIPExCTR_SQCLR);
        } else {
            DEF_BIT_SET(p_reg->DCPCTR, RENESAS_USBHS_DCPCTR_SQCLR);
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
* Return(s)   : None.
*
* Note(s)     : (1) VBSTS bit is read several times to remove the chattering effect until the same value
*                   is read repeatedly from the bit.
*
*               (2) The Renesas High-Speed USB module automatically handles any SET_ADDRESS standard
*                   request received. For the driver and the USB-Device stack to operate correctly
*                   together, it is required to :
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
              CPU_BOOLEAN              vbus1;
              CPU_BOOLEAN              vbus2;
              CPU_BOOLEAN              vbus3;
              CPU_INT08U               ep_log_nbr;
              CPU_INT08U               dfifo_status;
              CPU_INT08U               dfifo_cnt;
              CPU_INT16U               intsts0_reg;
              CPU_INT16U               brdysts_reg;
              CPU_INT16U               bempsts_reg;
    volatile  CPU_INT16U               dummy;
              CPU_INT32U               setup_pkt_buf[2u];
              CPU_INT16U              *p_setup_pkt_buf;
              USBD_DRV_DATA           *p_drv_data;
              USBD_DRV_DFIFO_INFO     *p_dfifo_info;
              USBD_RENESAS_USBHS_REG  *p_reg;


    p_reg       = (USBD_RENESAS_USBHS_REG *)p_drv->CfgPtr->BaseAddr;
    p_drv_data  = (USBD_DRV_DATA          *)p_drv->DataPtr;

    intsts0_reg    =  p_reg->INTSTS0;

    bempsts_reg    =  p_reg->BEMPSTS;
    p_reg->BEMPSTS = ~bempsts_reg;

    brdysts_reg    =  p_reg->BRDYSTS;
    p_reg->BRDYSTS = ~brdysts_reg;


                                                                /* --------------- CONN / DISCONN EVENT --------------- */
    if (DEF_BIT_IS_SET(intsts0_reg, RENESAS_USBHS_INTSTS0_VBINT) == DEF_YES) {
        p_reg->INTSTS0 = (CPU_INT16U)~((CPU_INT16U)RENESAS_USBHS_INTSTS0_VBINT);

        do {                                                    /* Ensure port debounce. See note (1).                  */
            vbus1 = DEF_BIT_IS_SET(p_reg->INTSTS0,
                                   RENESAS_USBHS_INTSTS0_VBSTS);
            USBD_BSP_DlyUs(10u);
            vbus2 = DEF_BIT_IS_SET(p_reg->INTSTS0,
                                   RENESAS_USBHS_INTSTS0_VBSTS);
            USBD_BSP_DlyUs(10u);
            vbus3 = DEF_BIT_IS_SET(p_reg->INTSTS0,
                                   RENESAS_USBHS_INTSTS0_VBSTS);
        } while ((vbus1 != vbus2) ||
                 (vbus2 != vbus3));

        if (DEF_BIT_IS_SET(p_reg->INTSTS0, RENESAS_USBHS_INTSTS0_VBSTS) == DEF_YES) {
            USBD_EventConn(p_drv);

            if (p_drv_data->Ctrlr == USBD_RENESAS_USBHS_CTRLR_RX64M) {
                DEF_BIT_SET(p_reg->SYSCFG0, RENESAS_USBHS_SYSCFG0_CNEN);
            }
        } else {
            USBD_EventDisconn(p_drv);

            if (p_drv_data->Ctrlr == USBD_RENESAS_USBHS_CTRLR_RX64M) {
                DEF_BIT_CLR(p_reg->SYSCFG0, RENESAS_USBHS_SYSCFG0_CNEN);
            }
        }
    }


                                                                /* ------------------ SUSPEND EVENT ------------------- */
    if (DEF_BIT_IS_SET(intsts0_reg, RENESAS_USBHS_INTSTS0_DVSQ_SUSP) == DEF_YES) {
        USBD_EventSuspend(p_drv);
    }


                                                                /* ------------------- RESUME EVENT ------------------- */
    if (DEF_BIT_IS_SET(intsts0_reg, RENESAS_USBHS_INTSTS0_RESM) == DEF_YES) {
        p_reg->INTSTS0 = ~((CPU_INT16U)RENESAS_USBHS_INTSTS0_RESM);
        USBD_EventResume(p_drv);
    }


                                                                /* ------------ DEVICE STATUS CHANGE EVENT ------------ */
    if (DEF_BIT_IS_SET(intsts0_reg, RENESAS_USBHS_INTSTS0_DVST) == DEF_YES) {

        switch (intsts0_reg & RENESAS_USBHS_INTSTS0_DVSQ_MASK) {
            case RENESAS_USBHS_INTSTS0_DVSQ_DFLT:               /* Default state.                                       */
                 USBD_EventReset(p_drv);

                 if ((p_reg->DVSTCTR0 & RENESAS_USBHS_DVSCTR_RHST_MASK) == RENESAS_USBHS_DVSCTR_RHST_HS) {
                     USBD_EventHS(p_drv);
                 }

                 USBD_RenesasUSBHS_ResetEvent(p_drv_data);

                 p_drv_data->IssueSetAddr = DEF_YES;
                 break;


            case RENESAS_USBHS_INTSTS0_DVSQ_ADDR:               /* Addressed state.                                     */
                 if (p_drv_data->IssueSetAddr == DEF_YES) {     /* Generate dummy SET_ADDR setup pkt. See note (2).     */

                     p_setup_pkt_buf   = (CPU_INT16U *)&setup_pkt_buf[0];
                    *p_setup_pkt_buf++ =  USBD_REQ_SET_ADDRESS << 8u;
                    *p_setup_pkt_buf++ =  p_reg->USBADDR;
                    *p_setup_pkt_buf++ =  0x0000u;
                    *p_setup_pkt_buf   =  0x0000u;

                                                                /* Enqueue setup pkt. See note (2).                     */
                     USBD_DrvLib_SetupPktQAdd(&p_drv_data->SetupPktQ,
                                               p_drv,
                                              &setup_pkt_buf[0u]);

                     p_drv_data->NoZLP        = DEF_YES;
                     p_drv_data->IssueSetAddr = DEF_NO;
                 }
                 break;


            case RENESAS_USBHS_INTSTS0_DVSQ_POWERED:            /* Powered state.                                       */
                 p_drv_data->IssueSetAddr = DEF_YES;
                 break;


            case RENESAS_USBHS_INTSTS0_DVSQ_CONFIG:             /* Configured state.                                    */
            default:
                 break;
        }

        p_reg->INTSTS0 = ~((CPU_INT16U)RENESAS_USBHS_INTSTS0_DVST);
    }


                                                                /* ------- CONTROL TRANSFER STAGE CHANGE EVENT -------- */
    if (DEF_BIT_IS_SET(intsts0_reg, RENESAS_USBHS_INTSTS0_CTRT) == DEF_YES) {

        switch (p_reg->INTSTS0 & RENESAS_USBHS_INTSTS0_CTSQ_MASK) {
            case RENESAS_USBHS_INTSTS0_CTSQ_WR_STATUS_NDATA:
                 p_drv_data->CtrlWrStatusStart = DEF_YES;
            case RENESAS_USBHS_INTSTS0_CTSQ_RD_DATA:
            case RENESAS_USBHS_INTSTS0_CTSQ_WR_DATA:

                 p_setup_pkt_buf   = (CPU_INT16U *)&setup_pkt_buf[0];
                *p_setup_pkt_buf++ =  p_reg->USBREQ;
                *p_setup_pkt_buf++ =  p_reg->USBVAL;
                *p_setup_pkt_buf++ =  p_reg->USBINDX;
                *p_setup_pkt_buf   =  p_reg->USBLENG;

                                                                /* Enqueue setup pkt. See note (2).                     */
                 USBD_DrvLib_SetupPktQAdd(&p_drv_data->SetupPktQ,
                                           p_drv,
                                          &setup_pkt_buf[0u]);

                                                                /* Clr valid setup pkt recvd.                           */
                 DEF_BIT_CLR(p_reg->INTSTS0, RENESAS_USBHS_INTSTS0_VALID);
                 break;


            case RENESAS_USBHS_INTSTS0_CTSQ_RD_STATUS:          /* Indicates ctrl rd stage has begin.                   */
                 p_drv_data->CtrlRdStatusStart = DEF_YES;
                 break;


            case RENESAS_USBHS_INTSTS0_CTSQ_WR_STATUS:          /* Indicates ctrl wr stage has begin.                   */
                 p_drv_data->CtrlWrStatusStart = DEF_YES;
                 break;


            case RENESAS_USBHS_INTSTS0_CTSQ_SETUP:              /* Indicates ctrl xfer is completed.                    */
                 if (p_drv_data->CtrlRdStatusStart == DEF_YES) {
                     USBD_EP_RxCmpl(p_drv, 0u);
                     p_drv_data->CtrlRdStatusStart = DEF_NO;
                 }

                 if (p_drv_data->CtrlWrStatusStart == DEF_YES) {
                     USBD_EP_TxCmpl(p_drv, 0u);
                     p_drv_data->CtrlWrStatusStart = DEF_NO;
                 }
                 break;


            case RENESAS_USBHS_INTSTS0_CTSQ_SEQ_ERR:
            default:
                 break;
        }

        p_reg->INTSTS0 = ~((CPU_INT16U)RENESAS_USBHS_INTSTS0_CTRT);
    }


                                                                /* --------------- DMA INTERRUPT STATUS --------------- */
    if (p_drv_data->DMA_En == DEF_ENABLED) {
        for (dfifo_cnt = 0u; dfifo_cnt < USBD_DrvDFIFO_Qty[p_drv_data->Ctrlr]; dfifo_cnt++) {
            dfifo_status = USBD_BSP_DMA_ChStatusGet(p_drv->DevNbr, dfifo_cnt);

                                                                /* DMA xfer completed.                                  */
            if (DEF_BIT_IS_SET(dfifo_status, DEF_BIT_00) == DEF_YES) {
                USBD_RenesasUSBHS_DFIFO_Event(p_drv, dfifo_cnt);

                USBD_BSP_DMA_ChStatusClr(p_drv->DevNbr, dfifo_cnt);
            } else if (DEF_BIT_IS_SET(dfifo_status, DEF_BIT_01) == DEF_YES) {
                p_dfifo_info = &p_drv_data->DFIFO_InfoTbl[dfifo_cnt];

                if (p_dfifo_info->XferIsRd == DEF_YES) {
                    p_dfifo_info->Err = USBD_ERR_RX;

                    USBD_EP_RxCmpl(p_drv, p_dfifo_info->EP_LogNbr);
                } else {
                    p_dfifo_info->Err = USBD_ERR_TX;

                    DEF_BIT_SET(p_drv_data->AvailDFIFO,         /* Mark FIFO as available.                              */
                                DEF_BIT(dfifo_cnt));

                    USBD_EP_TxCmpl(p_drv, p_dfifo_info->EP_LogNbr);
                }

                USBD_BSP_DMA_ChStatusClr(p_drv->DevNbr, dfifo_cnt);
            }
        }
    }


                                                                /* ---------- BUFFER EMPTY INTERRUPT STATUS ----------- */
    bempsts_reg &= p_reg->BEMPENB;
    while (bempsts_reg != 0u) {
        ep_log_nbr = CPU_CntTrailZeros(bempsts_reg);

        USBD_RenesasUSBHS_BEMP_Event(p_drv, ep_log_nbr);        /* Handle IN xfer completion.                           */

        DEF_BIT_CLR(bempsts_reg, DEF_BIT(ep_log_nbr));
    }


                                                                /* ---------- BUFFER READY INTERRUPT STATUS ----------- */
    brdysts_reg &= p_reg->BRDYENB;
    while (brdysts_reg != 0u) {
        ep_log_nbr = CPU_CntTrailZeros(brdysts_reg);

        USBD_RenesasUSBHS_BRDY_Event(p_drv, ep_log_nbr);

        DEF_BIT_CLR(brdysts_reg, DEF_BIT(ep_log_nbr));
    }


    dummy = p_reg->INTSTS0;                                     /* Memory barrier (Ensure reg updated in USB ctrlr).    */
    dummy = p_reg->INTSTS0;
    dummy = p_reg->INTSTS0;

    dummy = p_reg->BRDYSTS;
    dummy = p_reg->BRDYSTS;
    dummy = p_reg->BRDYSTS;

    dummy = p_reg->BEMPSTS;
    dummy = p_reg->BEMPSTS;
    dummy = p_reg->BEMPSTS;

    (void)dummy;                                               /* Avoid unwanted warning with certain compilers.       */
}


/*
*********************************************************************************************************
*                                      USBD_RenesasUSBHS_Init()
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
* Note(s)     : (1) The Renesas USB High-Speed controller uses an 8 Kbytes memory that is shared among
*                   all its pipes. This 8 Kbytes memory is divided in 128 buffers of 64 bytes each. Each
*                   pipe must have its (group of) buffer associated to it. Control and Interrupt pipes
*                   have a dedicated 64 bytes buffer each. The remaining buffers are shared among bulk and
*                   isochronous pipes. The size of the buffer must be at least equal to the max packet
*                   size of the pipe. Each isochronous and bulk pipe must define its starting buffer
*                   index and size. A buffer can have a maximum length of 2048 bytes when continuous mode
*                   is enabled. If double-buffering is used, we must reserve twice the space in the
*                   memory for the pipe. The init() function will determine the size of the buffer it must
*                   allocates for each pipe according to the MaxPktLen value of the pipe info table
*                   defined in the BSP.
*********************************************************************************************************
*/

static  void  USBD_RenesasUSBHS_Init (USBD_DRV  *p_drv,
                                      USBD_ERR  *p_err)
{
    CPU_INT08U           next_buf_ix;
    CPU_INT08U           buf_cnt;
    CPU_INT08U           ep_log_nbr;
    LIB_ERR              err_lib;
    USBD_DRV_BSP_API    *p_bsp_api;
    USBD_DRV_EP_INFO    *p_ep_info;
    USBD_DRV_DATA       *p_data;
    USBD_DRV_PIPE_INFO  *p_pipe_info;


    p_data = (USBD_DRV_DATA *)Mem_HeapAlloc(              sizeof(USBD_DRV_DATA),
                                                          sizeof(CPU_ALIGN),
                                            (CPU_SIZE_T *)0,
                                                         &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

    Mem_Clr((void *)p_data, sizeof(USBD_DRV_DATA));

    p_drv->DataPtr = (void *)p_data;

    USBD_DrvLib_SetupPktQInit(&p_data->SetupPktQ,
                              RENESAS_USBHS_SETUP_PKT_Q_SIZE,
                              p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

                                                                /* Compute next buf ix for each pipe.                   */
    next_buf_ix = RENESAS_USBHS_BUF_STARTING_IX;
    p_ep_info   = p_drv->CfgPtr->EP_InfoTbl;
    while (p_ep_info->Attrib != 0u) {
        ep_log_nbr  =  p_ep_info->Nbr;
        p_pipe_info = &p_data->PipeInfoTbl[ep_log_nbr];

        if ((DEF_BIT_IS_SET(p_ep_info->Attrib, USBD_EP_INFO_TYPE_ISOC) == DEF_YES) ||
            (DEF_BIT_IS_SET(p_ep_info->Attrib, USBD_EP_INFO_TYPE_BULK) == DEF_YES)) {

            if (ep_log_nbr < USBD_DrvPipeQty[p_data->Ctrlr]) {
                                                                /* Determine nbr of buf needed for the pipe.            */
                buf_cnt = (p_ep_info->MaxPktSize / RENESAS_USBHS_BUF_LEN);

                if (((CPU_INT08U)(next_buf_ix + buf_cnt)) <= RENESAS_USBHS_BUF_QTY_AVAIL) {
                    p_pipe_info->PipebufStartIx  = next_buf_ix;
                    p_pipe_info->TotBufLen       = p_ep_info->MaxPktSize;
                    next_buf_ix                 += buf_cnt;
                } else {
                    p_pipe_info->PipebufStartIx = 0u;
                }
            }
        } else {
            p_pipe_info->TotBufLen = p_ep_info->MaxPktSize;     /* Ctrl and intr EP have fixed buf ix/size.             */
        }

        p_ep_info++;
    }

    p_bsp_api = p_drv->BSP_API_Ptr;                             /* Get driver BSP API reference.                        */
    if (p_bsp_api->Init != DEF_NULL) {
        p_bsp_api->Init(p_drv);                                 /* Call board/chip specific device controller ...       */
                                                                /* ... initialization function.                         */
    }

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                    USBD_RenesasUSBHS_CFIFO_Rd()
*
* Description : Read data from CFIFO.
*
* Arguments   : p_reg       Pointer to registers structure.
*
*               ep_log_nbr  Endpoint logical number.
*
*               p_buf       Pointer to buffer.
*
*               buf_len     Buffer length in octets.
*
*               p_rx_len    Pointer to variable that will receive the received length.
*
* Return(s)   : DEF_OK,   if NO error(s),
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) This function requires p_buf to be aligned on a 32 bit boundary.
*
*               (2) This function must be called from a critical section.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_RenesasUSBHS_CFIFO_Rd (USBD_RENESAS_USBHS_REG  *p_reg,
                                                 CPU_INT08U               ep_log_nbr,
                                                 CPU_INT08U              *p_buf,
                                                 CPU_INT32U               buf_len,
                                                 CPU_INT32U              *p_rx_len)
{
    CPU_BOOLEAN   valid;
    CPU_INT08U    nbr_byte;
    CPU_INT08U   *p_buf8;
    CPU_INT32U    cnt;
    CPU_INT32U    nbr_word;
    CPU_INT32U    data_len;
    CPU_INT32U   *p_buf32;
    CPU_INT32U    last_word;


    valid = USBD_RenesasUSBHS_CurPipeSet(&p_reg->CFIFOSEL,
                                          ep_log_nbr,
                                          DEF_NO);
    if (valid != DEF_OK) {
        return (DEF_FAIL);
    }

   *p_rx_len = (p_reg->CFIFOCTR & RENESAS_USBHS_FIFOCTR_DTLN_MASK);
    data_len =  DEF_MIN(*p_rx_len, buf_len);

    nbr_word = data_len / 4u;
    nbr_byte = data_len % 4u;

    p_buf32 = (CPU_INT32U *)p_buf;
    for (cnt = 0u; cnt < nbr_word; cnt++) {
        p_buf32[cnt] = p_reg->CFIFO;
    }

    if (nbr_byte > 0u) {
        p_buf8 = (CPU_INT08U *)&p_buf32[cnt];

        last_word = p_reg->CFIFO;
        for (cnt = 0u; cnt < nbr_byte; cnt++) {
            p_buf8[cnt] = (last_word >> (8u * cnt)) & 0x000000FFu;
        }
    }

    DEF_BIT_SET(p_reg->CFIFOCTR, RENESAS_USBHS_FIFOCTR_BCLR);   /* Clr FIFO buffer.                                     */

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                    USBD_RenesasUSBHS_CFIFO_Wr()
*
* Description : Write data to CFIFO.
*
* Arguments   : p_reg           Pointer to registers structure.
*
*               p_pipe_info     Pointer to pipe information table.
*
*               ep_log_nbr      Endpoint logical number.
*
*               p_buf           Pointer to buffer.
*
*               buf_len         Buffer length in octets.
*
* Return(s)   : DEF_OK,   if NO error(s),
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) This function requires p_buf to be aligned on a 32 bit boundary.
*
*               (2) This function must be called within a critical section.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_RenesasUSBHS_CFIFO_Wr (USBD_RENESAS_USBHS_REG  *p_reg,
                                                 USBD_DRV_PIPE_INFO      *p_pipe_info,
                                                 CPU_INT08U               ep_log_nbr,
                                                 CPU_INT08U              *p_buf,
                                                 CPU_INT32U               buf_len)
{
    CPU_BOOLEAN   valid;
    CPU_INT08U    cnt;
    CPU_INT32U    nbr_word;
    CPU_INT32U    nbr_byte;
    CPU_INT32U   *p_buf32;
    CPU_INT08U   *p_buf8;


    valid = USBD_RenesasUSBHS_CurPipeSet(&p_reg->CFIFOSEL,
                                          ep_log_nbr,
                                          DEF_YES);
    if (valid != DEF_OK) {
        return (DEF_FAIL);
    }

    nbr_word = buf_len / 4u;
    nbr_byte = buf_len % 4u;

    p_buf32 = (CPU_INT32U *)p_buf;
    for (cnt = 0u; cnt < nbr_word; cnt++) {
        p_reg->CFIFO = p_buf32[cnt];
    }

    if (nbr_byte > 0u) {
        p_buf8 = (CPU_INT08U *)&p_buf32[cnt];

        DEF_BIT_CLR(p_reg->CFIFOSEL,                            /* Set FIFO access width to 8 bit.                      */
                    RENESAS_USBHS_CFIFOSEL_MBW_MASK);

        DEF_BIT_FIELD_WR(p_reg->CFIFOSEL,
                         RENESAS_USBHS_CFIFOSEL_MBW_8,
                         RENESAS_USBHS_CFIFOSEL_MBW_MASK);

        for (cnt = 0u; cnt < nbr_byte; cnt++) {
            p_reg->CFIFO = p_buf8[cnt];
        }
    }

    if (( buf_len == 0u) ||                                     /* Indicate wr is cmpl if not a multiple of buf len.    */
        (((buf_len % p_pipe_info->MaxBufLen)                          != 0u) &&
        ( DEF_BIT_IS_CLR(p_reg->CFIFOCTR, RENESAS_USBHS_FIFOCTR_BVAL) == DEF_YES))) {
        p_reg->CFIFOCTR = RENESAS_USBHS_FIFOCTR_BVAL;
    }

    valid = USBD_RenesasUSBHS_EP_PID_Set(p_reg,
                                         ep_log_nbr,
                                         RENESAS_USBHS_PIPExCTR_PID_BUF);

    return (valid);
}


/*
*********************************************************************************************************
*                                    USBD_RenesasUSBHS_DFIFO_Rd()
*
* Description : Read data from DFIFO.
*
* Arguments   : p_drv           Pointer to device driver structure.
*
*               p_reg           Pointer to registers structure.
*
*               p_pipe_info     Pointer to pipe information structure.
*
*               p_dfifo_info    Pointer to DFIFO information structure.
*
*               dev_nbr         Device number.
*
*               dfifo_nbr       DFIFO channel number.
*
*               ep_log_nbr      Endpoint logical number.
*
* Return(s)   : DEF_OK,   if NO error(s),
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) This function requires p_buf to be aligned on a 32 bit boundary.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_RenesasUSBHS_DFIFO_Rd (USBD_DRV                *p_drv,
                                                 USBD_RENESAS_USBHS_REG  *p_reg,
                                                 USBD_DRV_PIPE_INFO      *p_pipe_info,
                                                 USBD_DRV_DFIFO_INFO     *p_dfifo_info,
                                                 CPU_INT08U               dev_nbr,
                                                 CPU_INT08U               dfifo_nbr,
                                                 CPU_INT08U               ep_log_nbr)
{
    CPU_BOOLEAN   valid;
    CPU_BOOLEAN   start_dma;
    CPU_BOOLEAN   xfer_end_flag;
    CPU_INT08U   *p_buf;
    CPU_INT16U    xfer_len;
    CPU_INT16U    rx_len;
    CPU_SR_ALLOC();


    valid         =  DEF_OK;
    xfer_end_flag =  DEF_NO;
    rx_len        =  p_reg->DxFIFOn[dfifo_nbr].CTR & RENESAS_USBHS_FIFOCTR_DTLN_MASK;
    p_buf         = &p_dfifo_info->BufPtr[p_dfifo_info->DMA_XferByteCnt];

    if (rx_len <= (p_dfifo_info->BufLen - p_dfifo_info->USB_XferByteCnt)) {
        p_dfifo_info->RemByteCnt = rx_len % sizeof(CPU_INT32U);
        xfer_len                 = rx_len - p_dfifo_info->RemByteCnt;
    } else {
        xfer_len                 = (p_dfifo_info->BufLen - p_dfifo_info->USB_XferByteCnt);
        p_dfifo_info->RemByteCnt =  xfer_len % sizeof(CPU_INT32U);
        xfer_len                 =  xfer_len - p_dfifo_info->RemByteCnt;
        p_dfifo_info->Err        =  USBD_ERR_RX;
    }

    p_dfifo_info->USB_XferByteCnt += (xfer_len + p_dfifo_info->RemByteCnt);

                                                                /* Compute end of xfer flag.                            */
    if ((p_dfifo_info->USB_XferByteCnt       >= p_dfifo_info->BufLen) ||
        (xfer_len                            == 0u)                   ||
        (xfer_len % p_pipe_info->MaxPktSize) != 0u) {
        xfer_end_flag = DEF_YES;
    }

    if (xfer_len > 0u) {
        CPU_CRITICAL_ENTER();                                   /* Chk if no dma xfer q-ed.                             */
        if (p_dfifo_info->DMA_XferNewestIx == p_dfifo_info->DMA_XferOldestIx) {
            start_dma = DEF_YES;
        } else {
            start_dma = DEF_NO;
        }

        p_dfifo_info->DMA_XferTbl[p_dfifo_info->DMA_XferNewestIx] = xfer_len;

        p_dfifo_info->DMA_XferNewestIx++;
        if (p_dfifo_info->DMA_XferNewestIx >= RENESAS_USBHS_RX_Q_SIZE) {
            p_dfifo_info->DMA_XferNewestIx = 0u;
        }

        p_dfifo_info->XferEndFlag = xfer_end_flag;
        CPU_CRITICAL_EXIT();

        if (start_dma == DEF_YES) {
            valid = USBD_BSP_DMA_CopyStart(dev_nbr,
                                           dfifo_nbr,
                                           DEF_YES,
                                           p_buf,
                                          &p_reg->DxFIFO[dfifo_nbr],
                                           xfer_len);
        }
    } else if (p_dfifo_info->DMA_XferNewestIx == p_dfifo_info->DMA_XferOldestIx) {
        DEF_BIT_CLR(p_reg->BRDYENB, DEF_BIT(ep_log_nbr));

        USBD_RenesasUSBHS_DFIFO_RemBytesRd(p_reg,
                                           p_buf,
                                           p_dfifo_info->RemByteCnt,
                                           dfifo_nbr);

        USBD_EP_RxCmpl(p_drv, ep_log_nbr);
    }

    return (valid);
}


/*
*********************************************************************************************************
*                                    USBD_RenesasUSBHS_DFIFO_Wr()
*
* Description : Write data to CFIFO.
*
* Arguments   : p_reg           Pointer to registers structure.
*
*               p_pipe_info     Pointer to pipe information structure.
*
*               p_dfifo_info    Pointer to DFIFO information structure
*
*               dev_nbr         Device number.
*
*               dfifo_nbr       DFIFO channel number.
*
*               ep_log_nbr      Endpoint logical number.
*
* Return(s)   : DEF_OK,   if NO error(s),
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) This function requires p_buf to be aligned on a 32 bit boundary.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_RenesasUSBHS_DFIFO_Wr (USBD_RENESAS_USBHS_REG  *p_reg,
                                                 USBD_DRV_PIPE_INFO      *p_pipe_info,
                                                 USBD_DRV_DFIFO_INFO     *p_dfifo_info,
                                                 CPU_INT08U               dev_nbr,
                                                 CPU_INT08U               dfifo_nbr,
                                                 CPU_INT08U               ep_log_nbr)
{
    CPU_BOOLEAN   valid;
    CPU_INT08U   *p_buf;
    CPU_INT32U    dma_xfer_len;


                                                                /* DMA engine must do 32 bit accesses.                  */
    dma_xfer_len              =  p_dfifo_info->BufLen - p_dfifo_info->DMA_XferByteCnt;
    dma_xfer_len              =  DEF_MIN(dma_xfer_len, p_pipe_info->MaxBufLen);
    p_dfifo_info->RemByteCnt  =  dma_xfer_len % sizeof(CPU_INT32U);
    dma_xfer_len             -=  p_dfifo_info->RemByteCnt;
    p_buf                     = &p_dfifo_info->BufPtr[p_dfifo_info->DMA_XferByteCnt];

    if (dma_xfer_len > 0u) {

        p_dfifo_info->DMA_XferByteCnt += dma_xfer_len;
        p_dfifo_info->CurDmaTxLen      = dma_xfer_len;

        if ((p_dfifo_info->DMA_XferByteCnt + p_dfifo_info->RemByteCnt) >= p_dfifo_info->BufLen) {
            DEF_BIT_CLR(p_reg->BRDYENB, DEF_BIT(ep_log_nbr));   /* No notification of avail buf if last xfer.           */
        }

        CPU_MB();                                               /* Ensure USB operations executed before DMA.           */

        valid = USBD_BSP_DMA_CopyStart(dev_nbr,
                                       dfifo_nbr,
                                       DEF_NO,
                                       p_buf,
                                      &p_reg->DxFIFO[dfifo_nbr],
                                       dma_xfer_len);
    } else {
                                                                /* Process any remaining bytes in data xfer.            */
        USBD_RenesasUSBHS_DFIFO_RemBytesWr(              p_reg,
                                           (CPU_INT08U *)p_buf,
                                                         p_dfifo_info->RemByteCnt,
                                                         dfifo_nbr);

        p_dfifo_info->DMA_XferByteCnt+= p_dfifo_info->RemByteCnt;
        p_dfifo_info->CopyDataCnt    += p_dfifo_info->RemByteCnt;

        DEF_BIT_CLR(p_reg->BRDYENB, DEF_BIT(ep_log_nbr));       /* No notification of avail buf if last xfer.           */

        DEF_BIT_SET(p_reg->DxFIFOn[dfifo_nbr].CTR,              /* Inidicates wr to FIFO is complete.                   */
                    RENESAS_USBHS_FIFOCTR_BVAL);

        valid = USBD_RenesasUSBHS_EP_PID_Set(p_reg,             /* Set pipe as ready to send data.                      */
                                             ep_log_nbr,
                                             RENESAS_USBHS_PIPExCTR_PID_BUF);
    }

    return (valid);
}


/*
*********************************************************************************************************
*                              USBD_RenesasUSBHS_DFIFO_RemBytesWr()
*
* Description : Write remaining bytes on DFIFO channel.
*
* Arguments   : p_reg               Pointer to registers structure.
*
*               p_buf               Pointer to buffer.
*
*               rem_bytes_cnt       Number of bytes remaining.
*
*               dfifo_ch_nbr        DFIFO channel number.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  USBD_RenesasUSBHS_DFIFO_RemBytesWr (USBD_RENESAS_USBHS_REG  *p_reg,
                                                  CPU_INT08U              *p_buf,
                                                  CPU_INT08U               rem_bytes_cnt,
                                                  CPU_INT08U               dfifo_ch_nbr)
{
    CPU_INT08U  byte_cnt;


    if (rem_bytes_cnt > 0u) {
        DEF_BIT_CLR(p_reg->DxFIFOn[dfifo_ch_nbr].SEL,           /* Set fifo access width to 8 bit.                      */
                    RENESAS_USBHS_CFIFOSEL_MBW_MASK);

        DEF_BIT_FIELD_WR(p_reg->DxFIFOn[dfifo_ch_nbr].SEL,
                         RENESAS_USBHS_CFIFOSEL_MBW_8,
                         RENESAS_USBHS_CFIFOSEL_MBW_MASK);

                                                                /* Wr to FIFO.                                          */
        for (byte_cnt = 0u; byte_cnt < rem_bytes_cnt; byte_cnt++) {
            p_reg->DxFIFO[dfifo_ch_nbr] = p_buf[byte_cnt];
        }
    }
}


/*
*********************************************************************************************************
*                              USBD_RenesasUSBHS_DFIFO_RemBytesRd()
*
* Description : Reads remaining bytes on DFIFO channel.
*
* Arguments   : p_reg               Pointer to registers structure.
*
*               p_buf               Pointer to buffer.
*
*               rem_bytes_cnt       Number of bytes remaining.
*
*               dfifo_ch_nbr        DFIFO channel number.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  USBD_RenesasUSBHS_DFIFO_RemBytesRd (USBD_RENESAS_USBHS_REG  *p_reg,
                                                  CPU_INT08U              *p_buf,
                                                  CPU_INT08U               rem_bytes_cnt,
                                                  CPU_INT08U               dfifo_ch_nbr)
{
    CPU_INT08U  byte_cnt;
    CPU_INT32U  word;


    if (rem_bytes_cnt > 0u) {
        word = p_reg->DxFIFO[dfifo_ch_nbr];

                                                                /* Rd from temp buf.                                    */
        for (byte_cnt = 0u; byte_cnt < rem_bytes_cnt; byte_cnt++) {
            p_buf[byte_cnt] = (CPU_INT08U)((word >> (DEF_INT_08_NBR_BITS * byte_cnt)) & DEF_INT_08_MASK);
        }
    }
}


/*
*********************************************************************************************************
*                                  USBD_RenesasUSBHS_FIFO_Acquire()
*
* Description : Acquires an available DFIFO channel to use for data transfer.
*
* Arguments   : p_drv_data      Pointer to driver data.
*
*               ep_log_nbr      Endpoint logical number.
*
* Return(s)   : DFIFO number to use, if DMA enabled and one DFIFO channel is available and endpoint
*                                    type is not control,
*
*               RENESAS_USBHS_CFIFO, otherwise.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  CPU_INT08U  USBD_RenesasUSBHS_FIFO_Acquire (USBD_DRV_DATA  *p_drv_data,
                                                    CPU_INT08U      ep_log_nbr)
{
    CPU_INT08U  fifo_ch_nbr;
    CPU_SR_ALLOC();


                                                                /* ------- DETERMINE WHICH FIFO CHANNEL TO USE -------- */
    if ((ep_log_nbr         != 0u) &&
        (p_drv_data->DMA_En == DEF_ENABLED)) {

        CPU_CRITICAL_ENTER();

        fifo_ch_nbr = CPU_CntTrailZeros08(p_drv_data->AvailDFIFO);
        if (fifo_ch_nbr < USBD_DrvDFIFO_Qty[p_drv_data->Ctrlr]) {
            DEF_BIT_CLR(p_drv_data->AvailDFIFO,                 /* Mark DFIFO as unavailable.                           */
                        DEF_BIT(fifo_ch_nbr));
        } else {
            fifo_ch_nbr = RENESAS_USBHS_CFIFO;                  /* No DFIFO available. Use CFIFO in this case.          */
        }

        CPU_CRITICAL_EXIT();
    } else {
        fifo_ch_nbr = RENESAS_USBHS_CFIFO;
    }

    return (fifo_ch_nbr);
}


/*
*********************************************************************************************************
*                                   USBD_RenesasUSBHS_EP_PID_Set()
*
* Description : Sets the current PID (state) of given pipe.
*
* Arguments   : p_reg       Pointer to registers structure.
*
*               ep_log_nbr  Endpoint logical number.
*
*               resp_pid    Response PID to set the endpoint to.
*
*                       RENESAS_USBHS_PIPExCTR_PID_NAK
*                       RENESAS_USBHS_PIPExCTR_PID_BUF
*                       RENESAS_USBHS_PIPExCTR_STALL1
*                       RENESAS_USBHS_PIPExCTR_STALL2
*
* Return(s)   : DEF_OK,   if NO error(s),
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_RenesasUSBHS_EP_PID_Set (USBD_RENESAS_USBHS_REG  *p_reg,
                                                   CPU_INT08U               ep_log_nbr,
                                                   CPU_INT08U               resp_pid)
{
    CPU_BOOLEAN   valid;
    CPU_INT08U    prev_pid;
    CPU_INT08U    cnt;
    CPU_INT16U    pipe_ctr_val;
    CPU_REG16    *p_pipe_ctr_reg;


    if (ep_log_nbr != 0u) {
        p_pipe_ctr_reg = &p_reg->PIPExCTR[ep_log_nbr - 1u];
    } else {
        p_pipe_ctr_reg = &p_reg->DCPCTR;
    }

    valid    =  DEF_OK;
    prev_pid = (*p_pipe_ctr_reg & RENESAS_USBHS_PIPExCTR_PID_MASK);

    if (prev_pid == resp_pid) {                                 /* If PID already correct, return immediately.          */
        return (DEF_OK);
    }

    switch (resp_pid) {
        case RENESAS_USBHS_PIPExCTR_PID_BUF:
             if (prev_pid == RENESAS_USBHS_PIPExCTR_PID_STALL2) {
                 pipe_ctr_val = *p_pipe_ctr_reg;
                 DEF_BIT_CLR(pipe_ctr_val, RENESAS_USBHS_PIPExCTR_PID_MASK);
                 DEF_BIT_SET(pipe_ctr_val, RENESAS_USBHS_PIPExCTR_PID_STALL1);
                *p_pipe_ctr_reg = pipe_ctr_val;
             }

             pipe_ctr_val = *p_pipe_ctr_reg;
             DEF_BIT_CLR(pipe_ctr_val, RENESAS_USBHS_PIPExCTR_PID_MASK);
             DEF_BIT_SET(pipe_ctr_val, RENESAS_USBHS_PIPExCTR_PID_NAK);
            *p_pipe_ctr_reg = pipe_ctr_val;

             DEF_BIT_CLR(pipe_ctr_val, RENESAS_USBHS_PIPExCTR_PID_MASK);
             DEF_BIT_SET(pipe_ctr_val, RENESAS_USBHS_PIPExCTR_PID_BUF);
            *p_pipe_ctr_reg = pipe_ctr_val;
             break;


        case RENESAS_USBHS_PIPExCTR_PID_NAK:
             if (prev_pid == RENESAS_USBHS_PIPExCTR_PID_STALL2) {
                 pipe_ctr_val = *p_pipe_ctr_reg;
                 DEF_BIT_CLR(pipe_ctr_val, RENESAS_USBHS_PIPExCTR_PID_MASK);
                 DEF_BIT_SET(pipe_ctr_val, RENESAS_USBHS_PIPExCTR_PID_STALL1);
                *p_pipe_ctr_reg = pipe_ctr_val;
             }

             pipe_ctr_val = *p_pipe_ctr_reg;
             DEF_BIT_CLR(pipe_ctr_val, RENESAS_USBHS_PIPExCTR_PID_MASK);
             DEF_BIT_SET(pipe_ctr_val, RENESAS_USBHS_PIPExCTR_PID_NAK);
            *p_pipe_ctr_reg = pipe_ctr_val;

             if (prev_pid == RENESAS_USBHS_PIPExCTR_PID_BUF) {
                 if (DEF_BIT_IS_CLR(*p_pipe_ctr_reg, RENESAS_USBHS_PIPExCTR_PBUSY) == DEF_YES) {
                     break;
                 }

                 cnt = 0u;
                 do {
                     cnt++;
                     USBD_BSP_DlyUs(1u);
                 } while ((DEF_BIT_IS_SET(*p_pipe_ctr_reg, RENESAS_USBHS_PIPExCTR_PBUSY) == DEF_YES) &&
                          (cnt                                                           <  200u));

                 if (cnt >= 200u) {
                     valid = DEF_FAIL;
                 }
             }
             break;


        case RENESAS_USBHS_PIPExCTR_PID_STALL1:
        case RENESAS_USBHS_PIPExCTR_PID_STALL2:
             pipe_ctr_val = *p_pipe_ctr_reg;
             DEF_BIT_CLR(pipe_ctr_val, RENESAS_USBHS_PIPExCTR_PID_MASK);

             if (prev_pid == RENESAS_USBHS_PIPExCTR_PID_BUF) {
                 DEF_BIT_SET(pipe_ctr_val, RENESAS_USBHS_PIPExCTR_PID_STALL2);
             } else {
                 DEF_BIT_SET(pipe_ctr_val, RENESAS_USBHS_PIPExCTR_PID_STALL1);
             }

            *p_pipe_ctr_reg = pipe_ctr_val;
             break;


        default:
             break;
    }

    return (valid);
}


/*
*********************************************************************************************************
*                                   USBD_RenesasUSBHS_CurPipeSet()
*
* Description : Bind an endpoint (pipe) to a FIFO channel.
*
* Arguments   : p_fifosel_reg       Pointer to FIFOSEL register.
*
*               ep_log_nbr          Endpoint logical number.
*
*               dir_in              Pipe direction (only used with control endpoint).
*
*                               DEF_YES     Endpoint direction is IN.
*                               DEF_NO      Endpoint direction is OUT.
*
* Return(s)   : DEF_OK,   if NO error(s),
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) Depending on the external bus speed of CPU, you may need to wait for 450ns here.
                    For details, look at the data sheet.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_RenesasUSBHS_CurPipeSet (CPU_REG16    *p_fifosel_reg,
                                                   CPU_INT08U    ep_log_nbr,
                                                   CPU_BOOLEAN   dir_in)
{
    CPU_INT08U  cnt;
    CPU_INT16U  fifosel_val;
    CPU_INT16U  fifosel_val_masked;


    fifosel_val = *p_fifosel_reg;

    DEF_BIT_CLR(fifosel_val, RENESAS_USBHS_CFIFOSEL_CURPIPE_MASK);

                                                                /* Set ISEL bit for ctrl pipe.                          */
    if ((ep_log_nbr == 0u) &&
        (dir_in     == DEF_YES)) {
        DEF_BIT_SET(fifosel_val, RENESAS_USBHS_CFIFOSEL_ISEL);
    } else {
        DEF_BIT_CLR(fifosel_val, RENESAS_USBHS_CFIFOSEL_ISEL);
    }
   *p_fifosel_reg = fifosel_val;

    fifosel_val_masked = fifosel_val & (RENESAS_USBHS_CFIFOSEL_ISEL | RENESAS_USBHS_CFIFOSEL_CURPIPE_MASK);

                                                                /* Ensure FIFOSEL reg is cleared.                       */
    if ((*p_fifosel_reg & (RENESAS_USBHS_CFIFOSEL_ISEL | RENESAS_USBHS_CFIFOSEL_CURPIPE_MASK)) != fifosel_val_masked) {

        cnt = 0u;
        do {
            cnt++;
            USBD_BSP_DlyUs(1u);
        } while (((*p_fifosel_reg & (RENESAS_USBHS_CFIFOSEL_ISEL | RENESAS_USBHS_CFIFOSEL_CURPIPE_MASK)) != fifosel_val_masked) &&
                 ( cnt                                                                                    < 4u));
        if (cnt >= 4u) {
            return (DEF_FAIL);
        }
    }

                                                                /* Set FIFO to 32 bit width access.                     */
    DEF_BIT_CLR(fifosel_val, RENESAS_USBHS_CFIFOSEL_MBW_MASK);
    DEF_BIT_SET(fifosel_val, RENESAS_USBHS_CFIFOSEL_MBW_32);

    fifosel_val        |= (ep_log_nbr & RENESAS_USBHS_CFIFOSEL_CURPIPE_MASK);
   *p_fifosel_reg       =  fifosel_val;
    fifosel_val_masked  =  fifosel_val & (RENESAS_USBHS_CFIFOSEL_ISEL | RENESAS_USBHS_CFIFOSEL_CURPIPE_MASK);

                                                                /* Ensure FIFOSEL reg is correctly assigned.            */
    if ((*p_fifosel_reg & (RENESAS_USBHS_CFIFOSEL_ISEL | RENESAS_USBHS_CFIFOSEL_CURPIPE_MASK)) != fifosel_val_masked) {

        cnt = 0u;
        do {
            cnt++;
            USBD_BSP_DlyUs(1u);
        } while (((*p_fifosel_reg & (RENESAS_USBHS_CFIFOSEL_ISEL | RENESAS_USBHS_CFIFOSEL_CURPIPE_MASK)) != fifosel_val_masked) &&
                 ( cnt                                                                                    < 4u));
        if (cnt >= 4u) {
            return (DEF_FAIL);
        }
    }

    USBD_BSP_DlyUs(1u);                                         /* See note (1).                                        */

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                   USBD_RenesasUSBHS_ResetEvent()
*
* Description : Resets driver's internal data.
*
* Argument(s) : p_drv_data      Pointer to device driver data structure.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  USBD_RenesasUSBHS_ResetEvent (USBD_DRV_DATA  *p_drv_data)
{
    p_drv_data->NoZLP             = DEF_NO;
    p_drv_data->CtrlRdStatusStart = DEF_NO;
    p_drv_data->CtrlWrStatusStart = DEF_NO;

    p_drv_data->AvailDFIFO = RENESAS_USBHS_DFIFO_MASK;          /* Mark all DFIFO channels as available.                */

    USBD_DrvLib_SetupPktQClr(&p_drv_data->SetupPktQ);
}


/*
*********************************************************************************************************
*                                   USBD_RenesasUSBHS_BRDY_Event()
*
* Description : Handle buffer ready ISR for given endpoint logical number.
*
* Argument(s) : p_drv           Pointer to device driver structure.
*
*               ep_log_nbr      Endpoint logical number.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  USBD_RenesasUSBHS_BRDY_Event (USBD_DRV    *p_drv,
                                            CPU_INT08U   ep_log_nbr)
{
    USBD_DRV_DATA           *p_drv_data;
    USBD_DRV_PIPE_INFO      *p_pipe_info;
    USBD_DRV_DFIFO_INFO     *p_dfifo_info;
    USBD_RENESAS_USBHS_REG  *p_reg;


    p_reg         = (USBD_RENESAS_USBHS_REG *)p_drv->CfgPtr->BaseAddr;
    p_drv_data    = (USBD_DRV_DATA          *)p_drv->DataPtr;
    p_pipe_info   = &p_drv_data->PipeInfoTbl[ep_log_nbr];
    p_dfifo_info  = &p_drv_data->DFIFO_InfoTbl[p_pipe_info->FIFO_IxUsed];

    if (p_pipe_info->FIFO_IxUsed != RENESAS_USBHS_CFIFO) {
        if (p_dfifo_info->XferIsRd == DEF_YES) {
            (void)USBD_RenesasUSBHS_DFIFO_Rd(p_drv,             /* Trigger DMA rd.                                      */
                                             p_reg,
                                             p_pipe_info,
                                            &p_drv_data->DFIFO_InfoTbl[p_pipe_info->FIFO_IxUsed],
                                             p_drv->DevNbr,
                                             p_pipe_info->FIFO_IxUsed,
                                             ep_log_nbr);
        } else {
            (void)USBD_RenesasUSBHS_DFIFO_Wr(p_reg,             /* Trigger DMA wr.                                      */
                                             p_pipe_info,
                                            &p_drv_data->DFIFO_InfoTbl[p_pipe_info->FIFO_IxUsed],
                                             p_drv->DevNbr,
                                             p_pipe_info->FIFO_IxUsed,
                                             ep_log_nbr);
        }
    } else {

        DEF_BIT_CLR(p_reg->BRDYENB, DEF_BIT(ep_log_nbr));       /* Disable int.                                         */

        (void)USBD_RenesasUSBHS_EP_PID_Set(p_reg,
                                           ep_log_nbr,
                                           RENESAS_USBHS_PIPExCTR_PID_NAK);

        USBD_EP_RxCmpl(p_drv, ep_log_nbr);
    }
}


/*
*********************************************************************************************************
*                                   USBD_RenesasUSBHS_BEMP_Event()
*
* Description : Handle buffer empty ISR for given endpoint logical number.
*
* Argument(s) : p_drv           Pointer to device driver structure.
*
*               ep_log_nbr      Endpoint logical number.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  USBD_RenesasUSBHS_BEMP_Event (USBD_DRV    *p_drv,
                                            CPU_INT08U   ep_log_nbr)
{
    USBD_DRV_DATA           *p_drv_data;
    USBD_DRV_DFIFO_INFO     *p_dfifo_info;
    USBD_DRV_PIPE_INFO      *p_pipe_info;
    USBD_RENESAS_USBHS_REG  *p_reg;
    CPU_SR_ALLOC();


    p_reg        = (USBD_RENESAS_USBHS_REG *)p_drv->CfgPtr->BaseAddr;
    p_drv_data   = (USBD_DRV_DATA          *)p_drv->DataPtr;
    p_pipe_info  = &p_drv_data->PipeInfoTbl[ep_log_nbr];

    if (p_pipe_info->FIFO_IxUsed != RENESAS_USBHS_CFIFO) {
        p_dfifo_info = &p_drv_data->DFIFO_InfoTbl[p_pipe_info->FIFO_IxUsed];

        CPU_CRITICAL_ENTER();
        p_dfifo_info->USB_XferByteCnt+= p_dfifo_info->CopyDataCnt;
        p_dfifo_info->CopyDataCnt     = 0u;
        CPU_CRITICAL_EXIT();

        if (p_dfifo_info->USB_XferByteCnt >= p_dfifo_info->BufLen) {

            (void)USBD_RenesasUSBHS_EP_PID_Set(p_reg,
                                               ep_log_nbr,
                                               RENESAS_USBHS_PIPExCTR_PID_NAK);

            CPU_CRITICAL_ENTER();
            DEF_BIT_SET(p_drv_data->AvailDFIFO,                 /* Mark DFIFO as available.                             */
                        DEF_BIT(p_pipe_info->FIFO_IxUsed));
            CPU_CRITICAL_EXIT();

            DEF_BIT_CLR(p_reg->BRDYENB, DEF_BIT(ep_log_nbr));   /* Disable int.                                         */
            DEF_BIT_CLR(p_reg->BEMPENB, DEF_BIT(ep_log_nbr));

            USBD_EP_TxCmpl(p_drv, ep_log_nbr);
        }
    } else {
        (void)USBD_RenesasUSBHS_EP_PID_Set(p_reg,
                                           ep_log_nbr,
                                           RENESAS_USBHS_PIPExCTR_PID_NAK);

        if (p_pipe_info->UseDblBuf == DEF_YES) {
            CPU_CRITICAL_ENTER();                               /* Re-enable double buffering.                          */
            p_reg->PIPESEL = (ep_log_nbr & RENESAS_USBHS_PIPESEL_PIPESEL_MASK);
            DEF_BIT_SET(p_reg->PIPECFG, RENESAS_USBHS_PIPCFG_DBLB);
            p_reg->PIPESEL =  0u;
            CPU_CRITICAL_EXIT();
        }

        DEF_BIT_CLR(p_reg->BEMPENB, DEF_BIT(ep_log_nbr));       /* Disable int.                                         */

        USBD_EP_TxCmpl(p_drv, ep_log_nbr);
    }
}


/*
*********************************************************************************************************
*                                   USBD_RenesasUSBHS_DFIFO_Event()
*
* Description : Handle DFIFO DMA ISR for given DFIFO channel.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               dfifo_nbr   DFIFO channel number.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  USBD_RenesasUSBHS_DFIFO_Event (USBD_DRV    *p_drv,
                                             CPU_INT08U   dfifo_nbr)
{
    CPU_INT08U               ep_log_nbr;
    CPU_INT32U               xfer_len_next;
    USBD_DRV_DATA           *p_drv_data;
    USBD_DRV_DFIFO_INFO     *p_dfifo_info;
    USBD_DRV_PIPE_INFO      *p_pipe_info;
    USBD_RENESAS_USBHS_REG  *p_reg;
    CPU_SR_ALLOC();


    p_reg        = (USBD_RENESAS_USBHS_REG *)p_drv->CfgPtr->BaseAddr;
    p_drv_data   = (USBD_DRV_DATA          *)p_drv->DataPtr;
    p_dfifo_info = &p_drv_data->DFIFO_InfoTbl[dfifo_nbr];
    ep_log_nbr   =  p_dfifo_info->EP_LogNbr;
    p_pipe_info  = &p_drv_data->PipeInfoTbl[ep_log_nbr];


    if (p_dfifo_info->XferIsRd == DEF_YES) {
        xfer_len_next = 0u;
        CPU_CRITICAL_ENTER();                                   /* Increment DMA xfer cnt with oldest rx xfer.          */
        p_dfifo_info->DMA_XferByteCnt += p_dfifo_info->DMA_XferTbl[p_dfifo_info->DMA_XferOldestIx];
        p_dfifo_info->DMA_XferOldestIx++;
        if (p_dfifo_info->DMA_XferOldestIx >= RENESAS_USBHS_RX_Q_SIZE) {
            p_dfifo_info->DMA_XferOldestIx = 0u;
        }

                                                                /* Determine if DMA xfer start or if xfer is cmpl.      */
        if (p_dfifo_info->DMA_XferOldestIx != p_dfifo_info->DMA_XferNewestIx) {
            xfer_len_next = p_dfifo_info->DMA_XferTbl[p_dfifo_info->DMA_XferOldestIx];
            CPU_CRITICAL_EXIT();
        } else if (p_dfifo_info->XferEndFlag == DEF_YES) {
            CPU_CRITICAL_EXIT();

            DEF_BIT_CLR(p_reg->BRDYENB, DEF_BIT(ep_log_nbr));

            USBD_RenesasUSBHS_DFIFO_RemBytesRd(         p_reg,
                                               (void *)&p_dfifo_info->BufPtr[p_dfifo_info->DMA_XferByteCnt],
                                                        p_dfifo_info->RemByteCnt,
                                                        dfifo_nbr);

            USBD_EP_RxCmpl(p_drv, ep_log_nbr);
        } else {
            CPU_CRITICAL_EXIT();
        }

        if (xfer_len_next > 0u) {
            (void)USBD_BSP_DMA_CopyStart(         p_drv->DevNbr,
                                                  dfifo_nbr,
                                                  DEF_YES,
                                         (void *)&p_dfifo_info->BufPtr[p_dfifo_info->DMA_XferByteCnt],
                                                 &p_reg->DxFIFO[dfifo_nbr],
                                                  xfer_len_next);
        }
    } else {
        USBD_RenesasUSBHS_DFIFO_RemBytesWr(p_reg,               /* Process any remaining bytes in data xfer.            */
                                          &p_dfifo_info->BufPtr[p_dfifo_info->DMA_XferByteCnt],
                                           p_dfifo_info->RemByteCnt,
                                           dfifo_nbr);

        p_dfifo_info->CopyDataCnt     += p_dfifo_info->RemByteCnt;
        p_dfifo_info->DMA_XferByteCnt += p_dfifo_info->RemByteCnt;
        p_dfifo_info->CopyDataCnt     += p_dfifo_info->CurDmaTxLen;

                                                                /* End of wr to FIFO if buf len not multiple of buf len.*/
        if (((p_dfifo_info->DMA_XferByteCnt % p_pipe_info->MaxBufLen)                   != 0u) &&
            ( DEF_BIT_IS_CLR(p_reg->DxFIFOn[dfifo_nbr].CTR, RENESAS_USBHS_FIFOCTR_BVAL) == DEF_YES)) {
            DEF_BIT_SET(p_reg->DxFIFOn[dfifo_nbr].CTR, RENESAS_USBHS_FIFOCTR_BVAL);
        }

        (void)USBD_RenesasUSBHS_EP_PID_Set(p_reg,               /* Enable data transmission on pipe.                    */
                                           ep_log_nbr,
                                           RENESAS_USBHS_PIPExCTR_PID_BUF);
    }
}
