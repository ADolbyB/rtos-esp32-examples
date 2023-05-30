/**
 * Joel Brigida
 * May 29, 2023
 * This demo program uses a Hardware ISR to sample the ADC value at 10Hz and add that value
 * to a buffer. After the buffer has 10 values, Task A wakes up and computes the average, which
 * is a global floating point variable. Task B handles the Serial Terminal.
 * When the user enters "avg" into the serial terminal, the latest average value
 * of the ADC will be output. Any other message entered into the serial terminal will just
 * be echoed back to the terminal.
 */

#include <Arduino.h>
//#include <semphr.h>                                           // Only for Vanill FreeRTOS

#if CONFIG_FREERTOS_UNICORE
    static const BaseType_t app_cpu = 0;
#else
    static const BaseType_t app_cpu = 1;                        // Use only Core 1
#endif

enum { BUF_LEN = 10 };                                          // 10 elements for ADC samples
enum { MSG_LEN = 100 };                                         // Maximum # of char in struct message body
enum { MSG_QUEUE_LEN = 5 };                                     // 5 elements in message queue
enum { CMD_BUF_LEN = 255 };                                     // Max char in CLI message body

static const char termCommand[] = "avg";                        // Terminal Command to display Average ADC value
static const uint16_t timerDivider = 8;                         // Timer counts at 10MHz
static const uint64_t timerMaxCount = 1000000;                  // 0.1 sec @ 10MHz
static const uint32_t CLIdelay = 25;                            // delay for printing user CLI messages & for task yielding
static const int ADCpin = A0;                                   // A0 = ADC2_CH0: GPIO 26 on ESP32

static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;    // Declare spinlock mutex for ISR critical section
static hw_timer_t *timer = NULL;                                // Delare ESP32 HAL timer (part of Arduino Library)
static TaskHandle_t processingTask = NULL;                      // Declare Task Notification for ADC ISR
static SemaphoreHandle_t semDoneReading = NULL;                 // Declare Semaphore for when ADC is done being read
static QueueHandle_t msgQueue;                                  // Declare queue for CLI messages

static volatile uint16_t buf0[BUF_LEN];                         // 1st Buffer for ADC values
static volatile uint16_t buf1[BUF_LEN];                         // 2nd Buffer for ADC values
static volatile uint16_t *writeTo = buf0;                       // pointer to buffer buf0
static volatile uint16_t *readFrom = buf1;                      // pointer to buffer buf1
static volatile uint8_t bufOverrun = 0;                         // Flag for Double buffer overrun
static float ADCavg;                                            // Calculated average for 10 ADC samples

struct Message 
{
    char msgBody[MSG_LEN];                                      // Queue elements for error messages
};

void IRAM_ATTR swap();                                          // Called from ISR to swap buffer pointers
void IRAM_ATTR ISRtimer();                                      // Timer function stored in RAM
void userCLI(void *param);                                      // Function for Serial Terminal CLI
void calcAvg(void *param);                                      // Calculate average of 10 ADC values

void setup()
{
    msgQueue = xQueueCreate(MSG_QUEUE_LEN, sizeof(Message));    // Instantiate queue to hold CLI commands
    semDoneReading = xSemaphoreCreateBinary();                  // Instantiate semaphore to signal when ISR is done reading ADC

    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("\n=>> FreeRTOS ADC Sample & Average Demo w/ CLI <<=");

    if(semDoneReading == NULL)                                  // Restart ESP32 if the semaphore can't be created
    {
        Serial.println("ERROR: COULD NOT INSTANTIATE SEMAPHORE");
        Serial.println("RESTARTING....");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ESP.restart();                                          // Restart ESP32
    }

    xSemaphoreGive(semDoneReading);                             // Initialize the 'Done Reading' semaphore to 1

    xTaskCreatePinnedToCore(                                    // Instatiate task to handle the user CLI
        userCLI,
        "User CLI Terminal",
        1536,
        NULL,
        2,                                                      // Higher Priority, but only runs every 20ms
        NULL,
        app_cpu
    );

    xTaskCreatePinnedToCore(                                    // Instatiate task for ADC average value calculation
        calcAvg,
        "Calculate ADC Average",
        1536,
        NULL,
        1,
        &processingTask,                                        // Task Handle for notifications
        app_cpu
    );

    timer = timerBegin(0, timerDivider, true);                  // instantiate 100ms timer for ISR: (Start Value, divider, Count Up)
    timerAttachInterrupt(timer, &ISRtimer, true);               // Attach timer to ISR: (timer, function, Rising Edge)
    timerAlarmWrite(timer, timerMaxCount, true);                // Attach ISR trigger to timer: (timer, count, Auto Reload)
    timerAlarmEnable(timer);                                    // Enable ISR trigger
    
    vTaskDelete(NULL);                                          // Self delete setup() & loop() tasks
}

void loop() {}

void IRAM_ATTR swap()                                           // Function can be called from anywhere
{
    volatile uint16_t *tempPtr;                                 // Swaps the writeTo & readFrom pointers in the double buffer
    tempPtr = writeTo;
    writeTo = readFrom;
    readFrom = tempPtr;
}

