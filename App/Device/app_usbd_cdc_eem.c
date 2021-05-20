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
*                         USB DEVICE CDC EEM CLASS APPLICATION INITIALIZATION
*
*                                              TEMPLATE
*
* Filename : app_usbd_cdc_eem.c
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
    (APP_CFG_USBD_CDC_EEM_EN == DEF_ENABLED)
#include  <Class/CDC-EEM/usbd_cdc_eem.h>

#include  <Source/net.h>
#include  <Source/net_ascii.h>
#include  <Source/net_err.h>
#include  <IF/net_if.h>
#include  <IP/IPv4/net_ipv4.h>
#include  <net_dev_cfg.h>
#include  <Dev/Ether/USBD_CDCEEM/net_dev_usbd_cdceem.h>


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
*                                       App_USBD_CDC_EEM_Init()
*
* Description : Initialize USB device CDC EEM subclass.
*
* Argument(s) : dev_nbr    Device number.
*
*               cfg_hs     Index of high-speed configuration to which this interface will be added to.
*
*               cfg_fs     Index of high-speed configuration to which this interface will be added to.
*
* Return(s)   : DEF_OK,    if successful.
*               DEF_FAIL,  otherwise.
*
* Note(s)     : (1) This function assumes that uC/TCP-IP with USBD_CDCEEM driver is part of the project
*                   and that it has been initialized.
*
*               (2) This example will set a static IPv4 address to the interface. For more examples that
*                   uses IPv6 and/or assignation of address via DHCP, see uC/TCP-IPs application
*                   examples.
*********************************************************************************************************
*/

CPU_BOOLEAN  App_USBD_CDC_EEM_Init (CPU_INT08U  dev_nbr,
                                    CPU_INT08U  cfg_hs,
                                    CPU_INT08U  cfg_fs)
{
    CPU_BOOLEAN    valid;
    CPU_INT08U     cdc_eem_nbr;
    USBD_ERR       err;
    NET_IF_NBR     net_if_nbr;
    NET_IPv4_ADDR  addr;
    NET_IPv4_ADDR  subnet_mask;
    NET_IPv4_ADDR  dflt_gateway;
    NET_ERR        err_net;


    APP_TRACE_DBG(("        Initializing CDC EEM class ... \r\n"));

    USBD_CDC_EEM_Init(&err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not initialize CDC EEM class w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

    cdc_eem_nbr = USBD_CDC_EEM_Add(&err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not create CDC EEM class instance w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

                                                                /* Add CDC EEM class instance to USB configuration(s).  */
    if (cfg_hs != USBD_CFG_NBR_NONE) {
        USBD_CDC_EEM_CfgAdd(cdc_eem_nbr,
                            dev_nbr,
                            cfg_hs,
                           "CDC EEM interface",
                           &err);
        if (err != USBD_ERR_NONE) {
            APP_TRACE_DBG(("        ... could not add CDC EEM instance #%d to HS configuration w/err = %d\r\n\r\n", cdc_eem_nbr, err));
            return (DEF_FAIL);
        }
    }

    if (cfg_fs != USBD_CFG_NBR_NONE) {
        USBD_CDC_EEM_CfgAdd(cdc_eem_nbr,
                            dev_nbr,
                            cfg_fs,
                           "CDC EEM interface",
                           &err);
        if (err != USBD_ERR_NONE) {
            APP_TRACE_DBG(("        ... could not add CDC EEM instance #%d to FS configuration w/err = %d\r\n\r\n", cdc_eem_nbr, err));
            return (DEF_FAIL);
        }
    }

                                                                /* Add uC/TCP-IP interface using CDC EEM.               */
    NetDev_Cfg_Ether_USBD_CDCEEM.ClassNbr = cdc_eem_nbr;        /* Set CDC EEM class instance number to drv cfg.        */
    net_if_nbr = NetIF_Add((void *)&NetIF_API_Ether,
                           (void *)&NetDev_API_USBD_CDCEEM,
                                    DEF_NULL,
                           (void *)&NetDev_Cfg_Ether_USBD_CDCEEM,
                                    DEF_NULL,
                                    DEF_NULL,
                                   &err_net);
    if (err_net != NET_IF_ERR_NONE) {
        APP_TRACE_DBG(("    ... could not add TCP IP IF w/err = %d\r\n\r\n", err_net));
        return (DEF_FAIL);
    }


                                                                /* Set static address to device.                        */
    addr         = NetASCII_Str_to_IPv4("192.168.0.10",
                                        &err_net);
    subnet_mask  = NetASCII_Str_to_IPv4("255.255.255.0",
                                        &err_net);
    dflt_gateway = NetASCII_Str_to_IPv4("192.168.0.1",
                                        &err_net);

    NetIPv4_CfgAddrAdd(net_if_nbr,
                       addr,
                       subnet_mask,
                       dflt_gateway,
                      &err_net);
    if (err_net != NET_IPv4_ERR_NONE) {
        APP_TRACE_DBG(("    ... could not add static IPv4 address to TCP IP IF w/err = %d\r\n\r\n", err_net));
        return (DEF_FAIL);
    }

    NetIF_Start(net_if_nbr, &err_net);                          /* Start uC/TCP-IP interface.                           */
    if (err_net != NET_IF_ERR_NONE) {
        APP_TRACE_DBG(("    ... could not start TCP IP IF w/err = %d\r\n\r\n", err_net));
        return (DEF_FAIL);
    }

    return (DEF_OK);
}
#endif
