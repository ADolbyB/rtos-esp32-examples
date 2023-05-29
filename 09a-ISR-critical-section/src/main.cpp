/**
 * Joel Brigida
 * May 28, 2023
 * Hardware ISR with a critical section
 * This is a simple program that uses a timer to trigger an ISR.
 * The counter is incremented in the ISR, and decremented & printed to serial terminal in the task
 * NOTE: When a number in the serial output is repeated, it means the interrupt triggered
 * and the ISR ran between Serial.print() statements. This is expected behavior since the ISR
 * runs asyncronously and separately from the task.
 */

#include <Arduino.h>

#if CONFIG_FREERTOS_UNICORE
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;
#endif

static const uint16_t timerDivider = 8;                         // Clock Ticks at 10MHz now
static const uint64_t timerMaxCount = 1000000;                  // Timer reaches max in 100ms
static const TickType_t taskDelay = 2000 / portTICK_PERIOD_MS;  // 2 Second Delay for task so ISR can run a few times

static hw_timer_t *timer = NULL;                                // Delare ESP32 HAL timer (part of Arduino Library)
static volatile int ISRcounter;                                 // volatile: value can chage outside currently executing task (Inside the ISR)
static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;    // special type of mutex: prevent other CPU core from accessing critical section
                                                                // Tasks attempting to take spinlocks loop forever waiting for it to be available
void IRAM_ATTR onOffTimer();                                    // Function stored in RAM instead of flash for faster access
void printValues(void *param);                                  // Print values to Serial Terminal

void setup()
{
    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("\n=>> FreeRTOS ISR Using Critical Section <<=");

    xTaskCreatePinnedToCore(                                    // Instantiate Task
        printValues,
        "Print To Serial",
        1536,
        NULL,                                                   // Task accepts no parameters
        1,
        NULL,
        app_cpu
    );

    timer = timerBegin(0, timerDivider, true);                  // Instantiate Timer: (Start value, divider, countUp)
    timerAttachInterrupt(timer, &onOffTimer, true);             // Attach timer to interrupt (timer, function, Clock Rising Edge)
    timerAlarmWrite(timer, timerMaxCount, true);                // Trigger for ISR: (timer, count, autoReload)
    timerAlarmEnable(timer);                                    // Enable ISR trigger using timer

    vTaskDelete(NULL);                                          // Delete Setup() & Loop() Tasks

}

void loop() {}

void IRAM_ATTR onOffTimer()                                     // ISR: executes when timer reaches max & resets
{
    portENTER_CRITICAL_ISR(&spinlock);                          // Enter Critical Section in ISR: ESP-IDF version
    ISRcounter++;
    portEXIT_CRITICAL_ISR(&spinlock);                           // Exit Critical Section (similar to mutex)

    /** Vanilla FreeRTOS version: critical section inside ISR
     * UBaseType_t saved_int_states;
     * saved_int_status = taskENTER_CRITICAL_FROM_ISR();
     * ISRcounter++;
     * taskEXIT_CRITICAL_FROM_ISR(saved_int_status);
     */

}

void printValues(void *param)                                   // Function waits for semaphore & prints ADC value when received
{
    for(;;)
    {
        while(ISRcounter > 0)
        {
            Serial.print(ISRcounter);                           // Print Counter Values
            Serial.print("  ");

            portENTER_CRITICAL(&spinlock);                      // ESP-IDF: Enter critical section from task
            ISRcounter--;
            portEXIT_CRITICAL(&spinlock);                       // Exit critical section

            /** Vanilla FreeRTOS version: critical section in a task
             * taskENTER_CRITICAL(); 
             * ISRCounter--;
             * taskEXIT_CRITICAL();
             */

        }
        Serial.println("\n");

        vTaskDelay(taskDelay);                                  // 2 second delay
    }
}