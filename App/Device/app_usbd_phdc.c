/*
*********************************************************************************************************
*                                            EXAMPLE CODE
*
*               This file is provided as an example on how to use Micrium products.
*
*               Please feel free to use any application code labeled as 'EXAMPLE CODE' in
*               your application products.  Example code may be used as is, in whole or in
*               part, or may be used as a reference only. This file can be modified as
*               required to meet the end-product requirements.
*
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                  USB DEVICE PHDC TEST APPLICATION
*
*                                              TEMPLATE
*
* Filename : app_usbd_phdc.c
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <app_usbd.h>

#if (APP_CFG_USBD_EN      == DEF_ENABLED) && \
    (APP_CFG_USBD_PHDC_EN == DEF_ENABLED)
#include  <Class/PHDC/usbd_phdc.h>
#include  <Source/os.h>


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  APP_USBD_PHDC_ITEM_DATA_LEN_MIN                   5u
#define  APP_USBD_PHDC_ITEM_Q_NBR_MAX                     APP_CFG_USBD_PHDC_ITEM_NBR_MAX
#define  APP_USBD_PHDC_ITEM_DFLT_PERIOD                  100u

#ifndef  APP_CFG_USBD_PHDC_MAX_NBR_TASKS
#define  APP_CFG_USBD_PHDC_MAX_NBR_TASKS                   1u
#endif


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                        APPLICATION COMMANDS
*********************************************************************************************************
*/

typedef  enum  app_usbd_phdc_cmds {
    APP_USBD_PHDC_CMD_NONE     = 0u,
    APP_USBD_PHDC_CMD_RESET    = 1u,
    APP_USBD_PHDC_CMD_ADD_ITEM = 2u
} APP_USBD_PHDC_CMDS;


/*
*********************************************************************************************************
*                                          APPLICATION ITEM
*
* Note(s): (1) Data buffer of each item has the following mapping:
*
*              +---------+---------------+-------------------+-----------------------------------+
*              | Offset  |     Field     |       Size        |            Description            |
*              +---------+---------------+-------------------+-----------------------------------+
*              |    0    | Cmd           |         1         | Command from host.                |
*              +---------+---------------+-------------------+-----------------------------------+
*              |    1    | Item index    |         1         | Index of item.                    |
*              +---------+---------------+-------------------+-----------------------------------+
*              |    2    | Period (LSB)  |         1         | Periodicity of transfer.          |
*              +---------+---------------+-------------------+-----------------------------------+
*              |    3    | Period (MSB)  |         1         | Periodicity of transfer.          |
*              +---------+---------------+-------------------+-----------------------------------+
*              |    4    | Latency /     |         1         | Latency / reliability (QoS) of    |
*              |         | Reliability   |                   | transfer.                         |
*              +---------+---------------+-------------------+-----------------------------------+
*              |    5    | Data          | [0..DATA_LEN_MAX] | Data.                             |
*              +---------+---------------+-------------------+-----------------------------------+
*
*********************************************************************************************************
*/

typedef  struct  app_usbd_phdc_item {
    CPU_INT16U           Period;                                /* Periodicity of xfer.                                 */
    LATENCY_RELY_FLAGS   LatencyRely;                           /* Latency / rely of xfer.                              */
    CPU_INT08U           Data[APP_CFG_USBD_PHDC_ITEM_DATA_LEN_MAX];
    CPU_INT08U           DataOpaque[USBD_PHDC_CFG_DATA_OPAQUE_MAX_LEN];
    CPU_INT16U           DataLen;
    CPU_INT08U           DataOpaqueLen;
    OS_TMR              *Tmr;
} APP_USBD_PHDC_ITEM;


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/

static  const  CPU_INT08U  App_USBD_PHDC_OpaqueDataRx[]           = {"Micrium PHDC Rx"};
static  const  CPU_INT08U  App_USBD_PHDC_OpaqueDataTx[]           = {"Micrium PHDC Tx"};
static  const  CPU_INT08U  App_USBD_PHDC_OpaqueDataTxLowLatency[] = {"Micrium PHDC Tx low latency"};


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

