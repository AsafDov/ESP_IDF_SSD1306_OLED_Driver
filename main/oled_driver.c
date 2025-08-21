#include "driver/i2c.h"
// #include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include <string.h>
/**
 * OLED driver for ESP IDF
 * Created by Asaf "Arliden The Bard"
 */
 
#include "oled_driver.h"

#define I2C_MASTER_TIMEOUT_MS 1000
#define TAG "OLED"

const unsigned char clear_screen_bitmap[1024] = {0};

struct oled{
    i2c_master_bus_handle_t bus_handle;
    uint16_t device_address;
    i2c_addr_bit_len_t address_bit_length;
    int width;
    int height;
    int refresh_rate_ms;
    uint32_t scl_speed_hz;
    i2c_master_dev_handle_t i2c_device_oled_handle;
    unsigned char* frame_buffer; 
};

int oled_get_refresh_rate(oled_handle_t oled_handle) {
    return oled_handle->refresh_rate_ms;
}

void send_command(oled_handle_t oled_handle,unsigned char* cmd, int cmd_size){
    esp_err_t ret = ESP_FAIL;
    switch (cmd_size){
        case 2:
        case 3:
        case 4:
            ret = i2c_master_transmit(oled_handle->i2c_device_oled_handle, cmd, cmd_size, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
        break;

        default:
            ESP_LOGI("OLED", "Invalid array size in send_command");
    }
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Write OK");
    } else if (ret == ESP_ERR_TIMEOUT) {
        ESP_LOGW(TAG, "Bus is busy");
    } else {
        ESP_LOGW(TAG, "Write Failed");
    }
}

void reset_cursor(oled_handle_t oled_handle) {
    // Set Column Address Range
    // This command requires 3 bytes: Command (0x21), Start Address (0x00), End Address (0x7F)
    unsigned char set_column_address[4] = {0x00, 0x21, 0x00, 0x7F}; 
    send_command(oled_handle, set_column_address, 4);

    // Set Page Address Range
    // This command requires 3 bytes: Command (0x22), Start Address (0x00), End Address (0x07)
    unsigned char set_page_address[4] = {0x00, 0x22, 0x00, 0x07};
    send_command(oled_handle, set_page_address, 4);
}

// @brief Convert bitmap to page buffer format for OLED
void bitmap_to_page_buffer(oled_handle_t oled_handle, unsigned char* bitmap, unsigned char* page_buffer, unsigned int page_buffer_size){
    // Page index starts from 1 because the first byte holds the command byte 0x40
    // Take each byte. And it with a Mask of 2^j
    unsigned char column;

    // for(int page=1; page<page_buffer_size; page++){
    //     for(int i=0; i<(oled_handle->width * oled_handle->height) - 8; i=i+8){
    //         for(int j=7; j>=0; j--){
    //             // Shift column left and OR with bitmap data
    //             column = (column << 1) | bitmap[i + j * oled_handle->width];
    //         }
    //     }
    //     page_buffer[page] = column;
    // }

    for(int page_num=0; page_num<8; page_num++){
        for (int byte_num=page_num*128; byte_num<(page_num+1)*128; byte_num++ ){
            for(int bit_num=256; bit_num>0; bit_num >>= 1){
                for(int k=0; k<8; k++){
                    column |= (bitmap[byte_num+k*16] & (bit_num>>1)) >> 1 ;
                }
                page_buffer[byte_num] = column;
            }
        }
    }

}

