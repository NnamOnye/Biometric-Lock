/*
 * fingersource.c
 *
 *  Created on: Jun 6, 2025
 *      Author: M.O
 */

#include<driver/gpio.h>
#include "esp_sleep.h"
#include "soc/gpio_num.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "driver/uart.h"
#include<esp_log.h>
#include "nvs.h"
#include "nvs_flash.h"
#include <stdio.h>
#include"buttonsource.h"
#include "ssd1306.h"


#define ESP_LOGI(tag, format, ...)
#define ESP_LOGE(tag, format, ...)

#define CONFIG_OFFSETX 0  // or 2, or whatever the offset needs to be
#define TAG "FPM"

//Define Finger Print Sensor --> RS558-S
#define FPM_UART_PORT UART_NUM_1
#define FPM_HEADER     0xEF01
#define FPM_ADDRESS    0xFFFFFFFF
#define FPM_IDENTIFIER 0x01
#define UARTTX_GPIO GPIO_NUM_15
#define UARTRX_GPIO GPIO_NUM_16
#define TOUCHOUT_GPIO GPIO_NUM_5
#define BAUD_RATE 19200 
#define FINGERPRINT_ID 1  // storage location (0-255)
#define BUFFER_ID_1 1
#define BUFFER_ID_2 2
#define FPM_PID_CMD 0x01
#define FPM_PID_ACK 0x07
#define FPM_OK 0x00
#define UART_BUF_SIZE 64

SSD1306_t dev;

void screen_on();// screen on call from button source in case its needed

void configure_fingerscan(){
	//Finger Print Sensor configuration
	gpio_config_t uarttx_conf = {
	    .pin_bit_mask = (1ULL << UARTTX_GPIO),
	    .mode = GPIO_MODE_OUTPUT,
	    .pull_up_en = GPIO_PULLUP_DISABLE,
	    .intr_type = GPIO_INTR_DISABLE
	};
	
	gpio_config_t uartrx_conf = {
	    .pin_bit_mask = (1ULL << UARTRX_GPIO),
	    .mode = GPIO_MODE_OUTPUT,
	    .pull_up_en = GPIO_PULLUP_DISABLE,
	    .intr_type = GPIO_INTR_DISABLE
	};
	
	gpio_config_t touchout_conf = {
	    .pin_bit_mask = (1ULL << TOUCHOUT_GPIO),
	    .mode = GPIO_MODE_INPUT,
	    .pull_up_en = GPIO_PULLUP_DISABLE,
	    .intr_type = GPIO_INTR_POSEDGE
	};
	
	//Finger Print Sensor configuration
    gpio_config(&uarttx_conf);
    gpio_config(&uartrx_conf);
    gpio_config(&touchout_conf);
	
}