static  OS_EVENT            *App_USBD_PHDC_ItemQ[APP_CFG_USBD_PHDC_MAX_NBR_TASKS];
static  void                *App_USBD_PHDC_ItemQBuffer[APP_CFG_USBD_PHDC_MAX_NBR_TASKS][APP_USBD_PHDC_ITEM_Q_NBR_MAX];

static  CPU_STK              App_USBD_PHDC_TxCommTaskStk[APP_CFG_USBD_PHDC_MAX_NBR_TASKS][APP_CFG_USBD_PHDC_TASK_STK_SIZE];
static  CPU_STK              App_USBD_PHDC_RxCommTaskStk[APP_CFG_USBD_PHDC_TASK_STK_SIZE];

static  CPU_INT16U           App_USBD_PHDC_Ctr;
static  CPU_BOOLEAN          App_USBD_PHDC_PreambleEn;
static  CPU_INT08U           App_USBD_PHDC_ClassNbr;

static  APP_USBD_PHDC_ITEM   App_USBD_PHDC_Items[APP_CFG_USBD_PHDC_ITEM_NBR_MAX];
static  CPU_INT16U           App_USBD_PHDC_ItemNbrNext;


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

        CPU_BOOLEAN  App_USBD_PHDC_Init         (CPU_INT08U    dev_nbr,
                                                 CPU_INT08U    cfg_hs,
                                                 CPU_INT08U    cfg_fs);

static  void         App_USBD_PHDC_RxCommTask   (void         *p_arg);

static  void         App_USBD_PHDC_TxCommTask   (void         *p_arg);

static  void         App_USBD_PHDC_SetPreambleEn(CPU_INT08U    class_nbr,
                                                 CPU_BOOLEAN   preamble_en);

static  void         App_USBD_PHDC_TmrCallback  (void         *p_tmr,
                                                 void         *p_arg);


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/

#if  (OS_TMR_CFG_TICKS_PER_SEC < 100u)
#error  "OS_TMR_CFG_TICKS_PER_SEC illegally #define'd in 'os_cfg.h'[MUST be >= 100]"
#endif

#if  (OS_TASK_STAT_EN == 0u)
#error  "OS_TASK_STAT_EN illegally #define'd in 'os_cfg.h' [MUST be DEF_ENABLED]"
#endif

#if  (OS_TMR_EN == 0u)
#error  "OS_TMR_EN illegally #define'd in 'os_cfg.h' [MUST be DEF_ENABLED]"
#endif


