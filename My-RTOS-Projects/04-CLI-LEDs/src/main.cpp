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
static const int cmdQueueSize = 5;                                  // 5 elements in the command Queue
static const int msgQueueSize = 5;                                  // 5 elements in message Queue

static int brightness = 65;                                         // Initial Brightness value
static int fadeInterval = 5;                                        // LED fade interval
static int delayInterval = 30;                                      // Delay between changing fade intervals

static QueueHandle_t cmdQueue;                                      // Queues for commands & messages
static QueueHandle_t msgQueue;

CRGB leds[NUM_LEDS];                                                // Array for RGB LED on GPIO_2

struct Command                                                      // Struct for user Commands
{
    char cmd[20];                                                   // Commands need a command & number
    int num;
};

void userCLITask(void *param)                                       // Function definition for user CLI task
{
    Command sendMsg;                                                // Declare user message
    char input;                                                     // Each char of user input                                             
    char buffer[bufLen];                                            // buffer to hold user input
    int ledDelay;                                                   // blink delay in ms
    uint8_t index = 0;                                              // character count
    uint8_t cmdLen = strlen(delayCmd);

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
                
                if(memcmp(buffer, delayCmd, cmdLen) == 0)           // Check for 'delay' command
                {                                                   // Ref: https://cplusplus.com/reference/cstring/memcmp/
                    char* tailPtr = buffer + cmdLen;                // pointer arithmetic: move pointer to integer value
                    ledDelay = atoi(tailPtr);                       // retreive integer value at end of string
                    ledDelay = abs(ledDelay);                       // led_delay can't be negative

                    if(xQueueSend(cmdQueue, (void *)&ledDelay, 10) != pdTRUE) // Send to delay_queue & evaluate
                    {
                        Serial.println("ERROR: Could Not Put Item In Command Queue!");
                    }
                } 
                else                                                // User is sending message (not a command)
                {
                    strcpy(sendMsg.cmd, buffer);                    // Print user message to terminal
                    xQueueSend(msgQueue, (void *)&sendMsg, 10);     // Send to msgQueue
                }
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

    for(;;)
    {
        if(xQueueReceive(msgQueue, (void *)&someMsg, 0) == pdTRUE) // If message received in queue
        {
            Serial.print("User Entered:  ");
            Serial.print(someMsg.cmd);                              // print user message
        }
    }
}

void cmdRXTask(void *param)                                         // Task that Recieves delay command
{
    Command someCmd;                                                // object declaration for serial message
    int newDelay = 500;                                             // initial LED blink speed

    for(;;)
    {
        if(xQueueReceive(cmdQueue, (void *)&newDelay, 0) == pdTRUE)
        {
            someCmd.num = newDelay;                                 // Display New Delay in millisec.
            delayInterval = newDelay;                               // Change global delay variable
            Serial.print("New Delay Val: ");
            Serial.print(delayInterval);
            Serial.print("\n");
        }
    }
}

void RGBcolorWheelTask(void *param)
{
    leds[0] = CRGB::Red;
    FastLED.show();
    int hueVal = 0;                                                 // add 32 each time...

    for(;;)
    {
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

void setup()
{
    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("\n\n=>> FreeRTOS RGB LED Color Wheel Demo <<=");

    cmdQueue = xQueueCreate(cmdQueueSize, sizeof(int));             // Instantiate Queue objects
    msgQueue = xQueueCreate(msgQueueSize, sizeof(Command));

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

    xTaskCreatePinnedToCore(                                        // Instantiate Command RX Task
        cmdRXTask,
        "Receive Commands",
        1536,
        NULL,
        1,
        NULL,
        app_cpu
    );

    Serial.println("Command RX Task Instantiation Complete");

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

    Serial.print("\n\nEnter 'delay xxx' to change RGB Fade Speed:");
    Serial.println("Any other message will be printed to the terminal");

    vTaskDelete(NULL);                                              // Self Delete setup() & loop()
}

void loop() {}