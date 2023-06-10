/**
 * Joel Brigida
 * June 9, 2023
 * This is a multicore version of `09c-ISR-ADC-buffer-sample-avg`
 * The ISR will perform the sampling of the ADC on one core,
 * The CLI Terminal will be handled on the other core.
*/

#include <Arduino.h>
//#include <semphr.h>                                                           // Only for Vanilla FreeRTOS

static const BaseType_t PRO_CPU = 0;
static const BaseType_t APP_CPU = 1;

enum { BUF_LEN = 10 };                                                          // # of elements in sample buffer
enum { MSG_LEN = 100 };                                                         // Max chars in message body
enum { MSG_QUEUE_LEN = 5 };                                                     // # of elements in Message Queue
enum { CMD_BUF_LEN = 255 };                                                     // # of chars in command buffer

static const char avgCmd[] = "avg";
static const uint16_t timerDivider = 8;                                         // 80MHz / 8 = 10MHz
static const uint64_t timerMaxCount = 1000000;                                  // Timer counts to this value
static const uint32_t CLIdelay = 10;
static const int ADCpin = A0;                                                   // ADC = GPIO_26 = A0;

static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;
static hw_timer_t *timer = NULL;
static TaskHandle_t processTask = NULL;
static SemaphoreHandle_t semDoneReading = NULL;
static QueueHandle_t msgQueue;

static volatile uint16_t buf0[BUF_LEN];
static volatile uint16_t buf1[BUF_LEN];
static volatile uint16_t *writeTo = buf0;
static volatile uint16_t *readFrom = buf1;
static volatile uint8_t bufOverrun = 0;                                         // Flag for buffer overrun.
static float ADCavg;

struct Message
{
    char msgBody[MSG_LEN];
};

void IRAM_ATTR swap()                                                           // Called by ISR
{
    volatile uint16_t *tempPtr;                                                 // Swap read/write buffers
    tempPtr = writeTo;
    writeTo = readFrom;
    readFrom = tempPtr;
}

void IRAM_ATTR ISRtimer()                                                       // ISR Interrupt Function
{
    static uint16_t index = 0;
    BaseType_t taskWoken = pdFALSE;

    if((index < BUF_LEN) && (bufOverrun == 0))
    {
        writeTo[index++] = analogRead(ADCpin);                                  // Store ADC values to buffer
    }
    
    if(index >= BUF_LEN)                                                        // If buffer is full
    {
        if(xSemaphoreTakeFromISR(semDoneReading, &taskWoken) == pdFALSE)        // if reading is not done, buffer overrun flag is set & samples are dropped
        {
            bufOverrun = 1;
        }
        if(bufOverrun == 0)
        {
            index == 0;                                                         // swap buffers & reset index
            swap();
            vTaskNotifyGiveFromISR(processTask, &taskWoken);
        }
    }
    if(taskWoken)
    {
        portYIELD_FROM_ISR();                                                   // ESP-IDF
    }
    //portYIELD_FROM_ISR(taskWoken);                                            // Vanilla FreeRTOS
}

