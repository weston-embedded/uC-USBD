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
*                                   USB AUDIO CODEC DRIVER TEMPLATE
*
* Filename : usbd_audio_drv_template.c
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../../usbd_audio.h"
#include  "usbd_audio_drv_template.h"
#include  <usbd_audio_dev_cfg.h>


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/
                                                                /* +1u for master channel.                              */
#define  USBD_AUDIO_MAX_LOG_CH                         (USBD_AUDIO_STEREO + 1u)

#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_MAX_BANDS      1u
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_29_IX     0u
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_30_IX     1u

#define  USBD_AUDIO_MU_MAX_INPUT                        4u


/*
**********************************************************************************************************
*                                            TRACE MACROS
**********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/


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


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

static  CPU_INT08U   USBD_Audio_OT_CopyProtTbl[USBD_AUDIO_DEV_CFG_NBR_ENTITY];

static  CPU_BOOLEAN  USBD_Audio_FU_MuteTbl[USBD_AUDIO_DEV_CFG_NBR_ENTITY][USBD_AUDIO_MAX_LOG_CH];

static  CPU_INT16U   USBD_Audio_FU_VolTbl[USBD_AUDIO_DEV_CFG_NBR_ENTITY][USBD_AUDIO_MAX_LOG_CH];

static  CPU_INT08U   USBD_Audio_FU_BassTbl[USBD_AUDIO_DEV_CFG_NBR_ENTITY][USBD_AUDIO_MAX_LOG_CH];

static  CPU_INT08U   USBD_Audio_FU_MidTbl[USBD_AUDIO_DEV_CFG_NBR_ENTITY][USBD_AUDIO_MAX_LOG_CH];

static  CPU_INT08U   USBD_Audio_FU_TrebleTbl[USBD_AUDIO_DEV_CFG_NBR_ENTITY][USBD_AUDIO_MAX_LOG_CH];

static  CPU_INT08U   USBD_Audio_FU_GraphicEqualizerTbl[USBD_AUDIO_DEV_CFG_NBR_ENTITY][USBD_AUDIO_MAX_LOG_CH][USBD_AUDIO_FU_GRAPHIC_EQUALIZER_MAX_BANDS];

static  CPU_BOOLEAN  USBD_Audio_FU_AutoGainTbl[USBD_AUDIO_DEV_CFG_NBR_ENTITY][USBD_AUDIO_MAX_LOG_CH];

static  CPU_INT16U   USBD_Audio_FU_DlyTbl[USBD_AUDIO_DEV_CFG_NBR_ENTITY][USBD_AUDIO_MAX_LOG_CH];

static  CPU_BOOLEAN  USBD_Audio_FU_BassBoostTbl[USBD_AUDIO_DEV_CFG_NBR_ENTITY][USBD_AUDIO_MAX_LOG_CH];

static  CPU_BOOLEAN  USBD_Audio_FU_LoudnessTbl[USBD_AUDIO_DEV_CFG_NBR_ENTITY][USBD_AUDIO_MAX_LOG_CH];

static  CPU_INT16U   USBD_Audio_MU_Tbl[USBD_AUDIO_DEV_CFG_NBR_ENTITY][USBD_AUDIO_MU_MAX_INPUT * USBD_AUDIO_MAX_LOG_CH][USBD_AUDIO_MAX_LOG_CH];

static  CPU_INT08U   USBD_Audio_SU_Tbl[USBD_AUDIO_DEV_CFG_NBR_ENTITY];

static  CPU_INT32U   USBD_Audio_AS_EP_SamplingFreqTbl[USBD_AUDIO_DEV_CFG_NBR_ENTITY];

static  CPU_BOOLEAN  USBD_Audio_AS_EP_PitchTbl[USBD_AUDIO_DEV_CFG_NBR_ENTITY];


/*
*********************************************************************************************************
*                                 AUDIO API DRIVER FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void         USBD_Audio_DrvInit                         (USBD_AUDIO_DRV        *p_audio_drv,
                                                                 USBD_ERR              *p_err);

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlOT_CopyProtSet           (USBD_AUDIO_DRV        *p_audio_drv,
                                                                 CPU_INT08U             terminal_id,
                                                                 CPU_INT08U             copy_prot_level);

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_MuteManage            (USBD_AUDIO_DRV        *p_audio_drv,
                                                                 CPU_INT08U             unit_id,
                                                                 CPU_INT08U             log_ch_nbr,
                                                                 CPU_BOOLEAN            set_en,
                                                                 CPU_BOOLEAN           *p_mute);

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_VolManage             (USBD_AUDIO_DRV        *p_audio_drv,
                                                                 CPU_INT08U             req,
                                                                 CPU_INT08U             unit_id,
                                                                 CPU_INT08U             log_ch_nbr,
                                                                 CPU_INT16U            *p_vol);

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_BassManage            (USBD_AUDIO_DRV        *p_audio_drv,
                                                                 CPU_INT08U             req,
                                                                 CPU_INT08U             unit_id,
                                                                 CPU_INT08U             log_ch_nbr,
                                                                 CPU_INT08U            *p_bass);

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_MidManage             (USBD_AUDIO_DRV        *p_audio_drv,
                                                                 CPU_INT08U             req,
                                                                 CPU_INT08U             unit_id,
                                                                 CPU_INT08U             log_ch_nbr,
                                                                 CPU_INT08U            *p_mid);

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_TrebleManage          (USBD_AUDIO_DRV        *p_audio_drv,
                                                                 CPU_INT08U             req,
                                                                 CPU_INT08U             unit_id,
                                                                 CPU_INT08U             log_ch_nbr,
                                                                 CPU_INT08U            *p_treble);

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_GraphicEqualizerManage(USBD_AUDIO_DRV        *p_audio_drv,
                                                                 CPU_INT08U             req,
                                                                 CPU_INT08U             unit_id,
                                                                 CPU_INT08U             log_ch_nbr,
                                                                 CPU_INT08U             nbr_bands_present,
                                                                 CPU_INT32U            *p_bm_bands_present,
                                                                 CPU_INT08U            *p_buf);

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_AutoGainManage        (USBD_AUDIO_DRV        *p_audio_drv,
                                                                 CPU_INT08U             unit_id,
                                                                 CPU_INT08U             log_ch_nbr,
                                                                 CPU_BOOLEAN            set_en,
                                                                 CPU_BOOLEAN           *p_auto_gain);

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_DlyManage             (USBD_AUDIO_DRV        *p_audio_drv,
                                                                 CPU_INT08U             req,
                                                                 CPU_INT08U             unit_id,
                                                                 CPU_INT08U             log_ch_nbr,
                                                                 CPU_INT16U            *p_dly);

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_BassBoostManage       (USBD_AUDIO_DRV        *p_audio_drv,
                                                                 CPU_INT08U             unit_id,
                                                                 CPU_INT08U             log_ch_nbr,
                                                                 CPU_BOOLEAN            set_en,
                                                                 CPU_BOOLEAN           *p_bass_boost);

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_LoudnessManage        (USBD_AUDIO_DRV        *p_audio_drv,
                                                                 CPU_INT08U             unit_id,
                                                                 CPU_INT08U             log_ch_nbr,
                                                                 CPU_BOOLEAN            set_en,
                                                                 CPU_BOOLEAN           *p_loudness);

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlMU_CtrlManage            (USBD_AUDIO_DRV        *p_audio_drv,
                                                                 CPU_INT08U             req,
                                                                 CPU_INT08U             unit_id,
                                                                 CPU_INT08U             log_in_ch_nbr,
                                                                 CPU_INT08U             log_out_ch_nbr,
                                                                 CPU_INT16U            *p_ctrl);

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlSU_InPinManage           (USBD_AUDIO_DRV        *p_audio_drv,
                                                                 CPU_INT08U             unit_id,
                                                                 CPU_BOOLEAN            set_en,
                                                                 CPU_INT08U            *p_in_pin_nbr);

static  CPU_BOOLEAN  USBD_Audio_DrvAS_SamplingFreqManage        (USBD_AUDIO_DRV        *p_audio_drv,
                                                                 CPU_INT08U             terminal_id_link,
                                                                 CPU_BOOLEAN            set_en,
                                                                 CPU_INT32U            *p_sampling_freq);

static  CPU_BOOLEAN  USBD_Audio_DrvAS_PitchManage               (USBD_AUDIO_DRV        *p_audio_drv,
                                                                 CPU_INT08U             terminal_id_link,
                                                                 CPU_BOOLEAN            set_en,
                                                                 CPU_BOOLEAN           *p_pitch);

static  CPU_BOOLEAN  USBD_Audio_DrvStreamStart                  (USBD_AUDIO_DRV        *p_audio_drv,
                                                                 USBD_AUDIO_AS_HANDLE   as_handle,
                                                                 CPU_INT08U             terminal_id_link);

static  CPU_BOOLEAN  USBD_Audio_DrvStreamStop                   (USBD_AUDIO_DRV        *p_audio_drv,
                                                                 CPU_INT08U             terminal_id_link);

static  void         USBD_Audio_DrvStreamRecordRx               (USBD_AUDIO_DRV        *p_audio_drv,
                                                                 CPU_INT08U             terminal_id_link,
                                                                 void                  *p_buf,
                                                                 CPU_INT16U            *p_buf_len,
                                                                 USBD_ERR              *p_err);

static  void         USBD_Audio_DrvStreamPlaybackTx             (USBD_AUDIO_DRV        *p_audio_drv,
                                                                 CPU_INT08U             terminal_id_link,
                                                                 void                  *p_buf,
                                                                 CPU_INT16U             buf_len,
                                                                 USBD_ERR              *p_err);


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
*                                  USB DEVICE CONTROLLER DRIVER API
*********************************************************************************************************
*/

