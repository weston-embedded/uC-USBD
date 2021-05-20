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
*                           USB DEVICE MSC CLASS APPLICATION INITIALIZATION
*
*                                              TEMPLATE
*
* Filename : app_usbd_msc.c
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <app_usbd.h>

#if (APP_CFG_USBD_EN     == DEF_ENABLED) && \
    (APP_CFG_USBD_MSC_EN == DEF_ENABLED)
#include  <Class/MSC/usbd_msc.h>


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         App_USBD_MSC_Init()
*
* Description : Initialize USB device mass storage class.
*
* Argument(s) : p_dev     Pointer to USB device.
*
*               cfg_hs    Index of high-speed configuration to which this interface will be added to.
*
*               cfg_fs    Index of high-speed configuration to which this interface will be added to.
*
* Return(s)   : DEF_OK,   if the Mass storage interface was added.
*               DEF_FAIL, if the Mass storage interface could not be added.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  App_USBD_MSC_Init (CPU_INT08U  dev_nbr,
                                CPU_INT08U  cfg_hs,
                                CPU_INT08U  cfg_fs)
{
    USBD_ERR     err;
    CPU_INT08U   msc_nbr;
    CPU_BOOLEAN  valid;


    APP_TRACE_DBG(("        Initializing MSC class ... \r\n"));

    USBD_MSC_Init(&err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not initialize MSC class w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

    msc_nbr = USBD_MSC_Add(&err);

    if (cfg_hs != USBD_CFG_NBR_NONE) {
        valid = USBD_MSC_CfgAdd (msc_nbr,
                                 dev_nbr,
                                 cfg_hs,
                                 &err);

        if (valid != DEF_YES) {
            APP_TRACE_DBG(("        ... could not add msc instance #%d to HS configuration w/err = %d\r\n\r\n", msc_nbr, err));
            return (DEF_FAIL);
        }
    }

    if (cfg_fs != USBD_CFG_NBR_NONE) {
        valid = USBD_MSC_CfgAdd (msc_nbr,
                                dev_nbr,
                                cfg_fs,
                               &err);

        if (valid != DEF_YES) {
            APP_TRACE_DBG(("        ... could not add msc instance #%d to FS configuration w/err = %d\r\n\r\n", msc_nbr, err));
            return (DEF_FAIL);
        }
    }
                                                                /* Add Logical Unit to MSC interface.                   */
    USBD_MSC_LunAdd((void *)"ram:0:",
                             msc_nbr,
                    (void *)"Micrium",
                    (void *)"MSC FS Storage",
                             0x00,
                             DEF_FALSE,
                            &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not add LU to MSC class w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

    return (DEF_OK);
}

#endif
