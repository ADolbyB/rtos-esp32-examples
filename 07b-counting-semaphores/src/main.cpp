/**
 * Joel Brigida
 * May 26, 2023
 * NOTE: FreeRTOS Semaphore/Mutex API reference: https://freertos.org/a00113.html
 * 
 */

#include <Arduino.h>
//#include <semphr.h>                                       // Only for Vanilla FreeRTOS

#if CONFIG_FREERTOS_UNICORE                                 // Use only CPU Core #1
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;
#endif

enum {BUFFER_SIZE = 5};                                     // Size of buffer array
static const int numProducerTasks = 5;                      // Number of producer tasks
static const int numConsumerTasks = 2;                      // Number of consumer tasks
static const int numWrites = 3;                             // Each task writes to buffer 3 times

static SemaphoreHandle_t semBinary;                         // Declare global binary semaphore
static int buffer[BUFFER_SIZE];                             // Shared buffer
static int head = 0;                                        // Write index to buffer
static int tail = 0;                                        // Read index to buffer

void producerTask(void *param);                             // Producer Task declaration
void consumerTask(void *param);                             // Consumer Task declaration

void setup()
{
    char taskName[20];
    semBinary = xSemaphoreCreateBinary();
    int i, j;

    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("\n\n=>> FreeRTOS Counting Semaphores With Tasks <<=");

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

    Serial.println("\n*** All Tasks Have Been Created ***");

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

    for(i = 0; i < numWrites; i++)                          // Fill Shared buffer with task number
    {
        buffer[head] = num;                                 // Critical Section: Shared Buffer
        head = (head + 1) % BUFFER_SIZE;
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);                    // 10ms delay
    vTaskDelete(NULL);                                      // Self delete task
}

void consumerTask(void *param)                              // Consumer reads from shared buffer
{
    int someVal;

    for(;;)                                                 // Read from buffer
    {
        someVal = buffer[tail];
        tail = (tail + 1) % BUFFER_SIZE;
        Serial.println(someVal);
    }
}