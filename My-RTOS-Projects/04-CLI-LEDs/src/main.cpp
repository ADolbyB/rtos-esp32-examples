/**
 * Joel Brigida
 * 5/31/2023
 * This is an RTOS Example that implements semi-atomic tasks and controls the 2 LEDs
 * on the ESP32 Thing Plus C (GPIO_2 RGB LED & GPIO 13 Blue LED).
 * The user types commands into the Serial CLI handled by `userCLITask`. There it is
 * checked if it is a valid command or not. If not a valid command, the message is printed 
 * to the terminal. If it is a valid command, it's sent to the `RGBcolorTask`
 */

#include <Arduino.h>
#include <FastLED.h>

#if CONFIG_FREERTOS_UNICORE
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;                                // Only use CPU Core 1
#endif

#define RGB_LED 2                                                       // Pin 2 on Thing Plus C is connected to WS2812 LED
#define BLUE_LED 13                                                     // Pin 13 is On-Board Blue LED
#define COLOR_ORDER GRB                                                 // RGB LED in top right corner
#define CHIPSET WS2812                                                  // Chipset for On-Board RGB LED
#define NUM_LEDS 1                                                      // Only 1 RGB LED on the ESP32 Thing Plus
#define NUM_PATTERNS 5                                                  // Total number of LED patterns
CRGB leds[NUM_LEDS];                                                    // Array for RGB LED on GPIO_2

static const int LEDCchan = 0;                                          // use LEDC Channel 0 for Blue LED
static const int LEDCtimer = 12;                                        // 12-bit precision LEDC timer
static const int LEDCfreq = 5000;                                       // 5000 Hz LEDC base freq.

static const uint8_t BUF_LEN = 255;                                     // Buffer Length setting for user CLI terminal
static const char delayCmd[] = "delay ";                                // delay command definition
static const char fadeCmd[] = "fade ";                                  // fade command definition
static const char patternCmd[] = "pattern ";                            // pattern command definition
static const char brightCmd[] = "bright ";                              // brightness command (pattern 3 only)
static const char cpuCmd[] = "cpu ";                                    // cpu speed control command
static const char getValues[] = "values";                               // show values of all user variables
static const char getFreq[] = "freq";                                   // show values for freq
static const int QueueSize = 5;                                         // 5 elements in either Queue

static QueueHandle_t msgQueue;                                          // Queue for CLI messages
static QueueHandle_t cmdQueue;                                          // Queue to handle commands
static SemaphoreHandle_t mutex1;                                        // mutex for operating in RGB task

struct Message                                                          // Struct for CLI input
{
    char msg[25];                                                       // Each node is a string / char array
};

struct Command                                                          // Sent from `msgRXTask` to `RGBcolorWheelTask`
{
    char cmd[25];
    int amount;
};

void userCLITask(void *param)                                           // Function definition for user CLI task
{
    Message sendMsg;                                                    // Declare user message
    char input;                                                         // Each char of user input                                             
    char buffer[BUF_LEN];                                               // buffer to hold user input
    uint8_t index = 0;                                                  // character count

    memset(buffer, 0, BUF_LEN);                                         // Clear user input buffer

    for(;;)
    {       
        if(Serial.available() > 0)
        {
            input = Serial.read();                                      // read each character of user input

            if(index < (BUF_LEN - 1))
            {
                buffer[index] = input;                                  // write received character to buffer
                index++;
            }

            if(input == '\n')                                           // Check when user presses ENTER key
            {
                Serial.print("\n");
                strcpy(sendMsg.msg, buffer);                            // copy input to Message node
                xQueueSend(msgQueue, (void *)&sendMsg, 10);             // Send to msgQueue for interpretation
                
                memset(buffer, 0, BUF_LEN);                             // Clear input buffer
                index = 0;                                              // Reset index counter.
            }
            else // echo each character back to the serial terminal
            {
                Serial.print(input);
            }
        }
        vTaskDelay(25 / portTICK_PERIOD_MS);                            // yield to other tasks
    }
}

