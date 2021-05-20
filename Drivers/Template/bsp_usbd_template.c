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
*                       USB DEVICE DRIVER BOARD SUPPORT PACKAGE (BSP) FUNCTIONS
*
*                                              TEMPLATE
*
* Filename : bsp_usbd_template.c
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../../Source/usbd_core.h"
#include  "bsp_usbd_template.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                USB DEVICE ENDPOINT INFORMATION TABLE
*********************************************************************************************************
*/

USBD_DRV_EP_INFO  USBD_DrvEP_InfoTbl_Template[] = {
    {USBD_EP_INFO_TYPE_CTRL                                                   | USBD_EP_INFO_DIR_OUT, 0u,   64u},
    {USBD_EP_INFO_TYPE_CTRL                                                   | USBD_EP_INFO_DIR_IN,  0u,   64u},
    {USBD_EP_INFO_TYPE_ISOC | USBD_EP_INFO_TYPE_BULK | USBD_EP_INFO_TYPE_INTR | USBD_EP_INFO_DIR_OUT, 1u, 1024u},
    {USBD_EP_INFO_TYPE_ISOC | USBD_EP_INFO_TYPE_BULK | USBD_EP_INFO_TYPE_INTR | USBD_EP_INFO_DIR_IN,  1u, 1024u},
    {USBD_EP_INFO_TYPE_ISOC | USBD_EP_INFO_TYPE_BULK | USBD_EP_INFO_TYPE_INTR | USBD_EP_INFO_DIR_OUT, 2u, 1024u},
    {USBD_EP_INFO_TYPE_ISOC | USBD_EP_INFO_TYPE_BULK | USBD_EP_INFO_TYPE_INTR | USBD_EP_INFO_DIR_IN,  2u, 1024u},
    {USBD_EP_INFO_TYPE_ISOC | USBD_EP_INFO_TYPE_BULK | USBD_EP_INFO_TYPE_INTR | USBD_EP_INFO_DIR_OUT, 3u, 1024u},
    {USBD_EP_INFO_TYPE_ISOC | USBD_EP_INFO_TYPE_BULK | USBD_EP_INFO_TYPE_INTR | USBD_EP_INFO_DIR_IN,  3u, 1024u},
    {DEF_BIT_NONE                                                                                 ,   0u,    0u}
};


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

static  USBD_DRV  *BSP_USBD_Controller_DrvPtr;


/*
*********************************************************************************************************
*                                            LOCAL MACROS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  BSP_USBD_Template_Init        (USBD_DRV  *p_drv);
static  void  BSP_USBD_Template_Connect     (void            );
static  void  BSP_USBD_Template_Disconnect  (void            );
static  void  BSP_USBD_Controller_IntHandler(void      *p_arg);


/*
*********************************************************************************************************
*                                   USB DEVICE DRIVER BSP INTERFACE
*********************************************************************************************************
*/

USBD_DRV_BSP_API  USBD_DrvBSP_Template = {
    BSP_USBD_Template_Init,
    BSP_USBD_Template_Connect,
    BSP_USBD_Template_Disconnect
};


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                      BSP_USBD_Template_Init()
*
* Description : USB device controller board-specific initialization.
*
* Argument(s) : p_drv    Pointer to device driver structure.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  BSP_USBD_Template_Init (USBD_DRV  *p_drv)
{
    (void)p_drv;

    /* $$$$ If ISR does not support argument passing, save reference to USBD_DRV in a local global variable. */
    BSP_USBD_Controller_DrvPtr = p_drv;

    /* $$$$ This function performs all operations that the device controller cannot do. Typical operations are: */

    /* $$$$ Enable device control registers and bus clock [mandatory]. */
    /* $$$$ Configure main USB device interrupt in interrupt controller (e.g. registering BSP ISR) [mandatory]. */
    /* $$$$ Disable device transceiver clock [optional]. */
    /* $$$$ Configure I/O pins [if necessary]. */
}


/*
*********************************************************************************************************
*                                     BSP_USBD_Template_Connect()
*
* Description : Connect pull-up on DP.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  BSP_USBD_Template_Connect (void)
{
    /* $$$$ Enable device transceiver clock [optional]. */
    /* $$$$ Enable pull-up resistor (this operation may be done in the driver) [mandatory]. */
    /* $$$$ Enable main USB device interrupt [mandatory]. */
}


/*
*********************************************************************************************************
*                                   BSP_USBD_Template_Disconnect()
*
* Description : Disconnect pull-up on DP.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  BSP_USBD_Template_Disconnect (void)
{
    /* $$$$ Disable device transceiver clock [optional]. */
    /* $$$$ Disable pull-up resistor (this operation may be done in the driver) [mandatory]. */
    /* $$$$ Disable main USB device interrupt [mandatory]. */
}


/*
*********************************************************************************************************
*                                  BSP_USBD_Controller_IntHandler()
*
* Description : USB device interrupt handler.
*
* Argument(s) : p_arg    Interrupt handler argument.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  BSP_USBD_Controller_IntHandler (void  *p_arg)
{
    USBD_DRV      *p_drv;
    USBD_DRV_API  *p_drv_api;


                                                                /* Get a reference to USBD_DRV.                         */
    /* $$$$ If ISR does not support argument passing, get USBD_DRV from a global variable initialized in BSP_USBD_Template_Init(). */
    p_drv = BSP_USBD_Controller_DrvPtr;
    /* $$$$ Otherwise if ISR supports argument passing, get USBD_DRV from the argument. Do not forget to pass USBD_DRV structure when registering the BSP ISR in BSP_USBD_Template_Init(). */
    p_drv = (USBD_DRV *)p_arg;

    p_drv_api = p_drv->API_Ptr;                                 /* Get a reference to USBD_DRV_API.                     */
    p_drv_api->ISR_Handler(p_drv);                              /* Call the USB Device driver ISR.                      */
}
