#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spiffs.h"
#include <string.h>
#include <arpa/inet.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include <string>
#include <PlainConnection.h>
#include <Session.h>
#include <SpircController.h>
#include <MercuryManager.h>
#include <ZeroconfAuthenticator.h>

#include <ApResolve.h>
#include <inttypes.h>
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "ConfigJSON.h"
#include "ESPFile.h"
#include "ProtoHelper.h"
#include "Logger.h"

#include "board.h"
#include "mdf_common.h"
#include "mwifi.h"
#include "esp_netif.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "wifi_mesh_stream.h"
#include "driver/gpio.h"

// Config
#define SINK        ES8018 // INTERNAL, AC101, ES8018, PCM5102
#define QUALITY     320      // 320, 160, 96
#define DEVICE_NAME "TruScapes Speakers"
#define WIFI_HOME   1

#define PA_CTRL    (22)
#define GPIO_OUTPUT_PIN_PA_CTRL  ((1ULL<<PA_CTRL))

#include <ES9018AudioSink.h>


static const char *TAG = "cspot";
static esp_netif_t *netif_sta = NULL;

std::shared_ptr<ConfigJSON> configMan;

extern "C"
{
    void app_main(void);
}
static void cspotTask(void *pvParameters)
{
    auto zeroconfAuthenticator = std::make_shared<ZeroconfAuthenticator>();

    // Config file
    auto file = std::make_shared<ESPFile>();
    configMan = std::make_shared<ConfigJSON>("/spiffs/config.json", file);

    if(!configMan->load())
    {
      CSPOT_LOG(error, "Config error");
    }

    configMan->deviceName = DEVICE_NAME;

#if QUALITY == 320
    configMan->format = AudioFormat::OGG_VORBIS_320;
#elif QUALITY == 160
    configMan->format = AudioFormat::OGG_VORBIS_160;
#else
    configMan->format = AudioFormat::OGG_VORBIS_96;
#endif

    // Blob file
    std::string credentialsFileName = "/spiffs/authBlob.json";
    std::shared_ptr<LoginBlob> blob;
    std::string jsonData;

    bool read_status = file->readFile(credentialsFileName, jsonData);

    if(jsonData.length() > 0 && read_status)
    {
      blob = std::make_shared<LoginBlob>();
      blob->loadJson(jsonData);
    }
    else
    {
      auto authenticator = std::make_shared<ZeroconfAuthenticator>();
      blob = authenticator->listenForRequests();
      file->writeFile(credentialsFileName, blob->toJson());
    }

    auto session = std::make_unique<Session>();
    session->connectWithRandomAp();
    auto token = session->authenticate(blob);

    // Auth successful
    if (token.size() > 0)
    {
        // @TODO Actually store this token somewhere
        auto mercuryManager = std::make_shared<MercuryManager>(std::move(session));
        mercuryManager->startTask();

        auto audioSink = std::make_shared<ES9018AudioSink>();


        auto spircController = std::make_shared<SpircController>(mercuryManager, blob->username, audioSink);
        mercuryManager->reconnectedCallback = [spircController]() {
            return spircController->subscribe();
        };
        mercuryManager->handleQueue();
    }
}

void init_spiffs()
{
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
}

static mdf_err_t wifi_init()
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    MDF_ERROR_ASSERT(esp_netif_init());
    MDF_ERROR_ASSERT(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_mesh_netifs(&netif_sta, NULL));
    MDF_ERROR_ASSERT(esp_wifi_init(&cfg));
    MDF_ERROR_ASSERT(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    MDF_ERROR_ASSERT(esp_wifi_set_mode(WIFI_MODE_STA));
    MDF_ERROR_ASSERT(esp_wifi_set_ps(WIFI_PS_NONE));
    MDF_ERROR_ASSERT(esp_mesh_set_6m_rate(true));

    MDF_ERROR_ASSERT(esp_wifi_start());

    return MDF_OK;
}

/**
 * @brief All module events will be sent to this task in esp-mdf
 *
 * @Note:
 *     1. Do not block or lengthy operations in the callback function.
 *     2. Do not consume a lot of memory in the callback function.
 *        The task memory of the callback function is only 4KB.
 */
