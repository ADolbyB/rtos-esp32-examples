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

void eatTask(void *param)
{
    int num;
    char buffer[50];
    int index1, index2;

    num = *(int *)param;                                                        // Copy parameter & increment semaphore count.
    xSemaphoreGive(binSemaphore);

    if(num < ((num + 1) % NUM_TASKS))
    {
        index1 = num;
        index2 = ((num + 1) % NUM_TASKS);
    }
    else
    {
        index1 = ((num + 1) % NUM_TASKS);
        index2 = num;
    }

    xSemaphoreTake(chopstick[index1], portMAX_DELAY);                           // Take Lower # chopstick 1st always
    sprintf(buffer, "Eat 1: Philosopher %d Took Chopstick %d\n\n", num, num);
    Serial.print(buffer);

    vTaskDelay(1 / portTICK_PERIOD_MS);                                         // Delay forces deadlock

    xSemaphoreTake(chopstick[index2], portMAX_DELAY);                           // Take Higher # chopstick 2nd always
    sprintf(buffer, "Eat 2: Philosopher %d Took Chopstick %d\n\n", num, (num + 1) % NUM_TASKS);
    Serial.print(buffer);

    sprintf(buffer, "Eat 3: Philosopher %d is eating\n\n", num);                // "Shared Resource" = eating
    Serial.print(buffer);
    vTaskDelay(10 / portTICK_PERIOD_MS);

    xSemaphoreGive(chopstick[index2]);                                          // Put down higher # chopstick 1st always
    sprintf(buffer, "Eat 4: Philosopher %d Returned Chopstick %d\n\n", num, (num + 1) % NUM_TASKS);
    Serial.print(buffer);

    xSemaphoreGive(chopstick[index1]);                                          // Put down lower # chopstick 2nd always
    sprintf(buffer, "Eat 5: Philosopher %d Returned Chopstick %d\n\n", num, num);
    Serial.print(buffer);

    xSemaphoreGive(doneSemaphore);                                              // Notify Main Task & Delete Self
    sprintf(buffer, "Eat 6: Done...Deleting Task #%d Now...\n\n", num);
    Serial.print(buffer);
    vTaskDelete(NULL);
}

void setup()
{
    char taskName[30];
    char testBuffer[80];
    
    binSemaphore = xSemaphoreCreateBinary();
    doneSemaphore = xSemaphoreCreateCounting(NUM_TASKS, 0);

    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("\n\n=>> FreeRTOS Dining Philosopher\'s Challenge: Hierarchy <<=\n");
    
    int i, j, k;
    for(i = 0; i < NUM_TASKS; i++)
    {
        chopstick[i] = xSemaphoreCreateMutex();
        sprintf(testBuffer, "Setup 1: Created & Gave Mutex (chopstick) #%d\n", i);
        Serial.print(testBuffer);
    }

    Serial.print("\n");
    
    for(j = 0; j < NUM_TASKS; j++)                                              // Simulate 5 Philosophers Starting to eat
    {
        sprintf(taskName, "Philosopher %d", i);
        xTaskCreatePinnedToCore(
            eatTask,
            taskName,
            TASK_STACK_SIZE,
            (void *)&j,
            1,
            NULL,
            app_cpu
        );
        xSemaphoreTake(binSemaphore, portMAX_DELAY);
        sprintf(testBuffer, "Setup 2: Task #%d Created & Took binSemaphore %d\n\n", j, j);
        Serial.print(testBuffer);
    }

    for(k = 0; k < NUM_TASKS; k++)
    {
        xSemaphoreTake(doneSemaphore, portMAX_DELAY);                           // All 5 philosophers have eaten
        sprintf(testBuffer, "Setup 3: Task #%d Finished & Took doneSemaphore #%d\n\n", k, k);
        Serial.print(testBuffer);
    }
    
    Serial.print("\nDONE! No Deadlock Occurred!\n");                            // Success Message
}

void loop() {}