void IRAM_ATTR ISRtimer()                                       // ISR function runs when timer reaches 'timerMaxCount' value & resets
{
    static uint16_t index = 0;                                  // counter for ADC readings
    BaseType_t taskWoken = pdFALSE;                             // Boolean for scheduler: pdTRUE when a new task can be unblocked & started immediately after a semaphore is given.

    if((index < BUF_LEN) && (bufOverrun == 0))                  // Read ADC if buffer is not overrun. Sample is dropped if buffer is overrun
    {
        writeTo[index] = analogRead(ADCpin);                    // read & store in the next buffer element
        index++;
    }

    if(index >= BUF_LEN)                                        // Check if buffer is full
    {
        if(xSemaphoreTakeFromISR(semDoneReading, &taskWoken) == pdFALSE)
        {                                                       // Non-critical section since its inside an ISR and they can't be interrupted.
            bufOverrun = 1;                                     // Set overrun flag if ADC reading is not done
        }
        if(bufOverrun == 0)
        {
            index = 0;                                          // Reset index
            swap();                                             // Swap buffers
            vTaskNotifyGiveFromISR(processingTask, &taskWoken); // Task notification: Like a binary semaphore but FASTER
        }
    }

    if(taskWoken)                                               // ESP-IDF
    {
        portYIELD_FROM_ISR();                                   // Exit from ISR
    }

    /** Vanilla FreeRTOS version:
     * portYIELD_FROM_ISR(taskWoken);                           // Exit from ISR (Vanilla FreeRTOS)
    */
}

void userCLI(void *param)
{
    Message rxMsg;                                              // struct for error messages
    char input;
    char commandBuf[CMD_BUF_LEN];                               // create 255 char buffer
    uint8_t index = 0;
    uint8_t cmdLen = strlen(termCommand);                       // cmdLen for "avg" command = 3

    memset(commandBuf, 0, CMD_BUF_LEN);                         // Clear the user CLI buffer
    
    for(;;)
    {
        if(xQueueReceive(msgQueue, (void *)&rxMsg, 0) == pdTRUE)// Check for any messages in Queue
        {
            Serial.println(rxMsg.msgBody);                      // print any messages to Terminal
        }

        if(Serial.available() > 0)
        {
            input = Serial.read();                              // Read characters from terminal
            
            if(index < CMD_BUF_LEN - 1)                         // if buffer is not full (255 char)
            {
                commandBuf[index] = input;                      // Store characters in buffer
                index++;
            }

            if(input == '\n')                                   // When user presses enter key
            {
                Serial.print("\n");                             // print newline in Terminal
                
                commandBuf[index] = '\0';                       // NULL terminate buffer string
                
                //if(strcmp(commandBuf, termCommand) == 0)      // DIDNT WORK. REF: https://cplusplus.com/reference/cstring/strcmp/
                if(memcmp(commandBuf, termCommand, cmdLen) == 0)// If User Enters "avg" into CLI: If no characters differ (strings are equal)
                {
                    Serial.print("Average ADC Value: ");        // REF: https://cplusplus.com/reference/cstring/memcmp/
                    Serial.println(ADCavg);                     // print ADC average value
                }
                else                                            // Echo user message to the Terminal
                {
                    Serial.print("User Entered: ");
                    strcpy(rxMsg.msgBody, commandBuf);          // Print user message to terminal
                    xQueueSend(msgQueue, (void *)&rxMsg, 10);   // Send to msg_queue
                }
                memset(commandBuf, 0, CMD_BUF_LEN);             // Clear the buffer
                index = 0;                                      // Reset index
            }
            else
            {
                Serial.print(input);                            // Echo user entered characters to the Terminal
            }
        }
        vTaskDelay(CLIdelay / portTICK_PERIOD_MS);              // Yield to other tasks (25ms) to prevent starving
    }
}

void calcAvg(void *param)
{
    Message errMsg;
    float localADCavg;
    int i;

    for(;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);                // Wait for notification from ISR (similar to binary semaphore but FASTER)
        localADCavg = 0.0;
        
        for(i = 0; i < BUF_LEN; i++)
        {
            localADCavg += (float)readFrom[i];                  // Sum ADC values in buffer
            //vTaskDelay(105 / portTICK_PERIOD_MS);             // Uncomment to test buffer overrun flag
        }
        localADCavg /= BUF_LEN;                                 // Calculate average

        portENTER_CRITICAL(&spinlock);                          // Enter critical section: updating a shared float may take multiple instructions
        ADCavg = localADCavg;                                   // Update global variable
        portEXIT_CRITICAL(&spinlock);                           // Exit critical section

        if(bufOverrun == 1)
        {
            strcpy(errMsg.msgBody, "ERROR: BUFFER OVERRUN!! SAMPLES DROPPED!!");
            xQueueSend(msgQueue, (void *)&errMsg, 10);
        }

        portENTER_CRITICAL(&spinlock);                          // Critical Section:
        bufOverrun = 0;                                         // Clearing overrun flag & giving back "doneReading" semaphore must not be interrupted
        xSemaphoreGive(semDoneReading);
        portEXIT_CRITICAL(&spinlock);
    }
}