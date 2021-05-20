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
*                         USB DEVICE VENDOR CLASS APPLICATION INITIALIZATION
*
*                                              TEMPLATE
*
* Filename : app_usbd_vendor.c
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <app_usbd.h>

#if (APP_CFG_USBD_EN        == DEF_ENABLED) && \
    (APP_CFG_USBD_VENDOR_EN == DEF_ENABLED)

#include  <Class/Vendor/usbd_vendor.h>
#include  <Source/os.h>


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#ifndef OS_VERSION
#error "OS_VERSION must be #define'd."
#endif

#if    (OS_VERSION > 30000u)
#define  APP_USBD_VENDOR_OS_III_EN          DEF_ENABLED
#else
#define  APP_USBD_VENDOR_OS_III_EN          DEF_DISABLED
#endif

#define  APP_RX_TIMEOUT_MS              5000u
#define  APP_TX_TIMEOUT_MS              5000u


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/

#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
static  const  CPU_INT08U  App_USBD_Vendor_MS_PropertyNameGUID[] = {
    'D', 0u, 'e', 0u, 'v', 0u, 'i', 0u, 'c', 0u, 'e', 0u,
    'I', 0u, 'n', 0u, 't', 0u, 'e', 0u, 'r', 0u, 'f', 0u, 'a', 0u, 'c', 0u, 'e', 0u,
    'G', 0u, 'U', 0u, 'I', 0u, 'D', 0u, 0u,  0u
};

static  const  CPU_INT08U  App_USBD_Vendor_MS_GUID[] = {
    '{', 0u, '1', 0u, '4', 0u, '3', 0u, 'F', 0u, '2', 0u, '0', 0u, 'B', 0u, 'D', 0u,
    '-', 0u, '7', 0u, 'B', 0u, 'D', 0u, '2', 0u, '-', 0u, '4', 0u, 'C', 0u, 'A', 0u, '6', 0u,
    '-', 0u, '9', 0u, '4', 0u, '6', 0u, '5', 0u,
    '-', 0u, '8', 0u, '8', 0u, '8', 0u, '2', 0u, 'F', 0u, '2', 0u, '1', 0u, '5', 0u, '6', 0u, 'B', 0u, 'D', 0u, '6', 0u,  '}', 0u, 0u, 0u
};
#endif


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

#if (APP_CFG_USBD_VENDOR_ECHO_SYNC_EN  == DEF_ENABLED)
#if (APP_USBD_VENDOR_OS_III_EN == DEF_ENABLED)
static  OS_TCB       App_USBD_Vendor_EchoSyncTaskTCB;
#endif
static  CPU_STK      App_USBD_Vendor_EchoSyncTaskStk[APP_CFG_USBD_VENDOR_TASK_STK_SIZE];
static  CPU_INT08U   App_USBD_Vendor_Buf[512u];
#endif

#if (APP_CFG_USBD_VENDOR_ECHO_ASYNC_EN == DEF_ENABLED)
#if (APP_USBD_VENDOR_OS_III_EN == DEF_ENABLED)
static  OS_TCB       App_USBD_Vendor_EchoAsyncTaskTCB;
static  OS_SEM       App_USBD_Vendor_EchoAsyncSem;
#else
static  OS_EVENT    *App_USBD_Vendor_EchoAsyncSemPtr;
#endif
static  CPU_STK      App_USBD_Vendor_EchoAsyncTaskStk[APP_CFG_USBD_VENDOR_TASK_STK_SIZE];
static  CPU_INT08U   App_USBD_Vendor_PayloadBuf[512u];
static  CPU_INT08U   App_USBD_Vendor_HeaderBuf[2u];
#endif


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  CPU_BOOLEAN  App_USBD_Vendor_VendorReq    (       CPU_INT08U       class_nbr,
                                                          CPU_INT08U       dev_nbr,
                                                   const  USBD_SETUP_REQ  *p_setup_req);

#if (APP_CFG_USBD_VENDOR_ECHO_SYNC_EN  == DEF_ENABLED)
static  void         App_USBD_Vendor_EchoSyncTask (       void            *p_arg);

static  CPU_BOOLEAN  App_USBD_Vendor_Echo         (       CPU_INT08U       class_nbr);
#endif

#if (APP_CFG_USBD_VENDOR_ECHO_ASYNC_EN == DEF_ENABLED)
static  void         App_USBD_Vendor_EchoAsyncTask(       void            *p_arg);

static  void         App_USBD_Vendor_RxHeaderCmpl (       CPU_INT08U       class_nbr,
                                                          void            *p_buf,
                                                          CPU_INT32U       buf_len,
                                                          CPU_INT32U       xfer_len,
                                                          void            *p_callback_arg,
                                                          USBD_ERR         err);

static  void         App_USBD_Vendor_RxPayloadCmpl(       CPU_INT08U       class_nbr,
                                                          void            *p_buf,
                                                          CPU_INT32U       buf_len,
                                                          CPU_INT32U       xfer_len,
                                                          void            *p_callback_arg,
                                                          USBD_ERR         err);