void oled_flush_gddram(oled_handle_t oled_handle) {
    int write_buf_size = (oled_handle->width * oled_handle->height)/8 + 1;
    uint8_t* write_buf = (uint8_t* )malloc(write_buf_size);
    write_buf[0] = 0x40;
    int bitmap_size = (oled_handle->width * oled_handle->height) / 8;
    memcpy(&write_buf[1], clear_screen_bitmap, bitmap_size);

    // Set the Column and Page addresses
    reset_cursor(oled_handle);

    // Send the data to the OLED display
    ESP_ERROR_CHECK(i2c_master_transmit(oled_handle->i2c_device_oled_handle, write_buf, write_buf_size, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS));

    free(write_buf);
}
void send_config_commands(oled_handle_t oled_handle){

    unsigned char double_cmd[3] = {0x00, 0x00, 0x00};
    unsigned char single_cmd[2] = {0x00, 0x00};
    // # Turn Display OFF (important before changing settings)
    single_cmd[0] = 0x00;
    single_cmd[1] = 0xAE;
    send_command(oled_handle, single_cmd, 2);

    // # Set Display Clock Divide Ratio/Oscillator Frequency
    double_cmd[0] = 0x00;
    double_cmd[1] = 0xD5;
    double_cmd[2] = 0x80;
    send_command(oled_handle, double_cmd, 3);

    // # Set Multiplex Ratio
    double_cmd[0] = 0x00;
    double_cmd[1] = 0xA8;
    double_cmd[2] = 0x3F;
    send_command(oled_handle, double_cmd, 3);

    // # Set Display Offset
    double_cmd[0] = 0x00;
    double_cmd[1] = 0xD3;
    double_cmd[2] = 0x00;
    send_command(oled_handle, double_cmd, 3);

    // # Set Display Start Line
    single_cmd[0] = 0x00;
    single_cmd[1] = 0x40; // Set display start line to 0
    send_command(oled_handle, single_cmd, 2);

    // # Enable Charge Pump Regulator
    double_cmd[0] = 0x00;
    double_cmd[1] = 0x8D;
    double_cmd[2] = 0x14;
    send_command(oled_handle, double_cmd, 3);

    // # Set Memory Addressing Mode
    double_cmd[0] = 0x00;
    double_cmd[1] = 0x20;
    double_cmd[2] = 0x00;
    send_command(oled_handle, double_cmd, 3);

    // # Set Segment Re-map
    single_cmd[0] = 0x00;
    single_cmd[1] = 0xA1;
    send_command(oled_handle, single_cmd, 2);

    // # Set COM Output Scan Direction
    single_cmd[0] = 0x00;
    single_cmd[1] = 0xC8;
    send_command(oled_handle, single_cmd, 2);

    // # Set COM Pins Hardware Configuration
    double_cmd[0] = 0x00;
    double_cmd[1] = 0xDA;
    double_cmd[2] = 0x12;
    send_command(oled_handle, double_cmd, 3);

    // # Set Contrast Control
    double_cmd[0] = 0x00;
    double_cmd[1] = 0x81;
    double_cmd[2] = 0xCF;
    send_command(oled_handle, double_cmd, 3);

    // # Set Pre-charge Period
    double_cmd[0] = 0x00;
    double_cmd[1] = 0xD9;
    double_cmd[2] = 0xF1;
    send_command(oled_handle, double_cmd, 3);

    // # Set VCOMH Deselect Level
    double_cmd[0] = 0x00;
    double_cmd[1] = 0xDB;
    double_cmd[2] = 0x40;
    send_command(oled_handle, double_cmd, 3);

     // # Disable Entire Display On (A4h)
    single_cmd[0] = 0x00; 
    single_cmd[1] = 0xA4;
    send_command(oled_handle, single_cmd, 2);

    // # Set Normal Display
    single_cmd[0] = 0x00;
    single_cmd[1] = 0xA6;
    send_command(oled_handle, single_cmd, 2);

    // # Turn Display ON
    single_cmd[0] = 0x00;
    single_cmd[1] = 0xAF;
    send_command(oled_handle, single_cmd, 2);

    // Set horizontal addressing mode
    unsigned char set_horizontal_addressing[3] = {0x00, 0x20, 0x00}; 
    send_command(oled_handle, set_horizontal_addressing, 3);

    // Flush GDDRAM
    oled_flush_gddram(oled_handle);

}

