// Standard includes
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// Driverlib includes
#include "rom.h"
#include "rom_map.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_types.h"
#include "hw_ints.h"
#include "uart.h"
#include "spi.h"
#include "interrupt.h"
#include "pin_mux_config.h"
#include "utils.h"
#include "prcm.h"
#include "hw_apps_rcm.h"
#include "gpio.h"
#include "hw_nvic.h"
#include "systick.h"

// Common interface include
#include "uart_if.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1351.h"

//*****************************************************************************
//                          MACROS
//*****************************************************************************
#define APPLICATION_VERSION  "1.4.0"
#define APP_NAME             "IR Decoding"
#define CONSOLE              UARTA0_BASE
#define UART1           UARTA1_BASE
#define SPI_IF_BIT_RATE      100000


//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
// Binary & HEX from manual decoding
    /* Button 0     =   0110 1110 1111 | 6EF        // SPACE
       Button 1     =   1111 1110 1111 | FEF
       Button 2     =   0111 1110 1111 | 7EF        // ABC
       Button 3     =   1011 1110 1111 | BEF        // DEF
       Button 4     =   0011 1110 1111 | 3EF        // GHI
       Button 5     =   1101 1110 1111 | DEF        // JKL
       Button 6     =   0101 1110 1111 | 5EF        // MNO
       Button 7     =   1001 1110 1111 | 9EF        // PQRS
       Button 8     =   0001 1110 1111 | 1EF        // TUV
       Button 9     =   1110 1110 1111 | EEF        // WXYZ
       Button LAST  =   0010 0010 1111 | 22F
       Button MUTE  =   1101 0110 1111 | D6F
    */
 // ASCII Characters
    /*
        DEC         HEX
        0           SPACE
    */
// some helpful macros for systick

// the cc3200's fixed clock frequency of 80 MHz
// note the use of ULL to indicate an unsigned long long constant
#define SYSCLKFREQ 80000000ULL

// systick reload value set to 40ms period
// (PERIOD_SEC) * (SYSCLKFREQ) = PERIOD_TICKS
#define SYSTICK_RELOAD_VAL 3200000UL

#define TICKS_TO_US(ticks) \
    ((((ticks) / SYSCLKFREQ) * 1000000ULL) + \
    ((((ticks) % SYSCLKFREQ) * 1000000ULL) / SYSCLKFREQ))\


// track systick counter periods elapsed
// if it is not 0, we know the transmission ended
volatile int systick_cnt = 0;

volatile unsigned char pin_out_intflag;     // flag set when full IR code received
volatile int edge_counter = 0;              // count how many edges/pulses processed in current transmission
volatile int data = 0;                      // variable that holds the accumulated IR decoded bits
volatile bool reading_data = false;         // flag to indicate whether we are currently processing transmission
volatile int prev_data = -1; // <-- maybe set back to 0? set to -1 to try and fix indentation

volatile uint32_t global_time = 0;          //
volatile uint32_t prev_detection = 0;       //
volatile uint32_t prev_print = 0;
volatile uint32_t last_time_button_pressed = 0;

volatile int curr_cycle = -1;
volatile char curr_letter = 0;
volatile int x = 0, rx = 6;
volatile char msg[50];
char received_msg[50];
int l;
volatile bool message_sent = false;
volatile bool caps_lock = false;

#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif
//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************



//*****************************************************************************
//                      LOCAL DEFINITION
//*****************************************************************************
static void GPIOA0IntHandler(void) {
    unsigned long ulStatus;

    ulStatus = MAP_GPIOIntStatus (GPIOA0_BASE, true);
    MAP_GPIOIntClear(GPIOA0_BASE, ulStatus);        // clear interrupts on GPIOA0

    uint32_t pin_state = MAP_GPIOPinRead(GPIOA0_BASE, 0x8);
    if ((pin_state & 0x8) == 0) // Read falling edge
    {
        HWREG(NVIC_ST_CURRENT) = 1;
        systick_cnt = 0;

        // Initialize decoding variables on starting edge
        if (!reading_data)
        {
            reading_data = true;
            edge_counter = 0;
            data = 0;
        }
    }
    else // Read rising edge
    {
        if (reading_data)
        {
            // Measure time between falling and rising edges
            uint64_t delta = TICKS_TO_US(SYSTICK_RELOAD_VAL - SysTickValueGet());
            if (edge_counter == 0)
            {
                // Reset decoding variables on invalid start bit and exit
                if (delta <= 2000)
                {
                    reading_data = false;
                    edge_counter = 0;
                    return;
                }
            }
            else
            {
                // Write to data
                data <<= 1;
                if (delta <= 1000)
                {
                    data |= 1;
                }
            }
            edge_counter++;

            // Return the decoded signal if complete
            if (edge_counter == 13)
            {
                pin_out_intflag = 1;
                reading_data = false;
                edge_counter = 0;
            }
        }
    }
}

