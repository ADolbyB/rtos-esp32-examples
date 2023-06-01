/**
 * Joel Brigida
 * 5/31/2023
 * ESP32 LEDC function Software Fade using 'ledcWrite' function
 * This example fades the on-board RGB LED (GPIO_2) using a single task.
 * The Serial Terminal accepts integer values to change the speed of the fading effect
 */

#include <Arduino.h>
#include <FastLED.h>

#if CONFIG_FREERTOS_UNICORE
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;                            // Only use CPU Core 1
#endif

#define LED_PIN 2                                                   // Pin 2 on Thing Plus C is connected to WS2812 LED
#define COLOR_ORDER GRB                                             // RGB LED in top right corner
#define CHIPSET WS2812
#define NUM_LEDS 1                                                  // Only 1 RGB LED on the ESP32 Thing Plus

static const int LEDCchan = 0;                                      // use LEDC Channel 0
static const int LEDCtimer = 12;                                    // 12-bit precision LEDC timer
static const int LEDCfreq = 5000;                                   // 5000 Hz LEDC base freq.
static const int LEDblue = LED_BUILTIN;                             // Use pin 13 blue LED for SW fading
static const uint8_t bufLen = 20;                                   // Buffer Length setting for user CLI terminal

static int brightness = 65;                                         // Initial Brightness value
static int fadeInterval = 5;                                        // LED fade interval
static int delayInterval = 30;                                      // Delay between changing fade intervals

CRGB leds[NUM_LEDS];                                                // Array for RGB LED on GPIO_2

void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255)// 'value' must be between 0 & 'valueMax'
{
    uint32_t duty = (4095 / valueMax) * min(value, valueMax);       // calculate duty cycle: 2^12 - 1 = 4095
    ledcWrite(channel, duty);                                       // write duty cycle to LEDC
}

void RGBcolorWheelTask(void *param)
{
    leds[0] = CRGB::Red;
    FastLED.show();
    int hueVal = 0;                                                 // add 32 each time...

    for(;;)
    {
        ledcAnalogWrite(LEDCchan, brightness);                      // Set brightness on LEDC channel 0
        brightness += fadeInterval;                                 // Adjust brightness by fadeInterval
        FastLED.setBrightness(brightness);
        FastLED.show();

        if (brightness <= 0 || brightness >= 255)
        {
            fadeInterval = -fadeInterval;                           // Reverse fade effect
            
            if(brightness <= 0)                                     // Only change color if value <= 0
            {
                hueVal += 32;                                       // Change color
                if(hueVal >= 255)
                {
                    hueVal = 0;                         
                }
                leds[0] = CHSV(hueVal, 255, 255);                   // Rotate: Rd-Orng-Yel-Grn-Aqua-Blu-Purp-Pnk
                FastLED.show();
            }
        } 
        vTaskDelay(delayInterval / portTICK_PERIOD_MS);             // CLI adjustable delay (non blocking)
    }
}

void readSerial(void *param)                                        // Function Definition for Task 2
{
    char input;                                                     // Each Character Input
    char buf[bufLen];                                               // Array to hold user input characters
    uint8_t index = 0;                                              // Number of characters entered by user.
    memset(buf, 0, bufLen);                                         // Clear the buffer 1st

    for(;;)
    {
        if(Serial.available() > 0)                                  // Check If Serial Terminal is open.
        {
            input = Serial.read();                                  // Read user input from serial terminal
            if(input == '\n')                                       // Update delay only if Enter key is pressed.
            {
                delayInterval = atoi(buf);                          // parse integers from CLI
                delayInterval = abs(delayInterval);                 // BUGFIX: value can't be negative
                Serial.print("\nNew LED Delay = ");
                Serial.print(delayInterval);
                Serial.println("ms");
                memset(buf, 0, bufLen);                             // Clear buffer after user input read.
                index = 0;                                          // Reset index
            }
            else
            {
                if(index < bufLen - 1)                              // Only append if index < message limit.
                {
                    buf[index] = input;
                    index++;
                    Serial.print(input);                            // Echo user input to the terminal
                }
            }
        }
    }
}

void setup()
{
    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("\n\n=>> FreeRTOS RGB LED Color Wheel Demo <<=");

    FastLED.addLeds <CHIPSET, LED_PIN, COLOR_ORDER> (leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(brightness);
    
    ledcSetup(LEDCchan, LEDCfreq, LEDCtimer);                       // Setup LEDC timer 
    ledcAttachPin(LED_PIN, LEDCchan);                               // Attach timer to LED pin

    leds[0] = CRGB::White;                                          // Power up all Pin 2 LEDs for Power On Test
    FastLED.show();
    vTaskDelay(2000 / portTICK_PERIOD_MS);                          // 2 Second Power On Delay

    Serial.println("Power On Test Complete...");

    leds[0] = CRGB::Black;
    FastLED.show();
    vTaskDelay(500 / portTICK_PERIOD_MS);                           // 0.5 Second off before Starting Tasks

    xTaskCreatePinnedToCore(                                        // Instantiate LED fade task
        RGBcolorWheelTask,
        "Fade and Rotate RGB",
        1536,
        NULL,
        1,
        NULL,
        app_cpu
    );

    Serial.println("RGB LED Task Instantiation Complete");          // debug

    xTaskCreatePinnedToCore(                                        // Instantiate readSerial task
        readSerial,
        "Read Serial",
        1536,
        NULL,
        1,
        NULL,
        app_cpu
    );

    Serial.println("readSerial Task Instantiation Complete");       // debug
    Serial.println("Enter an Integer Value in ms to change fade speed: ");

    vTaskDelete(NULL);                                              // Self Delete setup() & loop()
}

void loop() {}