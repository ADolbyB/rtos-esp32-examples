/**
 * Joel Brigida
 * May 29, 2023
 * This demo program uses a Hardware ISR to read the ADC value using a binary semaphore.
 */

#include <Arduino.h>
//#include <semphr.h>                                       // Only for Vanilla FreeRTOS

#if CONFIG_FREERTOS_UNICORE
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;
#endif

static const uint16_t timerDivider = 80;                    // Counts at 1MHz
static const uint64_t timerMaxCount = 1000000;              // 1 sec @ 1MHz

static const int ADCpin = A0;                               // A0 = ADC2_CH0: GPIO 26 on ESP32
static hw_timer_t *timer = NULL;                            // Delare ESP32 HAL timer (part of Arduino Library)
static volatile uint16_t someValue;                         // volatile: value can chage outside currently executing task (Inside the ISR)
static SemaphoreHandle_t binSemaphore = NULL;               // Declare global Binary Semaphore

void IRAM_ATTR onOffTimer();                                // Function resides in RAM instead of FLASH (faster)
void printValues(void *param);                              // Function to print ADC read value

void setup()
{
    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("\n=>> FreeRTOS ISR Semaphore ADC Demo <<=\n");

    binSemaphore = xSemaphoreCreateBinary();                // Instantiate binary semaphore
    if(binSemaphore == NULL)                                // Restart ESP32 if the semaphore can't be created
    {
        Serial.println("ERROR: COULD NOT INSTANTIATE SEMAPHORE");
        Serial.println("RESTARTING....");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ESP.restart();                                      // Restart ESP32
    }

    xTaskCreatePinnedToCore(
        printValues,                                        // Task runs 'printValues' function
        "Print ADC Values",
        1536,
        NULL,                                               // Task accepts no parameters
        2,                                                  // Priority 2 (higher than setup() & Loop())
        NULL,
        app_cpu
    );

    timer = timerBegin(0, timerDivider, true);              // instantiate timer: (Start value, divider, CountUp)
    timerAttachInterrupt(timer, &onOffTimer, true);         // Attach timer to ISR: (timer, function, Rising Edge)
    timerAlarmWrite(timer, timerMaxCount, true);            // Attach ISR trigger to timer: (timer, count, Auto Reload)
    timerAlarmEnable(timer);                                // Enable ISR Trigger

}

void loop() {}

void IRAM_ATTR onOffTimer()                                 // Function executes when timer reaches max & resets
{
    BaseType_t taskWoken = pdFALSE;                         // Boolean for scheduler: pdTRUE when a new task can be unblocked & started immediately after a semaphore is given.
    someValue = analogRead(ADCpin);                         // Read from ADC0 inside ISR
    xSemaphoreGiveFromISR(binSemaphore, &taskWoken);        // Give semaphore to indicate a new value is ready
    
    if(taskWoken)                                           // if condition == pdTRUE
    {
        portYIELD_FROM_ISR();                               // Exit from ISR (ESP-IDF)
    }

    /** Vanilla FreeRTOS version:
     * portYIELD_FROM_ISR(taskWoken);
    */

}

void printValues(void *param)
{
    for (;;)
    {
        xSemaphoreTake(binSemaphore, portMAX_DELAY);        // Wait for semaphore from ISR indefinitely
        Serial.print(someValue);
        Serial.print("  ");
    }
}