#include "BufferedAudioSink.h"

#include "driver/i2s.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "wifi_mesh_stream.h"

#include "mdf_common.h"
#include "mwifi.h"

RingbufHandle_t dataBuffer;
RingbufHandle_t dataBuffer_mesh;
RingbufHandle_t dataBuffer_stereo;

static const char *TAG = "bufferedaudioSink";

static void i2sFeed(void *pvParameters)
{
    while (true)
    {
        // if ( 1024 > (4096 * 8) - xRingbufferGetCurFreeSize(dataBuffer))
        // {
        //     vTaskDelay( 10 / portTICK_RATE_MS);
        //     continue;
        // }
        size_t itemSize;
        char *item = (char *)xRingbufferReceiveUpTo(dataBuffer, &itemSize, portMAX_DELAY, 512);
        if (item != NULL)
        {
            size_t written = 0;
            
            while (written < itemSize)
            {
                i2s_write((i2s_port_t)0, item, itemSize, &written, portMAX_DELAY);
            }
            vRingbufferReturnItem(dataBuffer, (void *)item);
        }
    }
}

static void meshFeed(void *pvParameters)
{
    mdf_err_t ret = MDF_OK;
    char *data = (char*) MDF_MALLOC(MWIFI_PAYLOAD_LEN);
    size_t size = MWIFI_PAYLOAD_LEN;
    uint8_t src_addr[MWIFI_ADDR_LEN] = {0x0};
    mwifi_data_type_t data_type = {0};
    int freeSize;

    while (true)
    {
        size = MWIFI_PAYLOAD_LEN;
        size_t itemSize;
        if ( 1024 * 7 > (4096 * 8) - xRingbufferGetCurFreeSize(dataBuffer_mesh))
        {
            vTaskDelay( 10 / portTICK_RATE_MS);
            continue;
        }

        ret = mwifi_root_read(src_addr, &data_type, data, &size, portMAX_DELAY);
        freeSize = *((int * ) data);
        
        //memset ( &freeSize, (int *) data, size);
        if ( freeSize < 1024 * 12)
        {
            ESP_LOGI(TAG, "freeSize=%d",  freeSize);
            continue;
        }
        char *item = (char *)xRingbufferReceiveUpTo(dataBuffer_mesh, &itemSize, portMAX_DELAY, 1024 * 7);
        
        _mesh_write(NULL, item, itemSize, portMAX_DELAY, NULL);
        vRingbufferReturnItem(dataBuffer_mesh, (void *)item);
        vTaskDelay( 30 / portTICK_RATE_MS);
        
    }


}

static void stereo_to_mono(void *pvParameters)
{
    char mono_buffer[512];
    uint16_t mono_size;
    size_t itemSize;
    while (true)
    {
        if ( 4096 > (4096 * 8) - xRingbufferGetCurFreeSize(dataBuffer_stereo))
        {
            vTaskDelay( 10 / portTICK_RATE_MS);
            continue;
        }

        char *item = (char *)xRingbufferReceiveUpTo(dataBuffer_stereo, &itemSize, portMAX_DELAY, 1024);
        //ESP_LOGI(TAG, "itemSize=%d",  itemSize);
        mono_size = itemSize / 2;

        //convert stereo to mono
        for ( int i = 0; i < mono_size; i+=2 )
        {

            mono_buffer[i] = item[2*i];
            mono_buffer[i+1] = item[2*i + 1];

        }
        //ESP_LOGI(TAG, "stereoBufferFreeSize=%d",   xRingbufferGetCurFreeSize(dataBuffer));
        // now write to buffers of mesh and I2S for playback
        xRingbufferSend(dataBuffer_mesh, mono_buffer, mono_size, portMAX_DELAY);
        //xRingbufferSend(dataBuffer, mono_buffer, mono_size, portMAX_DELAY);  
        
        //return the item
        vRingbufferReturnItem(dataBuffer_stereo, (void *)item);
    }


}

void BufferedAudioSink::startI2sFeed()
{
    dataBuffer = xRingbufferCreate(4096 * 8, RINGBUF_TYPE_BYTEBUF);
    dataBuffer_mesh = xRingbufferCreate(4096 * 8, RINGBUF_TYPE_BYTEBUF);
    dataBuffer_stereo = xRingbufferCreate(4096 * 8, RINGBUF_TYPE_BYTEBUF);
    xTaskCreatePinnedToCore(&i2sFeed, "i2sFeed", 4096, NULL, 10, NULL, 1);
    xTaskCreatePinnedToCore(&meshFeed, "meshFeed", 4096, NULL, 6, NULL, 1);
    xTaskCreatePinnedToCore(&stereo_to_mono, "stereo_to_mono", 4096, NULL, 6, NULL, 1);
}

void BufferedAudioSink::feedPCMFrames(std::vector<uint8_t> &data)
{
    
    xRingbufferSend(dataBuffer_stereo, &data[0], data.size(), portMAX_DELAY);
     
}

