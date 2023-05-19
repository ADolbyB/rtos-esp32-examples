/**
 * Joel Brigida
 * January 3, 2022
 * This demonstration shows a higher priority task which interrupts a lower priority task
 * by printing messages to the serial terminal.
 */

#include <Arduino.h>

/* Set ESP32 to use only core #1 in this demonstration */

#if CONFIG_FREERTOS_UNICORE
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;
#endif

/* Declare a message for printing to the serial terminal */

const char msg[] = "This message will print to the serial terminal";

int msg_len = strlen(msg);                                  // Count number of characters in string

/* Declare 2 tasks in static memory with NULL handles */
/* Source: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html */

static TaskHandle_t task_1 = NULL;
static TaskHandle_t task_2 = NULL;
/* 
* From <task.h>: [Taskhandle_t] Type by which tasks are referenced.  For example, a call to xTaskCreate()
* returns (via a pointer parameter) an TaskHandle_t variable that can then
* be used as a parameter to vTaskDelete to delete the task.
*/

/* Function definition for Task 1 (will have lower priority) */

void startTask1(void *parameter)
{
    for (;;)
    {
        Serial.println();                                   // Print a newline '\n' in terminal
        for (int i = 0; i < msg_len; i++)
            {
                Serial.print(msg[i]);                       // Print string to Terminal 1 character at a time
            }
        Serial.println();                                   // Print a newline '\n' in terminal
        vTaskDelay(1000 / portTICK_PERIOD_MS);              // 1 second delay after printing
    }
}

/* Function definition for Task 2 (will have higher priority) */

void startTask2(void *parameter) 
{
    for (;;) 
        {
            Serial.print('*');                              // Print a single asterisk to the serial terminal
            vTaskDelay(100 / portTICK_PERIOD_MS);           // 0.1 sec display
        }
}

/* Main code setup: runs on core #1 with priority 1 */

void setup() 
{
    Serial.begin(300);                                      // Configure slowest serial speed to view character printing
    vTaskDelay(1000 / portTICK_PERIOD_MS);                  // Wait 1 sec before output: don't miss terminal output

    Serial.println();
    Serial.println("=>> ESP32 FreeRTOS Task Demo <<=");

    Serial.print("Setup and loop task running on core ");   // Prints priority of the Setup and Loop functions
    Serial.print(xPortGetCoreID());
    Serial.print(" with priority ");
    Serial.println(uxTaskPriorityGet(NULL));                // NULL argument designates the calling function

    xTaskCreatePinnedToCore(startTask1,                     // Function Name: startTask1 runs forever
                            "Task 1",                       // Name of task
                            1024,                           // Stack size of task (minimum 768 for empty task)
                            NULL,                           // Task Parameter
                            1,                              // Task Priority
                            &task_1,                        // Task handle for tracking task
                            app_cpu);                       // CPU Core which runs task (app_cpu = 1)

    xTaskCreatePinnedToCore(startTask2,                     // Function Name: startTask1 runs once with higher priority
                            "Task 2",                       // Name of task
                            1024,                           // Stack size of task (minimum 768 for empty task)
                            NULL,                           // Task Parameter
                            2,                              // Task Priority
                            &task_2,                        // Task handle for tracking task
                            app_cpu);                       // CPU Core which runs task 
}

void loop()
{
    for (int i = 0; i < 3; i++) 
        {
            vTaskSuspend(task_2);                           // Suspend task_2 for 2 seconds
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            vTaskResume(task_2);                            // Resume task_2 for 2 seconds (task_2 prints '*' every 0.1 sec)
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }

    if (task_1 != NULL) 
        {
            vTaskDelete(task_1);                            // Delete task_1 (task_1 prints message to the serial terminal)
            task_1 = NULL;
        }
}