/**
 * Joel Brigida
 * June 7, 2023
 * ESP32 Multicore Demo: Uses 2 tasks on separate CPUs to blink the LED.
*/

#include <Arduino.h>

static const BaseType_t PRO_CPU = 0;
static const BaseType_t APP_CPU = 1;

static const TickType_t taskDelay = 500;
static const int LEDpin = 13;

static SemaphoreHandle_t binSemaphore;

void task0(void *param)                                                         // Runs on Core 0
{
    pinMode(LEDpin, OUTPUT);
    char string[25];

    for(;;)
    {
        xSemaphoreGive(binSemaphore);                                           // Notify the other task
        sprintf(string, "Task 0: Core #%d\n", xPortGetCoreID());
        Serial.print(string);
        vTaskDelay(taskDelay);
    }
}

void task1(void *param)                                                         // Runs on Core 1
{
    char string[25];

    for(;;)
    {
        xSemaphoreTake(binSemaphore, portMAX_DELAY);
        sprintf(string, "Task 1: Core #%d\n", xPortGetCoreID());
        Serial.print(string);
        digitalWrite(LEDpin, !digitalRead(LEDpin));                             // Toggle LED on/off
    }
}

void setup()
{
    binSemaphore = xSemaphoreCreateBinary();
    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.print("\n\n=>> FreeRTOS Multicore Blinky Demo <<=\n\n");

    xTaskCreatePinnedToCore(
        task0,
        "CPU 0 Task",
        1536,
        NULL,
        1,
        NULL,
        PRO_CPU
    );

    xTaskCreatePinnedToCore(
        task1,
        "CPU 1 Task",
        1536,
        NULL,
        1,
        NULL,
        APP_CPU
    );

    vTaskDelete(NULL);
}

void loop() {}