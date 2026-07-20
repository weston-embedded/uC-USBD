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
*                                  USB DEVICE CDC ECM TEST APPLICATION
*
*                                              TEMPLATE
*
* Filename : app_usbd_cdc_ecm.c
* Version  : V4.06.01
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <app_usbd.h>

#if (APP_CFG_USBD_EN         == DEF_ENABLED) && \
    (APP_CFG_USBD_CDC_ECM_EN == DEF_ENABLED)
#include  <Class/CDC/ECM/usbd_ecm.h>

#include  <lib_str.h>
#include  <Source/os.h>


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  APP_USBD_ECM_MAC_LEN                              6u
#define  APP_USBD_ECM_MAC_ARP_OFFSET_1                     6u
#define  APP_USBD_ECM_MAC_ARP_OFFSET_2                    22u

#define  APP_USBD_ECM_PCK_SIZE                          1514u

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

static        OS_TCB       App_USBD_ECM_TaskTCB;
static        CPU_STK      App_USBD_ECM_TaskStk[APP_USBD_ECM_TASK_STK_SIZE];
static        CPU_BOOLEAN  App_USBD_ECM_NetworkConnected = DEF_FALSE; /* Flag to indicate network connection status.    */
static        CPU_INT08U   App_USBD_ECM_SubclassNbr;
static        CPU_INT08U   App_USBD_ECM_RxBuf[APP_USBD_ECM_PCK_SIZE];
static        CPU_INT08U   App_USBD_ECM_TxBuf[APP_USBD_ECM_PCK_SIZE];

static        CPU_INT08U   App_ARP_ReqWhoHas[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0x08,
                                                  0x06, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x01, 0xaa, 0xaa, 0xaa, 0xaa,
                                                  0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa9,
                                                  0xfe, 0x36, 0x1a};

static        CPU_INT08U   App_ARP_announce[]  = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0x08,
                                                  0x06, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x01, 0xaa, 0xaa, 0xaa, 0xaa,
                                                  0xaa, 0xaa, 0xa9, 0xfe, 0x36, 0x1a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa9,
                                                  0xfe, 0x36, 0x1a};

static const  CPU_CHAR     App_USBD_ECM_MACAddrStr[]  = "001122334455";
static const  CPU_INT08U   App_USBD_ECM_MACAddr[]     = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
static const  CPU_INT32U   App_USBD_ECM_EthStats      = 0x00;
static const  CPU_INT16U   App_USBD_ECM_MaxSegSize    = APP_USBD_ECM_PCK_SIZE;
static const  CPU_INT16U   App_USBD_ECM_NumMCFilters  = 0x00;
static const  CPU_INT08U   App_USBD_ECM_NumPwrFilters = 0x00;


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  CPU_BOOLEAN  App_USBD_ECM_MgmtReq  (       CPU_INT08U       dev_nbr,
                                            const  USBD_SETUP_REQ  *p_setup_req,
                                                   void            *p_subclass_arg);

static  void         App_USBD_ECM_RxCmpl   (       CPU_INT08U       dev_nbr,
                                                   CPU_INT08U       ep_addr,
                                                   void            *p_buf,
                                                   CPU_INT32U       buf_len,
                                                   CPU_INT32U       xfer_len,
                                                   void            *p_arg,
                                                   USBD_ERR         err);

static  void         App_USBD_ECM_Task     (       void            *p_arg);

static  void         App_USBD_ECM_ARPSetMAC(       CPU_INT08U      *p_buf,
                                            const  CPU_INT08U      *p_mac);


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                   INITIALIZED GLOBAL VARIABLES ACCESSES BY OTHER MODULES/OBJECTS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           GLOBAL FUNCTION
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                       App_USBD_CDC_ECM_Init()
*
* Description : Initialize the USB CDC ECM application.
*
* Argument(s) : dev_nbr    Device number.
*
*               cfg_hs     High-speed configuration number, set to USBD_CFG_NBR_NONE if not used.
*
*               cfg_fs     Full-speed configuration number, set to USBD_CFG_NBR_NONE if not used.
*
* Return(s)   : DEF_OK,    if CDC ECM class initialized successfully.
*
*               DEF_FAIL,  otherwise.
*
* Note(s)     : None.
*********************************************************************************************************
*/