void msgRXTask(void *param)
{
    Message someMsg;
    Command someCmd;
    uint8_t delayLen = strlen(delayCmd);
    uint8_t fadeLen = strlen(fadeCmd);
    uint8_t patternLen = strlen(patternCmd);
    uint8_t brightLen = strlen(brightCmd);
    uint8_t cpuLen = strlen(cpuCmd);
    uint8_t valLen = strlen(getValues);
    uint8_t freqLen = strlen(getFreq);
    uint8_t localCPUFreq;                                               // 80, 160 or 240Mhz
    char buffer[BUF_LEN];                                               // string buffer for Terminal Message
    short ledDelay;                                                     // blink delay in ms
    short fadeAmt;
    short pattern;
    short bright;

    memset(buffer, 0, BUF_LEN);                                         // Clear input buffer

    for(;;)
    {
        /* CLI Input Handling */
        if(xQueueReceive(msgQueue, (void *)&someMsg, 0) == pdTRUE)      // If message received from User CLI
        {
            if(memcmp(someMsg.msg, fadeCmd, fadeLen) == 0)              // Check for `fade ` command
            {                                                           // Ref: https://cplusplus.com/reference/cstring/memcmp/
                char* tailPtr = someMsg.msg + fadeLen;                  // pointer arithmetic: move pointer to integer value
                fadeAmt = atoi(tailPtr);                                // retreive integer value at end of string
                fadeAmt = abs(fadeAmt);                                 // fadeAmt can't be negative
                if(fadeAmt <= 0 || fadeAmt > 128)
                {
                    Serial.println("Value Must Be Between 1 & 128");
                    Serial.println("Returning....");
                    continue;
                }
                
                strcpy(someCmd.cmd, "fade");
                someCmd.amount = fadeAmt;                               // copy input to Message node
                xQueueSend(cmdQueue, (void *)&someCmd, 10);             // Send to cmdQueue for interpretation
            }
            else if(memcmp(someMsg.msg, delayCmd, delayLen) == 0)       // Check for `delay ` command
            {
                char* tailPtr = someMsg.msg + delayLen;                 // pointer arithmetic: move pointer to integer value
                ledDelay = atoi(tailPtr);                               // retreive integer value at end of string
                ledDelay = abs(ledDelay);                               // ledDelay can't be negative

                strcpy(someCmd.cmd, "delay");
                someCmd.amount = ledDelay;                              // copy input to Message node
                xQueueSend(cmdQueue, (void *)&someCmd, 10);             // Send to cmdQueue for interpretation                    // Clear input buffer
            }
            else if(memcmp(someMsg.msg, patternCmd, patternLen) == 0)   // Check for `pattern ` command
            {
                char* tailPtr = someMsg.msg + patternLen;               // pointer arithmetic: move pointer to integer value
                pattern = atoi(tailPtr);                                // retreive integer value at end of string
                pattern = abs(pattern);                                 // patternType can't be negative

                strcpy(someCmd.cmd, "pattern");
                someCmd.amount = pattern;                               // copy input to Message node
                xQueueSend(cmdQueue, (void *)&someCmd, 10);             // Send to cmdQueue for interpretation
            }
            else if(memcmp(someMsg.msg, brightCmd, brightLen) == 0)     // Check for `bright ` command
            {
                char* tailPtr = someMsg.msg + brightLen;                // pointer arithmetic: move pointer to integer value
                bright = atoi(tailPtr);                                 // retreive integer value at end of string
                bright = abs(bright);                                   // ledDelay can't be negative
                
                strcpy(someCmd.cmd, "bright");
                someCmd.amount = bright;                                // copy input to Message node
                xQueueSend(cmdQueue, (void *)&someCmd, 10);             // Send to cmdQueue for interpretation
            }
            else if(memcmp(someMsg.msg, cpuCmd, cpuLen) == 0)           // check for `cpu ` command
            {
                char* tailPtr = someMsg.msg + cpuLen;
                localCPUFreq = atoi(tailPtr);
                localCPUFreq = abs(localCPUFreq);

                strcpy(someCmd.cmd, "cpu");
                someCmd.amount = localCPUFreq;                          // copy input to Message node
                xQueueSend(cmdQueue, (void *)&someCmd, 10);             // Send to cmdQueue for interpretation
            }
            else if(memcmp(someMsg.msg, getValues, valLen) == 0)
            {
                strcpy(someCmd.cmd, "values");
                someCmd.amount = 0;                                     // copy input to Message node
                xQueueSend(cmdQueue, (void *)&someCmd, 10);             // Send to cmdQueue for interpretation
            }
            else if(memcmp(someMsg.msg, getFreq, freqLen) == 0)
            {
                strcpy(someCmd.cmd, "freq");
                someCmd.amount = 0;                                     // copy input to Message node
                xQueueSend(cmdQueue, (void *)&someCmd, 10);             // Send to cmdQueue for interpretation
            }
            else // Not a command: Print the message to the terminal
            {
                sprintf(buffer, "Invalid Command: %s\n", someMsg.msg);  // print user message
                Serial.print(buffer);
                memset(buffer, 0, BUF_LEN);                             // Clear input buffer             
            }
        }
        vTaskDelay(20 / portTICK_PERIOD_MS);                            // Yield to other tasks
    }
}

