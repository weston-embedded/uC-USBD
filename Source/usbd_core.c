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
*                                     USB DEVICE CORE OPERATIONS
*
* Filename : usbd_core.c
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#include  "usbd_core.h"
#include  "usbd_internal.h"
#include  <lib_str.h>
#include  <lib_mem.h>
#include  <lib_math.h>
#include  <cpu_core.h>


/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                    OBJECTS TOTAL NUMBER DEFINES
*********************************************************************************************************
*/

#define  USBD_DEV_NBR_TOT                 (DEF_INT_08U_MAX_VAL - 1u)
#define  USBD_CFG_NBR_TOT                 (DEF_INT_08U_MAX_VAL - 1u)
#define  USBD_IF_NBR_TOT                  (DEF_INT_08U_MAX_VAL - 1u)
#define  USBD_IF_ALT_NBR_TOT              (DEF_INT_08U_MAX_VAL - 1u)
#define  USBD_IF_GRP_NBR_TOT              (DEF_INT_08U_MAX_VAL - 1u)
#define  USBD_EP_NBR_TOT                  (DEF_INT_08U_MAX_VAL - 1u)


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*
* Note(s) : (1) The descriptor buffer is used to send the device, configuration and string descriptors.
*
*               (a) The size of the descriptor buffer is set to 64 which is the maximum packet size
*                   allowed by the USB specification for FS and HS devices.
*
*           (2) USB spec 2.0 section 9.6.3, table 9-10 specify the bitmap for the configuration
*               attributes.
*
*                   D7    Reserved (set to one)
*                   D6    Self-powered
*                   D5    Remote Wakeup
*                   D4..0 Reserved (reset to zero)
*********************************************************************************************************
*/

#define  USBD_CFG_DESC_BUF_LEN                            64u   /* See Note #1a.                                        */
#define  USBD_EP_CTRL_ALLOC                       (DEF_BIT_00 | DEF_BIT_01)

#define  USBD_CFG_DESC_SELF_POWERED                DEF_BIT_06   /* See Note #2.                                         */
#define  USBD_CFG_DESC_REMOTE_WAKEUP               DEF_BIT_05
#define  USBD_CFG_DESC_RSVD_SET                    DEF_BIT_07

                                                                /* -------------- MICROSOFT DESC DEFINES -------------- */
#define  USBD_MS_OS_DESC_COMPAT_ID_HDR_VER_1_0        0x0010u
#define  USBD_MS_OS_DESC_EXT_PROPERTIES_HDR_VER_1_0   0x000Au
#define  USBD_MS_OS_DESC_VER_1_0                      0x0100u

#define  USBD_STR_MS_OS_LEN                               18u   /* Length of MS OS string.                              */
#define  USBD_STR_MS_OS_IX                              0xEEu   /* Index  of MS OS string.                              */

#define  USBD_MS_OS_DESC_COMPAT_ID_HDR_LEN                16u
#define  USBD_MS_OS_DESC_COMPAT_ID_SECTION_LEN            24u

#define  USBD_MS_OS_DESC_EXT_PROPERTIES_HDR_LEN           10u
#define  USBD_MS_OS_DESC_EXT_PROPERTIES_SECTION_HDR_LEN    8u

#define  USBD_MS_OS_FEATURE_COMPAT_ID                 0x0004u
#define  USBD_MS_OS_FEATURE_EXT_PROPERTIES            0x0005u


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*
* Note(s) : (1) For more information, see "Extended Compat ID OS Feature Descriptor Specification",
*               Appendix A, available at http://msdn.microsoft.com/en-us/windows/hardware/gg463179.aspx.
*********************************************************************************************************
*/
#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
static  const  CPU_CHAR  USBD_StrMS_Signature[] = "MSFT100";    /* Signature used in MS OS string desc.                 */

static  const  CPU_CHAR  USBD_MS_CompatID[][8u] = {
        {0u,  0u,  0u,  0u,  0u,  0u,  0u,  0u},
        {'R', 'N', 'D', 'I', 'S', 0u,  0u,  0u},
        {'P', 'T', 'P', 0u,  0u,  0u,  0u,  0u},
        {'M', 'T', 'P', 0u,  0u,  0u,  0u,  0u},
        {'X', 'U', 'S', 'B', '2', '0', 0u,  0u},
        {'B', 'L', 'U', 'T', 'U', 'T', 'H', 0u},
        {'W', 'I', 'N', 'U', 'S', 'B', 0u,  0u},
};

static  const  CPU_CHAR  USBD_MS_SubCompatID[][8u] = {
        {0u,  0u,  0u,  0u, 0u, 0u, 0u, 0u},
        {'1', '1', 0u,  0u, 0u, 0u, 0u, 0u},
        {'1', '2', 0u,  0u, 0u, 0u, 0u, 0u},
        {'E', 'D', 'R', 0u, 0u, 0u, 0u, 0u},
};
#endif

/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                        CORE EVENTS DATA TYPE
*********************************************************************************************************
*/

typedef  enum  usbd_event_code {
    USBD_EVENT_BUS_RESET = 0u,
    USBD_EVENT_BUS_SUSPEND,
    USBD_EVENT_BUS_RESUME,
    USBD_EVENT_BUS_CONN,
    USBD_EVENT_BUS_DISCONN,
    USBD_EVENT_BUS_HS,
    USBD_EVENT_EP,
    USBD_EVENT_SETUP
} USBD_EVENT_CODE;


/*
*********************************************************************************************************
*                                   ENDPOINT INFORMATION DATA TYPE
*********************************************************************************************************
*/

typedef  struct usbd_ep_info {
            CPU_INT08U        Addr;                             /* Endpoint address.                                    */
            CPU_INT08U        Attrib;                           /* Endpoint attributes.                                 */
            CPU_INT08U        Interval;                         /* Endpoint interval.                                   */
            CPU_INT16U        MaxPktSize;
            CPU_INT08U        SyncAddr;                         /* Audio Class Only: associated sync endpoint.          */
            CPU_INT08U        SyncRefresh;                      /* Audio Class Only: sync feedback rate.                */
#if (USBD_CFG_OPTIMIZE_SPD == DEF_DISABLED)
    struct  usbd_ep_info     *NextPtr;                          /* Pointer to next interface group structure.           */
#endif
} USBD_EP_INFO;


/*
*********************************************************************************************************
*                                  DEVICE INTERFACE GROUP DATA TYPE
*********************************************************************************************************
*/

typedef  struct  usbd_if_grp  {
            CPU_INT08U    ClassCode;                            /* IF     class code.                                   */
            CPU_INT08U    ClassSubCode;                         /* IF sub class code.                                   */
            CPU_INT08U    ClassProtocolCode;                    /* IF protocol  code.                                   */
            CPU_INT08U    IF_Start;                             /* IF index of the first IFs associated with a group.   */
            CPU_INT08U    IF_Cnt;                               /* Number of contiguous  IFs associated with a group.   */
    const   CPU_CHAR     *NamePtr;                              /* IF group name.                                       */
#if (USBD_CFG_OPTIMIZE_SPD == DEF_DISABLED)
    struct  usbd_if_grp  *NextPtr;                              /* Pointer to next interface group structure.           */
#endif
} USBD_IF_GRP;


/*
*********************************************************************************************************
*                                INTERFACE ALTERNATE SETTING DATA TYPE
*********************************************************************************************************
*/

typedef  struct  usbd_if_alt {
            void          *ClassArgPtr;                         /* Dev class drv arg ptr specific to alternate setting. */
            CPU_INT32U     EP_AllocMap;                         /* EP allocation bitmap.                                */
            CPU_INT08U     EP_NbrTotal;                         /* Number of EP.                                        */
    const   CPU_CHAR      *NamePtr;
#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)
            USBD_EP_INFO  *EP_TblPtrs[USBD_EP_MAX_NBR];
            CPU_INT32U     EP_TblMap;
#else
            USBD_EP_INFO  *EP_HeadPtr;
            USBD_EP_INFO  *EP_TailPtr;
#endif
#if (USBD_CFG_OPTIMIZE_SPD == DEF_DISABLED)
    struct  usbd_if_alt   *NextPtr;                             /* Pointer to next alternate setting structure.         */
#endif
} USBD_IF_ALT;


/*
*********************************************************************************************************
*                                         INTERFACE DATA TYPE
*
* Note(s):  (1) The interface structure contains information about the USB interfaces. It contains a
*               list of all alternate settings (including the default interface).
*
*                               IFs         | ---------------------  Alt IF Settings ------------------- |
*                                              Dflt           Alt_0          Alt_1     ...        Alt_n
*                -----       +--------+     +---------+    +---------+   +---------+           +---------+
*                  |  -----  | IF_0   |---->| IF_0_0  |--->| IF_0_0  |-->| IF_0_1  |-- ... --> | IF_0_1  |
*                  |    |    +--------+     +---------+    +---------+   +---------+           +---------+
*                  |    |        |
*                  |   GRP0      V
*                  |    |    +--------+     +---------+    +---------+   +---------+           +---------+
*                  |    |    | IF_1   |---->| IF_1_0  |--->| IF_1_0  |-->| IF_1_1  |-- ... --> | IF_1_1  |
*                  |  -----  +--------+     +---------+    +---------+   +---------+           +---------+
*                  |              |
*            CFGx  |              V
*                  |         +--------+     +---------+    +---------+   +---------+           +---------+
*                  |         | IF_2   |---->| IF_1_0  |--->| IF_1_0  |-->| IF_1_1  |-- ... --> | IF_1_1  |
*                  |         +--------+     +---------+    +---------+   +---------+           +---------+
*                  |             .
*                  |             .
*                  |             .
*                  |             |
*                  |             V
*                  |         +--------+     +---------+    +---------+   +---------+           +---------+
*                  |         | IF_n   |---->| IF_n_0  |--->| IF_n_0  |-->| IF_n_1  |-- ... --> | IF_n_1  |
*                  |         +--------+     +---------+    +---------+   +---------+           +---------+
*                ------
*
*            (2) Interfaces can be combined together creating a logical group.  This logical group
*                represents a function. The device uses the Interface Association Descriptor (IAD)
*                to notify the host that multiple interfaces belong to one single function.    The
*                'GrpNbr' stores the logical group number that the interface belongs to. By default,
*                it is defined to 'USBD_IF_GRP_NBR_NONE'.
*
*           (3)  The 'EP_AllocMap' is a bitmap of the allocated physical endpoints.
*********************************************************************************************************
*/

typedef  struct  usbd_if {
                                                                /* ------------ INTERFACE CLASS INFORMATION ----------- */
            CPU_INT08U        ClassCode;                        /* Device interface     class code.                     */
            CPU_INT08U        ClassSubCode;                     /* Device interface sub class code.                     */
            CPU_INT08U        ClassProtocolCode;                /* Device interface protocol  code.                     */
            USBD_CLASS_DRV   *ClassDrvPtr;                      /* Device class driver pointer.                         */
            void             *ClassArgPtr;                      /* Dev class drv arg ptr specific to interface.         */
                                                                /* ----------- INTERFACE ALTERNATE SETTINGS ----------- */
#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)
                                                                /* IF alternate settings array.                         */
            USBD_IF_ALT      *AltTblPtrs[USBD_CFG_MAX_NBR_IF_ALT];
#else
            USBD_IF_ALT      *AltHeadPtr;                       /* IF alternate settings linked-list.                   */
            USBD_IF_ALT      *AltTailPtr;
#endif
            USBD_IF_ALT      *AltCurPtr;                        /* Pointer to current alternate setting.                */
            CPU_INT08U        AltCur;                           /* Alternate setting selected by host.                  */
            CPU_INT08U        AltNbrTotal;                      /* Number of alternate settings supported by this IF.   */
            CPU_INT08U        GrpNbr;                           /* Interface group number.                              */
            CPU_INT32U        EP_AllocMap;                      /* EP allocation bitmap.                                */
#if (USBD_CFG_OPTIMIZE_SPD == DEF_DISABLED)
    struct  usbd_if          *NextPtr;                          /* Pointer to next interface structure.                 */
#endif
} USBD_IF;


/*
*********************************************************************************************************
*                                   DEVICE CONFIGURATION DATA TYPE
*
* Note(s):  (1) The configuration structure contains information about USB configurations. It contains a
*               list of interfaces.
*
*                                       CFG       | ----------------- INTERFACES  ---------------- |
*                        -----       +-------+    +------+    +------+    +------+          +------+
*                          |         | CFG_0 |--->| IF_0 |--->| IF_1 |--->| IF_2 |-- ... -->| IF_n |
*                          |         +-------+    +------+    +------+    +------+          +------+
*                          |             |
*                          |             V
*                          |         +-------+    +------+    +------+    +------+          +------+
*                          |         | CFG_1 |--->| IF_0 |--->| IF_1 |--->| IF_2 |-- ... -->| IF_n |
*                          |         +-------+    +------+    +------+    +------+          +------+
*                          |             |
*                          |             V
*                          |         +-------+    +------+    +------+    +------+          +------+
*                  DEVICEx |         | CFG_2 |--->| IF_0 |--->| IF_1 |--->| IF_2 |-- ... -->| IF_n |
*                          |         +-------+    +------+    +------+    +------+          +------+
*                          |             |
*                          |             V
*                          |             .
*                          |             .
*                          |             .
*                          |             |
*                          |             V
*                          |         +-------+    +------+    +------+    +------+          +------+
*                          |         | CFG_n |--->| IF_0 |--->| IF_1 |--->| IF_2 |-- ... -->| IF_n |
*                       ------       +-------+    +------+    +------+    +------+          +------+
*
*********************************************************************************************************
*/

typedef struct  usbd_cfg {                                      /* -------------- CONFIGURATION STRUCTURE ------------- */
            CPU_INT08U    Attrib;                               /* Configuration attributes.                            */
            CPU_INT16U    MaxPwr;                               /* Maximum bus power drawn.                             */
            CPU_INT16U    DescLen;                              /* Configuration descriptor length.                     */
    const   CPU_CHAR     *NamePtr;                              /* Configuration name.                                  */

#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)                      /* Interface & group list:                              */
            USBD_IF      *IF_TblPtrs[USBD_CFG_MAX_NBR_IF];      /*     Interfaces list (array).                         */
                                                                /*     Interfaces group list (array).                   */
#if (USBD_CFG_MAX_NBR_IF_GRP > 0)
            USBD_IF_GRP  *IF_GrpTblPtrs[USBD_CFG_MAX_NBR_IF_GRP];
#endif
#else
            USBD_IF      *IF_HeadPtr;                           /*     Interfaces list (linked list).                   */
            USBD_IF      *IF_TailPtr;
            USBD_IF_GRP  *IF_GrpHeadPtr;                        /*     Interfaces group list (linked list).             */
            USBD_IF_GRP  *IF_GrpTailPtr;
#endif

            CPU_INT08U    IF_NbrTotal;                          /* Number of interfaces in this configuration.          */
            CPU_INT08U    IF_GrpNbrTotal;                       /* Number of interfaces group.                          */
            CPU_INT32U    EP_AllocMap;                          /* EP allocation bitmap.                                */

#if (USBD_CFG_HS_EN == DEF_ENABLED)
            CPU_INT08U    CfgOtherSpd;                          /* Other-speed configuration.                           */
#endif
#if (USBD_CFG_OPTIMIZE_SPD == DEF_DISABLED)
    struct  usbd_cfg     *NextPtr;
#endif
} USBD_CFG;


/*
*********************************************************************************************************
*                                        USB DEVICE DATA TYPE
*
* Note(s):  (1) A USB device could contain multiple configurations. A configuration is a set of
*               interfaces.
*
*               USB Spec 2.0 section 9.2.6.6 states "device capable of operation at high-speed
*               can operate in either full- or high-speed. The device always knows its operational
*               speed due to having to manage its transceivers correctly as part of reset processing."
*
*              "A device also operates at a single speed after completing the reset sequence. In
*               particular, there is no speed switch during normal operation. However, a high-
*               speed capable device may have configurations that are speed dependent. That is,
*               it may have some configurations that are only possible when operating at high-
*               speed or some that are only possible when operating at full-speed. High-speed
*               capable devices must support reporting their speed dependent configurations."
*
*               The device structure contains two list of configurations for HS and FS.
*
*                   +--------+    +-------+    +-------+    +-------+          +-------+
*                   | HS_CFG |--->| CFG_0 |--->| CFG_1 |--->| CFG_2 |-- ... -->| CFG_n |
*                   +--------+    +-------+    +-------+    +-------+          +-------+
*
*                   +--------+    +-------+    +-------+    +-------+          +-------+
*                   | FS_CFG |--->| CFG_0 |--->| CFG_1 |--->| CFG_2 |-- ... -->| CFG_n |
*                   +--------+    +-------+    +-------+    +-------+          +-------+
*
*           (2) If the USB stack is optimized for speed, objects (Cfgs, IFs, EPs, etc) are implemented
*               using a hash linking. Pointers are stored in an array allowing easy access by index.
*
*           (3) If the USB stack is optimized for size,  objects (Cfgs, IFs, EPs, etc) are implemented
*               using a link list. Objects are linked dynamically reducing the overall memory footprint.
*********************************************************************************************************
*/

typedef  struct  usbd_dev {                                     /* ----------------- DEVICE STRUCTURE ----------------- */
           CPU_INT08U       Addr;                               /* Device address assigned by host.                     */
           USBD_DEV_STATE   State;                              /* Device state.                                        */
           USBD_DEV_STATE   StatePrev;                          /* Device previous state.                               */
           CPU_BOOLEAN      ConnStatus;                         /* Device connection status.                            */
           USBD_DEV_SPD     Spd;                                /* Device operating speed.                              */
           CPU_INT08U       Nbr;                                /* Device instance number                               */
           USBD_DEV_CFG    *DevCfgPtr;                          /* Device configuration pointer.                        */
           USBD_DRV         Drv;                                /* Device driver information.                           */
                                                                /* --------------- DEVICE CONFIGURATIONS -------------- */
#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)                      /* Configuration list (see Note #1).                    */
                                                                /*     FS configuration array (see Note #2).            */
           USBD_CFG        *CfgFS_SpdTblPtrs[USBD_CFG_MAX_NBR_CFG];
#if (USBD_CFG_HS_EN == DEF_ENABLED)
                                                                /*     HS configuration array (see Note #2).            */
           USBD_CFG        *CfgHS_SpdTblPtrs[USBD_CFG_MAX_NBR_CFG];
#endif
#else
           USBD_CFG        *CfgFS_HeadPtr;                      /*     FS configuration linked-list (see Note #3).      */
           USBD_CFG        *CfgFS_TailPtr;
#if (USBD_CFG_HS_EN == DEF_ENABLED)
           USBD_CFG        *CfgHS_HeadPtr;                      /*     HS configuration linked-list (see Note #3).      */
           USBD_CFG        *CfgHS_TailPtr;
#endif
#endif
           USBD_CFG        *CfgCurPtr;                          /* Current device configuration pointer.                */
           CPU_INT08U       CfgCurNbr;                          /* Current device configuration number.                 */

           CPU_INT08U       CfgFS_TotalNbr;                     /* Number of FS configurations supported by the device. */
#if (USBD_CFG_HS_EN == DEF_ENABLED)
           CPU_INT08U       CfgHS_TotalNbr;                     /* Number of HS configurations supported by the device. */
#endif
                                                                /* ---- CONFIGURATION AND STRING DESCRIPTOR BUFFER ---- */
           CPU_INT08U      *ActualBufPtr;                       /* Pointer to the buffer where data will be written.    */
           CPU_INT08U      *DescBufPtr;                         /* Configuration & string descriptor buffer.            */
           CPU_INT08U       DescBufIx;                          /* Configuration & string descriptor buffer index.      */
           CPU_INT16U       DescBufReqLen;                      /* Configuration & string descriptor requested length.  */
           CPU_INT16U       DescBufMaxLen;                      /* Configuration & string descriptor maximum length.    */
           USBD_ERR        *DescBufErrPtr;                      /* Configuration & string descriptor error pointer.     */
                                                                /* --------------- ENDPOINT INFORMATION  -------------- */
           CPU_INT16U       EP_CtrlMaxPktSize;                  /* Ctrl EP maximum packet size.                         */
           CPU_INT08U       EP_IF_Tbl[USBD_EP_MAX_NBR];         /* EP to IF number reference table.                     */
           CPU_INT08U       EP_MaxPhyNbr;                       /* EP Maximum physical number.                          */
                                                                /* ------------------ STRING STORAGE  ----------------- */
#if (USBD_CFG_MAX_NBR_STR > 0u)
    const  CPU_CHAR        *StrDesc_Tbl[USBD_CFG_MAX_NBR_STR];  /* String pointers table.                               */
           CPU_INT08U       StrMaxIx;                           /* Current String index.                                */
#endif
#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
           CPU_INT08U       StrMS_VendorCode;                   /* Microsoft Vendor code used in Microsoft OS str.      */
#endif

           USBD_BUS_FNCTS  *BusFnctsPtr;                        /* Pointer to bus events callback functions.            */
           USBD_SETUP_REQ   SetupReq;                           /* Setup request.                                       */
           USBD_SETUP_REQ   SetupReqNext;                       /* Next setup request.                                  */

           CPU_BOOLEAN      SelfPwr;                            /* Device self powered?                                 */

           CPU_BOOLEAN      RemoteWakeup;                       /* Remote Wakeup feature.                               */

           CPU_INT08U      *CtrlStatusBufPtr;                   /* Buf used for ctrl status xfers.                      */
} USBD_DEV;


/*
*********************************************************************************************************
*                                              USB CORE EVENTS
*
* Note(s) : (1) USB device driver queues bus and transaction events to the core task queue using
*               the 'USBD_CORE_EVENT' structure.
*********************************************************************************************************
*/

typedef  struct  usbd_core_event {
    USBD_EVENT_CODE   Type;                                     /* Core event type.                                     */
    USBD_DRV         *DrvPtr;                                   /* Pointer to driver structure.                         */
    CPU_INT08U        EP_Addr;                                  /* Endpoint address.                                    */
    USBD_ERR          Err;                                      /* Error Code returned by Driver, if any.               */
} USBD_CORE_EVENT;


/*
*********************************************************************************************************
*                                         USB DEBUG DATA TYPE
*********************************************************************************************************
*/

#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
typedef struct  usbd_dbg_event {
    const   CPU_CHAR        *MsgPtr;
            CPU_INT08U       EP_Addr;
            CPU_INT08U       IF_Nbr;
            CPU_BOOLEAN      ArgEn;
            CPU_INT32U       Arg;
            USBD_ERR         Err;
            CPU_INT32U       Cnt;
            CPU_TS           Ts;
    struct  usbd_dbg_event  *NextPtr;
} USBD_DBG_EVENT;
#endif


/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           LOCAL MACROS'S
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          USB OBJECTS POOL
*
* Note(s) : (1) USB objects (device, configuration, interfaces, alternative interface, endpoint, etc)
*               are allocated from their pools.
*
*               (a) USB objects CANNOT be returned to the pool.
*********************************************************************************************************
*/


static  USBD_DEV            USBD_DevTbl[USBD_CFG_MAX_NBR_DEV];  /* Device object pool.                                  */
static  CPU_INT08U          USBD_DevNbrNext;

static  USBD_CFG            USBD_CfgTbl[USBD_CFG_MAX_NBR_CFG];  /* Configuration object pool.                           */
static  CPU_INT08U          USBD_CfgNbrNext;

static  USBD_IF             USBD_IF_Tbl[USBD_CFG_MAX_NBR_IF];   /* Interface object pool.                               */
static  CPU_INT08U          USBD_IF_NbrNext;

                                                                /* Alternative interface object pool.                   */
static  USBD_IF_ALT         USBD_IF_AltTbl[USBD_CFG_MAX_NBR_IF_ALT];
static  CPU_INT08U          USBD_IF_AltNbrNext;

#if (USBD_CFG_MAX_NBR_IF_GRP > 0)
                                                                /* Interface group object pool.                         */
static  USBD_IF_GRP         USBD_IF_GrpTbl[USBD_CFG_MAX_NBR_IF_GRP];
static  CPU_INT08U          USBD_IF_GrpNbrNext;
#endif
                                                                /* Endpoints object pool.                               */
static  USBD_EP_INFO        USBD_EP_InfoTbl[USBD_CFG_MAX_NBR_EP_DESC];
static  CPU_INT08U          USBD_EP_InfoNbrNext;

#if (USBD_CFG_DBG_STATS_EN == DEF_ENABLED)
        USBD_DBG_STATS_DEV  USBD_DbgStatsDevTbl[USBD_CFG_MAX_NBR_DEV];
#endif


/*
*********************************************************************************************************
*                                        CORE EVENTS POOL
*
* Note(s) : (1) USB device driver signals the core task using a core event queue.
*               The core event queue contains core event objects. These objects are
*               allocated from the core event pool.
*********************************************************************************************************
*/

static  CPU_INT32U        USBD_CoreEventPoolIx;
static  USBD_CORE_EVENT   USBD_CoreEventPoolData[USBD_CORE_EVENT_NBR_TOTAL];
static  USBD_CORE_EVENT  *USBD_CoreEventPoolPtrs[USBD_CORE_EVENT_NBR_TOTAL];


/*
*********************************************************************************************************
*                                          DEBUG EVENTS POOL
*********************************************************************************************************
*/

#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
                                                                /* Debug event pool.                                    */
static  USBD_DBG_EVENT   USBD_DbgEventTbl[USBD_CFG_DBG_TRACE_NBR_EVENTS];

static  USBD_DBG_EVENT  *USBD_DbgEventFreePtr;                  /* Free debug events list pointer.                      */

static  USBD_DBG_EVENT  *USBD_DbgEventHeadPtr;                  /* Debug events list head pointer.                      */
static  USBD_DBG_EVENT  *USBD_DbgEventTailPtr;                  /* Debug events list tail pointer.                      */
static  CPU_INT32U       USBD_DbgEventCtr;                      /* Global debug event counter.                          */
#endif


/*
*********************************************************************************************************
*                                           TRACING MACROS
*********************************************************************************************************
*/


#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
#define  USBD_DBG_CORE_BUS(msg)                      USBD_DBG_GENERIC((msg),                                                \
                                                                       USBD_EP_ADDR_NONE,                                   \
                                                                       USBD_IF_NBR_NONE)

#define  USBD_DBG_CORE_STD(msg)                      USBD_DBG_GENERIC((msg),                                                \
                                                                       0u,                                                  \
                                                                       USBD_IF_NBR_NONE)

#define  USBD_ERR_LINE_0(x)                          #x
#define  USBD_ERR_LINE(x)                            USBD_ERR_LINE_0(x)
#define  USBD_DBG_CORE_STD_ERR(msg, err)             USBD_DBG_GENERIC_ERR( msg " line: "USBD_ERR_LINE(__LINE__)" error: ", \
                                                                           0u,                                              \
                                                                           USBD_IF_NBR_NONE,                                \
                                                                          (err))

#define  USBD_DBG_CORE_STD_ARG(msg, arg)             USBD_DBG_GENERIC_ARG((msg),                                            \
                                                                           0u,                                              \
                                                                           USBD_IF_NBR_NONE,                                \
                                                                          (arg))
#else
#define  USBD_DBG_CORE_BUS(msg)
#define  USBD_DBG_CORE_STD(msg)
#define  USBD_DBG_CORE_STD_ERR(msg, err)
#define  USBD_DBG_CORE_STD_ARG(msg, arg)
#endif


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/
                                                                /* ------------- STANDARD REQUEST HANDLERS ------------ */
static  void               USBD_StdReqHandler(       USBD_DEV         *p_dev);

static  CPU_BOOLEAN        USBD_StdReqDev    (       USBD_DEV         *p_dev,
                                                     CPU_INT08U        request);

static  CPU_BOOLEAN        USBD_StdReqIF     (       USBD_DEV         *p_dev,
                                                     CPU_INT08U        request);

static  CPU_BOOLEAN        USBD_StdReqEP     (const  USBD_DEV         *p_dev,
                                                     CPU_INT08U        request);

static  CPU_BOOLEAN        USBD_StdReqClass  (const  USBD_DEV         *p_dev);

static  CPU_BOOLEAN        USBD_StdReqVendor (const  USBD_DEV         *p_dev);

#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
static  CPU_BOOLEAN        USBD_StdReqDevMS  (const  USBD_DEV         *p_dev);

static  CPU_BOOLEAN        USBD_StdReqIF_MS  (const  USBD_DEV         *p_dev);
#endif

static  CPU_BOOLEAN        USBD_StdReqDescGet(       USBD_DEV         *p_dev);

#if 0
static  CPU_BOOLEAN        USBD_StdReqDescSet(       USBD_DEV         *p_dev);
#endif

static  void               USBD_CfgClose     (       USBD_DEV         *p_dev);

static  void               USBD_CfgOpen      (       USBD_DEV         *p_dev,
                                                     CPU_INT08U        cfg_nbr,
                                                     USBD_ERR         *p_err);

static  void               USBD_DevDescSend  (       USBD_DEV         *p_dev,
                                                     CPU_BOOLEAN       other,
                                                     CPU_INT16U        req_len,
                                                     USBD_ERR         *p_err);

static  void               USBD_CfgDescSend  (       USBD_DEV         *p_dev,
                                                     CPU_INT08U        cfg_nbr,
                                                     CPU_BOOLEAN       other,
                                                     CPU_INT16U        req_len,
                                                     USBD_ERR         *p_err);

static  void               USBD_StrDescSend  (       USBD_DEV         *p_dev,
                                                     CPU_INT08U        str_ix,
                                                     CPU_INT16U        req_len,
                                                     USBD_ERR         *p_err);

#if (USBD_CFG_MAX_NBR_STR > 0u)
static  void               USBD_StrDescAdd   (       USBD_DEV         *p_dev,
                                              const  CPU_CHAR         *p_str,
                                                     USBD_ERR         *p_err);
#endif

static  CPU_INT08U         USBD_StrDescIxGet (const  USBD_DEV         *p_dev,
                                              const  CPU_CHAR         *p_str);

static  const  CPU_CHAR   *USBD_StrDescGet   (const  USBD_DEV         *p_dev,
                                                     CPU_INT08U        str_nbr);

static  void               USBD_DescWrStart  (       USBD_DEV         *p_dev,
                                                     CPU_INT16U        req_len);

static  void               USBD_DescWrStop   (       USBD_DEV         *p_dev,
                                                     USBD_ERR         *p_err);

static  void               USBD_DescWrReq08  (       USBD_DEV         *p_dev,
                                                     CPU_INT08U        val);

static  void               USBD_DescWrReq16  (       USBD_DEV         *p_dev,
                                                     CPU_INT16U        val);

static  void               USBD_DescWrReq    (       USBD_DEV         *p_dev,
                                              const  CPU_INT08U       *p_buf,
                                                     CPU_INT16U        len);

                                                                /* --------------- USB OBJECT FUNCTIONS --------------- */
static  USBD_DEV          *USBD_DevRefGet    (       CPU_INT08U        dev_nbr);

static  USBD_CFG          *USBD_CfgRefGet    (const  USBD_DEV         *p_dev,
                                                     CPU_INT08U        cfg_nbr);

static  USBD_IF           *USBD_IF_RefGet    (const  USBD_CFG         *p_cfg,
                                                     CPU_INT08U        if_nbr);

static  USBD_IF_ALT       *USBD_IF_AltRefGet (const  USBD_IF          *p_if,
                                                     CPU_INT08U        if_alt_nbr);

static  void               USBD_IF_AltOpen   (       USBD_DEV         *p_dev,
                                                     CPU_INT08U        if_nbr,
                                              const  USBD_IF_ALT      *p_if_alt,
                                                     USBD_ERR         *p_err);

static  void               USBD_IF_AltClose  (       USBD_DEV         *p_dev,
                                              const  USBD_IF_ALT      *p_if_alt);
#if (USBD_CFG_MAX_NBR_IF_GRP > 0)
static  USBD_IF_GRP       *USBD_IF_GrpRefGet (const  USBD_CFG         *p_cfg,
                                                     CPU_INT08U        if_grp_nbr);
#endif

static  void               USBD_EventSet     (       USBD_DRV         *p_drv,
                                                     USBD_EVENT_CODE   event);

static  void               USBD_EventProcess (       USBD_DEV         *p_dev,
                                                     USBD_EVENT_CODE   event);


static  CPU_INT08U         USBD_EP_Add       (       CPU_INT08U        dev_nbr,
                                                     CPU_INT08U        cfg_nbr,
                                                     CPU_INT08U        if_nbr,
                                                     CPU_INT08U        if_alt_nbr,
                                                     CPU_INT08U        attrib,
                                                     CPU_BOOLEAN       dir_in,
                                                     CPU_INT16U        max_pkt_len,
                                                     CPU_INT08U        interval,
                                                     USBD_ERR         *p_err);

static  CPU_BOOLEAN        USBD_EP_Alloc     (       USBD_DEV         *p_dev,
                                                     USBD_DEV_SPD      spd,
                                                     CPU_INT08U        type,
                                                     CPU_BOOLEAN       dir_in,
                                                     CPU_INT16U        max_pkt_len,
                                                     CPU_INT08U        if_alt_nbr,
                                                     USBD_EP_INFO     *p_ep,
                                                     CPU_INT32U       *p_alloc_bit_map);

static  void               USBD_CoreEventFree(       USBD_CORE_EVENT   *p_core_event);

static  USBD_CORE_EVENT   *USBD_CoreEventGet (void);

#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
static  USBD_DBG_EVENT    *USBD_DbgEventGet  (void);

static  void               USBD_DbgEventFree (       USBD_DBG_EVENT    *p_event);

static  void               USBD_DbgEventPut  (       USBD_DBG_EVENT    *p_event);
#endif


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          GLOBAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                             USBD_Init()
*
* Description : (1) Initialize USB device stack:
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           Device stack successfully initialized.
*                               USBD_ERR_ALLOC          Allocation failed.
*
*                                                       ---- RETURNED BY USBD_OS_Init() : ----
*                               USBD_ERR_OS_INIT_FAIL   OS layer NOT successfully initialized.
*
* Return(s)   : none.
*
* Note(s)     : (1) USBD_Init() MUST be called ... :
*
*                   (a) ONLY ONCE from a product's application; ...
*                   (b) (1) AFTER  product's OS has been initialized.
*                       (2) BEFORE product's application calls any USB device stack function(s).
*********************************************************************************************************
*/

