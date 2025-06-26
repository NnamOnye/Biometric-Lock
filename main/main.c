#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include"buttonsource.h"
#include "esp_log.h"
#include"fingersource.h"
#include "freertos/idf_additions.h"
#include "ssd1306.h"
#include<driver/gpio.h>
#include "nvs_flash.h"

void app_main(void)
{
	// Initialize NVS (required for saving password/finger print config)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
	
	char input_buffer[32] = {0};
	screen_on();
	check_for_savedpass(input_buffer, sizeof(input_buffer)); // this function checks for save pass and if none save it will prompt for creation. In that event its acting as a first time setup function also.
	init_fingerprint_uart();
	run_first_time_setup();
	fpm_check_library_contains(8);
	check_fingerprint_match();
	try_factory_reset();
	fpm_sleep();
	power_manager();
	
	static TickType_t last_fingerprint_time = 0;
	TickType_t now = xTaskGetTickCount();
	
	while (true) {
    	
    	scan_keypad();
    	
    	handle_key_action(-1);
    	
	    // === 2. Check Fingerprint Sensor ===
	    if (gpio_get_level(TOUCHOUT_GPIO) == 0) {  // defined in fingersource.c
	    	check_fingerprint_match();
	    	/*if (check_fingerprint_match() && (now - last_fingerprint_time > pdMS_TO_TICKS(3000))) {
	    		unlock_solenoid();
	    		last_fingerprint_time = now;
			}*/ //---> uncomment this if statement if the lock repeatedly unlocks after access granted with finger print
	        //ESP_LOGE("AUTH", "Fingerprint access granted!");
	    }
	
	    // === 3. Small delay for polling stability ===
	    vTaskDelay(pdMS_TO_TICKS(100));  // ~100ms loop
	}
}
