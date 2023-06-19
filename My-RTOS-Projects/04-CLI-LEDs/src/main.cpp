/**
 * Joel Brigida
 * 5/31/2023
 * This is an RTOS Example that implements semi-atomic tasks and controls the 2 LEDs
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
#define NUM_PATTERNS 5                                              // Total number of LED patterns
CRGB leds[NUM_LEDS];                                                // Array for RGB LED on GPIO_2

static const int LEDCchan = 0;                                      // use LEDC Channel 0 for Blue LED
static const int LEDCtimer = 12;                                    // 12-bit precision LEDC timer
static const int LEDCfreq = 5000;                                   // 5000 Hz LEDC base freq.

static const uint8_t BUF_LEN = 255;                                 // Buffer Length setting for user CLI terminal
static const char delayCmd[] = "delay ";                            // delay command definition
static const char fadeCmd[] = "fade ";                              // fade command definition
static const char patternCmd[] = "pattern ";                        // pattern command definition
static const char brightCmd[] = "bright ";                          // brightness command (pattern 3 only)
static const char cpuCmd[] = "cpu ";                                // cpu speed control command
static const char getValues[] = "values";                           // show values of all user variables
static const char getFreq[] = "freq";                               // show values for freq
static const int msgQueueSize = 5;                                  // 5 elements in message Queue

static int brightness = 65;                                         // Initial Brightness value
static int brightVal = 250;                                         // Brightness Command (Pattern 3) initial value
static int fadeInterval = 5;                                        // LED fade interval
static int delayInterval = 30;                                      // Delay between changing fade intervals
static int patternType = 1;                                         // default LED pattern: case 1
static int CPUFreq = 240;                                           // Default CPU speed = 240MHz

static QueueHandle_t msgQueue;
static SemaphoreHandle_t mutex1;                                    // for `brightVal`, `fadeInterval`, `delayInterval`, `patternType` & `CPUFreq`

struct Command                                                      // Struct for user Commands
{
    char cmd[25];                                                   // Each node is a string / char array
};

void userCLITask(void *param)                                       // Function definition for user CLI task
{
    Command sendMsg;                                                // Declare user message
    char input;                                                     // Each char of user input                                             
    char buffer[BUF_LEN];                                           // buffer to hold user input
    uint8_t index = 0;                                              // character count

    memset(buffer, 0, BUF_LEN);                                     // Clear user input buffer

    for(;;)
    {       
        if(Serial.available() > 0)
        {
            input = Serial.read();                                  // read each character of user input

            if(index < (BUF_LEN - 1))
            {
                buffer[index] = input;                              // write received character to buffer
                index++;
            }

            if(input == '\n')                                       // Check when user presses ENTER key
            {
                Serial.print("\n");
                strcpy(sendMsg.cmd, buffer);                        // copy input to Command node
                xQueueSend(msgQueue, (void *)&sendMsg, 10);         // Send to msgQueue for interpretation
                
                memset(buffer, 0, BUF_LEN);                         // Clear input buffer
                index = 0;                                          // Reset index counter.
            }
            else // echo each character back to the serial terminal
            {
                Serial.print(input);
            }
        }
        vTaskDelay(25 / portTICK_PERIOD_MS);                        // yield to other tasks
    }
}

void msgRXTask(void *param)
{
    Command someMsg;
    uint8_t delayLen = strlen(delayCmd);
    uint8_t fadeLen = strlen(fadeCmd);
    uint8_t patternLen = strlen(patternCmd);
    uint8_t brightLen = strlen(brightCmd);
    uint8_t cpuLen = strlen(cpuCmd);
    uint8_t valLen = strlen(getValues);
    uint8_t freqLen = strlen(getFreq);
    uint8_t cpuFreq;                                                // 80, 160 or 240Mhz
    char buffer[BUF_LEN];                                           // string buffer for Terminal Message
    short ledDelay;                                                 // blink delay in ms
    short fadeAmt;
    short pattern;
    short bright;

    memset(buffer, 0, BUF_LEN);                                     // Clear input buffer

    for(;;)
    {
        if(xQueueReceive(msgQueue, (void *)&someMsg, 0) == pdTRUE)  // If message received in queue
        {
            if(memcmp(someMsg.cmd, fadeCmd, fadeLen) == 0)          // Check for `fade ` command
            {                                                       // Ref: https://cplusplus.com/reference/cstring/memcmp/
                char* tailPtr = someMsg.cmd + fadeLen;              // pointer arithmetic: move pointer to integer value
                fadeAmt = atoi(tailPtr);                            // retreive integer value at end of string
                fadeAmt = abs(fadeAmt);                             // fadeAmt can't be negative
                if(fadeAmt <= 0 || fadeAmt > 128)
                {
                    Serial.println("Value Must Be Between 1 & 128");
                    Serial.println("Returning....");
                    continue;
                }
                xSemaphoreTake(mutex1, portMAX_DELAY);              // enter critical section
                    fadeInterval = fadeAmt;                         // Change global fade variable
                xSemaphoreGive(mutex1);

                sprintf(buffer, "New Fade Value: %d\n\n", fadeAmt); // BUGFIX: sometimes displays negative number
                Serial.print(buffer);                
                memset(buffer, 0, BUF_LEN);                         // Clear input buffer
            }
            else if(memcmp(someMsg.cmd, delayCmd, delayLen) == 0)   // Check for `delay ` command
            {
                char* tailPtr = someMsg.cmd + delayLen;             // pointer arithmetic: move pointer to integer value
                ledDelay = atoi(tailPtr);                           // retreive integer value at end of string
                ledDelay = abs(ledDelay);                           // ledDelay can't be negative
                
                xSemaphoreTake(mutex1, portMAX_DELAY);              // enter critical section
                    delayInterval = ledDelay;                       // Change global delay variable
                xSemaphoreGive(mutex1);
                
                sprintf(buffer, "New Delay Value: %dms\n\n", ledDelay);
                Serial.print(buffer);
                memset(buffer, 0, BUF_LEN);                         // Clear input buffer
            }
            else if(memcmp(someMsg.cmd, patternCmd, patternLen) == 0)// Check for `pattern ` command
            {
                char* tailPtr = someMsg.cmd + patternLen;           // pointer arithmetic: move pointer to integer value
                pattern = atoi(tailPtr);                            // retreive integer value at end of string
                pattern = abs(pattern);                             // patternType can't be negative
                
                xSemaphoreTake(mutex1, portMAX_DELAY);              // enter critical section
                    patternType = pattern;                          // Change global pattern variable
                xSemaphoreGive(mutex1);                             // exit critical section

                if(abs(pattern) <= NUM_PATTERNS)
                {
                    sprintf(buffer, "New Pattern: %d\n\n", pattern);
                    Serial.print(buffer);
                    memset(buffer, 0, BUF_LEN);                     // Clear input buffer
                }
            }
            else if(memcmp(someMsg.cmd, brightCmd, brightLen) == 0) // Check for `bright ` command
            {
                char* tailPtr = someMsg.cmd + brightLen;            // pointer arithmetic: move pointer to integer value
                bright = atoi(tailPtr);                             // retreive integer value at end of string
                bright = abs(bright);                               // ledDelay can't be negative
                if(bright >= 255)
                {
                    Serial.println("Maximum Value 255...");
                    bright = 255;
                }
                
                xSemaphoreTake(mutex1, portMAX_DELAY);              // Start critical section
                    brightVal = bright;                             // Change global `brightVal` variable
                xSemaphoreGive(mutex1);                             // exit critical section
                
                sprintf(buffer, "New Brightness: %d / 255\n\n", bright);
                Serial.print(buffer);
                memset(buffer, 0, BUF_LEN);                         // Clear input buffer
            }
            else if(memcmp(someMsg.cmd, cpuCmd, cpuLen) == 0)       // check for `cpu ` command
            {
                char* tailPtr = someMsg.cmd + cpuLen;
                cpuFreq = atoi(tailPtr);
                cpuFreq = abs(cpuFreq);
                if(cpuFreq != 240 && cpuFreq != 160 && cpuFreq != 80)
                {
                    Serial.println("Invalid Input: Must Be 240, 160, or 80Mhz");
                    Serial.println("Returning....\n");
                    continue;
                }

                xSemaphoreTake(mutex1, portMAX_DELAY);              // Access Critical Section
                    CPUFreq = cpuFreq;                              // change global CPU Frequency
                    setCpuFrequencyMhz(CPUFreq);                    // Set New CPU Freq
                xSemaphoreGive(mutex1);                             // Exit Critical Section

                vTaskDelay(10 / portTICK_PERIOD_MS);
                sprintf(buffer, "\nNew CPU Frequency is: %dMHz\n\n", getCpuFrequencyMhz());
                Serial.print(buffer);
                memset(buffer, 0, BUF_LEN);                         // Clear Input Buffer;
            }
            else if(memcmp(someMsg.cmd, getValues, valLen) == 0)
            {
                xSemaphoreTake(mutex1, portMAX_DELAY);              // Access Critical Section
                    sprintf(buffer, "\nCurrent Delay = %dms.           (default = 30ms)\n", delayInterval);
                    Serial.print(buffer);
                    memset(buffer, 0, BUF_LEN);
                    sprintf(buffer, "Current Fade Interval = %d.      (default = 5)\n", abs(fadeInterval));
                    Serial.print(buffer);
                    memset(buffer, 0, BUF_LEN);
                    sprintf(buffer, "Current Pattern = %d.            (default = 1)\n", patternType);
                    Serial.print(buffer);
                    memset(buffer, 0, BUF_LEN);
                    sprintf(buffer, "Current Brightness = %d / 255. (default = 250)\n\n", brightVal);
                    Serial.print(buffer);
                    memset(buffer, 0, BUF_LEN);
                xSemaphoreGive(mutex1);                             // Exit Critical Section

            }
            else if(memcmp(someMsg.cmd, getFreq, freqLen) == 0)
            {
                sprintf(buffer, "\nCPU Frequency is:  %d MHz", getCpuFrequencyMhz());
                Serial.print(buffer);
                memset(buffer, 0, BUF_LEN);
                sprintf(buffer, "\nXTAL Frequency is: %d MHz", getXtalFrequencyMhz());
                Serial.print(buffer);
                memset(buffer, 0, BUF_LEN); 
                sprintf(buffer, "\nAPB Freqency is:   %d MHz\n\n", (getApbFrequency() / 1000000));
                Serial.print(buffer);
                memset(buffer, 0, BUF_LEN);
            }
            else // Not a command: Print the message to the terminal
            {
                sprintf(buffer, "Invalid Command: %s\n", someMsg.cmd);// print user message
                Serial.print(buffer);
                memset(buffer, 0, BUF_LEN);                         // Clear input buffer             
            }
        }
        vTaskDelay(20 / portTICK_PERIOD_MS);                        // Yield to other tasks
    }
}

void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255)// 'value' must be between 0 & 'valueMax'
{
    uint32_t duty = (4095 / valueMax) * min(value, valueMax);       // calculate duty cycle: 2^12 - 1 = 4095
    ledcWrite(channel, duty);                                       // write duty cycle to LEDC
}

void RGBcolorWheelTask(void *param)
{
    leds[0] = CRGB::Red;
    FastLED.show();
    short hueVal = 0;                                               // add 32 each time...
    bool swap = false;                                              // Swap Red/Blue colors
    bool lightsOff = false;
    uint8_t accessLEDCAnalog = 1;

    for(;;)
    {
        if(patternType == 1)                                        // Fade On/Off & cycle through 8 colors
        {
            lightsOff = false;
            if(accessLEDCAnalog == 1)
            {
                ledcAnalogWrite(LEDCchan, 0);                       // Only Need to do this ONCE
                accessLEDCAnalog = 0;
                //Serial.println("Case 1: accessLEDCAnalog = 0"); // debug
            }
            
            brightness += fadeInterval;                             // Adjust brightness by fadeInterval
            if(brightness <= 0)                                     // Only change color if value <= 0
            {
                brightness = 0;
                fadeInterval = -fadeInterval;                       // Reverse fade effect
                hueVal += 32;                                       // Change color
                if(hueVal >= 255)
                {
                    hueVal = 0;                         
                }
                leds[0] = CHSV(hueVal, 255, 255);                   // Rotate: Rd-Orng-Yel-Grn-Aqua-Blu-Purp-Pnk
            }
            else if(brightness >= 255)
            {
                brightness = 255;
                fadeInterval = -fadeInterval;                       // Reverse fade effect
            }
            FastLED.setBrightness(brightness);
            FastLED.show();
        }
        else if(patternType == 2)                                   // Fade On/Off Red/Blue Police Pattern
        {
            lightsOff = false;
            if(accessLEDCAnalog == 1)
            {
                ledcAnalogWrite(LEDCchan, 0);                       // Only Need to do this ONCE
                accessLEDCAnalog = 0;
                //Serial.println("Case 2: accessLEDCAnalog = 0");   // debug
            }
            
            brightness += fadeInterval;                             // Adjust brightness by fadeInterval
            if(brightness <= 0)                                     // Only change color if value <= 0
            {
                brightness = 0;
                fadeInterval = -fadeInterval;                       // Reverse fade effect
                swap = !swap;                                       // swap colors
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
                fadeInterval = -fadeInterval;                       // Reverse fade effect
            }
            
            FastLED.setBrightness(brightness);
            FastLED.show();
        }
        else if(patternType == 3)                                   // Rotate Colors w/o fade
        {
            lightsOff = false;
            if(accessLEDCAnalog == 1)
            {
                ledcAnalogWrite(LEDCchan, 0);                       // Only Need to do this ONCE
                accessLEDCAnalog = 0;
                //Serial.println("Case 3: accessLEDCAnalog = 0");   // debug
            }
            
            brightness = brightVal;                                 // Pull value from global integer
            hueVal += fadeInterval;                                 // Change color based on global value
            if(hueVal >= 255)
            {
                hueVal = 0;                         
            }
            
            leds[0] = CHSV(hueVal, 255, 255);                       // Rotate Colors 0 - 255
            FastLED.setBrightness(brightness);
            FastLED.show();
        }
        else if(patternType == 4)                                   // Blue LED (Pin 13) Fades on/off
        {
            lightsOff = false;
            if(accessLEDCAnalog == 0)
            {
                leds[0] = CRGB::Black;                              // Turn Off RGB LED
                FastLED.show();
                accessLEDCAnalog = 1;                               // Only need to do this ONCE
                //Serial.println("Case 4: accessLEDCAnalog = 1");
            }

            brightness += fadeInterval;                             // Adjust brightness by fadeInterval
            if(brightness <= 0)                                     // Reverse fade effect at min/max values
            {
                brightness = 0;
                fadeInterval = -fadeInterval;
            }
            else if(brightness >= 255)
            {
                brightness = 255;
                fadeInterval = -fadeInterval;
            }
            
            ledcAnalogWrite(LEDCchan, brightness);                  // Set brightness on LEDC channel 0
        }
        else if(patternType == 5)                                   // Blue LED (pin 13) Cycles on/off
        {
            lightsOff = false;
            if(accessLEDCAnalog == 0)
            {
                leds[0] = CRGB::Black;                              // Turn Off RGB LED
                FastLED.show();
                accessLEDCAnalog = 1;                               // Only need to do this ONCE
                //Serial.println("Case 5: accessLEDCAnalog = 1");
            }
            
            swap = !swap;
            if(swap)
            {
                ledcAnalogWrite(LEDCchan, 255);
            }
            else
            {
                ledcAnalogWrite(LEDCchan, 0);
            }
        }
        else                                                        // Turn Off Everything
        {
            if(lightsOff == false)
            {
                lightsOff = true;
                if(accessLEDCAnalog == 0)
                {
                    leds[0] = CRGB::Black;                          // Turn Off RGB LED
                    FastLED.show();
                    //accessLEDCAnalog = 1;                         // Only need to do this ONCE
                }
                else // RGB LED already off
                {
                    ledcAnalogWrite(LEDCchan, 0);                   // Turn off Blue LED
                }
                Serial.println("Invalid Entry...Turning Lights Off!!\n");
            }
        }
        vTaskDelay(delayInterval / portTICK_PERIOD_MS);             // CLI adjustable delay (non blocking)
    }
}

void setup()
{
    mutex1 = xSemaphoreCreateMutex();                               // Initialized to 1: For msgRXTask() which accesses global variables
    msgQueue = xQueueCreate(msgQueueSize, sizeof(Command));         // Instantiate message queue

    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("\n\n=>> FreeRTOS RGB LED Color Wheel Demo <<=");

    FastLED.addLeds <CHIPSET, RGB_LED, COLOR_ORDER> (leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(brightness);
    leds[0] = CRGB::White;                                          // Power up all Pin 2 LEDs for Power On Test
    FastLED.show();
    
    ledcSetup(LEDCchan, LEDCfreq, LEDCtimer);                       // Setup LEDC timer 
    ledcAttachPin(BLUE_LED, LEDCchan);                              // Attach timer to LED pin
    vTaskDelay(2000 / portTICK_PERIOD_MS);                          // 2 Second Power On Delay

    Serial.println("Power On Test Complete...Starting Tasks");

    leds[0] = CRGB::Black;
    FastLED.show();
    
    vTaskDelay(500 / portTICK_PERIOD_MS);                           // 0.5 Second off before Starting Tasks

    xTaskCreatePinnedToCore(                                        // Instantiate CLI Terminal
        userCLITask,
        "Serial CLI Terminal",
        2048,
        NULL,
        1,
        NULL,
        app_cpu
    );

    Serial.println("User CLI Task Instantiation Complete");

    xTaskCreatePinnedToCore(                                        // Instantiate Message RX Task
        msgRXTask,
        "Receive Messages",
        2048,
        NULL,
        1,
        NULL,
        app_cpu
    );

    Serial.println("Message RX Task Instantiation Complete");

    xTaskCreatePinnedToCore(                                        // Instantiate LED fade task
        RGBcolorWheelTask,
        "Fade and Rotate RGB",
        2048,
        NULL,
        1,
        NULL,
        app_cpu
    );

    Serial.println("RGB LED Task Instantiation Complete");          // debug

    Serial.print("\n\nEnter \'delay xxx\' to change RGB Fade Speed.\n");
    Serial.print("Enter \'fade xxx\' to change RGB Fade Amount.\n");
    Serial.print("Enter \'pattern xxx\' to change RGB Pattern.\n");
    Serial.print("Enter \'bright xxx\' to change RGB Brightness (Only Pattern 3).\n");
    Serial.print("Enter \'cpu xxx\' to change CPU Frequency.\n");
    Serial.print("Enter \'values\' to retrieve current delay, fade, pattern & bright values.\n");
    Serial.print("Enter \'freq\' to retrieve current CPU, XTAL & APB Frequencies.\n\n");

    vTaskDelete(NULL);                                              // Self Delete setup() & loop()
}

void loop() {}