void  USBD_Init (USBD_ERR  *p_err)
{
    USBD_DEV        *p_dev;
    USBD_CFG        *p_cfg;
    USBD_IF         *p_if;
    USBD_IF_ALT     *p_if_alt;
#if (USBD_CFG_MAX_NBR_IF_GRP > 0)
    USBD_IF_GRP     *p_if_grp;
#endif
    USBD_EP_INFO    *p_ep;
#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
    USBD_DBG_EVENT  *p_event;
#endif
    CPU_INT16U       tbl_ix;
    LIB_ERR          err_lib;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

    USBD_OS_Init(p_err);                                        /* Initialize OS Interface.                             */
    if (*p_err != USBD_ERR_NONE) {
        return;
    }
                                                                /* ------------ DEVICE TABLE INITIALIZATION ----------- */
    for (tbl_ix = 0u; tbl_ix < USBD_CFG_MAX_NBR_DEV; tbl_ix++) {
        p_dev             = &USBD_DevTbl[tbl_ix];
        p_dev->Addr       = 0u;                                 /* Dflt dev addr.                                       */
        p_dev->State      = USBD_DEV_STATE_NONE;
        p_dev->StatePrev  = USBD_DEV_STATE_NONE;
        p_dev->ConnStatus = DEF_FALSE;
        p_dev->Spd        = USBD_DEV_SPD_INVALID;
        p_dev->DevCfgPtr  = (USBD_DEV_CFG *)0;

#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)                      /* Init HS & FS cfg list:                               */
                                                                /*    array implementation.                             */
        Mem_Clr((void      *)&p_dev->CfgFS_SpdTblPtrs[0u],
                (CPU_SIZE_T )USBD_CFG_MAX_NBR_CFG * (sizeof(USBD_DEV_CFG *)));

#if (USBD_CFG_HS_EN == DEF_ENABLED)
        Mem_Clr((void      *)&p_dev->CfgHS_SpdTblPtrs[0u],
                (CPU_SIZE_T )USBD_CFG_MAX_NBR_CFG * (sizeof(USBD_DEV_CFG *)));
#endif

#else
        p_dev->CfgFS_HeadPtr = (USBD_CFG *)0;                   /*     linked-list implementation.                      */
        p_dev->CfgFS_TailPtr = (USBD_CFG *)0;
#if (USBD_CFG_HS_EN == DEF_ENABLED)
        p_dev->CfgHS_HeadPtr = (USBD_CFG *)0;
        p_dev->CfgHS_TailPtr = (USBD_CFG *)0;
#endif
#endif

        p_dev->CfgCurPtr      = (USBD_CFG *)0;
        p_dev->CfgCurNbr      = USBD_CFG_NBR_NONE;
        p_dev->CfgFS_TotalNbr = 0u;
#if (USBD_CFG_HS_EN == DEF_ENABLED)
        p_dev->CfgHS_TotalNbr = 0u;
#endif

        Mem_Clr((void     *)&p_dev->EP_IF_Tbl[0u],
                (CPU_SIZE_T)(USBD_EP_MAX_NBR * (sizeof(CPU_INT08U))));

                                                                /* Alloc desc buf from heap.                            */
        p_dev->DescBufPtr = (CPU_INT08U *)Mem_HeapAlloc(              USBD_CFG_DESC_BUF_LEN,
                                                                      USBD_CFG_BUF_ALIGN_OCTETS,
                                                        (CPU_SIZE_T *)DEF_NULL,
                                                                     &err_lib);
        if (err_lib != LIB_MEM_ERR_NONE) {
           *p_err = USBD_ERR_ALLOC;
            return;
        }

        Mem_Clr((void *)&p_dev->DescBufPtr[0u],
                         USBD_CFG_DESC_BUF_LEN);
                                                                /* Alloc ctrl status buf from heap.                     */
        p_dev->CtrlStatusBufPtr = (CPU_INT08U *)Mem_HeapAlloc(              2u,
                                                                            USBD_CFG_BUF_ALIGN_OCTETS,
                                                              (CPU_SIZE_T *)DEF_NULL,
                                                                           &err_lib);
        if (err_lib != LIB_MEM_ERR_NONE) {
           *p_err = USBD_ERR_ALLOC;
            return;
        }

        p_dev->ActualBufPtr  =  p_dev->DescBufPtr;
        p_dev->DescBufIx     =  0u;
        p_dev->DescBufReqLen =  0u;
        p_dev->DescBufMaxLen =  USBD_CFG_DESC_BUF_LEN;
        p_dev->DescBufErrPtr = (USBD_ERR *)0u;

#if (USBD_CFG_MAX_NBR_STR > 0u)
        Mem_Clr((void     *)&p_dev->StrDesc_Tbl[0u],
                (CPU_SIZE_T)USBD_CFG_MAX_NBR_STR);

        p_dev->StrMaxIx         = 0u;
#endif
#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
        p_dev->StrMS_VendorCode = 0u;
#endif
        p_dev->BusFnctsPtr      = (USBD_BUS_FNCTS *)0;

        Mem_Clr((void     *)&p_dev->SetupReq,
                (CPU_SIZE_T)sizeof(USBD_SETUP_REQ));

        Mem_Clr((void     *)&p_dev->SetupReqNext,
                (CPU_SIZE_T)sizeof(USBD_SETUP_REQ));

        p_dev->EP_CtrlMaxPktSize = 0u;
        p_dev->EP_MaxPhyNbr      = 0u;

        Mem_Set((void      *)&p_dev->EP_IF_Tbl[0u],
                              USBD_IF_NBR_NONE,
                (CPU_SIZE_T ) USBD_EP_MAX_NBR);

        p_dev->SelfPwr         =  DEF_NO;
        p_dev->RemoteWakeup    =  DEF_DISABLED;
        p_dev->Drv.DevNbr      =  USBD_DEV_NBR_NONE;
        p_dev->Drv.API_Ptr     = (USBD_DRV_API     *)0;
        p_dev->Drv.CfgPtr      = (USBD_DRV_CFG     *)0;
        p_dev->Drv.DataPtr     = (void             *)0;
        p_dev->Drv.BSP_API_Ptr = (USBD_DRV_BSP_API *)0;

        USBD_DBG_STATS_DEV_RESET(tbl_ix);
        USBD_DBG_STATS_DEV_SET_DEV_NBR(tbl_ix);
    }
                                                                /* -------- CONFIGURATION TABLE INITIALIZATION -------- */
    for (tbl_ix = 0u; tbl_ix < USBD_CFG_MAX_NBR_CFG; tbl_ix++) {
        p_cfg          = &USBD_CfgTbl[tbl_ix];
        p_cfg->Attrib  = DEF_BIT_NONE;
        p_cfg->MaxPwr  = 0u;
        p_cfg->NamePtr = (CPU_CHAR *)0;
        p_cfg->DescLen = 0u;
#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)                      /* Init IF list:                                        */
                                                                /*    array implementation.                             */
        Mem_Clr((void     *)&p_cfg->IF_TblPtrs[0u],
                (CPU_SIZE_T)USBD_CFG_MAX_NBR_IF * (sizeof(USBD_IF *)));

#if (USBD_CFG_MAX_NBR_IF_GRP > 0)
        Mem_Clr((void     *)&p_cfg->IF_GrpTblPtrs[0u],
                (CPU_SIZE_T)USBD_CFG_MAX_NBR_IF_GRP * (sizeof(USBD_IF_GRP *)));
#endif

#else
        p_cfg->IF_HeadPtr     = (USBD_IF     *)0;               /*    linked-list implementation.                      */
        p_cfg->IF_TailPtr     = (USBD_IF     *)0;
        p_cfg->IF_GrpHeadPtr  = (USBD_IF_GRP *)0;
        p_cfg->IF_GrpTailPtr  = (USBD_IF_GRP *)0;
        p_cfg->NextPtr        = (USBD_CFG    *)0;
#endif
        p_cfg->IF_NbrTotal    = 0u;
        p_cfg->IF_GrpNbrTotal = 0u;
        p_cfg->EP_AllocMap    = DEF_BIT_NONE;
#if (USBD_CFG_HS_EN == DEF_ENABLED)
        p_cfg->CfgOtherSpd    = USBD_CFG_NBR_NONE;
#endif
    }
                                                                /* ---------- INTERFACE TABLE INITIALIZATION ---------- */
    for (tbl_ix = 0u; tbl_ix < USBD_CFG_MAX_NBR_IF; tbl_ix++) {
        p_if                    = &USBD_IF_Tbl[tbl_ix];
        p_if->ClassCode         =  USBD_CLASS_CODE_USE_IF_DESC;
        p_if->ClassSubCode      =  USBD_SUBCLASS_CODE_USE_IF_DESC;
        p_if->ClassProtocolCode =  USBD_PROTOCOL_CODE_USE_IF_DESC;
        p_if->ClassDrvPtr       = (USBD_CLASS_DRV *)0;
        p_if->ClassArgPtr       = (void           *)0;

#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)
        Mem_Clr((void     *)&p_if->AltTblPtrs[0u],
                (CPU_SIZE_T)USBD_CFG_MAX_NBR_IF_ALT * (sizeof(USBD_IF_ALT *)));
#else
        p_if->AltHeadPtr  = (USBD_IF_ALT *)0;
        p_if->AltTailPtr  = (USBD_IF_ALT *)0;
#endif
        p_if->AltCurPtr   = (USBD_IF_ALT *)0;
        p_if->AltCur      = USBD_IF_ALT_NBR_NONE;
        p_if->AltNbrTotal = 0u;
        p_if->GrpNbr      = USBD_IF_GRP_NBR_NONE;
        p_if->EP_AllocMap = DEF_BIT_NONE;
#if (USBD_CFG_OPTIMIZE_SPD == DEF_DISABLED)
        p_if->NextPtr     = (USBD_IF *)0;
#endif
    }
                                                                /* ------ ALTERNATE SETTINGS TABLE INITIALIZATION ----- */
    for (tbl_ix = 0u; tbl_ix < USBD_CFG_MAX_NBR_IF_ALT; tbl_ix++) {
        p_if_alt              = &USBD_IF_AltTbl[tbl_ix];
        p_if_alt->ClassArgPtr = (void *)0;
        p_if_alt->EP_AllocMap = DEF_BIT_NONE;
        p_if_alt->EP_NbrTotal = 0u;
        p_if_alt->NamePtr     = (const CPU_CHAR *)0;
#if (USBD_CFG_OPTIMIZE_SPD == DEF_DISABLED)
        p_if_alt->NextPtr     = (USBD_IF_ALT *)0;
#endif
#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)
        Mem_Clr((void     *)&p_if_alt->EP_TblPtrs[0u],
                (CPU_SIZE_T)USBD_EP_MAX_NBR * (sizeof(USBD_EP_INFO *)));
        p_if_alt->EP_TblMap  = 0u;
#else
        p_if_alt->EP_HeadPtr = (USBD_EP_INFO *)0;
        p_if_alt->EP_TailPtr = (USBD_EP_INFO *)0;
#endif
    }
                                                                /* ------- INTERFACE GROUP TABLE INITIALIZATION ------- */
#if (USBD_CFG_MAX_NBR_IF_GRP > 0)
    for (tbl_ix = 0u; tbl_ix <  USBD_CFG_MAX_NBR_IF_GRP; tbl_ix++) {
        p_if_grp                    = &USBD_IF_GrpTbl[tbl_ix];
        p_if_grp->ClassCode         = USBD_CLASS_CODE_USE_IF_DESC;
        p_if_grp->ClassSubCode      = USBD_SUBCLASS_CODE_USE_IF_DESC;
        p_if_grp->ClassProtocolCode = USBD_PROTOCOL_CODE_USE_IF_DESC;
        p_if_grp->IF_Start          = USBD_IF_NBR_NONE;
        p_if_grp->IF_Cnt            = 0u;
        p_if_grp->NamePtr           = (const CPU_CHAR *)0;
#if (USBD_CFG_OPTIMIZE_SPD == DEF_DISABLED)
        p_if_grp->NextPtr           = (USBD_IF_GRP    *)0;
#endif
    }
#endif
                                                                /* ----- ENDPOINT INFORMATION TABLE INITIALIZATION ---- */
    for (tbl_ix = 0u; tbl_ix < USBD_CFG_MAX_NBR_EP_DESC; tbl_ix++) {
        p_ep              = &USBD_EP_InfoTbl[tbl_ix];
        p_ep->Addr        =  USBD_EP_NBR_NONE;
        p_ep->Attrib      =  DEF_BIT_NONE;
        p_ep->Interval    =  0u;
        p_ep->SyncAddr    =  0u;                                /* Dflt sync addr is zero.                              */
        p_ep->SyncRefresh =  0u;                                /* Dflt feedback rate exponent is zero.                 */
#if (USBD_CFG_OPTIMIZE_SPD == DEF_DISABLED)
        p_ep->NextPtr     = (USBD_EP_INFO  *)0;
#endif
    }

                                                                /* Init pool of core events.                            */
    for (tbl_ix = 0u; tbl_ix < USBD_CORE_EVENT_NBR_TOTAL; tbl_ix++) {
        USBD_CoreEventPoolPtrs[tbl_ix] = &USBD_CoreEventPoolData[tbl_ix];
    }

                                                                /* Init pool of debug events.                           */
#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
    for (tbl_ix = 0u; tbl_ix < (USBD_CFG_DBG_TRACE_NBR_EVENTS - 1u); tbl_ix++) {
        p_event          = &USBD_DbgEventTbl[tbl_ix];
        p_event->NextPtr = &USBD_DbgEventTbl[tbl_ix + 1u];
    }

    p_event              = &USBD_DbgEventTbl[tbl_ix];
    p_event->NextPtr     = (USBD_DBG_EVENT *)0;
    USBD_DbgEventCtr     = 0u;
    USBD_DbgEventFreePtr = &USBD_DbgEventTbl[0u];
#endif

    USBD_DevNbrNext       = 0u;
    USBD_CfgNbrNext       = 0u;
    USBD_IF_NbrNext       = 0u;
    USBD_IF_AltNbrNext    = 0u;
#if (USBD_CFG_MAX_NBR_IF_GRP > 0)
    USBD_IF_GrpNbrNext    = 0u;
#endif
    USBD_EP_InfoNbrNext   = 0u;
    USBD_CoreEventPoolIx  = USBD_CORE_EVENT_NBR_TOTAL;

    USBD_EP_Init();
}


/*
*********************************************************************************************************
*                                          USBD_VersionGet()
*
* Description : Get USB Device stack version.
*
* Argument(s) : none.
*
* Return(s)   : USB Device stack version (see Note #1b).
*
* Note(s)     : (1) (a) The USB Device software version is denoted as follows :
*
*                       Vx.yy.zz
*
*                           where
*                                   V               denotes 'Version' label
*                                   x               denotes major software version revision number
*                                   yy              denotes minor software version revision number
*                                   zz              denotes sub-minor software version revision number
*
*               (b) The software version label #define is formatted as follows :
*
*                       ver = x.yyzz * 100 * 100
*
*                           where
*                                   ver             denotes software version number scaled as an integer value
*                                   x.yyzz          denotes software version number, where the unscaled integer
*                                                       portion denotes the major version number & the unscaled
*                                                       fractional portion denotes the (concatenated) minor
*                                                       version numbers
*********************************************************************************************************
*/

CPU_INT16U  USBD_VersionGet (void)
{
    CPU_INT16U  ver;


    ver = USBD_VERSION;

    return (ver);
}


/*
*********************************************************************************************************
*                                            USBD_DevAdd()
*
* Description : Add a device to the stack:
*
*                   (a) Create default control endpoints.
*
* Argument(s) : p_dev_cfg   Pointer to specific USB device configuration.
*
*               p_bus_fnct  Pointer to specific USB device bus events callback functions.
*
*               p_drv_api   Pointer to specific USB device driver API.
*
*               p_drv_cfg   Pointer to specific USB device driver configuration.
*
*               p_bsp_api   Pointer to specific USB device board-specific API.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Device successfully added.
*                               USBD_ERR_NULL_PTR           Argument 'p_dev_cfg'/'p_drv_api'/'p_drv_cfg'/
*                                                               'p_bsp_api' passed a NULL pointer.
*                               USBD_ERR_DEV_ALLOC          NO available USB devices (see 'usbd_cfg.h'
*                                                               USBD_CFG_MAX_NBR_DEV).
*                               USBD_ERR_EP_NONE_AVAIL      Physical endpoint NOT available.
*
* Return(s)   : Device number,     if NO error(s).
*
*               USBD_DEV_NBR_NONE, otherwise.
*
* Note(s)     : (1) Some driver functions are required for the driver to work correctly with the core.
*                   The pointers to these functions are checked in this function to make sure they are
*                   valid and can be used throughout the core.
*********************************************************************************************************
*/

CPU_INT08U  USBD_DevAdd (USBD_DEV_CFG      *p_dev_cfg,
                         USBD_BUS_FNCTS    *p_bus_fnct,
                         USBD_DRV_API      *p_drv_api,
                         USBD_DRV_CFG      *p_drv_cfg,
                         USBD_DRV_BSP_API  *p_bsp_api,
                         USBD_ERR          *p_err)
{
    USBD_DEV      *p_dev;
    CPU_INT08U     dev_nbr;
    CPU_INT08U     ep_phy_nbr;
    CPU_INT32U     ep_alloc_map;
    USBD_EP_INFO   ep_info;
    CPU_BOOLEAN    alloc;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }

    if ((p_dev_cfg == (USBD_DEV_CFG     *)0) ||                 /* Validate mandatory ptrs.                             */
        (p_drv_api == (USBD_DRV_API     *)0) ||
        (p_drv_cfg == (USBD_DRV_CFG     *)0) ||
        (p_bsp_api == (USBD_DRV_BSP_API *)0)) {
       *p_err = USBD_ERR_NULL_PTR;
        return (USBD_DEV_NBR_NONE);
    }

    if ((p_drv_api->Init        == (void *)0) ||                /* Validate mandatory fnct ptrs. See Note #1.           */
        (p_drv_api->Start       == (void *)0) ||
        (p_drv_api->Stop        == (void *)0) ||
        (p_drv_api->EP_Open     == (void *)0) ||
        (p_drv_api->EP_Close    == (void *)0) ||
        (p_drv_api->EP_RxStart  == (void *)0) ||
        (p_drv_api->EP_Rx       == (void *)0) ||
        (p_drv_api->EP_RxZLP    == (void *)0) ||
        (p_drv_api->EP_Tx       == (void *)0) ||
        (p_drv_api->EP_TxStart  == (void *)0) ||
        (p_drv_api->EP_TxZLP    == (void *)0) ||
        (p_drv_api->EP_Abort    == (void *)0) ||
        (p_drv_api->EP_Stall    == (void *)0) ||
        (p_drv_api->ISR_Handler == (void *)0)) {
       *p_err = USBD_ERR_NULL_PTR;
        return (USBD_DEV_NBR_NONE);
    }
#endif

    CPU_CRITICAL_ENTER();
    dev_nbr = USBD_DevNbrNext;
    if (dev_nbr >= USBD_CFG_MAX_NBR_DEV) {                      /* Chk if dev nbr is valid.                             */
        CPU_CRITICAL_EXIT();
       *p_err =  USBD_ERR_DEV_ALLOC;
        return (USBD_DEV_NBR_NONE);
    }
    USBD_DevNbrNext++;
    CPU_CRITICAL_EXIT();

                                                                /* ------------ INITIALIZE DEVICE STRUCTURE ----------- */
    p_dev                  = &USBD_DevTbl[dev_nbr];
    p_dev->Nbr             = dev_nbr;
    p_dev->Spd             = USBD_DEV_SPD_FULL;                 /* Set dflt speed (FS).                                 */
    p_dev->BusFnctsPtr     = p_bus_fnct;
    p_dev->DevCfgPtr       = p_dev_cfg;
    p_dev->Drv.DevNbr      = dev_nbr;
    p_dev->Drv.API_Ptr     = p_drv_api;
    p_dev->Drv.CfgPtr      = p_drv_cfg;
    p_dev->Drv.BSP_API_Ptr = p_bsp_api;

    ep_alloc_map = DEF_BIT_NONE;


    alloc = USBD_EP_Alloc(p_dev,                                /* Alloc physical EP for ctrl OUT.                       */
                          p_drv_cfg->Spd,
                          USBD_EP_TYPE_CTRL,
                          DEF_NO,
                          0u,
                          0u,
                         &ep_info,
                         &ep_alloc_map);
    if (alloc != DEF_OK) {
       *p_err = USBD_ERR_EP_NONE_AVAIL;
        return (USBD_DEV_NBR_NONE);
    }

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_info.Addr);
    ep_phy_nbr++;

    if (p_dev->EP_MaxPhyNbr < ep_phy_nbr) {
        p_dev->EP_MaxPhyNbr = ep_phy_nbr;
    }

    alloc = USBD_EP_Alloc(p_dev,                                /* Alloc physical EP for ctrl IN.                       */
                          p_drv_cfg->Spd,
                          USBD_EP_TYPE_CTRL,
                          DEF_YES,
                          0u,
                          0u,
                         &ep_info,
                         &ep_alloc_map);
    if (alloc != DEF_OK) {
       *p_err = USBD_ERR_EP_NONE_AVAIL;
        return (USBD_DEV_NBR_NONE);
    }

    p_dev->EP_CtrlMaxPktSize = ep_info.MaxPktSize;

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_info.Addr);
    ep_phy_nbr++;

    if (p_dev->EP_MaxPhyNbr < ep_phy_nbr) {
        p_dev->EP_MaxPhyNbr = ep_phy_nbr;
    }

#if (USBD_CFG_MAX_NBR_STR > 0u)
                                                                /* Add device configuration strings:                    */
    USBD_StrDescAdd(p_dev,                                      /* Manufacturer string.                                 */
                    p_dev_cfg->ManufacturerStrPtr,
                    p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (USBD_DEV_NBR_NONE);
    }
    USBD_StrDescAdd(p_dev,                                      /* Product string.                                      */
                    p_dev_cfg->ProductStrPtr,
                    p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (USBD_DEV_NBR_NONE);
    }
    USBD_StrDescAdd(p_dev,                                      /* Serial number string.                                */
                    p_dev_cfg->SerialNbrStrPtr,
                    p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (USBD_DEV_NBR_NONE);
    }
#endif

   *p_err = USBD_ERR_NONE;
    return (dev_nbr);
}


/*
*********************************************************************************************************
*                                           USBD_DevStart()
*
* Description : Start device stack.
*
* Argument(s) : dev_nbr     Device number.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Device successfully started.
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE      Invalid device state (see Note #1).
*
*                                                               ---- RETURNED BY 'p_drv_api->Init()' : ----
*                                                               See specific device driver(s) 'Init()' for
*                                                                   additional return error codes.
*
*                                                               --- RETURNED BY 'p_drv_api->Start()' : ----
*                                                               See specific device driver(s) 'Start()' for
*                                                                   additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Device stack can be only started if the device is in one of the following states:
*
*                   USBD_DEV_STATE_NONE    Device controller has not been initialized.
*                   USBD_DEV_STATE_INIT    Device controller already      initialized.
*********************************************************************************************************
*/

void  USBD_DevStart (CPU_INT08U   dev_nbr,
                     USBD_ERR    *p_err)
{
    USBD_DEV      *p_dev;
    USBD_DRV      *p_drv;
    USBD_DRV_API  *p_drv_api;
    CPU_BOOLEAN    init;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return;
    }

    if ((p_dev->State != USBD_DEV_STATE_NONE) &&                /* Chk curr dev state.                                  */
        (p_dev->State != USBD_DEV_STATE_INIT)) {
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return;
    }

    p_drv     = &p_dev->Drv;
    p_drv_api =  p_drv->API_Ptr;

    init = DEF_NO;

    if (p_dev->State == USBD_DEV_STATE_NONE) {                  /* If dev not initialized ...                           */
        p_drv_api->Init(p_drv, p_err);                          /* ... call dev drv 'Init()' function.                  */
        if (*p_err != USBD_ERR_NONE) {
            return;
        }

        init = DEF_YES;
    }

    p_drv_api->Start(p_drv, p_err);

    if (init == DEF_YES) {
        CPU_CRITICAL_ENTER();
        p_dev->State = USBD_DEV_STATE_INIT;
        CPU_CRITICAL_EXIT();
    }
}


/*
*********************************************************************************************************
*                                           USBD_DevStop()
*
* Description : Stop device stack.
*
* Argument(s) : dev_nbr     Device number.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Device successfully stopped.
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE      Invalid device state.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_DevStop (CPU_INT08U   dev_nbr,
                    USBD_ERR    *p_err)
{
    USBD_DEV      *p_dev;
    USBD_DRV      *p_drv;
    USBD_DRV_API  *p_drv_api;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return;
    }

    CPU_CRITICAL_ENTER();                                       /* Signal to stop processing events while the driver    */
    p_dev->State = USBD_DEV_STATE_STOPPING;                     /* shuts down.                                          */
    CPU_CRITICAL_EXIT();

    if (p_dev->State == USBD_DEV_STATE_NONE) {
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return;
    }

    USBD_CfgClose(p_dev);                                       /* Close curr cfg.                                      */

    CPU_CRITICAL_ENTER();
    p_dev->State      = USBD_DEV_STATE_INIT;                    /* Re-init dev stack to 'INIT' state.                   */
    p_dev->StatePrev  = USBD_DEV_STATE_INIT;
    p_dev->ConnStatus = DEF_FALSE;
    CPU_CRITICAL_EXIT();

    p_drv     = &p_dev->Drv;
    p_drv_api =  p_drv->API_Ptr;

    p_drv_api->Stop(p_drv);

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            USBD_CfgAdd()
*
* Description : Add a configuration to the device.
*
* Argument(s) : dev_nbr     Device number.
*
*               attrib      Configuration attributes.
*
*                               USBD_DEV_ATTRIB_SELF_POWERED      Power does not come from VBUS.
*                               USBD_DEV_ATTRIB_REMOTE_WAKEUP     Remote wakeup feature enabled.
*
*               max_pwr     Bus power required for this device (see Note #1).
*
*               spd         Configuration speed.
*
*                               USBD_DEV_SPD_FULL   Configuration is added in the full-speed configuration set.
*                               USBD_DEV_SPD_HIGH   Configuration is added in the high-speed configuration set.
*
*               p_name      Pointer to string describing the configuration (see Note #2).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Configuration successfully added.
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE      Invalid device state  (see Note #3).
*                               USBD_ERR_DEV_INVALID_SPD        Speed mismatch in device controller (see Note #4).
*                               USBD_ERR_CFG_INVALID_MAX_PWR    Invalid maximum power (see Note #1).
*                               USBD_ERR_CFG_ALLOC              Configuration cannot be allocated.
*
* Return(s)   : Configuration number, if NO error(s).
*
*               USBD_CFG_NBR_NONE,    otherwise.
*
* Note(s)     : (1) USB spec 2.0, section 7.2.1.3/4 defines power constrains for bus-powered devices:
*
*                   "A low-power function is one that draws up to one unit load from the USB cable when
*                    operational"
*
*                   "A function is defined as being high-power if, when fully powered, it draws over
*                    one but no more than five unit loads from the USB cable."
*
*                    A unit load is defined as 100mA, thus 'max_pwr' argument should be between 0 mA
*                    and 500mA
*
*               (2) String support is optional, in this case 'p_name' can be a NULL string pointer.
*
*               (3) Configuration can ONLY be added when the device is in the following states:
*
*                   USBD_DEV_STATE_NONE    Device controller has not been initialized.
*                   USBD_DEV_STATE_INIT    Device controller already      initialized.
*
*               (4) A high-speed configuration can only be added if the device controller is high-speed.
*********************************************************************************************************
*/

CPU_INT08U  USBD_CfgAdd (       CPU_INT08U     dev_nbr,
                                CPU_INT08U     attrib,
                                CPU_INT16U     max_pwr,
                                USBD_DEV_SPD   spd,
                         const  CPU_CHAR      *p_name,
                                USBD_ERR      *p_err)
{
    USBD_DEV      *p_dev;
    USBD_CFG      *p_cfg;
    CPU_INT08U     cfg_tbl_ix;
    CPU_INT08U     cfg_nbr;
    CPU_SR_ALLOC();


#if (USBD_CFG_HS_EN == DEF_DISABLED)
    (void)spd;
#endif

#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }

    if (max_pwr > USBD_MAX_BUS_PWR_LIMIT_mA) {                  /* Chk max pwr (see Note #1).                           */
       *p_err = USBD_ERR_CFG_INVALID_MAX_PWR;
        return (USBD_CFG_NBR_NONE);
    }
#endif

    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return (USBD_CFG_NBR_NONE);
    }

    if ((p_dev->State != USBD_DEV_STATE_NONE) &&                /* Chk curr dev state.                                  */
        (p_dev->State != USBD_DEV_STATE_INIT)) {
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return (USBD_CFG_NBR_NONE);
    }

#if (USBD_CFG_HS_EN == DEF_ENABLED)
    if ((p_dev->Drv.CfgPtr->Spd != USBD_DEV_SPD_HIGH) &&        /* Chk if dev supports high spd.                        */
        (spd                    == USBD_DEV_SPD_HIGH)) {
       *p_err = USBD_ERR_DEV_INVALID_SPD;
        return (USBD_CFG_NBR_NONE);
    }
#endif

    CPU_CRITICAL_ENTER();
    cfg_tbl_ix = USBD_CfgNbrNext;
    if (cfg_tbl_ix >= USBD_CFG_MAX_NBR_CFG) {                   /* Chk if cfg is avail.                                 */
        CPU_CRITICAL_EXIT();
       *p_err =  USBD_ERR_CFG_ALLOC;
        return (USBD_CFG_NBR_NONE);
    }

#if (USBD_CFG_HS_EN == DEF_ENABLED)
                                                                /* Add cfg to dev ...                                   */
    if (spd == USBD_DEV_SPD_HIGH) {                             /* ... HS cfg.                                          */
        cfg_nbr = p_dev->CfgHS_TotalNbr;
        if ((cfg_nbr >  USBD_CFG_NBR_TOT) ||                    /* Chk cfg limit.                                       */
            (cfg_nbr >= USBD_CFG_MAX_NBR_CFG)) {
            CPU_CRITICAL_EXIT();
           *p_err = USBD_ERR_CFG_ALLOC;
            return (USBD_CFG_NBR_NONE);
        }
        p_dev->CfgHS_TotalNbr++;
    } else {
#endif
                                                                /* ... FS cfg.                                          */
        cfg_nbr = p_dev->CfgFS_TotalNbr;
        if ((cfg_nbr >  USBD_CFG_NBR_TOT) ||                    /* Chk cfg limit.                                       */
            (cfg_nbr >= USBD_CFG_MAX_NBR_CFG)) {
            CPU_CRITICAL_EXIT();
           *p_err = USBD_ERR_CFG_ALLOC;
            return (USBD_CFG_NBR_NONE);
        }
        p_dev->CfgFS_TotalNbr++;
#if (USBD_CFG_HS_EN == DEF_ENABLED)
    }
#endif
    USBD_CfgNbrNext++;

                                                                /* ------ CONFIGURATION STRUCTURE INITIALIZATION ------ */
    p_cfg = &USBD_CfgTbl[cfg_tbl_ix];
                                                                /* Link cfg into dev struct.                            */
#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)
    CPU_CRITICAL_EXIT();

#if (USBD_CFG_HS_EN == DEF_ENABLED)
    if (spd == USBD_DEV_SPD_HIGH) {
        p_dev->CfgHS_SpdTblPtrs[cfg_nbr] = p_cfg;
        DEF_BIT_SET(cfg_nbr, USBD_CFG_NBR_SPD_BIT);             /* Set spd bit in cfg nbr.                              */
    } else {
#endif
        p_dev->CfgFS_SpdTblPtrs[cfg_nbr] = p_cfg;
#if (USBD_CFG_HS_EN == DEF_ENABLED)
    }
#endif


#else
    p_cfg->NextPtr = (USBD_CFG *)0;

#if (USBD_CFG_HS_EN == DEF_ENABLED)
    if (spd == USBD_DEV_SPD_HIGH) {
        if (p_dev->CfgHS_HeadPtr == (USBD_CFG *)0) {            /* Link cfg in HS list.                                 */
            p_dev->CfgHS_HeadPtr  =  p_cfg;
            p_dev->CfgHS_TailPtr  =  p_cfg;
        } else {
            p_dev->CfgHS_TailPtr->NextPtr = p_cfg;
            p_dev->CfgHS_TailPtr          = p_cfg;
        }
        DEF_BIT_SET(cfg_nbr, USBD_CFG_NBR_SPD_BIT);             /* Set spd bit in cfg nbr.                              */
    } else {
#endif
        if (p_dev->CfgFS_HeadPtr == (USBD_CFG *)0) {            /* Link cfg in FS list.                                 */
            p_dev->CfgFS_HeadPtr  =  p_cfg;
            p_dev->CfgFS_TailPtr  =  p_cfg;
        } else {
            p_dev->CfgFS_TailPtr->NextPtr = p_cfg;
            p_dev->CfgFS_TailPtr          = p_cfg;
        }
#if (USBD_CFG_HS_EN == DEF_ENABLED)
    }
#endif
    CPU_CRITICAL_EXIT();
#endif

    p_cfg->Attrib      = attrib;
    p_cfg->NamePtr     = p_name;
    p_cfg->EP_AllocMap = USBD_EP_CTRL_ALLOC;                    /* Init EP alloc bitmap.                                */
    p_cfg->MaxPwr      = max_pwr;
    p_cfg->DescLen     = 0u;                                    /* Init cfg desc len.                                   */

#if (USBD_CFG_MAX_NBR_STR > 0u)
    USBD_StrDescAdd(p_dev, p_name, p_err);                      /* Add cfg string to dev.                               */
    if (*p_err != USBD_ERR_NONE) {
        return (USBD_CFG_NBR_NONE);
    }
#else
    (void)p_name;
#endif

   *p_err = USBD_ERR_NONE;
    return (cfg_nbr);
}


/*
*********************************************************************************************************
*                                        USBD_CfgOtherSpeed()
*
* Description : Associate a configuration with its other-speed counterpart.
*
* Argument(s) : dev_nbr     Device number.
*
*               cfg_nbr     Configuration number.
*
*               cfg_other   Other-speed configuration number.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Configuration successfully associated.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE  Invalid device state (see Note #2).
*                               USBD_ERR_CFG_INVALID_NBR    Invalid configuration number.
*
* Return(s)   : none.
*
* Note(s)     : (1) Configurations from high- and full-speed can be associated with each other to provide
*                   comparable functionality regardless of speed.
*
*               (2) Configuration can ONLY be associated when the device is in the following states:
*
*                   USBD_DEV_STATE_NONE    Device controller has not been initialized.
*                   USBD_DEV_STATE_INIT    Device controller already      initialized.
*********************************************************************************************************
*/

#if (USBD_CFG_HS_EN == DEF_ENABLED)
void  USBD_CfgOtherSpeed (CPU_INT08U   dev_nbr,
                          CPU_INT08U   cfg_nbr,
                          CPU_INT08U   cfg_other,
                          USBD_ERR    *p_err)
{
    USBD_DEV  *p_dev;
    USBD_CFG  *p_cfg;
    USBD_CFG  *p_cfg_other;


                                                                /* ---------------- VALIDATE ARGUMENTS ---------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return;
    }

    if ((p_dev->State != USBD_DEV_STATE_NONE) &&                /* Chk curr dev state.                                  */
        (p_dev->State != USBD_DEV_STATE_INIT)) {
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return;
    }

    if (((cfg_nbr   & USBD_CFG_NBR_SPD_BIT)  ^                  /* Chk if both cfg are from same spd.                   */
         (cfg_other & USBD_CFG_NBR_SPD_BIT)) != USBD_CFG_NBR_SPD_BIT) {
       *p_err = USBD_ERR_CFG_INVALID_NBR;
        return;
    }

    p_cfg       = USBD_CfgRefGet(p_dev, cfg_nbr);
    p_cfg_other = USBD_CfgRefGet(p_dev, cfg_other);
    if ((p_cfg       == (USBD_CFG *)0) ||
        (p_cfg_other == (USBD_CFG *)0)) {
       *p_err = USBD_ERR_CFG_INVALID_NBR;
        return;
    }

    if ((p_cfg->CfgOtherSpd       != USBD_CFG_NBR_NONE) ||      /* Chk if cfg already associated.                       */
        (p_cfg_other->CfgOtherSpd != USBD_CFG_NBR_NONE)) {
       *p_err = USBD_ERR_CFG_INVALID_NBR;
        return;
    }

    p_cfg->CfgOtherSpd       = cfg_other;
    p_cfg_other->CfgOtherSpd = cfg_nbr;

   *p_err = USBD_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                         USBD_DevStateGet()
*
* Description : Get current device state.
*
* Argument(s) : dev_nbr     Device number.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Device state successfully retrieved.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*
* Return(s)   : Current device state, if NO error(s).
*
*               USBD_DEV_STATE_NONE,  otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

USBD_DEV_STATE  USBD_DevStateGet (CPU_INT08U   dev_nbr,
                                  USBD_ERR    *p_err)
{
    USBD_DEV  *p_dev;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return (USBD_DEV_STATE_NONE);
    }

   *p_err = USBD_ERR_NONE;
    return (p_dev->State);
}


/*
*********************************************************************************************************
*                                          USBD_DevSpdGet()
*
* Description : Get device speed.
*
* Argument(s) : dev_nbr     Device number.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Frame number successfully retrieved.
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE      Invalid device state.
*
* Return(s)   : The current device speed, if successful.
*               USBD_DEV_SPD_INVALID,     otherwise.
*
* Note(s)     : None.
*********************************************************************************************************
*/

USBD_DEV_SPD  USBD_DevSpdGet (CPU_INT08U   dev_nbr,
                              USBD_ERR    *p_err)
{
    USBD_DEV  *p_dev;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return (USBD_DEV_SPD_INVALID);
    }

    if (p_dev->State == USBD_DEV_STATE_NONE) {
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return (USBD_DEV_SPD_INVALID);
    }

   *p_err = USBD_ERR_NONE;

    return (p_dev->Spd);
}


/*
*********************************************************************************************************
*                                         USBD_DevSelfPwrSet()
*
* Description : Set device current power source.
*
* Argument(s) : dev_nbr     Device number.
*
*               self_pwr    The power source of the device :
*
*                           DEF_TRUE  device is self-powered.
*                           DEF_FALSE device is  bus-powered.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Device power source correctly set.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_DevSelfPwrSet (CPU_INT08U    dev_nbr,
                          CPU_BOOLEAN   self_pwr,
                          USBD_ERR     *p_err)
{
    USBD_DEV  *p_dev;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return;
    }

   *p_err = USBD_ERR_NONE;
    CPU_CRITICAL_ENTER();
    p_dev->SelfPwr = self_pwr;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                     USBD_DevSetMS_VendorCode()
*
* Description : Set device Microsoft vendor code.
*
* Argument(s) : dev_nbr         Device number.
*
*               vendor_code     Microsoft vendor code.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Microsoft vendor code correctly set.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*
* Return(s)   : none.
*
* Note(s)     : (1) The vendor code used MUST be different from any vendor bRequest value.
*********************************************************************************************************
*/
#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
void  USBD_DevSetMS_VendorCode (CPU_INT08U   dev_nbr,
                                CPU_INT08U   vendor_code,
                                USBD_ERR    *p_err)
{
    USBD_DEV  *p_dev;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return;
    }

    CPU_CRITICAL_ENTER();
    p_dev->StrMS_VendorCode = vendor_code;
    CPU_CRITICAL_EXIT();

   *p_err = USBD_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                          USBD_DevGetCfg()
*
* Description : Get device configuration.
*
* Argument(s) : dev_nbr     Device number.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Device state successfully retrieved.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*
* Return(s)   : Pointer to device configuration, if NO error(s).
*
*               Pointer to NULL,                 otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

USBD_DEV_CFG  *USBD_DevCfgGet (CPU_INT08U   dev_nbr,
                               USBD_ERR    *p_err)
{
    USBD_DEV  *p_dev;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return ((USBD_DEV_CFG *)0);
    }
   *p_err = USBD_ERR_NONE;
    return (p_dev->DevCfgPtr);
}


/*
*********************************************************************************************************
*                                       USBD_DevFrameNbrGet()
*
* Description : Get the last frame number from the driver.
*
* Argument(s) : dev_nbr     Device number.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Frame number successfully retrieved.
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE      Invalid device state.
*                               USBD_ERR_DEV_UNAVAIL_FEAT       Driver does not support this feature.
*
* Return(s)   : The current frame number.
*
* Note(s)     : (1) The frame number will always be in the range 0-2047 (11 bits).
*
*               (2) Frame number returned to the caller contains the frame and microframe numbers. It is
*                   encoded following this 16-bit format:
*
*                   | 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
*                   |  0    0 |  microframe  |                  frame                     |
*
*                   Caller must use the macros USBD_FRAME_NBR_GET() or USBD_MICROFRAME_NBR_GET() to get
*                   the frame or microframe number only.
*********************************************************************************************************
*/

CPU_INT16U  USBD_DevFrameNbrGet (CPU_INT08U   dev_nbr,
                                 USBD_ERR    *p_err)
{
    USBD_DEV      *p_dev;
    USBD_DRV      *p_drv;
    USBD_DRV_API  *p_drv_api;
    CPU_INT16U     frame_nbr;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0u);
    }
#endif

    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return (0u);
    }

    if (p_dev->State == USBD_DEV_STATE_NONE) {
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return (0u);
    }

    p_drv     = &p_dev->Drv;
    p_drv_api =  p_drv->API_Ptr;
    if (p_drv_api->FrameNbrGet == (void*)0) {
       *p_err = USBD_ERR_DEV_UNAVAIL_FEAT;
        return (0u);
    }

    frame_nbr = p_drv_api->FrameNbrGet(p_drv);
   *p_err = USBD_ERR_NONE;

    return (frame_nbr);                                         /* See Note #2.                                         */
}


