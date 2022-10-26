/**
 * \brief   2021 Allhallowtide HT16D35A driver.
 * \brief   LED controller driver for the HT16D35A, for use in the 2021
 *          Allhallowtide badge. This driver handles the low-level interaction
 *          with the LED controller itself; the application-level animations
 *          are left to different code.
 * \author  George Louthan <duplico@dupli.co>
 * \date    2021
 * \copyright MIT License.
 */

/// Low-level driver for the HT16D35B LED controller.
/**
 ** This file is mostly used by `leds.c`, which is the high-level LED animation
 ** module. Most functions in this module are preceded by `ht16d_` and
 ** correspond to the direct control of the LED controller's functions, and the
 ** mapping between our LED layout and its.
 ** \file ht16d35b.c
 ** \author George Louthan
 ** \date   2018
 ** \copyright (c) 2018 George Louthan @duplico. MIT License.
 */

#include <stdint.h>

#include <driverlib.h>
#include <msp430.h>

#include "badge.h"

#include "ht16d35a.h"

// Command definitions:

/// Write the buffer that follows to display memory.
#define HTCMD_WRITE_DISPLAY 0x80
#define HTCMD_READ_DISPLAY  0x81
/// Read the status register.
#define HTCMD_READ_STATUS   0x71
/// Command to toggle between binary and grayscale mode.
#define HTCMD_BWGRAY_SEL    0x31
/// Payload for `HTCMD_BWGRAY_SEL` to select binary (black & white) mode.
#define HTCMD_BWGRAY_SEL_BINARY 0x01
/// Payload for `HTCMD_BWGRAY_SEL` to select 6-bit grayscale mode.
#define HTCMD_BWGRAY_SEL_GRAYSCALE 0x00
/// Select the number of COM (column) pins in use.
#define HTCMD_COM_NUM       0x32
/// Control blinking.
#define HTCMD_BLINKING      0x33
/// System and oscillator control command.
#define HTCMD_SYS_OSC_CTL   0x35
/// Set the constant-current ratio.
#define HTCMD_I_RATIO       0x36
/// Set the global brightness (0x40 is max).
#define HTCMD_GLOBAL_BRTNS  0x37
#define HTCMD_MODE_CTL      0x38
#define HTCMD_COM_PIN_CTL   0x41
#define HTCMD_ROW_PIN_CTL   0x42
#define HTCMD_DIR_PIN_CTL   0x43
/// Command to order a software reset of the HT16D35B.
#define HTCMD_SW_RESET      0xCC
/// The number of RGB (3-channel) LEDs in the system.
#define HT16D_LED_COUNT 9

/// 8-bit values for the RGB LEDs.
/**
 ** This is a HT16D_LED_COUNT-element array of 3-tuples of RGB color (1 byte /
 ** 8 bits per channel). Note that, in this array, all 8 bits are "significant";
 ** the right-shifting by two is done in `led_send_gray()`, to send 6-bit data
 ** to the LED controller, because it only has 6 bits of grayscale.
 **
 */
uint8_t ht16d_gs_values[HT16D_LED_COUNT][3] = {0,};

/// Correlate our LED_ID,COLOR to COL,ROW.
/**
 ** Note that the HT16D35B does include a feature to handle this mapping for us
 ** onboard the chip. We are currently not using it, but there's not really
 ** any reason that we couldn't.
 */
const uint8_t ht16d_col_mapping[1][HT16D_LED_COUNT*3][2] = {{{0, 2}, {0, 1}, {0, 0}, {1, 2}, {1, 1}, {1, 0}, {2, 2}, {2, 1}, {2, 0}, {3, 2}, {3, 1}, {3, 0}, {4, 2}, {4, 1}, {4, 0}, {5, 2}, {5, 1}, {5, 0}, {6, 2}, {6, 1}, {6, 0}, {7, 2}, {7, 1}, {7, 0}, {8, 2}, {8, 1}, {8, 0}}};

