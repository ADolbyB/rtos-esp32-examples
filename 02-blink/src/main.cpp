/**
 * Joel Brigida
 * January 3, 2022
 * Blink LED at different rates using separate tasks
 * This uses the FastLED library to show each task blink change using the RGB LED on GPIO_2
 * Ideally, if we had 4 separate LED colors instead of 3 for our RGB, we could assign 2 colors 
 * to each task, so we can visually follow when each task toggles the LED on pin 13
*/

#include <Arduino.h>
#include <FastLED.h>


/* Directives for FastLED.h */

#define LED_PIN 2                   				      // Pin 2 on Thing Plus C is connected to WS2812 LED
#define COLOR_ORDER GRB                 			    // RGB LED in top right corner
#define CHIPSET WS2812
#define NUM_LEDS 1
#define BRIGHTNESS 25
CRGB leds[NUM_LEDS];                    			    // Array for LEDS on GPIO_2


/* Directives for multicore systems */

#if CONFIG_FREERTOS_UNICORE                       // This demo uses only core 1.
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif


/* Pin definitions */

static const int led_pin = LED_BUILTIN;           // Defined in pins_Arduino.h for ESP32: LED pin = pin 13 */


/* Blink rate definitions in milliseconds */

static const int rate_1 = 1500;
static const int rate_2 = 968;

void toggleLED_1(void *parameter)                 // Define one task: Blink the LED on and off at "rate_1"
{
  for(;;) 
  {
    digitalWrite(led_pin, HIGH);
    leds[0] = CRGB::Red;                   		    // Turn on Red LED when task 1 activates LED
    FastLED.show();
    vTaskDelay(rate_1 / portTICK_PERIOD_MS);      // delay / blink at rate_1 = 500 ms
    digitalWrite(led_pin, LOW);
    leds[0] = CRGB::Black;                   		  // Turn off Red LED when task 1 deactivates LED
    FastLED.show();
    vTaskDelay(rate_1 / portTICK_PERIOD_MS);
  }
}

void toggleLED_2(void *parameter) 
{
  for(;;) 
  {
    digitalWrite(led_pin, HIGH);
    leds[0] = CRGB::Green;                   		  // Turn on Green LED when task 2 activates LED
    FastLED.show();
    vTaskDelay(rate_2 / portTICK_PERIOD_MS);      // delay / blink at rate_2 = 323 ms
    digitalWrite(led_pin, LOW);
    leds[0] = CRGB::Black;                   		  // Turn on Green LED when task 2 activates LED
    FastLED.show();
    vTaskDelay(rate_2 / portTICK_PERIOD_MS);
  }
}


/* main code setup */

void setup() 
{
  FastLED.addLeds <CHIPSET, LED_PIN, COLOR_ORDER> (leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  pinMode(led_pin, OUTPUT);                       // configure pin 13 for output

  leds[0] = CRGB::White;                   		    // Power up all Pin 2 LEDs for Power On Test
  FastLED.show();
  delay(1000);                                    // Blocking delay for 1 second prevents tasks from executing

  /* Task Definitions */

  /**
   * Note that each task runs on Core 1 with the same priority = 1.
   * This creates an odd blink pattern, since both tasks are changing Pin 13 at the same time.
   */
  
  xTaskCreatePinnedToCore(                        // Use xTaskCreate() in vanilla FreeRTOS
                toggleLED_1,                      // Function to be called
                "Toggle 1",                       // Name of task
                1024,                             // Stack size in bytes for ESP32
                NULL,                             // Parameter to pass to function
                1,                                // Task priority (0 to 24 (24 = configMAX_PRIORITIES - 1))
                NULL,                             // Task handle: See notes
                app_cpu);                         // Core # which runs task
  
  xTaskCreatePinnedToCore(                        // Use xTaskCreate() in vanilla FreeRTOS
                toggleLED_2,                      // Function to be called
                "Toggle 2",                       // Name of task
                1024,                             // Stack size in bytes for ESP32
                NULL,                             // Parameter to pass to function
                1,                                // Task priority (0 to 24 (24 = configMAX_PRIORITIES - 1))
                NULL,                             // Task handle
                app_cpu);                         // Core # which runs task
}

/**
 * Note that the "task handle" is a pointer to the task which allows the task to be managed by another task, 
 * or the main execution loop. 
 * This pointer allows checking of the task's status, memory usage, ending the task, etc.
 */

void loop() {}
/**
 *  setup() and loop() run in their own task with priority 1 in core 1
 */