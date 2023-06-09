/**
 * Joel Brigida
 * June 8, 2023
 * ESP32 Multicore ISR Interrupt Demo/Skeleton. This version does not crash.
 * Note that if this was not a demo, the `vTaskDelay()` functions would be replaced
 * with data operations or other functionality.
*/

#include <Arduino.h>

static const BaseType_t PRO_CPU = 0;                                            // Protocol CPU
static const BaseType_t APP_CPU = 1;                                            // Application CPU

static const uint16_t timerDivider = 80;                                        // 1 MHz HW Timer
static const uint64_t timerMaxCount = 1000000;                                  // 1 Sec ISR interval

static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t mutex;
static hw_timer_t *timer = NULL;

void IRAM_ATTR timedISRroutine()                                                // Executes When Timer Reaches Max & Resets
{
    BaseType_t taskWoken = pdFALSE;
    TickType_t timestamp;
    char string[50];

    timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;

    Serial.print("ISR Running...\n");
    xSemaphoreTakeFromISR(mutex, &taskWoken);

    // Perform ISR Operations here...

    if(taskWoken)                                                               // if condition == pdTRUE
    {
        portYIELD_FROM_ISR();                                                   // Exit from ISR (ESP-IDF)
    }
    //portYIELD_FROM_ISR(taskWoken);                                            // Vanilla FreeRTOS
}

void taskL(void *param)                                                         // Low Priority
{
    TickType_t timestamp;
    char string[50];

    for(;;)
    {
        timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
        sprintf(string, "Task L Running on Core #%d...\n", xPortGetCoreID());
        Serial.print(string);
        
        // Perform Some Function Here instead of a Delay
        vTaskDelay(500 / portTICK_PERIOD_MS);

        sprintf(string, "Task L Finished in %d ms\n", (xTaskGetTickCount() * portTICK_PERIOD_MS) - timestamp);
        Serial.print(string);
    }
}

void taskH(void *param)
{
    TickType_t timestamp;
    char string[50];

    for(;;)
    {
        timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
        sprintf(string, "Task H Running on Core #%d...\n", xPortGetCoreID());
        Serial.print(string);
        
        portENTER_CRITICAL(&spinlock);
        Serial.print("Spinning in Task H...\n");
        portEXIT_CRITICAL(&spinlock);
        
        // Perform Some Function Here instead of a Delay
        vTaskDelay(500 / portTICK_PERIOD_MS);

        sprintf(string, "Task H Finished in %d ms\n", (xTaskGetTickCount() * portTICK_PERIOD_MS) - timestamp);
        Serial.print(string);
    }
}

void setup() 
{
    mutex = xSemaphoreCreateMutex();

    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.print("\n\nFreeRTOS Multicore ISR Timer Demo <<=\n\n");

    timer = timerBegin(0, timerDivider, true);                                  // Instantiate Timer w/ divider: startVal = 0, dividerVal = 80, countUp = True
    timerAttachInterrupt(timer, &timedISRroutine, true);                        // Attach ISR to timer function, risingEdge = True
    timerAlarmWrite(timer, timerMaxCount, true);                                // Trigger timer @ timerMaxCount, autoReload = True
    timerAlarmEnable(timer);                                                    // Allow ISR to trigger via timer

    Serial.print("ISR Timer Setup Done...\n\n");

    xTaskCreatePinnedToCore(                                                    // Task L runs on CPU 1
        taskL,
        "Low Pri Task",
        2048,
        NULL,
        1,
        NULL,
        APP_CPU
    );

    Serial.print("Task L Created...\n\n");

    xTaskCreatePinnedToCore(                                                    // Task H runs on CPU 0
        taskH,
        "High Pri Task",
        2048,
        NULL,
        2,
        NULL,
        PRO_CPU
    );

    Serial.print("Task H Created...\n\n");

    vTaskDelete(NULL);
}

void loop() {}