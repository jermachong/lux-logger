#include <msp430fr6989.h>
#include "LcdDriver/Crystalfontz128x128_ST7735.h"
#include "LcdDriver/HAL_MSP_EXP430FR6989_Crystalfontz128x128_ST7735.h"
#include "grlib.h"
#include <stdio.h>

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
void Initialize_UART(void);      // For PC Data Sync 
void Initialize_I2C(void);       // For the OPT3001 Lux Sensor
void Initialize_Timer(void);     // For thesystem clock 
void Initialize_ADC(void);       // For the Joystick: Adjusts Time
void Initialize_Buzzer(void);    // For Buzzer: High/Low pitch alerts (Lab 7.3)
void Initialize_Display(void);   // For Pixel Display grid 

// UART Functions
void uart_write_string(const char *str);
void uart_write_uint16(unsigned int n);
void uart_write_char(unsigned char ch);
unsigned char uart_read_char(void);

// I2C Functions
unsigned int i2c_read_lux_raw(void);
unsigned int convert_to_lux(unsigned int raw);

// Peripheral Functions
void play_alert(int is_high);

// Variables to track time
volatile int seconds = 0;
volatile int minutes = 0;
volatile int hours = 12;         // Default Start Time
volatile int log_ready = 0;      // Flag to trigger UART to print new time 

// Variables to facilitate logging
volatile unsigned int log_interval = 1; // Adjustable through Joystick: 1 (1s), 10 (10s), 3600 (1hr) 
volatile unsigned int interval_counter = 0;

// Variables To Store Data
// Array size of 256 to support 4 pages (8x8 = 64 pixels per page)
unsigned int lux_history[256];    
int current_index = 0;           
int display_page = 0;            // For replaying multiple pages

// Graphics Context for the screen
Graphics_Context g_sContext;

// Sensors Threshold Variables for Buzzer
int high_limit = 0;
int low_limit = 0;
unsigned char first_run = 1;


int main(void) {
    WDTCTL = WDTPW | WDTHOLD;    

    // Initialize all hardware 
    Initialize_Clock_System();   // Boost to 16MHz for Display and UART
    Initialize_UART();
    Initialize_I2C();
    Initialize_Timer();
    Initialize_ADC();            
    Initialize_Buzzer();         
    Initialize_Display();        

    // Unlock GPIO pins and enable interrupts
    PM5CTL0 &= ~LOCKLPM5;
    __enable_interrupt();

    for(;;) {
        // Logging Logic triggered by the Timer ISR based on log_interval
        if (log_ready) {
            unsigned int raw = i2c_read_lux_raw();
            unsigned int current_lux = convert_to_lux(raw);

            // Buzzer Threshold
            if (!first_run) {
                if (current_lux > high_limit) play_alert(1);      
                else if (current_lux < low_limit) play_alert(0);  
            }

            // Update limits for next pass (25% window)
            high_limit = current_lux + (current_lux / 4);
            low_limit = current_lux - (current_lux / 4);
            first_run = 0;

            // Store data in Array and update the index
            lux_history[current_index] = current_lux;
            //Calulation to get the index 
            current_index = (current_index + 1) % 256; 

            log_ready = 0; 
        }

        // Joystick and Display
        //The ADC reading and SPI display updates here
    }
}


// Function Implements


void Initialize_Clock_System() {
    // 16MHz setup required for the LCD and UART
    FRCTL0 = FRCTLPW | NWAITS_1;
    CSCTL0 = CSKEY;
    CSCTL1 = DCOFSEL_4 | DCORSEL;
    CSCTL3 = DIVS__1 | DIVM__1;
    CSCTL0_H = 0;
}


// I2C Initialization logic
void Initialize_I2C(void) {
    UCB1CTLW0 |= UCSWRST;                   
    // Set I2C, Master, SMCLK
    UCB1CTLW0 |= UCMODE_3 | UCMST | UCSSEL__SMCLK;
    // Divider for 100kHz when SMCLK is 16MHz
    UCB1BRW = 160;                          
    // Set pins from doc P4.0 = SDA, P4.1 = SCL
    P4SEL0 |= BIT0 | BIT1;
    P4SEL1 &= ~(BIT0 | BIT1);
    // Take out of reset
    UCB1CTLW0 &= ~UCSWRST;                 
}