static mdf_err_t event_loop_cb(mdf_event_loop_t event, void *ctx)
{
    MDF_LOGI("event_loop_cb, event: %d", event);

    switch (event)
    {
    case MDF_EVENT_MWIFI_STARTED:
        MDF_LOGI("MESH is started");
        break;

    case MDF_EVENT_MWIFI_PARENT_CONNECTED:
        MDF_LOGI("Parent is connected on station interface");

        if (esp_mesh_is_root())
        {
            esp_netif_dhcpc_start(netif_sta);
        }
        break;

    case MDF_EVENT_MWIFI_ROUTING_TABLE_ADD:
        MDF_LOGI("Child is connected on station interface");
        update_station_list();
        break;

    case MDF_EVENT_MWIFI_ROUTING_TABLE_REMOVE:
        MDF_LOGI("Child is disconnected on station interface");
        update_station_list();
        break;

    case MDF_EVENT_MWIFI_PARENT_DISCONNECTED:
        MDF_LOGI("Parent is disconnected on station interface");
        
        break;
    case MDF_EVENT_MWIFI_ROOT_GOT_IP:
        MDF_LOGI("Root obtains the IP address. It is posted by LwIP stack automatically");
        //ESP_ERROR_CHECK(example_connect());
        xTaskCreatePinnedToCore(&cspotTask, "cspot", 8192 * 8, NULL, 5, NULL, 0);
        break;
    default:
        break;
    }

    return MDF_OK;
}

void mesh_init()
{

mwifi_init_config_t cfg = MWIFI_INIT_CONFIG_DEFAULT();

    mwifi_config_t config = {};
    strcpy((char*)config.mesh_id, "12345");
    strcpy((char*)config.mesh_password, "12345678");
#if WIFI_HOME
    strcpy((char*)config.router_ssid, "Haq's Home");
    strcpy((char*)config.router_password, "ehabulhaq");
#else
    strcpy((char*)config.router_ssid, "Tru-Scapes");
    strcpy((char*)config.router_password, "tru12345");

        //config.router_ssid = "Tru-Scapes",
        //config.router_password = "tru12345",
#endif  
    config.mesh_type = MWIFI_MESH_ROOT;
    config.channel = CONFIG_MESH_CHANNEL;

    /**
     * @brief Initialize wifi mesh.
     */
    MDF_ERROR_ASSERT(mdf_event_loop_init(event_loop_cb));
    MDF_ERROR_ASSERT(wifi_init());
    MDF_ERROR_ASSERT(mwifi_init(&cfg));
    MDF_ERROR_ASSERT(mwifi_set_config(&config));
    MDF_ERROR_ASSERT(mwifi_start());

}
void app_main(void)
{
    
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    //io_conf.pull_down_en = 0;
    //io_conf.pull_up_en = 0;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_PA_CTRL;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
    gpio_set_level((gpio_num_t)PA_CTRL,true);

    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    esp_log_level_set("bufferedaudioSink", ESP_LOG_INFO);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    init_spiffs();

    mesh_init();

    uint8_t primary = 0;
    mesh_addr_t parent_bssid = {0};
    uint8_t sta_mac[MWIFI_ADDR_LEN] = {0};
    wifi_sta_list_t wifi_sta_list = {0x0};

    while(1)
    {
        // esp_wifi_get_mac(ESP_IF_WIFI_STA, sta_mac);
        // esp_wifi_ap_get_sta_list(&wifi_sta_list);
        // esp_mesh_get_parent_bssid(&parent_bssid);

        // MDF_LOGI("System information, channel: %d, layer: %d, self mac: " MACSTR ", parent bssid: " MACSTR
        // ", parent rssi: %d, node num: %d, free heap: %u",
        // primary,
        // esp_mesh_get_layer(), MAC2STR(sta_mac), MAC2STR(parent_bssid.addr),
        // mwifi_get_parent_rssi(), esp_mesh_get_total_node_num(), esp_get_free_heap_size());

        //MDF_LOGI("tsf_time: %lld", esp_mesh_get_tsf_time());
        //mesh_light_set(esp_mesh_get_layer());
        for (int i = 0; i < wifi_sta_list.num; i++)
        {
            MDF_LOGI("Child mac: " MACSTR, MAC2STR(wifi_sta_list.sta[i].mac));
        }
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
}

