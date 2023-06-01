/**
 * Joel Brigida
 * May 21, 2023
 * NOTE: Queue Management API Reference: https://freertos.org/a00018.html
 * 1st Task prints new messages received from Queue 2 & Reads serial input from user.
 * Serial input is echoed back to serial terminal.
 * If "delay xxx" is entered, then it is sent to Queue 1.
 * 2nd Task
 */

#include <Arduino.h>

#if CONFIG_FREERTOS_UNICORE                                         // Use Core 1 only.
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;
#endif

static const uint8_t buffer_len = 255;                              // Buffer size for user input
static const char command[] = "delay ";                             // Specified string for changing delay
static const int delay_queue_len = 5;                               // Delay queue holds 5 elements.
static const int msg_queue_len = 5;                                 // Message queue holds 5 elements
static const uint8_t blink_max = 100;                               // Blink LED 100 times before message.
static const int led_pin = LED_BUILTIN;                             // Pin 13 = onboard LED.

struct Message
{
    char body[20];                                                  // Struct contains user message
    int count;
};

static QueueHandle_t delay_queue;                                   // Declare queues for messages & delay
static QueueHandle_t msg_queue;

void userCommandTask(void *param)                                   // Function definition for user CLI task
{
    Message received_msg;                                           // object declaration for user message
    char input;                                                 
    char buffer[buffer_len];                                        // buffer to hold user input
    int led_delay;                                                  // blink delay in milliseconds
    uint8_t index = 0;
    uint8_t command_len = strlen(command);

    memset(buffer, 0, buffer_len);                                  // Clear user input buffer

    for(;;)
    {
        if(xQueueReceive(msg_queue, (void *)&received_msg, 0) == pdTRUE) // If message in queue
        {
            Serial.print(received_msg.body);                        // print message body
            Serial.println(received_msg.count);                     // print message integer value
        }
        
        if(Serial.available() > 0)
        {
            input = Serial.read();                                  // read each character of user input

            if(index < (buffer_len - 1))
            {
                buffer[index] = input;                              // write received character to buffer
                index++;
            }

            if(input == '\n')                                       // Check when user presses ENTER key
            {
                Serial.print("\n");
                
                if(memcmp(buffer, command, command_len) == 0)       // If 1st 6 characters are 'delay'
                {                                                   // Ref: https://cplusplus.com/reference/cstring/memcmp/
                    char* tail = buffer + command_len;              // declare & move a pointer to the integer value at the end of the buffer string
                    led_delay = atoi(tail);                         // retreive integer value at end of string
                    led_delay = abs(led_delay);                     // led_delay can't be negative

                    if(xQueueSend(delay_queue, (void *)&led_delay, 10) != pdTRUE) // Send to delay_queue & evaluate
                    {
                        Serial.println("ERROR: Could Not Put Item In Delay Queue!");
                    }
                } 
                else
                {
                    Serial.print("User Entered: ");
                    strcpy(received_msg.body, buffer);              // Print user message to terminal
                    received_msg.count = index;                     // Print # of characters in message
                    xQueueSend(msg_queue, (void *)&received_msg, 10);   // Send to msg_queue
                }
                memset(buffer, 0, buffer_len);                      // Clear input buffer
                index = 0;                                          // Reset index counter.
            }
            else // echo each character back to the serial terminal
            {
                Serial.print(input);
            }
        }
    }
}

void blinkLEDTask(void *param)
{
    Message some_msg;                                               // object declaration for serial message
    int led_delay = 500;                                            // LED blink speed
    uint8_t counter = 0;                                            // Count how many times LED blinks
    
    pinMode(LED_BUILTIN, OUTPUT);                                   // Set pin 13 as output

    for(;;)
    {
        if(xQueueReceive(delay_queue, (void *)&led_delay, 0) == pdTRUE)
        {
            strcpy(some_msg.body, "New Delay Val: ");
            some_msg.count = led_delay;                             // Display New Delay in millisec.
            xQueueSend(msg_queue, (void *)&some_msg, 10);           // Place message in queue
        }

        digitalWrite(led_pin, HIGH);                                // Blink the LED
        vTaskDelay(led_delay / portTICK_PERIOD_MS);
        digitalWrite(led_pin, LOW);
        vTaskDelay(led_delay / portTICK_PERIOD_MS);
        counter++;                                                  // Increment blink counter

        if(counter >= blink_max)
        {
            strcpy(some_msg.body, "# Of Blinks: ");
            some_msg.count = counter;
            xQueueSend(msg_queue, (void *)&some_msg, 10);           // Place message in queue
            counter = 0;                                            // reset blink counter
        }
    }
}

void setup()
{
    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    Serial.println("\n\t=>> FreeRTOS Queue Multitask <<=");
    Serial.println("Enter 'delay xxx' where xxx is");
    Serial.println("the new LED blink delay in ms");

    delay_queue = xQueueCreate(delay_queue_len, sizeof(int));       // Instantiate queue objects.
    msg_queue = xQueueCreate(msg_queue_len, sizeof(Message));

    xTaskCreatePinnedToCore(                                        // Task read user input
        userCommandTask,
        "User CLI Terminal",
        1536,
        NULL,
        1,
        NULL,
        app_cpu
    );

    xTaskCreatePinnedToCore(                                        // task blinks LED
        blinkLEDTask,
        "Blink LED",
        1536,
        NULL,
        1,
        NULL,
        app_cpu
    );

    vTaskDelete(NULL);                                              // Delete setup & loop task.
}

void loop() {}