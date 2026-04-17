#include <msp430fr6989.h>
// #include "LcdDriver/Crystalfontz128x128_ST7735.h"
// #include "LcdDriver/HAL_MSP_EXP430FR6989_Crystalfontz128x128_ST7735.h"
#include "GrLib/grlib/grlib.h"
#include "LcdDriver/lcd_driver.h" // LCD driver
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FLAGS UCA1IFG      // Contains the transmit & receive flags
#define RXFLAG UCRXIFG     // Receive flag
#define TXFLAG UCTXIFG     // Transmit flag
#define TXBUFFER UCA1TXBUF // Transmit buffer
#define RXBUFFER UCA1RXBUF // Receive buffer

/**
 EEL 4742C - Lab Project: Visual Lux Logger
 Name: Parth Patel, Jeremy Achong
 Project Requirements :
 Uses Joystick
 Uses Pixel Display
 Uses Buzzer
 Adjustable Time Units (1s, 10s, 1hr)
 Replay multiple pages of history
 UART Dump/Populate via PC
 */

// Hardware Initializers
void Initialize_Clock_System(void);
void Initialize_UART(void);    // For PC Data Sync
void Initialize_I2C(void);     // For the OPT3001 Lux Sensor
void Initialize_Timer(void);   // For thesystem clock
void Initialize_ADC(void);     // For the Joystick: Adjusts Time
void Initialize_Buzzer(void);  // For Buzzer: High/Low pitch alerts (Lab 7.3)
void Initialize_Display(void); // For Pixel Display grid

void config_ACLK_to_32KHz_crystal();

// UART Functions
void uart_write_string(const char *str);
void uart_write_uint16(unsigned int n);
void uart_write_char(unsigned char ch);
unsigned char uart_read_char(void);

// I2C Functions
unsigned int i2c_read_lux_raw(void);
int i2c_read_word(unsigned char i2c_address, unsigned char i2c_reg,
                  unsigned int *data);
int i2c_write_word(unsigned char i2c_address, unsigned char i2c_reg,
                   unsigned int data);

unsigned int convert_to_lux(unsigned int raw);

// Peripheral Functions
void play_alert(int is_high);
uint32_t get_brightness(int lux);
void draw_log_grid();

// Variables to track time
volatile int seconds = 0;
volatile int minutes = 0;
volatile int hours = 12;    // Default Start Time
volatile int log_ready = 1; // Flag to trigger UART to print new time

// Variables to facilitate logging
volatile unsigned int log_interval = 1; // Adjustable through Joystick: 1s, 10s, 20s
volatile unsigned int interval_counter = 0;

// Variables To Store Data
// Array size of 256 to support 4 pages (8x8 = 64 pixels per page)
unsigned int lux_history[256];
int current_index = 0;
int display_page = 0; // For replaying multiple pages

uint16_t lux_log[256] = { 0 }; // Stores one page -> (8x8 grid)
uint8_t log_index = 0;       // Current position in the grid (0-63)

// Graphics Context for the screen
Graphics_Context g_sContext;

// Sensors Threshold Variables for Buzzer
int high_limit = 0;
int low_limit = 0;
unsigned char first_run = 1;