void init_fingerprint_uart() { //initializing UART for the RS-558-S
    uart_config_t uart_config = {
        .baud_rate = BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    // Configure UART
    uart_param_config(FPM_UART_PORT, &uart_config);

    // Set TX and RX pins
    uart_set_pin(1, 15, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Install UART driver (RX buffer size, no event queue)
    //uart_driver_install(FPM_UART_PORT, 2048, 0, 0, NULL, 0);

    ESP_LOGI("UART", "Fingerprint UART initialized on TX=%d RX=%d",UARTTX_GPIO, UARTRX_GPIO);
}


void send_fpm_packet(uint8_t *packet, size_t len) {
    uart_write_bytes(FPM_UART_PORT, packet, len);
}

// Helper to calculate checksum for dynamic packets
uint16_t fpm_checksum(uint8_t *packet, size_t start_index, size_t len) {
    uint16_t sum = 0;
    for (size_t i = start_index; i < len; ++i) {
        sum += packet[i];
    }
    return sum;
}

// Wait for response
bool fpm_wait_for_ack(uint8_t expected_code) {
    uint8_t response[UART_BUF_SIZE];
    int len = uart_read_bytes(FPM_UART_PORT, response, sizeof(response), pdMS_TO_TICKS(500));

    if (len >= 12 && response[6] == FPM_PID_ACK) {
        if (response[9] == expected_code) {
            ESP_LOGI(TAG, "Received expected ACK 0x%02X", expected_code);
            return true;
        } else {
            ESP_LOGW(TAG, "Received ACK 0x%02X instead of 0x%02X", response[9], expected_code);
        }
    } else {
        ESP_LOGE(TAG, "Invalid or no ACK response.");
    }
    return false;
}



// 0x01 - Generate Image
void fpm_gen_image() {
    uint8_t packet[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
                        0x01, 0x00, 0x03, 0x01, 0x00, 0x05};
    send_fpm_packet(packet, sizeof(packet));
}

void fpm_gen_char(uint8_t buffer_id) {
    uint8_t packet[12] = {
        0xEF, 0x01,                         // Header
        0xFF, 0xFF, 0xFF, 0xFF,             // Module address
        0x01,                               // Packet identifier
        0x00, 0x04,                         // Packet length = 4
        0x02,                               // Instruction code: GenChar
        buffer_id,                          // Buffer ID (1 or 2)
        0x00, 0x00                          // Checksum placeholder
    };

    uint16_t checksum = fpm_checksum(packet, 0, 11);  // sum from [0] to [10]
    packet[10] = (checksum >> 8) & 0xFF;
    packet[11] = checksum & 0xFF;

    send_fpm_packet(packet, sizeof(packet));
    fpm_wait_for_ack(FPM_OK);
}

// 0x29 - Get Enroll Image
void fpm_get_enroll_image() {
    uint8_t packet[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
                        0x01, 0x00, 0x03, 0x29, 0x00, 0x2D};
    send_fpm_packet(packet, sizeof(packet));
}

// 0x03 - Match templates
void fpm_match() {
    uint8_t packet[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
                        0x01, 0x00, 0x03, 0x03, 0x00, 0x07};
    send_fpm_packet(packet, sizeof(packet));
}

// 0x04 - Search template
//void fpm_search() {
//    uint8_t packet[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
//                        0x01, 0x00, 0x08, 0x04, 0x01, 0x00, 0x00, 0x00, 0x0A, 0x14};
//    send_fpm_packet(packet, sizeof(packet));
//}

// 0x06 - Store template
void fpm_store_template(uint8_t buffer_id, uint16_t page_id) {
    uint8_t packet[14] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
                          0x01, 0x00, 0x06, 0x06, buffer_id,
                          (uint8_t)(page_id >> 8), (uint8_t)(page_id & 0xFF), 0x00};
    uint16_t checksum = fpm_checksum(packet, 6, 13);
    packet[13] = (uint8_t)(checksum & 0xFF);
    send_fpm_packet(packet, sizeof(packet));
}

// 0x0C - Delete template
void fpm_delete_template(uint16_t page_id, uint16_t num) {
    uint8_t packet[16] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
                          0x01, 0x00, 0x07, 0x0C,
                          (uint8_t)(page_id >> 8), (uint8_t)(page_id & 0xFF),
                          (uint8_t)(num >> 8), (uint8_t)(num & 0xFF), 0x00, 0x00};
    uint16_t checksum = fpm_checksum(packet, 6, 14);
    packet[14] = (uint8_t)(checksum >> 8);
    packet[15] = (uint8_t)(checksum & 0xFF);
    send_fpm_packet(packet, sizeof(packet));
}

// 0x12 - Set Password
void fpm_set_password(uint32_t password) {
    uint8_t packet[16] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
                          0x01, 0x00, 0x07, 0x12,
                          (password >> 24) & 0xFF, (password >> 16) & 0xFF,
                          (password >> 8) & 0xFF, password & 0xFF, 0x00, 0x00};
    uint16_t checksum = fpm_checksum(packet, 6, 14);
    packet[14] = (uint8_t)(checksum >> 8);
    packet[15] = (uint8_t)(checksum & 0xFF);
    send_fpm_packet(packet, sizeof(packet));
}

// 0x13 - Verify Password
void fpm_verify_password(uint32_t password) {
    fpm_set_password(password);  // Uses same format
}