/// Initialize GPIO and SPI peripheral.
void ht16d_init_peripheral() {
    // Configure the eUSCI.
    UCB0CTLW0 = UCSWRST;  // Shut down USCI_B0 and clear CTLW0.

    UCB0CTLW0 |= UCCKPH; // Capture on first, change on next
    UCB0CTLW0 &= ~UCCKPL; // Idle LOW polarity
    UCB0CTLW0 |= UCMSB; // MSB first.
    UCB0CTLW0 |= UCSYNC; // SYNCHRONOUS mode (SPI)
    UCB0CTLW0 |= UCSSEL_2; // SMCLK source
    UCB0CTLW0 |= UCMST; // Master mode.

    UCB0BRW = 0x04; // Baudrate is SMCLK/4 = 2 MHz

    UCB0CTLW0 &= ~UCSWRST; // enable it.
}

/// Transmit a `len` byte array `txdat` to the HT16D35B.
void ht16d_send_array(uint8_t txdat[], uint8_t len) {
    // TODO: Convert to SPI

    // CS low
    P1OUT &= ~BIT0;

    // Send bits
    for (uint8_t i=0; i<len; i++) {
        // Wait for the TX buffer to become available again.
        while (!(UCB0IFG & UCTXIFG)); // While TX is unavailable, spin.

        UCB0IFG &= ~UCTXIFG; // Clear TX flag.
        UCB0TXBUF = txdat[i]; // write dat.
    }

    // CS high
    P1OUT |= BIT0;
}

/// Transmit a single byte command to the HT16D35B.
void ht16d_send_cmd_single(uint8_t cmd) {
    ht16d_send_array(&cmd, 1);
}

/// Transmit two bytes to the HT16D35B.
void ht16_d_send_cmd_dat(uint8_t cmd, uint8_t dat) {
    uint8_t v[2];
    v[0] = cmd;
    v[1] = dat;
    ht16d_send_array(v, 2);
}

/// Initialize the HT16D35B, and enable the eUSCI for talking to it.
/**
 ** Specifically, we initialize the device with the following characteristics:
 ** * All LEDs off
 ** * All rows in use except for 27, 26, 22, and 21
 ** * Grayscale mode
 ** * No fade, UCOM, USEG, or matrix masking
 ** * Global brightness to `HT16D_BRIGHTNESS_DEFAULT`
 ** * Only columns 0, 1, and 2 in use
 ** * Maximum constant current ratio
 ** * HIGH SCAN mode (common-anode on columns)
 */
void ht16d_init() {
    // On POR:
    //  All registers reset to default, but DDRAM not cleared
    //  Oscillator off
    //  COM and ROW high impedance
    //  LED display OFF.

    ht16d_init_peripheral();

    // SW Reset (HTCMD_SW_RESET)
    ht16d_send_cmd_single(HTCMD_SW_RESET);

    // Set global brightness
    ht16_d_send_cmd_dat(HTCMD_GLOBAL_BRTNS, HT16D_BRIGHTNESS_DEFAULT);
    // Set BW/Binary display mode.
    ht16_d_send_cmd_dat(HTCMD_BWGRAY_SEL, HTCMD_BWGRAY_SEL_GRAYSCALE);
    // Set column pin control for in-use cols (HTCMD_COM_PIN_CTL)
    // TODO: variable for this:
    ht16_d_send_cmd_dat(HTCMD_COM_PIN_CTL, 0b0000001);
    // Set constant current ratio (HTCMD_I_RATIO)
    ht16_d_send_cmd_dat(HTCMD_I_RATIO, 0b0111); // 0b000 (max) is :fire: :fire:
    // Set columns to 3 (0--2), and HIGH SCAN mode (HTCMD_COM_NUM)
    ht16_d_send_cmd_dat(HTCMD_COM_NUM, 0x02);

    // Set ROW pin control for in-use rows (HTCMD_ROW_PIN_CTL)
    uint8_t row_ctl[] = {HTCMD_ROW_PIN_CTL, 0b01111111, 0xff, 0xff, 0xff};
    ht16d_send_array(row_ctl, 5);
    ht16_d_send_cmd_dat(HTCMD_SYS_OSC_CTL, 0b10); // Activate oscillator.

    ht16d_all_one_color(128,128,128); // Turn off all the LEDs.

    ht16_d_send_cmd_dat(HTCMD_SYS_OSC_CTL, 0b11); // Activate oscillator & display.
}