static  void         App_USBD_Vendor_TxPayloadCmpl(       CPU_INT08U       class_nbr,
                                                          void            *p_buf,
                                                          CPU_INT32U       buf_len,
                                                          CPU_INT32U       xfer_len,
                                                          void            *p_callback_arg,
                                                          USBD_ERR         err);
#endif


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       App_USBD_Vendor_Init()
*
* Description : Add a Vendor interface to the USB device stack.
*
* Argument(s) : dev_nbr    Device number.
*
*               cfg_hs     Index of high-speed configuration to which this interface will be added to.
*
*               cfg_fs     Index of full-speed configuration to which this interface will be added to.
*
* Return(s)   : DEF_OK,    if Vendor interface successfully added.
*
*               DEF_FAIL,  otherwise.
*
* Note(s)     : (1) The semaphore is initially available so that the Async task can initiate the first
*                   transfer to receive the header. Then the semaphore is used only when an error
*                   condition is detected during the test sequence execution. This allows to restart
*                   the test sequence in case of communication error.
*********************************************************************************************************
*/

CPU_BOOLEAN  App_USBD_Vendor_Init (CPU_INT08U  dev_nbr,
                                   CPU_INT08U  cfg_hs,
                                   CPU_INT08U  cfg_fs)
{
    USBD_ERR    err;
    USBD_ERR    err_hs;
    USBD_ERR    err_fs;
    CPU_INT08U  class_nbr_0;
#if ((APP_CFG_USBD_VENDOR_ECHO_SYNC_EN  == DEF_ENABLED) || \
     (APP_CFG_USBD_VENDOR_ECHO_ASYNC_EN == DEF_ENABLED))
    OS_ERR      os_err;
#endif
#if ((APP_CFG_USBD_VENDOR_ECHO_SYNC_EN  == DEF_ENABLED) && \
     (APP_CFG_USBD_VENDOR_ECHO_ASYNC_EN == DEF_ENABLED))
    CPU_INT08U  class_nbr_1;
#endif


    err_hs = USBD_ERR_NONE;
    err_fs = USBD_ERR_NONE;

    APP_TRACE_DBG(("        Initializing Vendor class ... \r\n"));

    USBD_Vendor_Init(&err);                                      /* Init Vendor class.                                   */
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not initialize Vendor class w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }
                                                                /* Create a Vendor class instance.                      */
    class_nbr_0 = USBD_Vendor_Add(DEF_FALSE, 0u, App_USBD_Vendor_VendorReq, &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not instantiate a Vendor class w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

    if (cfg_hs != USBD_CFG_NBR_NONE) {
                                                                /* Add vendor class to HS dflt cfg.                     */
        USBD_Vendor_CfgAdd(class_nbr_0, dev_nbr, cfg_hs, &err_hs);
        if (err_hs != USBD_ERR_NONE) {
            APP_TRACE_DBG(("        ... could not add Vendor class instance #%d to HS configuration w/err = %d\r\n\r\n", class_nbr_0, err_hs));
        }
    }

    if (cfg_fs != USBD_CFG_NBR_NONE) {
                                                                /* Add vendor class to FS dflt cfg.                     */
        USBD_Vendor_CfgAdd(class_nbr_0, dev_nbr, cfg_fs, &err_fs);
        if (err_fs != USBD_ERR_NONE) {
            APP_TRACE_DBG(("        ... could not add Vendor class instance #%d to FS configuration w/err = %d\r\n\r\n", class_nbr_0, err_fs));
        }
    }

    if ((err_hs != USBD_ERR_NONE) &&                            /* If HS and FS cfg fail, stop class init.              */
        (err_fs != USBD_ERR_NONE)) {
        return (DEF_FAIL);
    }

#if ((APP_CFG_USBD_VENDOR_ECHO_SYNC_EN  == DEF_ENABLED) && \
     (APP_CFG_USBD_VENDOR_ECHO_ASYNC_EN == DEF_ENABLED))
                                                                /* Create a Vendor class instance.                      */
    class_nbr_1 = USBD_Vendor_Add(                      DEF_FALSE,
                                                        0u,
                                  (USBD_VENDOR_REQ_FNCT)0u,
                                                       &err);

    if (cfg_hs != USBD_CFG_NBR_NONE) {
                                                                /* Add vendor class to HS dflt cfg.                     */
        USBD_Vendor_CfgAdd(class_nbr_1, dev_nbr, cfg_hs, &err_hs);
        if (err_hs != USBD_ERR_NONE) {
            APP_TRACE_DBG(("        ... could not add Vendor class instance #%d to HS configuration w/err = %d\r\n\r\n", class_nbr_1, err_hs));
        }
    }

    if (cfg_fs != USBD_CFG_NBR_NONE) {
                                                                /* Add vendor class to FS dflt cfg.                     */
        USBD_Vendor_CfgAdd(class_nbr_1, dev_nbr, cfg_fs, &err_fs);
        if (err_fs != USBD_ERR_NONE) {
            APP_TRACE_DBG(("        ... could not add Vendor class instance #%d to FS configuration w/err = %d\r\n\r\n", class_nbr_1, err_fs));
        }
    }

    if ((err_hs != USBD_ERR_NONE) &&                            /* If HS and FS cfg fail, stop class init.              */
        (err_fs != USBD_ERR_NONE)) {
        return (DEF_FAIL);
    }
