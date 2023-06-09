/**
 * Joel Brigida
 * June 9, 2023
 * This is a multicore blinky demo that uses tasks and an ISR.
 * Note that using spinlocks in Task1 will cause CPU Reset due to WDT expiry.
 * This doesn't happen with Task0.
*/

#include <Arduino.h>
#include <FastLED.h>                                                            // For RGB LED

#define RGB_LED 2                                                               // Pin 2 on Thing Plus C is connected to WS2812 LED
#define BLUE_LED 13                                                             // Pin 13 is On-Board Blue LED
#define COLOR_ORDER GRB                                                         // RGB LED in top right corner
#define CHIPSET WS2812                                                          // Chipset for On-Board RGB LED
#define NUM_LEDS 1                                                              // Only 1 RGB LED on the ESP32 Thing Plus

CRGB leds[NUM_LEDS];

static const BaseType_t PRO_CPU = 0;
static const BaseType_t APP_CPU = 1;

static const TickType_t timeHog = 1;                                            // time task1 hogs the CPU
static const TickType_t task0Delay = 250;                                       // time task0 blocks itself
static const TickType_t task1Delay = 350;                                       // time task1 blocks itself

static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

void task0(void *param)
{
    pinMode(BLUE_LED, OUTPUT);

    for(;;)
    {
        portENTER_CRITICAL(&spinlock);
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        portEXIT_CRITICAL(&spinlock);

        vTaskDelay(task0Delay / portTICK_PERIOD_MS);                            // Yield processor (non-blocking)
    }
}

void task1(void *param)
{
    for(;;)
    {
        leds[0] = CRGB::Black;
        FastLED.show();
        vTaskDelay(500 / portTICK_PERIOD_MS);

        leds[0] = CRGB::White;
        FastLED.show();
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void setup()
{
    Serial.begin(115200);
    
    FastLED.addLeds <CHIPSET, RGB_LED, COLOR_ORDER> (leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(50);

    leds[0] = CRGB::White;                                                      // Power up all Pin 2 LEDs for Power On Test
    FastLED.show();
    vTaskDelay(2000 / portTICK_PERIOD_MS);                                      // 2 Second Power On Delay
    Serial.print("\n\n=>> FreeRTOS Multicore 2 Task Blinker <<=\n\n");

    Serial.print("Power On Test Complete...Starting Tasks\n");

    leds[0] = CRGB::Black;
    FastLED.show();
    vTaskDelay(500 / portTICK_PERIOD_MS);

    xTaskCreatePinnedToCore(
        task0,
        "Do Task 0",
        1536,
        NULL,
        1,
        NULL,
        PRO_CPU
    );

    xTaskCreatePinnedToCore(
        task1,
        "Do Task 1",
        1536,
        NULL,
        1,
        NULL,
        APP_CPU
    );

    vTaskDelete(NULL);
}

void loop() {}