void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255)// 'value' must be between 0 & 'valueMax'
{
    uint32_t duty = (4095 / valueMax) * min(value, valueMax);           // calculate duty cycle: 2^12 - 1 = 4095
    ledcWrite(channel, duty);                                           // write duty cycle to LEDC
}

void RGBcolorWheelTask(void *param)
{
    Command someCmd;                                                    // Recieved from `msgRXTask`
    uint8_t localCPUFreq;
    char buffer[BUF_LEN];
     
    int fadeInterval = 5;                                               // LED fade interval
    int delayInterval = 30;                                             // Delay between changing fade intervals
    int patternType = 1;                                                // default LED pattern: case 1
    int brightVal = 250;                                                // Brightness Command (Pattern 3) initial value
    int brightness = 65;                                                // Initial Brightness value

    short hueVal = 0;                                                   // add 32 each time for each color...
    bool swap = false;                                                  // Swap Red/Blue colors
    bool lightsOff = false;
    uint8_t accessLEDCAnalog = 1;
    leds[0] = CRGB::Red;
    FastLED.show();

    for(;;)
    {
        /* Command Handling */
        if(xQueueReceive(cmdQueue, (void *)&someCmd, 0) == pdTRUE)      // if command received from MSG QUEUE
        {
            if(memcmp(someCmd.cmd, fadeCmd, 4) == 0)                    // if `fade` command rec'd (compare to global var)
            {
                fadeInterval = someCmd.amount;
                sprintf(buffer, "New Fade Value: %d\n\n", someCmd.amount); // BUGFIX: sometimes displays negative number
                Serial.print(buffer);                
                memset(buffer, 0, BUF_LEN); 
            }
            else if(memcmp(someCmd.cmd, delayCmd, 5) == 0)              // if `delay` command rec'd (compare to global var)
            {
                delayInterval = someCmd.amount;
                sprintf(buffer, "New Delay Value: %dms\n\n", someCmd.amount);
                Serial.print(buffer);
                memset(buffer, 0, BUF_LEN);  
            }
            else if((memcmp(someCmd.cmd, patternCmd, 6) == 0))          // if `pattern` command rec'd (compare to global var)
            {
                patternType = someCmd.amount;
                if(int(abs(patternType)) <= NUM_PATTERNS && int(patternType) != 0) // BUGFIX: "New Pattern: 0" with invalid entry
                {
                    sprintf(buffer, "New Pattern: %d\n\n", someCmd.amount);
                    Serial.print(buffer);
                    memset(buffer, 0, BUF_LEN);
                }
            }
            else if(memcmp(someCmd.cmd, brightCmd, 5) == 0)             // if `bright` command rec'd (compare to global var)
            {
                brightVal = someCmd.amount;                
                if(brightVal >= 255)
                {
                    Serial.println("Maximum Value 255...");
                    brightVal = 255;
                }               
                sprintf(buffer, "New Brightness: %d / 255\n\n", someCmd.amount);
                Serial.print(buffer);
                memset(buffer, 0, BUF_LEN);
            }
            else if(memcmp(someCmd.cmd, cpuCmd, 3) == 0)                // if `cpu` command rec'd (complare to global var)
            {
                localCPUFreq = someCmd.amount;
                if(localCPUFreq != 240 && localCPUFreq != 160 && localCPUFreq != 80)
                {
                    Serial.println("Invalid Input: Must Be 240, 160, or 80Mhz");
                    Serial.println("Returning....\n");
                    continue;
                }
                setCpuFrequencyMhz(localCPUFreq);                       // Set New CPU Freq
                vTaskDelay(10 / portTICK_PERIOD_MS);                    // yield for a brief moment

                sprintf(buffer, "\nNew CPU Frequency is: %dMHz\n\n", getCpuFrequencyMhz());
                Serial.print(buffer);
                memset(buffer, 0, BUF_LEN);
            }
            else if(memcmp(someCmd.cmd, getValues, 6) == 0)             // if `values` command rec'd (complare to global var)
            {
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
            }
            else if(memcmp(someCmd.cmd, getFreq, 4) == 0)               // if `freq` command rec'd (complare to global var)
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
            vTaskDelay(10 / portTICK_PERIOD_MS);                        // yield briefly (only if command rec'd)
        }
        else /* Show LED Patterns */
        {
            if(patternType == 1)                                            // Fade On/Off & cycle through 8 colors
            {
                lightsOff = false;
                if(accessLEDCAnalog == 1)
                {
                    ledcAnalogWrite(LEDCchan, 0);                           // Only Need to do this ONCE
                    accessLEDCAnalog = 0;
                }
            
                brightness += fadeInterval;                                 // Adjust brightness by fadeInterval
                if(brightness <= 0)                                         // Only change color if value <= 0
                {
                    brightness = 0;
                    fadeInterval = -fadeInterval;                           // Reverse fade effect
                    hueVal += 32;                                           // Change color
                    if(hueVal >= 255)
                    {
                        hueVal = 0;                         
                    }
                    leds[0] = CHSV(hueVal, 255, 255);                       // Rotate: Rd-Orng-Yel-Grn-Aqua-Blu-Purp-Pnk
                }
                else if(brightness >= 255)
                {
                    brightness = 255;
                    fadeInterval = -fadeInterval;                           // Reverse fade effect
                }
                FastLED.setBrightness(brightness);
                FastLED.show();
            }
            else if(patternType == 2)                                       // Fade On/Off Red/Blue Police Pattern
            {
                lightsOff = false;
                if(accessLEDCAnalog == 1)
                {
                    ledcAnalogWrite(LEDCchan, 0);                           // Only Need to do this ONCE
                    accessLEDCAnalog = 0;
                }
                
                brightness += fadeInterval;                                 // Adjust brightness by fadeInterval
                if(brightness <= 0)                                         // Only change color if value <= 0
                {
                    brightness = 0;
                    fadeInterval = -fadeInterval;                           // Reverse fade effect
                    swap = !swap;                                           // swap colors
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
                    fadeInterval = -fadeInterval;                           // Reverse fade effect
                }
                
                FastLED.setBrightness(brightness);
                FastLED.show();
            }
            else if(patternType == 3)                                       // Rotate Colors w/o fade
            {
                lightsOff = false;
                if(accessLEDCAnalog == 1)
                {
                    ledcAnalogWrite(LEDCchan, 0);                           // Only Need to do this ONCE
                    accessLEDCAnalog = 0;
                }
                
                brightness = brightVal;                                     // Pull value from global integer
                hueVal += fadeInterval;                                     // Change color based on global value
                if(hueVal >= 255)
                {
                    hueVal = 0;                         
                }
                
                leds[0] = CHSV(hueVal, 255, 255);                           // Rotate Colors 0 - 255
                FastLED.setBrightness(brightness);
                FastLED.show();
            }
            else if(patternType == 4)                                       // Blue LED (Pin 13) Fades on/off
            {
                lightsOff = false;
                if(accessLEDCAnalog == 0)
                {
                    leds[0] = CRGB::Black;                                  // Turn Off RGB LED
                    FastLED.show();
                    accessLEDCAnalog = 1;                                   // Only need to do this ONCE
                }

                brightness += fadeInterval;                                 // Adjust brightness by fadeInterval
                if(brightness <= 0)                                         // Reverse fade effect at min/max values
                {
                    brightness = 0;
                    fadeInterval = -fadeInterval;
                }
                else if(brightness >= 255)
                {
                    brightness = 255;
                    fadeInterval = -fadeInterval;
                }
                
                ledcAnalogWrite(LEDCchan, brightness);                      // Set brightness on LEDC channel 0
            }
            else if(patternType == 5)                                       // Blue LED (pin 13) Cycles on/off
            {
                lightsOff = false;
                if(accessLEDCAnalog == 0)
                {
                    leds[0] = CRGB::Black;                                  // Turn Off RGB LED
                    FastLED.show();
                    accessLEDCAnalog = 1;                                   // Only need to do this ONCE
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
            else                                                            // Turn Off Everything
            {
                if(lightsOff == false)
                {
                    lightsOff = true;
                    if(accessLEDCAnalog == 0)
                    {
                        leds[0] = CRGB::Black;                              // Turn Off RGB LED
                        FastLED.show();
                    }
                    else // RGB LED already off
                    {
                        ledcAnalogWrite(LEDCchan, 0);                       // Turn off Blue LED
                    }
                    Serial.println("Invalid Pattern...Turning Lights Off!!\n");
                }
            }
        }
        vTaskDelay(delayInterval / portTICK_PERIOD_MS);                 // CLI adjustable delay (non blocking)
    }
}

void setup()
{
    msgQueue = xQueueCreate(QueueSize, sizeof(Message));                // Instantiate message queue
    cmdQueue = xQueueCreate(QueueSize, sizeof(Command));                // Instantiate command queue

    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("\n\n=>> FreeRTOS RGB LED Color Wheel Demo <<=");

    FastLED.addLeds <CHIPSET, RGB_LED, COLOR_ORDER> (leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(75);
    leds[0] = CRGB::White;                                              // Power up all Pin 2 LEDs for Power On Test
    FastLED.show();
    
    ledcSetup(LEDCchan, LEDCfreq, LEDCtimer);                           // Setup LEDC timer 
    ledcAttachPin(BLUE_LED, LEDCchan);                                  // Attach timer to LED pin
    vTaskDelay(2000 / portTICK_PERIOD_MS);                              // 2 Second Power On Delay

    Serial.println("Power On Test Complete...Starting Tasks");

    leds[0] = CRGB::Black;
    FastLED.show();
    
    vTaskDelay(500 / portTICK_PERIOD_MS);                               // 0.5 Second off before Starting Tasks

    xTaskCreatePinnedToCore(                                            // Instantiate CLI Terminal
        userCLITask,
        "Serial CLI Terminal",
        2048,
        NULL,
        1,
        NULL,
        app_cpu
    );

    Serial.println("User CLI Task Instantiation Complete");

    xTaskCreatePinnedToCore(                                            // Instantiate Message RX Task
        msgRXTask,
        "Receive Messages",
        2048,
        NULL,
        1,
        NULL,
        app_cpu
    );

    Serial.println("Message RX Task Instantiation Complete");

    xTaskCreatePinnedToCore(                                            // Instantiate LED fade task
        RGBcolorWheelTask,
        "Fade and Rotate RGB",
        2048,
        NULL,
        1,
        NULL,
        app_cpu
    );

    Serial.println("RGB LED Task Instantiation Complete");              // debug

    Serial.print("\n\nEnter \'delay xxx\' to change RGB Fade Speed.\n");
    Serial.print("Enter \'fade xxx\' to change RGB Fade Amount.\n");
    Serial.print("Enter \'pattern xxx\' to change RGB Pattern.\n");
    Serial.print("Enter \'bright xxx\' to change RGB Brightness (Only Pattern 3).\n");
    Serial.print("Enter \'cpu xxx\' to change CPU Frequency.\n");
    Serial.print("Enter \'values\' to retrieve current delay, fade, pattern & bright values.\n");
    Serial.print("Enter \'freq\' to retrieve current CPU, XTAL & APB Frequencies.\n\n");

    vTaskDelete(NULL);                                                  // Self Delete setup() & loop()
}

void loop() {}