#endif

#if (APP_CFG_USBD_VENDOR_ECHO_SYNC_EN == DEF_ENABLED)
                                                                /* Create Echo Sync task.                               */
#if (APP_USBD_VENDOR_OS_III_EN == DEF_ENABLED)                  /* ---------------------- OS-III ---------------------- */
    OSTaskCreate(                 &App_USBD_Vendor_EchoSyncTaskTCB,
                                  "USB Device Vendor Echo Sync",
                                   App_USBD_Vendor_EchoSyncTask,
                 (void *)(CPU_ADDR)class_nbr_0,
                                   APP_CFG_USBD_VENDOR_ECHO_SYNC_TASK_PRIO,
                                  &App_USBD_Vendor_EchoSyncTaskStk[0],
                                   APP_CFG_USBD_VENDOR_TASK_STK_SIZE / 10u,
                                   APP_CFG_USBD_VENDOR_TASK_STK_SIZE,
                                   0u,
                                   0u,
                 (void *)          0,
                                   OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
                                  &os_err);
    if (os_err != OS_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not add Vendor echo sync task w/err = %d\r\n\r\n", os_err));
        return (DEF_FAIL);
    }
#else                                                           /* ---------------------- OS-II ----------------------- */
#if (OS_STK_GROWTH == 1u)
    os_err = OSTaskCreateExt(                  App_USBD_Vendor_EchoSyncTask,
                             (void *)(CPU_ADDR)class_nbr_0,
                                              &App_USBD_Vendor_EchoSyncTaskStk[APP_CFG_USBD_VENDOR_TASK_STK_SIZE - 1],
                                               APP_CFG_USBD_VENDOR_ECHO_SYNC_TASK_PRIO,
                                               APP_CFG_USBD_VENDOR_ECHO_SYNC_TASK_PRIO,
                                              &App_USBD_Vendor_EchoSyncTaskStk[0],
                                               APP_CFG_USBD_VENDOR_TASK_STK_SIZE,
                             (void *)          0,
                                               OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);
#else
    os_err = OSTaskCreateExt(                  App_USBD_Vendor_EchoSyncTask,
                             (void *)(CPU_ADDR)class_nbr_0,
                                              &App_USBD_Vendor_EchoSyncTaskStk[0],
                                               APP_CFG_USBD_VENDOR_ECHO_SYNC_TASK_PRIO,
                                               APP_CFG_USBD_VENDOR_ECHO_SYNC_TASK_PRIO,
                                              &App_USBD_Vendor_EchoSyncTaskStk[APP_CFG_USBD_VENDOR_TASK_STK_SIZE - 1],
                                               APP_CFG_USBD_VENDOR_TASK_STK_SIZE,
                             (void *)          0,
                                               OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);
#endif
    if (os_err != OS_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not add Vendor echo sync task w/err = %d\r\n\r\n", os_err));
        return (DEF_FAIL);
    }
#if (OS_TASK_NAME_EN > 0u)
    OSTaskNameSet(APP_CFG_USBD_VENDOR_ECHO_SYNC_TASK_PRIO, (INT8U *)"USB Device Vendor Echo Sync", &os_err);
#endif
#endif
#endif

#if (APP_CFG_USBD_VENDOR_ECHO_ASYNC_EN == DEF_ENABLED)
                                                                /* Create sem to signal re(starting) of test sequence.  */
#if (APP_USBD_VENDOR_OS_III_EN == DEF_ENABLED)                  /* ---------------------- OS-III ---------------------- */
    OSSemCreate(&App_USBD_Vendor_EchoAsyncSem,
                "Async task sem",
                 1u,                                            /* Sem initially available (see Note #1).               */
                &os_err);
    if (os_err != OS_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not create Vendor echo async semaphore w/err = %d\r\n\r\n", os_err));
        return (DEF_FAIL);
    }
                                                                /* Create Echo Async task.                              */
    OSTaskCreate(                 &App_USBD_Vendor_EchoAsyncTaskTCB,
                                  "USB Device Vendor Echo Async",
                                   App_USBD_Vendor_EchoAsyncTask,
#if ((APP_CFG_USBD_VENDOR_ECHO_SYNC_EN  == DEF_ENABLED) && \
     (APP_CFG_USBD_VENDOR_ECHO_ASYNC_EN == DEF_ENABLED))
                 (void *)(CPU_ADDR)class_nbr_1,
#else
                 (void *)(CPU_ADDR)class_nbr_0,
