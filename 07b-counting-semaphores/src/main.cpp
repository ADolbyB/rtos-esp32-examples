/**
 * Joel Brigida
 * May 26, 2023
 * NOTE: FreeRTOS Semaphore/Mutex API reference: https://freertos.org/a00113.html
 * This program simulates a queue using a shared buffer, semaphores and a mutex.
 * The consumer tasks attempt to read the shared buffer, while the producer tasks
 * attempt to write the shared buffer.
 * Each producer task writes its task number to the buffer 3 times.
 * Each consumer task reads anything it finds in the buffer.
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

static SemaphoreHandle_t semBinary;                         // Waits for parameter to be read
static SemaphoreHandle_t mutex;                             // Protect critical section: buffer & serial
static SemaphoreHandle_t semEmpty;                          // Count empty slots in buffer
static SemaphoreHandle_t semFilled;                         // Count filled slots in buffer
static int buffer[BUFFER_SIZE];                             // Shared buffer
static int head = 0;                                        // Write index to buffer
static int tail = 0;                                        // Read index to buffer

void producerTask(void *param);                             // Producer Task declaration
void consumerTask(void *param);                             // Consumer Task declaration

void setup()
{
    char taskName[20];
    int i, j;
    semBinary = xSemaphoreCreateBinary();                   // Instantiate all semaphores
    mutex = xSemaphoreCreateMutex();
    semEmpty = xSemaphoreCreateCounting(BUFFER_SIZE, BUFFER_SIZE);
    semFilled = xSemaphoreCreateCounting(BUFFER_SIZE, 0);

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

    xSemaphoreTake(mutex, portMAX_DELAY);                   // Wait for serial port availability
    
    Serial.println("\n*** All Tasks Created ***\n");        // Critical Section

    xSemaphoreGive(mutex);

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
        xSemaphoreTake(semEmpty, portMAX_DELAY);            // Wait for empty buffer slot
        xSemaphoreTake(mutex, portMAX_DELAY);               // Lock critial section w/ mutex

        buffer[head] = num;                                 // Critical Section: write Buffer
        head = (head + 1) % BUFFER_SIZE;

        xSemaphoreGive(mutex);                              // Unlock critial section
        xSemaphoreGive(semFilled);                          // Signal Consumer Tasks: 1 buffer slot filled

    }

    vTaskDelay(10 / portTICK_PERIOD_MS);                    // 10ms delay
    vTaskDelete(NULL);                                      // Self delete task
}

void consumerTask(void *param)                              // Consumer reads from shared buffer
{
    int someVal;

    for(;;)                                                 // Read from buffer
    {
        xSemaphoreTake(semFilled, portMAX_DELAY);           // Wait for a buffer slot to be filled
        xSemaphoreTake(mutex, portMAX_DELAY);               // Lock critical section

        someVal = buffer[tail];                             // Critical section: read buffer
        tail = (tail + 1) % BUFFER_SIZE;
        Serial.print(someVal);
        Serial.print("  ");

        xSemaphoreGive(mutex);                              // Release Critical Section
        xSemaphoreGive(semEmpty);                           // Signal Producer Tasks: 1 Buffer Slot free
    }
}