// Function to get the raw data from the sensor
unsigned int i2c_read_lux_raw(void) {
    unsigned int raw_data;
    // Set the sensor address
    UCB1I2CSA = 0x44;                       

    // Point to Result Register
    UCB1CTLW0 |= UCTR | UCTXSTT;            // Set to transmit and start
    while(!(UCB1IFG & UCTXIFG));            // Wait for TX buffer to be ready
    UCB1TXBUF = 0x00;                       // Send the address of the result register
    while(UCB1CTLW0 & UCTXSTT);             // Wait for start bit to clear

    // Restart and Receive Data
    UCB1CTLW0 &= ~UCTR;                     // Set to receive mode
    UCB1CTLW0 |= UCTXSTT;                   // Send the restart bit
    while(UCB1CTLW0 & UCTXSTT);             // Wait for restart to clear

    while(!(UCB1IFG & UCRXIFG));            // Wait for first byte (MSB)
    raw_data = (UCB1RXBUF << 8);            // Shift to the upper 8 bits
    UCB1CTLW0 |= UCTXSTP;                   // Send the stop bit
    while(!(UCB1IFG & UCRXIFG));            // Wait for second byte (LSB)
    raw_data |= UCB1RXBUF;                  // Combine it with the MSB
    
    return raw_data;                        // Return the raw 16-B word
}


// Convert raw data to Lux
unsigned int convert_to_lux(unsigned int raw) {
    // Extract bits 15-12
    unsigned int exponent = (raw >> 12);  
    // Mask to keep only bits 11-0   
    unsigned int result = (raw & 0x0FFF);    
    // Formula: Lux = 0.01 * (2^{E}) * (R)
    return (result * (1 << exponent)) / 100;
}


// Timer Initialization logic
void Initialize_Timer(void) {
    TA1CTL = TASSEL__ACLK | MC__UP;         // Use TA1 to avoid Buzzer conflict on TA0
    TA1CCR0 = 32768;                        // Set to 1 second
    TA1CCTL0 |= CCIE;                       // Enable the timer interrupt
}


// Timer ISR to handle the logging intervals
#pragma vector=TIMER1_A0_VECTOR
__interrupt void Timer_A1_ISR(void) {
    interval_counter++;                     // Add 1 every second
    
    // Check if its time to log based on joystick 
    if (interval_counter >= log_interval) {
        log_ready = 1;                      // Set the flag to log data
        interval_counter = 0;               // Start the counter over
    }
}


// ** GOTTA WORK ON THIS **
// Display Initialization logic
void Initialize_Display(void) {
    // Wake up the LCD screen
    Crystalfontz128x128_Init();             
    // Set view to vertical
    Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP); 

    // Set up the context for drawing
    Graphics_initContext(&g_sContext, &g_sCrystalfontz128x128);
    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_GREEN); // Green text/blocks
    Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLACK); // Black background
    Graphics_clearDisplay(&g_sContext);      // Wipe the screen clean
}


void Initialize_Buzzer(void) {
    P1DIR |= BIT4;                          // Set P1.4 as output
    P1SEL0 |= BIT4;                         
}


void play_alert(int is_high) {
    // High or Low pitch
    TA0CCR0 = is_high ? 500 : 2000;         
    TA0CCTL1 = OUTMOD_7;           
    // Half volume         
    TA0CCR1 = TA0CCR0 / 2;                  
     // Delay for short beep
    __delay_cycles(800000);                
    TA0CCTL1 = OUTMOD_0;                    
}


void Initialize_UART(void) {
    UCA0CTLW0 |= UCSWRST;                   
    UCA0CTLW0 |= UCSSEL__SMCLK;        
    // 9600 Baud at 16MHz     
    UCA0BRW = 104;                         
    UCA0MCTLW = 0x1100 | UCOS16 | 0x0020;   
    P2SEL0 |= BIT0 | BIT1;                  // UART Pins
    P2SEL1 &= ~BIT0;
    UCA0CTLW0 &= ~UCSWRST;                  
}


void Initialize_ADC(void) {
    ADC12CTL0 |= ADC12ON;                   
    ADC12CTL1 |= ADC12SHP;                  
    P8SEL0 |= BIT4 | BIT5;                  // Joystick X/Y
    P8SEL1 |= BIT4 | BIT5;                  
    ADC12MCTL0 |= ADC12INCH_13;             
}


// UART Functions 
void uart_write_char(unsigned char ch) {
    while(!(UCA0IFG & UCTXIFG));
    UCA0TXBUF = ch;
}


void uart_write_string(const char *str) {
    while(*str) uart_write_char(*str++);
}


void uart_write_uint16(unsigned int n) {
    char buf[10];
    sprintf(buf, "%u", n);
    uart_write_string(buf);
}