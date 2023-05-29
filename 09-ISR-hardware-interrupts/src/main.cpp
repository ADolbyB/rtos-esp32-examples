/**
 * Joel Brigida
 * May 28, 2023
 * Hardware ISR
 * This is a simple program that uses a timer that triggers an ISR.
 * Inside the ISR, the LED is toggled to the opposite state.
 */

#include <Arduino.h>
//#include <timers.h>                                           // Only for Vanilla FreeRTOS         

#if CONFIG_FREERTOS_UNICORE
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;                        // Use only CPU core 1
#endif

static const uint16_t timerDivider = 80;                        // Divide Clock Freq To 1MHz
static const uint64_t timerMaxCount = 1000000;                  // 1 second @ 80Mhz
static const int ledPin = LED_BUILTIN;                          // LED = Pin 13

static hw_timer_t *timer = NULL;                                // Delare ESP32 HAL timer (part of Arduino Library)

void IRAM_ATTR onOffTimer();                                    // ISR resides in internal RAM instead of FLASH so it can be accessed faster.

void setup()
{
    pinMode(ledPin, OUTPUT);
    timer = timerBegin(0, timerDivider, true);                  // Instantiate timer w/ divider 
                                                                // (startValue = 0, dividerValue = 80, CountUp = TRUE)
    timerAttachInterrupt(timer, &onOffTimer, true);             // Attach ISR to timer function. Rising Edge = TRUE
    timerAlarmWrite(timer, timerMaxCount, true);                // Trigger timer @ timerMaxCount, Auto Relaod = TRUE
    timerAlarmEnable(timer);                                    // Allow ISR to trigger via timer
}

void loop() {}

void IRAM_ATTR onOffTimer()                                     // Function executes when timer reaches max value (then timer resets)
{
    int pinState;
    pinState = digitalRead(ledPin);
    digitalWrite(ledPin, !pinState);                            // Toggle LED
}