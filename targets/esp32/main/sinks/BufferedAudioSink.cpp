#include "BufferedAudioSink.h"

#include "driver/i2s.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "wifi_mesh_stream.h"

RingbufHandle_t dataBuffer;
RingbufHandle_t dataBuffer_mesh;

static const char *TAG = "bufferedaudioSink";

static void i2sFeed(void *pvParameters)
{
    while (true)
    {
        size_t itemSize;
        char *item = (char *)xRingbufferReceiveUpTo(dataBuffer, &itemSize, portMAX_DELAY, 256);
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
    while (true)
    {
        size_t itemSize;
        char *item = (char *)xRingbufferReceiveUpTo(dataBuffer_mesh, &itemSize, portMAX_DELAY, 1456);
        _mesh_write(NULL, item, itemSize, portMAX_DELAY, NULL);
        vRingbufferReturnItem(dataBuffer_mesh, (void *)item);
    }


}
void BufferedAudioSink::startI2sFeed()
{
    dataBuffer = xRingbufferCreate(4096 * 8, RINGBUF_TYPE_BYTEBUF);
    dataBuffer_mesh = xRingbufferCreate(4096 * 8, RINGBUF_TYPE_BYTEBUF);
    xTaskCreatePinnedToCore(&i2sFeed, "i2sFeed", 4096, NULL, 10, NULL, 1);
    xTaskCreatePinnedToCore(&meshFeed, "meshFeed", 4096, NULL, 6, NULL, 1);
}

void BufferedAudioSink::feedPCMFrames(std::vector<uint8_t> &data)
{
    xRingbufferSend(dataBuffer_mesh, &data[0], data.size(), portMAX_DELAY);
    xRingbufferSend(dataBuffer, &data[0], data.size(), portMAX_DELAY);
     
    ESP_LOGE(TAG,"data size = %d", data.size()); 
}