/*
*********************************************************************************************************
*                                            USBD_IF_Add()
*
* Description : Add an interface to a specific configuration.
*
* Argument(s) : dev_nbr             Device number.
*
*               cfg_nbr             Configuration index to add the interface.
*
*               p_class_drv         Pointer to interface driver.
*
*               p_if_class_arg      Pointer to interface driver argument.
*
*               p_if_alt_class_arg  Pointer to alternate interface argument.
*
*               class_code          Class code assigned by the USB-IF.
*
*               class_sub_code      Subclass code assigned by the USB-IF.
*
*               class_protocol_code Protocol code assigned by the USB-IF.
*
*               p_name              Pointer to string describing the Interface.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Interface successfully added.
*                               USBD_ERR_NULL_PTR               Argument 'p_class_drv'/'p_class_drv->Conn'/
*                                                                   'p_class_drv->Disconn'/'p_class_drv->IF_Desc'/
*                                                                   'p_class_drv->IF_DescSizeGet'/'p_class_drv->EP_Desc'/
*                                                                   'p_class_drv->EP_DescSizeGet' passed a NULL
*                                                               pointer.
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE      Invalid device state.
*                               USBD_ERR_CFG_INVALID_NBR        Invalid configuration number.
*                               USBD_ERR_IF_ALLOC               Interfaces                   NOT available.
*                               USBD_ERR_IF_ALT_ALLOC           Interface alternate settings NOT available.
*
* Return(s)   : Interface number, if NO error(s).
*
*               USBD_IF_NBR_NONE, otherwise.
*
* Note(s)     : (1) USB Spec 2.0 section 9.6.5 Interface states: "An interface may include alternate
*                   settings that allow the endpoints and/or their characteristics to be varied after
*                   the device has been configured. The default setting for an interface is always
*                   alternate setting zero."
*********************************************************************************************************
*/

CPU_INT08U  USBD_IF_Add (       CPU_INT08U        dev_nbr,
                                CPU_INT08U        cfg_nbr,
                                USBD_CLASS_DRV   *p_class_drv,
                                void             *p_if_class_arg,
                                void             *p_if_alt_class_arg,
                                CPU_INT08U        class_code,
                                CPU_INT08U        class_sub_code,
                                CPU_INT08U        class_protocol_code,
                         const  CPU_CHAR         *p_name,
                                USBD_ERR         *p_err)
{
    CPU_INT08U    if_tbl_ix;
    CPU_INT08U    if_nbr;
    CPU_INT08U    if_alt_nbr;
    USBD_DEV     *p_dev;
    USBD_CFG     *p_cfg;
    USBD_IF      *p_if;
    USBD_IF_ALT  *p_if_alt;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }

    if (p_class_drv == (USBD_CLASS_DRV *)0) {
       *p_err = USBD_ERR_NULL_PTR;
        return (USBD_IF_NBR_NONE);
    }

    if (((p_class_drv->IF_Desc        == (void *)0)  &&         /* Chk if IF_Desc() & IF_DescSizeGet() are either ...   */
         (p_class_drv->IF_DescSizeGet != (void *)0)) ||         /* ... present or not.                                  */
        ((p_class_drv->IF_Desc        != (void *)0)  &&
         (p_class_drv->IF_DescSizeGet == (void *)0))) {
       *p_err = USBD_ERR_NULL_PTR;
        return (USBD_IF_NBR_NONE);
    }

    if (((p_class_drv->EP_Desc        == (void *)0)  &&         /* Chk if EP_Desc() & EP_DescSizeGet() are either ...   */
         (p_class_drv->EP_DescSizeGet != (void *)0)) ||         /* ... present or not.                                  */
        ((p_class_drv->EP_Desc        != (void *)0)  &&
         (p_class_drv->EP_DescSizeGet == (void *)0))) {
       *p_err = USBD_ERR_NULL_PTR;
        return (USBD_IF_NBR_NONE);
    }
#endif

    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return (USBD_IF_NBR_NONE);
    }

    if ((p_dev->State != USBD_DEV_STATE_NONE) &&                /* Chk curr dev state.                                  */
        (p_dev->State != USBD_DEV_STATE_INIT)) {
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return (USBD_IF_NBR_NONE);
    }

    p_cfg = USBD_CfgRefGet(p_dev, cfg_nbr);                     /* Get cfg struct.                                      */
    if (p_cfg == (USBD_CFG *)0) {
       *p_err = USBD_ERR_CFG_INVALID_NBR;
        return (USBD_IF_NBR_NONE);
    }

    CPU_CRITICAL_ENTER();
    if_tbl_ix = USBD_IF_NbrNext;
    if (if_tbl_ix >= USBD_CFG_MAX_NBR_IF) {                     /* Chk if IF struct is avail.                           */
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_IF_ALLOC;
        return (USBD_IF_NBR_NONE);
    }

    if_alt_nbr = USBD_IF_AltNbrNext;
    if (if_alt_nbr >= USBD_CFG_MAX_NBR_IF_ALT) {                /* Chk if IF alt struct is avail.                       */
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_IF_ALT_ALLOC;
        return (USBD_IF_NBR_NONE);
    }

    if_nbr = p_cfg->IF_NbrTotal;                                /* Get next IF nbr in cfg.                              */
    if ((if_nbr >  USBD_IF_NBR_TOT) ||                          /* Chk IF limit.                                        */
        (if_nbr >= USBD_CFG_MAX_NBR_IF_ALT)) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_IF_ALLOC;
        return (USBD_IF_NBR_NONE);
    }

    USBD_IF_NbrNext++;
    USBD_IF_AltNbrNext++;
    p_cfg->IF_NbrTotal++;

    p_if     = &USBD_IF_Tbl[if_tbl_ix];
    p_if_alt = &USBD_IF_AltTbl[if_alt_nbr];                     /* Get IF alt struct (see Note #1).                     */

#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)                      /* Link IF and alt setting.                             */
    CPU_CRITICAL_EXIT();

    p_cfg->IF_TblPtrs[if_nbr] = p_if;
    p_if->AltTblPtrs[0u]      = p_if_alt;
#else
    p_if->NextPtr     = (USBD_IF     *)0;
    p_if_alt->NextPtr = (USBD_IF_ALT *)0;
    p_if->AltHeadPtr  =  p_if_alt;
    p_if->AltTailPtr  =  p_if_alt;

    if (p_cfg->IF_HeadPtr == (USBD_IF *)0) {
        p_cfg->IF_HeadPtr = p_if;
        p_cfg->IF_TailPtr = p_if;
    } else {
        p_cfg->IF_TailPtr->NextPtr = p_if;
        p_cfg->IF_TailPtr          = p_if;
    }
    CPU_CRITICAL_EXIT();
#endif

    p_if->ClassCode         = class_code;
    p_if->ClassSubCode      = class_sub_code;
    p_if->ClassProtocolCode = class_protocol_code;
    p_if->ClassDrvPtr       = p_class_drv;
    p_if->ClassArgPtr       = p_if_class_arg;
    p_if->EP_AllocMap       = USBD_EP_CTRL_ALLOC;
    p_if->AltCurPtr         = p_if_alt;                         /* Set curr alt setting.                                */
    p_if->AltCur            = 0u;
    p_if->AltNbrTotal       = 1u;
    p_if_alt->NamePtr       = p_name;
    p_if_alt->EP_AllocMap   = p_if->EP_AllocMap;
    p_if_alt->ClassArgPtr   = p_if_alt_class_arg;

#if (USBD_CFG_MAX_NBR_STR > 0u)
    USBD_StrDescAdd(p_dev, p_name, p_err);                      /* Add IF string to dev.                                */
    if (*p_err != USBD_ERR_NONE) {
        return (USBD_IF_NBR_NONE);
    }
#else
    (void)p_name;
#endif

   *p_err = USBD_ERR_NONE;
    return (if_nbr);
}


/*
*********************************************************************************************************
*                                          USBD_IF_AltAdd()
*
* Description : Add an alternate setting to a specific interface.
*
* Argument(s) : dev_nbr         Device number.
*
*               cfg_nbr         Configuration number.
*
*               if_nbr          Interface number.
*
*               p_class_arg     Pointer to alternate interface argument.
*
*               p_name          Pointer to alternate setting name.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Interface alternate setting successfully added.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device        number.
*                               USBD_ERR_CFG_INVALID_NBR    Invalid configuration number
*                               USBD_ERR_IF_INVALID_NBR     Invalid interface     number.
*                               USBD_ERR_IF_ALT_ALLOC       Interface alternate settings NOT available.
*
* Return(s)   : Interface alternate setting number, if NO error(s).
*
*               USBD_IF_ALT_NBR_NONE,               otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  USBD_IF_AltAdd (       CPU_INT08U   dev_nbr,
                                   CPU_INT08U   cfg_nbr,
                                   CPU_INT08U   if_nbr,
                                   void        *p_class_arg,
                            const  CPU_CHAR    *p_name,
                                   USBD_ERR    *p_err)
{
    USBD_DEV      *p_dev;
    USBD_CFG      *p_cfg;
    USBD_IF       *p_if;
    USBD_IF_ALT   *p_if_alt;
    CPU_INT08U     if_alt_tbl_ix;
    CPU_INT08U     if_alt_nbr;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif
                                                                /* --------------- GET OBJECT REFERENCES -------------- */
    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return (USBD_IF_ALT_NBR_NONE);
    }

    p_cfg = USBD_CfgRefGet(p_dev, cfg_nbr);                     /* Get cfg struct.                                      */
    if (p_cfg == (USBD_CFG *)0) {
       *p_err = USBD_ERR_CFG_INVALID_NBR;
        return (USBD_IF_ALT_NBR_NONE);
    }

    p_if = USBD_IF_RefGet(p_cfg, if_nbr);                       /* Get IF struct.                                       */
    if (p_if == (USBD_IF *)0) {
       *p_err = USBD_ERR_IF_INVALID_NBR;
        return (USBD_IF_ALT_NBR_NONE);
    }

    CPU_CRITICAL_ENTER();
    if_alt_tbl_ix = USBD_IF_AltNbrNext;
    if (if_alt_tbl_ix >= USBD_CFG_MAX_NBR_IF_ALT) {             /* Chk if next alt setting is avail.                    */
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_IF_ALT_ALLOC;
        return (USBD_IF_ALT_NBR_NONE);
    }

    if_alt_nbr = p_if->AltNbrTotal;
    if ((if_alt_nbr > USBD_IF_ALT_NBR_TOT) ||                   /* Chk if alt setting is avail.                         */
        (if_alt_nbr >= USBD_CFG_MAX_NBR_IF_ALT)) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_IF_ALT_ALLOC;
        return (USBD_IF_ALT_NBR_NONE);
    }

    USBD_IF_AltNbrNext++;
    p_if->AltNbrTotal++;

    p_if_alt = &USBD_IF_AltTbl[if_alt_tbl_ix];

#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)                      /* Add alt setting to IF.                               */
    CPU_CRITICAL_EXIT();

    p_if->AltTblPtrs[if_alt_nbr] = p_if_alt;
#else
    p_if_alt->NextPtr         = (USBD_IF_ALT *)0;

    p_if->AltTailPtr->NextPtr = p_if_alt;
    p_if->AltTailPtr          = p_if_alt;
    CPU_CRITICAL_EXIT();
#endif


    p_if_alt->ClassArgPtr = p_class_arg;
    p_if_alt->EP_AllocMap = USBD_EP_CTRL_ALLOC;
    p_if_alt->NamePtr     = p_name;

    DEF_BIT_CLR(p_if_alt->EP_AllocMap, p_if->EP_AllocMap);
    DEF_BIT_SET(p_if_alt->EP_AllocMap, USBD_EP_CTRL_ALLOC);

#if (USBD_CFG_MAX_NBR_STR > 0u)
    USBD_StrDescAdd(p_dev, p_name, p_err);                      /* Add alt setting string to dev.                       */
    if (*p_err != USBD_ERR_NONE) {
        return (USBD_IF_ALT_NBR_NONE);
    }
#else
    (void)p_name;
#endif

   *p_err = USBD_ERR_NONE;
    return (if_alt_nbr);
}


/*
*********************************************************************************************************
*                                            USBD_IF_Grp()
*
* Description : Create a interface group.
*
* Argument(s) : dev_nbr             Device number
*
*               cfg_nbr             Configuration number.
*
*               class_code          Class code assigned by the USB-IF.
*
*               class_sub_code      Subclass code assigned by the USB-IF.
*
*               class_protocol_code Protocol code assigned by the USB-IF.
*
*               if_start            Interface number of the first interface that is associated with this group.
*
*               if_cnt              Number of consecutive interfaces that are associated with this group.
*
*               p_name              Pointer to string describing interface group.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Interface group successfully added.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device        number.
*                               USBD_ERR_CFG_INVALID_NBR    Invalid configuration number.
*                               USBD_ERR_IF_INVALID_NBR     Invalid interface     number.
*                               USBD_ERR_IF_GRP_NBR_IN_USE  Interface is part of another group.
*                               USBD_ERR_IF_GRP_ALLOC       Interface groups NOT available.
*
* Return(s)   : Interface group number, if NO error(s).
*
*               USBD_IF_GRP_NBR_NONE,   otherwise.
*
* Note(s)     : None.
*********************************************************************************************************
*/

#if (USBD_CFG_MAX_NBR_IF_GRP > 0)
CPU_INT08U  USBD_IF_Grp (       CPU_INT08U   dev_nbr,
                                CPU_INT08U   cfg_nbr,
                                CPU_INT08U   class_code,
                                CPU_INT08U   class_sub_code,
                                CPU_INT08U   class_protocol_code,
                                CPU_INT08U   if_start,
                                CPU_INT08U   if_cnt,
                         const  CPU_CHAR    *p_name,
                                USBD_ERR    *p_err)
{
    USBD_DEV     *p_dev;
    USBD_CFG     *p_cfg;
    USBD_IF      *p_if;
    USBD_IF_GRP  *p_if_grp;
    CPU_INT08U    if_grp_tbl_ix;
    CPU_INT08U    if_grp_nbr;
    CPU_INT08U    if_nbr;
    CPU_INT08U    if_end;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }
#endif

    if (((CPU_INT16U)(if_start) + (CPU_INT16U)(if_cnt)) > (CPU_INT16U)USBD_IF_NBR_TOT) {
       *p_err = USBD_ERR_IF_INVALID_NBR;
        return (USBD_IF_GRP_NBR_NONE);
    }

                                                                /* --------------- GET OBJECT REFERENCES -------------- */
    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return (USBD_IF_GRP_NBR_NONE);
    }

    p_cfg = USBD_CfgRefGet(p_dev, cfg_nbr);                     /* Get cfg struct.                                      */
    if (p_cfg == (USBD_CFG *)0) {
       *p_err = USBD_ERR_CFG_INVALID_NBR;
        return (USBD_IF_GRP_NBR_NONE);
    }
                                                                /* Verify that IFs do NOT belong to another group.      */
    for (if_nbr = 0u; if_nbr < if_cnt; if_nbr++) {
        p_if = USBD_IF_RefGet(p_cfg, if_nbr + if_start);
        if (p_if == (USBD_IF *)0) {
           *p_err = USBD_ERR_IF_INVALID_NBR;
            return (USBD_IF_GRP_NBR_NONE);
        }

        if (p_if->GrpNbr != USBD_IF_GRP_NBR_NONE) {
           *p_err = USBD_ERR_IF_GRP_NBR_IN_USE;
            return (USBD_IF_GRP_NBR_NONE);
        }
    }

    CPU_CRITICAL_ENTER();
    if_grp_tbl_ix = USBD_IF_GrpNbrNext;
    if (if_grp_tbl_ix >= USBD_CFG_MAX_NBR_IF_GRP) {             /* Chk if IF grp is avail.                              */
        CPU_CRITICAL_EXIT();
       *p_err =  USBD_ERR_IF_GRP_ALLOC;
        return (USBD_IF_GRP_NBR_NONE);
    }
    USBD_IF_GrpNbrNext++;

    p_if_grp = &USBD_IF_GrpTbl[if_grp_tbl_ix];

#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)
    if_grp_nbr = p_cfg->IF_GrpNbrTotal;
    p_cfg->IF_GrpNbrTotal++;
    CPU_CRITICAL_EXIT();

    p_cfg->IF_GrpTblPtrs[if_grp_nbr] = p_if_grp;
#else
    p_if_grp->NextPtr = (USBD_IF_GRP *)0;

    if_grp_nbr = p_cfg->IF_GrpNbrTotal;
    p_cfg->IF_GrpNbrTotal++;

    if (if_grp_nbr == 0u) {
        p_cfg->IF_GrpHeadPtr = p_if_grp;
        p_cfg->IF_GrpTailPtr = p_if_grp;
    } else {
        p_cfg->IF_GrpTailPtr->NextPtr = p_if_grp;
        p_cfg->IF_GrpTailPtr          = p_if_grp;
    }
    CPU_CRITICAL_EXIT();
#endif

    p_if_grp->ClassCode         =  class_code;
    p_if_grp->ClassSubCode      =  class_sub_code;
    p_if_grp->ClassProtocolCode =  class_protocol_code;
    p_if_grp->IF_Start          =  if_start;
    p_if_grp->IF_Cnt            =  if_cnt;
    p_if_grp->NamePtr           =  p_name;

    if_end = if_cnt + if_start;
    for (if_nbr = if_start; if_nbr < if_end; if_nbr++) {
        p_if = USBD_IF_RefGet(p_cfg, if_nbr);
        if (p_if == (USBD_IF *)0) {
           *p_err = USBD_ERR_IF_INVALID_NBR;
            return (USBD_IF_GRP_NBR_NONE);
        }

        if (p_if->GrpNbr != USBD_IF_GRP_NBR_NONE) {
           *p_err = USBD_ERR_IF_GRP_NBR_IN_USE;
            return (USBD_IF_GRP_NBR_NONE);
        }

        CPU_CRITICAL_ENTER();
        p_if->GrpNbr = if_grp_nbr;
        CPU_CRITICAL_EXIT();
    }

#if (USBD_CFG_MAX_NBR_STR > 0u)
    USBD_StrDescAdd(p_dev, p_name, p_err);                      /* Add IF grp string to dev.                            */
    if (*p_err != USBD_ERR_NONE) {
        return (USBD_IF_GRP_NBR_NONE);
    }
#else
    (void)p_name;
#endif

   *p_err = USBD_ERR_NONE;
    return (if_grp_nbr);
}
#endif


/*
*********************************************************************************************************
*                                           USBD_DescDevGet()
*
* Description : Get the device descriptor.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               p_buf       Pointer to the destination buffer.
*
*               max_len     Maximum number of bytes to write in destination buffer.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Device descriptor successfully obtained.
*                               USBD_ERR_INVALID_ARG        Invalid argument(s) passed to 'p_drv'/'p_buf'/
*                                                               'max_len'.
*                               USBD_ERR_NULL_PTR
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number obtained from p_drv.
*                               USBD_ERR_DEV_INVALID_STATE  State of the device is not USBD_DEV_STATE_NONE.
*                               USBD_ERR_ALLOC              From 'DescBufErrPtr', in USBD_DescWrReq(), if
*                                                           function was called from a driver's context.
*
*                               - RETURNED BY USBD_DevDescSend() -
*                               See USBD_DevDescSend() for additional return error codes.
*
* Return(s)   : Number of bytes actually in the descriptor, if NO error(s).
*
*               0,                                          otherwise.
*
* Note(s)     : (1) This function should be used by drivers supporting standard requests auto-reply,
*                   during the initialization process.
*********************************************************************************************************
*/

CPU_INT08U  USBD_DescDevGet (USBD_DRV    *p_drv,
                             CPU_INT08U  *p_buf,
                             CPU_INT08U   max_len,
                             USBD_ERR    *p_err)
{
    USBD_DEV    *p_dev;
    CPU_INT08U   desc_len;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0u);
    }

    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_NULL_PTR;
        return (0u);
    }

    if (p_buf == (CPU_INT08U *)0) {
       *p_err = USBD_ERR_NULL_PTR;
        return (0u);
    }

    if (max_len == 0u) {
       *p_err = USBD_ERR_INVALID_ARG;
        return (0u);
    }
#endif

    p_dev = USBD_DevRefGet(p_drv->DevNbr);                      /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return (0u);
    }

    if (p_dev->State != USBD_DEV_STATE_NONE) {
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return (0u);
    }

    p_dev->ActualBufPtr  = p_buf;
    p_dev->DescBufMaxLen = max_len;
    p_dev->DescBufErrPtr = p_err;

    USBD_DevDescSend(p_dev,
                     DEF_NO,
                     max_len,
                     p_err);

    desc_len             =  p_dev->DescBufIx;
    p_dev->DescBufErrPtr = (USBD_ERR *)0;

    return (desc_len);
}


/*
*********************************************************************************************************
*                                           USBD_DescCfgGet()
*
* Description : Get a configuration descriptor.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               p_buf       Pointer to the destination buffer.
*
*               max_len     Maximum number of bytes to write in destination buffer.
*
*               cfg_ix      Index of the desired configuration descriptor.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Configuration descriptor successfully obtained.
*                               USBD_ERR_NULL_PTR           Null pointer passed to 'p_drv'/'p_buf'.
*                               USBD_ERR_INVALID_ARG        Invalid argument(s) passed to 'max_len'.
*                               USBD_ERR_DEV_INVALID_STATE  State of the device is not USBD_DEV_STATE_NONE.
*                               USBD_ERR_ALLOC              From 'DescBufErrPtr', in USBD_DescWrReq(), if
*                                                           function was called from a driver's context.
*
*                               - RETURNED BY USBD_CfgDescSend() -
*                               See USBD_CfgDescSend() for additional return error codes.
*
*
* Return(s)   : Number of bytes actually in the descriptor, if NO error(s).
*
*               0,                                          otherwise.
*
* Note(s)     : (1) This function should be used by drivers supporting standard requests auto-reply,
*                   during the initialization process.
*********************************************************************************************************
*/

CPU_INT16U  USBD_DescCfgGet (USBD_DRV    *p_drv,
                             CPU_INT08U  *p_buf,
                             CPU_INT16U   max_len,
                             CPU_INT08U   cfg_ix,
                             USBD_ERR    *p_err)
{
    USBD_DEV    *p_dev;
    CPU_INT08U   desc_len;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0u);
    }

    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_NULL_PTR;
        return (0u);
    }

    if (p_buf == (CPU_INT08U *)0) {
       *p_err = USBD_ERR_NULL_PTR;
        return (0u);
    }

    if (max_len == 0u) {
       *p_err = USBD_ERR_INVALID_ARG;
        return (0u);
    }
#endif

    p_dev = USBD_DevRefGet(p_drv->DevNbr);                      /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
        return (0u);
    }

    if (p_dev->State != USBD_DEV_STATE_NONE) {
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return (0u);
    }

    p_dev->ActualBufPtr  = p_buf;
    p_dev->DescBufMaxLen = max_len;
    p_dev->DescBufErrPtr = p_err;

    USBD_CfgDescSend(p_dev,
                     cfg_ix,
                     DEF_NO,
                     max_len,
                     p_err);

    desc_len             =  p_dev->DescBufIx;
    p_dev->DescBufErrPtr = (USBD_ERR *)0;

    return (desc_len);
}


/*
*********************************************************************************************************
*                                           USBD_DescStrGet()
*
* Description : Get a string descriptor.
*
* Argument(s) : p_drv       Pointer to device driver structure.
*
*               p_buf       Pointer to the destination buffer.
*
*               max_len     Maximum number of bytes to write in destination buffer.
*
*               str_ix      Index of the desired string descriptor.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               String descriptor successfully obtained.
*                               USBD_ERR_NULL_PTR           Null pointer passed to 'p_drv'/'p_buf'.
*                               USBD_ERR_INVALID_ARG        Invalid argument(s) passed to 'max_len'.
*                               USBD_ERR_DEV_INVALID_STATE  State of the device is not USBD_DEV_STATE_NONE.
*                               USBD_ERR_ALLOC              From 'DescBufErrPtr', in USBD_DescWrReq(), if
*                                                           function was called from a driver's context.
*
*                               - RETURNED BY USBD_StrDescSend() -
*                               See USBD_StrDescSend() for additional return error codes.
*
* Return(s)   : Number of bytes actually in the descriptor, if NO error(s).
*
*               0,                                          otherwise.
*
* Note(s)     : (1) This function should be used by drivers supporting standard requests auto-reply,
*                   during the initialization process.
*********************************************************************************************************
*/

CPU_INT08U  USBD_DescStrGet (USBD_DRV    *p_drv,
                             CPU_INT08U  *p_buf,
                             CPU_INT08U   max_len,
                             CPU_INT08U   str_ix,
                             USBD_ERR    *p_err)
{
    USBD_DEV    *p_dev;
    CPU_INT08U   desc_len;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0u);
    }

    if (p_drv == (USBD_DRV *)0) {
       *p_err = USBD_ERR_NULL_PTR;
        return (0u);
    }

    if (p_buf == (CPU_INT08U *)0) {
       *p_err = USBD_ERR_NULL_PTR;
        return (0u);
    }

    if (max_len == 0u) {
       *p_err = USBD_ERR_INVALID_ARG;
        return (0u);
    }
#endif

    p_dev = USBD_DevRefGet(p_drv->DevNbr);                      /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
        return (0u);
    }

    if (p_dev->State != USBD_DEV_STATE_NONE) {
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return (0u);
    }

    p_dev->ActualBufPtr  = p_buf;
    p_dev->DescBufMaxLen = max_len;
    p_dev->DescBufErrPtr = p_err;

    USBD_StrDescSend(p_dev,
                     str_ix,
                     max_len,
                     p_err);

    desc_len             =  p_dev->DescBufIx;
    p_dev->DescBufErrPtr = (USBD_ERR *)0;

    return (desc_len);
}


/*
*********************************************************************************************************
*                                            USBD_StrAdd()
*
* Description : Add string to USB device.
*
* Argument(s) : dev_nbr     Device number.
*
*               p_str       Pointer to string to add (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               String successfully added.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*
*                               -RETURNED BY USBD_StrDescAdd()-
*                               See USBD_StrDescAdd() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) USB spec 2.0 chapter 9.5 states "Where appropriate, descriptors contain references
*                   to string descriptors that provide displayable information describing a descriptor
*                   in human-readable form. The inclusion of string descriptors is optional.  However,
*                   the reference fields within descriptors are mandatory. If a device does not support
*                   string descriptors, string reference fields must be reset to zero to indicate no
*                   string descriptor is available".
*
*                   Since string descriptors are optional, 'p_str' could be a NULL pointer.
*********************************************************************************************************
*/

void  USBD_StrAdd (       CPU_INT08U   dev_nbr,
                   const  CPU_CHAR    *p_str,
                          USBD_ERR    *p_err)
{
    USBD_DEV    *p_dev;


    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return;
    }

    if ((p_dev->State != USBD_DEV_STATE_NONE) &&                /* Chk curr dev state.                                  */
        (p_dev->State != USBD_DEV_STATE_INIT)) {
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return;
    }

#if (USBD_CFG_MAX_NBR_STR > 0u)
    USBD_StrDescAdd(p_dev, p_str, p_err);
#else
    (void)p_str;
#endif
}


/*
*********************************************************************************************************
*                                           USBD_StrIxGet()
*
* Description : Get string index corresponding to a given string.
*
* Argument(s) : dev_nbr     Device number.
*
*               p_str       Pointer to string.
*
* Return(s)   : String index.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  USBD_StrIxGet (       CPU_INT08U   dev_nbr,
                           const  CPU_CHAR    *p_str)
{
    USBD_DEV    *p_dev;
    CPU_INT08U  str_ix;


    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
        return (0u);
    }

    str_ix = USBD_StrDescIxGet(p_dev, p_str);

    return (str_ix);
}


/*
*********************************************************************************************************
*                                           USBD_DescWr08()
*
* Description : Write 8-bit value to the descriptor buffer.
*
* Argument(s) : dev_nbr     Device number.
*
*               val         8-bit value to write in descriptor buffer.
*
* Return(s)   : none.
*
* Note(s)     : (1) USB classes MAY used this function to append class-specific descriptors to the
*                   configuration descriptor.
*********************************************************************************************************
*/

void  USBD_DescWr08 (CPU_INT08U  dev_nbr,
                     CPU_INT08U  val)
{
    USBD_DEV  *p_dev;


    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
        return;
    }

    if (*(p_dev->DescBufErrPtr) == USBD_ERR_NONE) {
        USBD_DescWrReq(p_dev, &val, 1u);
    }
}


/*
*********************************************************************************************************
*                                           USBD_DescWr16()
*
* Description : Write a 16-bit value to the descriptor buffer.
*
* Argument(s) : dev_nbr     Device number.
*
*               val         16-bit value to write in descriptor buffer.
*
* Return(s)   : none.
*
* Note(s)     : (1) USB classes MAY used this function to append class-specific descriptors to the
*                   configuration descriptor.
*
*               (2) USB descriptors are in little-endian format.
*********************************************************************************************************
*/

void  USBD_DescWr16 (CPU_INT08U  dev_nbr,
                     CPU_INT16U  val)
{
    USBD_DEV    *p_dev;


    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
        return;
    }

    if (*(p_dev->DescBufErrPtr) == USBD_ERR_NONE) {
        CPU_INT08U   buf[2u];


        buf[0u] = (CPU_INT08U)( val        & DEF_INT_08_MASK);
        buf[1u] = (CPU_INT08U)((val >> 8u) & DEF_INT_08_MASK);

        USBD_DescWrReq(p_dev, &buf[0u], 2u);
    }
}


/*
*********************************************************************************************************
*                                           USBD_DescWr24()
*
* Description : Write a 24-bit value to the descriptor buffer.
*
* Argument(s) : dev_nbr     Device number.
*
*               val         32-bit value containing 24 useful bits to write in descriptor buffer.
*
* Return(s)   : none.
*
* Note(s)     : (1) USB classes MAY used this function to append class-specific descriptors to the
*                   configuration descriptor.
*
*               (2) USB descriptors are in little-endian format.
*********************************************************************************************************
*/

void  USBD_DescWr24 (CPU_INT08U  dev_nbr,
                     CPU_INT32U  val)
{
    USBD_DEV    *p_dev;


    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
        return;
    }

    if (*(p_dev->DescBufErrPtr) == USBD_ERR_NONE) {
        CPU_INT08U   buf[3u];


        buf[0u] = (CPU_INT08U)( val         & DEF_INT_08_MASK);
        buf[1u] = (CPU_INT08U)((val >> 8u)  & DEF_INT_08_MASK);
        buf[2u] = (CPU_INT08U)((val >> 16u) & DEF_INT_08_MASK);

        USBD_DescWrReq(p_dev, &buf[0u], 3u);
    }
}


/*
*********************************************************************************************************
*                                           USBD_DescWr32()
*
* Description : Write a 32-bit value to the descriptor buffer.
*
* Argument(s) : dev_nbr     Device number.
*
*               val         32-bit value to write in descriptor buffer.
*
* Return(s)   : none.
*
* Note(s)     : (1) USB classes MAY used this function to append class-specific descriptors to the
*                   configuration descriptor.
*
*               (2) USB descriptors are in little-endian format.
*********************************************************************************************************
*/

void  USBD_DescWr32 (CPU_INT08U  dev_nbr,
                     CPU_INT32U  val)
{
    USBD_DEV    *p_dev;


    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
        return;
    }

    if (*(p_dev->DescBufErrPtr) == USBD_ERR_NONE) {
        CPU_INT08U   buf[4u];


        buf[0u] = (CPU_INT08U)( val         & DEF_INT_08_MASK);
        buf[1u] = (CPU_INT08U)((val >> 8u ) & DEF_INT_08_MASK);
        buf[2u] = (CPU_INT08U)((val >> 16u) & DEF_INT_08_MASK);
        buf[3u] = (CPU_INT08U)((val >> 24u) & DEF_INT_08_MASK);

        USBD_DescWrReq(p_dev, &buf[0u], 4u);
    }
}


/*
*********************************************************************************************************
*                                            USBD_DescWr()
*
* Description : Write buffer into the descriptor buffer.
*
* Argument(s) : dev_nbr     Device number.
*
*               p_buf       Pointer to buffer to write into descriptor buffer.
*
*               len         Length of the buffer.
*
* Return(s)   : none.
*
* Note(s)     : (1) USB classes MAY used this function to append class-specific descriptors to the
*                   configuration descriptor.
*********************************************************************************************************
*/

void  USBD_DescWr (       CPU_INT08U   dev_nbr,
                   const  CPU_INT08U  *p_buf,
                          CPU_INT16U   len)
{
    USBD_DEV  *p_dev;


    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
        return;
    }

    if ((p_buf == (CPU_INT08U *)0) ||
        (len   ==  0u)) {
        return;
    }

    if (*(p_dev->DescBufErrPtr) == USBD_ERR_NONE) {
        USBD_DescWrReq(p_dev, p_buf, len);
    }
}


/*
*********************************************************************************************************
*                                           USBD_BulkAdd()
*
* Description : Add a bulk endpoint to alternate setting interface.
*
* Argument(s) : dev_nbr         Device number.
*
*               cfg_nbr         Configuration number.
*
*               if_nbr          Interface number.
*
*               if_alt_nbr      Interface alternate setting number.
*
*               dir_in          Endpoint direction.
*                                   DEF_YES    IN   direction.
*                                   DEF_NO     OUT  direction.
*
*               max_pkt_len     Endpoint maximum packet length (see Note #1)
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Bulk endpoint successfully added.
*                               USBD_ERR_INVALID_ARG        Invalid argument(s) passed to 'max_pkt_len'.
*
*                                                           ------- RETURNED BY USBD_EP_Add() : -------
*                               USBD_ERR_NONE               Endpoint successfully added.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device              number.
*                               USBD_ERR_CFG_INVALID_NBR    Invalid configuration       number.
*                               USBD_ERR_IF_INVALID_NBR     Invalid           interface number.
*                               USBD_ERR_IF_ALT_INVALID_NBR Invalid alternate interface number.
*                               USBD_ERR_EP_NONE_AVAIL      Physical endpoint NOT available.
*                               USBD_ERR_EP_ALLOC           Endpoints NOT available.
*
* Return(s)   : Endpoint address,  if NO error(s).
*
*               USBD_EP_ADDR_NONE, otherwise.
*
* Note(s)     : (1) If the 'max_pkt_len' argument is '0' the stack will allocate the first available
*                   BULK endpoint regardless its maximum packet size.
*********************************************************************************************************
*/

CPU_INT08U  USBD_BulkAdd (CPU_INT08U    dev_nbr,
                          CPU_INT08U    cfg_nbr,
                          CPU_INT08U    if_nbr,
                          CPU_INT08U    if_alt_nbr,
                          CPU_BOOLEAN   dir_in,
                          CPU_INT16U    max_pkt_len,
                          USBD_ERR     *p_err)
{
    CPU_INT08U  ep_addr;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }

#if (USBD_CFG_HS_EN == DEF_ENABLED)                             /* USBD_CFG_NBR_SPD_BIT will always be clear in FS.     */
    if (((max_pkt_len !=   0u)  &&                              /* Chk EP size.                                         */
         (max_pkt_len != 512u)) &&
         (DEF_BIT_IS_SET(cfg_nbr, USBD_CFG_NBR_SPD_BIT))) {
       *p_err = USBD_ERR_INVALID_ARG;
        return (USBD_EP_ADDR_NONE);
    }
#endif

#if (USBD_CFG_HS_EN == DEF_ENABLED)
    if (((max_pkt_len !=  0u)  &&
         (max_pkt_len !=  8u)  &&
         (max_pkt_len != 16u)  &&
         (max_pkt_len != 32u)  &&
         (max_pkt_len != 64u)) &&
         (DEF_BIT_IS_CLR(cfg_nbr, USBD_CFG_NBR_SPD_BIT) == DEF_YES)) {
#else
    if ( (max_pkt_len !=  0u)  &&
         (max_pkt_len !=  8u)  &&
         (max_pkt_len != 16u)  &&
         (max_pkt_len != 32u)  &&
         (max_pkt_len != 64u)) {
#endif
       *p_err = USBD_ERR_INVALID_ARG;
        return (USBD_EP_ADDR_NONE);
    }
#endif

    ep_addr = USBD_EP_Add(dev_nbr,
                          cfg_nbr,
                          if_nbr,
                          if_alt_nbr,
                          USBD_EP_TYPE_BULK,
                          dir_in,
                          max_pkt_len,
                          0u,
                          p_err);
    return (ep_addr);
}


/*
*********************************************************************************************************
*                                           USBD_IntrAdd()
*
* Description : Add an interrupt endpoint to alternate setting interface.
*
* Argument(s) : dev_nbr         Device number.
*
*               cfg_nbr         Configuration number.
*
*               if_nbr          Interface number.
*
*               if_alt_nbr      Interface alternate setting number.
*
*               dir_in          Endpoint Direction.
*                                   DEF_YES    IN   direction.
*                                   DEF_NO     OUT  direction.
*
*               max_pkt_len     Endpoint maximum packet length. (see note #1)
*
*               interval        Endpoint interval in frames or microframes.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Interrupt endpoint successfully added.
*                               USBD_ERR_INVALID_ARG        Invalid argument(s) passed to 'interval'/
*                                                               'max_pkt_len'.
*
*                                                           ------- RETURNED BY USBD_EP_Add() : -------
*                               USBD_ERR_NONE               Endpoint successfully added.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device              number.
*                               USBD_ERR_CFG_INVALID_NBR    Invalid configuration       number.
*                               USBD_ERR_IF_INVALID_NBR     Invalid           interface number.
*                               USBD_ERR_IF_ALT_INVALID_NBR Invalid alternate interface number.
*                               USBD_ERR_EP_NONE_AVAIL      Physical endpoint NOT available.
*                               USBD_ERR_EP_ALLOC           Endpoints NOT available.
*
* Return(s)   : Endpoint address,  if NO error(s).
*
*               USBD_EP_ADDR_NONE, otherwise.
*
* Note(s)     : (1) If the 'max_pkt_len' argument is '0' the stack will allocate the first available
*                   INTERRUPT endpoint regardless its maximum packet size.
*
*               (2) For high-speed interrupt endpoints, bInterval value must be in the range
*                   from 1 to 16. The bInterval value is used as the exponent for a 2^(bInterval-1)
*                   value. Maximum polling interval value is 2^(16-1) = 32768 32768 microframes
*                   (i.e. 4096 frames) in high-speed.
*********************************************************************************************************
*/

CPU_INT08U  USBD_IntrAdd (CPU_INT08U    dev_nbr,
                          CPU_INT08U    cfg_nbr,
                          CPU_INT08U    if_nbr,
                          CPU_INT08U    if_alt_nbr,
                          CPU_BOOLEAN   dir_in,
                          CPU_INT16U    max_pkt_len,
                          CPU_INT16U    interval,
                          USBD_ERR     *p_err)
{
    CPU_INT08U  ep_addr;
    CPU_INT08U  interval_code;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if (p_err == (USBD_ERR *)0) {                               /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0);
    }

    if (interval == 0u) {
       *p_err = USBD_ERR_INVALID_ARG;
        return (USBD_EP_NBR_NONE);
    }
#endif

