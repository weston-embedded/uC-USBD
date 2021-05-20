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
*                          USB DEVICE AUDIO CLASS APPLICATION INITIALIZATION
*
*                                              TEMPLATE
*
* Filename : app_usbd_audio.c
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <app_usbd.h>

#if (APP_CFG_USBD_EN       == DEF_ENABLED) && \
    (APP_CFG_USBD_AUDIO_EN == DEF_ENABLED)
#include  "usbd_audio_drv_simulation.h"
#include  <Class/Audio/usbd_audio.h>
#include  <usbd_audio_dev_cfg.h>


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  APP_USBD_AUDIO_CFG_TASKS_Q_LEN                    20u  /* Queue len for playback & record tasks.               */


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_STAT_EN == DEF_ENABLED)
static  USBD_AUDIO_STAT  *App_MicStatPtr;
#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
static  USBD_AUDIO_STAT  *App_SpeakerStatPtr;
#endif
#endif


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  App_USBD_Audio_Conn   (CPU_INT08U            dev_nbr,
                                     CPU_INT08U            cfg_nbr,
                                     CPU_INT08U            terminal_id,
                                     USBD_AUDIO_AS_HANDLE  as_handle);

static  void  App_USBD_Audio_Disconn(CPU_INT08U            dev_nbr,
                                     CPU_INT08U            cfg_nbr,
                                     CPU_INT08U            terminal_id,
                                     USBD_AUDIO_AS_HANDLE  as_handle);


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/

const  USBD_AUDIO_EVENT_FNCTS  App_USBD_Audio_EventFncts = {
    App_USBD_Audio_Conn,
    App_USBD_Audio_Disconn
};


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                        App_USBD_Audio_Init()
*
* Description : Initialize USB device audio class.
*
* Argument(s) : dev_nbr,    Device number.
*
*               cfg_hs,     High speed configuration number.
*
*               cfg_fs,     Full speed configuration number.
*
* Return(s)   : DEF_OK,     if the audio interface was added.
*               DEF_FAIL,   if the audio interface could not be added.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  App_USBD_Audio_Init (CPU_INT08U  dev_nbr,
                                  CPU_INT08U  cfg_hs,
                                  CPU_INT08U  cfg_fs)
{
    USBD_ERR                 err;
    CPU_INT08U               audio_nbr;
    USBD_AUDIO_AS_IF_HANDLE  mic_record_as_if_handle;
#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
    USBD_AUDIO_AS_IF_HANDLE  speaker_playback_as_if_handle;
#endif


    APP_TRACE_DBG(("        Initializing Audio class ... \r\n"));
                                                                /* Init Audio class.                                    */
    USBD_Audio_Init(APP_USBD_AUDIO_CFG_TASKS_Q_LEN, &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not initialize Audio class w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

                                                                /* Create an audio class instance.                      */
    audio_nbr = USBD_Audio_Add(USBD_AUDIO_DEV_CFG_NBR_ENTITY,
                              &USBD_Audio_DrvCommonAPI_Simulation,
                              &App_USBD_Audio_EventFncts,
                              &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not add a new Audio class instance w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

    if (cfg_hs != USBD_CFG_NBR_NONE) {
                                                                /* --------------- ADD HS CONFIGURATION --------------- */
        USBD_Audio_CfgAdd(audio_nbr,                            /* Add audio class to HS dflt cfg.                      */
                          dev_nbr,
                          cfg_hs,
                         &err);
        if (err != USBD_ERR_NONE) {
            APP_TRACE_DBG(("        ... could not add audio instance #%d to HS configuration w/err = %d\r\n\r\n", audio_nbr, err));
            return (DEF_FAIL);
        }
    }

    if (cfg_fs != USBD_CFG_NBR_NONE) {
                                                                /* --------------- ADD FS CONFIGURATION --------------- */
        USBD_Audio_CfgAdd(audio_nbr,                            /* Add audio class to FS dflt cfg.                      */
                          dev_nbr,
                          cfg_fs,
                         &err);
        if (err != USBD_ERR_NONE) {
            APP_TRACE_DBG(("        ... could not add audio instance #%d to FS configuration w/err = %d\r\n\r\n", audio_nbr, err));
            return (DEF_FAIL);
        }
    }
                                                                /* ----------- BUILD AUDIO FUNCTION TOPOLOGY ---------- */
                                                                /* Add terminals and units.                             */
    Mic_IT_ID = USBD_Audio_IT_Add(audio_nbr,
                                 &USBD_IT_MIC_Cfg,
                                 &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not add Input Terminal w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

    Mic_OT_USB_IN_ID = USBD_Audio_OT_Add(                            audio_nbr,
                                                                    &USBD_OT_USB_IN_Cfg,
                                         (USBD_AUDIO_DRV_AC_OT_API *)DEF_NULL,
                                                                    &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not add Output Terminal w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

    Mic_FU_ID = USBD_Audio_FU_Add(audio_nbr,
                                 &USBD_FU_MIC_Cfg,
                                 &USBD_Audio_DrvFU_API_Simulation,
                                 &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not add Feature Unit w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
    Speaker_IT_USB_OUT_ID = USBD_Audio_IT_Add(audio_nbr,
                                             &USBD_IT_USB_OUT_Cfg,
                                             &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not add Input Terminal w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

    Speaker_OT_ID = USBD_Audio_OT_Add(audio_nbr,
                                     &USBD_OT_SPEAKER_Cfg,
                                      DEF_NULL,
                                     &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not add Output Terminal w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

    Speaker_FU_ID = USBD_Audio_FU_Add(audio_nbr,
                                     &USBD_FU_SPEAKER_Cfg,
                                     &USBD_Audio_DrvFU_API_Simulation,
                                     &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not add Feature Unit w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }
#endif
                                                                /* Bind terminals and units.                            */
    USBD_Audio_IT_Assoc(audio_nbr,
                        Mic_IT_ID,
                        USBD_AUDIO_TERMINAL_NO_ASSOCIATION,
                       &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not bind Input Terminal w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

    USBD_Audio_OT_Assoc(audio_nbr,
                        Mic_OT_USB_IN_ID,
                        Mic_FU_ID,
                        USBD_AUDIO_TERMINAL_NO_ASSOCIATION,
                       &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not bind Output Terminal w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

    USBD_Audio_FU_Assoc(audio_nbr,
                        Mic_FU_ID,
                        Mic_IT_ID,
                       &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not bind Feature Unit w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
    USBD_Audio_IT_Assoc(audio_nbr,
                        Speaker_IT_USB_OUT_ID,
                        USBD_AUDIO_TERMINAL_NO_ASSOCIATION,
                       &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not bind Input Terminal w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

    USBD_Audio_OT_Assoc(audio_nbr,
                        Speaker_OT_ID,
                        Speaker_FU_ID,
                        USBD_AUDIO_TERMINAL_NO_ASSOCIATION,
                       &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not bind Output Terminal w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

    USBD_Audio_FU_Assoc(audio_nbr,
                        Speaker_FU_ID,
                        Speaker_IT_USB_OUT_ID,
                       &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not bind Feature Unit w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }
#endif
                                                                /* ----------- CONFIGURE AUDIO STREAMING IF ----------- */
#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
   speaker_playback_as_if_handle =  USBD_Audio_AS_IF_Cfg(&USBD_SpeakerStreamCfg,
                                                         &USBD_AS_IF1_SpeakerCfg,
                                                         &USBD_Audio_DrvAS_API_Simulation,
                                                          DEF_NULL,
                                                          Speaker_IT_USB_OUT_ID,
                                                          DEF_NULL,
                                                         &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not configure speaker stream w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }
#endif

   mic_record_as_if_handle =  USBD_Audio_AS_IF_Cfg(&USBD_MicStreamCfg,
                                                   &USBD_AS_IF2_MicCfg,
                                                   &USBD_Audio_DrvAS_API_Simulation,
                                                    DEF_NULL,
                                                    Mic_OT_USB_IN_ID,
                                                    DEF_NULL,
                                                   &err);
    if (err != USBD_ERR_NONE) {
        APP_TRACE_DBG(("        ... could not configure record stream w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

    if (cfg_hs != USBD_CFG_NBR_NONE) {
                                                                /* -------------- ADD AUDIO STREAMING IF -------------- */
#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)

        USBD_Audio_AS_IF_Add(audio_nbr,
                             cfg_hs,
                             speaker_playback_as_if_handle,
                            &USBD_AS_IF1_SpeakerCfg,
                            "Speaker AudioStreaming IF",
                            &err);
        if (err != USBD_ERR_NONE) {
            APP_TRACE_DBG(("        ... could not add AudioStreaming Interface to HS configuration w/err = %d\r\n\r\n", err));
            return (DEF_FAIL);
        }
#endif

        USBD_Audio_AS_IF_Add(audio_nbr,
                             cfg_hs,
                             mic_record_as_if_handle,
                            &USBD_AS_IF2_MicCfg,
                            "Microphone AudioStreaming IF",
                            &err);
        if (err != USBD_ERR_NONE) {
            APP_TRACE_DBG(("        ... could not add AudioStreaming Interface to HS configuration w/err = %d\r\n\r\n", err));
            return (DEF_FAIL);
        }
    }

    if (cfg_fs != USBD_CFG_NBR_NONE) {
                                                                /* -------------- ADD AUDIO STREAMING IF -------------- */
#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)

        USBD_Audio_AS_IF_Add(audio_nbr,
                             cfg_fs,
                             speaker_playback_as_if_handle,
                            &USBD_AS_IF1_SpeakerCfg,
                            "Speaker AudioStreaming IF",
                            &err);
        if (err != USBD_ERR_NONE) {
            APP_TRACE_DBG(("        ... could not add AudioStreaming Interface to FS configuration w/err = %d\r\n\r\n", err));
            return (DEF_FAIL);
        }
#endif

        USBD_Audio_AS_IF_Add(audio_nbr,
                             cfg_fs,
                             mic_record_as_if_handle,
                            &USBD_AS_IF2_MicCfg,
                            "Microphone AudioStreaming IF",
                            &err);
        if (err != USBD_ERR_NONE) {
            APP_TRACE_DBG(("        ... could not add AudioStreaming Interface to FS configuration w/err = %d\r\n\r\n", err));
            return (DEF_FAIL);
        }
    }

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                        App_USBD_Audio_Conn()
*
* Description : Inform the application about audio device connection to host. The host has completed the
*               device's enumeration and the device is ready for communication.
*
* Argument(s) : dev_nbr        Device number.
*
*               cfg_nbr        Configuration number.
*
*               terminal_id    Terminal ID.
*
*               as_handle      AudioStreaming interface handle.
*
* Return(s)   : none.
*
* Note(s)     : (1) When calling this function, you may call the function USBD_Audio_AS_IF_StatGet() to
*                   get audio statistics for a given AudioStreaming interface.
*                   The audio statistics structure can be consulted during debug operations in a watch
*                   window for instance. This structure collects long-term statistics for a given
*                   AudioStreaming interface, that is each time the corresponding stream is open
*                   and used by the host.
*********************************************************************************************************
*/

static  void  App_USBD_Audio_Conn (CPU_INT08U            dev_nbr,
                                   CPU_INT08U            cfg_nbr,
                                   CPU_INT08U            terminal_id,
                                   USBD_AUDIO_AS_HANDLE  as_handle)
{
    (void)dev_nbr;
    (void)cfg_nbr;

#if (USBD_AUDIO_CFG_STAT_EN == DEF_ENABLED)
    if (terminal_id == Mic_OT_USB_IN_ID) {
        App_MicStatPtr = USBD_Audio_AS_IF_StatGet(as_handle);
#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
    } else if (terminal_id == Speaker_IT_USB_OUT_ID) {
        App_SpeakerStatPtr = USBD_Audio_AS_IF_StatGet(as_handle);
#endif
    }
#else
    (void)terminal_id;
    (void)as_handle;
#endif
}


/*
*********************************************************************************************************
*                                      App_USBD_Audio_Disconn()
*
* Description : Inform the application about audio device configuration being inactive because of
*               device disconnection from host or host selecting another device configuration.
*
* Argument(s) : dev_nbr        Device number.
*
*               cfg_nbr        Configuration number.
*
*               terminal_id    Terminal ID.
*
*               as_handle      AudioStreaming interface handle.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  App_USBD_Audio_Disconn (CPU_INT08U            dev_nbr,
                                      CPU_INT08U            cfg_nbr,
                                      CPU_INT08U            terminal_id,
                                      USBD_AUDIO_AS_HANDLE  as_handle)
{
    (void)dev_nbr;
    (void)cfg_nbr;
    (void)terminal_id;
    (void)as_handle;
}

#endif
