/**
 * Joel Brigida
 * May 27, 2023
 * NOTE: FreeRTOS Semaphore/Mutex API reference: https://freertos.org/a00113.html
 * NOTE: FreeRTOS Queue API reference: https://freertos.org/a00018.html
 * This program implements a queue to store data that is sent from producer
 * task and then read by the consumer tasks.
 * Note that this accomplishes the same goal in a simpler manner using a queue.
 */

#include <Arduino.h>
//#include <semphr.h>                                       // Only for Vanilla FreeRTOS

#if CONFIG_FREERTOS_UNICORE                                 // Use only CPU Core #1
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;
#endif

static const uint8_t queueLength = 10;                      // Size of queue
static const int numProducerTasks = 5;                      // Number of producer tasks
static const int numConsumerTasks = 2;                      // Number of consumer tasks
static const int numWrites = 3;                             // Each task writes to queue 3 times

static SemaphoreHandle_t semBinary;                         // Waits for parameter to be read
static SemaphoreHandle_t mutex;                             // Protect critical section
static QueueHandle_t messageQueue;                          // Declare global queue to store messages

void producerTask(void *param);                             // Producer Task declaration
void consumerTask(void *param);                             // Consumer Task declaration

void setup()
{
    char taskName[20];
    int i, j;
    
    semBinary = xSemaphoreCreateBinary();                   // Instantiate semaphores
    mutex = xSemaphoreCreateMutex();
    messageQueue = xQueueCreate(queueLength, sizeof(int));  // Instantiate message queue

    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("\n\n=>> FreeRTOS Counting Semaphores With A Queue <<=");

    for(i = 0; i < numProducerTasks; i++)                   // Generate producer tasks
    {
        sprintf(taskName, "Producer Task #%d", i);
        xTaskCreatePinnedToCore(
            producerTask,
            taskName,
            1536,
            (void *)&i,                                     // Producer takes task # argument
            1,
            NULL,
            app_cpu
        );

        xSemaphoreTake(semBinary, portMAX_DELAY);           // wait to read argument for each task if needed
    }

    for(j = 0; j < numConsumerTasks; j++)                   // Generate consumer tasks
    {
        sprintf(taskName, "Consumer Task #%d", j);
        xTaskCreatePinnedToCore(
            consumerTask,
            taskName,
            1536,
            NULL,                                           // Consumer takes no argument
            1,
            NULL,
            app_cpu
        );
    }

    xSemaphoreTake(mutex, portMAX_DELAY);                   // Wait for serial port availability
    
    Serial.println("\n*** All Tasks Created ***\n");        // Critical Section

    xSemaphoreGive(mutex);                                  // Return mutex

}

void loop()
{
    vTaskDelay(1000 / portTICK_PERIOD_MS);                  // Yield to lower priority tasks
}

void producerTask(void *param)                              // Producer writes to shared buffer
{
    int num = *(int *)param;                                // Copy parameter to local memory
    int i;

    xSemaphoreGive(semBinary);                              // Release binary semaphore

    for(i = 0; i < numWrites; i++)                          // Fill Queue with task number
    {                                                       // wait max time if queue is full
        xQueueSend(messageQueue, (void *)&num, portMAX_DELAY);
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);                    // 10ms delay
    vTaskDelete(NULL);                                      // Self delete task
}

void consumerTask(void *param)                              // Consumer reads from shared buffer
{
    int someVal;

    for(;;)                                                 // Read from buffer
    {
        xQueueReceive(messageQueue, (void *)&someVal, portMAX_DELAY);

        xSemaphoreTake(mutex, portMAX_DELAY);               // Lock critical section

        Serial.print(someVal);                              // Print to Serial Terminal.
        Serial.print("  ");

        xSemaphoreGive(mutex);                              // Release Critical Section
    }
}