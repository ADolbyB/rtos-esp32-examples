/**
 * Joel Brigida
 * May 27, 2023
 * NOTE: FreeRTOS Software Timer API reference: https://freertos.org/FreeRTOS-Software-Timer-API-Functions.html
 * This is a simple demo of a one-shot timer and an auto-reload timer.
 */

#include <Arduino.h>
//#include <timers.h>

#if CONFIG_FREERTOS_UNICORE                                             // Use only Core 1
    static const BaseType_t app_cpu = 0;    
#else
    static const BaseType_t app_cpu = 1;
#endif

static TimerHandle_t oneShotTimer = NULL;                               // Declare timer objects
static TimerHandle_t autoReloadTimer = NULL;

void timerCallbacks(TimerHandle_t xTimer);                              // Function Declaration

void setup()
{
    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("\n=>> FreeRTOS Timer Demo <<=\n");

    oneShotTimer = xTimerCreate(                                        // Instantiate One-Shot Timer
        "One-Shot Timer",                                               // Timer Name
        2000 / portTICK_PERIOD_MS,                                      // Timer Period (2 seconds)
        pdFALSE,                                                        // Auto-Reload (disabled)
        (void *)0,                                                      // Timer ID (0)
        timerCallbacks                                                  // Callback Function
    );

    autoReloadTimer = xTimerCreate(                                     // Instantiate Auto Reload Timer
        "Auto-Reload Timer",                                            // Timer Name
        1000 / portTICK_PERIOD_MS,                                      // Timer Period (1 second)
        pdTRUE,                                                         // Auto-Reload (Enabled)
        (void *)1,                                                      // Timer ID (1)
        timerCallbacks                                                  // Callback Function
    );

    if(oneShotTimer == NULL)                                            // Check to make sure timers are created
    {
        Serial.println("Failed To Create One-Shot Timer");
    }
    else if(autoReloadTimer == NULL)
    {
        Serial.println("Failed To Create Auto-Reload Timer");
    }
    else // Timers were created
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        Serial.println("*** Starting Timers ***");

        xTimerStart(oneShotTimer, portMAX_DELAY);                       // Start timers
        xTimerStart(autoReloadTimer, portMAX_DELAY);                    // Wait if command queue is full
    }

    vTaskDelete(NULL);                                                  // Self Delete Setup & Loop: Timers will still work w/ no user tasks
}

void loop() {}

void timerCallbacks(TimerHandle_t xTimer)
{
    if((uint32_t)pvTimerGetTimerID(xTimer) == 0)
    {
        Serial.println("One-Shot Timer Expired");                       
    }

    if((uint32_t)pvTimerGetTimerID(xTimer) == 1)
    {
        Serial.println("Auto-Reload Timer Expired");
    }
}