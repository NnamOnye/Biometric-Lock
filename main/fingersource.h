/*
 * fingersource.h
 *
 *  Created on: Jun 6, 2025
 *      Author: Owner
 */

#ifndef MAIN_FINGERSOURCE_H_
#define MAIN_FINGERSOURCE_H_

//Define Finger Print Sensor --> RS558-S
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include <strings.h>
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

void fpm_gen_image(void);
void fpm_match(void);
void fpm_init(void);
void send_fpm_packet(void);
uint16_t fpm_checksum(uint8_t *packet, size_t start_index, size_t len);
void fpm_gen_image(void);
void fpm_get_enroll_image(void);
void fpm_match(void);
void fpm_search(void);
void fpm_store_template(uint8_t buffer_id, uint16_t page_id);
void fpm_delete_template(uint16_t page_id, uint16_t num);
void fpm_set_password(uint32_t password);
void fpm_verify_password(uint32_t password);
void fpm_sleep(void);
void fpm_upload_image(void);
void fpm_load_char(void);
void fpm_read_sys_para(void);
void fpm_restore_factory(void);
void setup_new_fingerprint(void);
void init_fingerprint_uart(void);
esp_err_t enroll_fingerprint();
bool fpm_check_library_contains(uint16_t id);
bool check_fingerprint_match();

#endif /* MAIN_FINGERSOURCE_H_ */