// 0x33 - Sleep
void fpm_sleep() {
    uint8_t packet[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
                        0x01, 0x00, 0x03, 0x33, 0x00, 0x37};
    send_fpm_packet(packet, sizeof(packet));
}

// 0x0A - Upload image
void fpm_upload_image() {
    uint8_t packet[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
                        0x01, 0x00, 0x03, 0x0A, 0x00, 0x0E};
    send_fpm_packet(packet, sizeof(packet));
}

// 0x07 - Load character file from library
void fpm_load_char(uint8_t buffer_id, uint16_t page_id) {
    uint8_t packet[15] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
                          0x01, 0x00, 0x06, 0x07, buffer_id,
                          (page_id >> 8), (page_id & 0xFF), 0x00, 0x00};
    uint16_t checksum = fpm_checksum(packet, 6, 13);
    packet[13] = (uint8_t)(checksum >> 8);
    packet[14] = (uint8_t)(checksum & 0xFF);
    send_fpm_packet(packet, sizeof(packet));
}

// 0x0F - Read system parameters
void fpm_read_sys_para() {
    uint8_t packet[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
                        0x01, 0x00, 0x03, 0x0F, 0x00, 0x13};
    send_fpm_packet(packet, sizeof(packet));
}

// 0x3B - Restore to factory settings
void fpm_restore_factory() {
    uint8_t packet[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
                        0x01, 0x00, 0x03, 0x3B, 0x00, 0x3F};
    send_fpm_packet(packet, sizeof(packet));
}

// 0x05 - Register Model
void fpm_reg_model() {
    uint8_t packet[] = {
        0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
        0x01, 0x00, 0x03, 0x05, 0x00, 0x09
    };
    send_fpm_packet(packet, sizeof(packet));
    fpm_wait_for_ack(FPM_OK);
}

// 0x04 - Search template
void fpm_search(uint8_t buffer_id, uint16_t start_page, uint16_t page_num) {
    uint8_t packet[17] = {
        0xEF, 0x01,                         // Header
        0xFF, 0xFF, 0xFF, 0xFF,             // Module address
        0x01,                               // Packet identifier
        0x00, 0x08,                         // Packet length = 8
        0x04,                               // Instruction code: Search
        buffer_id,                          // Buffer ID (usually 1)
        (start_page >> 8) & 0xFF,
        start_page & 0xFF,
        (page_num >> 8) & 0xFF,
        page_num & 0xFF,
        0x00, 0x00                          // Checksum placeholder
    };

    uint16_t checksum = fpm_checksum(packet, 6, 14);
    packet[14] = (checksum >> 8) & 0xFF;
    packet[15] = checksum & 0xFF;

    send_fpm_packet(packet, sizeof(packet));
    fpm_wait_for_ack(FPM_OK);
}