const  USBD_AUDIO_DRV_COMMON_API  USBD_Audio_DrvCommonAPI_Template = {
    USBD_Audio_DrvInit
};

const  USBD_AUDIO_DRV_AC_OT_API  USBD_Audio_DrvOT_API_Template = {
    USBD_Audio_DrvCtrlOT_CopyProtSet
};

const  USBD_AUDIO_DRV_AC_FU_API  USBD_Audio_DrvFU_API_Template = {
    USBD_Audio_DrvCtrlFU_MuteManage,
    USBD_Audio_DrvCtrlFU_VolManage,
    USBD_Audio_DrvCtrlFU_BassManage,
    USBD_Audio_DrvCtrlFU_MidManage,
    USBD_Audio_DrvCtrlFU_TrebleManage,
    USBD_Audio_DrvCtrlFU_GraphicEqualizerManage,
    USBD_Audio_DrvCtrlFU_AutoGainManage,
    USBD_Audio_DrvCtrlFU_DlyManage,
    USBD_Audio_DrvCtrlFU_BassBoostManage,
    USBD_Audio_DrvCtrlFU_LoudnessManage
};

const  USBD_AUDIO_DRV_AC_MU_API  USBD_Audio_DrvMU_API_Template = {
    USBD_Audio_DrvCtrlMU_CtrlManage
};

const  USBD_AUDIO_DRV_AC_SU_API  USBD_Audio_DrvSU_API_Template = {
    USBD_Audio_DrvCtrlSU_InPinManage
};