/*
*********************************************************************************************************
*                                          GLOBAL FUNCTIONS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                        App_USBD_PHDC_Init()
*
* Description : Add a PHDC interface to the USB device stack and initialize it.
*
* Argument(s) : dev_nbr    Device number.
*
*               cfg_hs     Index of high-speed configuration to which this interface will be added to.
*
*               cfg_fs     Index of high-speed configuration to which this interface will be added to.
*
* Return(s)   : DEF_OK,    if PHDC interface successfully added.
*
*               DEF_FAIL,  otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  App_USBD_PHDC_Init (CPU_INT08U  dev_nbr,
                                 CPU_INT08U  cfg_hs,
                                 CPU_INT08U  cfg_fs)
{
    CPU_BOOLEAN          valid_cfg_hs;
    CPU_BOOLEAN          valid_cfg_fs;
    CPU_INT08U           cnt;
    CPU_INT32U           tmr_period;
    LATENCY_RELY_FLAGS   latency_rely_flags;
    APP_USBD_PHDC_ITEM  *p_dflt_item;
    USBD_ERR             err;
    OS_ERR               os_err;


    valid_cfg_hs = DEF_NO;
    valid_cfg_fs = DEF_NO;

    APP_TRACE_DBG(("        Initializing PHDC class ... \r\n"));

    USBD_PHDC_Init(&err);                                       /* ------------------- INIT PHDC ---------------------- */
    if (err != USBD_ERR_NONE) {
        return (DEF_FAIL);
    }
                                                                /* Add PHDC instance.                                   */
    App_USBD_PHDC_ClassNbr = USBD_PHDC_Add(DEF_NO,              /* Vendor-defined data format.                          */
                                           DEF_YES,             /* Preamble capable.                                    */
                                           App_USBD_PHDC_SetPreambleEn,
                                           8u,
                                          &err);
    if (err != USBD_ERR_NONE) {
        return (DEF_FAIL);
    }

    latency_rely_flags = USBD_PHDC_LATENCY_VERYHIGH_RELY_BEST |
                         USBD_PHDC_LATENCY_HIGH_RELY_BEST     |
                         USBD_PHDC_LATENCY_MEDIUM_RELY_BEST;
    USBD_PHDC_RdCfg(App_USBD_PHDC_ClassNbr,                     /* Cfg rd xfers with all possible latency/rely flags.   */
                    latency_rely_flags,
                    App_USBD_PHDC_OpaqueDataRx,
                    sizeof(App_USBD_PHDC_OpaqueDataRx),
                   &err);
    if (err != USBD_ERR_NONE) {
        return (DEF_FAIL);
    }

    latency_rely_flags = USBD_PHDC_LATENCY_VERYHIGH_RELY_BEST |
                         USBD_PHDC_LATENCY_HIGH_RELY_BEST     |
                         USBD_PHDC_LATENCY_MEDIUM_RELY_BEST   |
                         USBD_PHDC_LATENCY_MEDIUM_RELY_BETTER |
                         USBD_PHDC_LATENCY_MEDIUM_RELY_GOOD;
    USBD_PHDC_WrCfg(App_USBD_PHDC_ClassNbr,                     /* Cfg very high, high and medium xfers with metadata.  */
                    latency_rely_flags,
                    App_USBD_PHDC_OpaqueDataTx,
                    sizeof(App_USBD_PHDC_OpaqueDataTx),
                   &err);

    if (err != USBD_ERR_NONE) {
        return (DEF_FAIL);
    }

    USBD_PHDC_WrCfg(App_USBD_PHDC_ClassNbr,                     /* Cfg low latency xfers with specific metadadata.      */
                    USBD_PHDC_LATENCY_LOW_RELY_GOOD,
                    App_USBD_PHDC_OpaqueDataTxLowLatency,
                    sizeof(App_USBD_PHDC_OpaqueDataTxLowLatency),
                   &err);

    if (err != USBD_ERR_NONE) {
        return (DEF_FAIL);
    }

    if (cfg_hs != USBD_CFG_NBR_NONE) {
        valid_cfg_hs = USBD_PHDC_CfgAdd(App_USBD_PHDC_ClassNbr,
                                        dev_nbr,
                                        cfg_hs,
                                       &err);                   /* Add PHDC class to HS dflt cfg.                       */
    }

    if (cfg_fs != USBD_CFG_NBR_NONE) {
        valid_cfg_fs = USBD_PHDC_CfgAdd(App_USBD_PHDC_ClassNbr,
                                        dev_nbr,
                                        cfg_fs,
                                       &err);                   /* Add PHDC class to FS dflt cfg.                       */
    }

    if ((valid_cfg_fs == DEF_NO) &&
        (valid_cfg_hs == DEF_NO)) {
        return (DEF_FAIL);
    }

                                                                /* -------------------- INIT APP ---------------------- */
    p_dflt_item                = &App_USBD_PHDC_Items[0u];      /* Init dflt item.                                      */
    p_dflt_item->Period        =  APP_USBD_PHDC_ITEM_DFLT_PERIOD;
    p_dflt_item->LatencyRely   =  USBD_PHDC_LATENCY_LOW_RELY_GOOD;
    p_dflt_item->DataLen       =  APP_USBD_PHDC_ITEM_DATA_LEN_MIN + 3u;
    p_dflt_item->DataOpaqueLen =  0u;
    p_dflt_item->Data[1u]      =  0u;                           /* See Note 1 of APPLICATION ITEM section.              */
    p_dflt_item->Data[2u]      = (CPU_INT08U)APP_USBD_PHDC_ITEM_DFLT_PERIOD;
    p_dflt_item->Data[3u]      = (CPU_INT08U)(APP_USBD_PHDC_ITEM_DFLT_PERIOD >> 8u);
    p_dflt_item->Data[5u]      =  0u;                           /* Ctr value, 0 by dflt.                                */
    p_dflt_item->Data[6u]      =  0u;
    p_dflt_item->Data[7u]      =  OSCPUUsage;

    tmr_period = ((((INT32U)p_dflt_item->Period * OS_TMR_CFG_TICKS_PER_SEC) + 1000u - 1u) / 1000u);
    p_dflt_item->Tmr = OSTmrCreate(         tmr_period,         /* Create tmr for dflt item.                            */
                                            0,
                                            OS_TMR_OPT_ONE_SHOT,
                                            App_USBD_PHDC_TmrCallback,
                                   (void  *)p_dflt_item,
                                   (INT8U *)0,
                                           &os_err);
    if (os_err != OS_ERR_NONE) {
        APP_TRACE_DBG(("Could not add default item timer w/err = %d\r\n", os_err));
        return (DEF_FAIL);
    }

                                                                /* Create one Q per xfer priority.                      */
    for (cnt = 0u; cnt < APP_CFG_USBD_PHDC_MAX_NBR_TASKS; cnt++) {
        App_USBD_PHDC_ItemQ[cnt] = OSQCreate(&App_USBD_PHDC_ItemQBuffer[cnt][0u],
                                              APP_USBD_PHDC_ITEM_Q_NBR_MAX);
        if (App_USBD_PHDC_ItemQ[cnt] == (OS_EVENT *)0) {
            APP_TRACE_DBG(("Could not create item Q w/err = %d\r\n", os_err));
            return (DEF_FAIL);
        }
    }

    App_USBD_PHDC_PreambleEn  = DEF_DISABLED;                   /* Metadata msg preamble disabled by default.           */
    App_USBD_PHDC_ItemNbrNext = 1u;
    App_USBD_PHDC_Ctr         = 0u;
                                                                /* Create task that will handle rd procedures.          */
