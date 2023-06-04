/**
 * Joel Brigida
 * June 2, 2023
 * This is an example of a system deadlock solution using a rule where each task
 * must always take the lowest numbered mutex 1st no matter what. This is nearly
 * identical to the previous example 10a, except the order of taking mutexes has changed.
 * This is a more efficient solution since neither task times out waiting for a mutex.
*/

#include <Arduino.h>
//#include <semphr.h>                                                           // Only for Vanilla FreeRTOS

#if CONFIG_FREERTOS_UNICORE
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;                                        // Only use CPU core 1
#endif

TickType_t mtxTimeout = (1000 / portTICK_PERIOD_MS);                            // 1 sec Mutex Timeout
TickType_t taskDelay = (500 / portTICK_PERIOD_MS);                              // 0.5 sec Task Delays to simulate Work/Sleep

static SemaphoreHandle_t mutex1;
static SemaphoreHandle_t mutex2;

void highPriTaskA(void *param)                                                  // Task A has Priority 2
{
    for(;;)
    {
        if(xSemaphoreTake(mutex1, mtxTimeout) == pdTRUE)                        // Take mutex1
        {
            Serial.println("Task A Took Mutex 1...");
            vTaskDelay(1 / portTICK_PERIOD_MS);                                 // delay will force deadlock

            if(xSemaphoreTake(mutex2, mtxTimeout) == pdTRUE)                    // Take Mutex2
            {                     
                Serial.println("Task A Took Mutex 2...");
                Serial.println("Task A Working in Critical Section");           // simulated critical section
                vTaskDelay(taskDelay);                                          // delay simulates worktime
                xSemaphoreGive(mutex2);                                         // Give back mutexes in REVERSE order
                xSemaphoreGive(mutex1);
                Serial.println("Task A Released Both Mutexes: Going To Sleep");
            }
            else
            {
                xSemaphoreGive(mutex1);                                         // Not needed here, but left as a safety measure
                Serial.println("Task A Timed Out Waiting For Mutex 2 & Released Mutex1");
            }
        }
        else
        {
            Serial.println("Task A Timed Out Waiting For Mutex 1");
        }
        vTaskDelay(taskDelay);                                                  // Delay simulates sleep time
    }
}

void lowPriTaskB(void *param)                                                   // Task B has Priority 1
{
    for(;;)
    {
        if(xSemaphoreTake(mutex1, mtxTimeout) == pdTRUE)                        // Take mutex2
        {
            Serial.println("Task B Took Mutex 1...");
            vTaskDelay(1 / portTICK_PERIOD_MS);                                 // delay will force deadlock

            if(xSemaphoreTake(mutex2, mtxTimeout) == pdTRUE)                    // Take Mutex1
            {                     
                Serial.println("Task B Took Mutex 2...");
                Serial.println("Task B Working in Critical Section");           // simulated critical section
                vTaskDelay(taskDelay);                                          // delay simulates worktime
                xSemaphoreGive(mutex2);                                         // Give back mutexes in REVERSE order
                xSemaphoreGive(mutex1);
                Serial.println("Task B Released Both Mutexes: Going To Sleep");
            }
            else
            {
                xSemaphoreGive(mutex1);                                         // Not needed here, but left as a safety measure
                Serial.println("Task B Timed Out Waiting For Mutex 2 & Released Mutex1");
            }
        }
        else
        {
            Serial.println("Task B Timed Out Waiting For Mutex1");
        }
        vTaskDelay(taskDelay);                                                  // Delay simulates sleep time
    }
}

void setup()
{
    mutex1 = xSemaphoreCreateMutex();
    mutex2 = xSemaphoreCreateMutex();
    
    Serial.begin(115200);
    vTaskDelay(taskDelay);
    Serial.print("\n\n=>> FreeRTOS Deadlock Demo 2 <<=\n");

    xTaskCreatePinnedToCore(
        highPriTaskA,
        "Task A: Pri 2",
        1536,
        NULL,
        2,
        NULL,
        app_cpu
    );

    xTaskCreatePinnedToCore(
        lowPriTaskB,
        "Task B: Pri 1",
        1536,
        NULL,
        1,
        NULL,
        app_cpu
    );

    vTaskDelete(NULL);                                                          // Self Delete setup() & loop()
}

void loop() {}