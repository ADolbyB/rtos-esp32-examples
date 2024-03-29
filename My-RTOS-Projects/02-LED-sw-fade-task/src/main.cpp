/**
 * Joel Brigida
 * 5/30/2023
 * ESP32 LEDC function Software Fade using 'ledcWrite' function
 * This example fades the on-board LED (pin 13) using a single task.
 * The Serial Terminal accepts integer values to change the speed of the fading effect
 */
#include <Arduino.h>

#if CONFIG_FREERTOS_UNICORE
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;                            // use only CPU core #1
#endif

static const int LEDCchan = 0;                                      // use LEDC Channel 0
static const int LEDCtimer = 12;                                    // 12-bit precision LEDC timer
static const int LEDCfreq = 5000;                                   // 5000 Hz LEDC base freq.
static const int LEDpin = LED_BUILTIN;                              // Use pin 13 on-board LED for SW fading
static const uint8_t bufLen = 20;                                   // Buffer Length setting for user CLI terminal

static int brightness = 0;                                          // LED brightness
static int fadeInterval = 5;                                        // LED fade interval
static int delayInterval = 30;                                      // Delay between changing fade intervals

void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255)// 'value' must be between 0 & 'valueMax'
{
    uint32_t duty = (4095 / valueMax) * min(value, valueMax);       // calculate duty cycle: 2^12 - 1 = 4095
    ledcWrite(channel, duty);                                       // write duty cycle to LEDC
}

void LEDfadeTask(void *param)
{
    for(;;)
    {
        ledcAnalogWrite(LEDCchan, brightness);                      // Set brightness on LEDC channel 0
        brightness += fadeInterval;                                 // Adjust brightness by fadeInterval

        if (brightness <= 0 || brightness >= 255)                   // Reverse fade effect at min/max values
        {
            fadeInterval = -fadeInterval;
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
    Serial.println("\n\n=>> FreeRTOS LED Fading Example <<=");
    
    ledcSetup(LEDCchan, LEDCfreq, LEDCtimer);                       // Setup LEDC timer 
    ledcAttachPin(LEDpin, LEDCchan);                                // Attach timer to LED pin

    Serial.println("LEDC Setup Complete: Creating Tasks...");       // debug

    xTaskCreatePinnedToCore(                                        // Instantiate LED fade task
        LEDfadeTask,
        "Fade LED On and Off",
        1536,
        NULL,
        1,
        NULL,
        app_cpu
    );

    Serial.println("LEDC Task Instantiation Complete");             // debug

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

    vTaskDelete(NULL);                                              // Self Delete Setup() & Loop()
}

void loop() {}