#if (OS_TASK_CREATE_EXT_EN == 1u)
#if (OS_STK_GROWTH == 1u)
    os_err = OSTaskCreateExt(        App_USBD_PHDC_RxCommTask,
                             (void *)0,
                                    &App_USBD_PHDC_RxCommTaskStk[APP_CFG_USBD_PHDC_TASK_STK_SIZE - 1u],
                                     APP_CFG_USBD_PHDC_RX_COMM_TASK_PRIO,
                                     APP_CFG_USBD_PHDC_RX_COMM_TASK_PRIO,
                                    &App_USBD_PHDC_RxCommTaskStk[0],
                                     APP_CFG_USBD_PHDC_TASK_STK_SIZE,
                             (void *)0,
                                     OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK);
#else
    os_err = OSTaskCreateExt(        App_USBD_PHDC_RxCommTask,
                             (void *)0,
                                    &App_USBD_PHDC_RxCommTaskStk[0],
                                     APP_CFG_USBD_PHDC_RX_COMM_TASK_PRIO,
                                     APP_CFG_USBD_PHDC_RX_COMM_TASK_PRIO,
                                    &App_USBD_PHDC_RxCommTaskStk[APP_CFG_USBD_PHDC_TASK_STK_SIZE - 1u],
                                     APP_CFG_USBD_PHDC_TASK_STK_SIZE,
                             (void *)0,
                                     OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK);
#endif

#else                                                           /* Ext task creation is not avail.                      */

