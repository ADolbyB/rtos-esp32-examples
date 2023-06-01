/**
 * Joel Brigida
 * May 27, 2023
 * NOTE: FreeRTOS Software Timer API reference: https://freertos.org/FreeRTOS-Software-Timer-API-Functions.html
 * This is a simple demo of a timer that tunrs off the LED 5 seconds after the user stops
 * entering information in the User CLI Terminal.
 * This is a useful function that can be reused for an LED display backight timer.
 */

#include <Arduino.h>
//#include <timers.h>                                                   // Only for Vanilla FreeRTOS         

#if CONFIG_FREERTOS_UNICORE
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;                                // Use only CPU core 1
#endif

static const TickType_t dimmerDelay = 5000 / portTICK_PERIOD_MS;        // LED on for 5 seconds
static const int ledPin = LED_BUILTIN;                                  // Assign LED to pin 13
static TimerHandle_t oneShotTimer = NULL;                               // Declare one-shot timer                   

void autoDimmerCallback(TimerHandle_t xTimer);                          // Callback Functions
void userCLI(void *parameters);

void setup() 
{
    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("\n=>> FreeRTOS CLI LED Timer Demo <<=\n");

    oneShotTimer = xTimerCreate(                                        // instantiate timer
        "One-shot timer",                                               // Name of timer
        dimmerDelay,                                                    // 5 second timer
        pdFALSE,                                                        // Disable Auto-reload
        (void *)0,                                                      // Timer ID (0)
        autoDimmerCallback                                              // Callback function
    );  

    xTaskCreatePinnedToCore(                                            // Instantiate Task
        userCLI,                                                        // Callback Function
        "User CLI Terminal",                                            // Function Name
        1536,                                                           // Stack Size (bytes)
        NULL,                                                           // Does not accept arguments
        1,                                                              // Priority
        NULL,                                                           // Task Handle
        app_cpu                                                         // Runs on CPU #1
    );
    
    vTaskDelete(NULL);                                                  // Self Delete setup() and loop()
}

void loop() {}

void autoDimmerCallback(TimerHandle_t xTimer) 
{
    digitalWrite(ledPin, LOW);                                          // Turn off LED when timer expires
}

void userCLI(void *parameters)
{
    char input;                                                         // each user input character
    pinMode(ledPin, OUTPUT);                                            // Configure LED as output

    for(;;)
    {
        if (Serial.available() > 0)                                     // Check if serial terminal available
        {
            input = Serial.read();
            Serial.print(input);                                        // echo everythin back to Termnal

            digitalWrite(ledPin, HIGH);                                 // Turn on LED
            xTimerStart(oneShotTimer, portMAX_DELAY);                   // Start countdown. If timer is already running, this works like xTimerReset()
       }
    }
}