#endif
                                   APP_CFG_USBD_VENDOR_ECHO_ASYNC_TASK_PRIO,
                                  &App_USBD_Vendor_EchoAsyncTaskStk[0],
                                   APP_CFG_USBD_VENDOR_TASK_STK_SIZE / 10u,
                                   APP_CFG_USBD_VENDOR_TASK_STK_SIZE,
                                   0u,
                                   0u,
                 (void *)          0,
                                   OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
                                  &os_err);
    if (os_err != OS_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not add Vendor echo async task w/err = %d\r\n\r\n", os_err));
        return (DEF_FAIL);
    }
#else                                                           /* ---------------------- OS-II ----------------------- */
    App_USBD_Vendor_EchoAsyncSemPtr = OSSemCreate(1u);          /* Sem initially available (see Note #1).               */
    if (App_USBD_Vendor_EchoAsyncSemPtr == (OS_EVENT *)0) {
        APP_TRACE_DBG(("        ... could not create Vendor echo async semaphore w/err = %d\r\n\r\n", os_err));
        return (DEF_FAIL);
    }
                                                                /* Create Echo Async task.                              */
#if (OS_STK_GROWTH == 1u)
    os_err = OSTaskCreateExt(                  App_USBD_Vendor_EchoAsyncTask,
#if ((APP_CFG_USBD_VENDOR_ECHO_SYNC_EN  == DEF_ENABLED) && \
     (APP_CFG_USBD_VENDOR_ECHO_ASYNC_EN == DEF_ENABLED))
                             (void *)(CPU_ADDR)class_nbr_1,
#else
                             (void *)(CPU_ADDR)class_nbr_0,
#endif
                                              &App_USBD_Vendor_EchoAsyncTaskStk[APP_CFG_USBD_VENDOR_TASK_STK_SIZE - 1],
                                               APP_CFG_USBD_VENDOR_ECHO_ASYNC_TASK_PRIO,
                                               APP_CFG_USBD_VENDOR_ECHO_ASYNC_TASK_PRIO,
                                              &App_USBD_Vendor_EchoAsyncTaskStk[0],
                                               APP_CFG_USBD_VENDOR_TASK_STK_SIZE,
                             (void *)          0,
                                               OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);
#else
    os_err = OSTaskCreateExt(                  App_USBD_Vendor_EchoAsyncTask,
#if ((APP_CFG_USBD_VENDOR_TEST_ECHO_SYNC_EN  == DEF_ENABLED) && \
     (APP_CFG_USBD_VENDOR_TEST_ECHO_ASYNC_EN == DEF_ENABLED))
                             (void *)(CPU_ADDR)class_nbr_1,
#else
                             (void *)(CPU_ADDR)class_nbr_0,
#endif
                                              &App_USBD_Vendor_EchoAsyncTaskStk[0],
                                               APP_CFG_USBD_VENDOR_ECHO_ASYNC_TASK_PRIO,
                                               APP_CFG_USBD_VENDOR_ECHO_ASYNC_TASK_PRIO,
                                              &App_USBD_Vendor_EchoAsyncTaskStk[APP_CFG_USBD_VENDOR_TASK_STK_SIZE - 1],
                                               APP_CFG_USBD_VENDOR_TASK_STK_SIZE,
                             (void *)          0,
                                               OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);
#endif
    if (os_err != OS_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not add Vendor echo async task w/err = %d\r\n\r\n", os_err));
        return (DEF_FAIL);
    }
#if (OS_TASK_NAME_EN > 0u)
    OSTaskNameSet(APP_CFG_USBD_VENDOR_ECHO_ASYNC_TASK_PRIO, (INT8U *)"USB Device Vendor Echo Async", &os_err);
#endif
#endif
#endif