#if (OS_STK_GROWTH == 1u)
    os_err = OSTaskCreate(        App_USBD_PHDC_RxCommTask,
                          (void *)0,
                                 &App_USBD_PHDC_RxCommTaskStk[APP_CFG_USBD_PHDC_TASK_STK_SIZE - 1u],
                                  APP_CFG_USBD_PHDC_RX_COMM_TASK_PRIO);
#else
    os_err = OSTaskCreate(        App_USBD_PHDC_RxCommTask,
                          (void *)0,
                                 &App_USBD_PHDC_RxCommTaskStk[0],
                                  APP_CFG_USBD_PHDC_RX_COMM_TASK_PRIO);
#endif
#endif

    if (os_err != OS_ERR_NONE) {
        APP_TRACE_DBG(("Could not add PHDC rx comm task w/err = %d\r\n", os_err));
        return (DEF_FAIL);
    }

#if (OS_TASK_NAME_EN > 0)
    OSTaskNameSet(APP_CFG_USBD_PHDC_RX_COMM_TASK_PRIO, (INT8U *)"USB Device PHDC rx comm", &os_err);
#endif

                                                                /* Create one tx task per prio (QoS).                   */
    for (cnt = 0; cnt < APP_CFG_USBD_PHDC_MAX_NBR_TASKS; cnt++) {
#if (OS_TASK_CREATE_EXT_EN == 1u)                               /* Ext task creation is avail.                          */
#if (OS_STK_GROWTH == 1u)
        os_err = OSTaskCreateExt(                  App_USBD_PHDC_TxCommTask,
                                 (void *)(CPU_ADDR)cnt,
                                                  &App_USBD_PHDC_TxCommTaskStk[cnt][APP_CFG_USBD_PHDC_TASK_STK_SIZE - 1u],
                                                   APP_CFG_USBD_PHDC_TX_COMM_TASK_PRIO + cnt,
                                                   APP_CFG_USBD_PHDC_TX_COMM_TASK_PRIO + cnt,
                                                  &App_USBD_PHDC_TxCommTaskStk[cnt][0u],
                                                   APP_CFG_USBD_PHDC_TASK_STK_SIZE,
                                 (void *)          0,
                                                   OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK);
#else
        os_err = OSTaskCreateExt(                  App_USBD_PHDC_TxCommTask,
                                 (void *)(CPU_ADDR)cnt,
                                                  &App_USBD_PHDC_TxCommTaskStk[cnt][0u],
                                                   APP_CFG_USBD_PHDC_TX_COMM_TASK_PRIO + cnt,
                                                   APP_CFG_USBD_PHDC_TX_COMM_TASK_PRIO + cnt,
                                                  &App_USBD_PHDC_TxCommTaskStk[cnt][APP_CFG_USBD_PHDC_TASK_STK_SIZE - 1u],
                                                   APP_CFG_USBD_PHDC_TASK_STK_SIZE,
                                 (void *)          0,
                                                   OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK);
#endif

#else                                                           /* Ext task creation is not avail.                      */

#if (OS_STK_GROWTH == 1u)
        os_err = OSTaskCreate(                  App_USBD_PHDC_TxCommTask,
                              (void *)(CPU_ADDR)cnt,
                                               &App_USBD_PHDC_TxCommTaskStk[cnt][APP_CFG_USBD_PHDC_TASK_STK_SIZE - 1u],
                                                APP_CFG_USBD_PHDC_TX_COMM_TASK_PRIO + cnt);
#else
        os_err = OSTaskCreate(                  App_USBD_PHDC_TxCommTask,
                              (void *)(CPU_ADDR)cnt,
                                               &App_USBD_PHDC_TxCommTaskStk[cnt][0u],
                                                APP_CFG_USBD_PHDC_TX_COMM_TASK_PRIO + cnt);
#endif

#endif
        if (os_err != OS_ERR_NONE) {
            APP_TRACE_DBG(("Could not add PHDC tx comm task w/err = %d\r\n", os_err));
            return (DEF_FAIL);
        }

#if (OS_TASK_NAME_EN > 0)
        OSTaskNameSet(APP_CFG_USBD_PHDC_RX_COMM_TASK_PRIO, (INT8U *)"USBD PHDC rx comm", &os_err);
#endif

    }

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                     App_USBD_PHDC_RxCommTask()
*
* Description : Read PHDC data and opaque data from host. Then take action in function of the application
*               command sent by the host.
*
* Argument(s) : p_arg    Argument passed to the task.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  App_USBD_PHDC_RxCommTask (void *p_arg)
{
    CPU_BOOLEAN          is_conn;
    CPU_INT08U           data_opaque_len;
    CPU_INT08U           data_opaque[USBD_PHDC_CFG_DATA_OPAQUE_MAX_LEN];
    CPU_INT08U           data[APP_CFG_USBD_PHDC_ITEM_DATA_LEN_MAX];
    CPU_INT08U           nbr_xfer;
    CPU_INT16U           cnt;
    CPU_INT16U           xfer_len;
    CPU_INT32U           tmr_period;
    APP_USBD_PHDC_ITEM  *p_dflt_item;
    APP_USBD_PHDC_ITEM  *p_item;
    USBD_ERR             usb_err;
    OS_ERR               os_err;


    (void)p_arg;
    p_dflt_item = &App_USBD_PHDC_Items[0u];

    while (DEF_TRUE) {
        is_conn = USBD_PHDC_IsConn(App_USBD_PHDC_ClassNbr);     /* Wait for configuration.                              */
        while (is_conn == DEF_NO) {
            OSTimeDlyHMSM(0u, 0u, 0u, 100u);
            is_conn = USBD_PHDC_IsConn(App_USBD_PHDC_ClassNbr);
        }

        data_opaque_len = 0u;
        if (App_USBD_PHDC_PreambleEn == DEF_ENABLED) {          /* Host decide of preamble state.                       */
            data_opaque_len = USBD_PHDC_PreambleRd(        App_USBD_PHDC_ClassNbr,
                                                   (void *)data_opaque,
                                                           USBD_PHDC_CFG_DATA_OPAQUE_MAX_LEN,
                                                          &nbr_xfer,
                                                           0u,
                                                          &usb_err);

            if ((usb_err  != USBD_ERR_NONE) ||
                (nbr_xfer != 1u           )) {
                if (usb_err != USBD_ERR_OS_ABORT) {             /* Host changed preamble state when xfer is aborted.    */
                    APP_TRACE_DBG(("Rd preamble failed w/err = %d\r\n", usb_err));
                    OSTimeDlyHMSM(0u, 0u, 0u, 250u);
                }
                continue;
            }
        }

        xfer_len = USBD_PHDC_Rd(        App_USBD_PHDC_ClassNbr,
                                (void *)data,
                                        APP_CFG_USBD_PHDC_ITEM_DATA_LEN_MAX,
                                        0u,
                                       &usb_err);
        if (usb_err != USBD_ERR_NONE) {
            if (usb_err != USBD_ERR_OS_ABORT) {
                APP_TRACE_DBG(("Rd failed w/err = %d\r\n", usb_err));
                OSTimeDlyHMSM(0u, 0u, 0u, 250u);
            }
            continue;
        }

        switch (data[0u]) {                                     /* First data octets is the cmd.                        */
            case APP_USBD_PHDC_CMD_ADD_ITEM:                    /* Host added a new item.                               */
                 if (App_USBD_PHDC_ItemNbrNext < APP_CFG_USBD_PHDC_ITEM_NBR_MAX) {
                     p_item                = &App_USBD_PHDC_Items[App_USBD_PHDC_ItemNbrNext];
                     p_item->Period        = (CPU_INT16U) data[2u];
                     p_item->Period       += (CPU_INT16U)(data[3u] << 8u);
                     p_item->LatencyRely   =  data[4u];
                     p_item->DataLen       =  xfer_len;
                     p_item->DataOpaqueLen =  data_opaque_len;

                     Mem_Copy(p_item->Data,
                              data,
                              xfer_len);

                     Mem_Copy(p_item->DataOpaque,
                              data_opaque,
                              data_opaque_len);

                     tmr_period = ((((INT32U)p_item->Period * OS_TMR_CFG_TICKS_PER_SEC)  + 1000u - 1u) / 1000u);
                     p_item->Tmr = OSTmrCreate(tmr_period,      /* Create tmr for new item.                             */
                                               0,
                                               OS_TMR_OPT_ONE_SHOT,
                                               App_USBD_PHDC_TmrCallback,
                                      (void  *)p_item,
                                      (INT8U *)0,
                                              &os_err);
                     if (os_err != OS_ERR_NONE) {
                         APP_TRACE_DBG(("Could not create timer w/err = %d\r\n", os_err));
                         break;
                     }

                     (void)OSTmrStart(p_item->Tmr,
                                     &os_err);
                     if (os_err != OS_ERR_NONE) {
                         APP_TRACE_DBG(("Timer start failed w/err = %d\r\n", os_err));
                         break;
                     }

                     App_USBD_PHDC_ItemNbrNext++;
                 }
                 break;

            case APP_USBD_PHDC_CMD_RESET:                       /* Host send reset cmd when app start.                  */
                 USBD_PHDC_Reset(App_USBD_PHDC_ClassNbr);       /* Reset PHDC instance.                                 */

                                                                /* Flush all Qs.                                        */
                 for (cnt = 0; cnt < APP_CFG_USBD_PHDC_MAX_NBR_TASKS; cnt ++) {
                     os_err = OSQFlush(App_USBD_PHDC_ItemQ[cnt]);
                     if (os_err != OS_ERR_NONE) {
                         APP_TRACE_DBG(("Could not flush queue w/err = %d\r\n", os_err));
                     }
                 }

                                                                /* Delete all items tmr.                                */
                 for (cnt = 1u; cnt < App_USBD_PHDC_ItemNbrNext; cnt++) {
                     (void)OSTmrDel(App_USBD_PHDC_Items[cnt].Tmr,
                                   &os_err);
                     if (os_err != OS_ERR_NONE) {
                         APP_TRACE_DBG(("Could not delete timer w/err = %d\r\n", os_err));
                     }
                 }
                 App_USBD_PHDC_ItemNbrNext = 1;
                                                                /* Start dflt item tmr.                                 */
                 (void)OSTmrStart(p_dflt_item->Tmr,
                                 &os_err);
                 if (os_err != OS_ERR_NONE) {
                     APP_TRACE_DBG(("Could not start default item timer w/err = %d\r\n", os_err));
                 }
                 break;

            default:
                 break;
        }
    }
}


