/*--------------------------------------------------------
GEORGE MASON UNIVERSITY
ECE 447 - Lab 9
  Requires TFT LCD to be connected as follows
  P1.3 LCD Backlight - TA1.2
  P1.4 SPI SCLK      - UCB0CLK
  P1.6 SPI MOSI      - UCB0SIMO
  P1.7 SPI  MISO      - UCB0SOMI
  P2.5 SPI TFT CS
  P4.7 LCD D/C

  Test Data Output (4-bit counter at 1kHz)
  P9.1 LSB
  P9.2
  P9.3
  P9.4 MSB

  Logic Analyzer Data Inputs
  P3.0 LSB
  P3.1
  P3.2
  P3.3 MSB

Date:   Fall 2020
Author: Jens-Peter Kaps

Change Log:
20201101 Initial Version
--------------------------------------------------------*/

#include "lcd.h"
#include "graphics.h"
#include "color.h"
#include "ports.h"
#include "lab9.h"

unsigned char mult = 0x0F;
unsigned int hz = 10000;
unsigned int bits = 0;
unsigned char samp = 0;
uint8_t channel1[256];
uint8_t channel2[256];
uint8_t channel3[256];
uint8_t channel4[256];

void introScreen(void) {
    clearScreen(1);
	setColor(COLOR_16_WHITE);
	drawString(2, 2, FONT_MD, "CH");
	setColor(COLOR_16_RED);
	drawString(25,2, FONT_MD, "LOGIC ANALYZER");

	// Grid Lines
	setColor(COLOR_16_WHITE);
	drawLine(20,2,20,75);
	drawLine(1,15,160,15);

	// CH Nums
	setColor(COLOR_16_YELLOW);
	drawString(10,20, FONT_MD, "1");
	setColor(COLOR_16_BLUE);
	drawString(10,35, FONT_MD, "2");
	setColor(COLOR_16_GREEN);
	drawString(10,47, FONT_MD, "3");
	setColor(COLOR_16_PURPLE);
	drawString(10,60, FONT_MD, "4");
	setColor(COLOR_16_WHITE);
	drawLine(0,75,160,75);
	drawString(10, 80, FONT_SM, "Trigger: off");
	drawString(10, 100, FONT_SM, "Frequency: 10k Hz");
	drawString(10, 110, FONT_SM, "Samples: 256");
}

/*
 *  Needs to write data to the device using spi. We will only want to write to
 *  the device we wont worry the reads.
 */
void writeData(uint8_t data) {
    P2OUT &= ~LCD_CS_PIN;
    P4OUT |= LCD_DC_PIN;
    UCB0TXBUF = data;
}

/*
 *	Needs to write commands to the device using spi
 */
void writeCommand(uint8_t command) {
    P2OUT &= ~LCD_CS_PIN;
    P4OUT &= ~LCD_DC_PIN;
    UCB0TXBUF = command;
}

void initMSP430(void) {

    P3DIR &= ~(BIT0 | BIT1 | BIT2 | BIT3);
    TB0CTL = TBSSEL_2 | ID_0 | MC_1 | TBCLR;
    TB0CCR0 = 100;

    /*********************** Test Data Generator 1 kHz *********************/
    P9DIR   |= BIT1 | BIT2 | BIT3 | BIT4;               // Test Data Output
    TA2CTL   = TASSEL__SMCLK | MC__CONTINUOUS | TACLR;
    TA2CCR1  = 100;                                     // 1MHz * 1/1000 Hz
    TA2CCTL1 = CCIE;                                    // enable interrupts
    testcnt  = 0;                                       // start test counter at 0

    /**************************** PWM Backlight ****************************/
    P1DIR   |= BIT3;
	P1SEL0  |= BIT3;
	P1SEL1  &= ~BIT3;
    TA1CCR0  = 511;
	TA1CCTL2 = OUTMOD_7;
	TA1CCR2  = 255;
    TA1CTL   = TASSEL__ACLK | MC__UP | TACLR;

    /******************************** SPI **********************************/
    P2DIR  |=   LCD_CS_PIN;	                            // DC and CS
	P4DIR  |=   LCD_DC_PIN;
    P1SEL0 |=   LCD_MOSI_PIN | LCD_UCBCLK_PIN;          // MOSI and UCBOCLK
    P1SEL1 &= ~(LCD_MOSI_PIN | LCD_UCBCLK_PIN);

    UCB0CTLW0 |= UCSWRST;		// Reset UCB0
    /*
     * UCBxCTLW0 	 - eUSCI_Bx Control Register 0
     * UCSSEL__SMCLK - SMCLK in master mode
     * UCCKPL 		 - Clock polarity select
     * UCMSB		 - MSB first select
     * UCMST		 - Master mode select
     * UCMODE_0      - eUSCI mode 3-pin SPI select
     * UCSYNC		 -	Synchronous mode enable
     */
    UCB0CTLW0 |= UCSSEL__SMCLK | UCCKPH | UCMSB | UCMST | UCMODE_0 | UCSYNC;
    UCB0BR0   |= 0x01;         // Clock = SMCLK/1
    UCB0BR1    = 0;       
    UCB0CTL1  &= ~UCSWRST;     // Clear UCSWRST to release the eUSCI for operation
	PM5CTL0   &= ~LOCKLPM5;    // Unlock ports from power manager

	__enable_interrupt();
}

void main(void) {
    WDTCTL = WDTPW | WDTHOLD;	// kill the watchdog
    initMSP430();

    __delay_cycles(10);

    initLCD();

    introScreen();

    while (TRUE) {
        clearHelp(20,30,160,30);
        setColor(COLOR_16_YELLOW);
        drawLogicLine(20,20,160,10,channel1);
        setColor(COLOR_16_BLUE);
        drawLogicLine(20,35,160,10,channel2);
        setColor(COLOR_16_GREEN);
        drawLogicLine(20,47,160,10,channel3);
        setColor(COLOR_16_PURPLE);
        drawLogicLine(20,60,160,10,channel4);
        TB0CCTL0 |= CCIE;
    }
}

// Interrupt Service Routine for Timer B
#pragma vector = TIMER0_B0_VECTOR
__interrupt void TIMERB_ISR(void)
{
    if(hz == 10000) {
        // For every sample, setting one bit in channels
        channel1[samp] |= (P3IN & BIT0) * 0x0F;
        channel2[samp] |= ((P3IN & BIT1) / 0x02) * 0x0F;
        channel3[samp] |= ((P3IN & BIT2) / 0x04) * 0x0F;
        channel4[samp] |= ((P3IN & BIT3) / 0x08) * 0x0F;
        bits++;
        if(bits == 4 || bits == 8) {
            if(bits == 8) {
                if(samp > 256) {
                    samp = 0;
                }
                samp++;
                mult = 0x0F;
                bits = 0;
            }
            else {
                mult = 0xF0;
            }
        }
        TB0CCTL0 &= ~(CCIE);
    }
}

// Interrupt Service Routine for Timer A2
// Only used for test data generation
#pragma vector=TIMER2_A1_VECTOR
__interrupt void TIMER2_A1_ISR(void)
{
  switch(__even_in_range(TA2IV,2))
  {
    case  0: break;                             // No interrupt
    case  2:                                    // CCR1
        P9OUT   &= ((testcnt << 1) | 0xE1 );    // set 0 bits
        P9OUT   |= ((testcnt << 1) & 0x1E );    // set 1 bits
        testcnt ++;                             // increment counter
        testcnt &= 0x0F;                        // limit to 4 bits
        TA2CCR1 += 1000;                        // back here in 1000 timer cycles
        break;
    default: break;
  }
}
