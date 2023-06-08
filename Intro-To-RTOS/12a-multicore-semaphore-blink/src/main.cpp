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

    for(;;)
    {
        xSemaphoreGive(binSemaphore);                                           // Notify the other task
        vTaskDelay(taskDelay);
    }
}

void task1(void *param)                                                         // Runs on Core 1
{
    for(;;)
    {
        xSemaphoreTake(binSemaphore, portMAX_DELAY);
        digitalWrite(LEDpin, !digitalRead(LEDpin));                             // Toggle LED on/off
    }
}

void setup()
{
    binSemaphore = xSemaphoreCreateBinary();

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