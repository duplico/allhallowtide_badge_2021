/**
 * \brief   2021 Allhallowtide badge main.
 * \brief   Main entry point for 2021 Allhallowtide Party electronic badge.
 * \author  George Louthan <duplico@dupli.co>
 * \date    2021
 * \copyright MIT License.
 */

// MSP430 main header
#include <msp430fr2633.h>

// C headers
#include <stdint.h>

// Accessory headers
#include <driverlib.h>

// Local headers

// Interrupt flags
volatile uint8_t f_time_loop = 0;

/// Initialize clock signals and the three system clocks.
/**
 ** We'll take the DCO to 16 MHz, and divide it by 2 for MCLK.
 ** Then we'll divide MCLK by 1 to get 8 MHz SMCLK.
 **
 ** Our available clock sources are:
 **  VLO:     10kHz very low power low-freq
 **  REFO:    32.768kHz (typ) reference oscillator
 **  DCO:     Digitally controlled oscillator (1MHz default)
 **           Specifically, 1048576 Hz typical.
 **
 ** At startup, our clocks are as follows:
 **  MCLK:  Sourced by the DCO
 **         (Available: DCO, REFO, VLO)
 **  SMCLK: Sourced from MCLK, with no divider
 **         (Available dividers: {1,2,4,8})
 **  ACLK: Sourced from REFO
 **         (the only available internal source)
 */
void init_clocks() {
    ///// DCO  (Digitally-controlled oscillator)

    // Configure FRAM wait state (set to 1 to support 16MHz MCLK)
    FRCTL0 = FRCTLPW | NWAITS_1;

    // Recommended procedure for setting up these clocks is located in the
    //  user's guide, SLAU445i, page 106, and the formula for DCOCLK is at
    //  page 104.
    // Set DCOCLKDIV to 16 MHz.
    __bis_SR_register(SCG0);                // disable FLL
    CSCTL3 |= SELREF__REFOCLK;              // Set REFO as FLL reference source
    CSCTL0 = 0;                             // clear DCO and MOD registers
    CSCTL1 &= ~(DCORSEL_7);                 // Clear DCO frequency select bits
    CSCTL1 |= DCORSEL_5;                    // Set DCO = 16MHz range
    // CSCTL feedback loop:
    CSCTL2 = FLLD__1 + 487;                  // DCODIV = /1

    // f_DCOCLK = 2^FLLD x (FLLN + 1) x (f_FLLREFCLK / FLLREFDIV)
    // f_DCOCLKDIV =       (FLLN + 1) x (f_FLLREFCLK / FLLREFDIV)
    //    15990784 =          488     x (   32768    /    1     )

    __delay_cycles(3);
    __bic_SR_register(SCG0);                // enable FLL
    while(CSCTL7 & (FLLUNLOCK0 | FLLUNLOCK1)); // Poll until FLL is locked

    // SYSTEM CLOCKS
    // =============

    // CSCTL5 has a reset value of 0x0000; no need to clear it.

    // MCLK (8 MHz)
    //  All sources but MODOSC are available at up to /128
    //  Set to DCOCLKDIV/2 = 8 MHz
    // DIVM__2;


    // SMCLK (8 MHz)
    //  Derived from MCLK with divider up to /8
    //  Set to MCLK/2 = 8 MHz
    // DIVS__1;

    CSCTL5 = VLOAUTOOFF_H | DIVS__1 | DIVM__2;
}

/// Apply the initial configuration of the GPIO and peripheral pins.
/**
 **
 */