void CLItask(void *param)
{
    Message rxMsg;
    char input;
    char cmdBuffer[CMD_BUF_LEN];
    uint8_t index = 0;
    uint8_t cmdLength = strlen(avgCmd);

    memset(cmdBuffer, 0, CMD_BUF_LEN);                                          // Clear the buffer

    for(;;)
    {
        if(xQueueReceive(msgQueue, (void *)&rxMsg, 0) == pdTRUE)
        {
            Serial.println(rxMsg.msgBody);                                      // Print received messages
        }
        
        if(Serial.available() > 0)
        {
            input = Serial.read();                                              // Read characters from terminal
            
            if(index < CMD_BUF_LEN - 1)                                         // if buffer is not full (255 char)
            {
                cmdBuffer[index++] = input;                                     // Store characters in buffer
            }

            if(input == '\n')                                                   // When user presses enter key
            {
                Serial.print("\n");                                             // print newline in Terminal
                
                cmdBuffer[index - 1] = '\0';                                    // NULL terminate buffer string
                
                //if(strcmp(cmdBuffer, avgCommand) == 0)                        // DIDNT WORK. REF: https://cplusplus.com/reference/cstring/strcmp/
                if(memcmp(cmdBuffer, avgCmd, cmdLength) == 0)                   // If User Enters "avg" into CLI: If no characters differ (strings are equal)
                {
                    Serial.print("Average ADC Value: ");                        // REF: https://cplusplus.com/reference/cstring/memcmp/
                    Serial.println(ADCavg);                                     // print ADC average value
                }
                else                                                            // Echo user message to the Terminal
                {
                    Serial.print("User Entered: ");
                    strcpy(rxMsg.msgBody, cmdBuffer);                           // Print user message to terminal
                    xQueueSend(msgQueue, (void *)&rxMsg, 10);                   // Send to msgQueue
                }
                memset(cmdBuffer, 0, CMD_BUF_LEN);                              // Clear the buffer
                index = 0;                                                      // Reset index
            }
            else // if(input != '\n')
            {
                Serial.print(input);                                            // Echo user entered characters to the Terminal
            }
        }
        vTaskDelay(CLIdelay / portTICK_PERIOD_MS);                              // Yield to other tasks for a short while
    }
}

void calcAvg(void *param)
{
    Message someMsg;
    float localADCavg;
    int i;

    timer = timerBegin(0, timerDivider, true);                                  // Instantiate Timer w/ divider: startVal = 0, dividerVal = 80, countUp = True
    timerAttachInterrupt(timer, &ISRtimer, true);                               // Attach ISR to timer function, risingEdge = True
    timerAlarmWrite(timer, timerMaxCount, true);                                // Trigger timer @ timerMaxCount, autoReload = True
    timerAlarmEnable(timer);                                                    // Allow ISR to trigger via timer

    for(;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);                                // Wait for notification from ISR (similar to binary semaphore but FASTER)
        localADCavg = 0.0;
        
        for(i = 0; i < BUF_LEN; i++)
        {
            localADCavg += (float)readFrom[i];
            //vTaskDelay(105 / portTICK_PERIOD_MS);                             // Uncomment to test overrun flag
        }
        localADCavg /= BUF_LEN;                                                 // average all readings

        portENTER_CRITICAL(&spinlock);                                          // Enter critical section: updating a shared float may take multiple instructions
        ADCavg = localADCavg;                                                   // Update global variable
        portEXIT_CRITICAL(&spinlock);                                           // Exit critical section

        if(bufOverrun == 1)
        {
            strcpy(someMsg.msgBody, "ERROR: BUFFER OVERRUN!! SAMPLES DROPPED!!");
            xQueueSend(msgQueue, (void *)&someMsg, 10);
        }

        portENTER_CRITICAL(&spinlock);                                          // Critical Section: Can substitute w/ Mutex
        bufOverrun = 0;                                                         // Clearing overrun flag & giving back "doneReading" semaphore must not be interrupted
        xSemaphoreGive(semDoneReading);
        portEXIT_CRITICAL(&spinlock);
    }
}


void setup()
{
    semDoneReading = xSemaphoreCreateBinary();
    if(semDoneReading == NULL)
    {
        Serial.print("\n\nERROR: Could not instantiate semaphore\n\n");
        ESP.restart();
    }

    xSemaphoreGive(semDoneReading);                                             // Initialize the 'Done Reading' semaphore to 1
    msgQueue = xQueueCreate(MSG_QUEUE_LEN, sizeof(Message));

    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.print("\n\n=>> FreeRTOS Multicore ADC Sample & Average w/ CLI <<=\n\n");

    xTaskCreatePinnedToCore(                                                    // CLI Terminal Runs on Core 1
        CLItask,
        "CLI Terminal",
        1536,
        NULL,
        2,
        NULL,
        APP_CPU
    );

    xTaskCreatePinnedToCore(
        calcAvg,
        "ADC Average",
        1536,
        NULL,
        1,
        &processTask,
        PRO_CPU
    );

    vTaskDelete(NULL);    
}

void loop() {}