int main(void)
{
    char mystring[20];
    WDTCTL = WDTPW | WDTHOLD;
    // Unlock GPIO pins and enable interrupts
    PM5CTL0 &= ~LOCKLPM5;

    // Initialize all hardware
    Initialize_Clock_System(); // Boost to 16MHz for Display and UART
    //   config_ACLK_to_32KHz_crystal(); // Configure ACLK BEFORE timer to ensure
    // correct timing
    Initialize_UART();
    Initialize_I2C();
    Initialize_Timer();
    Initialize_ADC();
    Initialize_Buzzer();
    play_alert(1); // test
    Initialize_Display();

    // Set the default font for strings
    GrContextFontSet(&g_sContext, &g_sFontFixed6x8);
    // Print message
    Graphics_drawStringCentered(&g_sContext, "Welcome to", AUTO_STRING_LENGTH,
                                64, 30, OPAQUE_TEXT);
    sprintf(mystring, "Lux Logger!");
    Graphics_drawStringCentered(&g_sContext, mystring, AUTO_STRING_LENGTH, 64,
                                55,
                                OPAQUE_TEXT);
    // Adjusted for 16MHz (4s delay)
    __delay_cycles(64000000);
    Graphics_clearDisplay(&g_sContext); // Wipe the screen clean

    unsigned int light_data = 0, lux = 0;
    volatile unsigned int x_value;

    // RN=0111b=7 The LSB bit is worth 1.28
    // CT=0 Result produced in 100 ms
    // M=11b=3 Continuous readings
    // ME=1 Mask (hide) the Exponent from the result
    //  RN  CT M   ...        ME
    //   |  |  |              |
    //   v  v  v              v
    // 0111 0  11 0 0 0 0 1 0 1 00 = 0x7614
    i2c_write_word(0x44, 0x01, 0x7614);
    i2c_read_word(0x44, 0x00, &light_data);
    high_limit = light_data;
    low_limit = light_data;

    uart_write_string("*** Lux Logger ***\n\n");

    __enable_interrupt();
    config_ACLK_to_32KHz_crystal(); // Configure ACLK BEFORE timer to ensure

    for (;;)
    {
        if (log_ready)
        {
            i2c_read_word(0x44, 0x00, &light_data);
            // Comes from the OPT3001 datasheet
            lux = light_data * 1.28;
            uart_write_uint16(light_data);
            uart_write_string("\t");
            uart_write_uint16(lux);
            uart_write_string("\n");

            // Buzzer Threshold
            if (!first_run)
            {
                if (lux > high_limit)
                    play_alert(1); // Call High Pitch
                else if (lux < low_limit)
                    play_alert(0); // Call Low Pitch
            }

            // Update limits for next pass (25% window)
            high_limit = lux + (lux / 4);
            low_limit = lux - (lux / 4);
            first_run = 0;

            // Store data in Array and update the index
            lux_log[current_index] = lux;
            // Calulation to get the index (Wrap at 64 to keep current on grid)
            current_index = (current_index + 1) % 64;

            log_ready = 0;
            draw_log_grid();
        }

        // Joystick and Display
        // The ADC reading and SPI display updates here
        ADC12CTL0 |= ADC12SC;
        while (ADC12CTL0 & ADC12BUSY)
            ;
        x_value = ADC12MEM0; // receive x value of joystick
        if (x_value > 3000)
        {
            log_interval += 10;
            // Adjusted for 16MHz (0.5s debounce)
            __delay_cycles(8000000);
        }
        else if (x_value < 1000)
        {
            if (log_interval > 10)
                log_interval -= 10;
            // Adjusted for 16MHz (0.5s debounce)
            __delay_cycles(8000000);
        }

        uart_write_string("Logging Interval:\t");
        uart_write_uint16(log_interval);
        uart_write_string("\n");
    }
}

// Function Implements

void Initialize_Clock_System()
{
    // 16MHz setup required for the LCD and UART
    FRCTL0 = FRCTLPW | NWAITS_1;
    CSCTL0 = CSKEY;
    CSCTL1 = DCOFSEL_4 | DCORSEL;
    CSCTL3 = DIVS__1 | DIVM__1;
    CSCTL0_H = 0;
}

// I2C Initialization logic
void Initialize_I2C()
{
    P4SEL1 |= (BIT1 | BIT0);
    P4SEL0 &= ~(BIT1 | BIT0);
    UCB1CTLW0 = UCSWRST;
    UCB1CTLW0 |= UCMST | UCMODE_3 | UCSYNC | UCSSEL_3;
    // Divider for 16MHz to keep I2C at ~100kHz
    UCB1BRW = 160;
    UCB1CTLW0 &= ~UCSWRST;
}

