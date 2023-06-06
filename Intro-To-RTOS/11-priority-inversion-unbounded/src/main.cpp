/**
 * Joel Brigida
 * June 5, 2023
 * Priority Inversion Example
 * This is a demonstration of unbounded priority inversion similar to the original
 * Mars Sojourner rover mission on 1997 using the VxWorks RTOS system. This
 * priority inversion is a result of using binary semaphores. Using a Mutex in place
 * of the binary semaphore fixes this problem using priority inheritance.
*/

#include <Arduino.h>
//#include <semphr.h>                                                           // Use only CPU Core 1

#if CONFIG_FREERTOS_UNICORE
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;
#endif

#define UNBOUNDED_INVERSION                                                     // Uses Binary Semaphores
//#define INVERSION_FIX                                                           // Uses Mutexes

TickType_t critSecWait = 500;                                                   // Time (ms) needed in Critical Section
TickType_t medWait = 5000;                                                      // Time (ms) that Medium Task needs to do work.

static SemaphoreHandle_t lock;

void lowPriTaskL(void *param)                                                   // Task L: Low Priority
{
    TickType_t timestamp;

    for(;;)
    {
        Serial.print("\nTask L Trying To Take Lock...\n");
        timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
        xSemaphoreTake(lock, portMAX_DELAY);                                    // Take lock (or wait indefinitely until available)
    
        Serial.print("\nTask L Rec'd Lock. Spent ");
        Serial.print((xTaskGetTickCount() * portTICK_PERIOD_MS) - timestamp);
        Serial.print(" ms waiting for the lock. Working in Critical Section...\n");

        timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;                   
        while((xTaskGetTickCount() * portTICK_PERIOD_MS) - timestamp < critSecWait)
        {
            // perform some critical section operation here                     // Use CPU (simulate work)
        }

        Serial.print("\nTask L *DONE!* Releasing Lock...\n");
        xSemaphoreGive(lock);

        vTaskDelay(1500 / portTICK_PERIOD_MS);                                   // Put Task L to sleep
    }
}

void medPriTaskM(void *param)                                                   // Medium Priority Task
{
    TickType_t timestamp;

    for(;;)
    {
        Serial.print("\nTask M doing some work...\n");
        timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
        while((xTaskGetTickCount() * portTICK_PERIOD_MS) - timestamp < medWait)
        {
            // Do some medium priority task (non-critical section)
        }

        Serial.print("\nTask M *DONE!*\n");
        vTaskDelay(1500 / portTICK_PERIOD_MS);                                   // Put Task M to sleep
    }
}

void highPriTaskH(void *param)
{
    TickType_t timestamp;

    for(;;)
    {
        Serial.print("\nTask H Trying To Take Lock...\n");
        timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
        xSemaphoreTake(lock, portMAX_DELAY);                                    // Take Lock (or wait indefinitely until available)

        Serial.print("\nTask H Got Lock...Spent ");
        Serial.print((xTaskGetTickCount() * portTICK_PERIOD_MS) - timestamp);
        Serial.print(" ms Waiting For Lock. Now Doing Work...\n");

        timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
        while((xTaskGetTickCount() * portTICK_PERIOD_MS) - timestamp < critSecWait)
        {
            // Do High Priority Task (Critical Section)
        }

        Serial.print("\nTask H *DONE!* Releasing Lock...\n");
        xSemaphoreGive(lock);

        vTaskDelay(1500 / portTICK_PERIOD_MS);                                   // Put Task H to sleep
    }
}

void setup()
{
    #ifdef UNBOUNDED_INVERSION                                                  // Problem: using binary semaphores
        lock = xSemaphoreCreateBinary();
        xSemaphoreGive(lock);                                                   // Initialize lock to 1
    #endif

    #ifdef INVERSION_FIX                                                        // One Solution: Use Mutexes instead.
        lock = xSemaphoreCreateMutex();                                         // Mutex Starts at 1 automatically
    #endif

    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.print("\n\n=>> FreeRTOS Priority Inversion Demonstration <<=\n\n");

    xTaskCreatePinnedToCore(                                                    // Note that the order of tasks starting matters to force priority inversion
        lowPriTaskL,
        "Low Pri Task",
        1536,
        NULL,
        1,
        NULL,
        app_cpu
    );

    vTaskDelay(1 / portTICK_PERIOD_MS);                                         // Delay forces priority inversion

    xTaskCreatePinnedToCore(
        highPriTaskH,
        "High Pri Task",
        1536,
        NULL,
        3,
        NULL,
        app_cpu
    );

    xTaskCreatePinnedToCore(
        medPriTaskM,
        "Med Pri Task",
        1536,
        NULL,
        2,
        NULL,
        app_cpu
    );

    vTaskDelete(NULL);                                                          // Self delete setup() & loop() tasks
}

void loop() {}