#if (USBD_CFG_HS_EN == DEF_ENABLED)                             /* USBD_CFG_NBR_SPD_BIT will always be clear in FS.     */
                                                                /* Full spd validation.                                 */
    if (DEF_BIT_IS_CLR(cfg_nbr, USBD_CFG_NBR_SPD_BIT) == DEF_YES) {
#endif
        if (max_pkt_len > 64u) {
           *p_err = USBD_ERR_INVALID_ARG;
            return (USBD_EP_NBR_NONE);
        }

        if (interval < 255u) {
            interval_code = (CPU_INT08U)interval;
        } else {
           *p_err = USBD_ERR_INVALID_ARG;
            return (USBD_EP_NBR_NONE);
        }
#if (USBD_CFG_HS_EN == DEF_ENABLED)
    } else {                                                    /* High spd validation.                                 */
        if (((if_alt_nbr  ==    0u) &&
             (max_pkt_len >    64u)) ||
             (max_pkt_len >  1024u)) {
           *p_err = USBD_ERR_INVALID_ARG;
            return (USBD_EP_NBR_NONE);
        }

        if (interval > USBD_EP_MAX_INTERVAL_VAL) {              /* See Note #2.                                         */
           *p_err = USBD_ERR_INVALID_ARG;
            return (USBD_EP_NBR_NONE);
        }

        if (MATH_IS_PWR2(interval) == DEF_NO) {                 /* Interval must be a power of 2.                       */
           *p_err = USBD_ERR_INVALID_ARG;
            return (USBD_EP_NBR_NONE);
        }
                                                               /* Compute bInterval exponent in 2^(bInterval-1).        */
        interval_code = (CPU_INT08U)(32u - CPU_CntLeadZeros32(interval));

        if (interval_code > 16u) {
           *p_err = USBD_ERR_INVALID_ARG;
            return (USBD_EP_NBR_NONE);
        }
    }
#endif

    ep_addr = USBD_EP_Add(dev_nbr,
                          cfg_nbr,
                          if_nbr,
                          if_alt_nbr,
                          USBD_EP_TYPE_INTR,
                          dir_in,
                          max_pkt_len,
                          interval_code,
                          p_err);
    return (ep_addr);
}


/*
*********************************************************************************************************
*                                           USBD_IsocAdd()
*
* Description : Add an isochronous endpoint to alternate setting interface.
*
* Argument(s) : dev_nbr             Device number.
*
*               cfg_nbr             Configuration number.
*
*               if_nbr              Interface number.
*
*               if_alt_nbr          Interface alternate setting number.
*
*               dir_in              Endpoint Direction :
*                                       DEF_YES,    IN  direction.
*                                       DEF_NO,     OUT direction.
*
*               attrib              Isochronous endpoint synchronization and usage type attributes.
*
*               max_pkt_len         Endpoint maximum packet length (see Note #1).
*
*               transaction_frame   Endpoint transactions per (micro)frame (see Note #2).
*
*               interval            Endpoint interval in frames or microframes.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Interrupt endpoint successfully added.
*                               USBD_ERR_INVALID_ARG        Invalid argument(s) passed to 'attrib'/
*                                                               'max_pkt_len'/'transaction_frame'/
*                                                               'interval'.
*
*                                                           ------- RETURNED BY USBD_EP_Add() : -------
*                               USBD_ERR_NONE               Endpoint successfully added.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device              number.
*                               USBD_ERR_CFG_INVALID_NBR    Invalid configuration       number.
*                               USBD_ERR_IF_INVALID_NBR     Invalid           interface number.
*                               USBD_ERR_IF_ALT_INVALID_NBR Invalid alternate interface number.
*                               USBD_ERR_EP_NONE_AVAIL      Physical endpoint NOT available.
*                               USBD_ERR_EP_ALLOC           Endpoints NOT available.
*
* Return(s)   : Endpoint address,  if NO error(s).
*
*               USBD_EP_ADDR_NONE, otherwise.
*
* Note(s)     : (1) If the 'max_pkt_len' argument is '0', the stack allocates the first available
*                   ISOCHRONOUS endpoint regardless of its maximum packet size.
*
*               (2) For full-speed endpoints, 'transaction_frame' must be set to 1 since there is no
*                   support for high-bandwidth endpoints.
*
*               (3) For full-/high-speed isochronous endpoints, bInterval value must be in the range
*                   from 1 to 16. The bInterval value is used as the exponent for a 2^(bInterval-1)
*                   value. Maximum polling interval value is 2^(16-1) = 32768 frames in full-speed and
*                   32768 microframes (i.e. 4096 frames) in high-speed.
*********************************************************************************************************
*/

#if (USBD_CFG_EP_ISOC_EN == DEF_ENABLED)
CPU_INT08U  USBD_IsocAdd (CPU_INT08U    dev_nbr,
                          CPU_INT08U    cfg_nbr,
                          CPU_INT08U    if_nbr,
                          CPU_INT08U    if_alt_nbr,
                          CPU_BOOLEAN   dir_in,
                          CPU_INT08U    attrib,
                          CPU_INT16U    max_pkt_len,
                          CPU_INT08U    transaction_frame,
                          CPU_INT16U    interval,
                          USBD_ERR     *p_err)
{
    CPU_INT08U  ep_addr;
    CPU_INT16U  pkt_len;
    CPU_INT08U  interval_code;

                                                                /* ---------------- VALIDATE ARGUMENTS ---------------- */
    if ((if_alt_nbr  == 0u) &&                                  /* Chk if dflt IF setting with isoc EP max_pkt_len > 0. */
        (max_pkt_len >  0u)) {
       *p_err = USBD_ERR_INVALID_ARG;
        return (USBD_EP_NBR_NONE);
    }

                                                                /* Chk if sync & usage bits are used.                   */
    if ((attrib & ~(USBD_EP_TYPE_SYNC_MASK | USBD_EP_TYPE_USAGE_MASK)) != 0u) {
       *p_err = USBD_ERR_INVALID_ARG;
        return (USBD_EP_NBR_NONE);
    }

#if (USBD_CFG_HS_EN == DEF_ENABLED)                             /* USBD_CFG_NBR_SPD_BIT will always be clear in FS.     */
                                                                /* Full spd validation.                                 */
    if (DEF_BIT_IS_CLR(cfg_nbr, USBD_CFG_NBR_SPD_BIT) == DEF_YES) {
#endif
        if (max_pkt_len > 1023u) {
           *p_err = USBD_ERR_INVALID_ARG;
            return (USBD_EP_NBR_NONE);
        }

        if (transaction_frame != 1u) {
           *p_err = USBD_ERR_INVALID_ARG;
            return (USBD_EP_NBR_NONE);
        }
#if (USBD_CFG_HS_EN == DEF_ENABLED)
    } else {                                                    /* High spd validation.                                 */
        switch (transaction_frame) {
            case 1u:
                 if (max_pkt_len > 1024u) {
                    *p_err = USBD_ERR_INVALID_ARG;
                     return (USBD_EP_NBR_NONE);
                 }
                 break;


            case 2u:
                 if ((max_pkt_len <  513u) ||
                     (max_pkt_len > 1024u)) {
                    *p_err = USBD_ERR_INVALID_ARG;
                     return (USBD_EP_NBR_NONE);
                 }
                 break;

            case 3u:
                 if ((max_pkt_len <  683u) ||
                     (max_pkt_len > 1024u)) {
                    *p_err = USBD_ERR_INVALID_ARG;
                     return (USBD_EP_NBR_NONE);
                 }
                 break;

            default:
                *p_err = USBD_ERR_INVALID_ARG;
                 return (USBD_EP_NBR_NONE);
        }
    }
#endif

                                                                /* Explicit feedback EP must be set to no sync.         */
    if (((attrib & USBD_EP_TYPE_USAGE_MASK) == USBD_EP_TYPE_USAGE_FEEDBACK) &&
        ((attrib & USBD_EP_TYPE_SYNC_MASK)  != USBD_EP_TYPE_SYNC_NONE)) {
       *p_err = USBD_ERR_INVALID_ARG;
        return (USBD_EP_NBR_NONE);
    }

    if (interval > USBD_EP_MAX_INTERVAL_VAL) {                  /* See Note #3.                                         */
       *p_err = USBD_ERR_INVALID_ARG;
        return (USBD_EP_NBR_NONE);
    }

    if (MATH_IS_PWR2(interval) == DEF_NO) {                     /* Interval must be a power of 2.                       */
       *p_err = USBD_ERR_INVALID_ARG;
        return (USBD_EP_NBR_NONE);
    }
                                                               /* Compute bInterval exponent in 2^(bInterval-1).        */
    interval_code = (CPU_INT08U)(32u - CPU_CntLeadZeros32(interval));

    if (interval_code > 16u) {
       *p_err = USBD_ERR_INVALID_ARG;
        return (USBD_EP_NBR_NONE);
    }

    pkt_len = (transaction_frame - 1u) << 11u |
               max_pkt_len;

    ep_addr = USBD_EP_Add(dev_nbr,
                          cfg_nbr,
                          if_nbr,
                          if_alt_nbr,
                          USBD_EP_TYPE_ISOC | attrib,
                          dir_in,
                          pkt_len,
                          interval_code,
                          p_err);

    return (ep_addr);
}
#endif


/*
*********************************************************************************************************
*                                      USBD_IsocSyncRefreshSet()
*
* Description : Set synchronization feedback rate on synchronization isochronous endpoint.
*
* Argument(s) : dev_nbr         Device number.
*
*               cfg_nbr         Configuration number.
*
*               if_nbr          Interface number.
*
*               if_alt_nbr      Interface alternate setting number.
*
*               synch_ep_addr   Synchronization endpoint address.
*
*               sync_refresh    Exponent of synchronization feedback rate (see Note #3).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Interrupt endpoint successfully added.
*                               USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE      Invalid device state (see Note #1).
*                               USBD_ERR_CFG_INVALID_NBR        Invalid configuration number.
*                               USBD_ERR_IF_INVALID_NBR         Invalid interface number (see Note #2).
*                               USBD_ERR_IF_ALT_INVALID_NBR     Invalid interface alternate setting number.
*                               USBD_ERR_EP_INVALID_ADDR        Invalid endpoint address.
*                               USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*                               USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'sync_refresh'.
*
* Return(s)   : none.
*
* Note(s)     : (1) Synchronization endpoints can ONLY be associated when the device is in the following
*                   states:
*
*                   USBD_DEV_STATE_NONE    Device controller has not been initialized.
*                   USBD_DEV_STATE_INIT    Device controller already      initialized.
*
*               (2) Interface class code must be USBD_CLASS_CODE_AUDIO and protocol 'zero', for audio
*                   class 1.0.
*
*               (3) If explicit synchronization mechanism is needed to maintain synchronization during
*                   transfers, the information carried over the synchronization path must be available
*                   every 2 ^ (10 - P) frames, with P ranging from 1 to 9 (512 ms down to 2 ms).
*
*               (4) Table 4-22 "Standard AS Isochronous Synch Endpoint Descriptor" of Audio 1.0
*                   specification indicates for bmAttributes field no usage type for bits 5..4. But
*                   USB 2.0 specification, Table 9-13 "Standard Endpoint Descriptor" indicates several
*                   types of usage. When an explicit feedback is defined for a asynchronous isochronous
*                   endpoint, the associated synch feedback should use the Usage type 'Feedback endpoint'.
*********************************************************************************************************
*/

#if (USBD_CFG_EP_ISOC_EN == DEF_ENABLED)
void  USBD_IsocSyncRefreshSet (CPU_INT08U   dev_nbr,
                               CPU_INT08U   cfg_nbr,
                               CPU_INT08U   if_nbr,
                               CPU_INT08U   if_alt_nbr,
                               CPU_INT08U   synch_ep_addr,
                               CPU_INT08U   sync_refresh,
                               USBD_ERR    *p_err)
{
    USBD_DEV      *p_dev;
    USBD_CFG      *p_cfg;
    USBD_IF       *p_if;
    USBD_IF_ALT   *p_if_alt;
    USBD_EP_INFO  *p_ep;
#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)
    CPU_INT32U     ep_alloc_map;
#endif
    CPU_INT08U     ep_nbr;
    CPU_BOOLEAN    found;
    CPU_SR_ALLOC();


    if ((sync_refresh < 1u) ||                                  /* See Note #3.                                         */
        (sync_refresh > 9u)) {
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }

                                                                /* --------------- GET OBJECT REFERENCES -------------- */
    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return;
    }

    if ((p_dev->State != USBD_DEV_STATE_NONE) &&                /* Chk curr dev state.                                  */
        (p_dev->State != USBD_DEV_STATE_INIT)) {
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return;
    }

    p_cfg = USBD_CfgRefGet(p_dev, cfg_nbr);                     /* Get cfg struct.                                      */
    if (p_cfg == (USBD_CFG *)0) {
       *p_err = USBD_ERR_CFG_INVALID_NBR;
        return;
    }

    p_if = USBD_IF_RefGet(p_cfg, if_nbr);                       /* Get IF struct.                                       */
    if (p_if == (USBD_IF *)0) {
       *p_err = USBD_ERR_IF_INVALID_NBR;
        return;
    }

    if (p_if->ClassCode != USBD_CLASS_CODE_AUDIO) {             /* Chk if audio class.                                  */
       *p_err = USBD_ERR_IF_INVALID_NBR;
        return;
    }

    if (p_if->ClassProtocolCode != 0u) {                        /* Chk if audio class, version 1.0.                     */
       *p_err = USBD_ERR_IF_INVALID_NBR;
        return;
    }

    p_if_alt = USBD_IF_AltRefGet(p_if, if_alt_nbr);             /* Get IF alt setting struct.                           */
    if (p_if_alt == (USBD_IF_ALT *)0) {
       *p_err = USBD_ERR_IF_ALT_INVALID_NBR;
        return;
    }

    found =  DEF_NO;
    p_ep  = (USBD_EP_INFO *)0;

#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)
    ep_alloc_map = p_if_alt->EP_TblMap;
    while ((ep_alloc_map != DEF_BIT_NONE) &&
           (found        != DEF_YES)) {
        ep_nbr = (CPU_INT08U)CPU_CntTrailZeros32(ep_alloc_map);
        p_ep   =  p_if_alt->EP_TblPtrs[ep_nbr];

        if (p_ep->Addr == synch_ep_addr) {
            found       = DEF_YES;
        }

        DEF_BIT_CLR(ep_alloc_map, DEF_BIT32(ep_nbr));
    }
#else
    p_ep = p_if_alt->EP_HeadPtr;

    for (ep_nbr = 0u; ep_nbr < p_if_alt->EP_NbrTotal; ep_nbr++) {
        if (p_ep->Addr == synch_ep_addr) {
            found       = DEF_YES;
            break;
        }

        p_ep = p_ep->NextPtr;
    }
#endif

    if (found != DEF_YES) {
       *p_err  = USBD_ERR_EP_INVALID_ADDR;
        return;
    }
                                                                /* Chk EP type attrib.                                  */
    if ((p_ep->Attrib & USBD_EP_TYPE_MASK) != USBD_EP_TYPE_ISOC) {
       *p_err = USBD_ERR_EP_INVALID_TYPE;
        return;
    }

    switch (p_ep->Attrib & USBD_EP_TYPE_SYNC_MASK) {            /* Chk EP sync type attrib.                             */
        case USBD_EP_TYPE_SYNC_NONE:
             break;

        case USBD_EP_TYPE_SYNC_ASYNC:
        case USBD_EP_TYPE_SYNC_ADAPTIVE:
        case USBD_EP_TYPE_SYNC_SYNC:
        default:
            *p_err = USBD_ERR_EP_INVALID_TYPE;
             return;
    }

    switch (p_ep->Attrib & USBD_EP_TYPE_USAGE_MASK) {           /* Chk EP usage type attrib.                            */
        case USBD_EP_TYPE_USAGE_FEEDBACK:                       /* See Note #4.                                         */
             break;

        case USBD_EP_TYPE_USAGE_DATA:
        case USBD_EP_TYPE_USAGE_IMPLICIT_FEEDBACK:
        default:
            *p_err = USBD_ERR_EP_INVALID_TYPE;
             return;
    }

    if (p_ep->SyncAddr != 0u) {                                 /* Chk associated sync EP addr.                         */
       *p_err = USBD_ERR_EP_INVALID_TYPE;
        return;
    }

    CPU_CRITICAL_ENTER();
    p_ep->SyncRefresh = sync_refresh;
    CPU_CRITICAL_EXIT();
}
#endif


/*
*********************************************************************************************************
*                                       USBD_IsocSyncAddrSet()
*
* Description : Associate synchronization endpoint to isochronous endpoint.
*
* Argument(s) : dev_nbr         Device number.
*
*               cfg_nbr         Configuration number.
*
*               if_nbr          Interface number.
*
*               if_alt_nbr      Interface alternate setting number.
*
*               data_ep_addr    Data endpoint address.
*
*               sync_addr       Associated synchronization endpoint.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   USBD_ERR_NONE                   Interrupt endpoint successfully added.
*                                   USBD_ERR_DEV_INVALID_NBR        Invalid device number.
*                                   USBD_ERR_DEV_INVALID_STATE      Invalid device state (see Note #1).
*                                   USBD_ERR_CFG_INVALID_NBR        Invalid configuration number.
*                                   USBD_ERR_IF_INVALID_NBR         Invalid interface number (see Note #2).
*                                   USBD_ERR_IF_ALT_INVALID_NBR     Invalid interface alternate setting number.
*                                   USBD_ERR_EP_INVALID_ADDR        Invalid endpoint address.
*                                   USBD_ERR_EP_INVALID_TYPE        Invalid endpoint type.
*                                   USBD_ERR_INVALID_ARG            Invalid argument(s) passed to 'sync_addr'.
*
* Return(s)   : none.
*
* Note(s)     : (1) Synchronization endpoints can ONLY be associated when the device is in the following
*                   states:
*
*                   USBD_DEV_STATE_NONE    Device controller has not been initialized.
*                   USBD_DEV_STATE_INIT    Device controller already      initialized.
*
*               (2) Interface class code must be USBD_CLASS_CODE_AUDIO and protocol 'zero', for audio
*                   class 1.0.
*********************************************************************************************************
*/

#if (USBD_CFG_EP_ISOC_EN == DEF_ENABLED)
void  USBD_IsocSyncAddrSet (CPU_INT08U   dev_nbr,
                            CPU_INT08U   cfg_nbr,
                            CPU_INT08U   if_nbr,
                            CPU_INT08U   if_alt_nbr,
                            CPU_INT08U   data_ep_addr,
                            CPU_INT08U   sync_addr,
                            USBD_ERR    *p_err)
{
    USBD_DEV      *p_dev;
    USBD_CFG      *p_cfg;
    USBD_IF       *p_if;
    USBD_IF_ALT   *p_if_alt;
    USBD_EP_INFO  *p_ep;
    USBD_EP_INFO  *p_ep_isoc;
#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)
    CPU_INT32U     ep_alloc_map;
#endif
    CPU_INT08U     ep_nbr;
    CPU_BOOLEAN    found_ep;
    CPU_BOOLEAN    found_sync;
    CPU_SR_ALLOC();


                                                                /* --------------- GET OBJECT REFERENCES -------------- */
    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return;
    }

    if ((p_dev->State != USBD_DEV_STATE_NONE) &&                /* Chk curr dev state.                                  */
        (p_dev->State != USBD_DEV_STATE_INIT)) {
       *p_err = USBD_ERR_DEV_INVALID_STATE;
        return;
    }

    p_cfg = USBD_CfgRefGet(p_dev, cfg_nbr);                     /* Get cfg struct.                                      */
    if (p_cfg == (USBD_CFG *)0) {
       *p_err = USBD_ERR_CFG_INVALID_NBR;
        return;
    }

    p_if = USBD_IF_RefGet(p_cfg, if_nbr);                       /* Get IF struct.                                       */
    if (p_if == (USBD_IF *)0) {
       *p_err = USBD_ERR_IF_INVALID_NBR;
        return;
    }

    if (p_if->ClassCode != USBD_CLASS_CODE_AUDIO) {             /* Chk if audio class.                                  */
       *p_err = USBD_ERR_IF_INVALID_NBR;
        return;
    }

    if (p_if->ClassProtocolCode != 0u) {                        /* Chk if audio class, version 1.0.                     */
       *p_err = USBD_ERR_IF_INVALID_NBR;
        return;
    }

    p_if_alt = USBD_IF_AltRefGet(p_if, if_alt_nbr);             /* Get IF alt setting struct.                           */
    if (p_if_alt == (USBD_IF_ALT *)0) {
       *p_err = USBD_ERR_IF_ALT_INVALID_NBR;
        return;
    }

    found_ep   =  DEF_NO;
    found_sync =  DEF_NO;
    p_ep_isoc  = (USBD_EP_INFO *)0;

#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)
    ep_alloc_map = p_if_alt->EP_TblMap;
    while ((ep_alloc_map != DEF_BIT_NONE) &&
           ((found_ep    != DEF_YES)      ||
            (found_sync  != DEF_YES))) {
        ep_nbr = (CPU_INT08U)CPU_CntTrailZeros32(ep_alloc_map);
        p_ep   =  p_if_alt->EP_TblPtrs[ep_nbr];

        if (p_ep->Addr == data_ep_addr) {
            found_ep    = DEF_YES;
            p_ep_isoc   = p_ep;
        }

        if (p_ep->Addr == sync_addr) {
            found_sync  = DEF_YES;
        }

        DEF_BIT_CLR(ep_alloc_map, DEF_BIT32(ep_nbr));
    }
#else
    p_ep = p_if_alt->EP_HeadPtr;

    for (ep_nbr = 0u; ep_nbr < p_if_alt->EP_NbrTotal; ep_nbr++) {
        if (p_ep->Addr == data_ep_addr) {
            found_ep    = DEF_YES;
            p_ep_isoc   = p_ep;
        }

        if (p_ep->Addr == sync_addr) {
            found_sync  = DEF_YES;
        }

        if ((found_ep   == DEF_YES) &&
            (found_sync == DEF_YES)) {
             break;
        }

        p_ep = p_ep->NextPtr;
    }
#endif

    if (found_ep != DEF_YES) {
       *p_err = USBD_ERR_EP_INVALID_ADDR;
        return;
    }

    if (found_sync != DEF_YES) {
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }
                                                                /* Chk EP type attrib.                                  */
    if ((p_ep_isoc->Attrib & USBD_EP_TYPE_MASK) != USBD_EP_TYPE_ISOC) {
       *p_err = USBD_ERR_EP_INVALID_TYPE;
        return;
    }

    switch (p_ep_isoc->Attrib & USBD_EP_TYPE_SYNC_MASK) {       /* Chk EP sync type attrib.                             */
        case USBD_EP_TYPE_SYNC_ASYNC:
             if (USBD_EP_IS_IN(p_ep_isoc->Addr) == DEF_YES) {
                *p_err = USBD_ERR_EP_INVALID_TYPE;
                 return;
             }
             break;

        case USBD_EP_TYPE_SYNC_ADAPTIVE:
             if (USBD_EP_IS_IN(p_ep_isoc->Addr) == DEF_NO) {
                *p_err = USBD_ERR_EP_INVALID_TYPE;
                 return;
             }
             break;

        case USBD_EP_TYPE_SYNC_NONE:
        case USBD_EP_TYPE_SYNC_SYNC:
        default:
            *p_err = USBD_ERR_EP_INVALID_TYPE;
             return;
    }

    switch (p_ep_isoc->Attrib & USBD_EP_TYPE_USAGE_MASK) {      /* Chk EP usage type attrib.                            */
        case USBD_EP_TYPE_USAGE_DATA:
             break;

        case USBD_EP_TYPE_USAGE_FEEDBACK:
        case USBD_EP_TYPE_USAGE_IMPLICIT_FEEDBACK:
        default:
            *p_err = USBD_ERR_EP_INVALID_TYPE;
             return;
    }

    if (p_ep_isoc->SyncRefresh != 0u) {                         /* Refresh interval must be set to zero.                */
       *p_err = USBD_ERR_EP_INVALID_TYPE;
        return;
    }

    CPU_CRITICAL_ENTER();
    p_ep_isoc->SyncAddr = sync_addr;
    CPU_CRITICAL_EXIT();
}
#endif


/*
*********************************************************************************************************
*                                            USBD_EP_Add()
*
* Description : Add an endpoint to alternate setting interface.
*
* Argument(s) : dev_nbr             Device number.
*
*               cfg_nbr             Configuration number.
*
*               if_nbr              Interface number.
*
*               if_alt_nbr          Interface alternate setting number.
*
*               type                Endpoint's attributes.
*
*               dir_in              Endpoint Direction.
*
*               max_pkt_len         Endpoint maximum packet size.
*               -----------         Argument validated by the caller.
*
*               interval            Interval for polling data transfers.
*               --------            Argument validated by the caller.
*
*               class_desc          Callback to append a class-specific descriptor in the configuration
*                                   descriptor.
*
*               class_desc_size     Size of the class class-specific descriptor.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Endpoint successfully added.
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device              number.
*                               USBD_ERR_CFG_INVALID_NBR    Invalid configuration       number.
*                               USBD_ERR_IF_INVALID_NBR     Invalid           interface number.
*                               USBD_ERR_IF_ALT_INVALID_NBR Invalid alternate interface number.
*                               USBD_ERR_EP_NONE_AVAIL      Physical endpoint NOT available.
*                               USBD_ERR_EP_ALLOC           Endpoints NOT available.
*
* Return(s)   : Endpoint number,  if NO error(s).
*
*               USBD_EP_NBR_NONE, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT08U  USBD_EP_Add (CPU_INT08U    dev_nbr,
                                 CPU_INT08U    cfg_nbr,
                                 CPU_INT08U    if_nbr,
                                 CPU_INT08U    if_alt_nbr,
                                 CPU_INT08U    attrib,
                                 CPU_BOOLEAN   dir_in,
                                 CPU_INT16U    max_pkt_len,
                                 CPU_INT08U    interval,
                                 USBD_ERR     *p_err)

{
    USBD_DEV      *p_dev;
    USBD_CFG      *p_cfg;
    USBD_IF       *p_if;
    USBD_IF_ALT   *p_if_alt;
    USBD_EP_INFO  *p_ep;
    CPU_INT08U     ep_type;
    CPU_INT32U     ep_alloc_map;
    CPU_INT32U     ep_alloc_map_clr;
    CPU_INT08U     ep_nbr;
    CPU_INT08U     ep_phy_nbr;
    USBD_DEV_SPD   dev_spd;
    CPU_BOOLEAN    alloc;
    CPU_SR_ALLOC();


                                                                /* --------------- GET OBJECT REFERENCES -------------- */
    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
       *p_err = USBD_ERR_DEV_INVALID_NBR;
        return (USBD_EP_NBR_NONE);
    }

    p_cfg = USBD_CfgRefGet(p_dev, cfg_nbr);                     /* Get cfg struct.                                      */
    if (p_cfg == (USBD_CFG *)0) {
       *p_err = USBD_ERR_CFG_INVALID_NBR;
        return (USBD_EP_NBR_NONE);
    }

    p_if = USBD_IF_RefGet(p_cfg, if_nbr);                       /* Get IF struct.                                       */
    if (p_if == (USBD_IF *)0) {
       *p_err = USBD_ERR_IF_INVALID_NBR;
        return (USBD_EP_NBR_NONE);
    }

    p_if_alt = USBD_IF_AltRefGet(p_if, if_alt_nbr);             /* Get IF alt setting struct.                           */
    if (p_if_alt == (USBD_IF_ALT *)0) {
       *p_err = USBD_ERR_IF_ALT_INVALID_NBR;
        return (USBD_EP_NBR_NONE);
    }

    if (p_if_alt->EP_NbrTotal >= USBD_CFG_MAX_NBR_EP_OPEN) {
       *p_err = USBD_ERR_EP_NONE_AVAIL;
        return (USBD_EP_NBR_NONE);
    }

    CPU_CRITICAL_ENTER();
    ep_nbr = USBD_EP_InfoNbrNext;
    if (ep_nbr >= USBD_CFG_MAX_NBR_EP_DESC) {                  /* Chk if EP is avail.                                  */
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_EP_ALLOC;
        return (USBD_EP_NBR_NONE);
    }
    USBD_EP_InfoNbrNext++;
    CPU_CRITICAL_EXIT();

    ep_type = attrib & USBD_EP_TYPE_MASK;
#if (USBD_CFG_HS_EN == DEF_ENABLED)
    if (DEF_BIT_IS_SET(cfg_nbr, USBD_CFG_NBR_SPD_BIT) == DEF_YES) {
        dev_spd = USBD_DEV_SPD_HIGH;
    } else {
#endif
        dev_spd = USBD_DEV_SPD_FULL;
#if (USBD_CFG_HS_EN == DEF_ENABLED)
    }
#endif

    p_ep              = &USBD_EP_InfoTbl[ep_nbr];
    p_ep->Interval    =  interval;
    p_ep->Attrib      =  attrib;
    p_ep->SyncAddr    =  0u;                                    /* Dflt sync addr is zero.                              */
    p_ep->SyncRefresh =  0u;                                    /* Dflt feedback rate exponent is zero.                 */

    CPU_CRITICAL_ENTER();
    ep_alloc_map  = p_cfg->EP_AllocMap;                         /* Get cfg EP alloc bit map.                            */
    DEF_BIT_CLR(ep_alloc_map, p_if->EP_AllocMap);               /* Clr EP already alloc'd in the IF.                    */
    DEF_BIT_SET(ep_alloc_map, p_if_alt->EP_AllocMap);

    ep_alloc_map_clr = ep_alloc_map;

    alloc = USBD_EP_Alloc(p_dev,                                /* Alloc physical EP.                                   */
                          dev_spd,
                          ep_type,
                          dir_in,
                          max_pkt_len & 0x7FF,                  /* Mask out transactions per microframe.                */
                          if_alt_nbr,
                          p_ep,
                         &ep_alloc_map);
    if (alloc != DEF_OK) {
        USBD_EP_InfoNbrNext--;
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_EP_NONE_AVAIL;
        return (USBD_EP_NBR_NONE);
    }

    p_ep->MaxPktSize |= max_pkt_len & 0x1800;                   /* Set transactions per microframe.                     */

    ep_phy_nbr = USBD_EP_ADDR_TO_PHY(p_ep->Addr);
    ep_phy_nbr++;

    if (p_dev->EP_MaxPhyNbr < ep_phy_nbr) {
        p_dev->EP_MaxPhyNbr = ep_phy_nbr;
    }

    p_if_alt->EP_AllocMap |= ep_alloc_map & ~ep_alloc_map_clr;
    p_if->EP_AllocMap     |= p_if_alt->EP_AllocMap;
    p_cfg->EP_AllocMap    |= p_if->EP_AllocMap;

    p_if_alt->EP_NbrTotal++;

#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)
    ep_nbr                       = USBD_EP_ADDR_TO_PHY(p_ep->Addr);
    p_if_alt->EP_TblPtrs[ep_nbr] = p_ep;
    DEF_BIT_SET(p_if_alt->EP_TblMap, DEF_BIT32(ep_nbr));
#else
    p_ep->NextPtr = (USBD_EP_INFO *)0;
    if (p_if_alt->EP_HeadPtr == (USBD_EP_INFO *)0) {
        p_if_alt->EP_HeadPtr = p_ep;
        p_if_alt->EP_TailPtr = p_ep;
    } else {
        p_if_alt->EP_TailPtr->NextPtr = p_ep;
        p_if_alt->EP_TailPtr          = p_ep;
    }
#endif
    CPU_CRITICAL_EXIT();

   *p_err = USBD_ERR_NONE;

    return (p_ep->Addr);
}


/*
*********************************************************************************************************
*                                           USBD_EP_Alloc()
*
* Description : Allocate a physical endpoint from the device controller.
*
* Argument(s) : p_dev               Pointer to USB device.
*               -----               Argument validated in 'USBD_DevAdd()' & 'USBD_EP_Add()'
*
*               spd                 Endpoint speed.
*
*                                       USBD_DEV_SPD_FULL   Endpoint is full-speed.
*                                       USBD_DEV_SPD_HIGH   Endpoint is high-speed.
*
*               type                Endpoint type.
*                                       USBD_EP_TYPE_CTRL  Control endpoint.
*                                       USBD_EP_TYPE_ISOC  Isochronous endpoint.
*                                       USBD_EP_TYPE_BULK  Bulk endpoint.
*                                       USBD_EP_TYPE_INTR  Interrupt endpoint.
*
*               dir_in              Endpoint direction.
*                                       DEF_YES  IN  endpoint.
*                                       DEF_NO   OUT endpoint.
*
*               max_pkt_len         Endpoint maximum packet size length.
*
*               if_alt_nbr          Alternate interface number containing the endpoint.
*
*               p_ep                Pointer to variable that will receive the endpoint parameters.
*               ----                Argument validated in 'USBD_DevAdd()' & 'USBD_EP_Add()'
*
*               p_alloc_bit_map     Pointer to allocation table bit-map.
*               ---------------     Argument validated in 'USBD_DevAdd()' & 'USBD_EP_Add()'
*
* Return(s)   : none.
*
* Note(s)     : (1) 'Universal Serial Bus Specification, Revision 2.0, April 27, 2000' Section 5.5.3
*
*                   "An endpoint for control transfers specifies the maximum data payload size that
*                    the endpoint can accept from or transmit to the bus. The allowable maximum control
*                    transfer data payload sizes for full-speed devices is 8, 16, 32, or 64 bytes; for
*                    high-speed devices, it is 64 bytes and for low-speed devices, it is 8 bytes."
*
*                   "All Host Controllers are required to have support for 8-, 16-, 32-, and 64-byte
*                    maximum data payload sizes for full-speed control endpoints, only 8-byte maximum
*                    data payload sizes for low-speed control endpoints, and only 64-byte maximum data
*                    payload size for high-speed control endpoints"
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_EP_Alloc (USBD_DEV      *p_dev,
                                    USBD_DEV_SPD   spd,
                                    CPU_INT08U     type,
                                    CPU_BOOLEAN    dir_in,
                                    CPU_INT16U     max_pkt_len,
                                    CPU_INT08U     if_alt_nbr,
                                    USBD_EP_INFO  *p_ep,
                                    CPU_INT32U    *p_alloc_bit_map)
{
    USBD_DRV_EP_INFO  *p_ep_tbl;
    USBD_DRV          *p_drv;
    CPU_INT08U         ep_tbl_ix;
    CPU_INT08U         ep_attrib;
    CPU_INT08U         ep_attrib_srch;
    CPU_INT08U         ep_max_pkt_bits;
    CPU_INT16U         ep_max_pkt;
    CPU_BOOLEAN        ep_found;


#if (USBD_CFG_HS_EN == DEF_DISABLED)
    (void)spd;
    (void)if_alt_nbr;
#endif

    if (dir_in == DEF_YES) {
        ep_attrib_srch = USBD_EP_INFO_DIR_IN;
    } else {
        ep_attrib_srch = USBD_EP_INFO_DIR_OUT;
    }

    switch (type) {
        case USBD_EP_TYPE_CTRL:
             DEF_BIT_SET(ep_attrib_srch, USBD_EP_INFO_TYPE_CTRL);
             break;

        case USBD_EP_TYPE_ISOC:
             DEF_BIT_SET(ep_attrib_srch, USBD_EP_INFO_TYPE_ISOC);
             break;

        case USBD_EP_TYPE_BULK:
             DEF_BIT_SET(ep_attrib_srch, USBD_EP_INFO_TYPE_BULK);
             break;

        case USBD_EP_TYPE_INTR:
             DEF_BIT_SET(ep_attrib_srch, USBD_EP_INFO_TYPE_INTR);
             break;

        default:
              return (DEF_FAIL);
    }

    p_drv     = &p_dev->Drv;
    p_ep_tbl  =  p_drv->CfgPtr->EP_InfoTbl;                     /* Get ctrl EP info tbl.                                */
    ep_attrib =  p_ep_tbl->Attrib;                              /* Get attrib for first entry.                          */
    ep_tbl_ix =  0u;
    ep_found  =  DEF_NO;

    while ((ep_attrib != DEF_BIT_NONE) &&                       /* Search until last entry or EP found.                 */
           (ep_found  == DEF_NO)) {
                                                                /* Chk if EP not alloc'd and EP attrib match req'd ...  */
                                                                /* ... attrib.                                          */
        if ((DEF_BIT_IS_CLR(*p_alloc_bit_map, DEF_BIT32(ep_tbl_ix)) == DEF_YES) &&
            (DEF_BIT_IS_SET(ep_attrib, ep_attrib_srch)              == DEF_YES)) {

            ep_max_pkt = p_ep_tbl[ep_tbl_ix].MaxPktSize;

            switch (type) {
                case USBD_EP_TYPE_CTRL:                         /* Chk ctrl transfer pkt size constrains.               */
                     ep_max_pkt      = DEF_MIN(ep_max_pkt, 64u);
                                                                /* Get next power of 2.                                 */
                     ep_max_pkt_bits = (CPU_INT08U)(31u - CPU_CntLeadZeros32(ep_max_pkt));
                     ep_max_pkt      = DEF_BIT16(ep_max_pkt_bits);
#if (USBD_CFG_HS_EN == DEF_ENABLED)
                     if ((spd        == USBD_DEV_SPD_HIGH) &&
                         (ep_max_pkt != 64u)) {
                         break;
                     }

                     if ((spd        == USBD_DEV_SPD_HIGH) &&
                         (ep_max_pkt <  8u)) {
                         break;
                     }
#endif
                     ep_found = DEF_YES;
                     break;

                case USBD_EP_TYPE_BULK:
#if (USBD_CFG_HS_EN == DEF_ENABLED)
                                                                /* Max pkt size is 512 for bulk EP in HS.               */
                     ep_max_pkt      = DEF_MIN(ep_max_pkt, 512u);
                     if ((spd        == USBD_DEV_SPD_HIGH) &&
                         (ep_max_pkt == 512u)) {
                         ep_found = DEF_YES;
                         break;
                     }
#endif
                                                                /* Max pkt size is 64 for bulk EP in FS.                */
                     ep_max_pkt      = DEF_MIN(ep_max_pkt, 64u);
                     ep_max_pkt_bits = (CPU_INT08U)(31u - CPU_CntLeadZeros32(ep_max_pkt));
                     ep_max_pkt      = DEF_BIT16(ep_max_pkt_bits);
#if (USBD_CFG_HS_EN == DEF_ENABLED)
                     if ((spd        == USBD_DEV_SPD_HIGH) &&
                         (ep_max_pkt >= 8u)) {
                          break;
                     }
#endif
                     ep_found = DEF_YES;
                     break;

                 case USBD_EP_TYPE_ISOC:
#if (USBD_CFG_HS_EN == DEF_ENABLED)
                      if (spd == USBD_DEV_SPD_HIGH) {
                          ep_max_pkt = DEF_MIN(ep_max_pkt, 1024u);
                      } else {
#endif
                          ep_max_pkt = DEF_MIN(ep_max_pkt, 1023u);
#if (USBD_CFG_HS_EN == DEF_ENABLED)
                      }
#endif

                      if (max_pkt_len > 0u) {
                          ep_max_pkt = DEF_MIN(ep_max_pkt, max_pkt_len);
                      }

                      ep_found = DEF_YES;
                      break;

                 case USBD_EP_TYPE_INTR:
#if (USBD_CFG_HS_EN == DEF_ENABLED)
                      if ((spd        == USBD_DEV_SPD_HIGH) &&
                          (if_alt_nbr != 0u)) {                 /* Dflt IF intr EP max pkt size limited to 64.          */
                          ep_max_pkt = DEF_MIN(ep_max_pkt, 1024u);
                      } else {
#endif
                          ep_max_pkt = DEF_MIN(ep_max_pkt, 64u);
#if (USBD_CFG_HS_EN == DEF_ENABLED)
                      }
#endif
                      if (max_pkt_len > 0u) {
                          ep_max_pkt = DEF_MIN(ep_max_pkt, max_pkt_len);
                      }

                      ep_found = DEF_YES;
                      break;

                 default:
                       return (DEF_FAIL);
            }

            if ((ep_found    == DEF_YES)    &&
               ((max_pkt_len == ep_max_pkt) ||
                (max_pkt_len == 0u))) {
                p_ep->MaxPktSize = ep_max_pkt;
                DEF_BIT_SET(*p_alloc_bit_map, DEF_BIT32(ep_tbl_ix));
                p_ep->Addr = p_ep_tbl[ep_tbl_ix].Nbr;
                if (dir_in == DEF_TRUE) {
                    p_ep->Addr |= USBD_EP_DIR_IN;               /* Add dir bit (IN EP).                                 */
                }
            } else {
                ep_found = DEF_NO;
                ep_tbl_ix++;
                ep_attrib = p_ep_tbl[ep_tbl_ix].Attrib;
            }
        } else {
            ep_tbl_ix++;
            ep_attrib = p_ep_tbl[ep_tbl_ix].Attrib;
        }
    }

    if (ep_found == DEF_NO) {
        return (DEF_FAIL);
    }

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                       USBD_EP_MaxPhyNbrGet()
*
* Description : Get the maximum physical endpoint number.
*
* Argument(s) : dev_nbr     Device number.
*
* Return(s)   : Maximum physical endpoint number, if NO error(s).
*
*               USBD_EP_PHY_NONE,                 otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  USBD_EP_MaxPhyNbrGet (CPU_INT08U  dev_nbr)
{
    USBD_DEV    *p_dev;
    CPU_INT08U   ep_phy_nbr;


    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */

    if (p_dev == (USBD_DEV *)0) {
        return (USBD_EP_PHY_NONE);
    }

    if (p_dev->EP_MaxPhyNbr == 0u) {
        ep_phy_nbr = USBD_EP_PHY_NONE;
    } else {
        ep_phy_nbr = p_dev->EP_MaxPhyNbr - 1u;
    }

    return (ep_phy_nbr);
}