#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
    USBD_DevSetMS_VendorCode(dev_nbr, 1u, &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not set MS vendor code w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

    USBD_Vendor_MS_ExtPropertyAdd(class_nbr_0,
                                  USBD_MS_OS_PROPERTY_TYPE_REG_SZ,
                                  App_USBD_Vendor_MS_PropertyNameGUID,
                                  sizeof(App_USBD_Vendor_MS_PropertyNameGUID),
                                  App_USBD_Vendor_MS_GUID,
                                  sizeof(App_USBD_Vendor_MS_GUID),
                                 &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not set MS vendor GUID w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

#if ((APP_CFG_USBD_VENDOR_ECHO_SYNC_EN  == DEF_ENABLED) && \
     (APP_CFG_USBD_VENDOR_ECHO_ASYNC_EN == DEF_ENABLED))
    USBD_Vendor_MS_ExtPropertyAdd(class_nbr_1,
                                  USBD_MS_OS_PROPERTY_TYPE_REG_SZ,
                                  App_USBD_Vendor_MS_PropertyNameGUID,
                                  sizeof(App_USBD_Vendor_MS_PropertyNameGUID),
                                  App_USBD_Vendor_MS_GUID,
                                  sizeof(App_USBD_Vendor_MS_GUID),
                                 &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not set MS vendor GUID w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }
#endif
#endif

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                     App_USBD_Vendor_VendorReq()
*
* Description : Process vendor-specific requests.
*
* Argument(s) : class_nbr      Class instance number.
*
*               dev_nbr        Device number.
*
*               p_setup_req    Pointer to setup request structure.
*
* Return(s)   : DEF_OK,        if NO error(s) occurred and request is supported.
*
*               DEF_FAIL,      otherwise.
*
* Note(s)     : (1) USB Spec 2.0, section 9.3 'USB Device Requests' specifies the Setup packet format.
*
*               (2) The customer may process inside this callback his proprietary vendor-specific
*                   requests decoding.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  App_USBD_Vendor_VendorReq (       CPU_INT08U       class_nbr,
                                                       CPU_INT08U       dev_nbr,
                                                const  USBD_SETUP_REQ  *p_setup_req)
{
    (void)class_nbr;
    (void)dev_nbr;
    (void)p_setup_req;

    return (DEF_FAIL);                                          /* Indicate request is not supported.                   */
}


/*
*********************************************************************************************************
*                                   App_USBD_Vendor_EchoSyncTask()
*
* Description : Perform Echo demo with synchronous communication:
*
*                   (a) Read header.
*                   (b) Read payload of a certain size.
*                   (c) Write payload previously received.
*
* Argument(s) : p_arg    Argument passed to the task.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (APP_CFG_USBD_VENDOR_ECHO_SYNC_EN == DEF_ENABLED)
static  void  App_USBD_Vendor_EchoSyncTask (void  *p_arg)
{
    CPU_INT08U   class_nbr;
    CPU_BOOLEAN  conn;
    CPU_BOOLEAN  err;
#if (APP_USBD_VENDOR_OS_III_EN == DEF_ENABLED)
    OS_ERR       os_err;
#endif


    class_nbr = (CPU_INT08U)(CPU_ADDR)p_arg;
    err       =  DEF_OK;

    APP_TRACE_DBG(("        Executing Vendor Echo Sync demo...\r\n"));

    while (DEF_TRUE) {

                                                                /* Wait for cfg activated by host.                      */
        conn = USBD_Vendor_IsConn(class_nbr);
        while (conn != DEF_YES) {
#if (APP_USBD_VENDOR_OS_III_EN == DEF_ENABLED)
            OSTimeDlyHMSM(0u, 0u, 0u, 250u, OS_OPT_TIME_HMSM_NON_STRICT, &os_err);
#else
            OSTimeDlyHMSM(0u, 0u, 0u, 250u);
#endif

            conn = USBD_Vendor_IsConn(class_nbr);
        }

        err = App_USBD_Vendor_Echo(class_nbr);

        if (err != DEF_OK) {
                                                                /* Delay 250 ms if an error occurred.                   */
#if (APP_USBD_VENDOR_OS_III_EN == DEF_ENABLED)
            OSTimeDlyHMSM(0u, 0u, 0u, 250u, OS_OPT_TIME_HMSM_NON_STRICT, &os_err);
#else
            OSTimeDlyHMSM(0u, 0u, 0u, 250u);
#endif
        }
    }
}
#endif


/*
*********************************************************************************************************
*                                       App_USBD_Vendor_Echo()
*
* Description : Read and write transfers from different size.
*
* Argument(s) : class_nbr    Vendor class instance number.
*
* Return(s)   : DEF_OK,      if NO error(s).
*
*               DEF_FAIL,    otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (APP_CFG_USBD_VENDOR_ECHO_SYNC_EN == DEF_ENABLED)
static  CPU_BOOLEAN  App_USBD_Vendor_Echo (CPU_INT08U  class_nbr)
{
    USBD_ERR    err;
    CPU_INT08U  msg_hdr[2];
    CPU_INT32U  nbr_octets_recv;
    CPU_INT32U  payload_len;
    CPU_INT32U  rem_payload_len;
    CPU_INT32U  xfer_len;

                                                                /* ----------------- HEADER RECEPTION ----------------- */
    nbr_octets_recv = USBD_Vendor_Rd(         class_nbr,
                                     (void *)&msg_hdr[0],
                                              2,
                                              0,
                                             &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... Echo Sync demo failed to receive message header w/err = %d\r\n", err));
        return (DEF_FAIL);
    }

    if (nbr_octets_recv != 2) {                                 /* Verify that hdr is 2 octets.                         */
        return (DEF_FAIL);
    }
                                                                /* ---------------- PAYLOAD PROCESSING ---------------- */
    payload_len      = DEF_BIT_MASK(msg_hdr[0], 8);             /* Get nbr of octets from the message header.           */
    payload_len     |= msg_hdr[1];
    rem_payload_len  = payload_len;

    while (rem_payload_len > 0) {

        if (rem_payload_len >= 512) {
            xfer_len = 512;
        } else {
            xfer_len = rem_payload_len;
        }
                                                                /* Receive payload chunk from host.                     */
        nbr_octets_recv = USBD_Vendor_Rd(         class_nbr,
                                         (void *)&App_USBD_Vendor_Buf[0],
                                                  xfer_len,
                                                  APP_RX_TIMEOUT_MS,
                                                 &err);
        if (err != USBD_ERR_NONE) {
            APP_TRACE_DBG(("        ... Echo Sync demo failed to read data from host w/err = %d\r\n", err));
            return (DEF_FAIL);
        }

        rem_payload_len -= nbr_octets_recv;                     /* Remaining nbr of octets to be received.              */

                                                                /* Send back received payload chunk to host.            */
        (void)USBD_Vendor_Wr(         class_nbr,
                             (void *)&App_USBD_Vendor_Buf[0],
                                      nbr_octets_recv,
                                      APP_TX_TIMEOUT_MS,
                                      DEF_FALSE,
                                     &err);
        if (err != USBD_ERR_NONE) {
            APP_TRACE_DBG(("       ... Echo Sync demo failed to write data to host w/err = %d\r\n", err));
            return (DEF_FAIL);
        }

    }

    return (DEF_OK);
}
#endif


