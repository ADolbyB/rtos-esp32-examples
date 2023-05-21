/**
 * Joel Brigida
 * May 20, 2023
 * This program initializes a simple queue for ints in static memory.
 * Note that if the delay in the preintMessagesTask() is less than the delay in the loop() task
 * then the queue will remove items faster than it inserts them, and trigger QUEUE EMPTY error msg.
 * If the delay in the loop() is less than the delay in the printMessagesTask() then the queue will 
 * eventually fill up and trigger QUEUE FULL error msg.
 */

#include <Arduino.h>

#if CONFIG_FREERTOS_UNICORE                                     // Use Core 1 only.
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;
#endif

static int counter = 0;                                         // Track # of items in queue
static const uint8_t queue_len = 5;                             // Maximum Queue Size
static QueueHandle_t msg_queue;                                 // Declare a Global Queue Object

void printMessagesTask(void *param)                             // Task Function Definition
{
    int read_item;                                              // item to be read from queue

    for(;;)
    {
        if(xQueueReceive(msg_queue, (void *)&read_item, 0) == pdTRUE)   // remove item from queue
        {
            Serial.println("*** Item Removed From Queue ***");
            if(counter > 0)
            {
                counter--;                                      // Decrement queue counter when item read.
            }
            Serial.print("Items in Queue After Remove: ");
            Serial.println(read_item);                          // Print item removed from queue
        }
        else
        {
            Serial.println("Error: Queue Empty!!!");
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);                  // Wait until reading queue again
    }
}

void setup()
{
    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    Serial.println("\n\n\t=>> FreeRTOS Queue Demo <<=");

    msg_queue = xQueueCreate(queue_len, sizeof(int));           // Instantiate the queue: can hold 5 ints.

    xTaskCreatePinnedToCore(                                    // Create task to handle queue
        printMessagesTask,
        "Print Messages To Terminal",
        1024,
        NULL,
        1,
        NULL,
        app_cpu
    );
}

void loop()
{
    if(xQueueSend(msg_queue, (void *)&counter, 10) == pdTRUE)   // Try to add item to queue for 10 ticks
    {
        Serial.println("*** Item Added to Queue ***");
        counter++;                                              // Increment counter if item is added.
        Serial.print("Items in Queue After Adding: ");
        Serial.println(counter);                                // Print item added to queue
    }
    else // if item is not added to queue
    {
        Serial.println("Error: Queue Is Full!!");               // Print error message if failed to add to queue
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);                      // 1 sec delay before adding to queue again
}