/**
 * Joel Brigida
 * May 24, 2023
 * NOTE: FreeRTOS Semaphore/Mutex API reference: https://freertos.org/a00113.html
 * In this program, a parameter is passed to a task using a binary semaphore.
 * This is nearly identical to the '06a-mutex-tasks' program with minor changes to use 
 * a binary semaphore instead. This program waits for input from the user in the
 * setup() function before 'delay' goes out of scope when setup() ends.
 */

#include <Arduino.h>
//#include <semphr.h>                                       // Only for Vanilla FreeRTOS

#if CONFIG_FREERTOS_UNICORE
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;                    // Only run on CPU Core 1
#endif

static const int led_pin = LED_BUILTIN;                     // Pin 13 for ESP32 is on-board LED.
static SemaphoreHandle_t binarySem;                         // Declare global binary semaphore

void blinkLEDTask(void *param);                             // Function Declaration 

void setup()                                                // setup() & loop() run on Core 1 w/ Priority 1
{
    long int ms_delay;                                      // User entered blink rate in ms
    binarySem = xSemaphoreCreateBinary();                   // Instantiate binary semaphore

    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    Serial.println("\n\n\t=>> FreeRTOS Binary Semaphore Task Parameter Hack <<=");
    Serial.println("Enter an integer delay in milliseconds: ");
    
    while(Serial.available() <= 0);                         // Wait for Serial Terminal
    
    ms_delay = Serial.parseInt();                           // Read user input integer
    Serial.print("Sending: ");
    Serial.println(ms_delay); 

    xTaskCreatePinnedToCore(                                // Instantiate Task
        blinkLEDTask,
        "LED Blink",
        1536,
        (void *)&ms_delay,                                  // Pass delay parameter to task
        2,
        NULL,
        app_cpu
    );

    xSemaphoreTake(binarySem, portMAX_DELAY);               // Wait until Semaphore is returned

    Serial.println("DONE!");                                // Print confirmation message

}

void loop() 
{
    vTaskDelay(1000 / portTICK_PERIOD_MS);                  // Non-blocking delay waits for lower priority tasks (user input)
}

void blinkLEDTask(void *param)                              // Blinks the LED based on rate from parameter
{
    xSemaphoreGive(binarySem);                              // Add one to the semaphore counter

    int delay;
    delay = *(int *)param;                                  // Copy parameter into a local variable

    Serial.print("Parameter Rec'd: ");
    Serial.println(delay);                                  // Print the delay parameter.

    pinMode(led_pin, OUTPUT);                               // Configure LED pin 13

    for(;;)                                                 // Blink the LED Forever
    {                                                       // Received Parameter determines blink rate.
        digitalWrite(led_pin, HIGH);
        vTaskDelay(delay / portTICK_PERIOD_MS);             
        digitalWrite(led_pin, LOW);
        vTaskDelay(delay / portTICK_PERIOD_MS);
    }
}