/**
 * Reset SysTick Counter
 */
static inline void SysTickReset(void) {
    // any write to the ST_CURRENT register clears it
    // after clearing it automatically gets reset without
    // triggering exception logic
    // see reference manual section 3.2.1
    HWREG(NVIC_ST_CURRENT) = 1;

    // clear the global count variable
    systick_cnt = 0;
}

/**
 * SysTick Interrupt Handler
 *
 * Keep track of whether the systick counter wrapped
 */
static void SysTickHandler(void) {
    // increment every time the systick handler fires
    systick_cnt++;
    global_time += 40;
}


void InitUART1(void)
{
    // Configure UART1 for 115200 baud, 8 data bits, 1 stop bit, no parity
    MAP_UARTConfigSetExpClk(UART1,
                            MAP_PRCMPeripheralClockGet(PRCM_UARTA1),
                            115200,
                            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
    MAP_UARTFIFOEnable(UART1);
    MAP_UARTEnable(UART1);
}

void UART1Send(const char *data)
{
    while (*data)
    {
        while (MAP_UARTBusy(UART1)); // Wait until UART is ready
        MAP_UARTCharPut(UART1, *data++);
    }
}

void UART1Receive(char *buffer)
{
    int index = 0;
    while (index < 20)
    {
        if (MAP_UARTCharsAvail(UART1)) {
            char received_char = MAP_UARTCharGetNonBlocking(UART1);
            if (received_char == '$') {break;}
            buffer[index++] = received_char;
        }
    }
    buffer[index] = '\0';
}

//*****************************************************************************
//
//! Application startup display on UART
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
static void
DisplayBanner(char * AppName)
{

    Report("\n\n\n\r");
    Report("\t\t *************************************************\n\r");
    Report("\t\t        CC3200 %s Application       \n\r", AppName);
    Report("\t\t *************************************************\n\r");
    Report("\n\n\n\r");
}

//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
static void
BoardInit(void)
{
/* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
  //
  // Set vector table base
  //
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}

/**
 * Initializes SysTick Module
 */
static void SysTickInit(void) {

    // configure the reset value for the systick countdown register
    MAP_SysTickPeriodSet(SYSTICK_RELOAD_VAL);

    // register interrupts on the systick module
    MAP_SysTickIntRegister(SysTickHandler);

    // enable interrupts on systick
    // (trigger SysTickHandler when countdown reaches 0)
    MAP_SysTickIntEnable();

    // enable the systick module itself
    MAP_SysTickEnable();
}
//*****************************************************************************
//
//! Main function handling the uart echo. It takes the input string from the
//! terminal while displaying each character of string. whenever enter command
//! is received it will echo the string(display). if the input the maximum input
//! can be of 80 characters, after that the characters will be treated as a part
//! of next string.
//!
//! \param  None
//!
//! \return None
//! 
//*****************************************************************************
void main()
 {
    unsigned long ulStatus;
    //
    // Initailizing the board
    //
    BoardInit();

    //
    // Muxing for Enabling UART_TX and UART_RX.
    //
    PinMuxConfig();

    InitUART1();

    // Enable SysTick
    SysTickInit();

    //
    // Initialising the Terminal.
    //
    InitTerm();
    //
    // Clearing the Terminal.
    //
    ClearTerm();
    DisplayBanner(APP_NAME);

    //
    // Reset the peripheral
    //
    MAP_PRCMPeripheralReset(PRCM_GSPI);

    MAP_SPIReset(GSPI_BASE);

    //
    // Configure SPI interface
    //
    MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
                     SPI_IF_BIT_RATE,SPI_MODE_MASTER,SPI_SUB_MODE_0,
                     (SPI_SW_CTRL_CS |
                     SPI_4PIN_MODE |
                     SPI_TURBO_OFF |
                     SPI_CS_ACTIVEHIGH |
                     SPI_WL_8));

    //
    // Enable SPI for communication
    //
    MAP_SPIEnable(GSPI_BASE);

    Adafruit_Init();
    fillScreen(0x0000);

    //
    // Register the interrupt handler
    //
    MAP_GPIOIntRegister(GPIOA0_BASE, GPIOA0IntHandler);

    //
    // Configure both edges interrupt on pin out
    //
    MAP_GPIOIntTypeSet(GPIOA0_BASE, 0x8, GPIO_BOTH_EDGES);

    ulStatus = MAP_GPIOIntStatus(GPIOA0_BASE, false);
    MAP_GPIOIntClear(GPIOA0_BASE, ulStatus);            // clear interrupts on GPIOA0

    // clear global variables
    pin_out_intflag = 0;

    // Enable pin out interrupt
    MAP_GPIOIntEnable(GPIOA0_BASE, 0x8);

    while (1) {
        if (pin_out_intflag) {
            pin_out_intflag = 0;    // clear flag
            if ((global_time - prev_detection) >= 200) { // Ignore repeated data after first signal
                prev_detection = global_time;

                // Erase previous message from screen
                if (message_sent) {
                    message_sent = false;
                    fillRect(0, 64, 128, 9, 0x0000);
                    x = 6;
                    msg[0] = '\0';
                }

                if (((data != prev_data) || (global_time - last_time_button_pressed >= 1500)) && (prev_data != 0xFEF) && (prev_data != 0xD6F) && (prev_data != 0x22F))
                {
                    int l = strlen(msg);
                    msg[l] = curr_letter;
                    msg[l + 1] = '\0';
                    x += 6;
                    curr_cycle = 0;
                }
                else
                {   // we press same button
                    curr_cycle++;
                }
                prev_data = data;
                last_time_button_pressed = global_time;

                switch(data) {
                    case 0x6EF: // space
                        curr_letter = 32;
                        break;
                    case 0xFEF:
                        caps_lock = !caps_lock;
                        curr_cycle--;
                        break;
                    case 0x7EF: // ABC
                        curr_cycle %= 3;
                        curr_letter = 'a' + curr_cycle - (caps_lock * 32);
                        break;
                    case 0xBEF: // DEF
                        curr_cycle %= 3;
                        curr_letter = 'd' + curr_cycle -(caps_lock * 32);
                        break;
                    case 0x3EF: // GHI
                        curr_cycle %= 3;
                        curr_letter = 'g' + curr_cycle - (caps_lock * 32);
                        break;
                    case 0xDEF: // JKL
                        curr_cycle %= 3;
                        curr_letter = 'j' + curr_cycle - (caps_lock * 32);
                        break;
                    case 0x5EF: // MNO
                        curr_cycle %= 3;
                        curr_letter = 'm' + curr_cycle - (caps_lock * 32);
                        break;
                    case 0x9EF: // PQRS
                        curr_cycle %= 4;
                        curr_letter = 'p' + curr_cycle - (caps_lock * 32);
                        break;
                    case 0x1EF: // TUV
                        curr_cycle %= 3;
                        curr_letter = 't' + curr_cycle - (caps_lock * 32);
                        break;
                    case 0xEEF: // WXYZ
                        curr_cycle %= 4;
                        curr_letter = 'w' + curr_cycle - (caps_lock * 32);
                        break;
                    case 0x22F: // delete
                        curr_letter = 32;
                        l = strlen(msg);
                        if (l > 0) {
                            msg[l - 1] = '\0';
                            x -= 6;
                        }
                        prev_data = data;
                        break;
                    case 0xD6F: // enter
                        l = strlen(msg);
                        msg[l] = '$';
                        msg[l + 1] = '\0';

                        Report("Message to be sent: %s\n\r", msg);
                        UART1Send(msg);
                        MAP_UtilsDelay(80000);
                        message_sent = true;
                        curr_cycle = -1;
                        break;
                }

                if (data != 0xD6F && data != 0xFEF) {
                    drawChar(x, 64, curr_letter, 0xFFFF, 0x0000, 1);
                }
            }
        }
        if (MAP_UARTCharsAvail(UART1))
        {
            UART1Receive(received_msg);
            Report("Received: %s\n\r", received_msg);
            fillRect(0, 0, 128, 9, 0x0000); // Erase received message from screen
            rx = 6;
            int i;
            for (i = 0; i < strlen(received_msg); i++) {
                drawChar(rx, 0, received_msg[i], 0xFFFF, 0x0000, 1);
                rx += 6;
            }
            memset(received_msg, 0, sizeof(received_msg));
            received_msg[0] = '\0';
        }
    }
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