/*
*********************************************************************************************************
*                                        USBD_EventConn()
*                                        USBD_EventDisconn()
*                                        USBD_EventReset()
*                                        USBD_EventHS()
*                                        USBD_EventSuspend()
*                                        USBD_EventResume()
*
* Description : Notify USB bus events to the device stack.
*
* Argument(s) : p_drv       Pointer to device driver.
*
* Return(s)   : none.
*
* Note(s)     : (1) USB device driver should call the following functions for each bus related events:
*
*                   (a) Bus connection      USBD_EventConn().
*                   (b) Bus disconnection   USBD_EventDisconn().
*                   (c) Bus reset           USBD_EventReset().
*                   (d) Bus HS detection    USBD_EventHS().
*                   (e) Bus suspend         USBD_EventSuspend().
*                   (f) Bus resume          USBD_EventResume().
*********************************************************************************************************
*/

void  USBD_EventConn (USBD_DRV  *p_drv)
{
    USBD_EventSet(p_drv, USBD_EVENT_BUS_CONN);
}


void  USBD_EventDisconn (USBD_DRV  *p_drv)
{
    USBD_EventSet(p_drv, USBD_EVENT_BUS_DISCONN);
}


void  USBD_EventHS (USBD_DRV  *p_drv)
{
    USBD_EventSet(p_drv, USBD_EVENT_BUS_HS);
}


void  USBD_EventReset (USBD_DRV  *p_drv)
{
    USBD_EventSet(p_drv, USBD_EVENT_BUS_RESET);
}

void  USBD_EventSuspend (USBD_DRV  *p_drv)
{
    USBD_EventSet(p_drv, USBD_EVENT_BUS_SUSPEND);
}

void  USBD_EventResume (USBD_DRV  *p_drv)
{
    USBD_EventSet(p_drv, USBD_EVENT_BUS_RESUME);
}


/*
*********************************************************************************************************
*                                          USBD_EventSetup()
*
* Description : Send a USB setup event to the core task.
*
* Argument(s) : p_drv       Pointer to device driver.
*
*               p_buf       Pointer to the setup packet.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_EventSetup (USBD_DRV  *p_drv,
                       void      *p_buf)
{
    USBD_DEV         *p_dev;
    USBD_CORE_EVENT  *p_core_event;
    CPU_INT08U       *p_buf_08;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_buf == (void *)0) {
        return;
    }
#endif

    p_dev = USBD_DevRefGet(p_drv->DevNbr);                      /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
        return;
    }

    p_core_event = USBD_CoreEventGet();                         /* Get core event struct.                                */
    if (p_core_event == (USBD_CORE_EVENT *)0) {
        return;
    }

    USBD_DBG_CORE_STD("Setup Pkt");

    p_buf_08                          = (CPU_INT08U *)p_buf;
    p_dev->SetupReqNext.bmRequestType =  p_buf_08[0u];
    p_dev->SetupReqNext.bRequest      =  p_buf_08[1u];
    p_dev->SetupReqNext.wValue        =  MEM_VAL_GET_INT16U_LITTLE(p_buf_08 + 2u);
    p_dev->SetupReqNext.wIndex        =  MEM_VAL_GET_INT16U_LITTLE(p_buf_08 + 4u);
    p_dev->SetupReqNext.wLength       =  MEM_VAL_GET_INT16U_LITTLE(p_buf_08 + 6u);

    p_core_event->Type                =  USBD_EVENT_SETUP;
    p_core_event->DrvPtr              =  p_drv;
    p_core_event->Err                 =  USBD_ERR_NONE;

    USBD_OS_CoreEventPut(p_core_event);
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         INTERNAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           USBD_EventEP()
*
* Description : Send a USB endpoint event to the core task.
*
* Argument(s) : p_drv       Pointer to device driver.
*
*               ep_addr     Endpoint address.
*
*               err         Error code returned by the USB device driver.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_EventEP (USBD_DRV    *p_drv,
                    CPU_INT08U   ep_addr,
                    USBD_ERR     err)
{
    USBD_CORE_EVENT  *p_core_event;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_drv == (USBD_DRV *)0) {
        return;
    }
#endif

    p_core_event = USBD_CoreEventGet();                         /* Get core event struct.                               */
    if (p_core_event == (USBD_CORE_EVENT *)0) {
        return;
    }

    p_core_event->Type    = USBD_EVENT_EP;
    p_core_event->DrvPtr  = p_drv;
    p_core_event->EP_Addr = ep_addr;
    p_core_event->Err     = err;

    USBD_OS_CoreEventPut(p_core_event);                         /* Queue core event.                                    */
}


/*
*********************************************************************************************************
*                                           USBD_DrvRefGet()
*
* Description : Get a reference to the device driver structure.
*
* Argument(s) : dev_nbr     Device number
*
* Return(s)   : Pointer to device driver structure, if NO error(s).
*
*               Pointer to NULL                   , otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

USBD_DRV  *USBD_DrvRefGet (CPU_INT08U  dev_nbr)
{
    USBD_DEV  *p_dev;
    USBD_DRV  *p_drv;


    p_dev = USBD_DevRefGet(dev_nbr);                            /* Get dev struct.                                      */
    if (p_dev == (USBD_DEV *)0) {
        return ((USBD_DRV *)0);
    }

    p_drv = &p_dev->Drv;

    return (p_drv);
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
*                                        USBD_StdReqHandler()
*
* Description : Standard request process.
*
* Argument(s) : p_dev       Pointer to USB device.
*               -----       Argument validated in 'USBD_DevSetupPkt()' before posting the event to queue.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_StdReqHandler (USBD_DEV  *p_dev)
{
    CPU_INT08U   recipient;
    CPU_INT08U   type;
    CPU_INT08U   request;
    CPU_BOOLEAN  valid;
    CPU_BOOLEAN  dev_to_host;
    USBD_ERR     err;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();                                       /* Copy setup request.                                 */
    p_dev->SetupReq.bmRequestType = p_dev->SetupReqNext.bmRequestType;
    p_dev->SetupReq.bRequest      = p_dev->SetupReqNext.bRequest;
    p_dev->SetupReq.wValue        = p_dev->SetupReqNext.wValue;
    p_dev->SetupReq.wIndex        = p_dev->SetupReqNext.wIndex;
    p_dev->SetupReq.wLength       = p_dev->SetupReqNext.wLength;
    CPU_CRITICAL_EXIT();


    recipient   = p_dev->SetupReq.bmRequestType & USBD_REQ_RECIPIENT_MASK;
    type        = p_dev->SetupReq.bmRequestType & USBD_REQ_TYPE_MASK;
    request     = p_dev->SetupReq.bRequest;
    dev_to_host = DEF_BIT_IS_SET(p_dev->SetupReq.bmRequestType, USBD_REQ_DIR_BIT);
    valid       = DEF_FAIL;

    switch (type) {
        case USBD_REQ_TYPE_STANDARD:
             switch (recipient) {                               /* Select req recipient:                                */
                 case USBD_REQ_RECIPIENT_DEVICE:                /*    Device.                                           */
                      valid = USBD_StdReqDev(p_dev, request);
                      break;

                 case USBD_REQ_RECIPIENT_INTERFACE:             /*    Interface.                                        */
                      valid = USBD_StdReqIF(p_dev, request);
                      break;

                 case USBD_REQ_RECIPIENT_ENDPOINT:              /*    Endpoint.                                         */
                      valid = USBD_StdReqEP(p_dev, request);
                      break;

                 case USBD_REQ_RECIPIENT_OTHER:                 /*    Not supported.                                    */
                 default:
                      break;
             }
             break;


        case USBD_REQ_TYPE_CLASS:                               /* Class-specific req.                                  */
             switch (recipient) {
                 case USBD_REQ_RECIPIENT_INTERFACE:
                 case USBD_REQ_RECIPIENT_ENDPOINT:
                      valid = USBD_StdReqClass(p_dev);          /* Class-specific req.                                  */
                      break;


                 case USBD_REQ_RECIPIENT_DEVICE:
                 case USBD_REQ_RECIPIENT_OTHER:
                 default:
                      break;
             }
             break;


        case USBD_REQ_TYPE_VENDOR:
             switch (recipient) {
                 case USBD_REQ_RECIPIENT_INTERFACE:
#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
                      if (request == p_dev->StrMS_VendorCode) {
                          err                  =  USBD_ERR_NONE;
                          p_dev->DescBufErrPtr = &err;
                          valid = USBD_StdReqIF_MS(p_dev);      /* Microsoft OS descriptor req.                         */
                          p_dev->DescBufErrPtr = (USBD_ERR *)0;
                      } else {
                          valid = USBD_StdReqVendor(p_dev);     /* Vendor-specific req.                                 */
                      }
                      break;
#endif
                 case USBD_REQ_RECIPIENT_ENDPOINT:
                      valid = USBD_StdReqVendor(p_dev);         /* Vendor-specific req.                                 */
                      break;

                 case USBD_REQ_RECIPIENT_DEVICE:
#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
                      if (request == p_dev->StrMS_VendorCode) {
                          err                  =  USBD_ERR_NONE;
                          p_dev->DescBufErrPtr = &err;
                          valid = USBD_StdReqDevMS(p_dev);      /* Microsoft OS descriptor req.                         */
                          p_dev->DescBufErrPtr = (USBD_ERR *)0;
                      }
#endif
                      break;

                 case USBD_REQ_RECIPIENT_OTHER:
                 default:
                      break;
             }
             break;


        case USBD_REQ_TYPE_RESERVED:
        default:
             break;
    }

    if (valid == DEF_FAIL) {
        USBD_DBG_CORE_STD("Request Error");
        USBD_CtrlStall(p_dev->Nbr, &err);

    } else {

        if (dev_to_host == DEF_YES)  {
            USBD_DBG_CORE_STD("Rx Status");
            USBD_CtrlRxStatus(p_dev->Nbr, USBD_CFG_CTRL_REQ_TIMEOUT_mS, &err);
        } else {
            USBD_DBG_CORE_STD("Tx Status");
            USBD_CtrlTxStatus(p_dev->Nbr, USBD_CFG_CTRL_REQ_TIMEOUT_mS, &err);

            if ((type                       ==  USBD_REQ_TYPE_STANDARD)    &&
                (recipient                  ==  USBD_REQ_RECIPIENT_DEVICE) &&
                (request                    ==  USBD_REQ_SET_ADDRESS)      &&
                (p_dev->Drv.API_Ptr->AddrEn != (void*)0)) {
                p_dev->Drv.API_Ptr->AddrEn(&p_dev->Drv, p_dev->Addr);
            }
        }
    }
}


/*
*********************************************************************************************************
*                                          USBD_StdReqDev()
*
* Description : Process device standard request.
*
* Argument(s) : p_dev       Pointer to USB device.
*               -----       Argument validated in 'USBD_DevSetupPkt()' before posting the event to queue.
*
*               request     USB device request.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) USB Spec 2.0, section 9.4.6 specifies the format of the SET_ADDRESS request. The
*                   SET_ADDRESS sets the device address for all future device access.
*
*                   (a) The 'wValue' filed specify the device address to use for all subsequent accesses.
*
*                   (b) If the specified device address is greater than 127 or if 'wIndex' or 'wLength'
*                       are non-zero, the behavior of the device is not specified.
*
*                   (c) If the device is in the default state and the address specified is non-zero,
*                       the device shall enter the device address, otherwise the device remains in the
*                       default state' (this is not an error condition).
*
*                   (d) If the device is in the address state and the address specified is zero, then
*                       the device shall enter the default state otherwise, the device remains in
*                       the address state but uses the newly-specified address.
*
*                   (e) Device behavior when the SET_ADDRESS request is received while the device is not
*                       in the default or address state is not specified.
*
*                   (f) USB Spec 2.0, section 9.2.6.3 specifies the maximum timeout for the SET_ADDRESS
*                       request:
*
*                       "After the reset/resume recovery interval, if a device receives a SetAddress()
*                        request, the device must be able to complete processing of the request and be
*                        able to successfully complete the Status stage of the request within 50 ms. In
*                        the case of the SetAddress() request, the Status stage successfully completes
*                        when the device sends the zero-length Status packet or when the device sees
*                        the ACK in response to the Status stage data packet."
*
*               (2) USB Spec 2.0, section 9.4.7 specifies the format of the SET_CONFIGURATION request.
*
*                   (a) The lower byte of 'wValue' field specifies the desired configuration.
*
*                   (b) If 'wIndex', 'wLength', or the upper byte of wValue is non-zero, then the behavior
*                       of this request is not specified.
*
*                   (c) The configuration value must be zero or match a configuration value from a
*                       configuration value from a configuration descriptor. If the configuration value
*                       is zero, the device is place in its address state.
*
*                   (d) Device behavior when this request is received while the device is in the Default
*                       state is not specified.
*
*                   (e) If device is in address state and the specified configuration value is zero,
*                       then the device remains in the Address state. If the specified configuration value
*                       matches the configuration value from a configuration descriptor, then that
*                       configuration is selected and the device enters the Configured state. Otherwise,
*                       the device responds with a Request Error.
*
*                   (f) If the specified configuration value is zero, then the device enters the Address
*                       state. If the specified configuration value matches the configuration value from a
*                       configuration descriptor, then that configuration is selected and the device
*                       remains in the Configured state. Otherwise, the device responds with a Request
*                       Error.
*
*               (3) USB Spec 2.0, section 9.4.2 specifies the format of the GET_CONFIGURATION request.
*
*                   (a) If 'wValue' or 'wIndex' are non-zero or 'wLength' is not '1', then the device
*                       behavior is not specified.
*
*                   (b) If the device is in default state, the device behavior is not specified.
*
*                   (c) In address state a value of zero MUST be returned.
*
*                   (d) In configured state, the non-zero bConfigurationValue of the current configuration
*                       must be returned.
*
*               (4) USB Spec 2.0, section 9.4.5 specifies the format of the GET_STATUS request.
*
*                  (a) If 'wValue' is non-zero or 'wLength is not equal to '2', or if wIndex is non-zero
*                      then the behavior of the device is not specified.
*
*                  (b) USB Spec 2, 0, figure 9-4 shows the format of information returned by the device
*                      for a GET_STATUS request.
*
*                      +====|====|====|====|====|====|====|========|=========+
*                      | D0 | D1 | D2 | D3 | D4 | D3 | D2 |   D1   |    D0   |
*                      |----------------------------------|--------|---------|
*                      |     RESERVED (RESET TO ZERO)     | Remote |   Self  |
*                      |                                  | Wakeup | Powered |
*                      +==================================|========|=========+
*
*                      (1) The Self Powered field indicates whether the device is currently self-powered.
*                          If D0 is reset to zero, the device is bus-powered. If D0 is set to one, the
*                          device is self-powered. The Self Powered field may not be changed by the
*                          SetFeature() or ClearFeature() requests.
*
*                      (2) The Remote Wakeup field indicates whether the device is currently enabled to
*                          request remote wakeup. The default mode for devices that support remote wakeup
*                          is disabled. If D1 is reset to zero, the ability of the device to signal
*                          remote wakeup is disabled. If D1 is set to one, the ability of the device to
*                          signal remote wakeup is enabled. The Remote Wakeup field can be modified by
*                          the SetFeature() and ClearFeature() requests using the DEVICE_REMOTE_WAKEUP
*                          feature selector. This field is reset to zero when the device is reset.
*
*               (5) USB Spec 2.0, section 9.4.1/9.4.9 specifies the format of the CLEAR_FEATURE/SET_FEATURE
*                   request.
*
*                   (a) If 'wLength' or 'wIndex' are non-zero, then the device behavior is not specified.
*
*                   (b) The device CLEAR_FEATURE request is only valid when the device is in the
*                       configured state.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_StdReqDev (USBD_DEV    *p_dev,
                                     CPU_INT08U   request)
{
    USBD_DRV      *p_drv;
    USBD_DRV_API  *p_drv_api;
    CPU_INT08U     cfg_nbr;
    CPU_BOOLEAN    valid;
    CPU_BOOLEAN    addr_set;
    CPU_BOOLEAN    dev_to_host;
    CPU_INT08U     dev_addr;
    USBD_ERR       err;
    CPU_SR_ALLOC();


    USBD_DBG_STATS_DEV_INC(p_dev->Nbr, StdReqDevNbr);

    dev_to_host = DEF_BIT_IS_SET(p_dev->SetupReq.bmRequestType, USBD_REQ_DIR_BIT);
    valid       = DEF_FAIL;

    switch (request) {
        case USBD_REQ_GET_DESCRIPTOR:                           /* ------------------ GET DESCRIPTOR ------------------ */
             if (dev_to_host != DEF_YES) {
                 break;
             }

             valid = USBD_StdReqDescGet(p_dev);
             break;


        case USBD_REQ_SET_ADDRESS:                              /* -------------------- SET ADDRESS ------------------- */
             if (dev_to_host != DEF_NO) {
                 break;
             }

             USBD_DBG_STATS_DEV_INC(p_dev->Nbr, StdReqSetAddrNbr);

             dev_addr = (CPU_INT08U)(p_dev->SetupReq.wValue &   /* Get dev addr (see Note #1a).                         */
                                     DEF_INT_08_MASK);

             USBD_DBG_CORE_STD_ARG("  Set Address", dev_addr);

             if ((dev_addr                >  127u) ||           /* Validate request values. (see Note #1b).             */
                 (p_dev->SetupReq.wIndex  !=   0u) ||
                 (p_dev->SetupReq.wLength !=   0u)) {
                  break;
             }

             p_drv     = &p_dev->Drv;
             p_drv_api =  p_dev->Drv.API_Ptr;
             switch (p_dev->State) {
                 case USBD_DEV_STATE_DEFAULT:
                      if (dev_addr > 0u) {                      /* See Note #1c.                                        */
                          if (p_drv_api->AddrSet != (void*)0) {
                              addr_set = p_drv_api->AddrSet(p_drv, dev_addr);
                              if (addr_set == DEF_FAIL) {
                                  USBD_DBG_CORE_STD_ERR("  Set Address", USBD_ERR_FAIL);
                                  break;
                              }
                          }

                          CPU_CRITICAL_ENTER();                 /* Set dev in addressed state.                          */
                          p_dev->Addr  = dev_addr;
                          p_dev->State = USBD_DEV_STATE_ADDRESSED;
                          CPU_CRITICAL_EXIT();

                          valid = DEF_OK;
                      }
                      break;


                 case USBD_DEV_STATE_ADDRESSED:                 /* See Note #1c.                                        */
                      if (dev_addr == 0u) {                     /* If dev addr is zero ...                              */
                                                                /* ... set addr in dev drv.                             */
                          if (p_drv_api->AddrSet != (void*)0) {
                              addr_set = p_drv_api->AddrSet(p_drv, 0u);
                              if (addr_set == DEF_FAIL) {
                                  USBD_DBG_CORE_STD_ERR("  Set Address", USBD_ERR_FAIL);
                                  break;
                              }
                          }

                          CPU_CRITICAL_ENTER();                 /* Dev enters default state.                            */
                          p_dev->Addr  = 0u;
                          p_dev->State = USBD_DEV_STATE_DEFAULT;
                          CPU_CRITICAL_EXIT();

                          valid = DEF_OK;

                      } else {
                                                                /* ... remains in addressed state and set new addr.     */
                          if (p_drv_api->AddrSet != (void*)0) {
                              addr_set = p_drv_api->AddrSet(p_drv, dev_addr);
                              if (addr_set == DEF_FAIL) {
                                  USBD_DBG_CORE_STD_ERR("  Set Address", USBD_ERR_FAIL);
                                  break;
                              }
                          }

                          CPU_CRITICAL_ENTER();
                          p_dev->Addr = dev_addr;
                          CPU_CRITICAL_EXIT();

                          valid = DEF_OK;
                      }
                      break;


                 case USBD_DEV_STATE_NONE:
                 case USBD_DEV_STATE_INIT:
                 case USBD_DEV_STATE_ATTACHED:
                 case USBD_DEV_STATE_CONFIGURED:
                 case USBD_DEV_STATE_SUSPENDED:
                 default:
                      USBD_DBG_CORE_STD_ERR("  Set Address", USBD_ERR_DEV_INVALID_STATE);
                      break;
             }
             break;


        case USBD_REQ_SET_CONFIGURATION:                        /* ----------------- SET CONFIGURATION ---------------- */
             if (dev_to_host != DEF_NO) {
                 break;
             }

             USBD_DBG_STATS_DEV_INC(p_dev->Nbr, StdReqSetCfgNbr);

             if (((p_dev->SetupReq.wValue & 0xFF00u) != 0u) &&  /* Validate request values (see Note #2b).              */
                  (p_dev->SetupReq.wIndex            != 0u) &&
                  (p_dev->SetupReq.wLength           != 0u)) {
                   break;
             }
                                                                /* Get cfg value.                                       */
             cfg_nbr = (CPU_INT08U)(p_dev->SetupReq.wValue & DEF_INT_08_MASK);
             USBD_DBG_CORE_STD_ARG("  Set Configuration", cfg_nbr);

#if (USBD_CFG_HS_EN == DEF_ENABLED)
                                                                /* Cfg value MUST exists.                               */
             if ((cfg_nbr    >  p_dev->CfgHS_TotalNbr) &&
                 (p_dev->Spd == USBD_DEV_SPD_HIGH)) {
                  USBD_DBG_CORE_STD_ERR("  Set Configuration", USBD_ERR_CFG_INVALID_NBR);
                  break;
             }
#endif

             if ((cfg_nbr    >  p_dev->CfgFS_TotalNbr) &&
                 (p_dev->Spd == USBD_DEV_SPD_FULL)) {
                  USBD_DBG_CORE_STD_ERR("  Set Configuration", USBD_ERR_CFG_INVALID_NBR);
                  break;
             }

             switch (p_dev->State) {
                 case USBD_DEV_STATE_ADDRESSED:                 /* See Note #2e.                                        */
                      if (cfg_nbr > 0u) {
                          USBD_CfgOpen(p_dev,                   /* Open cfg.                                            */
                                      (cfg_nbr - 1u),
                                      &err);
                          if (err != USBD_ERR_NONE) {
                              USBD_DBG_CORE_STD_ERR("  Set Configuration", err);
                              break;
                          }

                          valid = DEF_OK;
                      } else {
                          valid = DEF_OK;                       /* Remain in addressed state.                           */
                      }
                      break;


                 case USBD_DEV_STATE_CONFIGURED:                /* See Note #2f.                                        */
                      if (cfg_nbr > 0u) {
                          if (p_dev->CfgCurNbr == (cfg_nbr - 1u)) {
                              valid = DEF_OK;
                              break;
                          }

                          USBD_CfgClose(p_dev);                 /* Close curr  cfg.                                     */

                          USBD_CfgOpen(p_dev,                   /* Open cfg.                                            */
                                      (cfg_nbr - 1u),
                                      &err);
                          if (err != USBD_ERR_NONE) {
                              USBD_DBG_CORE_STD_ERR("  Set Configuration", err);
                              break;
                          }

                          valid = DEF_OK;

                      } else {
                          USBD_CfgClose(p_dev);                 /* Close curr cfg.                                      */

                          CPU_CRITICAL_ENTER();
                          p_dev->State = USBD_DEV_STATE_ADDRESSED;
                          CPU_CRITICAL_EXIT();

                          valid = DEF_OK;
                      }
                      break;


                 case USBD_DEV_STATE_NONE:
                 case USBD_DEV_STATE_INIT:
                 case USBD_DEV_STATE_ATTACHED:
                 case USBD_DEV_STATE_DEFAULT:
                 case USBD_DEV_STATE_SUSPENDED:
                 default:
                      USBD_DBG_CORE_STD_ERR("  Set Configuration", USBD_ERR_DEV_INVALID_STATE);
                      break;
             }
             break;


        case USBD_REQ_GET_CONFIGURATION:                        /* ----------------- GET CONFIGURATION ---------------- */
             if (dev_to_host != DEF_YES) {
                 break;
             }

             if ((p_dev->SetupReq.wLength != 1u) &&             /* Validate request values (see Note #3a).              */
                 (p_dev->SetupReq.wIndex  != 0u) &&
                 (p_dev->SetupReq.wValue  != 0u)) {
                  break;
             }

             switch (p_dev->State)  {
                 case USBD_DEV_STATE_ADDRESSED:                 /* See Note #3b.                                        */
                      cfg_nbr = 0u;
                      USBD_DBG_CORE_STD_ARG("  Get Configuration", cfg_nbr);

                      p_dev->CtrlStatusBufPtr[0u] = cfg_nbr;    /* Uses Ctrl status buf to follow USB mem alignment.    */

                      (void)USBD_CtrlTx(         p_dev->Nbr,
                                        (void *)&p_dev->CtrlStatusBufPtr[0u],
                                                 1u,
                                                 USBD_CFG_CTRL_REQ_TIMEOUT_mS,
                                                 DEF_NO,
                                                &err);
                      if (err != USBD_ERR_NONE) {
                          break;
                      }

                      valid = DEF_OK;
                      break;


                 case USBD_DEV_STATE_CONFIGURED:                /* See Note #3c.                                        */
                      if (p_dev->CfgCurPtr == (USBD_CFG *)0) {
                          break;
                      }

                      cfg_nbr = p_dev->CfgCurNbr + 1u;
                      USBD_DBG_CORE_STD_ARG("  Get Configuration", cfg_nbr);

                      p_dev->CtrlStatusBufPtr[0u] = cfg_nbr;    /* Uses Ctrl status buf to follow USB mem alignment.    */

                      (void)USBD_CtrlTx(         p_dev->Nbr,
                                        (void *)&p_dev->CtrlStatusBufPtr[0u],
                                                 1u,
                                                 USBD_CFG_CTRL_REQ_TIMEOUT_mS,
                                                 DEF_NO,
                                                &err);
                      if (err != USBD_ERR_NONE) {
                          break;
                      }

                      valid = DEF_OK;
                      break;


                 case USBD_DEV_STATE_NONE:
                 case USBD_DEV_STATE_INIT:
                 case USBD_DEV_STATE_ATTACHED:
                 case USBD_DEV_STATE_DEFAULT:
                 case USBD_DEV_STATE_SUSPENDED:
                 default:
                      USBD_DBG_CORE_STD_ERR("  Get Configuration", USBD_ERR_DEV_INVALID_STATE);
                      break;
             }
             break;


        case USBD_REQ_GET_STATUS:                               /* -------------------- GET STATUS -------------------- */
             if (dev_to_host != DEF_YES) {
                 break;
             }

             if ((p_dev->SetupReq.wLength != 2u) &&             /* Validate request values (see Note #4a).              */
                 (p_dev->SetupReq.wIndex  != 0u) &&
                 (p_dev->SetupReq.wValue  != 0u)) {
                  break;
             }

             USBD_DBG_CORE_STD("  Get Status (Device)");
             p_dev->CtrlStatusBufPtr[0u] = DEF_BIT_NONE;
             p_dev->CtrlStatusBufPtr[1u] = DEF_BIT_NONE;

             switch (p_dev->State) {
                 case USBD_DEV_STATE_ADDRESSED:                 /* See Note #4b.                                        */
                      if (p_dev->SelfPwr == DEF_YES) {
                          p_dev->CtrlStatusBufPtr[0u] |= DEF_BIT_00;
                      }
                      if (p_dev->RemoteWakeup == DEF_ENABLED) {
                          p_dev->CtrlStatusBufPtr[0u] |= DEF_BIT_01;
                      }

                      (void)USBD_CtrlTx(         p_dev->Nbr,
                                        (void *)&p_dev->CtrlStatusBufPtr[0u],
                                                 2u,
                                                 USBD_CFG_CTRL_REQ_TIMEOUT_mS,
                                                 DEF_NO,
                                                &err);
                      if (err != USBD_ERR_NONE) {
                          break;
                      }

                      valid = DEF_OK;
                      break;


                 case USBD_DEV_STATE_CONFIGURED:
                      if (p_dev->CfgCurPtr != (USBD_CFG *)0) {
                          if (DEF_BIT_IS_SET(p_dev->CfgCurPtr->Attrib, USBD_DEV_ATTRIB_SELF_POWERED)) {
                              p_dev->CtrlStatusBufPtr[0u] |= DEF_BIT_00;
                          }
                          if (DEF_BIT_IS_SET(p_dev->CfgCurPtr->Attrib, USBD_DEV_ATTRIB_REMOTE_WAKEUP)) {
                              p_dev->CtrlStatusBufPtr[0u] |= DEF_BIT_01;
                          }
                      }

                      (void)USBD_CtrlTx(         p_dev->Nbr,
                                        (void *)&p_dev->CtrlStatusBufPtr[0u],
                                                 2u,
                                                 USBD_CFG_CTRL_REQ_TIMEOUT_mS,
                                                 DEF_NO,
                                                &err);
                      if (err != USBD_ERR_NONE) {
                          break;
                      }

                      valid = DEF_OK;
                      break;


                  case USBD_DEV_STATE_NONE:
                  case USBD_DEV_STATE_INIT:
                  case USBD_DEV_STATE_ATTACHED:
                  case USBD_DEV_STATE_DEFAULT:
                  case USBD_DEV_STATE_SUSPENDED:
                  default:
                      USBD_DBG_CORE_STD_ERR("  Get Status (Device)", USBD_ERR_DEV_INVALID_STATE);
                      break;
             }
             break;


        case USBD_REQ_CLEAR_FEATURE:                            /* ----------------- SET/CLEAR FEATURE ---------------- */
        case USBD_REQ_SET_FEATURE:
             if (dev_to_host != DEF_NO) {
                 break;
             }

             if ((p_dev->SetupReq.wLength != 0u) &&             /* Validate request values.                             */
                 (p_dev->SetupReq.wIndex  != 0u)) {
                  break;
             }

             if (request == USBD_REQ_CLEAR_FEATURE) {
                 USBD_DBG_CORE_STD("  Clear Feature (Device)");
             } else {
                 USBD_DBG_CORE_STD("  Set Feature (Device)");
             }

             switch (p_dev->State) {
                 case USBD_DEV_STATE_CONFIGURED:
                      if (p_dev->CfgCurPtr == (USBD_CFG *)0) {
                          break;
                      }

                      if ((p_dev->SetupReq.wValue == USBD_FEATURE_SEL_DEVICE_REMOTE_WAKEUP) &&
                          (DEF_BIT_IS_SET(p_dev->CfgCurPtr->Attrib, USBD_DEV_ATTRIB_REMOTE_WAKEUP))) {
                          p_dev->RemoteWakeup = (request == USBD_REQ_CLEAR_FEATURE) ? DEF_DISABLED : DEF_ENABLED;
                      }

                      valid = DEF_OK;
                      break;


                 case USBD_DEV_STATE_NONE:
                 case USBD_DEV_STATE_INIT:
                 case USBD_DEV_STATE_ATTACHED:
                 case USBD_DEV_STATE_DEFAULT:
                 case USBD_DEV_STATE_ADDRESSED:
                 case USBD_DEV_STATE_SUSPENDED:
                 default:
                     if (request == USBD_REQ_CLEAR_FEATURE) {
                         USBD_DBG_CORE_STD_ERR("  Clear Feature (Device)", USBD_ERR_DEV_INVALID_STATE);
                     } else {
                         USBD_DBG_CORE_STD_ERR("  Set Feature (Device)", USBD_ERR_DEV_INVALID_STATE);
                     }
                     break;
             }
             break;


        default:
             break;
    }

    USBD_DBG_STATS_DEV_INC_IF_TRUE(p_dev->Nbr, StdReqDevStallNbr, (valid == DEF_FAIL));

    return (valid);
}


