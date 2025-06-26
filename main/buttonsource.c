/*
 * buttonsource.c
 *
 *  Created on: Apr 3, 2025
 *      Author: M.O
 */
//#include<stdio.h>
#include<driver/gpio.h>
//#include "driver/i2c.h"
#include "esp_sleep.h"
#include "soc/gpio_num.h"
#include "ssd1306.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "driver/uart.h"
#include<esp_log.h>
//#include "i2c_master.h"
#include"driver/i2c_master.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdio.h>
#include"fingersource.h"

#define ESP_LOGI(tag, format, ...)
#define ESP_LOGE(tag, format, ...)

#define NVS_NAMESPACE "storage"
#define NVS_KEY_PASSWORD "password"

#define CONFIG_OFFSETX 0  // or 2, or whatever the offset needs to be



//defining the prototypes bellow since some functions call functions that have not been declared yet at the time of specified function call you can reorganize if you like
void run_first_time_setup(void);
void get_password_input(char *buffer);
esp_err_t save_password(const char *password);
void check_entered_password(const char *input);
void check_for_savedpass(char *buffer, size_t buffer_len);
void handle_key_action(int key);
void scan_keypad();


void screen_on() 
{
    
    // Initialize the OLED display
    SSD1306_t dev;  // Define the SSD1306 display instance
    
    i2c_master_init(&dev, /*sda_gpio*/18 ,/*scl_gpio*/19,/*reset_gpio*/ -1);
    ssd1306_init(&dev, 126, 64); // Correct initialization

    // Clear the screen and display text
    ssd1306_clear_screen(&dev, false);
    ssd1306_display_text(&dev,2, "Hello!",6,false);  // Pass device handle and coordinates
    vTaskDelay(pdMS_TO_TICKS(4000));  // Wait 4 second
    ssd1306_clear_screen(&dev, false);
}

// Define row GPIO pins
#define ROW1_GPIO GPIO_NUM_1
#define ROW2_GPIO GPIO_NUM_2
#define ROW3_GPIO GPIO_NUM_3
#define ROW4_GPIO GPIO_NUM_4


// Define column GPIO pins
#define COL1_GPIO GPIO_NUM_34
#define COL2_GPIO GPIO_NUM_33
#define COL3_GPIO GPIO_NUM_26
#define COL4_GPIO GPIO_NUM_21


//Solenoid Bolt-->MOSFET GPIO Pin
#define BOLT_MOSFET_GPIO GPIO_NUM_10

//Long press for Clear button
#define LONG_PRESS_TIME_MS 2500  // Define long press time in ms

//Long press for RESET button
#define LONG_PRESS_RESET_MS 10000   // Define long press time in ms for reset

//Long press for factory reset
#define LONG_PRESS_FACTORY_RESET_MS 40000 // Define long press time in ms for factory reset