const  USBD_AUDIO_DRV_AS_API  USBD_Audio_DrvAS_API_Template = {
    USBD_Audio_DrvAS_SamplingFreqManage,
    USBD_Audio_DrvAS_PitchManage,
    USBD_Audio_DrvStreamStart,
    USBD_Audio_DrvStreamStop,
    USBD_Audio_DrvStreamRecordRx,
    USBD_Audio_DrvStreamPlaybackTx
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
*                                        USBD_Audio_DrvInit()
*
* Description : Initialize software resources, audio codec and different MCU peripherals needed
*               to communicate with it.
*
* Argument(s) : p_audio_drv     Pointer to audio driver structure.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE   Audio device successfully initialized.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_Audio_DrvInit (USBD_AUDIO_DRV  *p_audio_drv,
                                  USBD_ERR        *p_err)
{
    (void)p_audio_drv;

    Mem_Clr((void *)&USBD_Audio_OT_CopyProtTbl[0u],
                    (USBD_AUDIO_DEV_CFG_NBR_ENTITY * (sizeof(CPU_INT08U))));

    Mem_Clr((void *)&USBD_Audio_FU_MuteTbl[0u][0u],
                    (USBD_AUDIO_DEV_CFG_NBR_ENTITY * USBD_AUDIO_MAX_LOG_CH * (sizeof(CPU_BOOLEAN))));

    Mem_Clr((void *)&USBD_Audio_FU_VolTbl[0u][0u],
                    (USBD_AUDIO_DEV_CFG_NBR_ENTITY * USBD_AUDIO_MAX_LOG_CH * (sizeof(CPU_INT16U))));

    Mem_Clr((void *)&USBD_Audio_FU_BassTbl[0u][0u],
                    (USBD_AUDIO_DEV_CFG_NBR_ENTITY * USBD_AUDIO_MAX_LOG_CH * (sizeof(CPU_INT08U))));

    Mem_Clr((void *)&USBD_Audio_FU_MidTbl[0u][0u],
                    (USBD_AUDIO_DEV_CFG_NBR_ENTITY * USBD_AUDIO_MAX_LOG_CH * (sizeof(CPU_INT08U))));

    Mem_Clr((void *)&USBD_Audio_FU_TrebleTbl[0u][0u],
                    (USBD_AUDIO_DEV_CFG_NBR_ENTITY * USBD_AUDIO_MAX_LOG_CH * (sizeof(CPU_INT08U))));

    Mem_Clr((void *)&USBD_Audio_FU_GraphicEqualizerTbl[0u][0u][0u],
                    (USBD_AUDIO_DEV_CFG_NBR_ENTITY * USBD_AUDIO_MAX_LOG_CH * USBD_AUDIO_FU_GRAPHIC_EQUALIZER_MAX_BANDS * (sizeof(CPU_INT08U))));

    Mem_Clr((void *)&USBD_Audio_FU_AutoGainTbl[0u][0u],
                    (USBD_AUDIO_DEV_CFG_NBR_ENTITY * USBD_AUDIO_MAX_LOG_CH * (sizeof(CPU_BOOLEAN))));

    Mem_Clr((void *)&USBD_Audio_FU_DlyTbl[0u][0u],
                    (USBD_AUDIO_DEV_CFG_NBR_ENTITY * USBD_AUDIO_MAX_LOG_CH * (sizeof(CPU_INT16U))));

    Mem_Clr((void *)&USBD_Audio_FU_BassBoostTbl[0u][0u],
                    (USBD_AUDIO_DEV_CFG_NBR_ENTITY * USBD_AUDIO_MAX_LOG_CH * (sizeof(CPU_BOOLEAN))));

    Mem_Clr((void *)&USBD_Audio_FU_LoudnessTbl[0u][0u],
                    (USBD_AUDIO_DEV_CFG_NBR_ENTITY * USBD_AUDIO_MAX_LOG_CH * (sizeof(CPU_BOOLEAN))));

    Mem_Clr((void *)&USBD_Audio_MU_Tbl[0u][0u][0u],
                    (USBD_AUDIO_DEV_CFG_NBR_ENTITY * (USBD_AUDIO_MU_MAX_INPUT * USBD_AUDIO_MAX_LOG_CH) * USBD_AUDIO_MAX_LOG_CH * (sizeof(CPU_INT16U))));

    Mem_Clr((void *)&USBD_Audio_SU_Tbl[0u],
                    (USBD_AUDIO_DEV_CFG_NBR_ENTITY * (sizeof(CPU_INT08U))));
    Mem_Set((void *)&USBD_Audio_SU_Tbl[0u],
                    1u,
                   (USBD_AUDIO_DEV_CFG_NBR_ENTITY * (sizeof(CPU_INT08U))));

    Mem_Clr((void *)&USBD_Audio_AS_EP_SamplingFreqTbl[0u],
                    (USBD_AUDIO_DEV_CFG_NBR_ENTITY * (sizeof(CPU_INT32U))));

    Mem_Clr((void *)&USBD_Audio_AS_EP_PitchTbl[0u],
                    (USBD_AUDIO_DEV_CFG_NBR_ENTITY * (sizeof(CPU_BOOLEAN))));\

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                 USBD_Audio_DrvCtrlOT_CopyProtSet()
*
* Description : Set Copy Protection Level for a particular Output Terminal.
*
* Argument(s) : p_audio_drv         Pointer to audio driver structure.
*
*               terminal_id         Output Terminal ID.
*
*               copy_prot_level     Copy protection level to set.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlOT_CopyProtSet (USBD_AUDIO_DRV  *p_audio_drv,
                                                       CPU_INT08U       terminal_id,
                                                       CPU_INT08U       copy_prot_level)
{
    (void)p_audio_drv;

    USBD_Audio_OT_CopyProtTbl[terminal_id - 1u] = copy_prot_level;

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                   USBD_Audio_DrvCtrlFU_MuteManage()
*
* Description : Get or set mute state for one or all logical channels inside a cluster.
*
* Argument(s) : p_audio_drv     Pointer to audio driver structure.
*
*               unit_id         Feature Unit ID.
*
*               log_ch_nbr      Logical channel number (see Note #1).
*
*               set_en          Flag indicating to get or set the mute.
*
*               p_mute          Pointer to the mute state to get or set.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) log_ch_nbr allows you to get the mute state for a specific channel. When log_ch_nbr
*                   is 0, you get the mute state for all channels. Indeed, the logical channel #0
*                   represents the master channel and encompasses all channels.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_MuteManage (USBD_AUDIO_DRV  *p_audio_drv,
                                                      CPU_INT08U       unit_id,
                                                      CPU_INT08U       log_ch_nbr,
                                                      CPU_BOOLEAN      set_en,
                                                      CPU_BOOLEAN     *p_mute)
{
    CPU_BOOLEAN  req_ok = DEF_FAIL;


    (void)p_audio_drv;

    if (set_en == DEF_FALSE) {
       *p_mute = USBD_Audio_FU_MuteTbl[unit_id - 1u][log_ch_nbr];
        req_ok = DEF_OK;
    } else {
        USBD_Audio_FU_MuteTbl[unit_id - 1u][log_ch_nbr] = *p_mute;
        req_ok                                          =  DEF_OK;
    }

    return (req_ok);
}


/*
*********************************************************************************************************
*                                  USBD_Audio_DrvCtrlFU_VolManage()
*
* Description : Get or set volume for one or all logical channels inside a cluster.
*
* Argument(s) : p_audio_drv     Pointer to audio driver structure.
*
*               req             Volume request:
*
*                               USBD_AUDIO_REQ_GET_CUR
*                               USBD_AUDIO_REQ_GET_RES
*                               USBD_AUDIO_REQ_GET_MIN
*                               USBD_AUDIO_REQ_GET_MAX
*                               USBD_AUDIO_REQ_SET_CUR
*
*               unit_id         Feature Unit ID.
*
*               log_ch_nbr      Logical channel number (see Note #2).
*
*               p_vol           Pointer to the volume value to set or get.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) The Volume Control values range allowed for Feature Unit is:
*
*                   (a) From +127.9961 dB (0x7FFF) down to -127.9961 dB (0x8001) for CUR, MIN, and MAX
*                       attributes
*
*                   (b) From 1/256 dB (0x0001) to +127.9961 dB (0x7FFF) for RES attribute.
*
*               (2) 'p_vol' uses little endian memory organization. Hence, you should use uC/LIB macros
*                   MEM_VAL_GET_INT16U_LITTLE() and MEM_VAL_SET_INT16U_LITTLE() when reading or writing
*                   the volume from/to it. This will ensure data is accessed correctly regarding your
*                   CPU endianness.
*
*               (3) log_ch_nbr allows you to get or set the volume for a specific channel. When
*                   log_ch_nbr is 0, you get or set the volume for all channels. Indeed, the logical
*                   channel #0 represents the master channel and encompasses all channels.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_VolManage (USBD_AUDIO_DRV  *p_audio_drv,
                                                     CPU_INT08U       req,
                                                     CPU_INT08U       unit_id,
                                                     CPU_INT08U       log_ch_nbr,
                                                     CPU_INT16U      *p_vol)
{
    CPU_BOOLEAN  req_ok = DEF_FAIL;


    (void)p_audio_drv;

    switch (req) {
        case USBD_AUDIO_REQ_GET_CUR:
            *p_vol  = USBD_Audio_FU_VolTbl[unit_id - 1u][log_ch_nbr];
             req_ok = DEF_OK;
             break;


        case USBD_AUDIO_REQ_GET_MIN:
            *p_vol  = 0x8001u;                                  /* -127.9961 dB (see Note #1a).                         */
             req_ok = DEF_OK;
             break;


        case USBD_AUDIO_REQ_GET_MAX:
            *p_vol  = 0x7FFFu;                                  /* +127.9961 dB (see Note #1a).                         */
             req_ok = DEF_OK;
             break;


        case USBD_AUDIO_REQ_GET_RES:
            *p_vol  = 0x0001u;                                  /* Resolution of 1/256 dB (see Note #1b).               */
             req_ok = DEF_OK;
             break;


        case USBD_AUDIO_REQ_SET_CUR:
             USBD_Audio_FU_VolTbl[unit_id - 1u][log_ch_nbr] = *p_vol;
             req_ok                                         =  DEF_OK;
             break;


        default:
             break;
    }

    return (req_ok);

}


/*
*********************************************************************************************************
*                                  USBD_Audio_DrvCtrlFU_BassManage()
*
* Description : Get or set bass for one or all logical channels inside a cluster.
*
* Argument(s) : p_audio_drv     Pointer to audio driver structure.
*
*               req             Bass request:
*
*                               USBD_AUDIO_REQ_GET_CUR
*                               USBD_AUDIO_REQ_GET_RES
*                               USBD_AUDIO_REQ_GET_MIN
*                               USBD_AUDIO_REQ_GET_MAX
*                               USBD_AUDIO_REQ_SET_CUR
*
*               unit_id         Feature Unit ID.
*
*               log_ch_nbr      Logical channel number (see Note #2).
*
*               p_bass          Pointer to the Bass value to set or get.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) Bass Control values range allowed for Feature Unit is:
*
*                   (a) From +31.75 dB (0x7F) down to  32.00 dB (0x80) for CUR, MIN, and MAX attributes.
*
*                   (b) From 0.25 dB (0x01) to +31.75 dB (0x7F) for RES attribute.
*
*               (2) log_ch_nbr allows you to get or set the bass for a specific channel. When log_ch_nbr
*                   is 0, you get or set the bass for all channels. Indeed, the logical channel #0
*                   represents the master channel and encompasses all channels.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_BassManage (USBD_AUDIO_DRV  *p_audio_drv,
                                                      CPU_INT08U       req,
                                                      CPU_INT08U       unit_id,
                                                      CPU_INT08U       log_ch_nbr,
                                                      CPU_INT08U      *p_bass)
{
    CPU_BOOLEAN  req_ok = DEF_FAIL;


    (void)p_audio_drv;

    switch (req) {
        case USBD_AUDIO_REQ_GET_CUR:
            *p_bass = USBD_Audio_FU_BassTbl[unit_id - 1u][log_ch_nbr];
             req_ok = DEF_OK;
             break;

        case USBD_AUDIO_REQ_GET_MIN:
            *p_bass = 0x80u;
             req_ok = DEF_OK;
             break;

        case USBD_AUDIO_REQ_GET_MAX:
            *p_bass = 0x7Fu;
             req_ok = DEF_OK;
             break;

        case USBD_AUDIO_REQ_GET_RES:
            *p_bass = 0x01u;
             req_ok = DEF_OK;
             break;

        case USBD_AUDIO_REQ_SET_CUR:
             USBD_Audio_FU_BassTbl[unit_id - 1u][log_ch_nbr] = *p_bass;
             req_ok                                          =  DEF_OK;
             break;

        default:
             break;
    }

    return (req_ok);
}


/*
*********************************************************************************************************
*                                  USBD_Audio_DrvCtrlFU_MidManage()
*
* Description : Get or set middle for one or all logical channels inside a cluster.
*
* Argument(s) : p_audio_drv     Pointer to audio driver structure.
*
*               req             Middle request:
*
*                               USBD_AUDIO_REQ_GET_CUR
*                               USBD_AUDIO_REQ_GET_RES
*                               USBD_AUDIO_REQ_GET_MIN
*                               USBD_AUDIO_REQ_GET_MAX
*                               USBD_AUDIO_REQ_SET_CUR
*
*               unit_id         Feature Unit ID.
*
*               log_ch_nbr      Logical channel number (see Note #2).
*
*               p_mid           Pointer to the Middle value to set or get.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) Middle Control values range allowed for Feature Unit is:
*
*                   (a) From +31.75 dB (0x7F) down to  32.00 dB (0x80) for CUR, MIN, and MAX attributes.
*
*                   (b) From 0.25 dB (0x01) to +31.75 dB (0x7F) for RES attribute.
*
*               (2) log_ch_nbr allows you to get or set the middle for a specific channel. When
*                   log_ch_nbr is 0, you get or set the middle for all channels. Indeed, the logical
*                   channel #0 represents the master channel and encompasses all channels.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_MidManage (USBD_AUDIO_DRV  *p_audio_drv,
                                                     CPU_INT08U       req,
                                                     CPU_INT08U       unit_id,
                                                     CPU_INT08U       log_ch_nbr,
                                                     CPU_INT08U      *p_mid)
{
    CPU_BOOLEAN  req_ok = DEF_FAIL;

    (void)p_audio_drv;

    switch (req) {
        case USBD_AUDIO_REQ_GET_CUR:
            *p_mid  = USBD_Audio_FU_MidTbl[unit_id - 1u][log_ch_nbr];
             req_ok = DEF_OK;
             break;

        case USBD_AUDIO_REQ_GET_MIN:
            *p_mid  = 0x80u;
             req_ok = DEF_OK;
             break;

        case USBD_AUDIO_REQ_GET_MAX:
            *p_mid  = 0x7Fu;
             req_ok = DEF_OK;
             break;

        case USBD_AUDIO_REQ_GET_RES:
            *p_mid  = 0x01u;
             req_ok = DEF_OK;
             break;

        case USBD_AUDIO_REQ_SET_CUR:
             USBD_Audio_FU_MidTbl[unit_id - 1u][log_ch_nbr] = *p_mid;
             req_ok                                         =  DEF_OK;
             break;

        default:
             break;
    }

    return (req_ok);
}


/*
*********************************************************************************************************
*                                 USBD_Audio_DrvCtrlFU_TrebleManage()
*
* Description : Get or set treble for one or all logical channels inside a cluster.
*
* Argument(s) : p_audio_drv     Pointer to audio driver structure.
*
*               req             Treble request:
*
*                               USBD_AUDIO_REQ_GET_CUR
*                               USBD_AUDIO_REQ_GET_RES
*                               USBD_AUDIO_REQ_GET_MIN
*                               USBD_AUDIO_REQ_GET_MAX
*                               USBD_AUDIO_REQ_SET_CUR
*
*               unit_id         Feature Unit ID.
*
*               log_ch_nbr      Logical channel number (see Note #2).
*
*               p_treble        Pointer to the Treble value to set or get.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) Treble Control values range allowed for Feature Unit is:
*
*                   (a) From +31.75 dB (0x7F) down to  32.00 dB (0x80) for CUR, MIN, and MAX attributes.
*
*                   (b) From 0.25 dB (0x01) to +31.75 dB (0x7F) for RES attribute.
*
*               (2) log_ch_nbr allows you to get or set the treble for a specific channel. When
*                   log_ch_nbr is 0, you get or set the treble for all channels. Indeed, the logical
*                   channel #0 represents the master channel and encompasses all channels.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_TrebleManage (USBD_AUDIO_DRV  *p_audio_drv,
                                                        CPU_INT08U       req,
                                                        CPU_INT08U       unit_id,
                                                        CPU_INT08U       log_ch_nbr,
                                                        CPU_INT08U      *p_treble)
{
    CPU_BOOLEAN  req_ok = DEF_FAIL;


    (void)p_audio_drv;

    switch (req) {
       case USBD_AUDIO_REQ_GET_CUR:
           *p_treble = USBD_Audio_FU_TrebleTbl[unit_id - 1u][log_ch_nbr];
            req_ok   = DEF_OK;
            break;

       case USBD_AUDIO_REQ_GET_MIN:
           *p_treble = 0x80u;
            req_ok   = DEF_OK;
            break;

       case USBD_AUDIO_REQ_GET_MAX:
           *p_treble = 0x7Fu;
            req_ok   = DEF_OK;
            break;

       case USBD_AUDIO_REQ_GET_RES:
           *p_treble = 0x01u;
            req_ok   = DEF_OK;
            break;

       case USBD_AUDIO_REQ_SET_CUR:
            USBD_Audio_FU_TrebleTbl[unit_id - 1u][log_ch_nbr] = *p_treble;
            req_ok                                            =  DEF_OK;
            break;

       default:
            break;
    }

    return (req_ok);
}


/*
*********************************************************************************************************
*                            USBD_Audio_DrvCtrlFU_GraphicEqualizerManage()
*
* Description : Get or set graphic equalizer for one or all logical channels inside a cluster.
*
* Argument(s) : p_audio_drv         Pointer to audio driver structure.
*
*               req                 Graphic Equalizer request:
*
*                                   USBD_AUDIO_REQ_GET_CUR
*                                   USBD_AUDIO_REQ_GET_RES
*                                   USBD_AUDIO_REQ_GET_MIN
*                                   USBD_AUDIO_REQ_GET_MAX
*                                   USBD_AUDIO_REQ_SET_CUR
*
*               unit_id             Feature Unit ID.
*
*               log_ch_nbr          Logical channel number.
*
*               nbr_bands_present   Number of band presents. Used only with SET_CUR request.
*
*               p_bm_bands_present  Pointer to bitmap indicating a new setting for one or several
*                                   frequency band(s). Used only with SET_CUR request.
*
*               p_buf               Pointer to the request value to set or get.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) Graphic Equalizer Control values range allowed for Feature Unit is:
*
*                   (a) From +31.75 dB (0x7F) down to  32.00 dB (0x80) for CUR, MIN, and MAX attributes.
*
*                   (b) From 0.25 dB (0x01) to +31.75 dB (0x7F) for RES attribute.
*
*               (2) log_ch_nbr allows you to get or set the graphic equalizer for a specific channel.
*                   When log_ch_nbr is 0, you get or set the graphic equalizer for all channels.
*                   Indeed, the logical channel #0 represents the master channel and encompasses all
*                   channels.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_GraphicEqualizerManage (USBD_AUDIO_DRV  *p_audio_drv,
                                                                  CPU_INT08U       req,
                                                                  CPU_INT08U       unit_id,
                                                                  CPU_INT08U       log_ch_nbr,
                                                                  CPU_INT08U       nbr_bands_present,
                                                                  CPU_INT32U      *p_bm_bands_present,
                                                                  CPU_INT08U      *p_buf)
{
    CPU_BOOLEAN  req_ok = DEF_FAIL;
    CPU_INT32U   bm_bands_present;
    CPU_INT08U   bit_position;
    CPU_INT08U   i;


    (void)p_audio_drv;

    switch (req) {
        case USBD_AUDIO_REQ_GET_CUR:
             DEF_BIT_SET(*p_bm_bands_present, USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_29);
             p_buf[0u] = USBD_Audio_FU_GraphicEqualizerTbl[unit_id - 1u][log_ch_nbr][USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_29_IX];
             req_ok    = DEF_OK;
             break;

        case USBD_AUDIO_REQ_GET_MIN:
             DEF_BIT_SET(*p_bm_bands_present, USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_29);
             p_buf[0u] = 0x80u;                                 /* 32.00 dB (see Note #1a).                            */
             req_ok    = DEF_OK;
             break;

        case USBD_AUDIO_REQ_GET_MAX:
             DEF_BIT_SET(*p_bm_bands_present, USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_29);
             p_buf[0u] = 0x7Fu;                                 /* +31.75 dB (see Note #1a).                            */
             req_ok    = DEF_OK;
             break;

        case USBD_AUDIO_REQ_GET_RES:
             DEF_BIT_SET(*p_bm_bands_present, USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_29);
             p_buf[0u] = 0x01u;                                 /* Resolution of 0.25 dB (see Note #1b).                */
             req_ok    = DEF_OK;
             break;

        case USBD_AUDIO_REQ_SET_CUR:
             bm_bands_present = *p_bm_bands_present;
             for (i = 0; i < nbr_bands_present; i++) {
                 bit_position                                                   = CPU_CntTrailZeros32(bm_bands_present);
                 USBD_Audio_FU_GraphicEqualizerTbl[unit_id - 1u][log_ch_nbr][i] = p_buf[i];
                 DEF_BIT_CLR(bm_bands_present, DEF_BIT(bit_position));
             }
             req_ok = DEF_OK;
             break;

        default:
             break;
    }

    return (req_ok);
}


