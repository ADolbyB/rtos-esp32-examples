/**
 * Joel Brigida
 * May 19, 2023
 * Global & Local 'static' variables are stored in static memory. They remain for the duration of the program.
 * Local Variables are pushed to the stack (LIFO) and can be popped off or deleted from the stack.
 * Memory is deallocated from the stack when a function returns. Stack memory is automatically allocated by the compiler.
 * Heap memory is dynamically allocated by the programmer with function calls like 'malloc' and 
 * Must be deallocated explicitly.
 * Tasks are allocated HEAP memory.
 */

#include <Arduino.h>

#if CONFIG_FREERTOS_UNICORE                                 // Use Core 1 only.
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;
#endif

void allocateMemTask(void *param)
{
    for(;;)
    {
        int test = 1;
        int array[100];                                     // sizeof(int) = 4 bytes. 4 * 100 = 400 bytes.
        int i, j;                                           // 768 bytes (minimum for empty task) + 400 = 1168 bytes required.

        for(i = 0; i < 100; i++)                            // Dump numbers into array so it is not optimized by compiler
        {
            array[i] = test + 1;
        }
        Serial.println(array[0]);
        
        Serial.print("High Water Mark (words): ");
        Serial.println(uxTaskGetStackHighWaterMark(NULL));  // Remaining Stack memory in WORDS

        Serial.print("Heap Size Before malloc (bytes): ");
        Serial.println(xPortGetFreeHeapSize());             // Remaining Heap Memory in BYTES

        int *ptr = (int*)pvPortMalloc(1024 * sizeof(int));  // Allocate 1kB Heap Memory
        
        if(ptr == NULL)                                     // If memory is full, a NULL pointer is returned.
        {
            Serial.println("NOT ENOUGH HEAP MEMORY");
        }
        else
        {
            for(j = 0; j < 1024; j++)                       // Dump numbers into array so it is not optimized by compiler
            {
                ptr[j] = test + 2;
            }
        }

        Serial.print("Heap After malloc (bytes): ");        // Amount of free heap memory in bytes
        Serial.println(xPortGetFreeHeapSize());

        vPortFree(ptr);                                     // Deallocate Heap memory

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void setup() 
{
    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);                  // Wait 1 second for serial port.

    Serial.println("\n\n=>> FreeRTOS Memory Test <<=");

    xTaskCreatePinnedToCore(                                // Single Task to run
        allocateMemTask,
        "Allocate Memory",
        1500,                                               // Allocate 1500 bytes
        NULL,
        1,
        NULL,
        app_cpu
    );

    vTaskDelete(NULL);                                      // Delete Setup & Loop Tasks
}

void loop() {}