/*
*********************************************************************************************************
*                                           USBD_StdReqIF()
*
* Description : Process device standard request (Interface).
*
* Argument(s) : p_dev       Pointer to USB device.
*               -----       Argument validated in 'USBD_DevSetupPkt()' before posting the event to queue.
*
*               request     USB device request.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) USB Spec 2.0, section 9.4.10 specifies the format of the SET_INTERFACE request.
*
*                   This request allows the host to select an alternate setting for the specified
*                   interface:
*
*                   (a) Some USB devices have configurations with interfaces that have mutually
*                       exclusive settings.  This request allows the host to select the desired
*                       alternate setting.  If a device only supports a default setting for the
*                       specified interface, then a STALL may be returned in the Status stage of
*                       the request. This request cannot be used to change the set of configured
*                       interfaces (the SetConfiguration() request must be used instead).
*
*                (2) USB Spec 2.0, section 9.4.4 specifies the format of the GET_INTERFACE request.
*                    This request returns the selected alternate setting for the specified interface.
*
*                   (a) If 'wValue' is non-zero or 'wLength' is not '1', then the device behavior is
*                       not specified.
*
*                   (b) The GET_INTERFACE request is only valid when the device is in the configured
*                       state.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_StdReqIF (USBD_DEV    *p_dev,
                                    CPU_INT08U   request)
{
    USBD_CFG        *p_cfg;
    USBD_IF         *p_if;
    USBD_IF_ALT     *p_if_alt;
    USBD_CLASS_DRV  *p_class_drv;
    CPU_INT08U       if_nbr;
    CPU_INT08U       if_alt_nbr;
    CPU_BOOLEAN      valid;
    CPU_BOOLEAN      dev_to_host;
    CPU_INT16U       req_len;
    USBD_ERR         err;
    CPU_SR_ALLOC();


    USBD_DBG_STATS_DEV_INC(p_dev->Nbr, StdReqIF_Nbr);

    p_cfg  = p_dev->CfgCurPtr;
    if (p_cfg == (USBD_CFG *) 0) {
        USBD_DBG_STATS_DEV_INC(p_dev->Nbr, StdReqIF_StallNbr);
        return (DEF_FAIL);
    }

    if_nbr = (CPU_INT08U)(p_dev->SetupReq.wIndex & DEF_INT_08_MASK);
    p_if   =  USBD_IF_RefGet(p_cfg, if_nbr);
    if (p_if == (USBD_IF *)0) {
        USBD_DBG_STATS_DEV_INC(p_dev->Nbr, StdReqIF_StallNbr);
        return (DEF_FAIL);
    }

    dev_to_host = DEF_BIT_IS_SET(p_dev->SetupReq.bmRequestType, USBD_REQ_DIR_BIT);
    valid       = DEF_FAIL;

    switch (request) {
        case USBD_REQ_GET_STATUS:
             if (dev_to_host != DEF_YES) {
                 break;
             }

             USBD_DBG_CORE_STD_ARG("  Get Status (Interface) IF ", if_nbr);

             if ((p_dev->State != USBD_DEV_STATE_ADDRESSED) &&
                 (p_dev->State != USBD_DEV_STATE_CONFIGURED)) {
                 break;
             }

             if ((p_dev->State == USBD_DEV_STATE_ADDRESSED) &&
                 (if_nbr       != 0u)) {
                 break;
             }

             p_dev->CtrlStatusBufPtr[0u] = DEF_BIT_NONE;

             (void)USBD_CtrlTx(         p_dev->Nbr,
                               (void *)&p_dev->CtrlStatusBufPtr[0u],
                                        1u,
                                        USBD_CFG_CTRL_REQ_TIMEOUT_mS,
                                        DEF_NO,
                                       &err);
             if (err != USBD_ERR_NONE) {
                 break;
             }

             valid = DEF_OK;
             break;


        case USBD_REQ_CLEAR_FEATURE:
        case USBD_REQ_SET_FEATURE:
             if (dev_to_host != DEF_NO) {
                 break;
             }

             if (request == USBD_REQ_CLEAR_FEATURE) {
                 USBD_DBG_CORE_STD_ARG("  Clear Feature (Interface)", if_nbr);
             } else {
                 USBD_DBG_CORE_STD_ARG("  Set Feature (Interface)", if_nbr);
             }

             if ((p_dev->State != USBD_DEV_STATE_ADDRESSED) &&
                 (p_dev->State != USBD_DEV_STATE_CONFIGURED)) {
                  break;
             }

             if ((p_dev->State == USBD_DEV_STATE_ADDRESSED) &&
                 (if_nbr       != 0u)) {
                  break;
             }

             valid = DEF_OK;
             break;


        case USBD_REQ_GET_DESCRIPTOR:
             if (dev_to_host != DEF_YES) {
                 break;
             }

             USBD_DBG_CORE_STD_ARG("  Get Descriptor (Interface) IF ", if_nbr);

             p_class_drv = p_if->ClassDrvPtr;
             if (p_class_drv->IF_Req == (void *)0) {
                 break;
             }

             req_len = p_dev->SetupReq.wLength;
             USBD_DescWrStart(p_dev, req_len);

             err                  =  USBD_ERR_NONE;
             p_dev->DescBufErrPtr = &err;

             valid = p_class_drv->IF_Req(p_dev->Nbr,
                                        &p_dev->SetupReq,
                                         p_if->ClassArgPtr);
             if (valid == DEF_OK) {
                 USBD_DescWrStop(p_dev, &err);
                 if (err != USBD_ERR_NONE) {
                     valid = DEF_FAIL;
                 }
             }
             p_dev->DescBufErrPtr = (USBD_ERR *)0;
             break;


        case USBD_REQ_GET_INTERFACE:
             if (dev_to_host != DEF_YES) {
                 break;
             }

             USBD_DBG_CORE_STD_ARG("  Get Interface IF ", if_nbr);

             if (p_dev->State != USBD_DEV_STATE_CONFIGURED) {
                 break;
             }

             p_dev->CtrlStatusBufPtr[0u] = p_if->AltCur;

             USBD_DBG_CORE_STD_ARG("                Alt", p_if->AltCur);

             (void)USBD_CtrlTx(         p_dev->Nbr,
                               (void *)&p_dev->CtrlStatusBufPtr[0u],
                                        1u,
                                        USBD_CFG_CTRL_REQ_TIMEOUT_mS,
                                        DEF_NO,
                                       &err);
             if (err != USBD_ERR_NONE) {
                 break;
             }

             valid = DEF_OK;
             break;


        case USBD_REQ_SET_INTERFACE:
             if (dev_to_host != DEF_NO) {
                 break;
             }

             USBD_DBG_CORE_STD_ARG("  Set Interface IF ", if_nbr);

             if (p_dev->State != USBD_DEV_STATE_CONFIGURED) {
                 break;
             }
                                                            /* Get IF alt setting nbr.                                  */
             if_alt_nbr = (CPU_INT08U)(p_dev->SetupReq.wValue  & DEF_INT_08_MASK);
             p_if_alt   =  USBD_IF_AltRefGet(p_if, if_alt_nbr);

             USBD_DBG_CORE_STD_ARG("                Alt", if_alt_nbr);

             if (p_if_alt == (USBD_IF_ALT *)0) {
                 USBD_DBG_CORE_STD_ERR("  Set Interface", USBD_ERR_IF_ALT_INVALID_NBR);
                 break;
             }

             if (p_if_alt == p_if->AltCurPtr) {                 /* If alt setting is the same as the cur one,...        */
                 valid = DEF_OK;                                /* ...no further processing is needed.                  */
                 break;
             }

             USBD_IF_AltClose(p_dev, p_if->AltCurPtr);          /* Close the cur alt setting.                           */

             USBD_IF_AltOpen(p_dev, if_nbr, p_if_alt, &err);    /* Open the new alt setting.                            */
             if (err != USBD_ERR_NONE) {                        /* Re-open curr IF alt setting, in case it fails.       */
                 USBD_IF_AltOpen(p_dev, p_if->AltCur, p_if->AltCurPtr, &err);
                 break;
             }

             CPU_CRITICAL_ENTER();                              /* Set IF alt setting.                                  */
             p_if->AltCurPtr = p_if_alt;
             p_if->AltCur    = if_alt_nbr;
             CPU_CRITICAL_EXIT();
                                                                /* Notify class that IF or alt IF has been updated.     */
             if (p_if->ClassDrvPtr->AltSettingUpdate != (void *)0) {
                 p_if->ClassDrvPtr->AltSettingUpdate(p_dev->Nbr,
                                                     p_dev->CfgCurNbr,
                                                     if_nbr,
                                                     if_alt_nbr,
                                                     p_if->ClassArgPtr,
                                                     p_if_alt->ClassArgPtr);
             }

             valid = DEF_OK;
             break;


        default:
             p_class_drv = p_if->ClassDrvPtr;
             if (p_class_drv->IF_Req == (void *)0) {
                 break;
             }

             valid = p_class_drv->IF_Req(p_dev->Nbr,
                                        &p_dev->SetupReq,
                                         p_if->ClassArgPtr);
             break;
    }

    USBD_DBG_STATS_DEV_INC_IF_TRUE(p_dev->Nbr, StdReqIF_StallNbr, (valid == DEF_FAIL));

    return (valid);
}


/*
*********************************************************************************************************
*                                           USBD_StdReqEP()
*
* Description : Process device standard request (Endpoint).
*
* Argument(s) : p_dev       Pointer to USB device.
*               -----       Argument validated in 'USBD_DevSetupPkt()' before posting the event to queue.
*
*               request     USB device request.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_StdReqEP (const  USBD_DEV    *p_dev,
                                           CPU_INT08U   request)
{
    USBD_IF      *p_if;
    USBD_IF_ALT  *p_alt_if;
    CPU_BOOLEAN   ep_is_stall;
    CPU_INT08U    if_nbr;
    CPU_INT08U    ep_addr;
    CPU_INT08U    ep_phy_nbr;
    CPU_BOOLEAN   valid;
    CPU_BOOLEAN   dev_to_host;
    CPU_INT08U    feature;
    USBD_ERR      err;


    USBD_DBG_STATS_DEV_INC(p_dev->Nbr, StdReqEP_Nbr);

    ep_addr     = (CPU_INT08U)(p_dev->SetupReq.wIndex & DEF_INT_08_MASK);
    feature     = (CPU_INT08U)(p_dev->SetupReq.wValue & DEF_INT_08_MASK);
    dev_to_host =  DEF_BIT_IS_SET(p_dev->SetupReq.bmRequestType, USBD_REQ_DIR_BIT);
    valid       =  DEF_FAIL;

    switch (request) {
        case USBD_REQ_CLEAR_FEATURE:
        case USBD_REQ_SET_FEATURE:
             if (dev_to_host != DEF_NO) {
                 break;
             }

             switch (p_dev->State) {
                 case USBD_DEV_STATE_ADDRESSED:
                      if (((ep_addr == 0x80u)  ||
                           (ep_addr == 0x00u)) &&
                           (feature == USBD_FEATURE_SEL_ENDPOINT_HALT)) {
                          if (request == USBD_REQ_CLEAR_FEATURE) {
                              USBD_DBG_CORE_STD_ARG("  Clear Feature (EP)(STALL)", USBD_EP_ADDR_TO_LOG(ep_addr));

                              USBD_EP_Stall(p_dev->Nbr,
                                            ep_addr,
                                            DEF_CLR,
                                           &err);
                              if (err != USBD_ERR_NONE) {
                                  break;
                              }
                          } else {
                              USBD_DBG_CORE_STD_ARG("  Set Feature (EP)(STALL)", USBD_EP_ADDR_TO_LOG(ep_addr));

                              USBD_EP_Stall(p_dev->Nbr,
                                            ep_addr,
                                            DEF_SET,
                                           &err);
                              if (err != USBD_ERR_NONE) {
                                  break;
                              }
                          }

                          valid = DEF_OK;
                      }
                      break;


                 case USBD_DEV_STATE_CONFIGURED:
                      if (feature == USBD_FEATURE_SEL_ENDPOINT_HALT) {
                          if (request == USBD_REQ_CLEAR_FEATURE) {
                              USBD_DBG_CORE_STD_ARG("  Clear Feature (EP)(STALL)", USBD_EP_ADDR_TO_LOG(ep_addr));

                              USBD_EP_Stall(p_dev->Nbr,
                                            ep_addr,
                                            DEF_CLR,
                                           &err);
                              if (err != USBD_ERR_NONE) {
                                  break;
                              }
                          } else {
                              USBD_DBG_CORE_STD_ARG("  Set Feature (EP)(STALL)", USBD_EP_ADDR_TO_LOG(ep_addr));

                              USBD_EP_Stall(p_dev->Nbr,
                                            ep_addr,
                                            DEF_SET,
                                           &err);
                              if (err != USBD_ERR_NONE) {
                                  break;
                              }
                          }

                          ep_phy_nbr = USBD_EP_ADDR_TO_PHY(ep_addr);
                          if_nbr     = p_dev->EP_IF_Tbl[ep_phy_nbr];
                          p_if       = USBD_IF_RefGet(p_dev->CfgCurPtr, if_nbr);
                          p_alt_if   = p_if->AltCurPtr;

                                                                /* Notify class that EP state has been updated.         */
                          if (p_if->ClassDrvPtr->EP_StateUpdate != (void *)0) {
                              p_if->ClassDrvPtr->EP_StateUpdate(p_dev->Nbr,
                                                                p_dev->CfgCurNbr,
                                                                if_nbr,
                                                                p_if->AltCur,
                                                                ep_addr,
                                                                p_if->ClassArgPtr,
                                                                p_alt_if->ClassArgPtr);
                          }

                          valid = DEF_OK;
                      }
                      break;


                 case USBD_DEV_STATE_NONE:
                 case USBD_DEV_STATE_INIT:
                 case USBD_DEV_STATE_ATTACHED:
                 case USBD_DEV_STATE_DEFAULT:
                 case USBD_DEV_STATE_SUSPENDED:
                 default:
                      break;
             }
             break;


        case USBD_REQ_GET_STATUS:
             if (dev_to_host != DEF_YES) {
                 break;
             }

             p_dev->CtrlStatusBufPtr[0u] = DEF_BIT_NONE;
             p_dev->CtrlStatusBufPtr[1u] = DEF_BIT_NONE;

             switch (p_dev->State) {
                 case USBD_DEV_STATE_ADDRESSED:
                      if ((ep_addr == 0x80u) ||
                          (ep_addr == 0x00u)) {
                          USBD_DBG_CORE_STD_ARG("  Get Status (EP)(STALL)", USBD_EP_ADDR_TO_LOG(ep_addr));
                          ep_is_stall = USBD_EP_IsStalled(p_dev->Nbr, ep_addr, &err);
                          if (ep_is_stall == DEF_TRUE) {
                              p_dev->CtrlStatusBufPtr[0u] = DEF_BIT_00;
                              p_dev->CtrlStatusBufPtr[1u] = DEF_BIT_NONE;
                          }

                          (void)USBD_CtrlTx(         p_dev->Nbr,
                                            (void *)&p_dev->CtrlStatusBufPtr[0u],
                                                     2u,
                                                     USBD_CFG_CTRL_REQ_TIMEOUT_mS,
                                                     DEF_NO,
                                                    &err);
                          if (err != USBD_ERR_NONE) {
                              break;
                          }

                          valid = DEF_OK;
                      }
                      break;


                 case USBD_DEV_STATE_CONFIGURED:
                      USBD_DBG_CORE_STD_ARG("  Get Status (EP)(STALL)", USBD_EP_ADDR_TO_LOG(ep_addr));
                      ep_is_stall = USBD_EP_IsStalled(p_dev->Nbr, ep_addr, &err);
                      if (ep_is_stall == DEF_TRUE) {
                          p_dev->CtrlStatusBufPtr[0u] = DEF_BIT_00;
                          p_dev->CtrlStatusBufPtr[1u] = DEF_BIT_NONE;
                      }

                      (void)USBD_CtrlTx(         p_dev->Nbr,
                                        (void *)&p_dev->CtrlStatusBufPtr[0],
                                                 2u,
                                                 USBD_CFG_CTRL_REQ_TIMEOUT_mS,
                                                 DEF_NO,
                                                &err);
                      if (err != USBD_ERR_NONE) {
                          break;
                      }

                      valid = DEF_OK;
                      break;


                 case USBD_DEV_STATE_NONE:
                 case USBD_DEV_STATE_INIT:
                 case USBD_DEV_STATE_ATTACHED:
                 case USBD_DEV_STATE_DEFAULT:
                 case USBD_DEV_STATE_SUSPENDED:
                 default:
                      break;
             }
             break;


        default:
             break;
    }

    USBD_DBG_STATS_DEV_INC_IF_TRUE(p_dev->Nbr, StdReqEP_StallNbr, (valid == DEF_FAIL));

    return (valid);
}


/*
*********************************************************************************************************
*                                         USBD_StdReqClass()
*
* Description : Class standard request handler.
*
* Argument(s) : p_dev           Pointer to USB device.
*               -----           Argument validated in 'USBD_DevSetupPkt()' before posting the event to queue.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_StdReqClass (const  USBD_DEV  *p_dev)
{
    USBD_CFG        *p_cfg;
    USBD_IF         *p_if;
    USBD_CLASS_DRV  *p_class_drv;
    CPU_INT08U       recipient;
    CPU_INT08U       if_nbr;
    CPU_INT08U       ep_addr;
    CPU_INT08U       ep_phy_nbr;
    CPU_BOOLEAN      valid;


    USBD_DBG_STATS_DEV_INC(p_dev->Nbr, StdReqClassNbr);

    p_cfg = p_dev->CfgCurPtr;
    if (p_cfg == (USBD_CFG *)0) {
        return (DEF_FAIL);
    }

    recipient = p_dev->SetupReq.bmRequestType & USBD_REQ_RECIPIENT_MASK;

    if (recipient == USBD_REQ_RECIPIENT_INTERFACE) {
        if_nbr = (CPU_INT08U)(p_dev->SetupReq.wIndex & DEF_INT_08_MASK);
    } else {
        ep_addr    = (CPU_INT08U)(p_dev->SetupReq.wIndex & DEF_INT_08_MASK);
        ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
        if_nbr     =  p_dev->EP_IF_Tbl[ep_phy_nbr];
    }

    p_if = USBD_IF_RefGet(p_cfg, if_nbr);
    if (p_if == (USBD_IF *)0) {
        USBD_DBG_STATS_DEV_INC(p_dev->Nbr, StdReqClassStallNbr);
        return (DEF_FAIL);
    }

    p_class_drv = p_if->ClassDrvPtr;
    if (p_class_drv->ClassReq == (void *)0) {
        USBD_DBG_STATS_DEV_INC(p_dev->Nbr, StdReqClassStallNbr);
        return (DEF_FAIL);
    }

    valid = p_class_drv->ClassReq(p_dev->Nbr,
                                 &p_dev->SetupReq,
                                  p_if->ClassArgPtr);

    USBD_DBG_STATS_DEV_INC_IF_TRUE(p_dev->Nbr, StdReqClassStallNbr, (valid == DEF_FAIL));

    return (valid);
}


/*
*********************************************************************************************************
*                                         USBD_StdReqVendor()
*
* Description : Vendor standard request handler.
*
* Argument(s) : p_dev       Pointer to USB device.
*               -----       Argument validated in 'USBD_DevSetupPkt()' before posting the event to queue.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_StdReqVendor (const  USBD_DEV  *p_dev)
{
    USBD_CFG        *p_cfg;
    USBD_IF         *p_if;
    USBD_CLASS_DRV  *p_class_drv;
    CPU_INT08U       recipient;
    CPU_INT08U       if_nbr;
    CPU_INT08U       ep_addr;
    CPU_INT08U       ep_phy_nbr;
    CPU_BOOLEAN      valid;


    p_cfg = p_dev->CfgCurPtr;
    if (p_cfg == (USBD_CFG *)0) {
        return (DEF_FAIL);
    }

    recipient = p_dev->SetupReq.bmRequestType & USBD_REQ_RECIPIENT_MASK;

    if (recipient == USBD_REQ_RECIPIENT_INTERFACE) {
        if_nbr = (CPU_INT08U)(p_dev->SetupReq.wIndex & DEF_INT_08_MASK);
    } else {
        ep_addr    = (CPU_INT08U)(p_dev->SetupReq.wIndex & DEF_INT_08_MASK);
        ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(ep_addr);
        if_nbr     =  p_dev->EP_IF_Tbl[ep_phy_nbr];
    }

    p_if = USBD_IF_RefGet(p_cfg, if_nbr);
    if (p_if == (USBD_IF *)0) {
        return (DEF_FAIL);
    }

    p_class_drv = p_if->ClassDrvPtr;
    if (p_class_drv->VendorReq == (void *)0) {
        return (DEF_FAIL);
    }

    valid = p_class_drv->VendorReq(p_dev->Nbr,
                                  &p_dev->SetupReq,
                                   p_if->ClassArgPtr);

    return (valid);
}


/*
*********************************************************************************************************
*                                           USBD_StdReqDevMS()
*
* Description : Microsoft descriptor request handler (when recipient is device).
*
* Argument(s) : p_dev       Pointer to USB device.
*               -----       Argument validated in 'USBD_DevSetupPkt()' before posting the event to queue.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) For more information on Microsoft OS decriptors, see
*                   'http://msdn.microsoft.com/en-us/library/windows/hardware/gg463179.aspx'.
*
*               (2) Page feature is not supported so Microsoft OS descriptors have their length limited
*                   to 64Kbytes.
*********************************************************************************************************
*/
#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
static  CPU_BOOLEAN  USBD_StdReqDevMS (const  USBD_DEV  *p_dev)
{
    CPU_BOOLEAN      valid;
    CPU_INT08U       if_nbr;
    CPU_INT08U       max_if;
    CPU_INT08U       if_ix;
    CPU_INT08U       compat_id_ix;
    CPU_INT08U       subcompat_id_ix;
    CPU_INT08U       section_cnt;
    CPU_INT16U       feature;
    CPU_INT16U       len;
    CPU_INT08U       cfg_nbr = 0u;
    CPU_INT32U       desc_len;
    USBD_CFG        *p_cfg;
    USBD_IF         *p_if;
    USBD_CLASS_DRV  *p_class_drv;


    valid   =  DEF_FAIL;
    feature =  p_dev->SetupReq.wIndex;
    if_nbr  = (CPU_INT08U)(p_dev->SetupReq.wValue & DEF_INT_08_MASK);
    len     =  p_dev->SetupReq.wLength;

                                                                /* Use 1st cfg as Microsoft doesn't specify cfg in ...  */
                                                                /* ... setup pkt.                                       */
#if (USBD_CFG_HS_EN == DEF_ENABLED)
    if (p_dev->Spd == USBD_DEV_SPD_HIGH) {
       p_cfg = USBD_CfgRefGet(p_dev, cfg_nbr | USBD_CFG_NBR_SPD_BIT);
    } else {
#endif
       p_cfg = USBD_CfgRefGet(p_dev, cfg_nbr);
#if (USBD_CFG_HS_EN == DEF_ENABLED)
    }
#endif
    if (p_cfg == (USBD_CFG *)0) {
        return (DEF_FAIL);
    }

    switch (feature) {
        case USBD_MS_OS_FEATURE_COMPAT_ID:                      /* See note (1).                                        */
                                                                /* ----------------- SEND DESC HEADER ----------------- */
                                                                /* Compute length of descriptor.                        */
             desc_len    = USBD_MS_OS_DESC_COMPAT_ID_HDR_LEN;
             section_cnt = 0u;
             if (if_nbr == 0u) {                                /* If req IF == 0, sends all dev compat IDs.            */
                 max_if = p_cfg->IF_NbrTotal;
             } else {
                 max_if = if_nbr + 1u;
             }

             for (if_ix = if_nbr; if_ix < max_if; if_ix++) {

                 p_if        = USBD_IF_RefGet(p_cfg, if_ix);
                 p_class_drv = p_if->ClassDrvPtr;
                 if (p_class_drv->MS_GetCompatID != DEF_NULL) {

                     compat_id_ix = p_class_drv->MS_GetCompatID(p_dev->Nbr, &subcompat_id_ix);
                     if (compat_id_ix != USBD_MS_OS_COMPAT_ID_NONE) {
                         desc_len += USBD_MS_OS_DESC_COMPAT_ID_SECTION_LEN;
                         section_cnt++;
                     }
                 }
             }

             USBD_DescWrStart((USBD_DEV *)p_dev, desc_len);     /* Wr desc hdr.                                         */

             USBD_DescWr32(p_dev->Nbr, desc_len);
             USBD_DescWr16(p_dev->Nbr, USBD_MS_OS_DESC_VER_1_0);
             USBD_DescWr16(p_dev->Nbr, feature);
             USBD_DescWr08(p_dev->Nbr, section_cnt);
             USBD_DescWr32(p_dev->Nbr, 0u);                     /* Add 7 null bytes (reserved).                         */
             USBD_DescWr16(p_dev->Nbr, 0u);
             USBD_DescWr08(p_dev->Nbr, 0u);


                                                                /* ---------------- SEND DESC SECTIONS ---------------- */
             if (len != USBD_MS_OS_DESC_COMPAT_ID_HDR_VER_1_0) {/* If req len = version, only send desc hdr.            */

                 for (if_ix = if_nbr; if_ix < max_if; if_ix++) {

                     p_if = USBD_IF_RefGet(p_cfg, if_ix);
                     if (p_if->ClassDrvPtr->MS_GetCompatID != DEF_NULL) {

                         compat_id_ix = p_if->ClassDrvPtr->MS_GetCompatID(p_dev->Nbr, &subcompat_id_ix);
                         if (compat_id_ix != USBD_MS_OS_COMPAT_ID_NONE) {
                             USBD_DescWr08(p_dev->Nbr, if_ix);
                             USBD_DescWr08(p_dev->Nbr, 0x01u);

                             USBD_DescWr(              p_dev->Nbr,
                                         (CPU_INT08U *)USBD_MS_CompatID[compat_id_ix],
                                                       8u);

                             USBD_DescWr(              p_dev->Nbr,
                                         (CPU_INT08U *)USBD_MS_SubCompatID[subcompat_id_ix],
                                                       8u);

                             USBD_DescWr32(p_dev->Nbr, 0u);  /* Add 6 null bytes (reserved).                         */
                             USBD_DescWr16(p_dev->Nbr, 0u);
                         }
                     }
                 }
             }

             USBD_DescWrStop((USBD_DEV *)p_dev, p_dev->DescBufErrPtr);
             if (*(p_dev->DescBufErrPtr) == USBD_ERR_NONE) {
                 valid = DEF_OK;
             }
             break;


        default:
             break;
    }

    return (valid);
}
#endif


/*
*********************************************************************************************************
*                                           USBD_StdReqIF_MS()
*
* Description : Microsoft descriptor request handler (when recipient is interface).
*
* Argument(s) : p_dev       Pointer to USB device.
*               -----       Argument validated in 'USBD_DevSetupPkt()' before posting the event to queue.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) For more information on Microsoft OS decriptors, see
*                   'http://msdn.microsoft.com/en-us/library/windows/hardware/gg463179.aspx'.
*
*               (2) Page feature is not supported so Microsoft OS descriptors have their length limited
*                   to 64Kbytes.
*********************************************************************************************************
*/
#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
static  CPU_BOOLEAN  USBD_StdReqIF_MS (const  USBD_DEV  *p_dev)
{
    CPU_BOOLEAN               valid;
    CPU_INT08U                if_nbr;
    CPU_INT08U                section_cnt;
    CPU_INT08U                ext_property_cnt;
    CPU_INT08U                ext_property_ix;
    CPU_INT16U                feature;
    CPU_INT16U                len;
    CPU_INT32U                desc_len;
    CPU_INT08U                cfg_nbr = 0u;
    USBD_CFG                 *p_cfg;
    USBD_IF                  *p_if;
    USBD_CLASS_DRV           *p_class_drv;
    USBD_MS_OS_EXT_PROPERTY  *p_ext_property;


    valid   =  DEF_FAIL;
    feature =  p_dev->SetupReq.wIndex;
    if_nbr  = (CPU_INT08U)(p_dev->SetupReq.wValue & DEF_INT_08_MASK);
    len     =  p_dev->SetupReq.wLength;

                                                                /* Use 1st cfg as Microsoft doesn't specify cfg in ...  */
                                                                /* ... setup pkt.                                       */
#if (USBD_CFG_HS_EN == DEF_ENABLED)
    if (p_dev->Spd == USBD_DEV_SPD_HIGH) {
       p_cfg = USBD_CfgRefGet(p_dev, cfg_nbr | USBD_CFG_NBR_SPD_BIT);
    } else {
#endif
       p_cfg = USBD_CfgRefGet(p_dev, cfg_nbr);
#if (USBD_CFG_HS_EN == DEF_ENABLED)
    }
#endif
    if (p_cfg == (USBD_CFG *)0) {
        return (DEF_FAIL);
    }

    switch (feature) {
        case USBD_MS_OS_FEATURE_EXT_PROPERTIES:                 /* See note (1).                                        */
                                                                /* ----------------- SEND DESC HEADER ----------------- */
                                                                /* Compute length of descriptor.                        */
             desc_len    = USBD_MS_OS_DESC_EXT_PROPERTIES_HDR_LEN;
             section_cnt = 0u;
             p_if        = USBD_IF_RefGet(p_cfg, if_nbr);
             p_class_drv = p_if->ClassDrvPtr;
             if (p_class_drv->MS_GetExtPropertyTbl != DEF_NULL) {

                 ext_property_cnt = p_class_drv->MS_GetExtPropertyTbl(p_dev->Nbr, &p_ext_property);
                 for (ext_property_ix = 0u; ext_property_ix < ext_property_cnt; ext_property_ix++) {
                     desc_len += USBD_MS_OS_DESC_EXT_PROPERTIES_SECTION_HDR_LEN;
                     desc_len += p_ext_property->PropertyNameLen;
                     desc_len += p_ext_property->PropertyLen;
                     desc_len += 6u;

                     section_cnt++;
                     p_ext_property++;
                 }
             }

             USBD_DescWrStart((USBD_DEV *)p_dev, desc_len);

             USBD_DescWr32(p_dev->Nbr, desc_len);
             USBD_DescWr16(p_dev->Nbr, USBD_MS_OS_DESC_VER_1_0);
             USBD_DescWr16(p_dev->Nbr, feature);
             USBD_DescWr16(p_dev->Nbr, section_cnt);


                                                                /* ---------------- SEND DESC SECTIONS ---------------- */
                                                                /* If req len = version, only send desc hdr.            */
             if (len != USBD_MS_OS_DESC_EXT_PROPERTIES_HDR_VER_1_0) {

                 p_if = USBD_IF_RefGet(p_cfg, if_nbr);
                 if (p_class_drv->MS_GetExtPropertyTbl != DEF_NULL) {

                     ext_property_cnt = p_class_drv->MS_GetExtPropertyTbl(p_dev->Nbr, &p_ext_property);
                     for (ext_property_ix = 0u; ext_property_ix < ext_property_cnt; ext_property_ix++) {

                                                                /* Compute desc section len.                            */
                         desc_len  = USBD_MS_OS_DESC_EXT_PROPERTIES_SECTION_HDR_LEN;
                         desc_len += p_ext_property->PropertyNameLen;
                         desc_len += p_ext_property->PropertyLen;
                         desc_len += 6u;

                                                                /* Wr desc section.                                     */
                         USBD_DescWr32(p_dev->Nbr, desc_len);
                         USBD_DescWr32(p_dev->Nbr, p_ext_property->PropertyType);

                         USBD_DescWr16(p_dev->Nbr, p_ext_property->PropertyNameLen);
                         USBD_DescWr  (              p_dev->Nbr,
                                       (CPU_INT08U *)p_ext_property->PropertyNamePtr,
                                                     p_ext_property->PropertyNameLen);

                         USBD_DescWr32(p_dev->Nbr, p_ext_property->PropertyLen);
                         USBD_DescWr  (              p_dev->Nbr,
                                       (CPU_INT08U *)p_ext_property->PropertyPtr,
                                                     p_ext_property->PropertyLen);

                         p_ext_property++;
                     }
                 }
             }

             USBD_DescWrStop((USBD_DEV *)p_dev, p_dev->DescBufErrPtr);
             if (*(p_dev->DescBufErrPtr) == USBD_ERR_NONE) {
                 valid = DEF_OK;
             }
             break;


        default:
             break;
    }

    return (valid);
}
#endif

/*
*********************************************************************************************************
*                                        USBD_StdReqDescGet()
*
* Description : GET_DESCRIPTOR standard request handler.
*
* Argument(s) : p_dev       Pointer to USB device.
*               -----       Argument validated in 'USBD_DevSetupPkt()' before posting the event to queue.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_StdReqDescGet (USBD_DEV  *p_dev)
{
    CPU_INT08U    desc_type;
    CPU_INT08U    desc_ix;
    CPU_INT16U    req_len;
    CPU_BOOLEAN   valid;
    USBD_ERR      err;
#if (USBD_CFG_HS_EN == DEF_ENABLED)
    USBD_DRV     *p_drv;
#endif

    desc_type            = (CPU_INT08U)((p_dev->SetupReq.wValue >> 8u) & DEF_INT_08_MASK);
    desc_ix              = (CPU_INT08U)( p_dev->SetupReq.wValue        & DEF_INT_08_MASK);
    valid                =  DEF_FAIL;
    req_len              =  p_dev->SetupReq.wLength;
    p_dev->ActualBufPtr  =  p_dev->DescBufPtr;                  /* Set the desc buf as the current buf.                 */
    p_dev->DescBufMaxLen =  USBD_CFG_DESC_BUF_LEN;              /* Set the max len for the desc buf.                    */
    p_dev->DescBufErrPtr = &err;

    switch (desc_type) {
        case USBD_DESC_TYPE_DEVICE:                             /* ----------------- DEVICE DESCRIPTOR ---------------- */
             USBD_DBG_CORE_STD("  Get Descriptor (Device)");
             USBD_DevDescSend(p_dev,
                              DEF_NO,
                              req_len,
                             &err);
             if (err == USBD_ERR_NONE) {
                 valid = DEF_OK;
             }
             break;


        case USBD_DESC_TYPE_CONFIGURATION:                      /* ------------- CONFIGURATION DESCRIPTOR ------------- */
             USBD_DBG_CORE_STD_ARG("  Get Descriptor (Configuration)", desc_ix);
             USBD_CfgDescSend(p_dev,
                              desc_ix,
                              DEF_NO,
                              req_len,
                             &err);
             if (err != USBD_ERR_NONE) {
                 USBD_DBG_CORE_STD_ERR("  Get Descriptor (Configuration)", err);
             } else {
                 valid = DEF_OK;
             }
             break;


        case USBD_DESC_TYPE_STRING:                             /* ---------------- STRING DESCRIPTOR ----------------- */
             USBD_DBG_CORE_STD_ARG("  Get Descriptor (String)", desc_ix);
             USBD_StrDescSend(p_dev,
                              desc_ix,
                              req_len,
                             &err);
             if (err == USBD_ERR_NONE) {
                 valid = DEF_OK;
             }
             break;


         case USBD_DESC_TYPE_DEVICE_QUALIFIER:                  /* ----------- DEVICE QUALIFIER DESCRIPTOR ------------ */
              USBD_DBG_CORE_STD("  Get Descriptor (Device Qualifier)");
#if (USBD_CFG_HS_EN == DEF_ENABLED)
              p_drv = &p_dev->Drv;

              if (p_drv->CfgPtr->Spd == USBD_DEV_SPD_FULL) {    /* Chk if dev only supports FS.                         */
                  break;
              }

              USBD_DevDescSend(p_dev,
                               DEF_YES,
                               req_len,
                              &err);
              if (err == USBD_ERR_NONE) {
                 valid = DEF_OK;
              }
#endif
              break;


         case USBD_DESC_TYPE_OTHER_SPEED_CONFIGURATION:         /* ------- OTHER-SPEED CONFIGURATION DESCRIPTOR ------- */
              USBD_DBG_CORE_STD("  Get Descriptor (Other Speed)");
#if (USBD_CFG_HS_EN == DEF_ENABLED)
              p_drv = &p_dev->Drv;

              if (p_drv->CfgPtr->Spd == USBD_DEV_SPD_FULL) {
                  break;
              }

              USBD_CfgDescSend(p_dev,
                               desc_ix,
                               DEF_YES,
                               req_len,
                              &err);
              if (err == USBD_ERR_NONE) {
                 valid = DEF_OK;
              }
#endif
              break;


         default :
              break;
    }

    p_dev->DescBufErrPtr = (USBD_ERR *)0;
    return (valid);
}


/*
*********************************************************************************************************
*                                        USBD_StdReqDescSet()
*
* Description : SET_DESCRIPTOR standard request handler.
*
* Argument(s) : p_dev       Pointer to USB device.
*               -----       Argument validated in 'USBD_DevSetupPkt()' before posting the event to queue.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) $$$$ SET_DESCRIPTOR MAY be implemented in future versions.
*********************************************************************************************************
*/

#if 0
static  CPU_BOOLEAN  USBD_StdReqDescSet (USBD_DEV  *p_dev)
{
    (void)p_dev;

    return (DEF_FAIL);
}
#endif


/*
*********************************************************************************************************
*                                           USBD_CfgClose()
*
* Description : Close current device configuration.
*
* Argument(s) : p_dev       Pointer to USB device.
*               -----       Argument validated in 'USBD_DevSetupPkt()' before posting the event to queue and
*                           'USBD_DevStop()' function.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_CfgClose (USBD_DEV  *p_dev)
{
    USBD_CFG      *p_cfg;
    USBD_IF       *p_if;
    USBD_IF_ALT   *p_if_alt;
    USBD_DRV_API  *p_drv_api;
    CPU_INT08U     if_nbr;
    CPU_SR_ALLOC();


    p_cfg = p_dev->CfgCurPtr;
    if (p_cfg == (USBD_CFG *)0) {
        return;
    }

    for (if_nbr = 0u; if_nbr < p_cfg->IF_NbrTotal; if_nbr++) {
        p_if = USBD_IF_RefGet(p_cfg, if_nbr);
        if (p_if == (USBD_IF *)0) {
            return;
        }

        p_if_alt = p_if->AltCurPtr;
        if (p_if_alt == (USBD_IF_ALT *)0) {
            return;
        }

        if (p_if->ClassDrvPtr->Disconn != (void *)0) {
            p_if->ClassDrvPtr->Disconn(p_dev->Nbr,              /* Notify class that cfg is not active.                 */
                                       p_dev->CfgCurNbr,
                                       p_if->ClassArgPtr);
        }

        USBD_IF_AltClose(p_dev, p_if_alt);

        p_if_alt = USBD_IF_AltRefGet(p_if, 0u);

        CPU_CRITICAL_ENTER();
        p_if->AltCurPtr = p_if_alt;
        p_if->AltCur    = 0u;
        CPU_CRITICAL_EXIT();
    }

    p_drv_api = p_dev->Drv.API_Ptr;
    if (p_drv_api->CfgClr != (void*)0) {
        p_drv_api->CfgClr(&p_dev->Drv, p_dev->CfgCurNbr);       /* Clr cfg in the driver.                               */
    }

    if (p_dev->BusFnctsPtr->CfgClr != (void *)0) {
                                                                /* Notify app about clr cfg.                            */
        p_dev->BusFnctsPtr->CfgClr(p_dev->Nbr, p_dev->CfgCurNbr);
    }

    CPU_CRITICAL_ENTER();
    p_dev->CfgCurPtr = (USBD_CFG *)0;
    p_dev->CfgCurNbr =  USBD_CFG_NBR_NONE;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                           USBD_CfgOpen()
*
* Description : Open specified configuration.
*
* Argument(s) : p_dev       Pointer to USB device.
*               -----       Argument validated in 'USBD_DevSetupPkt()' before posting the event to queue.
*
*               cfg_nbr     Configuration number.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                   Configuration successfully opened.
*                               USBD_ERR_CFG_INVALID_NBR        Invalid configuration number.
*                               USBD_ERR_IF_INVALID_NBR         Invalid interface number.
*                               USBD_ERR_IF_ALT_INVALID_NBR     Invalid interface alternate setting number.
*                               USBD_ERR_CFG_SET_FAIL           Device driver set configuration failed.
*
*                               - RETURNED BY USBD_IF_AltOpen() -
*                               See USBD_IF_AltOpen() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_CfgOpen (USBD_DEV    *p_dev,
                            CPU_INT08U   cfg_nbr,
                            USBD_ERR    *p_err)

{
    USBD_CFG      *p_cfg;
    USBD_IF       *p_if;
    USBD_IF_ALT   *p_if_alt;
    USBD_DRV_API  *p_drv_api;
    CPU_INT08U     if_nbr;
    CPU_BOOLEAN    cfg_set;
    CPU_SR_ALLOC();


#if (USBD_CFG_HS_EN == DEF_ENABLED)
    if (p_dev->Spd == USBD_DEV_SPD_HIGH) {
       p_cfg = USBD_CfgRefGet(p_dev, cfg_nbr | USBD_CFG_NBR_SPD_BIT);
    } else {
#endif
       p_cfg = USBD_CfgRefGet(p_dev, cfg_nbr);
#if (USBD_CFG_HS_EN == DEF_ENABLED)
    }
#endif

    if (p_cfg == (USBD_CFG *)0) {
       *p_err = USBD_ERR_CFG_INVALID_NBR;
        return;
    }

    for (if_nbr = 0u; if_nbr < p_cfg->IF_NbrTotal; if_nbr++) {
        p_if = USBD_IF_RefGet(p_cfg, if_nbr);
        if (p_if == (USBD_IF *)0) {
           *p_err = USBD_ERR_IF_INVALID_NBR;
            return;
        }

        p_if_alt = p_if->AltCurPtr;
        if (p_if_alt == (USBD_IF_ALT *)0) {
           *p_err = USBD_ERR_IF_ALT_INVALID_NBR;
            return;
        }

        USBD_IF_AltOpen(p_dev,
                        if_nbr,
                        p_if_alt,
                        p_err);
        if (*p_err != USBD_ERR_NONE) {
             return;
        }
    }

    CPU_CRITICAL_ENTER();
    p_dev->CfgCurPtr = p_cfg;
    p_dev->CfgCurNbr = cfg_nbr;
    CPU_CRITICAL_EXIT();

   *p_err = USBD_ERR_NONE;
    p_drv_api = p_dev->Drv.API_Ptr;
    if (p_drv_api->CfgSet != (void*)0) {
        cfg_set = p_drv_api->CfgSet(&p_dev->Drv, cfg_nbr);      /* Set cfg in the drv.                                  */
        if (cfg_set == DEF_FAIL) {
           *p_err = USBD_ERR_CFG_SET_FAIL;
            return;
        }
    }

    CPU_CRITICAL_ENTER();
    p_dev->State = USBD_DEV_STATE_CONFIGURED;
    CPU_CRITICAL_EXIT();

    for (if_nbr = 0u; if_nbr < p_cfg->IF_NbrTotal; if_nbr++) {
        p_if = USBD_IF_RefGet(p_cfg, if_nbr);
        if (p_if == (USBD_IF *)0) {
           *p_err = USBD_ERR_IF_INVALID_NBR;
            return;
        } else {

            if (p_if->ClassDrvPtr->Conn != (void *)0) {
                p_if->ClassDrvPtr->Conn(p_dev->Nbr,             /* Notify class that cfg is active.                     */
                                        cfg_nbr,
                                        p_if->ClassArgPtr);
            }
        }
    }

    if (p_dev->BusFnctsPtr->CfgSet != (void *)0) {
        p_dev->BusFnctsPtr->CfgSet(p_dev->Nbr, cfg_nbr);        /* Notify app about set cfg.                            */
    }
}