/*
*********************************************************************************************************
*                                     App_USBD_PHDC_TxCommTask()
*
* Description : Write next item data in queue along with its preamble.
*
* Argument(s) : p_arg    Argument passed to the task.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  App_USBD_PHDC_TxCommTask (void  *p_arg)
{
    CPU_BOOLEAN          is_conn;
    CPU_INT08U           nbr;
    APP_USBD_PHDC_ITEM  *p_item;
    USBD_ERR             usb_err;
    OS_ERR               os_err;


    nbr = (CPU_INT08U)(CPU_ADDR)p_arg;                          /* p_arg contain task nbr.                              */

    while (DEF_TRUE) {
        is_conn = USBD_PHDC_IsConn(App_USBD_PHDC_ClassNbr);     /* Wait for configuration.                              */
        while (is_conn == DEF_NO) {
            OSTimeDlyHMSM(0u, 0u, 0u, 100u);
            is_conn = USBD_PHDC_IsConn(App_USBD_PHDC_ClassNbr);
        }
                                                                /* Wait for data to wr on Q.                            */
        p_item = (APP_USBD_PHDC_ITEM *)OSQPend(App_USBD_PHDC_ItemQ[nbr],
                                               0,
                                              &os_err);
        if (os_err != OS_ERR_NONE) {
            APP_TRACE_DBG(("Queue pend failed w/err = %d\r\n", os_err));
            OSTimeDlyHMSM(0u, 0u, 0u, 100u);
            continue;
        }

        if (p_item->Data[1u] == 0u) {                           /* If dflt item, update ctr and CPU usage value.        */
            App_USBD_PHDC_Ctr++;
            p_item->Data[5u] = (CPU_INT08U) App_USBD_PHDC_Ctr;
            p_item->Data[6u] = (CPU_INT08U)(App_USBD_PHDC_Ctr >> 8u);
            p_item->Data[7u] =  OSCPUUsage;
        }

        usb_err = USBD_ERR_NONE;                                /* Write preamble (if applicable) and data to host.     */
        if ((App_USBD_PHDC_PreambleEn == DEF_ENABLED                    ) &&
            (p_item->LatencyRely      != USBD_PHDC_LATENCY_LOW_RELY_GOOD)) {

            USBD_PHDC_PreambleWr(        App_USBD_PHDC_ClassNbr,
                                 (void *)p_item->DataOpaque,
                                         p_item->DataOpaqueLen,
                                         p_item->LatencyRely,
                                         1u,
                                         0u,
                                        &usb_err);

            if (usb_err != USBD_ERR_NONE) {
                APP_TRACE_DBG(("Could not write PHDC item #%d preamble w/err = %d\r\n",
                                p_item->Data[1],
                                usb_err));
            }
        }

        if (usb_err == USBD_ERR_NONE) {
            USBD_PHDC_Wr(        App_USBD_PHDC_ClassNbr,
                         (void *)p_item->Data,
                                 p_item->DataLen,
                                 p_item->LatencyRely,
                                 0u,
                                &usb_err);

            if (usb_err != USBD_ERR_NONE) {
                APP_TRACE_DBG(("Could not write PHDC item #%d w/err = %d\r\n",
                               p_item->Data[1u],
                               usb_err));
            }
        }

        if (usb_err != USBD_ERR_OS_ABORT) {                     /* Start timer only if xfer not been aborted.           */
            (void)OSTmrStart(p_item->Tmr,
                            &os_err);
            if (os_err != OS_ERR_NONE) {
                APP_TRACE_DBG(("Timer start failed w/err = %d\r\n", os_err));
            }
        }
    }
}


