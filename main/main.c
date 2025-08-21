/*
    Running random walk app on an OLED
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "oled_driver.h"
#include "sdkconfig.h"
#include "../../../../esp/v5.5/esp-idf/components/esp_driver_i2c/test_apps/i2c_test_apps/main/test_board.h"
#include "bitmaps.h"
#include <stdlib.h> // Required for rand() and srand()
#include <time.h>   // Required for time()

// static const char *TAG = "example";

#define SCL_SPEED_HZ 100000
#define SCL_PIN GPIO_NUM_22
#define SDA_PIN GPIO_NUM_21

// OLED CONFIG
#define OLED_I2C_ADDRESS 0x3c
#define OLED_SCREEN_WIDTH 128
#define OLED_SCREEN_HEIGHT 64
#define OLED_STATUS_BAR_HEIGHT 16
#define OLED_CONTENT_HEIGHT (OLED_SCREEN_HEIGHT - OLED_STATUS_BAR_HEIGHT)
#define OLED_REFRESH_RATE_MS 100

// Globals:

// Functions
void random_walk(void* params){
    oled_handle_t oled_handle = (oled_handle_t)params;
    srand(time(NULL));
    int x = rand() % 128;
    int y = rand() % 48 + 16;
    int dir = 0;
    while (1) {
        oled_square_filled(oled_handle, x, y, x + 1, y + 1);
        vTaskDelay(oled_get_refresh_rate(oled_handle) / portTICK_PERIOD_MS);
        dir = rand() % 4; // Random direction: 0=up, 1=down, 2=left, 3=right
        switch (dir) {
            case 0: // Up
                y = ((y-16) - 1 + 48 ) % 48 + 16;
                break;
            case 1: // Down
                y = ((y-16) + 1 ) % 48 + 16;
                break;
            case 2: // Left
                x = (x - 1 + 128) % 128;
                break;
            case 3: // Right
                x = (x + 1) % 128;
                break;
        }
    }
};

void app_main(void)
{
    ESP_LOGI("I2C", "Configuring I2C Master");
    /* Configure and Instantiate I2C Master */
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = TEST_I2C_PORT,
        .scl_io_num = SCL_PIN, 
        .sda_io_num = SDA_PIN, 
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_LOGI("I2C", "Initializing I2C Master");
    static i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));

    /* Configure and Instantiate OLED */
    ESP_LOGI("OLED", "Configuring OLED");
    oled_config_t oled_cfg = {
        .device_address = OLED_I2C_ADDRESS,
        .address_bit_length = I2C_ADDR_BIT_LEN_7,
        .width = OLED_SCREEN_HEIGHT,
        .height = OLED_SCREEN_WIDTH,
        .refresh_rate_ms = OLED_REFRESH_RATE_MS,
        .scl_speed_hz = SCL_SPEED_HZ,
    };
    ESP_LOGI("OLED", "Initializing OLED");
    oled_handle_t oled_handle = oled_init(&bus_handle, &oled_cfg);
    ESP_LOGI("OLED", "OLED Initialized");

    // Start random walk task
    xTaskCreatePinnedToCore(random_walk, "random_walk", 2048, oled_handle, 5, NULL, 1);

    while(1){
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
