/**
 * Joel Brigida
 * May 20, 2023
 * This program uses 2 tasks: The 1st task reads from the serial terminal & constructs a message buffer.
 * The 2nd task prints the message to the console.
 */

#include <Arduino.h>

#if CONFIG_FREERTOS_UNICORE                                 // Use Core 1 only.
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;
#endif

static const uint8_t buf_len = 255;                         // 8 bit unsigned for buffer
static char *msg_ptr = NULL;                                // Pointer to user message
static volatile uint8_t msg_flag = 0;                       // flag sets to 1 when message is completed.

void readSerialTask(void *param)
{
    char user;                                              // each user entered character
    char buffer[buf_len];                                   // buffer for user entered string
    uint8_t index = 0;                                      // index represents string length.

    memset(buffer, 0, buf_len);                             // clear the buffer 1st
    Serial.println("Enter a string to print to the terminal: ");

    for(;;)
    {
        if(Serial.available() > 0)
        {
            user = Serial.read();                           // Read each character entered by user

            if(index < (buf_len - 1))                       // if buffer is not over the limit
            {
                buffer[index] = user;                       // Add user entered string to array
                index++;
            }

            if(user == '\n')                                // When user hits Enter key
            {
                buffer[index - 1] = '\0';                   // NULL terminate string

                if(msg_flag == 0)
                {
                    msg_ptr = (char *)pvPortMalloc(index * sizeof(char));   // allocate memory for message
                    configASSERT(msg_ptr);                  // throw error & reset if memory full.
                    memcpy(msg_ptr, buffer, index);         // Copy message to newly allocated memory
                    msg_flag = 1;                           // Notify other task that message is ready
                }
                memset(buffer, 0, buf_len);                 // Reset buffer & index counter.
                index = 0;                                  // Reset index.
            }
        }
    }
}

void printMessageTask(void *param)
{
    for(;;)
    {
        if(msg_flag == 1)                                   // After user hits ENTER key
        {
            Serial.print("\n=>> ");
            Serial.println(msg_ptr);                        // Print the message
            Serial.print("\nAfter malloc(): Heap Avail in Bytes = ");
            Serial.println(xPortGetFreeHeapSize());         // Print amount of free memory in HEAP

            vPortFree(msg_ptr);                             // Deallocate all memory
            Serial.print("After free(): Total Heap Bytes Avail = ");
            Serial.println(xPortGetFreeHeapSize());         // Display Heap Size after deallocation
            msg_ptr = NULL;                                 // Set pointer to NULL
            msg_flag = 0;                                   // Reset flag to accept user message
            Serial.println("\nEnter a string to print to the terminal: ");
        }
    }
}

void setup()
{
    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    Serial.println("\n\n\t=>> FreeRTOS Heap Message Demo <<=");

    xTaskCreatePinnedToCore(                                // Task for reading user input
        readSerialTask,                                     // Function That Runs inside task
        "Read Serial Input",
        1024,
        NULL,
        1,
        NULL,
        app_cpu
    );

    xTaskCreatePinnedToCore(                                // Task for printing to terminal
        printMessageTask,                                   // Function that runs inside task
        "Print Message To Serial",
        1024,
        NULL,
        1,
        NULL,
        app_cpu
    );

    vTaskDelete(NULL);                                      // Delete Setup() & Loop() task
}

void loop() {}