CPU_BOOLEAN  App_USBD_CDC_ECM_Init (CPU_INT08U  dev_nbr,
                                    CPU_INT08U  cfg_hs,
                                    CPU_INT08U  cfg_fs)
{
    USBD_ERR   err;
    OS_ERR     os_err;


    USBD_ECM_Init(&err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not create CDC ECM class instance w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

    App_USBD_ECM_SubclassNbr = USBD_ECM_Add(App_USBD_ECM_MgmtReq, NULL, DEF_TRUE, &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not create CDC ECM class instance w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

    if (cfg_hs != USBD_CFG_NBR_NONE) {                          /* Add CDC ECM class instance to USB configuration(s).  */
        USBD_ECM_CfgAdd(App_USBD_ECM_SubclassNbr,
                        dev_nbr,
                        cfg_hs,
                        App_USBD_ECM_MACAddrStr,
                        App_USBD_ECM_EthStats,
                        App_USBD_ECM_MaxSegSize,
                        App_USBD_ECM_NumMCFilters,
                        App_USBD_ECM_NumPwrFilters,
                        &err);
        if (err != USBD_ERR_NONE) {
            APP_TRACE_DBG(("        ... could not add CDC ECM instance #%d to HS configuration w/err = %d\r\n\r\n", App_USBD_ECM_SubclassNbr, err));
            return (DEF_FAIL);
        }
    }

    if (cfg_fs != USBD_CFG_NBR_NONE) {
        USBD_ECM_CfgAdd(App_USBD_ECM_SubclassNbr,
                        dev_nbr,
                        cfg_fs,
                        App_USBD_ECM_MACAddrStr,
                        App_USBD_ECM_EthStats,
                        App_USBD_ECM_MaxSegSize,
                        App_USBD_ECM_NumMCFilters,
                        App_USBD_ECM_NumPwrFilters,
                        &err);
        if (err != USBD_ERR_NONE) {
            APP_TRACE_DBG(("        ... could not add CDC ECM instance #%d to FS configuration w/err = %d\r\n\r\n", App_USBD_ECM_SubclassNbr, err));
            return (DEF_FAIL);
        }
    }

                                                                /* ----------- TASK FOR SENDING ARP MSG --------------- */
    OSTaskCreate(            &App_USBD_ECM_TaskTCB,
                 (CPU_CHAR*)  "Example USBD ECM Task",
                              App_USBD_ECM_Task,
                              DEF_NULL,
                              APP_USBD_ECM_TASK_PRIO,
                              App_USBD_ECM_TaskStk,
                              0,
                              APP_USBD_ECM_TASK_STK_SIZE,
                              0,
                              0,
                              0,
                              (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                             &os_err);
    if (os_err != OS_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not add CDC ECM Example task w/err = %d\r\n\r\n", os_err));
        return (DEF_FAIL);
    }

    return (DEF_OK);
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTION
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         App_USBD_ECM_Task()
*
* Description : Task to send ECM data.
*
* Argument(s) : p_args    Task arguments, not used
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  App_USBD_ECM_Task (void *p_arg)
{
    USBD_ERR err;


    (void)p_arg;

    while (DEF_ON) {
        if (App_USBD_ECM_NetworkConnected == DEF_TRUE) {
                                                                /* Send network connected notification.                 */
            USBD_ECM_NotifyNetConn(App_USBD_ECM_SubclassNbr, DEF_TRUE, &err);
            if (err != USBD_ERR_NONE) {
                APP_TRACE_DBG(("        ... Failed to send network connected notification to host w/err = %d\r\n\r\n", err));
            }

            OSTimeDlyHMSM(0, 0, 1, 0, OS_OPT_TIME_HMSM_STRICT, DEF_NULL);

                                                                /* Send connection speed change notification.           */
            USBD_ECM_NotifyConnSpdChng(App_USBD_ECM_SubclassNbr, 1000000, 1000000, &err);
            if (err != USBD_ERR_NONE) {
                APP_TRACE_DBG(("        ... Failed to send connection speed change notification to host w/err = %d\r\n\r\n", err));
            }

            OSTimeDlyHMSM(0, 0, 1, 0, OS_OPT_TIME_HMSM_STRICT, DEF_NULL);

                                                                /* Send ARP request.                                    */
            Mem_Copy(App_USBD_ECM_TxBuf, App_ARP_ReqWhoHas, sizeof(App_ARP_ReqWhoHas));
            App_USBD_ECM_ARPSetMAC(App_USBD_ECM_TxBuf, App_USBD_ECM_MACAddr);
            USBD_ECM_DataTx(App_USBD_ECM_SubclassNbr, App_USBD_ECM_TxBuf, sizeof(App_ARP_ReqWhoHas), 5000, &err);
            if (err != USBD_ERR_NONE) {
                APP_TRACE_DBG(("        ... Failed to send ARP request to host w/err = %d\r\n\r\n", err));
            }

            OSTimeDlyHMSM(0, 0, 1, 0, OS_OPT_TIME_HMSM_STRICT, DEF_NULL);

                                                                /* Send ARP announce.                                   */
            Mem_Copy(App_USBD_ECM_TxBuf, App_ARP_announce, sizeof(App_ARP_announce));
            App_USBD_ECM_ARPSetMAC(App_USBD_ECM_TxBuf, App_USBD_ECM_MACAddr);
            USBD_ECM_DataTx(App_USBD_ECM_SubclassNbr, App_USBD_ECM_TxBuf, sizeof(App_ARP_announce), 5000, &err);
            if (err != USBD_ERR_NONE) {
                APP_TRACE_DBG(("        ... Failed to send ARP announce to host w/err = %d\r\n\r\n", err));
            }

            OSTimeDlyHMSM(0, 0, 1, 0, OS_OPT_TIME_HMSM_STRICT, DEF_NULL);
        }

        OSTimeDlyHMSM(0, 0, 1, 0, OS_OPT_TIME_HMSM_STRICT, DEF_NULL);
    }
}


/*
*********************************************************************************************************
*                                        App_USBD_ECM_MgmtReq()
*
* Description : Callback called when an ECM management request is received.
*
* Argument(s) : dev_nbr           Device number.
*
*               p_setup_req       Pointer to setup request.
*
*               p_subclass_arg    Pointer to subclass argument.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  App_USBD_ECM_MgmtReq (       CPU_INT08U       dev_nbr,
                                           const  USBD_SETUP_REQ  *p_setup_req,
                                                  void            *p_subclass_arg) {
    USBD_ERR err;


    (void)dev_nbr;
    (void)p_setup_req;
    (void)p_subclass_arg;

    if (App_USBD_ECM_NetworkConnected == DEF_FALSE) {
        USBD_ECM_DataRxAsync(App_USBD_ECM_SubclassNbr,
                             App_USBD_ECM_RxBuf,
                             sizeof(App_USBD_ECM_RxBuf),
                             App_USBD_ECM_RxCmpl,
                             DEF_NULL,
                            &err);

        App_USBD_ECM_NetworkConnected = DEF_TRUE;
    }

    return DEF_TRUE;
}


/*
*********************************************************************************************************
*                                        App_USBD_ECM_RxCmpl()
*
* Description : Callback function called when a data packet is received.
*
* Argument(s) : dev_nbr     Device number.
*
*               ep_addr     Endpoint address.
*
*               p_buf       Pointer to the buffer containing the received data.
*
*               buf_len     Length of the buffer.
*
*               xfer_len    Length of the received data.
*
*               p_arg       Pointer to the argument.
*
*               err         Error code if the transfer failed.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static void  App_USBD_ECM_RxCmpl (CPU_INT08U   dev_nbr,
                                  CPU_INT08U   ep_addr,
                                  void        *p_buf,
                                  CPU_INT32U   buf_len,
                                  CPU_INT32U   xfer_len,
                                  void        *p_arg,
                                  USBD_ERR     err)
{
    (void)dev_nbr;
    (void)ep_addr;
    (void)p_buf;
    (void)buf_len;
    (void)xfer_len;
    (void)p_arg;

    if (err != USBD_ERR_NONE) {
        return;
    }

    USBD_ECM_DataRxAsync(App_USBD_ECM_SubclassNbr,
                         App_USBD_ECM_RxBuf,
                         sizeof(App_USBD_ECM_RxBuf),
                         App_USBD_ECM_RxCmpl,
                         DEF_NULL,
                        &err);
}


/*
*********************************************************************************************************
*                                       App_USBD_ECM_ARPSetMAC()
*
* Description : Set the MAC address in the ARP packet.
*
* Argument(s) : p_buf    ARP packet to update with the MAC address
*
*               p_mac    MAC address to write to the packet
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  App_USBD_ECM_ARPSetMAC (      CPU_INT08U  *p_buf,
                                      const CPU_INT08U  *p_mac)
{
    Mem_Copy(&p_buf[APP_USBD_ECM_MAC_ARP_OFFSET_1], p_mac, APP_USBD_ECM_MAC_LEN);
    Mem_Copy(&p_buf[APP_USBD_ECM_MAC_ARP_OFFSET_2], p_mac, APP_USBD_ECM_MAC_LEN);
}


/*
*********************************************************************************************************
*                                                  END
*********************************************************************************************************
*/

#endif