/// Set the global brightness of the display module.
/**
 **
 ** This is a scale of 0 to 64. This is the WRONG way to turn all the lights
 ** off, so expected values to this function should be between
 ** HT16D_BRIGHTNESS_MIN (1) and HT16D_BRIGHTNESS_MAX (64). This function DOES
 ** do bounds checking.
 **
 ** \param brightness The new brightness value from 1 to 64.
 **
 */
void ht16d_set_global_brightness(uint8_t brightness) {
    if (brightness > HT16D_BRIGHTNESS_MAX)
        brightness = HT16D_BRIGHTNESS_MAX;
    ht16_d_send_cmd_dat(HTCMD_GLOBAL_BRTNS, brightness);
}

/// Transmit the data currently in `led_values` to the LED controller.
/**
 ** Here, and only here, we also convert the LED channel brightness values
 ** from 8-bit to 6-bit.
 */
void ht16d_send_gray() {
    // We must send a column at a time.
    uint8_t light_array[30] = {HTCMD_WRITE_DISPLAY, 0x00, 0};

    // TODO: this time, we're only using one column...
    uint8_t col = 0;
    {
        light_array[1] = 0x20*col;
        // TODO: account for multiple columns:
        for (uint8_t row=0; row<28; row++) {
            uint8_t led_num = ht16d_col_mapping[col][row][0];
            uint8_t rgb_num = ht16d_col_mapping[col][row][1];

            light_array[row+2] = ht16d_gs_values[led_num][rgb_num]>>2;
        }
        ht16d_send_array(light_array, 30);
    }
}

/// Set some of the colors, but don't send them to the LED controller.
void ht16d_put_colors(uint8_t id_start, uint8_t id_len, rgbcolor16_t* colors) {
    if (id_start >= HT16D_LED_COUNT || id_start+id_len > HT16D_LED_COUNT) {
        return;
    }
    for (uint8_t i=0; i<id_len; i++) {
        ht16d_gs_values[(id_start+i)][0] = (uint8_t)(colors[i].r >> 7);
        ht16d_gs_values[(id_start+i)][1] = (uint8_t)(colors[i].g >> 7);
        ht16d_gs_values[(id_start+i)][2] = (uint8_t)(colors[i].b >> 7);
    }
}

/// Set some of the colors, and immediately send them to the LED controller.
void ht16d_set_colors(uint8_t id_start, uint8_t id_len, rgbcolor16_t* colors) {
    ht16d_put_colors(id_start, id_len, colors);
    ht16d_send_gray();
}

/// Set all LEDs to the same R,G,B colors.
void ht16d_all_one_color(uint8_t r, uint8_t g, uint8_t b) {
    for (uint8_t i=0; i<HT16D_LED_COUNT; i++) {
        ht16d_gs_values[i][0] = r;
        ht16d_gs_values[i][1] = g;
        ht16d_gs_values[i][2] = b;
    }

    ht16d_send_gray();

}

void ht16d_standby() {
    ht16_d_send_cmd_dat(HTCMD_SYS_OSC_CTL, 0b00); // Deactivate everything.
}
void ht16d_display_off() {
    ht16_d_send_cmd_dat(HTCMD_SYS_OSC_CTL, 0b10); // Activate oscillator.
}
void ht16d_display_on() {
    ht16_d_send_cmd_dat(HTCMD_SYS_OSC_CTL, 0b11); // Activate osc & display.
}