/*
*********************************************************************************************************
*                                 USBD_Audio_DrvCtrlFU_AutoGainManage()
*
* Description : Get or set auto gain state for one or all logical channels inside a cluster.
*
* Argument(s) : p_audio_drv     Pointer to audio driver structure.
*
*               unit_id         Feature Unit ID.
*
*               log_ch_nbr      Logical channel number.
*
*               set_en          Flag indicating to get or set the gain.
*
*               p_auto_gain     Pointer to the auto gain value to get or set.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) log_ch_nbr allows you to get the auto gain state for a specific channel. When
*                   log_ch_nbr is 0, you get the auto gain state for all channels. Indeed, the logical
*                   channel #0 represents the master channel and encompasses all channels.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_AutoGainManage (USBD_AUDIO_DRV  *p_audio_drv,
                                                          CPU_INT08U       unit_id,
                                                          CPU_INT08U       log_ch_nbr,
                                                          CPU_BOOLEAN      set_en,
                                                          CPU_BOOLEAN     *p_auto_gain)
{
    CPU_BOOLEAN  req_ok = DEF_FAIL;


    (void)p_audio_drv;

    if (set_en == DEF_FALSE) {
       *p_auto_gain = USBD_Audio_FU_AutoGainTbl[unit_id - 1u][log_ch_nbr];
        req_ok      = DEF_OK;
    } else {
        USBD_Audio_FU_AutoGainTbl[unit_id - 1u][log_ch_nbr] = *p_auto_gain;
        req_ok                                              =  DEF_OK;
    }

    return (req_ok);
}


/*
*********************************************************************************************************
*                                  USBD_Audio_DrvCtrlFU_DlyManage()
*
* Description : Get or set delay for one or all logical channels inside a cluster.
*
* Argument(s) : p_audio_drv     Pointer to audio driver structure.
*
*               req             Delay request:
*
*                               USBD_AUDIO_REQ_GET_CUR
*                               USBD_AUDIO_REQ_GET_RES
*                               USBD_AUDIO_REQ_GET_MIN
*                               USBD_AUDIO_REQ_GET_MAX
*                               USBD_AUDIO_REQ_SET_CUR
*
*               unit_id         Feature Unit ID.
*
*               log_ch_nbr      Logical channel number (see Note #3).
*
*               p_dly           Pointer to the Delay request value to set or get.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) Delay Control values range allowed for Feature Unit is:
*
*                   (a) From 0 (0x0000) to 1023.9844ms (0xFFFF) for CUR, MIN, MAX and RES attributes.
*
*               (2) 'p_dly' uses a little endian memory organization. Hence, you should use uC/LIB macros
*                   MEM_VAL_GET_INT16U_LITTLE() and MEM_VAL_SET_INT16U_LITTLE() when reading or writing
*                   the delay from/to it. This will ensure data is accessed correctly regarding your
*                   CPU endianness.
*
*               (3) log_ch_nbr allows you to get or set the delay for a specific channel. When log_ch_nbr
*                   is 0, you get or set the delay for all channels. Indeed, the logical channel #0
*                   represents the master channel and encompasses all channels.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_DlyManage (USBD_AUDIO_DRV  *p_audio_drv,
                                                     CPU_INT08U       req,
                                                     CPU_INT08U       unit_id,
                                                     CPU_INT08U       log_ch_nbr,
                                                     CPU_INT16U      *p_dly)
{
    CPU_BOOLEAN  req_ok = DEF_FAIL;


    (void)p_audio_drv;

    switch (req) {
          case USBD_AUDIO_REQ_GET_CUR:
              *p_dly  = USBD_Audio_FU_DlyTbl[unit_id - 1u][log_ch_nbr];
               req_ok = DEF_OK;
               break;

          case USBD_AUDIO_REQ_GET_MIN:
              *p_dly  = 0x0000u;
               req_ok = DEF_OK;
               break;

          case USBD_AUDIO_REQ_GET_MAX:
              *p_dly  = 0xFFFFu;
               req_ok = DEF_OK;
               break;

          case USBD_AUDIO_REQ_GET_RES:
              *p_dly  = 0x0001u;
               req_ok = DEF_OK;
               break;

          case USBD_AUDIO_REQ_SET_CUR:
               USBD_Audio_FU_DlyTbl[unit_id - 1u][log_ch_nbr] = *p_dly;
               req_ok                                         =  DEF_OK;
               break;

          default:
               break;
   }

    return (req_ok);
}


/*
*********************************************************************************************************
*                                USBD_Audio_DrvCtrlFU_BassBoostManage()
*
* Description : Get or set bass boost state for one or all logical channels inside a cluster.
*
* Argument(s) : p_audio_drv     Pointer to audio driver structure.
*
*               unit_id         Feature Unit ID.
*
*               log_ch_nbr      Logical channel number.
*
*               set_en          Flag indicating to get or set the bass boost.
*
*               p_bass_boost    Pointer to the bass boost value to get or set.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) log_ch_nbr allows you to get the bass boost state for a specific channel. When
*                   log_ch_nbr is 0, you get the bass boost state for all channels. Indeed, the
*                   logical channel #0 represents the master channel and encompasses all channels.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_BassBoostManage (USBD_AUDIO_DRV  *p_audio_drv,
                                                           CPU_INT08U       unit_id,
                                                           CPU_INT08U       log_ch_nbr,
                                                           CPU_BOOLEAN      set_en,
                                                           CPU_BOOLEAN     *p_bass_boost)
{
    CPU_BOOLEAN  req_ok = DEF_FAIL;


    (void)p_audio_drv;

    if (set_en == DEF_FALSE) {
       *p_bass_boost = USBD_Audio_FU_BassBoostTbl[unit_id - 1u][log_ch_nbr];
        req_ok       = DEF_OK;
    } else {
        USBD_Audio_FU_BassBoostTbl[unit_id - 1u][log_ch_nbr] = *p_bass_boost;
        req_ok                                               =  DEF_OK;
    }

    return (req_ok);
}


/*
*********************************************************************************************************
*                                 USBD_Audio_DrvCtrlFU_LoudnessManage()
*
* Description : Get or set loudness state for one or all logical channels inside a cluster.
*
* Argument(s) : p_audio_drv     Pointer to audio driver structure.
*
*               unit_id         Feature Unit ID.
*
*               log_ch_nbr      Logical channel number.
*
*               set_en          Flag indicating to get or set the loudness.
*
*               p_loudness      Pointer to the loudness value to get or set.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) log_ch_nbr allows you to get the loudness state for a specific channel. When
*                   log_ch_nbr is 0, you get the loudness state for all channels. Indeed, the logical
*                   channel #0 represents the master channel and encompasses all channels.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_LoudnessManage (USBD_AUDIO_DRV  *p_audio_drv,
                                                          CPU_INT08U       unit_id,
                                                          CPU_INT08U       log_ch_nbr,
                                                          CPU_BOOLEAN      set_en,
                                                          CPU_BOOLEAN     *p_loudness)
{
    CPU_BOOLEAN  req_ok = DEF_FAIL;


    (void)p_audio_drv;

    if (set_en == DEF_FALSE) {
       *p_loudness = USBD_Audio_FU_LoudnessTbl[unit_id - 1u][log_ch_nbr];
        req_ok     = DEF_OK;
    } else {
        USBD_Audio_FU_LoudnessTbl[unit_id - 1u][log_ch_nbr] = *p_loudness;
        req_ok                                              =  DEF_OK;
    }

    return (req_ok);
}


/*
*********************************************************************************************************
*                                  USBD_Audio_DrvCtrlMU_CtrlManage()
*
* Description : Get or set mix status for a certain logical input and output channels couple.
*
* Argument(s) : p_audio_drv     Pointer to audio driver structure.
*
*               req             Mixer status request:
*
*                               USBD_AUDIO_REQ_GET_CUR
*                               USBD_AUDIO_REQ_GET_RES
*                               USBD_AUDIO_REQ_GET_MIN
*                               USBD_AUDIO_REQ_GET_MAX
*                               USBD_AUDIO_REQ_SET_CUR
*
*               unit_id         Mixer Unit ID.
*
*               log_in_ch_nbr   Logical input channel number.
*
*               log_out_ch_nbr  Logical output channel number.
*
*               p_ctrl          Pointer to the mixer control request value to get or set.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) Mixer Control values range allowed for the Mixer Unit is:
*
*                   (a) From +127.9961 dB (0x7FFF) down to -127.9961 dB (0x8001) for CUR, MIN, and MAX
*                       attributes.
*
*                   (b) From 1/256 dB (0x0001) to +127.9961 dB (0x7FFF) for RES attribute.
*
*               (2) 'p_ctrl' uses a little endian memory organization. Hence, you should use uC/LIB
*                   macros MEM_VAL_GET_INT16U_LITTLE() and MEM_VAL_SET_INT16U_LITTLE() when reading or
*                   writing the mixing control value from/to it. This will ensure data is accessed
*                   correctly regarding your CPU endianness.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlMU_CtrlManage (USBD_AUDIO_DRV  *p_audio_drv,
                                                      CPU_INT08U       req,
                                                      CPU_INT08U       unit_id,
                                                      CPU_INT08U       log_in_ch_nbr,
                                                      CPU_INT08U       log_out_ch_nbr,
                                                      CPU_INT16U      *p_ctrl)
{
    CPU_BOOLEAN  req_ok = DEF_FAIL;


    (void)p_audio_drv;

    switch (req) {
        case USBD_AUDIO_REQ_GET_CUR:
            *p_ctrl = USBD_Audio_MU_Tbl[unit_id - 1u][log_in_ch_nbr][log_out_ch_nbr];
             req_ok = DEF_OK;
             break;

        case USBD_AUDIO_REQ_GET_MIN:
            *p_ctrl = 0x8001u;                                  /* -127.9961 dB (see Note #1a).                         */
             req_ok = DEF_OK;
             break;

        case USBD_AUDIO_REQ_GET_MAX:
            *p_ctrl = 0x7FFFu;                                  /* +127.9961 dB (see Note #1a).                         */
             req_ok = DEF_OK;
             break;

        case USBD_AUDIO_REQ_GET_RES:
            *p_ctrl = 0x0001u;                                  /* Resolution of 1/256 dB (see Note #1b).               */
             req_ok = DEF_OK;
             break;

        case USBD_AUDIO_REQ_SET_CUR:
             USBD_Audio_MU_Tbl[unit_id - 1u][log_in_ch_nbr][log_out_ch_nbr] = *p_ctrl;
             req_ok                                                         =  DEF_OK;
             break;

        default:
             break;
    }

    return (req_ok);
}


