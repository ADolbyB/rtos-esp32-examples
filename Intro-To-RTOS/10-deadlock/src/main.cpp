/**
 * Joel Brigida
 * June 2, 2023
 * This is an example of a system deadlock.
 * Each task requires 2 mutexes to run, but when each task takes 1
 * and waits indefinitely for the other, the program halts due to
 * deadlock. This is illustrated in the Serial Terminal output.
*/

#include <Arduino.h>
//# include <semphr.h>

#if CONFIG_FREERTOS_UNICORE
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;                                // Only use CPU core 1
#endif

static SemaphoreHandle_t mutex1;
static SemaphoreHandle_t mutex2;

void highPriTaskA(void *param)                                          // Task A has Priority 2
{
    for(;;)
    {
        xSemaphoreTake(mutex1, portMAX_DELAY);                          // Take mutex1
        Serial.println("Task A Took Mutex 1...");
        vTaskDelay(1 / portTICK_PERIOD_MS);                             // delay will force deadlock

        xSemaphoreTake(mutex2, portMAX_DELAY);                          // Take Mutex2
        Serial.println("Task A Took Mutex2...");

        Serial.println("Task A Working in Critical Section");           // simulated critical section
        vTaskDelay(500 / portTICK_PERIOD_MS);                           // delay simulates worktime

        xSemaphoreGive(mutex2);                                         // Give back mutexes in REVERSE order
        xSemaphoreGive(mutex1);

        Serial.println("Task A Released Both Mutexes: Going To Sleep");
        vTaskDelay(500 / portTICK_PERIOD_MS);                           // Delay simulates sleep time
    }
}

void lowPriTaskB(void *param)                                           // Task B has Priority 1
{
    for(;;)
    {
        xSemaphoreTake(mutex2, portMAX_DELAY);                          // Take mutex2
        Serial.println("Task B Took Mutex 2...");
        vTaskDelay(1 / portTICK_PERIOD_MS);                             // delay will force deadlock

        xSemaphoreTake(mutex1, portMAX_DELAY);                          // Take Mutex2
        Serial.println("Task B Took Mutex1...");

        Serial.println("Task B Working in Critical Section");           // simulated critical section
        vTaskDelay(500 / portTICK_PERIOD_MS);                           // delay simulates worktime

        xSemaphoreGive(mutex1);                                         // Give back mutexes in REVERSE order
        xSemaphoreGive(mutex2);

        Serial.println("Task B Released Both Mutexes: Going To Sleep");
        vTaskDelay(500 / portTICK_PERIOD_MS);                           // Delay simulates sleep time
    }
}

void setup()
{
    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.print("\n\n=>> FreeRTOS Deadlock Demo <<=\n");

    mutex1 = xSemaphoreCreateMutex();
    mutex2 = xSemaphoreCreateMutex();

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

    vTaskDelete(NULL);                                                  // Self Delete setup() & loop()

}

void loop() {}