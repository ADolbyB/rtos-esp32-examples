/**
 * Joel Brigida
 * May 26, 2023
 * NOTE: FreeRTOS Semaphore/Mutex API reference: https://freertos.org/a00113.html
 * This program will demonstrate the use of counting semaphores to create
 * 5 tasks with the same parameters.
 * The mutex is used to guard the serial terminal so that the messages are output properly
 */

#include <Arduino.h>
//#include <semphr.h>                                               // Only for Vanilla FreeRTOS

#if CONFIG_FREERTOS_UNICORE                                         // Use only Core 1
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;
#endif

static const int numTasks = 5;                                      // This demo uses 5 tasks.
static SemaphoreHandle_t countingSem;                               // Declare counting semaphore
static SemaphoreHandle_t mutex;                                     // Declare mutex for serial terminal

struct Message
{
    char msgBody[20];
    uint8_t msgLen;
};

void someTask(void *param);                                         // Task Function Declaration

void setup()
{
    Message someMsg;                                                // Instantiate struct object
    
    countingSem = xSemaphoreCreateCounting(numTasks, 0);            // Instantiate & Initialize semaphore to 0 w/ max of 5
    mutex = xSemaphoreCreateMutex();                                // Instantiate Mutex
    
    char taskName[12];
    char text[20] = "Here We Go";
    int i, j;                                                       // counters for for loops

    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("\n\n\t=>> FreeRTOS Counting Semaphore Demo <<=\n");

    strcpy(someMsg.msgBody, text);                                  // Message common to all tasks
    someMsg.msgLen = strlen(text);

    for(i = 0; i < numTasks; i++)                                   // create 5 tasks
    {
        sprintf(taskName, "Task #%d", i);                           // Generate Name for each task
        xTaskCreatePinnedToCore(
            someTask,                                               // Call to function someTask()
            taskName,
            1536,
            (void *)&someMsg,                                       // Pass struct object to task
            1,
            NULL,
            app_cpu
        );
    }
    
    for(j = 0; j < numTasks; j++)
    {
        xSemaphoreTake(countingSem, portMAX_DELAY);                 // Wait for all tasks to read shared memory
    }

    Serial.println("\nAll Tasks Created Successfully!");
}

void loop()
{
    vTaskDelay(1000 / portTICK_PERIOD_MS);                          // Wait & Yield to lower priority tasks
}

void someTask(void *param)                                          // Task Function Definition
{
    Message msg = *(Message *)param;                                // Copy paramter to local variable

    xSemaphoreTake(mutex, portMAX_DELAY);                           // Take mutex to protect critical section
    xSemaphoreGive(countingSem);                                    // Increment semaphore after reading parameter

    // Need to protect Serial Port w/ mutex
    Serial.print("Message Rec'd: ");                                // Print Message and length
    Serial.print(msg.msgBody);
    Serial.print(" || Msg Length: ");
    Serial.println(msg.msgLen);

    vTaskDelay(50 / portTICK_PERIOD_MS);                            // 50ms delay
    xSemaphoreGive(mutex);                                          // Release mutex after critical section

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    vTaskDelete(NULL);                                              // Wait and then self-delete task.
}