// Writes a 16-bit word to a specific register on an I2C slave device
int i2c_write_word(unsigned char i2c_address, unsigned char i2c_reg,
                   unsigned int data)
{
    unsigned char byte1, byte2;

    // Prepare the transaction
    UCB1I2CSA = i2c_address;    // Set the target slave address
    byte1 = (data >> 8) & 0xFF; // MSByte
    byte2 = data & 0xFF;        // LSByte

    // Start Condition
    UCB1IFG &= ~UCTXIFG0;        // Clear the transmit interrupt flag
    UCB1CTLW0 |= (UCTR | UCTXSTT); // Set to Transmit mode and send START

    // Send the Register Address
    while ((UCB1IFG & UCTXIFG0) == 0)
        ; // Wait for the buffer to be ready
    UCB1TXBUF = i2c_reg;               // Send the internal register address

    // Sends Data (MSByte)
    while ((UCB1IFG & UCTXIFG0) == 0)
        ; // Wait for the buffer
    UCB1TXBUF = byte1;                 // Send MSB

    //Sends Data (LSByte)
    while ((UCB1IFG & UCTXIFG0) == 0)
        ; // Wait for the buffer
    UCB1TXBUF = byte2;                 // Send LSB

    // Ends transmission
    while ((UCB1IFG & UCTXIFG0) == 0)
        ; // Wait for the final byte to clear
    UCB1CTLW0 |= UCTXSTP;              // Create Stop condition

    // Clean up the bus
    while ((UCB1CTLW0 & UCTXSTP) != 0)
        ; // Wait for the Stop bit to be sent
    while ((UCB1STATW & UCBBUSY) != 0)
        ; // Check that I2C bus is not busy

    return 0;
}

int i2c_read_word(unsigned char i2c_address, unsigned char i2c_reg,
                  unsigned int *data)
{
    unsigned char byte1 = 0, byte2 = 0; // Intialize to ensure successful reading
    UCB1I2CSA = i2c_address; // Set address
    UCB1IFG &= ~UCTXIFG0;
    UCB1CTLW0 |= UCTR | UCTXSTT;
    while ((UCB1IFG & UCTXIFG0) == 0)
    {
    } // Wait for flag to raise
    UCB1TXBUF = i2c_reg; // Write in the TX buffer
    while ((UCB1IFG & UCTXIFG0) == 0)
    {
    }
    UCB1CTLW0 &= ~UCTR;
    UCB1CTLW0 |= UCTXSTT;
    while ((UCB1IFG & UCRXIFG0) == 0)
    {
    } // Wait for flag to raise
    byte1 = UCB1RXBUF;
    UCB1CTLW0 |= UCTXSTP;
    while ((UCB1IFG & UCRXIFG0) == 0)
    {
    } // Wait for flag to raise
    byte2 = UCB1RXBUF;
    while ((UCB1CTLW0 & UCTXSTP) != 0)
    {
    }
    while ((UCB1STATW & UCBBUSY) != 0)
    {
    }
    *data = (byte1 << 8) | (byte2 & (unsigned int) 0x00FF);
    return 0;
}

// Timer Initialization logic
// Use Timer A1 for the 1second logging because Timer A0 is being used to drive the Buzzer 
void Initialize_Timer(void)
{
    TA1CTL = TASSEL__ACLK | MC__UP; // Use TA1 to avoid Buzzer conflict on TA0
    TA1CCR0 = 32768;            // Set to 1 second (ACLK is independent of MCLK)
    TA1CCTL0 |= CCIE;               // Enable the timer interrupt
}

// Timer ISR to handle the logging intervals
#pragma vector = TIMER1_A0_VECTOR
__interrupt void Timer_A1_ISR(void)
{
    interval_counter++; // Add 1 every second

    if (interval_counter >= log_interval)
    {
        log_ready = 1;        // Set the flag to log data
        interval_counter = 0; // Start the counter over
    }
}

// Display Initialization logic
void Initialize_Display(void)
{
    Crystalfontz128x128_Init();
    Crystalfontz128x128_SetOrientation(0);

    Graphics_initContext(&g_sContext, &g_sCrystalfontz128x128);
    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
    Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
    Graphics_clearDisplay(&g_sContext);
}

void Initialize_Buzzer(void)
{
    // Buzzer: simple GPIO output (P2.7)
    P2DIR |= BIT7;
    P2OUT &= ~BIT7; // start OFF
}

void play_alert(int is_high)
{
    int i;
//  const int delay ;
    // High Pitch = faster toggling, Low Pitch = slower toggling
    if (is_high)
        for (i = 0; i < 200; i++)
        {
            P2OUT ^= BIT7;
            __delay_cycles(2000);
        }
    else
        for (i = 0; i < 200; i++)
        {
            P2OUT ^= BIT7;
            __delay_cycles(5000);
        }
    // Generate tone by toggling GPIO

    // Ensure buzzer is OFF at end
    P2OUT &= ~BIT7;
}

