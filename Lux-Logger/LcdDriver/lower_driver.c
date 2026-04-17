// This code was ported from TI's sample code. See Copyright notice at the
// bottom of this file.

#include "Grlib/grlib/grlib.h"
#include "msp430fr6989.h"
#include <LcdDriver/lower_driver.h>
#include <stdint.h>


void HAL_LCD_PortInit(void) {
  /////////////////////////////////////
  // Configuring the SPI pins
  /////////////////////////////////////

  // Configure UCB0CLK/P1.4 pin to serial clock
  // Configure UCB0SIMO/P1.6 pin to SIMO
  // For MSP430FR6989: P1SEL1=0, P1SEL0=1 for these functions
  P1SEL1 &= ~(BIT4 | BIT6);
  P1SEL0 |= (BIT4 | BIT6);

  ///////////////////////////////////////////////
  // Configuring the display's other pins
  ///////////////////////////////////////////////

  // Set DC (P2.3), Reset (P2.4), and CS (P2.6) as outputs
  P2DIR |= BIT3 | BIT4 | BIT6;

  return;
}

void HAL_LCD_SpiInit(void) {
  //////////////////////////
  // SPI configuration
  //////////////////////////

  // Put eUSCI in reset state and set all fields in the register to 0
  UCB0CTLW0 = UCSWRST;

  // Set clock phase to "capture on 1st edge" (UCCKPH = 1)
  // Set MSB first (UCMSB = 1)
  // Set MCU to SPI master (UCMST = 1)
  // Set to 3-pin SPI (UCMODE_0)
  // Set to synchronous mode (UCSYNC = 1)
  // Set clock to SMCLK (UCSSEL__SMCLK)
  UCB0CTLW0 |= UCCKPH | UCMSB | UCMST | UCSYNC | UCSSEL__SMCLK;

  // Configure the clock divider
  // SMCLK is 16 MHz; Divider of 2 gives us 8 MHz SPI clock
  UCB0BRW = 2;

  // Exit the reset state at the end of the configuration
  UCB0CTLW0 &= ~UCSWRST;

  // Initialize Control Pins
  P2OUT &= ~BIT6; // Set CS' (chip select) bit to 0 (display always enabled)
  P2OUT |= BIT4;  // Bring Reset high (inactive)
  P2OUT &= ~BIT3; // Set DC' bit to 0 (assume command initially)

  return;
}

//*****************************************************************************
// Writes a command to the CFAF128128B-0145T.  This function implements the
// basic SPI interface to the LCD display.
//*****************************************************************************
void HAL_LCD_writeCommand(uint8_t command) {
  // Wait as long as the module is busy
  while (UCB0STATW & UCBUSY)
    ;

  // For command, set the DC' bit to low before transmission
  P2OUT &= ~BIT3;

  // Transmit data
  UCB0TXBUF = command;

  return;
}

//*****************************************************************************
// Writes a data to the CFAF128128B-0145T.  This function implements the basic
// SPI interface to the LCD display.
//*****************************************************************************
void HAL_LCD_writeData(uint8_t data) {
  // Wait as long as the module is busy
  while (UCB0STATW & UCBUSY)
    ;

  // Set DC' bit back to high
  P2OUT |= BIT3;

  // Transmit data
  UCB0TXBUF = data;

  return;
}

/* --COPYRIGHT--,BSD
 * Copyright (c) 2015, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