/*
*********************************************************************************************************
*                                   App_USBD_Vendor_EchoAsyncTask()
*
* Description : Perform Echo demo with asynchronous communication (see Note #1):
*
*                   (a) Read header.
*                   (b) Read payload of a certain size.
*                   (c) Write payload previously received.
*
* Argument(s) : p_arg    Argument passed to the task.
*
* Return(s)   : none.
*
* Note(s)     : (1) The Echo Async task will prepare the first read to initiate the test sequence with
*                   the host. The other steps of the test sequence will be managed in the different
*                   asynchronous callback.
*********************************************************************************************************
*/

#if (APP_CFG_USBD_VENDOR_ECHO_ASYNC_EN == DEF_ENABLED)
static  void  App_USBD_Vendor_EchoAsyncTask (void  *p_arg)
{
    CPU_INT08U   class_nbr;
    CPU_BOOLEAN  conn;
    USBD_ERR     usb_err;
    OS_ERR       os_err;
    CPU_INT32U   nbr_octets_expected;


    class_nbr = (CPU_INT08U)(CPU_ADDR)p_arg;

    APP_TRACE_DBG(("        Executing Vendor Echo Async demo...\r\n"));

    while (DEF_TRUE) {
                                                                /* Wait to (re)start the test sequence.                 */
#if (APP_USBD_VENDOR_OS_III_EN == DEF_ENABLED)                  /* ---------------------- OS-III ---------------------- */
        (void)OSSemPend(          &App_USBD_Vendor_EchoAsyncSem,
                                  0,
                                  OS_OPT_PEND_BLOCKING,
                        (CPU_TS *)0,
                                 &os_err);
#else                                                           /* ---------------------- OS-II ----------------------- */
        OSSemPend(App_USBD_Vendor_EchoAsyncSemPtr, 0, &os_err);
#endif
                                                                /* Wait for cfg activated by host.                      */
        conn = USBD_Vendor_IsConn(class_nbr);
        while (conn != DEF_YES) {
#if (APP_USBD_VENDOR_OS_III_EN == DEF_ENABLED)                  /* ---------------------- OS-III ---------------------- */
            OSTimeDlyHMSM(0u, 0u, 0u, 250u, OS_OPT_TIME_HMSM_NON_STRICT, &os_err);
#else                                                           /* ---------------------- OS-II ----------------------- */
            OSTimeDlyHMSM(0u, 0u, 0u, 250u);
#endif
            conn = USBD_Vendor_IsConn(class_nbr);
        }

        nbr_octets_expected = 0;
                                                                /* ----------------- HEADER RECEPTION ----------------- */
                                                                /* Prepare async Bulk OUT xfer.                         */
        USBD_Vendor_RdAsync(         class_nbr,
                            (void *)&App_USBD_Vendor_HeaderBuf[0],
                                     2u,
                                     App_USBD_Vendor_RxHeaderCmpl,
                            (void *)&nbr_octets_expected,
                                    &usb_err);

        if (usb_err != USBD_ERR_NONE) {
            APP_TRACE_DBG(("        ... Echo Async demo failed to prepare OUT transfer for header w/err = %d\r\n", usb_err));
#if (APP_USBD_VENDOR_OS_III_EN == DEF_ENABLED)                  /* ---------------------- OS-III ---------------------- */
            OSSemPost(&App_USBD_Vendor_EchoAsyncSem, OS_OPT_POST_ALL, &os_err);
#else                                                           /* ---------------------- OS-II ----------------------- */
            (void)OSSemPost(App_USBD_Vendor_EchoAsyncSemPtr);
#endif
        }
    }
}
#endif