// Configure UART
// 9600 baud, 8-bit data, LSB first, no parity bits, 1 stop bit
void Initialize_UART(void)
{
    P3SEL1 &= ~(BIT4 | BIT5);
    P3SEL0 |= (BIT4 | BIT5);
    UCA1CTLW0 = UCSWRST;
    UCA1CTLW0 |= UCSSEL_2; // Set clock to SMCLK which is now at 16MHz

    // Optimized values for 16MHz @ 9600 Baud:
    UCA1BRW = 104;         // Prescaler (16MHz / 16 / 9600 = 104.16)
    UCA1MCTLW = UCBRF_2 | 0xD600 | UCOS16; // Modulators for 16MHz

    UCA1CTLW0 &= ~UCSWRST;
}

void Initialize_ADC()
{
    P9SEL1 |= BIT2;
    P9SEL0 |= BIT2;
    ADC12CTL0 |= ADC12ON;
    ADC12CTL0 &= ~ADC12ENC;
    ADC12CTL0 |= ADC12SHT0_4;
    ADC12CTL1 |= ADC12SHP | ADC12SSEL_0;
    ADC12CTL2 |= ADC12RES_2;
    ADC12MCTL0 |= ADC12INCH_10;
    ADC12CTL0 |= ADC12ENC;
}

uint32_t get_brightness(int lux)
{
    int intensity = (lux * 255) / 4095;
    if (intensity > 255)
        intensity = 255; // Safety cap
    return ((uint32_t) intensity << 16) | ((uint32_t) intensity << 8)
            | intensity;
}

void draw_log_grid()
{
    uint8_t row, col, i;
    Graphics_Rectangle rect;

    // The Look-Up Table (Size 10): Maps directly to indexes 0 through 9
    uint32_t colors[10] = {
        0x00000000, 0x001C1C1C, 0x00393939, 0x00555555, 0x00717171,
        0x008E8E8E, 0x00AAAAAA, 0x00C6C6C6, 0x00E3E3E3, 0x00FFFFFF
    };

    for (i = 0; i < 64; i++)
    {
        row = i / 8; // Row number (0-7)
        col = i % 8; // Column number (0-7)

        rect.xMin = col * 16;
        rect.yMin = row * 16;
        rect.xMax = rect.xMin + 15;
        rect.yMax = rect.yMin + 15;

        // 1. Grab the current lux value and clamp it to a maximum of 600
        uint16_t current_lux = lux_log[i];
        if (current_lux > 600)
        {
            current_lux = 600;
        }

        // 2. Scale the lux (0 - 600) to an array index (0 - 9)
        uint8_t color_index = (current_lux * 9) / 600;

        // 3. Fill the square with the exact color from the Look-Up Table
        Graphics_setForegroundColor(&g_sContext, colors[color_index]);
        Graphics_fillRectangle(&g_sContext, &rect);

        // 4. Switch color to draw the grid lines
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);

        // 5. Draw the outline
        Graphics_drawRectangle(&g_sContext, &rect);
    }
}

void uart_write_char(unsigned char ch)
{
    while ((FLAGS & TXFLAG) == 0)
    {
    }
    TXBUFFER = ch;
}

void uart_write_string(const char *str)
{
    while (*str)
        uart_write_char(*str++);
}

void uart_write_uint16(unsigned int n)
{
    char str[6];
    unsigned int i = 0, j, k = 0;
    if (n == 0)
    {
        str[i++] = '0';
    }
    else
        while (n > 0)
        {
            str[i++] = n % 10 + '0';
            n /= 10;
        }
    str[i] = '\0';
    for (j = 0, k = i - 1; j < k; j++, k--)
    {
        char temp = str[j];
        str[j] = str[k];
        str[k] = temp;
    }
    uart_write_string(str);
}

// Configures ACLK to 32 KHz crystal
void config_ACLK_to_32KHz_crystal()
{
    PJSEL1 &= ~BIT4;
    PJSEL0 |= BIT4;
    CSCTL0 = CSKEY;

    do
    {
        CSCTL5 &= ~LFXTOFFG;
        SFRIFG1 &= ~OFIFG;
    }
    while ((CSCTL5 & LFXTOFFG) != 0);
    CSCTL0_H = 0;
}
