/**
 * Joel Brigida
 * June 3, 2023
 * Dining Philospher's Challenge: This program uses 5 tasks (the philosophers)
 * that all call the same function (they eat from the same plate.) This program is 
 * analogous to 5 different users or functions that need to share the same resource
 * on a network. Since the shared resource can only be accessed by 1 user at a time,
 * we must prevent deadlock and/or starving.
*/

#include <Arduino.h>
//#include <semphr.h>                                                           // Only for Vanilla FreeRTOS

#if CONFIG_FREERTOS_UNICORE
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;
#endif

enum { NUM_TASKS = 5 };                                                         // 5 tasks (philosophers) competing for the same resource
enum { TASK_STACK_SIZE = 2048 };                                                // 2kB Stack for ESP32 (words in Vanilla FreeRTOS)

static SemaphoreHandle_t binSemaphore;                                          // Wait for parameters to be read
static SemaphoreHandle_t doneSemaphore;                                         // Notify Main Task when finished
static SemaphoreHandle_t chopstick[NUM_TASKS];                                  // Only 5 total chopsticks for table. 2 are required for 1 person to eat.

void eat(void *param)
{
    int num;
    char buffer[50];

    num = *(int *)param;                                                        // Copy parameter & increment semaphore count.
    xSemaphoreGive(binSemaphore);

    xSemaphoreTake(chopstick[num], portMAX_DELAY);                              // Take LEFT chopstick
    sprintf(buffer, "Philosopher %i Took Chopstick %i", num, num);
    Serial.println(buffer);

    vTaskDelay(1 / portTICK_PERIOD_MS);                                         // Delay forces deadlock

    xSemaphoreTake(chopstick[(num + 1) % NUM_TASKS], portMAX_DELAY);            // Take RIGHT chopstick
    sprintf(buffer, "Philosopher %i Took Chopstick %i", num, ((num + 1) % NUM_TASKS));
    Serial.println(buffer);

    sprintf(buffer, "Philosopher %i is eating", num);                           // "Shared Resource" = eating
    Serial.println(buffer);
    vTaskDelay(10 / portTICK_PERIOD_MS);

    xSemaphoreGive(chopstick[(num + 1) % NUM_TASKS]);                           // Put down right chopstick
    sprintf(buffer, "Philosopher %i Returned Chopstick %i", num, (num + 1) % NUM_TASKS);
    Serial.println(buffer);

    xSemaphoreGive(chopstick[num]);                                             // Put down left chopstick
    sprintf(buffer, "Philosopher %i Returned Chopstick %i", num, num);
    Serial.println(buffer);

    xSemaphoreGive(doneSemaphore);                                              // Notify Main Task & Delete Self
    vTaskDelete(NULL);
}

void setup()
{
    char taskName[20];
    
    binSemaphore = xSemaphoreCreateBinary();
    doneSemaphore = xSemaphoreCreateCounting(NUM_TASKS, 0);

    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("\n\n=>> FreeRTOS Dining Philosopher\'s Challenge");
    
    int i, j, k;
    for(i = 0; i < NUM_TASKS; i++)
    {
        chopstick[i] = xSemaphoreCreateMutex();
    }
    
    for(j = 0; j < NUM_TASKS; j++)                                              // Simulate 5 Philosophers Starting to eat
    {
        sprintf(taskName, "Philosopher %i", i);
        xTaskCreatePinnedToCore(
            eat,
            taskName,
            TASK_STACK_SIZE,
            (void *)&i,
            1,
            NULL,
            app_cpu
        );
        xSemaphoreTake(binSemaphore, portMAX_DELAY);
    }

    for(k = 0; k < NUM_TASKS; k++)
    {
        xSemaphoreTake(doneSemaphore, portMAX_DELAY);                           // All 5 philosophers have eaten
    }

    Serial.println("DONE! No Deadlock Occurred!");                              // Success Message
}

void loop() {}