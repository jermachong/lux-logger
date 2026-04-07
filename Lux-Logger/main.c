#include <msp430fr6989.h>
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
int i2c_read_word(unsigned char i2c_address, unsigned char i2c_reg, unsigned int *data);
int i2c_write_word(unsigned char i2c_address, unsigned char i2c_reg, unsigned int data);

// Variables to track time
volatile int seconds = 0;
volatile int minutes = 0;
volatile int hours = 12;         // Default Start Time
volatile int log_ready = 0;      // Flag to trigger UART to print new time 

// Variables to facilitate logging
volatile unsigned int log_interval = 1; // Adjustable through Joystick: 1 (1s), 10 (10s), 3600 (1hr) 
volatile unsigned int interval_counter = 0;

// Variables To Store Data
// Array for one 8x8 of history 
unsigned int lux_history[64];    
int current_index = 0;           // Current pixel (0-63)
int display_page = 0;            // For replaying multiple pages

// Sensors Threshold Variables for Buzzer
int high_limit = 0;
int low_limit = 0;
unsigned char first_run = 1;


int main(void) {
    WDTCTL = WDTPW | WDTHOLD;    // Stop watchdog timer

    // Initialize all hardware 
    Initialize_UART();
    Initialize_I2C();
    Initialize_Timer();
    Initialize_ADC();            // Setup for Joystick
    Initialize_Buzzer();         // Setup for Buzzer
    Initialize_Display();        // Setup for Pixel Display

    // Unlock GPIO pins and enable interrupts
    PM5CTL0 &= ~LOCKLPM5;
    __enable_interrupt();

    
    
}