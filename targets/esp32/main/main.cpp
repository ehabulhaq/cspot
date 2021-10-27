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
#include "mupgrade.h"
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
uint8_t FW_update_flag = 1;

std::shared_ptr<ConfigJSON> configMan;

extern "C"
{
    void app_main(void);
    static void root_read_task(void *arg);
    static void node_read_task(void *arg);
    static void ota_task(void *arg);
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

static void root_read_task(void *arg)
{
    mdf_err_t ret = MDF_OK;
    char *data    = (char *)MDF_MALLOC(MWIFI_PAYLOAD_LEN);
    size_t size   = MWIFI_PAYLOAD_LEN;
    mwifi_data_type_t data_type      = {0};
    uint8_t src_addr[MWIFI_ADDR_LEN] = {0};

    MDF_LOGI("Root read task is running");


    while (FW_update_flag == 1) {
        size = MWIFI_PAYLOAD_LEN;
        memset(data, 0, MWIFI_PAYLOAD_LEN);
        ret = mwifi_root_read(src_addr, &data_type, data, &size, portMAX_DELAY);
        MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mwifi_root_recv", mdf_err_to_name(ret));

        if (data_type.upgrade) { // This mesh package contains upgrade data.
            ret = mupgrade_root_handle(src_addr, data, size);
            MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mupgrade_root_handle", mdf_err_to_name(ret));
        } else {
            MDF_LOGI("Receive [NODE] addr: " MACSTR ", size: %d, data: %s",
                     MAC2STR(src_addr), size, data);
        }
    }

    MDF_LOGW("Root read task is exit");

    MDF_FREE(data);
    vTaskDelete(NULL);
}

/**
 * @brief Handling data between wifi mesh devices.
 */
static void node_read_task(void *arg)
{
    mdf_err_t ret = MDF_OK;
    char *data    = (char *) MDF_MALLOC(MWIFI_PAYLOAD_LEN);
    size_t size   = MWIFI_PAYLOAD_LEN;
    mwifi_data_type_t data_type      = {0x0};
    uint8_t src_addr[MWIFI_ADDR_LEN] = {0};

    MDF_LOGI("Node read task is running");

    while (FW_update_flag == 1) {
        size = MWIFI_PAYLOAD_LEN;
        memset(data, 0, MWIFI_PAYLOAD_LEN);
        ret = mwifi_read(src_addr, &data_type, data, &size, portMAX_DELAY);
        MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mwifi_root_recv", mdf_err_to_name(ret));

        if (data_type.upgrade) { // This mesh package contains upgrade data.
            ret = mupgrade_handle(src_addr, data, size);
            MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mupgrade_handle", mdf_err_to_name(ret));
        } else {
            MDF_LOGI("Receive [ROOT] addr: " MACSTR ", size: %d, data: %s",
                     MAC2STR(src_addr), size, data);

            /**
             * @brief Finally, the node receives a restart notification. Restart it yourself..
             */
            if (!strcmp(data, "restart")) {
                MDF_LOGI("Restart the version of the switching device");
                MDF_LOGW("The device will restart after 3 seconds");
                vTaskDelay(pdMS_TO_TICKS(3000));
                esp_restart();
            }
        }
    }

    MDF_LOGW("Node read task is exit");

    MDF_FREE(data);
    vTaskDelete(NULL);
}


#define FIRMWARE_FILE_NAME_NODE                 "Node_speaker.bin"
#define CONFIG_FIRMWARE_UPGRADE_URL_NODE        "http://192.168.1.11:8070/Desktop/brian_stover/Node_speaker/build/Node_speaker.bin" 


#define FIRMWARE_FILE_NAME_ROOT                 "cspot-esp32.bin"
#define CONFIG_FIRMWARE_UPGRADE_URL_ROOT        "http://192.168.1.11:8070/Desktop/brian_stover/cspot/targets/esp32/build/cspot-esp32.bin"


static void ota_task(void *arg)
{
    mdf_err_t ret       = MDF_OK;
    uint8_t *data       = (uint8_t*)MDF_MALLOC(MWIFI_PAYLOAD_LEN);
    char name[32]       = {0x0};
    int total_size   = 0;
    int start_time      = 0;
    mupgrade_result_t upgrade_result = {0};
    mwifi_data_type_t data_type = {.communicate = MWIFI_COMMUNICATE_MULTICAST};

    /**
     * @note If you need to upgrade all devices, pass MWIFI_ADDR_ANY;
     *       If you upgrade the incoming address list to the specified device
     */
    // uint8_t dest_addr[][MWIFI_ADDR_LEN] = {{0x1, 0x1, 0x1, 0x1, 0x1, 0x1}, {0x2, 0x2, 0x2, 0x2, 0x2, 0x2},};
    uint8_t dest_addr[][MWIFI_ADDR_LEN] = {MWIFI_ADDR_ANY};

    /**
     * @brief In order to allow more nodes to join the mesh network for firmware upgrade,
     *      in the example we will start the firmware upgrade after 30 seconds.
     */
    vTaskDelay(10 * 1000 / portTICK_PERIOD_MS);

    esp_http_client_config_t config = {
        .url            = CONFIG_FIRMWARE_UPGRADE_URL_NODE,
        .transport_type = HTTP_TRANSPORT_UNKNOWN,
    };

    /**
     * @brief 1. Connect to the server
     */
    esp_http_client_handle_t client = esp_http_client_init(&config);
    //MDF_ERROR_GOTO(!client, EXIT, "Initialise HTTP connection");

    start_time = xTaskGetTickCount();

    MDF_LOGI("Open HTTP connection: %s", CONFIG_FIRMWARE_UPGRADE_URL_NODE);

    /**
     * @brief First, the firmware is obtained from the http server and stored on the root node.
     */
    do {
        ret = esp_http_client_open(client, 0);

        if (ret != MDF_OK) {
            if (!esp_mesh_is_root()) {
                return;
            }

            vTaskDelay(pdMS_TO_TICKS(1000));
            MDF_LOGW("<%s> Connection service failed", mdf_err_to_name(ret));
        }
    } while (ret != MDF_OK  && (xTaskGetTickCount() - start_time) * portTICK_RATE_MS / 1000  < 8); //break if more than 10 seconds

    if (ret != MDF_OK )
    {
        FW_update_flag = 0;  //signal other ota necessary tasks to exit as well
        xTaskCreatePinnedToCore(&cspotTask, "cspot", 8192 * 8, NULL, 5, NULL, 0); // run the cspot task and get out
        
        MDF_FREE(data);
        vTaskDelete(NULL);
        return;         // exit the ota_task. No firmware upgrade is happening today.
    }

    total_size = esp_http_client_fetch_headers(client);
    //sscanf(CONFIG_FIRMWARE_UPGRADE_URL, "%*[^/]//%*[^/]/%[^.]", name);

    if (total_size <= 0) {
        MDF_LOGW("Please check the address of the server");
        ret = esp_http_client_read(client, (char *)data, MWIFI_PAYLOAD_LEN);
        //MDF_ERROR_GOTO(ret < 0, EXIT, "<%s> Read data from http stream", mdf_err_to_name(ret));

        MDF_LOGW("Recv data: %.*s", ret, data);
        return;
    }

    /**
     * @brief 2. Initialize the upgrade status and erase the upgrade partition.
     */
    ret = mupgrade_firmware_init(FIRMWARE_FILE_NAME_NODE, total_size);
    //MDF_ERROR_GOTO(ret != MDF_OK, EXIT, "<%s> Initialize the upgrade status", mdf_err_to_name(ret));

    /**
     * @brief 3. Read firmware from the server and write it to the flash of the root node
     */
    for (ssize_t size = 0, recv_size = 0; recv_size < total_size; recv_size += size) {
        size = esp_http_client_read(client, (char *)data, MWIFI_PAYLOAD_LEN);
        //MDF_ERROR_GOTO(size < 0, EXIT, "<%s> Read data from http stream", mdf_err_to_name(ret));

        if (size > 0) {
            /* @brief  Write firmware to flash */
            ret = mupgrade_firmware_download(data, size);
            //MDF_ERROR_GOTO(ret != MDF_OK, EXIT, "<%s> Write firmware to flash, size: %d, data: %.*s",
            //               mdf_err_to_name(ret), size, size, data);
        } else {
            MDF_LOGW("<%s> esp_http_client_read", mdf_err_to_name(ret));
            return;
        }
    }

    MDF_LOGI("The service download firmware is complete, Spend time: %ds",
             (xTaskGetTickCount() - start_time) * portTICK_RATE_MS / 1000);

    start_time = xTaskGetTickCount();

    /**
     * @brief 4. The firmware will be sent to each node.
     */
    ret = mupgrade_firmware_send((uint8_t *)dest_addr, sizeof(dest_addr) / MWIFI_ADDR_LEN, &upgrade_result);
    //MDF_ERROR_GOTO(ret != MDF_OK, EXIT, "<%s> mupgrade_firmware_send", mdf_err_to_name(ret));

    if (upgrade_result.successed_num == 0) {
        MDF_LOGW("Devices upgrade failed, unfinished_num: %d", upgrade_result.unfinished_num);
        return;
    }

    MDF_LOGI("Firmware is sent to the device to complete, Spend time: %ds",
             (xTaskGetTickCount() - start_time) * portTICK_RATE_MS / 1000);
    MDF_LOGI("Devices upgrade completed, successed_num: %d, unfinished_num: %d", upgrade_result.successed_num, upgrade_result.unfinished_num);

    // Root downloads its own firmware from the server

    //mupgrade_result_free(&upgrade_result);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    esp_http_client_config_t config_root = {
        .url            = CONFIG_FIRMWARE_UPGRADE_URL_ROOT,
        .transport_type = HTTP_TRANSPORT_UNKNOWN,
    };

    /**
     * @brief 1. Connect to the server
     */
    client = esp_http_client_init(&config_root);
    //MDF_ERROR_GOTO(!client, EXIT, "Initialise HTTP connection");

    start_time = xTaskGetTickCount();

    MDF_LOGI("Open HTTP connection: %s", CONFIG_FIRMWARE_UPGRADE_URL_ROOT);

    /**
     * @brief First, the firmware is obtained from the http server and stored on the root node.
     */
    do {
        ret = esp_http_client_open(client, 0);

        if (ret != MDF_OK) {
            if (!esp_mesh_is_root()) {
                return;
            }

            vTaskDelay(pdMS_TO_TICKS(1000));
            MDF_LOGW("<%s> Connection service failed", mdf_err_to_name(ret));
        }
    } while (ret != MDF_OK);



    total_size = esp_http_client_fetch_headers(client);
    //sscanf(CONFIG_FIRMWARE_UPGRADE_URL, "%*[^/]//%*[^/]/%[^.]", name);

    if (total_size <= 0) {
        MDF_LOGW("Please check the address of the server");
        ret = esp_http_client_read(client, (char *)data, MWIFI_PAYLOAD_LEN);
        //MDF_ERROR_GOTO(ret < 0, EXIT, "<%s> Read data from http stream", mdf_err_to_name(ret));

        MDF_LOGW("Recv data: %.*s", ret, data);
        return;
    }

    /**
     * @brief 2. Initialize the upgrade status and erase the upgrade partition.
     */
    ret = mupgrade_firmware_init(FIRMWARE_FILE_NAME_ROOT, total_size);
    //MDF_ERROR_GOTO(ret != MDF_OK, EXIT, "<%s> Initialize the upgrade status", mdf_err_to_name(ret));

    /**
     * @brief 3. Read firmware from the server and write it to the flash of the root node
     */
    for (ssize_t size = 0, recv_size = 0; recv_size < total_size; recv_size += size) {
        size = esp_http_client_read(client, (char *)data, MWIFI_PAYLOAD_LEN);
        //MDF_ERROR_GOTO(size < 0, EXIT, "<%s> Read data from http stream", mdf_err_to_name(ret));

        if (size > 0) {
            /* @brief  Write firmware to flash */
            ret = mupgrade_firmware_download(data, size);
            //MDF_ERROR_GOTO(ret != MDF_OK, EXIT, "<%s> Write firmware to flash, size: %d, data: %.*s",
            //               mdf_err_to_name(ret), size, size, data);
        } else {
            MDF_LOGW("<%s> esp_http_client_read", mdf_err_to_name(ret));
            return;
        }
    }

    // 
    /**
     * @brief 5. the root notifies nodes to restart
     */
    const char *restart_str = "restart";
    ret = mwifi_root_write(upgrade_result.successed_addr, upgrade_result.successed_num,
                           &data_type, restart_str, strlen(restart_str), true);
    //MDF_ERROR_GOTO(ret != MDF_OK, EXIT, "<%s> mwifi_root_recv", mdf_err_to_name(ret));

    MDF_FREE(data);
    mupgrade_result_free(&upgrade_result);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
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

        xTaskCreate(node_read_task, "node_read_task", 4 * 1024,
                        NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);

            if (esp_mesh_get_layer() == MESH_ROOT_LAYER) {
                xTaskCreate(root_read_task, "root_read_task", 4 * 1024,
                            NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);
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
         xTaskCreate(ota_task, "ota_task", 4 * 1024,
                        NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);
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