/*
*********************************************************************************************************
*                                   App_USBD_Vendor_RxHeaderCmpl()
*
* Description : Callback called upon Bulk OUT transfer completion.
*
* Argument(s) : p_buf             Pointer to receive buffer.
*
*               buf_len           Receive buffer length.
*
*               xfer_len          Number of octets received.
*
*               p_callback_arg    Additional argument provided by application.
*
*               err               Transfer error status.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (APP_CFG_USBD_VENDOR_ECHO_ASYNC_EN == DEF_ENABLED)
static  void  App_USBD_Vendor_RxHeaderCmpl (CPU_INT08U   class_nbr,
                                            void        *p_buf,
                                            CPU_INT32U   buf_len,
                                            CPU_INT32U   xfer_len,
                                            void        *p_callback_arg,
                                            USBD_ERR     err)
{
    CPU_INT08U  *p_header_buf;
    CPU_INT32U   payload_len;
    CPU_INT32U  *p_nbr_octets_expected;
    USBD_ERR     usb_err;
#if (APP_USBD_VENDOR_OS_III_EN == DEF_ENABLED)
    OS_ERR       os_err;
#endif


    (void)buf_len;

    p_header_buf          = (CPU_INT08U *)p_buf;
    p_nbr_octets_expected = (CPU_INT32U *)p_callback_arg;

    if (err == USBD_ERR_NONE) {

        if (xfer_len != 2) {                                    /* Verify that hdr is 2 octets.                         */
            return;
        }
                                                                /* ---------------- PAYLOAD PROCESSING ---------------- */
                                                                /* Get nbr of octets from the message header.           */
        payload_len           = DEF_BIT_MASK(p_header_buf[0], 8);
        payload_len          |= p_header_buf[1];
       *p_nbr_octets_expected = payload_len;

        if (*p_nbr_octets_expected >= 512) {
            xfer_len = 512;
        } else {
            xfer_len = *p_nbr_octets_expected;
        }

                                                                /* Receive payload chunk from host.                     */
        USBD_Vendor_RdAsync(         class_nbr,
                            (void *)&App_USBD_Vendor_PayloadBuf[0],
                                     xfer_len,
                                     App_USBD_Vendor_RxPayloadCmpl,
                            (void *) p_nbr_octets_expected,
                                    &usb_err);
        if (usb_err != USBD_ERR_NONE) {                         /* Restart header reception.                            */
#if (APP_USBD_VENDOR_OS_III_EN == DEF_ENABLED)                  /* ---------------------- OS-III ---------------------- */
            OSSemPost(&App_USBD_Vendor_EchoAsyncSem, OS_OPT_POST_ALL, &os_err);
#else                                                           /* ---------------------- OS-II ----------------------- */
            (void)OSSemPost(App_USBD_Vendor_EchoAsyncSemPtr);
#endif
        }
    } else {                                                    /* Restart header reception.                            */
#if (APP_USBD_VENDOR_OS_III_EN == DEF_ENABLED)                  /* ---------------------- OS-III ---------------------- */
        OSSemPost(&App_USBD_Vendor_EchoAsyncSem, OS_OPT_POST_ALL, &os_err);
#else                                                           /* ---------------------- OS-II ----------------------- */
        (void)OSSemPost(App_USBD_Vendor_EchoAsyncSemPtr);
#endif
    }
}
#endif


/*
*********************************************************************************************************
*                                   App_USBD_Vendor_RxPayloadCmpl()
*
* Description : Callback called upon Bulk OUT transfer completion.
*
* Argument(s) : p_buf             Pointer to receive buffer.
*
*               buf_len           Receive buffer length.
*
*               xfer_len          Number of octets received.
*
*               p_callback_arg    Additional argument provided by application.
*
*               err               Transfer error status.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (APP_CFG_USBD_VENDOR_ECHO_ASYNC_EN == DEF_ENABLED)
static  void  App_USBD_Vendor_RxPayloadCmpl (CPU_INT08U   class_nbr,
                                             void        *p_buf,
                                             CPU_INT32U   buf_len,
                                             CPU_INT32U   xfer_len,
                                             void        *p_callback_arg,
                                             USBD_ERR     err)
{
    USBD_ERR  usb_err;
#if (APP_USBD_VENDOR_OS_III_EN == DEF_ENABLED)
    OS_ERR    os_err;
#endif


    (void)buf_len;

    if (err == USBD_ERR_NONE) {
                                                                /* ---------------- PAYLOAD PROCESSING ---------------- */
        USBD_Vendor_WrAsync( class_nbr,
                             p_buf,                             /* Rx buf becomes a Tx buf.                             */
                             xfer_len,
                             App_USBD_Vendor_TxPayloadCmpl,
                             p_callback_arg,                    /* Nbr of octets expected gotten from header msg.       */
                             DEF_FALSE,
                            &usb_err);
        if (usb_err != USBD_ERR_NONE) {                         /* Restart test sequence with header reception.         */
#if (APP_USBD_VENDOR_OS_III_EN == DEF_ENABLED)                  /* ---------------------- OS-III ---------------------- */
            OSSemPost(&App_USBD_Vendor_EchoAsyncSem, OS_OPT_POST_ALL, &os_err);
#else                                                           /* ---------------------- OS-II ----------------------- */
            (void)OSSemPost(App_USBD_Vendor_EchoAsyncSemPtr);
#endif
        }
    } else {                                                    /* Restart test sequence with header reception.         */
#if (APP_USBD_VENDOR_OS_III_EN == DEF_ENABLED)                  /* ---------------------- OS-III ---------------------- */
        OSSemPost(&App_USBD_Vendor_EchoAsyncSem, OS_OPT_POST_ALL, &os_err);
#else                                                           /* ---------------------- OS-II ----------------------- */
        (void)OSSemPost(App_USBD_Vendor_EchoAsyncSemPtr);
#endif
    }
}
#endif


