/**
 * Joel Brigida
 * 5/30/2023
 * ESP32 LEDC function Software Fade using 'ledcWrite' function
 * This example fades the on-board LED (pin 13) using a single task.
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

static int brightness = 0;                                          // LED brightness
static int fadeInterval = 5;                                        // LED fade interval

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
        
        vTaskDelay(30 / portTICK_PERIOD_MS);                        // 30ms delay (non blocking)
    }
}

void setup()
{
    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("\n\n=>> FreeRTOS LED Fading Example <<=");
    
    ledcSetup(LEDCchan, LEDCfreq, LEDCtimer);                       // Setup LEDC timer 
    ledcAttachPin(LEDpin, LEDCchan);                                // Attach timer to LED pin

    Serial.println("LEDC Setup Complete: Creating Task...");        // debug

    xTaskCreatePinnedToCore(                                        // Instantiate task for LED fading
        LEDfadeTask,
        "Fade LED On and Off",
        1536,
        NULL,
        1,
        NULL,
        app_cpu
    );

    Serial.println("LEDC Task Instantiation Complete");

    vTaskDelete(NULL);                                              // Self Delete Setup() & Loop()
}

void loop() {}