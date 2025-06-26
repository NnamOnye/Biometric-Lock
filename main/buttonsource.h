/*
 * buttonsource.h
 *
 *  Created on: Jun 6, 2025
 *      Author: Owner
 */

#ifndef MAIN_BUTTONSOURCE_H_
#define MAIN_BUTTONSOURCE_H_


#include "esp_err.h"

void run_first_time_setup(void);
void get_password_input(char *buffer);
esp_err_t save_password(const char *password);
void check_entered_password(const char *input);
void check_for_savedpass(char *buffer, size_t buffer_len);
void handle_key_action(int key);
void screen_on(void);
void configure_buttons(void);
void factory_reset(void);
void try_factory_reset(void);
void unlock_solenoid(void);
void scan_keypad(void);
void power_manager(void);


#endif /* MAIN_BUTTONSOURCE_H_ */