esp_err_t enroll_fingerprint() {
    ESP_LOGI(TAG, "Place finger on sensor...");
    ssd1306_clear_screen(&dev, false);
    ssd1306_display_text(&dev,4, "Place finger on sensor...",25,false);
    fpm_gen_image();
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (!fpm_wait_for_ack(FPM_OK)) return ESP_FAIL;
    
    fpm_gen_char( BUFFER_ID_1);
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (!fpm_wait_for_ack(FPM_OK)) return ESP_FAIL;

    fpm_load_char(BUFFER_ID_1, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (!fpm_wait_for_ack(FPM_OK)) return ESP_FAIL;
    
    fpm_store_template(BUFFER_ID_1, 1);
    if (!fpm_wait_for_ack(FPM_OK)) return ESP_FAIL;

    ESP_LOGI(TAG, "Remove and place the same finger again...");
    ssd1306_clear_screen(&dev, false);
    ssd1306_display_text(&dev,4, "Remove wait 5 seconds place the same finger again...",52,false);
    vTaskDelay(pdMS_TO_TICKS(4000));

    fpm_gen_image();
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (!fpm_wait_for_ack(FPM_OK)) return ESP_FAIL;
    
    fpm_gen_char(BUFFER_ID_2);
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (!fpm_wait_for_ack(FPM_OK)) return ESP_FAIL;

    fpm_load_char(BUFFER_ID_2,2);
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (!fpm_wait_for_ack(FPM_OK)) return ESP_FAIL;
    
    fpm_store_template(BUFFER_ID_2, 2);
    if (!fpm_wait_for_ack(FPM_OK)) return ESP_FAIL;

	fpm_match();
	vTaskDelay(pdMS_TO_TICKS(1000));
    if (!fpm_wait_for_ack(FPM_OK)) return ESP_FAIL;


	fpm_reg_model();
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (!fpm_wait_for_ack(FPM_OK)) return ESP_FAIL;
    
    
    
    ESP_LOGI(TAG, "Finger print enrolled at ID %d", FINGERPRINT_ID);
    ssd1306_clear_screen(&dev, false);
    ssd1306_display_text(&dev,4, "Finger print enrolled.",22,false);
    return ESP_OK;
}

void setup_new_fingerprint() {
    ESP_LOGI("SETUP", "Place your finger to enroll.");

    for (int attempt = 0; attempt < 3; attempt++){
		if (enroll_fingerprint()== ESP_OK) { //Replace enroll_fingerprint() with your actual fingerprint sensor's enrollment function (for example, from R558
        	ESP_LOGI("SETUP", "Fingerprint enrollment successful.");
        	ssd1306_clear_screen(&dev, false);
    		ssd1306_display_text(&dev,4, "Finger print enrollment successful.",35,false);
    		 return;
    	} else {
        	ESP_LOGE("SETUP", "Fingerprint enrollment failed.");
        	ESP_LOGE("SETUP", "Enrollment attempt %d failed", attempt + 1);
        	ssd1306_clear_screen(&dev, false);
        	ssd1306_display_text(&dev,4, "Finger print enrollment failed try again.",41,false);
        	vTaskDelay(pdMS_TO_TICKS(2000)); //delay before retry attempt
    	}
		
	}
	
	//After All Retries Failed
	ESP_LOGE("SETUP", "Finger print authentication failed after retries.");
    ssd1306_clear_screen(&dev, false);
    ssd1306_display_text(&dev, 4, "Enroll failed. Try reset.", 24, false);
    
}


bool fpm_check_library_contains(uint16_t id) {
    // Build LoadChar command to read the specified fingerprint ID from flash
    uint8_t packet[15] = {
        0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,  // Header + Address
        0x01, 0x00, 0x06, 0x07,             // PID + Length + LoadChar (0x07)
        0x01,                               // Buffer ID
        (uint8_t)(id >> 8), (uint8_t)(id & 0xFF), // Page ID (High, Low)
        0x00, 0x00                          // Checksum placeholder
    };
    uint16_t sum = fpm_checksum(packet, 13,20);
    packet[13] = (sum >> 8) & 0xFF;
    packet[14] = sum & 0xFF;

    send_fpm_packet(packet, sizeof(packet));
    return fpm_wait_for_ack(FPM_OK);
}

bool check_fingerprint_match() { // this function check for finger print matches for authentication
    ESP_LOGI("FPM", "Place finger on sensor to authenticate...");

    for (int attempt = 0; attempt < 3; attempt++) {
        // Step 1: Generate fingerprint image
        fpm_gen_image();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Step 2: Convert image to character file (Buffer 1)
        fpm_gen_char(BUFFER_ID_1);
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Step 3: Search for match in library (range 0-99)
        fpm_search(BUFFER_ID_1, 0, 100);
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Step 4: Wait for success response
        if (fpm_wait_for_ack(FPM_OK)) {
            ESP_LOGI("FPM", "Fingerprint matched.");
            ssd1306_display_text(&dev, 0, "Access Granted!", 15, false);
            unlock_solenoid();
            return true;
        } else {
            ESP_LOGW("FPM", "No match found.");
            ssd1306_display_text(&dev, 0, "Access Denied try again!", 24, false);
            if (attempt == 0) {
                ESP_LOGW("FPM", "Retrying...");
            }
        }
    }

    ESP_LOGE("FPM", "Finger print authentication failed after retries.");
    return false;
}