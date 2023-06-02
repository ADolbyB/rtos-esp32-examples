/**
 * Joel Brigida
 * 5/31/2023
 * This is an RTOS Example that implements atomic tasks and controls the 2 LEDs
 * on the ESP32 Thing Plus C (GPIO_2 RGB LED & GPIO 13 Blue LED).
 */

#include <Arduino.h>
#include <FastLED.h>

#if CONFIG_FREERTOS_UNICORE
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;                            // Only use CPU Core 1
#endif

#define RGB_LED 2                                                   // Pin 2 on Thing Plus C is connected to WS2812 LED
#define BLUE_LED 13                                                 // Pin 13 is On-Board Blue LED
#define COLOR_ORDER GRB                                             // RGB LED in top right corner
#define CHIPSET WS2812                                              // Chipset for On-Board RGB LED
#define NUM_LEDS 1                                                  // Only 1 RGB LED on the ESP32 Thing Plus

static const uint8_t bufLen = 255;                                  // Buffer Length setting for user CLI terminal
static const char delayCmd[] = "delay ";                            // delay command definition
static const char fadeCmd[] = "fade ";                              // fade command definition
static const char patternCmd[] = "pattern ";                        // pattern command definition
static const int msgQueueSize = 5;                                  // 5 elements in message Queue

static int brightness = 65;                                         // Initial Brightness value
static int fadeInterval = 5;                                        // LED fade interval
static int delayInterval = 30;                                      // Delay between changing fade intervals
static int patternType = 1;                                         // LED pattern type variable

static QueueHandle_t msgQueue;

CRGB leds[NUM_LEDS];                                                // Array for RGB LED on GPIO_2

struct Command                                                      // Struct for user Commands
{
    char cmd[25];                                                   // Each node is a string / char array
};

void userCLITask(void *param)                                       // Function definition for user CLI task
{
    Command sendMsg;                                                // Declare user message
    char input;                                                     // Each char of user input                                             
    char buffer[bufLen];                                            // buffer to hold user input
    uint8_t index = 0;                                              // character count

    memset(buffer, 0, bufLen);                                      // Clear user input buffer

    for(;;)
    {       
        if(Serial.available() > 0)
        {
            input = Serial.read();                                  // read each character of user input

            if(index < (bufLen - 1))
            {
                buffer[index] = input;                              // write received character to buffer
                index++;
            }
            if(input == '\n')                                       // Check when user presses ENTER key
            {
                Serial.print("\n");
                strcpy(sendMsg.cmd, buffer);                        // copy input to Command node
                xQueueSend(msgQueue, (void *)&sendMsg, 10);         // Send to msgQueue for interpretation
                
                memset(buffer, 0, bufLen);                          // Clear input buffer
                index = 0;                                          // Reset index counter.
            }
            else // echo each character back to the serial terminal
            {
                Serial.print(input);
            }
        }
    }
}

void msgRXTask(void *param)
{
    Command someMsg;
    uint8_t delayLen = strlen(delayCmd);
    uint8_t fadeLen = strlen(fadeCmd);
    uint8_t patternLen = strlen(patternCmd);
    short ledDelay;                                                 // blink delay in ms
    short fadeAmt;
    short pattern;

    for(;;)
    {
        if(xQueueReceive(msgQueue, (void *)&someMsg, 0) == pdTRUE)  // If message received in queue
        {
            if(memcmp(someMsg.cmd, fadeCmd, fadeLen) == 0)          // Check for 'fade' command
            {                                                       // Ref: https://cplusplus.com/reference/cstring/memcmp/
                char* tailPtr = someMsg.cmd + fadeLen;              // pointer arithmetic: move pointer to integer value
                fadeAmt = atoi(tailPtr);                            // retreive integer value at end of string
                fadeAmt = abs(fadeAmt);                             // fadeAmt can't be negative
                fadeInterval = fadeAmt;                             // Change global fade variable
                Serial.print("New Fade Value: ");
                Serial.print(fadeAmt);                              // BUGFIX: sometimes displays negative number
                Serial.print("\n");
            }
            else if(memcmp(someMsg.cmd, delayCmd, delayLen) == 0)   // Check for 'delay' command
            {                                                       // Ref: https://cplusplus.com/reference/cstring/memcmp/
                char* tailPtr = someMsg.cmd + delayLen;             // pointer arithmetic: move pointer to integer value
                ledDelay = atoi(tailPtr);                           // retreive integer value at end of string
                ledDelay = abs(ledDelay);                           // ledDelay can't be negative
                delayInterval = ledDelay;                           // Change global delay variable
                Serial.print("New Delay Value: ");
                Serial.print(delayInterval);
                Serial.print("\n");
            }
            else if(memcmp(someMsg.cmd, patternCmd, patternLen) == 0)// Check for 'pattern' command
            {                                                       // Ref: https://cplusplus.com/reference/cstring/memcmp/
                char* tailPtr = someMsg.cmd + patternLen;           // pointer arithmetic: move pointer to integer value
                pattern = atoi(tailPtr);                            // retreive integer value at end of string
                pattern = abs(pattern);                             // ledDelay can't be negative
                patternType = pattern;                              // Change global pattern variable
                Serial.print("New Pattern Type: ");
                Serial.print(patternType);
                Serial.print("\n");
            }
            else // Not a command: Print the message to the terminal
            {
                Serial.print("User Entered:  ");
                Serial.print(someMsg.cmd);                          // print user message
            }
        }
    }
}