/*
*********************************************************************************************************
*                                   App_USBD_Vendor_TxPayloadCmpl()
*
* Description : Callback called upon Bulk IN transfer completion.
*
* Argument(s) : p_buf             Pointer to transmit buffer.
*
*               buf_len           Transmit buffer length.
*
*               xfer_len          Number of octets transmitted.
*
*               p_callback_arg    Additional argument provided by application.
*
*               err               Transfer error status.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (APP_CFG_USBD_VENDOR_ECHO_ASYNC_EN == DEF_ENABLED)
static  void  App_USBD_Vendor_TxPayloadCmpl (CPU_INT08U   class_nbr,
                                             void        *p_buf,
                                             CPU_INT32U   buf_len,
                                             CPU_INT32U   xfer_len,
                                             void        *p_callback_arg,
                                             USBD_ERR     err)
{
    CPU_INT32U  *p_nbr_octets_expected;
    USBD_ERR     usb_err;
#if (APP_USBD_VENDOR_OS_III_EN == DEF_ENABLED)
    OS_ERR       os_err;
#endif


    (void)p_buf;
    (void)buf_len;
    (void)xfer_len;

    p_nbr_octets_expected = (CPU_INT32U *)p_callback_arg;       /* Nbr of octets expected gotten from header msg.       */

    if (err == USBD_ERR_NONE) {

       *p_nbr_octets_expected -= xfer_len;                      /* Update nbr of octets expected from host.             */

        if (*p_nbr_octets_expected == 0) {                      /* All the msg payload received from host.              */

                                                                /* ----------------- HEADER RECEPTION ----------------- */
                                                                /* Prepare async Bulk OUT xfer.                         */
            USBD_Vendor_RdAsync(         class_nbr,
                                (void *)&App_USBD_Vendor_HeaderBuf[0],
                                         2u,
                                         App_USBD_Vendor_RxHeaderCmpl,
                                (void *) p_nbr_octets_expected,
                                        &usb_err);
            if (usb_err != USBD_ERR_NONE) {                     /* Restart test sequence with header reception.         */
#if (APP_USBD_VENDOR_OS_III_EN == DEF_ENABLED)                  /* ---------------------- OS-III ---------------------- */
                OSSemPost(&App_USBD_Vendor_EchoAsyncSem, OS_OPT_POST_ALL, &os_err);
#else                                                           /* ---------------------- OS-II ----------------------- */
                (void)OSSemPost(App_USBD_Vendor_EchoAsyncSemPtr);
#endif
            }
        } else {
                                                                /* ---------------- PAYLOAD PROCESSING ---------------- */
            if (*p_nbr_octets_expected >= 512) {
                xfer_len = 512;
            } else {
                xfer_len = *p_nbr_octets_expected;
            }
                                                                /* Receive payload chunk from host.                     */
            USBD_Vendor_RdAsync(         class_nbr,
                                (void *)&App_USBD_Vendor_PayloadBuf[0],
                                         xfer_len,
                                         App_USBD_Vendor_RxPayloadCmpl,
                                (void *) p_nbr_octets_expected,
                                        &usb_err);
            if (usb_err != USBD_ERR_NONE) {                     /* Restart test sequence with header reception.         */
#if (APP_USBD_VENDOR_OS_III_EN == DEF_ENABLED)                  /* ---------------------- OS-III ---------------------- */
                OSSemPost(&App_USBD_Vendor_EchoAsyncSem, OS_OPT_POST_ALL, &os_err);
#else                                                           /* ---------------------- OS-II ----------------------- */
                (void)OSSemPost(App_USBD_Vendor_EchoAsyncSemPtr);
#endif
            }
        }
    } else {                                                    /* Restart test sequence with header reception.         */
#if (APP_USBD_VENDOR_OS_III_EN == DEF_ENABLED)                  /* ---------------------- OS-III ---------------------- */
        OSSemPost(&App_USBD_Vendor_EchoAsyncSem, OS_OPT_POST_ALL, &os_err);
#else                                                           /* ---------------------- OS-II ----------------------- */
        (void)OSSemPost(App_USBD_Vendor_EchoAsyncSemPtr);
#endif
    }
}
#endif


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif
