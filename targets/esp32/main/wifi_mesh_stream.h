#ifndef _WIFI_MESH_STREAM_H_
#define _WIFI_MESH_STREAM_H_

#include "audio_error.h"
#include "audio_element.h"
#include "esp_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MESH_STREAM_STATE_NONE,
    MESH_STREAM_STATE_CONNECTED,
} mesh_stream_status_t;

/**
 * @brief   MESH Stream massage configuration
 */
typedef struct mesh_stream_event_msg {
    void                          *source;          /*!< Element handle */
    void                          *data;            /*!< Data of input/output */
    int                           data_len;         /*!< Data length of input/output */
    //esp_transport_handle_t        sock_fd;          /*!< handle of socket*/
} mesh_stream_event_msg_t;

typedef esp_err_t (*mesh_stream_event_handle_cb)(mesh_stream_event_msg_t *msg, mesh_stream_status_t state, void *event_ctx);

/**
 * @brief   mesh Stream configuration, if any entry is zero then the configuration will be set to default values
 */
typedef struct {
    audio_stream_type_t         type;               /*!< Type of stream */
    int                         timeout_ms;         /*!< time timeout for read/write*/
    int                         port;               /*!< MESH port> */
    char                        *host;              /*!< MESH host> */
    int                         task_stack;         /*!< Task stack size */
    int                         task_core;          /*!< Task running in core (0 or 1) */
    int                         task_prio;          /*!< Task priority (based on freeRTOS priority) */
    bool                        ext_stack;          /*!< Allocate stack on extern ram */
    mesh_stream_event_handle_cb  event_handler;     /*!< MESH stream event callback*/
    void                        *event_ctx;         /*!< User context*/
} mesh_stream_cfg_t;

/**
* @brief    MESH stream parameters
*/
#define MESH_STREAM_DEFAULT_PORT             (8080)

#define MESH_STREAM_TASK_STACK               (3072)
#define MESH_STREAM_BUF_SIZE                 (1024)
#define MESH_STREAM_TASK_PRIO                (5)
#define MESH_STREAM_TASK_CORE                (0)

#define MESH_SERVER_DEFAULT_RESPONSE_LENGTH  (512)

#define MESH_STREAM_CFG_DEFAULT() {              \
    .type          = AUDIO_STREAM_READER,       \
    .timeout_ms    = 30 *1000,                  \
    .port          = MESH_STREAM_DEFAULT_PORT,   \
    .host          = NULL,                      \
    .task_stack    = MESH_STREAM_TASK_STACK,     \
    .task_core     = MESH_STREAM_TASK_CORE,      \
    .task_prio     = MESH_STREAM_TASK_PRIO,      \
    .ext_stack     = true,                      \
    .event_handler = NULL,                      \
    .event_ctx     = NULL,                      \
}

typedef enum 
{
    PLAY,
    PAUSE,
    STOP,
    RESUME,
    VOL_UP,
    VOL_DOWN,

}command;

typedef enum 
{
    MP3,
    AAC,

}codec_type;

typedef struct 
{
    uint32_t    delay_val_us;
    command     cmd;
    uint32_t    set_vol;
    codec_type  codec;
}music_header;

/**
 * @brief       Initialize a MESH stream to/from an audio element 
 *              This function creates a MESH stream to/from an audio element depending on the stream type configuration (e.g., 
 *              AUDIO_STREAM_READER or AUDIO_STREAM_WRITER). The handle of the audio element is the returned.
 *
 * @param      config The configuration
 *
 * @return     The audio element handle
 */
audio_element_handle_t mesh_stream_init(mesh_stream_cfg_t *config);
esp_err_t _mesh_write(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context);
#ifdef __cplusplus
}
#endif

#endif