/*
*********************************************************************************************************
*                                    App_USBD_PHDC_SetPreambleEn()
*
* Description : Callback called when host enable/disable preambles.
*
* Argument(s) : class_nbr      PHDC instance number.
*
*               preamble_en    DEF_ENABLED,  if preambles are enabled
*                              DEF_DISABLED, otherwise
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  App_USBD_PHDC_SetPreambleEn (CPU_INT08U   class_nbr,
                                           CPU_BOOLEAN  preamble_en)
{
    (void)class_nbr;

    App_USBD_PHDC_PreambleEn = preamble_en;
}


/*
*********************************************************************************************************
*                                     App_USBD_PHDC_TmrCallback()
*
* Description : Callback called upon timer count reach 0.
*
* Argument(s) : p_tmr    Pointer to timer structure.
*
*               p_arg    Pointer to application argument.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  App_USBD_PHDC_TmrCallback (void  *p_tmr,
                                         void  *p_arg)
{
    CPU_INT08U           nbr;
    APP_USBD_PHDC_ITEM  *p_item;
    OS_ERR               os_err;


    (void)p_tmr;
    p_item = (APP_USBD_PHDC_ITEM *)p_arg;
    nbr    =  CPU_CntTrailZeros08(p_item->LatencyRely);

                                                                /* Post item on Q.                                      */
    os_err = OSQPost(        App_USBD_PHDC_ItemQ[nbr],
                     (void *)p_item);
    if (os_err != OS_ERR_NONE) {
        APP_TRACE_DBG(("Q post failed w/err %d\r\n", os_err));
        return;
    }
}
#endif