void init_io() {
    // Per datasheet S4.6, p20, unused pins should be switched to outputs.

    // IO:
    // P1.0     unused      (SEL 00; DIR 1)
    // P1.1     UCB0 SCLK   (SEL 01; DIR 1)
    // P1.2     UCB0SIMO    (SEL 01; DIR 0)
    // P1.3     unused      (SEL 00; DIR 1)
    // P1.4     UCA0 TXD    (SEL 01; DIR 1)
    // P1.5     UCA0 RXD    (SEL 01; DIR 0)
    // P1.6     unused      (SEL 00; DIR 1)
    // P1.7     unused      (SEL 00; DIR 1)

    P1DIR = 0b11011011;
    P1SEL0 = 0b00110110; // LSB
    P1SEL1 = 0b00000000; // MSB
    P1REN = 0x00;
    P1OUT = 0x00;

    // Init P2 and P3 as unused:
    P2DIR = 0xFF;
    P2SEL0 = 0x00;
    P2SEL1 = 0x00;
    P2OUT = 0x00;

    P3DIR = 0xFF;
    P3SEL0 = 0x00;
    P3SEL1 = 0x00;
    P3OUT = 0x00;

    // Unlock the pins from high-impedance mode:
    // (AKA the MSP430FR magic make-it-work command)
    PM5CTL0 &= ~LOCKLPM5;
}

/// Initialize the timer for the time loop.
void init_timers() {
    // For our timer, we're going to use ACLK, which is sourced from REFO.
    //  (REFO is 32k)
    // We'd like to have this run at like 60-100 Hz, I think.
    // We'll divide our 32k clock by 64 to get 512 Hz.
    // Then, we'll use a period of 8 to get 64ish frames per second.
    Timer_A_initUpModeParam next_channel_timer_init = {};
    next_channel_timer_init.clockSource = TIMER_A_CLOCKSOURCE_ACLK;
    next_channel_timer_init.clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_64;
    next_channel_timer_init.timerPeriod = 8;
    next_channel_timer_init.timerInterruptEnable_TAIE = TIMER_A_TAIE_INTERRUPT_DISABLE;
    next_channel_timer_init.captureCompareInterruptEnable_CCR0_CCIE = TIMER_A_CCIE_CCR0_INTERRUPT_ENABLE;
    next_channel_timer_init.timerClear = TIMER_A_SKIP_CLEAR;
    next_channel_timer_init.startTimer = false;

    Timer_A_initUpMode(TIMER_A0_BASE, &next_channel_timer_init);
    Timer_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);
}

/// Make snafucated.
int main(void) {
    WDTCTL = WDTPW | WDTHOLD;

    init_clocks();
    init_io();
    init_timers();

    __bis_SR_register(GIE);

    // TODO:
    // ht16d_init();

    WDTCTL = WDTPW | WDTSSEL__ACLK | WDTIS__32K | WDTCNTCL; // 1 second WDT

    while(1)
    {
        // Check whether the time loop flag has been set; this is our
        //  main animation and debouncing loop.
        if (f_time_loop) {
            // First off, pat the dog.
            WDTCTL = WDTPW | WDTSSEL__ACLK | WDTIS__32K | WDTCNTCL; // 1 second WDT

            // Service the LED animation timestep.
            // leds_timestep();

            f_time_loop = 0;
        }

        __bis_SR_register(LPM0_bits);
    } // End background loop
}

// NB: In the Timer ISRs, for historical reasons, the vectors are named
//      in a confusing way.
//
// **** TL;DR: Timer A0 is TIMER0_A0_xxx; Timer A1 is TIMER1_A0_xxx.
//
//     This is, apparently, because originally devices only had a single
//      Timer A, Timer B, etc. So, the CCR registers' index determined
//      the major number: TIMER_A0 (Timer A, CCR0); TIMER_A1 (Timer A, CCR1),
//      etc. But now, devices like this one have multiple Timer As. So,
//      the naming convention must be Timer0_A... for Timer A0, and
//      Timer1_A... for A1, etc.
//     Anyway, that's why it looks like this.

// Dedicated ISR for Timer A0 CCR0. Vector is cleared on service.
#pragma vector=TIMER0_A0_VECTOR
__interrupt void TIMER0_A0_ISR_HOOK(void)
{
    f_time_loop = 1;
    LPM0_EXIT;
}
