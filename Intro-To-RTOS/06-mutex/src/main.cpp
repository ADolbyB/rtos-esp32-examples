/**
 * Joel Brigida
 * May 21, 2023
 * Mutex: MUTual EXclusion. These kernel objects help to eliminate "race conditions" when
 * working with global scope variables used by multiple tasks.
 * NOTE: FreeRTOS Semaphore/Mutex API reference: https://freertos.org/a00113.html
 */

#include <Arduino.h>
// #include <semphr.h>                                          // Semaphore header file only for Vanilla FreeRTOS

#if CONFIG_FREERTOS_UNICORE                                     // Use Core 1 only.
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;
#endif

static int shared_var = 0;                                      // Shared variable between tasks
static SemaphoreHandle_t mutex;                                 // Declare global mutex

void incrementTask(void *parameters)
{
    int local_variable;

    for(;;)                                                     // function increments a variable
    {
        if(xSemaphoreTake(mutex, 0) == pdTRUE)                  // Attempt to take mutex
        {
        
            local_variable = shared_var;
            local_variable++;
            vTaskDelay(random(250, 750) / portTICK_PERIOD_MS);  // Random delay between 250 - 750ms
            shared_var = local_variable;

            Serial.print("New Value: ");                        // Print variable before giving back mutex
            Serial.println(shared_var);                         // This avoids some other task changing it in the interim

            xSemaphoreGive(mutex);                              // Give back mutex after data opoeration

        }
        else
        {
            // We can do some other task while waiting for the mutex.
        }
    }
}

void setup()
{
    randomSeed(analogRead(0));                                  // pseudorandom hack
    
    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    Serial.println("\n\n\t=>> FreeRTOS Mutex Race Condition Demo <<=");

    mutex = xSemaphoreCreateMutex();                            // Instantiate mutex before task

    xTaskCreatePinnedToCore(                                    // Both tasks increment the same global variable
        incrementTask,
        "Increment Task 1",
        1024,
        NULL,
        1,
        NULL,
        app_cpu
    );

    xTaskCreatePinnedToCore(                                    // Both tasks run the same function
        incrementTask,
        "Increment Task 2",
        1024,
        NULL,
        1,
        NULL,
        app_cpu
    );

    vTaskDelete(NULL);                                          // Delete setup() & loop() task

}

void loop() {}