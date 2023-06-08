/**
 * Joel Brigida
 * June 7, 2023
 * ESP32 Multicore demo code.
 * Note that using the `badIdeaDelay()` results in CPU reset due to WDT timeout.
 * This timeout happens because the scheduler on CPU0 is blocked while the function is running.
 * Results:
 * Using `tskNO_AFFINITY` with `vTaskDelay()` results in only CPU0 running both tasks.
 * Using `tskNO_AFFINITY` with `badIdeaDelay()` results in Task H using CPU0, Task L using CPU1.
 * Pinning Task H to CPU0 & Task L to CPU1 with vTaskDelay() results in Task H using CPU0, Task L using CPU1.
*/

#include <Arduino.h>

static const BaseType_t PRO_CPU = 0;                                            // Core 0 = Protocol CPU (WiFi/BT Stack)
static const BaseType_t APP_CPU = 1;                                            // Core 1 = Application CPU

static const TickType_t timeDelay = 500;
static const TickType_t timeDelay2 = 200;

static void badIdeaDelay(uint32_t ms)                                           // Hack to block Task Scheduler
{
    uint32_t i, j;
    for(i = 0; i < ms; i++)
    {
        for(j = 0; j < 40000; j++)
        {
            asm("nop");                                                         // Inline Assembly: No Operation
        }
    }
}

void taskL(void *param)
{
    char string[20];

    for(;;)
    {
        sprintf(string, "Task L: Core #%d\n", xPortGetCoreID());
        Serial.print(string);

        vTaskDelay(timeDelay);
        //badIdeaDelay(timeDelay2);
    }
}

void taskH(void *param)
{
    char string2[20];

    for(;;)
    {
        sprintf(string2, "Task H: Core #%d\n", xPortGetCoreID());
        Serial.print(string2);

        vTaskDelay(timeDelay);
        //badIdeaDelay(timeDelay2);
    }
}

void setup()
{
    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.print("\n\n=>> FreeRTOS Multicore Demo <<=\n");

    xTaskCreatePinnedToCore(
        taskL,
        "Low Pri Task",
        2048,
        NULL,
        1,
        NULL,
        APP_CPU                                                                 // pin to core 1
        //tskNO_AFFINITY                                                        // Can use any available core
    );

    xTaskCreatePinnedToCore(
        taskH,
        "High Pri Task",
        2048,
        NULL,
        2,
        NULL,
        PRO_CPU                                                                 // pin to core 0
        //tskNO_AFFINITY                                                        // Can use any available core
    );

    vTaskDelete(NULL);                                                          // Delete setup() & Loop()
}

void loop() {}