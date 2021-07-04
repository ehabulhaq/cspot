#include "mdf_common.h"
#include "mwifi.h"

#include "esp_log.h"
#include "esp_err.h"
#include "audio_mem.h"
#include "wifi_mesh_stream.h"

static const char *TAG = "MESH_STREAM";

typedef struct mesh_stream
{
    esp_transport_handle_t t;
    audio_stream_type_t type;
    //int                           sock;
    //int                           port;
    //char                          *host;
    bool is_open;
    int timeout_ms;
    mesh_stream_event_handle_cb hook;
    void *ctx;
} mesh_stream_t;

static esp_err_t _mesh_open(audio_element_handle_t self)
{
    return ESP_OK;
}
static esp_err_t _mesh_close(audio_element_handle_t self)
{
    return ESP_OK;
}

static esp_err_t _mesh_destroy(audio_element_handle_t self)
{
    return ESP_OK;
}
static esp_err_t _mesh_read(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context)
{
    //mdf_err_t ret = MDF_OK;
    //mwifi_data_type_t data_type = {0x0};
    //u/int8_t src_addr[MWIFI_ADDR_LEN] = {0x0};
    //mesh_stream_t *mesh = (mesh_stream_t *)audio_element_getdata(self);
    // int rlen = esp_transport_read(mesh->t, buffer, len, mesh->timeout_ms);
    // ESP_LOGD(TAG, "read len=%d, rlen=%d", len, rlen);

    //ret = mwifi_read(src_addr, &data_type, buffer, len, ticks_to_wait);
    //MDF_ERROR_CONTINUE(ret != MDF_OK, "mwifi_read, ret: %x", ret);

    // if (ret < 0)
    // {
    //     //_get_socket_error_code_reason("TCP read", mesh->sock);
    //     return ESP_FAIL;
    // }
    // else if (ret == 0)
    // {
    //     ESP_LOGI(TAG, "Get end of the file");
    // }
    // else
    // {
    //     audio_element_update_byte_pos(self, len);
    // }
    return len;
}

esp_err_t _mesh_write(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context)
{
    wifi_sta_list_t wifi_sta_list = {0x0};
    mwifi_data_type_t data_type = {0};
    mdf_err_t ret = MDF_OK;

    if ( esp_wifi_ap_get_sta_list(&wifi_sta_list) == ESP_OK)
    {
        ret = mwifi_root_write(wifi_sta_list.sta[0].mac, 1, &data_type, buffer, len, false);

    }
    //uint8_t dst_addr[MWIFI_ADDR_LEN] = wifi_sta_list.;
    
    //data_type.communicate = MWIFI_COMMUNICATE_BROADCAST;
    

    ESP_LOGD(TAG, "read len=%d, ret=%d", len, ret);
    return len;
}

static esp_err_t _mesh_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
    int r_size = audio_element_input(self, in_buffer, in_len);
    int w_size = 0;
    if (r_size > 0)
    {
        w_size = audio_element_output(self, in_buffer, r_size);
        if (w_size > 0)
        {
            audio_element_update_byte_pos(self, w_size);
        }
    }
    else
    {
        w_size = r_size;
    }
    return w_size;
}

audio_element_handle_t mesh_stream_init(mesh_stream_cfg_t *config)
{
    AUDIO_NULL_CHECK(TAG, config, return NULL);

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    audio_element_handle_t el;
    cfg.open = _mesh_open;
    cfg.close = _mesh_close;
    cfg.process = _mesh_process;
    cfg.destroy = _mesh_destroy;
    cfg.task_stack = config->task_stack;
    cfg.task_prio = config->task_prio;
    cfg.task_core = config->task_core;
    cfg.stack_in_ext = config->ext_stack;
    cfg.tag = "mesh_client";
    //if (cfg.buffer_len == 0)
    //{
        cfg.buffer_len = MESH_STREAM_BUF_SIZE;
    //}

    mesh_stream_t *mesh = audio_calloc(1, sizeof(mesh_stream_t));
    AUDIO_MEM_CHECK(TAG, mesh, return NULL);

    mesh->type = config->type;
    //mesh->port = config->port;
    //mesh->host = config->host;
    mesh->timeout_ms = config->timeout_ms;
    if (config->event_handler)
    {
        mesh->hook = config->event_handler;
        if (config->event_ctx)
        {
            mesh->ctx = config->event_ctx;
        }
    }

    if (config->type == AUDIO_STREAM_WRITER)
    {
        cfg.write = _mesh_write;
    }
    else
    {
        cfg.read = _mesh_read;
    }

    el = audio_element_init(&cfg);
    AUDIO_MEM_CHECK(TAG, el, goto _mesh_init_exit);
    audio_element_setdata(el, mesh);

    return el;
_mesh_init_exit:
    audio_free(mesh);
    return NULL;
}