/*
*********************************************************************************************************
*                                  USBD_Audio_DrvCtrlSU_InPinManage()
*
* Description : Get or set the Input Pin of a particular Selector Unit.
*
* Argument(s) : p_audio_drv     Pointer to audio driver structure.
*
*               unit_id         Selector Unit ID.
*
*               set_en          Flag indicating to get or set the mute.
*
*               p_in_pin_nbr    Pointer to the input number to get or set.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlSU_InPinManage (USBD_AUDIO_DRV  *p_audio_drv,
                                                       CPU_INT08U       unit_id,
                                                       CPU_BOOLEAN      set_en,
                                                       CPU_INT08U      *p_in_pin_nbr)
{
    CPU_BOOLEAN  req_ok = DEF_FAIL;


    (void)p_audio_drv;

    if (set_en == DEF_FALSE) {
       *p_in_pin_nbr = USBD_Audio_SU_Tbl[unit_id - 1u];
        req_ok       = DEF_OK;
    } else {
        USBD_Audio_SU_Tbl[unit_id - 1u] = *p_in_pin_nbr;
        req_ok                          =  DEF_OK;
    }

    return (req_ok);
}


/*
*********************************************************************************************************
*                                 USBD_Audio_DrvAS_SamplingFreqManage()
*
* Description : Get or set current sampling frequency for a particular Terminal (i.e. endpoint).
*
* Argument(s) : p_audio_drv         Pointer to audio driver structure.
*
*               terminal_id_link    AudioStreaming terminal link.
*
*               set_en              Flag indicating to get or set the sampling frequency.
*
*               p_sampling_freq     Pointer to the sampling frequency value to get or set.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) 'p_cur_sampling_freq' uses a little endian memory organization. Hence, you should use
*                   uC/LIB macros MEM_VAL_GET_INT32U_LITTLE() and MEM_VAL_SET_INT32U_LITTLE() when
*                   reading or writing the sampling frequency from/to it. This will ensure data is
*                   accessed correctly regarding your CPU endianness.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvAS_SamplingFreqManage (USBD_AUDIO_DRV  *p_audio_drv,
                                                          CPU_INT08U       terminal_id_link,
                                                          CPU_BOOLEAN      set_en,
                                                          CPU_INT32U      *p_sampling_freq)
{
    CPU_BOOLEAN  req_ok = DEF_FAIL;


    (void)p_audio_drv;

    if (set_en == DEF_FALSE) {
       *p_sampling_freq = USBD_Audio_AS_EP_SamplingFreqTbl[terminal_id_link - 1u];
        req_ok          = DEF_OK;
    } else {
        USBD_Audio_AS_EP_SamplingFreqTbl[terminal_id_link - 1u] = *p_sampling_freq;
        req_ok                                                  =  DEF_OK;
    }

    return (req_ok);
}


/*
*********************************************************************************************************
*                                    USBD_Audio_DrvAS_PitchManage()
*
* Description : Get or set pitch state for a particular Terminal (i.e. endpoint).
*
* Argument(s) : p_audio_drv         Pointer to audio driver structure.
*
*               terminal_id_link    AudioStreaming terminal link.
*
*               set_en              Flag indicating to get or set the pitch.
*
*               p_pitch             Pointer to the pitch state to get or set.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvAS_PitchManage (USBD_AUDIO_DRV  *p_audio_drv,
                                                   CPU_INT08U       terminal_id_link,
                                                   CPU_BOOLEAN      set_en,
                                                   CPU_BOOLEAN     *p_pitch)
{
    CPU_BOOLEAN  req_ok = DEF_FAIL;


    (void)p_audio_drv;

    if (set_en == DEF_FALSE) {
       *p_pitch = USBD_Audio_AS_EP_PitchTbl[terminal_id_link - 1u];
        req_ok  = DEF_OK;
    } else {
        USBD_Audio_AS_EP_PitchTbl[terminal_id_link - 1u] = *p_pitch;
        req_ok                                           =  DEF_OK;
    }

    return (req_ok);
}


/*
*********************************************************************************************************
*                                      USBD_Audio_DrvStreamStart()
*
* Description : Start record or playback stream.
*
* Argument(s) : p_audio_drv         Pointer to audio driver structure.
*
*               as_handle           AudioStreaming handle.
*
*               terminal_id_link    AudioStreaming terminal link.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) A general purpose OS (Windows, Mac OS, Linux) opens a record stream by sending a
*                   SET_INTERFACE request to select the operational interface of an AudioStreaming
*                   interface. The SET_INTERFACE request is always followed by a SET_CUR request to
*                   configure the sampling frequency. The first record DMA transfer must be started ONLY
*                   when the sampling frequency has been configured. The Audio class ensures this
*                   sequence.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvStreamStart (USBD_AUDIO_DRV        *p_audio_drv,
                                                USBD_AUDIO_AS_HANDLE   as_handle,
                                                CPU_INT08U             terminal_id_link)
{
    (void)p_audio_drv;
    (void)as_handle;
    (void)terminal_id_link;

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                      USBD_Audio_DrvStreamStop()
*
* Description : Stop record or playback stream.
*
* Argument(s) : p_audio_drv         Pointer to audio driver structure.
*
*               terminal_id_link    AudioStreaming terminal link.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvStreamStop (USBD_AUDIO_DRV  *p_audio_drv,
                                               CPU_INT08U       terminal_id_link)
{
    (void)p_audio_drv;
    (void)terminal_id_link;

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                    USBD_Audio_DrvStreamRecordRx()
*
* Description : Get a ready record buffer from codec.
*
* Argument(s) : p_audio_drv         Pointer to audio driver structure.
*
*               terminal_id_link    Terminal ID associated to this stream.
*
*               p_buf               Pointer to record buffer.
*
*               p_buf_len           Pointer to buffer length in bytes.
*
*               p_err               Pointer to the variable that will receive the return error code from this function:
*
*                                   USBD_ERR_NONE   Buffer successfully retrieved.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_Audio_DrvStreamRecordRx (USBD_AUDIO_DRV  *p_audio_drv,
                                            CPU_INT08U       terminal_id_link,
                                            void            *p_buf,
                                            CPU_INT16U      *p_buf_len,
                                            USBD_ERR        *p_err)
{
    (void)p_audio_drv;
    (void)terminal_id_link;
    (void)p_buf;
    (void)p_buf_len;

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                   USBD_Audio_DrvStreamPlaybackTx()
*
* Description : Provide a ready playback buffer to codec.
*
* Argument(s) : p_audio_drv         Pointer to audio driver structure.
*
*               terminal_id_link    Terminal ID associated to this stream.
*
*               p_buf               Pointer to ready playback buffer.
*
*               buf_len             Buffer length in bytes.
*
*               p_err               Pointer to the variable that will receive the return error code from this function:
*
*                                   USBD_ERR_NONE   Buffer successfully retrieved.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_Audio_DrvStreamPlaybackTx (USBD_AUDIO_DRV  *p_audio_drv,
                                              CPU_INT08U       terminal_id_link,
                                              void            *p_buf,
                                              CPU_INT16U       buf_len,
                                              USBD_ERR        *p_err)
{
    (void)p_audio_drv;
    (void)terminal_id_link;
    (void)p_buf;
    (void)buf_len;

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/
