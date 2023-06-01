/**
 * Joel Brigida
 * May 19, 2023
 * This demonstration shows one task which blinks an LED at a set rate.
 * The 2nd task reads serial user input to change the delay of the LED blink rate,
 * then prints out the new blink rate to the terminal.
 */

#include <Arduino.h>
#include <stdlib.h>                         // used for `atoi()` function to read from serial.

/* Set ESP32 to use only core #1 in this demonstration */

#if CONFIG_FREERTOS_UNICORE
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;
#endif

static const uint8_t buf_len = 20;          // Buffer length setting for user input to serial terminal
static const int led_pin = LED_BUILTIN;     // Pin 13 for on-board LED.
static int led_delay = 500;                 // 500ms initial delay: changes with user input.

// 1st Task: blink LED on pin 13 at a rate set by a global variable
void toggleLED(void *parameter)             // Function Definition for Task 1
{
    for(;;)
    {
        digitalWrite(led_pin, HIGH);
        vTaskDelay(led_delay / portTICK_PERIOD_MS);
        digitalWrite(led_pin, LOW);
        vTaskDelay(led_delay / portTICK_PERIOD_MS);
    }
}

// 2nd Task: Read user input from Serial Terminal with `atoi()` function.
// Note that for Arduino Framework, `Serial.readString()` or `Serial.parseInt()` are also valid.
void readSerial(void *parameters)           // Function Definition for Task 2
{
    char input;                             // Each Character Input
    char buf[buf_len];                      // Array to hold user input characters
    uint8_t index = 0;                      // Number of characters entered by user.
    memset(buf, 0, buf_len);                // Clear the buffer 1st

    for(;;)
    {
        if(Serial.available() > 0)          // Check If Serial Terminal is open.
        {
            input = Serial.read();          // Read user input from serial terminal
            if(input == '\n')               // Update delay only if Enter key is pressed.
            {
                led_delay = atoi(buf);
                Serial.print("New LED Delay = ");
                Serial.print(led_delay);
                Serial.println("ms");
                memset(buf, 0, buf_len);    // Clear buffer after user input read.
                index = 0;                  // Reset index
            }
            else
            {
                if(index < buf_len - 1)     // Only append if index < message limit.
                {
                    buf[index] = input;
                    index++;
                }
            }
        }
    }
}

void setup() 
{
    pinMode(led_pin, OUTPUT);               // Configure LED as output.

    Serial.begin(115200);                   // Open Serial Monitor
    vTaskDelay(1000 / portTICK_PERIOD_MS);  // 1 second Delay

    Serial.println("\n\n\t=>> Multi-Task LED Demo <<=");
    Serial.println("Enter # of milliseconds to change LED Delay: ");

    xTaskCreatePinnedToCore(                // Start Blink Task
        toggleLED,                          // Function Called
        "Toggle LED",                       // Name of Task
        1024,                               // Stack size in bytes
        NULL,                               // Parameter passed to function
        1,                                  // Task Priority
        NULL,                               // Task Handle
        app_cpu                             // Core to run task on (Core #1)
    );

    xTaskCreatePinnedToCore(                // Start Blink Task
        readSerial,                         // Function Called
        "Read Serial",                      // Name of Task
        1024,                               // Stack size in bytes
        NULL,                               // Parameter passed to function
        1,                                  // Task Priority
        NULL,                               // Task Handle
        app_cpu                             // Core to run task on (Core #1)
    );
}

void loop() {
    // Execution will not get here
}