void configure_buttons() {
	//Row Configuration
    gpio_config_t row1_conf = {
    .pin_bit_mask = (1ULL << ROW1_GPIO),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .intr_type = GPIO_INTR_DISABLE
	};
	
	gpio_config_t row2_conf = {
	    .pin_bit_mask = (1ULL << ROW2_GPIO),
	    .mode = GPIO_MODE_INPUT,
	    .pull_up_en = GPIO_PULLUP_ENABLE,
	    .intr_type = GPIO_INTR_DISABLE
	};
	
	gpio_config_t row3_conf = {
	    .pin_bit_mask = (1ULL << ROW3_GPIO),
	    .mode = GPIO_MODE_INPUT,
	    .pull_up_en = GPIO_PULLUP_ENABLE,
	    .intr_type = GPIO_INTR_DISABLE
	};
	
	gpio_config_t row4_conf = {
	    .pin_bit_mask = (1ULL << ROW4_GPIO),
	    .mode = GPIO_MODE_INPUT,
	    .pull_up_en = GPIO_PULLUP_ENABLE,
	    .intr_type = GPIO_INTR_DISABLE
	};
	
	// Column configuration
	gpio_config_t col1_conf = {
	    .pin_bit_mask = (1ULL << COL1_GPIO),
	    .mode = GPIO_MODE_OUTPUT,
	    .pull_up_en = GPIO_PULLUP_DISABLE,
	    .intr_type = GPIO_INTR_DISABLE
	};
	
	gpio_config_t col2_conf = {
	    .pin_bit_mask = (1ULL << COL2_GPIO),
	    .mode = GPIO_MODE_OUTPUT,
	    .pull_up_en = GPIO_PULLUP_DISABLE,
	    .intr_type = GPIO_INTR_DISABLE
	};
	
	gpio_config_t col3_conf = {
	    .pin_bit_mask = (1ULL << COL3_GPIO),
	    .mode = GPIO_MODE_OUTPUT,
	    .pull_up_en = GPIO_PULLUP_DISABLE,
	    .intr_type = GPIO_INTR_DISABLE
	};
	
	gpio_config_t col4_conf = {
	    .pin_bit_mask = (1ULL << COL4_GPIO),
	    .mode = GPIO_MODE_OUTPUT,
	    .pull_up_en = GPIO_PULLUP_DISABLE,
	    .intr_type = GPIO_INTR_DISABLE
	};
	
	
	
	//SOLENOID BOLT--> MOSFET CONFIGURATION
	gpio_config_t mosfet_conf = {
	    .pin_bit_mask = (1ULL << BOLT_MOSFET_GPIO),
	    .mode = GPIO_MODE_INPUT,
	    .pull_up_en = GPIO_PULLUP_DISABLE,
	    .intr_type = GPIO_INTR_POSEDGE
	};
	gpio_set_level(BOLT_MOSFET_GPIO, 0);
	
    
    //Row pin GPIO CONFIG
    gpio_config(&row1_conf);
    gpio_config(&row2_conf);
    gpio_config(&row3_conf);
    gpio_config(&row4_conf);
    
    
    //Column pin GPIO CONFIG
    gpio_config(&col1_conf);
	gpio_config(&col2_conf);
	gpio_config(&col3_conf);
	gpio_config(&col4_conf);
    
    
    //Bolt --> Mosfet configuration
    gpio_config(&mosfet_conf);
    
}