/*
*********************************************************************************************************
*                                         USBD_DevDescSend()
*
* Description : Send device configuration descriptor.
*
* Argument(s) : p_dev       Pointer to USB device.
*               -----       Argument validated in 'USBD_DevSetupPkt()' before posting the event to queue.
*
*               other       Other speed configuration :
*
*                               DEF_YES     Current speed.
*                               DEF_NO      Other operational speed.
*
*               req_len     Requested length by the host.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Device configuration successfully sent.
*                               USBD_ERR_DEV_INVALID_SPD    Invalid speed; configuration descriptor not available.
*
*                               - RETURNED BY USBD_DescWrStop() -
*                               See USBD_DescWrStop() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) USB Spec 2.0 table 9-8 describes the standard device descriptor.
*
*                   +--------+--------------------+-------+----------+-----------------------------------+
*                   | Offset |        Field       |  Size |   Value  |            Description            |
*                   +--------+--------------------+-------+----------+-----------------------------------+
*                   |    0   | bLength            |   1   | Number   | Size of this descriptor           |
*                   +--------+--------------------+-------+----------+-----------------------------------+
*                   |    1   | bDescriptorType    |   1   | Const    | DEVICE Descriptor Type            |
*                   +--------+--------------------+-------+----------+-----------------------------------+
*                   |    2   | bcdUSB             |   2   | BCD USB  | Specification release number      |
*                   +--------+--------------------+-------+----------+-----------------------------------+
*                   |    4   | bDeviceClass       |   1   | Class    | Class code.                       |
*                   +--------+--------------------+-------+----------+-----------------------------------+
*                   |    5   | bDeviceSubClass    |   1   | SubClass | Subclass code.                    |
*                   +--------+--------------------+-------+----------+-----------------------------------+
*                   |    6   | bDeviceProtocol    |   1   | Protocol | Protocol code.                    |
*                   +--------+--------------------+-------+----------+-----------------------------------+
*                   |    7   | bMaxPacketSize0    |   1   | Number   | Max packet size for EP zero       |
*                   +--------+--------------------+-------+----------+-----------------------------------+
*                   |    8   | idVendor           |   2   | ID       | Vendor  ID                        |
*                   +--------+--------------------+-------+----------+-----------------------------------+
*                   |   10   | idProduct          |   2   | ID       | Product ID                        |
*                   +--------+--------------------+-------+----------+-----------------------------------+
*                   |   12   | bcdDevice          |   2   | BCD      | Dev release number in BCD format  |
*                   +--------+--------------------+-------+----------+-----------------------------------+
*                   |   14   | iManufacturer      |   1   | Index    | Index of manufacturer string      |
*                   +--------+--------------------+-------+----------+-----------------------------------+
*                   |   15   | iProduct           |   1   | Index    | Index of product string           |
*                   +--------|--------------------|-------|----------|-----------------------------------+
*                   |   16   | iSerialNumber      |   1   | Index    | Index of serial number string     |
*                   +--------|--------------------|-------|----------|-----------------------------------+
*                   |   17   | bNumConfigurations |   1   |  Number  | Number of possible configurations |
*                   +--------|--------------------|-------|----------|-----------------------------------+
*
*             (2) To enable host to identify devices that use the Interface Association descriptor the
*                 device descriptor should contain the following values.
*********************************************************************************************************
*/
static  void  USBD_DevDescSend (USBD_DEV     *p_dev,
                                CPU_BOOLEAN   other,
                                CPU_INT16U    req_len,
                                USBD_ERR     *p_err)
{
    USBD_CFG     *p_cfg;
    CPU_BOOLEAN   if_grp_en;
    CPU_INT08U    cfg_nbr;
    CPU_INT08U    cfg_nbr_spd;
    CPU_INT08U    cfg_nbr_total;
    CPU_INT08U    str_ix;


#if (USBD_CFG_HS_EN == DEF_DISABLED)
    (void)other;
#endif

    if_grp_en = DEF_NO;
   *p_err     = USBD_ERR_NONE;

#if (USBD_CFG_HS_EN == DEF_ENABLED)
    if (other == DEF_NO) {
#endif
        USBD_DescWrStart(p_dev, req_len);
        USBD_DescWrReq08(p_dev, USBD_DESC_LEN_DEV);             /* Desc len.                                            */
        USBD_DescWrReq08(p_dev, USBD_DESC_TYPE_DEVICE);         /* Dev desc type.                                       */
        USBD_DescWrReq16(p_dev, 0x200u);                        /* USB spec release nbr in BCD fmt (2.00).              */

#if (USBD_CFG_HS_EN == DEF_ENABLED)
        if (p_dev->Spd == USBD_DEV_SPD_FULL) {
#endif
            cfg_nbr_spd   = DEF_BIT_NONE;
            cfg_nbr_total = p_dev->CfgFS_TotalNbr;
#if (USBD_CFG_HS_EN == DEF_ENABLED)
        } else {
            cfg_nbr_spd   = USBD_CFG_NBR_SPD_BIT;
            cfg_nbr_total = p_dev->CfgHS_TotalNbr;
        }
#endif

        cfg_nbr = 0u;
        while ((cfg_nbr   <  cfg_nbr_total) &&
               (if_grp_en == DEF_NO)) {

            p_cfg = USBD_CfgRefGet(p_dev, cfg_nbr | cfg_nbr_spd);
            if (p_cfg != (USBD_CFG *)0) {
                if (p_cfg->IF_GrpNbrTotal > 0u) {
                    if_grp_en = DEF_YES;
                }
            }

            cfg_nbr++;
        }

        if (if_grp_en == DEF_NO) {
                                                                /* Dev class is specified in IF desc.                   */
            USBD_DescWrReq08(p_dev, USBD_CLASS_CODE_USE_IF_DESC);
            USBD_DescWrReq08(p_dev, USBD_SUBCLASS_CODE_USE_IF_DESC);
            USBD_DescWrReq08(p_dev, USBD_PROTOCOL_CODE_USE_IF_DESC);
        } else {
                                                                /* Multi-Interface function dev class.                  */
            USBD_DescWrReq08(p_dev, USBD_CLASS_CODE_MISCELLANEOUS);
            USBD_DescWrReq08(p_dev, USBD_SUBCLASS_CODE_USE_COMMON_CLASS);
            USBD_DescWrReq08(p_dev, USBD_PROTOCOL_CODE_USE_IAD);
        }
                                                                /* Set max pkt size for ctrl EP.                        */
        USBD_DescWrReq08(p_dev, (CPU_INT08U)p_dev->EP_CtrlMaxPktSize);
                                                                /* Set vendor id, product id and dev id.                */
        USBD_DescWrReq16(p_dev, p_dev->DevCfgPtr->VendorID);
        USBD_DescWrReq16(p_dev, p_dev->DevCfgPtr->ProductID);
        USBD_DescWrReq16(p_dev, p_dev->DevCfgPtr->DeviceBCD);

        str_ix = USBD_StrDescIxGet(p_dev, p_dev->DevCfgPtr->ManufacturerStrPtr);
        USBD_DescWrReq08(p_dev, str_ix);
        str_ix = USBD_StrDescIxGet(p_dev, p_dev->DevCfgPtr->ProductStrPtr);
        USBD_DescWrReq08(p_dev, str_ix);
        str_ix = USBD_StrDescIxGet(p_dev, p_dev->DevCfgPtr->SerialNbrStrPtr);
        USBD_DescWrReq08(p_dev, str_ix);
        USBD_DescWrReq08(p_dev, cfg_nbr_total);

#if (USBD_CFG_HS_EN == DEF_ENABLED)
    } else {

        if (p_dev->Drv.CfgPtr->Spd != USBD_DEV_SPD_HIGH) {
           *p_err = USBD_ERR_DEV_INVALID_SPD;
            return;
        }
        USBD_DescWrStart(p_dev, req_len);
        USBD_DescWrReq08(p_dev, USBD_DESC_LEN_DEV_QUAL);        /* Desc len.                                            */
        USBD_DescWrReq08(p_dev, USBD_DESC_TYPE_DEVICE_QUALIFIER);
        USBD_DescWrReq16(p_dev, 0x200u);                        /* USB spec release nbr in BCD fmt (2.00).              */

        if (p_dev->Spd == USBD_DEV_SPD_HIGH) {
            cfg_nbr_spd   = DEF_BIT_NONE;
            cfg_nbr_total = p_dev->CfgFS_TotalNbr;
        } else {
            cfg_nbr_spd   = USBD_CFG_NBR_SPD_BIT;
            cfg_nbr_total = p_dev->CfgHS_TotalNbr;
        }

        cfg_nbr = 0u;
        while ((cfg_nbr   <  cfg_nbr_total) &&
               (if_grp_en == DEF_NO)) {

            p_cfg = USBD_CfgRefGet(p_dev, cfg_nbr | cfg_nbr_spd);
            if (p_cfg != (USBD_CFG *)0) {
                if (p_cfg->IF_GrpNbrTotal > 0u) {
                    if_grp_en = DEF_YES;
                }
                cfg_nbr++;
            }
        }
        if (if_grp_en == DEF_NO) {
                                                                /* Dev class is specified in IF desc.                   */
            USBD_DescWrReq08(p_dev, USBD_CLASS_CODE_USE_IF_DESC);
            USBD_DescWrReq08(p_dev, USBD_SUBCLASS_CODE_USE_IF_DESC);
            USBD_DescWrReq08(p_dev, USBD_PROTOCOL_CODE_USE_IF_DESC);
        } else {
                                                                /* Multi-Interface function dev class.                  */
            USBD_DescWrReq08(p_dev, USBD_CLASS_CODE_MISCELLANEOUS);
            USBD_DescWrReq08(p_dev, USBD_SUBCLASS_CODE_USE_COMMON_CLASS);
            USBD_DescWrReq08(p_dev, USBD_PROTOCOL_CODE_USE_IAD);
        }
                                                                /* Set max pkt size for ctrl EP.                        */
        USBD_DescWrReq08(p_dev, (CPU_INT08U)p_dev->EP_CtrlMaxPktSize);
        USBD_DescWrReq08(p_dev, cfg_nbr_total);
        USBD_DescWrReq08(p_dev, 0u);
    }
#endif

    USBD_DescWrStop(p_dev, p_err);
}


/*
*********************************************************************************************************
*                                         USBD_CfgDescSend()
*
* Description : Send configuration descriptor.
*
* Argument(s) : p_dev       Pointer to device struct.
*               -----       Argument validated in 'USBD_DevSetupPkt()' before posting the event to queue.
*
*               cfg_nbr     Configuration number.
*
*               other       Other speed configuration :
*
*                               DEF_NO      Descriptor is build for the current speed.
*                               DEF_YES     Descriptor is build for the  other  speed.
*
*               req_len     Requested length by the host.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Device configuration successfully sent.
*                               USBD_ERR_CFG_INVALID_NBR    Invalid configuration number.
*
*                               - RETURNED BY USBD_DescWrStop() -
*                               See USBD_DescWrStop() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_CfgDescSend (USBD_DEV     *p_dev,
                                CPU_INT08U    cfg_nbr,
                                CPU_BOOLEAN   other,
                                CPU_INT16U    req_len,
                                USBD_ERR     *p_err)
{
    USBD_CFG        *p_cfg;
    USBD_IF         *p_if;
    USBD_EP_INFO    *p_ep;
    USBD_IF_ALT     *p_if_alt;
#if (USBD_CFG_MAX_NBR_IF_GRP > 0)
    USBD_IF_GRP     *p_if_grp;
#endif
    USBD_CLASS_DRV  *p_if_drv;
    CPU_INT08U       cfg_nbr_cur;
    CPU_INT08U       ep_nbr;
    CPU_INT08U       if_nbr;
    CPU_INT08U       if_total;
    CPU_INT08U       if_grp_cur;
    CPU_INT08U       if_alt_nbr;
    CPU_INT08U       str_ix;
    CPU_INT08U       attrib;
#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)
    CPU_INT32U       ep_alloc_map;
#endif


#if (USBD_CFG_HS_EN == DEF_ENABLED)
    if (p_dev->Spd == USBD_DEV_SPD_HIGH) {
        cfg_nbr_cur = cfg_nbr | USBD_CFG_NBR_SPD_BIT;
    } else {
#endif
        cfg_nbr_cur = cfg_nbr;
#if (USBD_CFG_HS_EN == DEF_ENABLED)
    }
#endif

   *p_err = USBD_ERR_NONE;

    p_cfg = USBD_CfgRefGet(p_dev, cfg_nbr_cur);
    if (p_cfg == (USBD_CFG *)0) {
       *p_err = USBD_ERR_CFG_INVALID_NBR;
        return;
    }

#if (USBD_CFG_HS_EN == DEF_ENABLED)                             /* other will always be DEF_NO when HS is disabled.     */
    if (other == DEF_YES) {
        if (p_cfg->CfgOtherSpd == USBD_CFG_NBR_NONE) {
           *p_err = USBD_ERR_CFG_INVALID_NBR;
            return;
        }

        cfg_nbr_cur = p_cfg->CfgOtherSpd;

        p_cfg = USBD_CfgRefGet(p_dev, cfg_nbr_cur);             /* Retrieve cfg struct for other spd.                   */
        if (p_cfg == (USBD_CFG *)0) {
           *p_err = USBD_ERR_CFG_INVALID_NBR;
            return;
        }
    }
#endif

    p_cfg->DescLen = USBD_DESC_LEN_CFG;                         /* Init cfg desc len.                                   */

    USBD_DescWrStart(p_dev, req_len);
                                                                /* ---------- BUILD CONFIGURATION DESCRIPTOR ---------- */
    USBD_DescWrReq08(p_dev, USBD_DESC_LEN_CFG);                 /* Desc len.                                            */
    if (other == DEF_YES) {
        USBD_DescWrReq08(p_dev, USBD_DESC_TYPE_OTHER_SPEED_CONFIGURATION);
    } else {
        USBD_DescWrReq08(p_dev, USBD_DESC_TYPE_CONFIGURATION);  /* Desc type.                                           */
    }

    if_total   = p_cfg->IF_NbrTotal;
    if_grp_cur = USBD_IF_GRP_NBR_NONE;

    for (if_nbr = 0u; if_nbr < if_total; if_nbr++) {
        p_if     = USBD_IF_RefGet(p_cfg, if_nbr);
        p_if_drv = p_if->ClassDrvPtr;

        if ((p_if->GrpNbr != if_grp_cur          ) &&
            (p_if->GrpNbr != USBD_IF_GRP_NBR_NONE)) {
                                                                /* Add IF assoc desc len.                               */
            p_cfg->DescLen += USBD_DESC_LEN_IF_ASSOCIATION;
            if_grp_cur      = p_if->GrpNbr;
        }

        p_cfg->DescLen += (USBD_DESC_LEN_IF * p_if->AltNbrTotal);

        for (if_alt_nbr = 0u; if_alt_nbr < p_if->AltNbrTotal; if_alt_nbr++) {
            p_if_alt        =  USBD_IF_AltRefGet(p_if, if_alt_nbr);
            p_cfg->DescLen += (USBD_DESC_LEN_EP * p_if_alt->EP_NbrTotal);

            if (p_if_drv->IF_DescSizeGet != (void *)0) {        /* Add IF functional desc len.                          */
                p_cfg->DescLen += p_if_drv->IF_DescSizeGet(p_dev->Nbr,
                                                           cfg_nbr_cur,
                                                           if_nbr,
                                                           if_alt_nbr,
                                                           p_if->ClassArgPtr,
                                                           p_if_alt->ClassArgPtr);
            }

#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)
            ep_alloc_map = p_if_alt->EP_TblMap;
            while (ep_alloc_map != DEF_BIT_NONE) {
                ep_nbr = (CPU_INT08U)CPU_CntTrailZeros32(ep_alloc_map);
                p_ep   =  p_if_alt->EP_TblPtrs[ep_nbr];

                if (p_if_drv->EP_DescSizeGet != (void *)0) {    /* Add EP functional desc len.                           */
                    p_cfg->DescLen += p_if_drv->EP_DescSizeGet(p_dev->Nbr,
                                                               cfg_nbr_cur,
                                                               if_nbr,
                                                               if_alt_nbr,
                                                               p_ep->Addr,
                                                               p_if->ClassArgPtr,
                                                               p_if_alt->ClassArgPtr);
                }

                if ((p_if->ClassCode                   == USBD_CLASS_CODE_AUDIO) &&
                    (p_if->ClassProtocolCode           == 0u                   ) &&
                  (((p_ep->Attrib & USBD_EP_TYPE_MASK) == USBD_EP_TYPE_ISOC)     ||
                   ((p_ep->Attrib & USBD_EP_TYPE_MASK) == USBD_EP_TYPE_INTR))) {
                     p_cfg->DescLen += 2u;                      /* EP desc on audio class v1.0 has 2 additional fields. */
                }

                DEF_BIT_CLR(ep_alloc_map, DEF_BIT32(ep_nbr));
            }
#else
            p_ep = p_if_alt->EP_HeadPtr;

            for (ep_nbr = 0u; ep_nbr < p_if_alt->EP_NbrTotal; ep_nbr++) {
                if (p_if_drv->EP_DescSizeGet != (void *)0) {
                    p_cfg->DescLen += p_if_drv->EP_DescSizeGet(p_dev->Nbr,
                                                               cfg_nbr_cur,
                                                               if_nbr,
                                                               if_alt_nbr,
                                                               p_ep->Addr,
                                                               p_if->ClassArgPtr,
                                                               p_if_alt->ClassArgPtr);
                }

                if ((p_if->ClassCode                   == USBD_CLASS_CODE_AUDIO) &&
                    (p_if->ClassProtocolCode           == 0u                   ) &&
                  (((p_ep->Attrib & USBD_EP_TYPE_MASK) == USBD_EP_TYPE_ISOC)     ||
                   ((p_ep->Attrib & USBD_EP_TYPE_MASK) == USBD_EP_TYPE_INTR))) {
                     p_cfg->DescLen += 2u;                      /* EP desc on audio class v1.0 has 2 additional fields. */
                }

                p_ep = p_ep->NextPtr;
            }
#endif
        }
    }
                                                                /* ------------------ BUILD CFG DESC ------------------ */
    USBD_DescWrReq16(p_dev, p_cfg->DescLen);                    /* Desc len.                                            */
    USBD_DescWrReq08(p_dev, p_cfg->IF_NbrTotal);                /* Nbr of IF.                                           */
    USBD_DescWrReq08(p_dev, cfg_nbr + 1u);                      /* Cfg ix.                                              */

    str_ix = USBD_StrDescIxGet(p_dev, p_cfg->NamePtr);          /* Add str ix.                                          */
    USBD_DescWrReq08(p_dev, str_ix);

    attrib = USBD_CFG_DESC_RSVD_SET;
    if (DEF_BIT_IS_SET(p_cfg->Attrib, USBD_DEV_ATTRIB_SELF_POWERED)) {
        DEF_BIT_SET(attrib, USBD_CFG_DESC_SELF_POWERED);
    }
    if (DEF_BIT_IS_SET(p_cfg->Attrib, USBD_DEV_ATTRIB_REMOTE_WAKEUP)) {
        DEF_BIT_SET(attrib, USBD_CFG_DESC_REMOTE_WAKEUP);
    }
    USBD_DescWrReq08(p_dev, attrib);
    USBD_DescWrReq08(p_dev, (CPU_INT08U)((p_cfg->MaxPwr + 1u) / 2u));

                                                                /* ------------ BUILD INTERFACE DESCRIPTOR ------------ */
    if_total   = p_cfg->IF_NbrTotal;
    if_grp_cur = USBD_IF_GRP_NBR_NONE;

    for (if_nbr = 0u; if_nbr < if_total; if_nbr++) {
        p_if     = USBD_IF_RefGet(p_cfg, if_nbr);
        p_if_drv = p_if->ClassDrvPtr;

#if (USBD_CFG_MAX_NBR_IF_GRP > 0)
        if ((p_if->GrpNbr != if_grp_cur          ) &&
            (p_if->GrpNbr != USBD_IF_GRP_NBR_NONE)) {
                                                                /* Add IF assoc desc (IAD).                             */
            p_if_grp = USBD_IF_GrpRefGet(p_cfg, p_if->GrpNbr);

            USBD_DescWrReq08(p_dev, USBD_DESC_LEN_IF_ASSOCIATION);
            USBD_DescWrReq08(p_dev, USBD_DESC_TYPE_IAD);
            USBD_DescWrReq08(p_dev, p_if_grp->IF_Start);
            USBD_DescWrReq08(p_dev, p_if_grp->IF_Cnt);
            USBD_DescWrReq08(p_dev, p_if_grp->ClassCode);
            USBD_DescWrReq08(p_dev, p_if_grp->ClassSubCode);
            USBD_DescWrReq08(p_dev, p_if_grp->ClassProtocolCode);

            str_ix = USBD_StrDescIxGet(p_dev, p_if_grp->NamePtr);
            USBD_DescWrReq08(p_dev, str_ix);
            if_grp_cur = p_if->GrpNbr;
        }
#endif
                                                                /* Add IF/alt settings desc.                            */
        for (if_alt_nbr = 0u; if_alt_nbr < p_if->AltNbrTotal; if_alt_nbr++) {
            p_if_alt = USBD_IF_AltRefGet(p_if, if_alt_nbr);

            USBD_DescWrReq08(p_dev, USBD_DESC_LEN_IF);
            USBD_DescWrReq08(p_dev, USBD_DESC_TYPE_INTERFACE);
            USBD_DescWrReq08(p_dev, if_nbr);
            USBD_DescWrReq08(p_dev, if_alt_nbr);
            USBD_DescWrReq08(p_dev, p_if_alt->EP_NbrTotal);
            USBD_DescWrReq08(p_dev, p_if->ClassCode);
            USBD_DescWrReq08(p_dev, p_if->ClassSubCode);
            USBD_DescWrReq08(p_dev, p_if->ClassProtocolCode);

            str_ix = USBD_StrDescIxGet(p_dev, p_if_alt->NamePtr);
            USBD_DescWrReq08(p_dev, str_ix);

            if (p_if_drv->IF_Desc != (void *)0) {               /* Add class specific IF desc.                          */
                p_if_drv->IF_Desc(p_dev->Nbr,
                                  cfg_nbr_cur,
                                  if_nbr,
                                  if_alt_nbr,
                                  p_if->ClassArgPtr,
                                  p_if_alt->ClassArgPtr);
            }
                                                                /* ------------------- BUILD EP DESC ------------------ */
#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)
            ep_alloc_map = p_if_alt->EP_TblMap;
            while (ep_alloc_map != DEF_BIT_NONE) {
                ep_nbr = (CPU_INT08U)CPU_CntTrailZeros32(ep_alloc_map);
                p_ep   =  p_if_alt->EP_TblPtrs[ep_nbr];

                if ((p_if->ClassCode                   == USBD_CLASS_CODE_AUDIO) &&
                    (p_if->ClassProtocolCode           == 0u                   ) &&
                  (((p_ep->Attrib & USBD_EP_TYPE_MASK) == USBD_EP_TYPE_ISOC)     ||
                   ((p_ep->Attrib & USBD_EP_TYPE_MASK) == USBD_EP_TYPE_INTR))) {
                                                                /* EP desc on audio class v1.0 has 2 additional fields. */
                     USBD_DescWrReq08(p_dev, USBD_DESC_LEN_EP + 2u);
                } else {
                     USBD_DescWrReq08(p_dev, USBD_DESC_LEN_EP);
                }

                USBD_DescWrReq08(p_dev, USBD_DESC_TYPE_ENDPOINT);
                USBD_DescWrReq08(p_dev, p_ep->Addr);
                USBD_DescWrReq08(p_dev, p_ep->Attrib);
                USBD_DescWrReq16(p_dev, p_ep->MaxPktSize);
                USBD_DescWrReq08(p_dev, p_ep->Interval);

                if ((p_if->ClassCode                   == USBD_CLASS_CODE_AUDIO) &&
                    (p_if->ClassProtocolCode           == 0u                   ) &&
                  (((p_ep->Attrib & USBD_EP_TYPE_MASK) == USBD_EP_TYPE_ISOC)     ||
                   ((p_ep->Attrib & USBD_EP_TYPE_MASK) == USBD_EP_TYPE_INTR))) {
                                                                /* EP desc on audio class v1.0 has 2 additional fields. */
                     USBD_DescWrReq08(p_dev, p_ep->SyncRefresh);
                     USBD_DescWrReq08(p_dev, p_ep->SyncAddr);
                }

                if (p_if_drv->EP_Desc != (void *)0) {
                    p_if_drv->EP_Desc(p_dev->Nbr,               /* Add class specific EP desc.                          */
                                      cfg_nbr_cur,
                                      if_nbr,
                                      if_alt_nbr,
                                      p_ep->Addr,
                                      p_if->ClassArgPtr,
                                      p_if_alt->ClassArgPtr);
                }

                DEF_BIT_CLR(ep_alloc_map, DEF_BIT32(ep_nbr));
            }
#else
            p_ep = p_if_alt->EP_HeadPtr;

            for (ep_nbr = 0u; ep_nbr < p_if_alt->EP_NbrTotal; ep_nbr++) {
                if ((p_if->ClassCode                   == USBD_CLASS_CODE_AUDIO) &&
                    (p_if->ClassProtocolCode           == 0u                   ) &&
                  (((p_ep->Attrib & USBD_EP_TYPE_MASK) == USBD_EP_TYPE_ISOC)     ||
                   ((p_ep->Attrib & USBD_EP_TYPE_MASK) == USBD_EP_TYPE_INTR))) {
                                                                /* EP desc on audio class v1.0 has 2 additional fields. */
                     USBD_DescWrReq08(p_dev, USBD_DESC_LEN_EP + 2u);
                } else {
                     USBD_DescWrReq08(p_dev, USBD_DESC_LEN_EP);
                }

                USBD_DescWrReq08(p_dev, USBD_DESC_TYPE_ENDPOINT);
                USBD_DescWrReq08(p_dev, p_ep->Addr);
                USBD_DescWrReq08(p_dev, p_ep->Attrib);
                USBD_DescWrReq16(p_dev, p_ep->MaxPktSize);
                USBD_DescWrReq08(p_dev, p_ep->Interval);

                if ((p_if->ClassCode                   == USBD_CLASS_CODE_AUDIO) &&
                    (p_if->ClassProtocolCode           == 0u                   ) &&
                  (((p_ep->Attrib & USBD_EP_TYPE_MASK) == USBD_EP_TYPE_ISOC)     ||
                   ((p_ep->Attrib & USBD_EP_TYPE_MASK) == USBD_EP_TYPE_INTR))) {
                                                                /* EP desc on audio class v1.0 has 2 additional fields. */
                     USBD_DescWrReq08(p_dev, p_ep->SyncRefresh);
                     USBD_DescWrReq08(p_dev, p_ep->SyncAddr);
                }

                if (p_if_drv->EP_Desc != (void *)0) {
                    p_if_drv->EP_Desc(p_dev->Nbr,               /* Add class specific EP desc.                          */
                                      cfg_nbr_cur,
                                      if_nbr,
                                      if_alt_nbr,
                                      p_ep->Addr,
                                      p_if->ClassArgPtr,
                                      p_if_alt->ClassArgPtr);
                }

                p_ep = p_ep->NextPtr;
            }
#endif
        }
    }

    USBD_DescWrStop(p_dev, p_err);
}


/*
*********************************************************************************************************
*                                         USBD_StrDescSend()
*
* Description : Send string descriptor.
*
* Argument(s) : p_dev       Pointer to USB device.
*               -----       Argument validated in 'USBD_DevSetupPkt()' before posting the event to queue.
*
*               str_ix      String index.
*
*               req_len     Requested length by the host.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               String descriptor successfully sent.
*                               USBD_ERR_NULL_PTR           String NOT available.
*
*                               - RETURNED BY USBD_DescWrStop() -
*                               See USBD_DescWrStop() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_StrDescSend (USBD_DEV    *p_dev,
                                CPU_INT08U   str_ix,
                                CPU_INT16U   req_len,
                                USBD_ERR    *p_err)
{
    const  CPU_CHAR    *p_str;
           CPU_SIZE_T   len;
#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
           CPU_INT08U   ix;
#endif


   *p_err = USBD_ERR_NONE;

    USBD_DescWrStart(p_dev, req_len);

    switch (str_ix) {
        case 0u:
             USBD_DescWrReq08(p_dev, 4u);
             USBD_DescWrReq08(p_dev, USBD_DESC_TYPE_STRING);
             USBD_DescWrReq16(p_dev, p_dev->DevCfgPtr->LangId);
             break;

#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
        case USBD_STR_MS_OS_IX:
             USBD_DescWrReq08(p_dev, USBD_STR_MS_OS_LEN);
             USBD_DescWrReq08(p_dev, USBD_DESC_TYPE_STRING);

             for (ix = 0u; ix < 7u; ix++) {
                 USBD_DescWrReq08(p_dev, (CPU_INT08U)USBD_StrMS_Signature[ix]);
                 USBD_DescWrReq08(p_dev, 0u);
             }

             USBD_DescWrReq08(p_dev, p_dev->StrMS_VendorCode);
             USBD_DescWrReq08(p_dev, 0u);
             break;
#endif

        default:
             p_str = USBD_StrDescGet(p_dev, str_ix - 1u);
             if (p_str != (CPU_CHAR *)0) {
                 len = Str_Len(p_str);
                 len = (2u * len) + 2u;
                 len = DEF_MIN(len, DEF_INT_08U_MAX_VAL);
                 len = len - (len % 2u);

                 USBD_DescWrReq08(p_dev, (CPU_INT08U)len);
                 USBD_DescWrReq08(p_dev, USBD_DESC_TYPE_STRING);

                 while (*p_str != '\0') {
                     USBD_DescWrReq08(p_dev, (CPU_INT08U)*p_str);
                     USBD_DescWrReq08(p_dev, 0u);

                     p_str++;
                 }
             } else {
                *p_err = USBD_ERR_NULL_PTR;
                 return;
             }
             break;
    }

    USBD_DescWrStop(p_dev, p_err);
}


/*
*********************************************************************************************************
*                                         USBD_DescWrStart()
*
* Description : Start write operation in the descriptor buffer.
*
* Argument(s) : p_dev       Pointer to USB device.
*               -----       Argument validated in 'USBD_DevSetupPkt()' before posting the event to queue.
*
*               req_len     Requested length by the host.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_DescWrStart (USBD_DEV    *p_dev,
                                CPU_INT16U   req_len)
{
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    p_dev->DescBufIx     = 0u;
    p_dev->DescBufReqLen = req_len;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                          USBD_DescWrStop()
*
* Description : Stop write operation in the descriptor buffer.
*
* Argument(s) : p_dev       Pointer to USB device.
*               -----       Argument validated in 'USBD_DevSetupPkt()' before posting the event to queue.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Write operation successfully completed.
*
*                               - RETURNED BY USBD_EP_CtrlTx() -
*                               See USBD_EP_CtrlTx() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function might be called in two contexts: when a Get Descriptor standard request
*                   is received, or when a driver supporting standard request auto-reply queries the
*                   device, a configuration or a string descriptor. The descriptor needs to be sent on
*                   control endpoint 0 only if this function is called for a Get Descriptor standard
*                   request. If the function is called when a driver needs the descriptor, nothing has to
*                   be done.
*********************************************************************************************************
*/

static  void  USBD_DescWrStop (USBD_DEV  *p_dev,
                               USBD_ERR  *p_err)
{
    if (*p_err == USBD_ERR_NONE) {
        if (p_dev->ActualBufPtr == p_dev->DescBufPtr) {         /* See Note #1.                                         */
            if (p_dev->DescBufIx > 0u) {
                (void)USBD_CtrlTx(            p_dev->Nbr,
                                             &p_dev->DescBufPtr[0u],
                                  (CPU_INT32U)p_dev->DescBufIx,
                                              USBD_CFG_CTRL_REQ_TIMEOUT_mS,
                                             (p_dev->DescBufReqLen > 0u) ? DEF_YES : DEF_NO,
                                              p_err);
            } else {
               *p_err = USBD_ERR_NONE;
            }
        }
    }
}


/*
*********************************************************************************************************
*                                         USBD_DescWrReq08()
*
* Description : Write 8-bit value in the descriptor buffer.
*
* Argument(s) : p_dev       Pointer to device.
*               -----       Argument validated by the caller(s).
*
*               val         8-bit value.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_DescWrReq08 (USBD_DEV    *p_dev,
                                CPU_INT08U   val)
{
    if (*(p_dev->DescBufErrPtr) == USBD_ERR_NONE) {
        USBD_DescWrReq(p_dev, &val, 1u);
    }
}


/*
*********************************************************************************************************
*                                           USBD_DescWr16()
*
* Description : Write 16-bit value in the descriptor buffer.
*
* Argument(s) : p_dev       Pointer to device.
*               -----       Argument validated by the caller(s)
*
*               val         16-bit value.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_DescWrReq16 (USBD_DEV    *p_dev,
                                CPU_INT16U   val)
{
    if (*(p_dev->DescBufErrPtr) == USBD_ERR_NONE) {
        CPU_INT08U  buf[2u];


        buf[0u] = (CPU_INT08U)( val        & DEF_INT_08_MASK);
        buf[1u] = (CPU_INT08U)((val >> 8u) & DEF_INT_08_MASK);

        USBD_DescWrReq(p_dev, &buf[0u], 2u);
    }
}


/*
*********************************************************************************************************
*                                          USBD_DescWrReq()
*
* Description : USB device configuration write request.
*
* Argument(s) : p_dev       Pointer to device.
*               -----       Argument validated by the caller(s)
*
*               p_buf       Pointer to data buffer.
*               -----       Argument validated by the caller(s).
*
*               len         Buffer length.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function might be called in two contexts: when a Get Descriptor standard request
*                   is received, or when a driver supporting standard request auto-reply queries the
*                   device, a configuration or a string descriptor. In the Get Descriptor standard
*                   request case, if the buffer is full, a transfer on control endpoint 0 is done, before
*                   resuming to fill the buffer. In the case of a driver supporting standard request
*                   auto-reply, if the buffer is full, an error is set and the function exits.
*
*               (2) If an error is reported by USBD_CtrlTx() during the construction of the descriptor,
*                   this pointer will store the error code, stop the rest of the data phase, skip the
*                   status phase and ensure that the control endpoint 0 is stalled to notify the host
*                   that an error has occurred.
*********************************************************************************************************
*/

static  void  USBD_DescWrReq (       USBD_DEV    *p_dev,
                              const  CPU_INT08U  *p_buf,
                                     CPU_INT16U   len)
{
    CPU_INT08U  *p_desc;
    CPU_INT08U   buf_cur_ix;
    CPU_INT16U   len_req;
    USBD_ERR     err;
    CPU_SR_ALLOC();


    p_desc     = p_dev->ActualBufPtr;
    buf_cur_ix = p_dev->DescBufIx;
    len_req    = p_dev->DescBufReqLen;
    err        = USBD_ERR_NONE;

    while ((len_req != 0u) &&
           (len     != 0u)) {
        if (buf_cur_ix >= p_dev->DescBufMaxLen) {
            if (p_dev->ActualBufPtr == p_dev->DescBufPtr) {     /* Send data in response to std req. See Note #1.       */
                buf_cur_ix = 0u;
                (void)USBD_CtrlTx(p_dev->Nbr,
                                 &p_dev->DescBufPtr[0u],
                                  USBD_CFG_DESC_BUF_LEN,
                                  USBD_CFG_CTRL_REQ_TIMEOUT_mS,
                                  DEF_NO,
                                 &err);
                if (err != USBD_ERR_NONE) {
                    break;
                }
            } else {                                            /* Buf provided by driver is too small. See Note #1.    */
                len_req = 0u;
                err     = USBD_ERR_ALLOC;
            }
        } else {
            p_desc[buf_cur_ix] = *p_buf;
            p_buf++;
            len--;
            len_req--;
            buf_cur_ix++;
        }
    }

    CPU_CRITICAL_ENTER();
    p_dev->DescBufIx     = buf_cur_ix;
    p_dev->DescBufReqLen = len_req;
    if (p_dev->DescBufErrPtr != (USBD_ERR *)0) {
      *(p_dev->DescBufErrPtr) = err;                            /* See Note #2.                                         */
    }
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                          USBD_DevRefGet()
*
* Description : Get device structure.
*
* Argument(s) : dev_nbr     Device number.
*
* Return(s)   : Pointer to device structure, if NO error(s).
*
*               Pointer to NULL,             otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  USBD_DEV  *USBD_DevRefGet (CPU_INT08U  dev_nbr)
{
    USBD_DEV  *p_dev;


    if (dev_nbr >= USBD_DevNbrNext) {                           /* Chk if dev nbr is valid.                             */
        return ((USBD_DEV *)0);
    }

    p_dev = &USBD_DevTbl[dev_nbr];                              /* Get dev struct.                                      */
    return (p_dev);
}


/*
*********************************************************************************************************
*                                          USBD_CfgRefGet()
*
* Description : Get configuration structure.
*
* Argument(s) : p_dev       Pointer to device struct.
*
*               cfg_nbr     Configuration number.
*
* Return(s)   : Pointer to configuration structure, if NO error(s).
*
*               Pointer to NULL,                    otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  USBD_CFG  *USBD_CfgRefGet (const  USBD_DEV    *p_dev,
                                          CPU_INT08U   cfg_nbr)
{
    USBD_CFG    *p_cfg;
    CPU_INT08U   cfg_val;
#if (USBD_CFG_OPTIMIZE_SPD == DEF_DISABLED)
    CPU_INT08U   cfg_ix;
#endif


#if (USBD_CFG_HS_EN == DEF_ENABLED)                             /* USBD_CFG_NBR_SPD_BIT will always be clear in FS.     */
    cfg_val = cfg_nbr & ~USBD_CFG_NBR_SPD_BIT;
#else
    cfg_val = cfg_nbr;
#endif

#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)                      /* Array implementation.                                */
#if (USBD_CFG_HS_EN == DEF_ENABLED)
    if (DEF_BIT_IS_SET(cfg_nbr, USBD_CFG_NBR_SPD_BIT) == DEF_YES) {
        if (cfg_val >= p_dev->CfgHS_TotalNbr) {                 /* Chk if cfg nbr is valid.                             */
            return ((USBD_CFG *)0);
        }
        p_cfg = p_dev->CfgHS_SpdTblPtrs[cfg_val];               /* Get HS cfg struct.                                   */

    } else {
#endif
        if (cfg_val >= p_dev->CfgFS_TotalNbr) {                 /* Chk if cfg nbr is valid.                             */
            return ((USBD_CFG *)0);
        }
        p_cfg = p_dev->CfgFS_SpdTblPtrs[cfg_val];               /* Get FS cfg struct.                                   */
#if (USBD_CFG_HS_EN == DEF_ENABLED)
    }
#endif
#else
#if (USBD_CFG_HS_EN == DEF_ENABLED)
    if (DEF_BIT_IS_SET(cfg_nbr, USBD_CFG_NBR_SPD_BIT)) {        /* Linked-list implementation.                          */
        if (cfg_val >= p_dev->CfgHS_TotalNbr) {                 /* Chk if cfg nbr is valid.                             */
            return ((USBD_CFG *)0);
        }
        p_cfg = p_dev->CfgHS_HeadPtr;

    } else {
#endif
        if (cfg_val >= p_dev->CfgFS_TotalNbr) {                 /* Chk if cfg nbr is valid.                             */
            return ((USBD_CFG *)0);
        }
        p_cfg = p_dev->CfgFS_HeadPtr;
#if (USBD_CFG_HS_EN == DEF_ENABLED)
    }
#endif

    for (cfg_ix = 0u; cfg_ix < cfg_val; cfg_ix++) {             /* Iterate thru list until to get cfg struct.           */
        p_cfg = p_cfg->NextPtr;
    }
#endif

    return (p_cfg);
}