void RGBcolorWheelTask(void *param)
{
    leds[0] = CRGB::Red;
    FastLED.show();
    short hueVal = 0;                                               // add 32 each time...
    bool swap = false;                                              // Swap Red/Blue colors

    for(;;)
    {
        switch(patternType)
        {
            case 1:                                                 // Fade On/Off & cycle through 8 colors
            {
                brightness += fadeInterval;                         // Adjust brightness by fadeInterval
                if(brightness <= 0)                                 // Only change color if value <= 0
                {
                    brightness = 0;
                    fadeInterval = -fadeInterval;                   // Reverse fade effect
                    hueVal += 32;                                   // Change color
                    if(hueVal >= 255)
                    {
                        hueVal = 0;                         
                    }
                    leds[0] = CHSV(hueVal, 255, 255);               // Rotate: Rd-Orng-Yel-Grn-Aqua-Blu-Purp-Pnk
                }
                else if(brightness >= 255)
                {
                    brightness = 255;
                    fadeInterval = -fadeInterval;                   // Reverse fade effect
                }
                FastLED.setBrightness(brightness);
                FastLED.show();
                break;
            }
            case 2:                                                 // Fade On/Off Red/Blue Police Pattern
            {
                brightness += fadeInterval;                         // Adjust brightness by fadeInterval
                if(brightness <= 0)                                 // Only change color if value <= 0
                {
                    brightness = 0;
                    fadeInterval = -fadeInterval;                   // Reverse fade effect
                    swap = !swap;                                   // swap colors
                    if(swap)
                    {
                        leds[0] = CRGB::Blue;
                    }
                    else
                    {
                        leds[0] = CRGB::Red;
                    }
                }
                else if(brightness >= 255)
                {
                    brightness = 255;
                    fadeInterval = -fadeInterval;                   // Reverse fade effect
                }
                FastLED.setBrightness(brightness);
                FastLED.show();
                break;
            }
            case 3:                                                 // Rotate Colors w/o fade
            {
                brightness = 250;
                hueVal += 1;                                        // Change color
                if(hueVal >= 255)
                {
                    hueVal = 0;                         
                }
                leds[0] = CHSV(hueVal, 255, 255);                   // Rotate Colors 0 - 255
                FastLED.setBrightness(brightness);
                FastLED.show();
                break;
            }
            default:
            {
                Serial.println("Invalid Selection: Defaulting to Pattern 1");
                patternType = 1;
                break;
            }
        }
        vTaskDelay(delayInterval / portTICK_PERIOD_MS);             // CLI adjustable delay (non blocking)
    }
}

void setup()
{
    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("\n\n=>> FreeRTOS RGB LED Color Wheel Demo <<=");

    msgQueue = xQueueCreate(msgQueueSize, sizeof(Command));         // Instantiate message queue

    FastLED.addLeds <CHIPSET, RGB_LED, COLOR_ORDER> (leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(brightness);

    leds[0] = CRGB::White;                                          // Power up all Pin 2 LEDs for Power On Test
    FastLED.show();
    vTaskDelay(2000 / portTICK_PERIOD_MS);                          // 2 Second Power On Delay

    Serial.println("Power On Test Complete...Starting Tasks");

    leds[0] = CRGB::Black;
    FastLED.show();
    vTaskDelay(500 / portTICK_PERIOD_MS);                           // 0.5 Second off before Starting Tasks

    xTaskCreatePinnedToCore(                                        // Instantiate CLI Terminal
        userCLITask,
        "Serial CLI Terminal",
        1536,
        NULL,
        1,
        NULL,
        app_cpu
    );

    Serial.println("User CLI Task Instantiation Complete");

    xTaskCreatePinnedToCore(                                        // Instantiate Message RX Task
        msgRXTask,
        "Receive Messages",
        1536,
        NULL,
        1,
        NULL,
        app_cpu
    );

    Serial.println("Message RX Task Instantiation Complete");

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

    Serial.print("\n\nEnter \'delay xxx\' to change RGB Fade Speed\n");
    Serial.print("Enter \'fade xxx\' to change RGB Fade Amount\n");
    Serial.print("Enter \'pattern xxx\' to change RGB Pattern\n");

    vTaskDelete(NULL);                                              // Self Delete setup() & loop()
}

void loop() {}