esp_err_t save_password(const char *password) {  
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    err = nvs_set_str(nvs, "password", password);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

esp_err_t load_password(char *buffer, size_t len) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    size_t required_size = len;
    err = nvs_get_str(nvs_handle, NVS_KEY_PASSWORD, buffer, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI("NVS", "Password loaded: %s", buffer);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW("NVS", "Password not found in NVS.");
    } else {
        ESP_LOGE("NVS", "Error reading password: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    return err;
}


void run_first_time_setup() { // call this function in main only if neccesary or if needed
	 char input_buffer[32] = {0};
     SSD1306_t dev;
     const int max_attempts = 3;
     int attempts = 0;
     
     while (attempts < max_attempts) {
		
		ssd1306_clear_screen(&dev, false);
    	ssd1306_display_text(&dev, 2, "Set Password", 12, false);
    	vTaskDelay(pdMS_TO_TICKS(1000));

    	get_password_input(input_buffer);  // blocks until password entered
    	save_password(input_buffer);

    	if (save_password(input_buffer) == ESP_OK) {
        	ESP_LOGI("SETUP", "Password set.");
        	ssd1306_display_text(&dev, 4, "Password Created", 14, false);
        	vTaskDelay(pdMS_TO_TICKS(3000));
        	
    	} else {
        	ESP_LOGE("SETUP", "Failed to set password/Finger Scan.");
        	ssd1306_display_text(&dev, 4, "Save Failed.", 11, false);
        	vTaskDelay(pdMS_TO_TICKS(4000));
        	
    	}
	 }
	 setup_new_fingerprint();// set outside of attempt loop because if user cannot set pass they can set finger print to factory reset.
	 
	 attempts++; //IF SETUP FAILS, USER WILL HAVE ONLY 3 TRIES TO SETUP

    
}


void get_password_input(char *buffer) {
	extern void *memset(void *, int, unsigned int);
	void scan_keypad();
    int index = 0;
    char key = 0;

    // Clear display (optional)
    SSD1306_t dev;
    ssd1306_clear_screen(&dev, false);
    ssd1306_display_text(&dev, 2, "Enter Password:", 15, false);

    while (index < 9) {  // 9 digits max + null terminator
        handle_key_action(key);  // Implement or call your button/key handler

        if (key >= '0' && key <= '9') {
            buffer[index++] = key;
            buffer[index] = '\0';  // Keep string null-terminated

            // Feedback on screen (optional: mask as '*')
            char display[10];
            memset(display, '*', index);
            display[index] = '\0';
            

        } else if (key == 'E') {  // '#' = Enter key (or whatever your system uses)
            break;
        } else if (key == 'C') {  // '*' = Backspace (optional)
            if (index > 0) {
                index--;
                buffer[index] = '\0';

                // Update display
                char display[10];
                memset(display, '*', index);
                display[index] = '\0';
               
            }
        }
    }
}

void factory_reset() {
	
	SSD1306_t dev;
	
    ESP_LOGI("FACTORY", "Factory reset initiated...");

    // Step 1: Erase stored password (example using NVS)
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        nvs_erase_all(nvs);  // or nvs_erase_key(nvs, "password");
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    // Step 2: Erase finger print data (depends on module, e.g., R558)
    fpm_restore_factory();  // Call your module’s erase function

    // Step 3: Optional: Reset other user settings if needed...

    // Step 4: Reboot 
    ESP_LOGI("FACTORY", "Rebooting into clean state.");
    ssd1306_clear_screen(&dev, false);
    ssd1306_display_text(&dev,2, "Rebooting into clean state.",27,false);
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
}


void try_factory_reset() {
	
	
	
	extern int strcmp(const char *, const char *);

	char input_buffer[32] = {0};  // Assuming max 10 digits
	int input_index = 0;    // To track the index for the next digit
	//static uint32_t press_start_time = 0;
	input_buffer[input_index] = '\0';  // Null-terminate the string
	
	SSD1306_t dev;  // Define the SSD1306 display instance
	ssd1306_clear_screen(&dev, false);
    ssd1306_display_text(&dev,2, "ENTER PASSWORD OR SCAN FINGER!",30,false); //screen prompt for user to enter password for factory reset confirm
	
    if (strcmp(input_buffer,input_buffer) == 0 /*|| verify_fingerprint()*/) {
        // You might show a prompt like: "Confirm Factory Reset? Press ENTER"
        vTaskDelay(pdMS_TO_TICKS(3000));
        factory_reset();
    } else {
		
        ESP_LOGE("FACTORY", "Unauthorized attempt to factory reset!");
    }
}


void unlock_solenoid() {
	
	 
	
    ESP_LOGI("LOCK", "Unlocking...");
    gpio_set_level(BOLT_MOSFET_GPIO, 1);  // Turn ON solenoid
    vTaskDelay(pdMS_TO_TICKS(10000));  // Keep unlocked for 10 seconds
    gpio_set_level(BOLT_MOSFET_GPIO, 0);  // Turn OFF solenoid
    ESP_LOGI("LOCK", "Locked.");
}



void scan_keypad() {
    // Loop through all columns
    for (int col = 0; col < 4; col++) {
        // Set all columns to high (disable them)
        gpio_set_level(COL1_GPIO, 1);
        gpio_set_level(COL2_GPIO, 1);
        gpio_set_level(COL3_GPIO, 1);
        gpio_set_level(COL4_GPIO, 1);

        // Set one column to low (enable it)
        switch (col) {
            case 1:
                gpio_set_level(COL1_GPIO, 0);
                break;
            case 2:
                gpio_set_level(COL2_GPIO, 0);
                break;
            case 3:
                gpio_set_level(COL3_GPIO, 0);
                break;
            case 4:
                gpio_set_level(COL4_GPIO, 0);
                break;
        }

        // Check if any row is pressed (low level)
        if (gpio_get_level(ROW1_GPIO) == 0) {
            printf("Button in column %d, row 1 pressed\n", col);
        }
        if (gpio_get_level(ROW2_GPIO) == 0) {
            printf("Button in column %d, row 2 pressed\n", col);
        }
        if (gpio_get_level(ROW3_GPIO) == 0) {
            printf("Button in column %d, row 3 pressed\n", col);
        }
        if (gpio_get_level(ROW4_GPIO) == 0) {
            printf("Button in column %d, row 4 pressed\n", col);
        }
    }
}


void handle_key_action(int key) {
	
	
	char input_buffer[32] = {0};
	
	get_password_input(input_buffer);
	
    // Define key mapping based on row and column
    char keypad_map[4][4] = {
        {'1', '2', '3', '4'},
        {'5', '6', '7', '8'},
        {'9', '0', 'E', 'C'},
        {'R', 'e', ' ', ' '}//small e for empty button with no writing 
    };

    if (key != -1) {
        char pressed_button = keypad_map[key / 4][key % 4];  // Map key to character
		SSD1306_t dev;// to Define the SSD1306 display instance dev/&dev
        // Perform actions based on the button pressed
        
        char input_buffer[10] = {0};
        char input_number[10];  // Assuming max 10 digits
		int input_index = 0;    // To track the index for the next digit
		static uint32_t press_start_time = 0;  // Declare globally
		static bool button_pressed = false;    // To track if the button is being pressed
        switch (pressed_button) {
			
            case '1':
                // Action for 1
                printf("Button 1 pressed\n");
                ssd1306_display_text(&dev, 0, "1", 1, false);
                input_number[input_index++] = '1';  // Append '1' to input_number
    			input_number[input_index] = '\0';  // Null-terminate the string
                break;
                
            case '2':
                // Action for 2
                printf("Button 2 pressed\n");
                ssd1306_display_text(&dev,0, "2", 1, false);
                input_number[input_index++] = '2';  // Append '2' to input_number
    			input_number[input_index] = '\0';  // Null-terminate the string
                break;
                
            case '3':
                // Action for 3
                printf("Button 3 pressed\n");
                ssd1306_display_text(&dev, 0, "3", 1, false);
                input_number[input_index++] = '3';  // Append '3' to input_number
    			input_number[input_index] = '\0';  // Null-terminate the string
                break;
                
            case '4':
                // Action for 4
                printf("Button 4 pressed\n");
                ssd1306_display_text(&dev, 0, "4", 1, false);
                input_number[input_index++] = '4';  // Append '4' to input_number
    			input_number[input_index] = '\0';  // Null-terminate the string
                break;
                
            case '5':
                // Action for 5
                printf("Button 5 pressed\n");
                ssd1306_display_text(&dev, 0, "5", 1, false);
                input_number[input_index++] = '5';  // Append '5' to input_number
    			input_number[input_index] = '\0';  // Null-terminate the string
                break;
                
            case '6':
                // Action for 6
                printf("Button 6 pressed\n");
                ssd1306_display_text(&dev, 0, "6", 1, false);
                input_number[input_index++] = '6';  // Append '6' to input_number
    			input_number[input_index] = '\0';  // Null-terminate the string
                break;   
       	
            case '7':
                // Action for 7
                printf("Button 7 pressed\n");
                ssd1306_display_text(&dev, 0, "7", 1, false);
                input_number[input_index++] = '7';  // Append '7' to input_number
    			input_number[input_index] = '\0';  // Null-terminate the string
                break;
                
            case '8':
                // Action for 8
                printf("Button 8 pressed\n");
                ssd1306_display_text(&dev, 0, "8", 1, false);
                input_number[input_index++] = '8';  // Append '8' to input_number
    			input_number[input_index] = '\0';  // Null-terminate the string
                break;
                
            case '9':
                // Action for 9
                printf("Button 9 pressed\n");
                ssd1306_display_text(&dev, 0, "9", 1, false);
                input_number[input_index++] = '9';  // Append '9' to input_number
    			input_number[input_index] = '\0';  // Null-terminate the string
                break;
                
            case '0':
                // Action for 0
                printf("Button 0 pressed\n");
                ssd1306_display_text(&dev, 0, "0", 1, false);
                input_number[input_index++] = '0';  // Append '0' to input_number
    			input_number[input_index] = '\0';  // Null-terminate the string
                break;
             
                
            case 'E':
                // Action for E
                printf("Button Enter pressed\n");
                
				input_number[input_index] = '\0';  // Null-terminate the string
				
				void check_for_savedpass(char *input_buffer, size_t buffer_len);
				
				void check_entered_password(const char *input);
				
				
			
				
				break;
                
            case 'C':
                // Action for C
                printf("Button Clear pressed\n");
                //ssd1306_display_text(&dev, 0, "CLEAR", 1, false);
                
               static bool clear_pressed = false;
               if (key == 'C' && !clear_pressed) {
		    	// Button just pressed
		       press_start_time = xTaskGetTickCount();
		       clear_pressed = true;
			   }
			   
			   if (key != 'C' && clear_pressed) {
			    // Button just released
			    uint32_t press_duration = xTaskGetTickCount() - press_start_time;
			    clear_pressed = false;
			
				    if (press_duration < pdMS_TO_TICKS(LONG_PRESS_TIME_MS)) {
				      // Short press: Delete last digit
				    	if (input_index > 0) {
				    		input_number[--input_index] = '\0';  // Remove last character
							printf("Current input: %s\n", input_number);  // For debugging
				        }
				    }
				    if (press_duration >= pdMS_TO_TICKS(LONG_PRESS_TIME_MS)) {
				       // Long press: Clear the screen
				        ssd1306_clear_screen(&dev, true);
				    } 
			   }   
                break;
                
            case 'R':
                // Action for R
                printf("Button Reset pressed\n");
                ssd1306_display_text(&dev, 0, "RESET", 1, false);
                if (key == 'R') {  // Assuming 'R' is the "RESET" button
				   
					press_start_time = xTaskGetTickCount();  // Capture start time when button is pressed
					uint32_t press_duration = xTaskGetTickCount() - press_start_time;  // Calculate how long the button has been pressed
					
					if (press_duration >= LONG_PRESS_RESET_MS) {
					  // Long press: Restart system
					    esp_restart();
					}
				}  
                break;
                
            case 'e':
                // Action for e
                printf("Button empty pressed\n");
                //ssd1306_display_text(&dev, 0, "Empty", 1, false);
                if (key == 'e') {  // Assuming 'e' is the "empty" button
				   
					press_start_time = xTaskGetTickCount();  // Capture start time when button is pressed
					uint32_t press_duration = xTaskGetTickCount() - press_start_time;  // Calculate how long the button has been pressed
					
					if (press_duration >= LONG_PRESS_FACTORY_RESET_MS) {
					  // Long press: FACTORY RESET system
					    try_factory_reset();
					}
				}  
                break;
                
                
             // Continue for other buttons...
            default:
                printf("Unknown key pressed\n");
                break;
        }
    }
    
}



void check_entered_password(const char *input) { // this is to check if the entered pass matches any of the saved passwords for authentication

	SSD1306_t dev;
	
    char saved_password[32] = {0};
    esp_err_t err = load_password(saved_password, sizeof(saved_password));
    
    if (err == ESP_OK) {
        if (strcmp(input, saved_password) == 0) {
            ESP_LOGI("AUTH", "Password correct!");
            ssd1306_display_text(&dev, 0, "Access Granted!", 15, false);
            unlock_solenoid();  // or trigger next step
        } else {
            ESP_LOGE("AUTH", "Incorrect password.");
            ssd1306_display_text(&dev, 0, "Access Denied!", 15, false);
            // Optional: Add retry limit, delay, or error tone
        }
    } else {
        ESP_LOGE("AUTH", "Failed to read stored password.");
        ssd1306_display_text(&dev, 0, "Failed to read stored password,use finger-print scanner!", 56, false);
    }
}

void check_for_savedpass(char *input_buffer, size_t buffer_len){ //this is to check if any password are presently saved if not it will run a startup sequence so the user can create a pass and finger print

	SSD1306_t dev;
	
	char saved_password[32] = {0};
    esp_err_t err = load_password(saved_password, sizeof(saved_password));

    bool password_missing = (err != ESP_OK || strlen(saved_password) == 0);
    bool fingerprint_missing = !fpm_check_library_contains(FINGERPRINT_ID);
	const int max_attempts = 3;
    int attempts = 0;
    
    while (attempts < max_attempts){
		
		if (password_missing) {
        ESP_LOGW("CHECK", "No saved password found.");
        ssd1306_clear_screen(&dev, false);
    	ssd1306_display_text(&dev,4, "No saved password found",23,false);
    	vTaskDelay(pdMS_TO_TICKS(3000)); 
    	ssd1306_clear_screen(&dev, false);
    	ssd1306_display_text(&dev,4, "create password...",18,false);
    	vTaskDelay(pdMS_TO_TICKS(3000));
    	ssd1306_clear_screen(&dev, false); 
        get_password_input(input_buffer);            // Ask user for new password
        save_password(input_buffer);                 // Save it
        
    	}

	    if (fingerprint_missing) {
	        ESP_LOGW("CHECK", "No fingerprint enrolled.");
	        ssd1306_clear_screen(&dev, false);
	    	ssd1306_display_text(&dev,4, "No finger print enrolled",24,false);
	    	vTaskDelay(pdMS_TO_TICKS(3000)); 
	    	ssd1306_clear_screen(&dev, false);
	    	ssd1306_display_text(&dev,4, "Enroll finger print...",23,false);
	    	vTaskDelay(pdMS_TO_TICKS(2000));
	    	ssd1306_clear_screen(&dev, false);
	        setup_new_fingerprint();                     // Enroll fingerprint
	    }
	    attempts++;
	}
	
	if (attempts >= max_attempts) {
        ESP_LOGE("No pass or finger print saved", "Setup failed after %d attempts.", max_attempts);
        ssd1306_clear_screen(&dev, false);
        ssd1306_display_text(&dev, 3, "no pass or finger scan saved.", 29, false);
        ssd1306_clear_screen(&dev, false);
        ssd1306_display_text(&dev, 3, "Setup failed. Rebooting.", 24, false);
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    }
}
    







#define WAKEUP_MASK  ((1ULL << ROW1_GPIO) | (1ULL << ROW2_GPIO) | (1ULL << ROW3_GPIO) | (1ULL << ROW4_GPIO) | \
                      (1ULL << COL1_GPIO) | (1ULL << COL2_GPIO) | (1ULL << COL3_GPIO) | (1ULL << COL4_GPIO)|(1ULL << TOUCHOUT_GPIO))
                      //This is a bitmask that includes all the GPIOs of the keypad matrix (rows and columns

void power_manager(){
	
	SSD1306_t dev;
	const int sleep_delay_sec = 180;  // 3 minutes
    bool cancel_sleep = false;
    
    for (int i = sleep_delay_sec; i > 0; i--) {
		
        // Check if any key GPIO is LOW (pressed)
        if ((gpio_get_level(ROW1_GPIO) == 0) || (gpio_get_level(ROW2_GPIO) == 0) || 
            (gpio_get_level(ROW3_GPIO) == 0) || (gpio_get_level(ROW4_GPIO) == 0) ||
            (gpio_get_level(COL1_GPIO) == 0) || (gpio_get_level(COL2_GPIO) == 0) ||
            (gpio_get_level(COL3_GPIO) == 0) || (gpio_get_level(COL4_GPIO) == 0)||(gpio_get_level(TOUCHOUT_GPIO) == 0)) {
            cancel_sleep = true;
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));  // Wait 1 second
    }
	if (cancel_sleep == true) {
        //ssd1306_display_text(&dev, 2, "Sleep cancelled", 15, false);
        vTaskDelay(pdMS_TO_TICKS(2000));
        //ssd1306_clear_screen(&dev, false);
        return;
    }
	
	
	
	esp_sleep_enable_ext1_wakeup(WAKEUP_MASK, ESP_EXT1_WAKEUP_ANY_LOW);//using ESP_EXT1_WAKEUP_ALL_LOW instead of ESP_EXT1_WAKEUP_ALL_HIGH since i have internal pull up enabled on button config, buttons are detected when low. 
	
	printf("Going to sleep now.");
	ssd1306_display_text(&dev,2, "Going to sleep now.",21,false);
	
	
 	vTaskDelay(pdMS_TO_TICKS(2000));  // Delay for 2 seconds before clearing screen
	
	
	
	esp_deep_sleep_start(); //puts the ESP32 into deep sleep, awaiting a wake-up event 
	
}