/*
*********************************************************************************************************
*                                           USBD_EventSet()
*
* Description : Send an event to the core task.
*
* Argument(s) : p_drv       Pointer to device driver.
*
*               event       Event code :
*
*                               USBD_EVENT_BUS_RESET    Reset.
*                               USBD_EVENT_BUS_SUSPEND  Suspend.
*                               USBD_EVENT_BUS_RESUME   Resume.
*                               USBD_EVENT_BUS_CONN     Connect.
*                               USBD_EVENT_BUS_DISCONN  Disconnect.
*                               USBD_EVENT_BUS_HS       High speed.
*                               USBD_EVENT_EP           Endpoint.
*                               USBD_EVENT_SETUP        Setup.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_EventSet (USBD_DRV         *p_drv,
                             USBD_EVENT_CODE   event)
{
    USBD_CORE_EVENT  *p_core_event;


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_drv == (USBD_DRV *)0) {
        return;
    }
#endif

    p_core_event = USBD_CoreEventGet();
    if (p_core_event == (USBD_CORE_EVENT *)0) {
        return;
    }

    p_core_event->Type   = event;
    p_core_event->DrvPtr = p_drv;
    p_core_event->Err    = USBD_ERR_NONE;

    USBD_OS_CoreEventPut(p_core_event);
}


/*
*********************************************************************************************************
*                                        USBD_CoreTaskHandler()
*
* Description : Process all core events and core operations.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_CoreTaskHandler (void)
{
    USBD_CORE_EVENT  *p_core_event;
    USBD_DEV         *p_dev;
    USBD_DRV         *p_drv;
    CPU_INT08U        ep_addr;
    USBD_EVENT_CODE   event;
    USBD_ERR          err;


    while (DEF_TRUE) {
                                                                /* Wait for an event.                                   */
        p_core_event = (USBD_CORE_EVENT *)USBD_OS_CoreEventGet(0u, &err);
        if (p_core_event != (USBD_CORE_EVENT *)0) {
            event = p_core_event->Type;
            p_drv = p_core_event->DrvPtr;
            p_dev = USBD_DevRefGet(p_drv->DevNbr);

            if (p_dev != (USBD_DEV *)0) {
                if (p_dev->State != USBD_DEV_STATE_STOPPING) {

                    switch (event) {                            /* Decode event.                                        */
                        case USBD_EVENT_BUS_RESET:              /* -------------------- BUS EVENTS -------------------- */
                        case USBD_EVENT_BUS_RESUME:
                        case USBD_EVENT_BUS_CONN:
                        case USBD_EVENT_BUS_HS:
                        case USBD_EVENT_BUS_SUSPEND:
                        case USBD_EVENT_BUS_DISCONN:
                             USBD_EventProcess(p_dev, event);
                             break;

                        case USBD_EVENT_EP:                     /* ------------------ ENDPOINT EVENTS ----------------- */
                             if (p_dev->State == USBD_DEV_STATE_SUSPENDED) {
                                 p_dev->State = p_dev->StatePrev;
                             }
                             ep_addr = p_core_event->EP_Addr;
                             USBD_EP_XferAsyncProcess(p_drv, ep_addr, p_core_event->Err);
                             break;

                        case USBD_EVENT_SETUP:                  /* ------------------- SETUP EVENTS ------------------- */
                             USBD_DBG_STATS_DEV_INC(p_dev->Nbr, DevSetupEventNbr);
                             if (p_dev->State == USBD_DEV_STATE_SUSPENDED) {
                                 p_dev->State = p_dev->StatePrev;
                             }
                             USBD_StdReqHandler(p_dev);
                             break;

                        default:
                             break;
                    }
                }
            }

            USBD_CoreEventFree(p_core_event);                   /* Return event to free pool.                           */
        }
    }
}


/*
*********************************************************************************************************
*                                         USBD_EventProcess()
*
* Description : Process bus related events.
*
* Argument(s) : p_dev       Pointer to USB device.
*               -----       Argument validated in 'USBD_DevSetupPkt()' before posting the event to queue.
*
*               event       Bus related events :
*
*                               USBD_EVENT_BUS_RESET    Reset.
*                               USBD_EVENT_BUS_SUSPEND  Suspend.
*                               USBD_EVENT_BUS_RESUME   Resume.
*                               USBD_EVENT_BUS_CONN     Connect.
*                               USBD_EVENT_BUS_DISCONN  Disconnect.
*                               USBD_EVENT_BUS_HS       High speed.
*
* Return(s)   : none.
*
* Note(s)     : (1) This prevents a suspend event to overwrite the internal status with a suspend state in
*                   the case of multiple suspend events in a row.
*
*               (2) USB Spec 2.0 section 9.1.1.6 states "When suspended, the USB device maintains any
*                   internal status, including its address and configuration."
*
*               (3) A suspend event is usually followed by a resume event when the bus activity comes back.
*                   But in some cases, after a suspend event, a reset event can be notified to the Core
*                   before a resume event. Thus, the internal state of the device should not be changed
*                   to the previous one.
*********************************************************************************************************
*/

static  void  USBD_EventProcess (USBD_DEV         *p_dev,
                                 USBD_EVENT_CODE   event)
{
    USBD_BUS_FNCTS  *p_bus_fnct;
    USBD_ERR         err;
    CPU_SR_ALLOC();


    p_bus_fnct = p_dev->BusFnctsPtr;

    switch (event) {
        case USBD_EVENT_BUS_RESET:                              /* -------------------- RESET EVENT ------------------- */
             USBD_DBG_STATS_DEV_INC(p_dev->Nbr, DevResetEventNbr);
             USBD_DBG_CORE_BUS("Bus Reset");

             CPU_CRITICAL_ENTER();
             if (p_dev->ConnStatus == DEF_FALSE) {
                 p_dev->ConnStatus = DEF_TRUE;
                 CPU_CRITICAL_EXIT();

                 if (p_bus_fnct->Conn != (void *)0) {
                     p_bus_fnct->Conn(p_dev->Nbr);              /* Call application connect callback.                   */
                 }
             } else {
                 CPU_CRITICAL_EXIT();
             }

             USBD_CtrlClose(p_dev->Nbr, &err);                  /* Close ctrl EP.                                       */

             if (p_dev->CfgCurNbr != USBD_CFG_NBR_NONE) {
                USBD_CfgClose(p_dev);                           /* Close curr cfg.                                      */
             }

             USBD_CtrlOpen(p_dev->Nbr,                         /* Open ctrl EP.                                        */
                           p_dev->EP_CtrlMaxPktSize,
                          &err);

             CPU_CRITICAL_ENTER();                              /* Set dev in default state, reset dev speed.           */
             p_dev->Addr  = 0u;
             p_dev->State = USBD_DEV_STATE_DEFAULT;
             p_dev->Spd   = USBD_DEV_SPD_FULL;
             CPU_CRITICAL_EXIT();

             if (p_bus_fnct->Reset != (void *)0) {
                 p_bus_fnct->Reset(p_dev->Nbr);                 /* Call application reset callback.                     */
             }
             break;


        case USBD_EVENT_BUS_SUSPEND:                            /* ------------------- SUSPEND EVENT ------------------ */
             USBD_DBG_STATS_DEV_INC(p_dev->Nbr, DevSuspendEventNbr);
             USBD_DBG_CORE_BUS("Bus Suspend");

             CPU_CRITICAL_ENTER();
             if (p_dev->State    != USBD_DEV_STATE_SUSPENDED) { /* See Note #1.                                         */
                 p_dev->StatePrev = p_dev->State;               /* Save cur       state (see Note #2).                  */
             }
             p_dev->State         = USBD_DEV_STATE_SUSPENDED;   /* Set  suspended state.                                */
             CPU_CRITICAL_EXIT();

             if (p_bus_fnct->Suspend != (void *)0) {
                 p_bus_fnct->Suspend(p_dev->Nbr);               /* Call application suspend callback.                   */
             }
             break;

        case USBD_EVENT_BUS_RESUME:                             /* ------------------- RESUME EVENT ------------------- */
             USBD_DBG_STATS_DEV_INC(p_dev->Nbr, DevResumeEventNbr);
             USBD_DBG_CORE_BUS("Bus Resume");

             CPU_CRITICAL_ENTER();
             if (p_dev->State == USBD_DEV_STATE_SUSPENDED) {    /* See Note #3.                                         */
                 p_dev->State = p_dev->StatePrev;               /* Restore prev state.                                  */
             }
             CPU_CRITICAL_EXIT();

             if (p_bus_fnct->Resume != (void *)0) {
                 p_bus_fnct->Resume(p_dev->Nbr);                /* Call application resume callback.                    */
             }
             break;


        case USBD_EVENT_BUS_CONN:                               /* ------------------- CONNECT EVENT ------------------ */
             USBD_DBG_STATS_DEV_INC(p_dev->Nbr, DevConnEventNbr);
             USBD_DBG_CORE_BUS("Bus Connect");

             CPU_CRITICAL_ENTER();
             p_dev->State      = USBD_DEV_STATE_ATTACHED;       /* Set attached state.                                  */
             p_dev->ConnStatus = DEF_TRUE;
             CPU_CRITICAL_EXIT();

             if (p_bus_fnct->Conn != (void *)0) {
                 p_bus_fnct->Conn(p_dev->Nbr);                  /* Call application connect callback.                   */
             }
             break;

        case USBD_EVENT_BUS_DISCONN:                            /* ----------------- DISCONNECT EVENT ----------------- */
             USBD_DBG_STATS_DEV_INC(p_dev->Nbr, DevDisconnEventNbr);
             USBD_DBG_CORE_BUS("Bus Disconnect");

             USBD_CtrlClose(p_dev->Nbr, &err);                  /* Close ctrl EP.                                       */

             if (p_dev->CfgCurNbr != USBD_CFG_NBR_NONE) {
                USBD_CfgClose(p_dev);                           /* Close curr cfg.                                      */
             }

             CPU_CRITICAL_ENTER();
             p_dev->Addr       = 0u;                            /* Set default address.                                 */
             p_dev->State      = USBD_DEV_STATE_INIT;           /* Dev is not attached.                                 */
             p_dev->CfgCurNbr  = USBD_CFG_NBR_NONE;             /* No active cfg.                                       */
             p_dev->ConnStatus = DEF_FALSE;
             CPU_CRITICAL_EXIT();

             if (p_bus_fnct->Disconn != (void *)0) {
                 p_bus_fnct->Disconn(p_dev->Nbr);               /* Call application disconnect callback.                */
             }
             break;


        case USBD_EVENT_BUS_HS:                                 /* ------------ HIGH-SPEED HANDSHAKE EVENT ------------ */
             USBD_DBG_CORE_BUS("High Speed detection");
#if (USBD_CFG_HS_EN == DEF_ENABLED)
             CPU_CRITICAL_ENTER();
             p_dev->Spd = USBD_DEV_SPD_HIGH;
             if (p_dev->State == USBD_DEV_STATE_SUSPENDED) {
                 p_dev->State = p_dev->StatePrev;
             }
             CPU_CRITICAL_EXIT();
#endif
             break;

        case USBD_EVENT_EP:
        case USBD_EVENT_SETUP:
        default:
             break;
    }
}


/*
*********************************************************************************************************
*                                        USBD_CoreEventGet()
*
* Description : Get a new core event from the pool.
*
* Argument(s) : none.
*
* Return(s)   : Pointer to core event, if NO error(s).
*
*               Pointer to NULL,       otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  USBD_CORE_EVENT  *USBD_CoreEventGet (void)
{
    USBD_CORE_EVENT  *p_core_event;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    if (USBD_CoreEventPoolIx < 1u) {                            /* Chk if core event is avail.                          */
        CPU_CRITICAL_EXIT();
        return ((USBD_CORE_EVENT *)0);
    }

    USBD_CoreEventPoolIx--;
    p_core_event = USBD_CoreEventPoolPtrs[USBD_CoreEventPoolIx];
    CPU_CRITICAL_EXIT();

    return (p_core_event);
}


/*
*********************************************************************************************************
*                                        USBD_CoreEventFree()
*
* Description : Return a core event to the pool.
*
* Argument(s) : p_core_event    Pointer to core event.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_CoreEventFree (USBD_CORE_EVENT  *p_core_event)
{
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    if (USBD_CoreEventPoolIx == USBD_CORE_EVENT_NBR_TOTAL) {
        CPU_CRITICAL_EXIT();
        return;
    }

    USBD_CoreEventPoolPtrs[USBD_CoreEventPoolIx] = p_core_event;
    USBD_CoreEventPoolIx++;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                          USBD_IF_RefGet()
*
* Description : Get interface structure.
*
* Argument(s) : p_cfg       Pointer to configuration structure.
*
*               if_nbr      Interface number.
*
* Return(s)   : Pointer to interface structure, if NO error(s).
*
*               Pointer to NULL,                otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  USBD_IF  *USBD_IF_RefGet (const  USBD_CFG    *p_cfg,
                                         CPU_INT08U   if_nbr)
{
    USBD_IF     *p_if;
#if (USBD_CFG_OPTIMIZE_SPD == DEF_DISABLED)
    CPU_INT08U   if_ix;
#endif

    if (if_nbr >= p_cfg->IF_NbrTotal) {                         /* Chk if IF nbr is valid.                              */
        return ((USBD_IF *)0);
    }

#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)                      /* Get IF struct.                                       */
    p_if = p_cfg->IF_TblPtrs[if_nbr];
#else
    p_if = p_cfg->IF_HeadPtr;

    for (if_ix = 0u; if_ix < if_nbr; if_ix++) {
        p_if = p_if->NextPtr;
    }
#endif

    return (p_if);
}


/*
*********************************************************************************************************
*                                         USBD_IF_AltRefGet()
*
* Description : Get alternate setting interface structure.
*
* Argument(s) : p_if        Pointer to interface structure.
*
*               if_alt_nbr  Alternate setting interface number.
*
* Return(s)   : Pointer to alternate setting interface structure, if NO error(s).
*
*               Pointer to NULL,                                  otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  USBD_IF_ALT  *USBD_IF_AltRefGet (const  USBD_IF     *p_if,
                                                CPU_INT08U   if_alt_nbr)
{
    USBD_IF_ALT  *p_if_alt;
#if (USBD_CFG_OPTIMIZE_SPD == DEF_DISABLED)
    CPU_INT08U    if_alt_ix;
#endif

    if (if_alt_nbr >= p_if->AltNbrTotal) {                      /* Chk alt setting nbr.                                 */
        return ((USBD_IF_ALT *)0);
    }

#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)                      /* Get alt IF struct.                                   */
    p_if_alt = p_if->AltTblPtrs[if_alt_nbr];
#else
    p_if_alt = p_if->AltHeadPtr;

    for (if_alt_ix = 0u; if_alt_ix < if_alt_nbr; if_alt_ix++) {
        p_if_alt = p_if_alt->NextPtr;
    }
#endif

    return (p_if_alt);
}


/*
*********************************************************************************************************
*                                          USBD_IF_AltOpen()
*
* Description : Open all endpoints from the specified alternate setting.
*
* Argument(s) : p_dev       Pointer to USB device.
*               -----       Argument validated in 'USBD_DevSetupPkt()' before posting the event to queue.
*
*               if_nbr      Interface number.
*
*               p_if_alt    Pointer to alternate setting interface.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Alternate setting successfully opened.
*
*                               - RETURNED BY USBD_EP_Open() -
*                               See USBD_EP_Open() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_IF_AltOpen (       USBD_DEV     *p_dev,
                                      CPU_INT08U    if_nbr,
                               const  USBD_IF_ALT  *p_if_alt,
                                      USBD_ERR     *p_err)
{
    CPU_INT08U     ep_nbr;
    CPU_INT08U     ep_phy_nbr;
    CPU_BOOLEAN    valid;
    USBD_EP_INFO  *p_ep;
#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)
    CPU_INT32U     ep_alloc_map;
#endif
    CPU_SR_ALLOC();


    valid = DEF_OK;

#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)
    ep_alloc_map = p_if_alt->EP_TblMap;
    while (ep_alloc_map != DEF_BIT_NONE) {
        ep_nbr     = (CPU_INT08U)CPU_CntTrailZeros32(ep_alloc_map);
        p_ep       =  p_if_alt->EP_TblPtrs[ep_nbr];
        ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(p_ep->Addr);

        CPU_CRITICAL_ENTER();
        p_dev->EP_IF_Tbl[ep_phy_nbr] = if_nbr;
        CPU_CRITICAL_EXIT();

        USBD_EP_Open(&p_dev->Drv,
                      p_ep->Addr,
                      p_ep->MaxPktSize,
                      p_ep->Attrib,
                      p_ep->Interval,
                      p_err);
        if (*p_err != USBD_ERR_NONE) {
                valid = DEF_FAIL;
                break;
        }

        DEF_BIT_CLR(ep_alloc_map, DEF_BIT32(ep_nbr));
    }
#else
    p_ep = p_if_alt->EP_HeadPtr;

    for (ep_nbr = 0u; ep_nbr < p_if_alt->EP_NbrTotal; ep_nbr++) {
        ep_phy_nbr = USBD_EP_ADDR_TO_PHY(p_ep->Addr);

        CPU_CRITICAL_ENTER();
        p_dev->EP_IF_Tbl[ep_phy_nbr] = if_nbr;
        CPU_CRITICAL_EXIT();

        USBD_EP_Open(&p_dev->Drv,
                      p_ep->Addr,
                      p_ep->MaxPktSize,
                      p_ep->Attrib,
                      p_ep->Interval,
                      p_err);
        if (*p_err != USBD_ERR_NONE) {
             valid = DEF_FAIL;
             break;
        }

        p_ep = p_ep->NextPtr;
    }
#endif

    if (valid == DEF_OK) {
       *p_err = USBD_ERR_NONE;
    } else {
        USBD_IF_AltClose(p_dev, p_if_alt);
    }
}


/*
*********************************************************************************************************
*                                         USBD_IF_AltClose()
*
* Description : Close all endpoints from the specified alternate setting.
*
* Argument(s) : p_dev       Pointer to USB device.
*               -----       Argument validated in 'USBD_DevSetupPkt()' before posting the event to queue.
*
*               p_if_alt    Pointer to alternate setting interface.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_IF_AltClose (       USBD_DEV     *p_dev,
                                const  USBD_IF_ALT  *p_if_alt)
{
    CPU_INT08U     ep_nbr;
    CPU_INT08U     ep_phy_nbr;
    USBD_EP_INFO  *p_ep;
    USBD_ERR       err;
#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)
    CPU_INT32U     ep_alloc_map;
#endif
    CPU_SR_ALLOC();


#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)
    ep_alloc_map = p_if_alt->EP_TblMap;
    while (ep_alloc_map != DEF_BIT_NONE) {
        ep_nbr     = (CPU_INT08U)CPU_CntTrailZeros32(ep_alloc_map);
        p_ep       =  p_if_alt->EP_TblPtrs[ep_nbr];
        ep_phy_nbr =  USBD_EP_ADDR_TO_PHY(p_ep->Addr);

        CPU_CRITICAL_ENTER();
        p_dev->EP_IF_Tbl[ep_phy_nbr] = USBD_IF_NBR_NONE;
        CPU_CRITICAL_EXIT();

        USBD_EP_Close(&p_dev->Drv,
                       p_ep->Addr,
                      &err);

        DEF_BIT_CLR(ep_alloc_map, DEF_BIT32(ep_nbr));
    }
#else
    p_ep = p_if_alt->EP_HeadPtr;

    for (ep_nbr = 0u; ep_nbr < p_if_alt->EP_NbrTotal; ep_nbr++) {
        ep_phy_nbr = USBD_EP_ADDR_TO_PHY(p_ep->Addr);

        CPU_CRITICAL_ENTER();
        p_dev->EP_IF_Tbl[ep_phy_nbr] = USBD_IF_NBR_NONE;
        CPU_CRITICAL_EXIT();

        USBD_EP_Close(&p_dev->Drv,
                       p_ep->Addr,
                      &err);

        p_ep = p_ep->NextPtr;
    }
#endif
}


/*
*********************************************************************************************************
*                                         USBD_IF_GrpRefGet()
*
* Description : Get interface group structure.
*
* Argument(s) : p_cfg       Pointer to configuration structure.
*
*               if_grp_nbr  Interface number.
*
* Return(s)   : Pointer to interface group structure, if NO error(s).
*
*               Pointer to NULL,                      otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_CFG_MAX_NBR_IF_GRP > 0)
static  USBD_IF_GRP  *USBD_IF_GrpRefGet (const  USBD_CFG   *p_cfg,
                                                CPU_INT08U  if_grp_nbr)
{
    USBD_IF_GRP  *p_if_grp;
#if (USBD_CFG_OPTIMIZE_SPD == DEF_DISABLED)
    CPU_INT08U    if_grp_ix;
#endif

#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (if_grp_nbr >= p_cfg->IF_GrpNbrTotal) {
        return ((USBD_IF_GRP *)0);
    }
#endif

#if (USBD_CFG_OPTIMIZE_SPD == DEF_ENABLED)
    p_if_grp =  p_cfg->IF_GrpTblPtrs[if_grp_nbr];
#else
    p_if_grp =  p_cfg->IF_GrpHeadPtr;

    for (if_grp_ix = 0u; if_grp_ix < if_grp_nbr; if_grp_ix++) {
        p_if_grp = p_if_grp->NextPtr;
    }
#endif

    return (p_if_grp);
}
#endif


/*
*********************************************************************************************************
*                                           USBD_StrDescAdd()
*
* Description : Add string to USB device.
*
* Argument(s) : p_dev   Pointer to device structure.
*               -----   Argument validated in the caller(s).
*
*               p_str   Pointer to string to add (see Note #1).
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           USBD_ERR_NONE               String successfully added.
*                           USBD_ERR_ALLOC              String cannot be stored in strings table.
*
* Return(s)   : none.
*
* Note(s)     : (1) USB spec 2.0 chapter 9.5 states "Where appropriate, descriptors contain references
*                   to string descriptors that provide displayable information describing a descriptor
*                   in human-readable form. The inclusion of string descriptors is optional.  However,
*                   the reference fields within descriptors are mandatory. If a device does not support
*                   string descriptors, string reference fields must be reset to zero to indicate no
*                   string descriptor is available.
*
*                   Since string descriptors are optional, 'p_str' could be a NULL pointer.
*********************************************************************************************************
*/

#if (USBD_CFG_MAX_NBR_STR > 0u)
static  void  USBD_StrDescAdd (       USBD_DEV  *p_dev,
                               const  CPU_CHAR  *p_str,
                                      USBD_ERR  *p_err)
{
    CPU_INT08U  str_ix;
    CPU_SR_ALLOC();


    if (p_str == (CPU_CHAR *)0) {                               /* Return if NULL ptr.                                  */
        return;
    }

    for (str_ix = 0u; str_ix < p_dev->StrMaxIx; str_ix++) {
        if (p_str == p_dev->StrDesc_Tbl[str_ix]) {              /* Str already stored in tbl.                           */
           *p_err = USBD_ERR_NONE;
            return;
        }
    }

    CPU_CRITICAL_ENTER();
    str_ix = p_dev->StrMaxIx;                                   /* Get curr str tbl ix.                                 */

    if (str_ix >= USBD_CFG_MAX_NBR_STR) {                       /* Chk if str can be stored in tbl.                     */
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_ALLOC;
        return;
    }

    p_dev->StrDesc_Tbl[str_ix] = p_str;
    p_dev->StrMaxIx++;
    CPU_CRITICAL_EXIT();

   *p_err = USBD_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                           USBD_StrDescGet()
*
* Description : Get string pointer.
*
* Argument(s) : p_dev       Pointer to device.
*               -----       Argument validate by the caller(s).
*
*               str_nbr     Number of the string to obtain.
*
* Return(s)   : Pointer to requested string, if NO error(s).
*
*               Pointer to NULL,             otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  const  CPU_CHAR  *USBD_StrDescGet (const  USBD_DEV    *p_dev,
                                                  CPU_INT08U   str_nbr)
{
#if (USBD_CFG_MAX_NBR_STR > 0u)
    const  CPU_CHAR  *p_str;


    if (str_nbr > p_dev->StrMaxIx) {
        return ((CPU_CHAR *)0);
    }

    p_str = p_dev->StrDesc_Tbl[str_nbr];
    return (p_str);
#else
    (void)p_dev;
    (void)str_nbr;

    return ((CPU_CHAR *)0);
#endif
}


/*
*********************************************************************************************************
*                                          USBD_StrDescIxGet()
*
* Description : Get string index.
*
* Argument(s) : p_dev       Pointer to device.
*               -----       Argument validated in 'USBD_DevSetupPkt()' before posting the event to queue.
*
*               p_str       Pointer to string.
*
* Return(s)   : String index.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT08U  USBD_StrDescIxGet (const  USBD_DEV  *p_dev,
                                       const  CPU_CHAR  *p_str)
{
#if (USBD_CFG_MAX_NBR_STR > 0u)
    CPU_INT08U  str_ix;


    if (p_str == (CPU_CHAR *)0) {                               /* Return if a NULL pointer.                           */
        return (0u);
    }

    for (str_ix = 0u; str_ix < p_dev->StrMaxIx; str_ix++) {
        if (p_str == p_dev->StrDesc_Tbl[str_ix]) {              /* Str already stored in tbl.                           */
            return (str_ix + 1u);
        }
    }
#else
    (void)p_dev;
    (void)p_str;
#endif

    return (0u);
}


/*
*********************************************************************************************************
*                                             USBD_Dbg()
*
* Description : Debug standard request.
*
* Argument(s) : p_msg       Debug message to display.
*
*               ep_addr     Endpoint address.
*
*               if_nbr      Interface number.
*
*               err         Error code associated with the debug message.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
void  USBD_Dbg (const  CPU_CHAR    *p_msg,
                       CPU_INT08U   ep_addr,
                       CPU_INT08U   if_nbr,
                       USBD_ERR     err)
{
    USBD_DBG_EVENT  *p_event;


    if (p_msg == (const CPU_CHAR *)0) {
        return;
    }

    p_event = USBD_DbgEventGet();
    if (p_event != (USBD_DBG_EVENT *)0) {
        p_event->MsgPtr  = p_msg;
        p_event->EP_Addr = ep_addr;
        p_event->IF_Nbr  = if_nbr;
        p_event->ArgEn   = DEF_NO;
        p_event->Err     = err;
        USBD_DbgEventPut(p_event);
        USBD_OS_DbgEventRdy();
    }
}
#endif


/*
*********************************************************************************************************
*                                            USBD_DbgArg()
*
* Description : Debug standard request with argument.
*
* Argument(s) : p_msg       Debug message to display.
*
*               ep_addr     Endpoint address.
*
*               if_nbr      Interface number.
*
*               arg         Argument   associated with the debug message.
*
*               err         Error code associated with the debug message.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
void  USBD_DbgArg (const  CPU_CHAR    *p_msg,
                          CPU_INT08U   ep_addr,
                          CPU_INT08U   if_nbr,
                          CPU_INT32U   arg,
                          USBD_ERR     err)
{
    USBD_DBG_EVENT  *p_event;


    if (p_msg == (const CPU_CHAR *)0) {
        return;
    }

    p_event = USBD_DbgEventGet();
    if (p_event != (USBD_DBG_EVENT *)0) {
        p_event->MsgPtr  = p_msg;
        p_event->EP_Addr = ep_addr;
        p_event->IF_Nbr  = if_nbr;
        p_event->ArgEn   = DEF_YES;
        p_event->Arg     = arg;
        p_event->Err     = err;
        USBD_DbgEventPut(p_event);
        USBD_OS_DbgEventRdy();
    }
}
#endif


/*
*********************************************************************************************************
*                                        USBD_DbgTaskHandler()
*
* Description : Debug event process task.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Note(s)     : (1) This task processes all the debug events queued by the device stack.
*********************************************************************************************************
*/

#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
void  USBD_DbgTaskHandler (void)
{
    USBD_DBG_EVENT   *p_event;
    CPU_INT32U        ts_us;
    CPU_TS_TMR_FREQ   ts_freq;
    CPU_INT32U        ts_freq_mul;
    CPU_INT32U        ts_freq_div;
    CPU_INT32U        dbg_cnt_cur;
    CPU_INT32U        dbg_cnt_skip;
    CPU_CHAR          str[10u];
#if (CPU_CFG_TS_TMR_EN == DEF_ENABLED)
    CPU_ERR           err;
#endif
    CPU_SR_ALLOC();


    dbg_cnt_cur = 0u;
#if (CPU_CFG_TS_TMR_EN == DEF_ENABLED)
    ts_freq      = CPU_TS_TmrFreqGet(&err);                     /* Get TS clk freq.                                     */
    if (ts_freq == 0u) {
        ts_freq  = 1u;
    }
#else
    ts_freq = 1u;
#endif
                                                                /* Balanced division to convert TS to microseconds.     */
    if ((ts_freq % 1000000) == 0) {                             /* Favor power of 10 TS freq.                           */
         ts_freq_mul = 1;
         ts_freq_div = ts_freq / 1000000;
    } else if ((ts_freq % 100000) == 0) {
         ts_freq_mul = 10;
         ts_freq_div = ts_freq / 100000;
    } else if ((ts_freq % 10000) == 0) {
         ts_freq_mul = 100;
         ts_freq_div = ts_freq / 10000;
    } else if ((ts_freq % 1000) == 0) {
         ts_freq_mul = 1000;
         ts_freq_div = ts_freq / 1000;
    } else if ((ts_freq % 100) == 0) {
         ts_freq_mul = 10000;
         ts_freq_div = ts_freq / 100;
                                                                /* If not power of 10, keep some accuracy and ...       */
    } else if (ts_freq >= 1000000) {                            /* ... minimize overflow.                               */
         ts_freq_mul =  100;
         ts_freq_div = (ts_freq + 5000) / 10000;
    } else if (ts_freq >= 100000) {
         ts_freq_mul =  1000;
         ts_freq_div = (ts_freq +  500) / 1000;
    } else {
         ts_freq_mul =  10000;
         ts_freq_div = (ts_freq +   50) / 100;
    }

    while (DEF_TRUE) {
        USBD_OS_DbgEventWait();                                 /* Wait until event is avail.                           */

        CPU_CRITICAL_ENTER();
        p_event = USBD_DbgEventHeadPtr;
        if (USBD_DbgEventHeadPtr != (USBD_DBG_EVENT *)0) {
            USBD_DbgEventHeadPtr = p_event->NextPtr;
        }
        CPU_CRITICAL_EXIT();

        if (p_event != (USBD_DBG_EVENT *)0) {
            if (p_event->Cnt > dbg_cnt_cur) {                   /* Events skipped if event cnt greater than curr ctr.   */
                dbg_cnt_skip = p_event->Cnt - dbg_cnt_cur;
                USBD_Trace("USB  ");
                (void)Str_FmtNbr_Int32U(dbg_cnt_skip,
                                        DEF_INT_32U_NBR_DIG_MAX,
                                        DEF_NBR_BASE_DEC,
                                       '\0',
                                        DEF_NO,
                                        DEF_YES,
                                       &str[0u]);
                USBD_Trace(str);
                USBD_Trace("  Skipped event(s) \n\r");
                dbg_cnt_cur = p_event->Cnt;                     /* Match event ctr.                                     */
            }

            dbg_cnt_cur++;

            ts_us = (p_event->Ts * ts_freq_mul) / ts_freq_div;  /* Convert TS to microseconds.                          */

            USBD_Trace("USB  ");
            (void)Str_FmtNbr_Int32U(ts_us,
                                    DEF_INT_32U_NBR_DIG_MAX,
                                    DEF_NBR_BASE_DEC,
                                    ' ',
                                    DEF_NO,
                                    DEF_YES,
                                   &str[0u]);
            USBD_Trace(str);
            USBD_Trace("  ");
            if (p_event->EP_Addr != USBD_EP_ADDR_NONE) {
                (void)Str_FmtNbr_Int32U((CPU_INT32U)p_event->EP_Addr,
                                                    2u,
                                                    DEF_NBR_BASE_HEX,
                                                    ' ',
                                                    DEF_NO,
                                                    DEF_YES,
                                                   &str[0u]);
                USBD_Trace(str);
                USBD_Trace("  ");
            } else {
                USBD_Trace("    ");
            }

            if (p_event->IF_Nbr != USBD_IF_NBR_NONE) {
                (void)Str_FmtNbr_Int32U((CPU_INT32U)p_event->IF_Nbr,
                                                    DEF_INT_08U_NBR_DIG_MAX,
                                                    DEF_NBR_BASE_DEC,
                                                   '\0',
                                                    DEF_NO,
                                                    DEF_YES,
                                                   &str[0u]);
                USBD_Trace(str);
                USBD_Trace("  ");
            } else {
                USBD_Trace("     ");
            }


            USBD_Trace(p_event->MsgPtr);
            if (p_event->Err != USBD_ERR_NONE) {
                (void)Str_FmtNbr_Int32U((CPU_INT32U)p_event->Err,
                                                    DEF_INT_16U_NBR_DIG_MAX,
                                                    DEF_NBR_BASE_DEC,
                                                   '\0',
                                                    DEF_NO,
                                                    DEF_YES,
                                                   &str[0u]);
                USBD_Trace(str);
                USBD_Trace("  ");
            }

            if (p_event->ArgEn == DEF_YES) {
                (void)Str_FmtNbr_Int32U(p_event->Arg,
                                        DEF_INT_32U_NBR_DIG_MAX,
                                        DEF_NBR_BASE_DEC,
                                       '\0',
                                        DEF_NO,
                                        DEF_YES,
                                       &str[0u]);

                USBD_Trace("  ");
                USBD_Trace(str);
                USBD_Trace("  ");
            }
            USBD_Trace("\n\r");

            USBD_DbgEventFree(p_event);                         /* Return debug event to free pool.                     */
        }
    }
}
#endif


/*
*********************************************************************************************************
*                                         USBD_DbgEventGet()
*
* Description : Get debug event object.
*
* Argument(s) : none.
*
* Return(s)   : Pointer to debug event object, if NO error(s).
*
*               Pointer to NULL,               otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
static  USBD_DBG_EVENT  *USBD_DbgEventGet (void)
{
    USBD_DBG_EVENT  *p_event;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    if (USBD_DbgEventFreePtr == (USBD_DBG_EVENT *)0) {
        USBD_DbgEventCtr++;
        CPU_CRITICAL_EXIT();
        return ((USBD_DBG_EVENT *)0);
    }

    p_event              = USBD_DbgEventFreePtr;
    p_event->Cnt         = USBD_DbgEventCtr;
    USBD_DbgEventFreePtr = USBD_DbgEventFreePtr->NextPtr;
    USBD_DbgEventCtr++;
    CPU_CRITICAL_EXIT();

#if (CPU_CFG_TS_TMR_EN == DEF_ENABLED)
    p_event->Ts      =  CPU_TS_Get32();
#else
    p_event->Ts      =  0u;
#endif
    p_event->NextPtr = (USBD_DBG_EVENT *)0;

    return (p_event);
}
#endif


/*
*********************************************************************************************************
*                                         USBD_DbgEventPut()
*
* Description : Queue a debug event object.
*
* Argument(s) : p_event   Pointer to debug event to queue.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
static  void  USBD_DbgEventPut (USBD_DBG_EVENT  *p_event)
{
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    if (USBD_DbgEventHeadPtr == (USBD_DBG_EVENT *)0) {
        USBD_DbgEventHeadPtr = p_event;
    } else {
        USBD_DbgEventTailPtr->NextPtr = p_event;
    }
    USBD_DbgEventTailPtr = p_event;
    CPU_CRITICAL_EXIT();
}
#endif


/*
*********************************************************************************************************
*                                         USBD_DbgEventFree()
*
* Description : Return a debug event object to the free list.
*
* Argument(s) : p_event   Pointer to debug event to return.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
static  void  USBD_DbgEventFree (USBD_DBG_EVENT  *p_event)
{
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    if (USBD_DbgEventFreePtr == (USBD_DBG_EVENT *)0) {
        p_event->NextPtr = (USBD_DBG_EVENT *)0;
    } else {
        p_event->NextPtr = USBD_DbgEventFreePtr;
    }
    USBD_DbgEventFreePtr = p_event;
    CPU_CRITICAL_EXIT();
}
#endif