/// @brief Flash all pixels
void test_oled(oled_handle_t oled_handle){
    unsigned char command[2] = {0,0};
    int delay = 100;
    ESP_LOGI("OLED", "Testing OLED. Should OLED falshing");

    for (int i=0; i<10; i++){
        command[0] = 0x00;
        command[1] = 0xA5;
        ESP_ERROR_CHECK(i2c_master_transmit(oled_handle->i2c_device_oled_handle, command, 2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS));
        
        vTaskDelay(delay/portTICK_PERIOD_MS);
        command[0] = 0x00;
        command[1] = 0xA4;
        ESP_ERROR_CHECK(i2c_master_transmit(oled_handle->i2c_device_oled_handle, command, 2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS));
        vTaskDelay(delay/portTICK_PERIOD_MS);
    }
    ESP_LOGI("OLED", "Finished testing OLED");
}
void oled_update_frame_buffer(oled_handle_t oled_handle) {
    int bitmap_size = (oled_handle->width * oled_handle->height) / 8;
    int write_buf_size = bitmap_size + 1;
    
    unsigned char* write_buf = (unsigned char*)malloc(write_buf_size);
    write_buf[0] = 0x40; // Data command byte
    memcpy(&write_buf[1], oled_handle->frame_buffer, bitmap_size);

    reset_cursor(oled_handle);
    ESP_ERROR_CHECK(i2c_master_transmit(oled_handle->i2c_device_oled_handle, write_buf, write_buf_size, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS));
    
    free(write_buf);
}
oled_handle_t oled_init(i2c_master_bus_handle_t* pBus_handle, oled_config_t* oled_cfg){

    oled_handle_t handle = (oled_handle_t)malloc(sizeof(struct oled));
    if(handle == NULL){
        ESP_LOGI("OLED","oled handle malloc failed");
        return NULL;
    }
    handle->bus_handle = *pBus_handle;
    handle->device_address = oled_cfg->device_address;
    handle->address_bit_length = oled_cfg->address_bit_length;
    handle->width = oled_cfg->width;
    handle->height = oled_cfg->height;
    handle->refresh_rate_ms = oled_cfg->refresh_rate_ms;
    handle->i2c_device_oled_handle = oled_cfg->i2c_device_oled_handle;
    handle->scl_speed_hz = oled_cfg->scl_speed_hz;

    // Allocate and clear the framebuffer
    int buffer_size = (handle->width * handle->height) / 8;
    handle->frame_buffer = (unsigned char*)malloc(buffer_size);
    memset(handle->frame_buffer, 0x00, buffer_size);

    // i2c device configuration adn initialization
    i2c_device_config_t i2c_device_oled_cfg = {
        .dev_addr_length = oled_cfg->address_bit_length, //OLED.Address_Length
        .device_address = oled_cfg->device_address, //OLED.address
        .scl_speed_hz = oled_cfg->scl_speed_hz, // OLED.clockspeed
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(*pBus_handle, &i2c_device_oled_cfg, &handle->i2c_device_oled_handle));
    send_config_commands(handle);
    test_oled(handle);

    return handle;
}

/// @brief Print the entire bitmap inefficiently.
/// @param oled_handle 
/// @param bitmap 
void oled_print_bitmap(oled_handle_t oled_handle, const unsigned char* pBitmap, int bitmap_size){
    
    
    int write_buf_size = (oled_handle->width * oled_handle->height)/8 +1;
    unsigned char* write_buf = (unsigned char* )malloc(write_buf_size);

    write_buf[0] = 0x40;
    // This operation prepares the bitmap format for display
    // TODO Need to see about bitmap size and check if fits the oled
    //bitmap_to_page_buffer(oled_handle, pBitmap, write_buf, write_buf_size);
    memcpy(&write_buf[1], pBitmap, bitmap_size);
    // Set the Column and Page addresses
    reset_cursor(oled_handle);

    // Send the data to the OLED display
    ESP_ERROR_CHECK(i2c_master_transmit(oled_handle->i2c_device_oled_handle, write_buf, write_buf_size, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS));

    free(write_buf);
};

// @param x1: The x-coordinate of the start point 0-127
// @param y1: The y-coordinate of the start point 0-64
// @param x2: The x-coordinate of the end point 0-127
// @param y2: The y-coordinate of the end point 0-64
// @param width: The width of the line
void oled_square_filled(oled_handle_t oled_handle, int x1, int y1, int x2, int y2) {
    int lower_x = x1>x2 ? x2:x1;
    int upper_x = x1>x2 ? x1:x2;
    int lower_y = (y1>y2 ? y2:y1);
    int upper_y = (y1>y2 ? y1:y2);

    // This function now modifies the local framebuffer, not the OLED directly
    for (int x = lower_x; x <= upper_x; x++) {
        for (int y = lower_y; y <= upper_y; y++) {
            // Calculate the position in the framebuffer
            int page = y / 8;
            int bit = y % 8;
            int index = x + (page * oled_handle->width);
            
            // Set the bit in the framebuffer using a read-modify-write operation
            // Uncomment for toggle mode
            // oled_handle->frame_buffer[index] ^= (1 << bit);
            
            oled_handle->frame_buffer[index] |= (1 << bit);
        }
    }

    // Update screen
    oled_update_frame_buffer(oled_handle);
}

