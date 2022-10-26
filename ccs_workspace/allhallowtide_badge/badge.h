/**
 * \brief   2021 Allhallowtide badge global header.
 * \brief   Global defines, structs, and declarations for the 2021 Allhallowtide
 *          badge. Anything related to the global state of the badge should
 *          be defined here, unless it's used only in main.c.
 * \author  George Louthan <duplico@dupli.co>
 * \date    2021
 * \copyright MIT License.
 */

#ifndef BADGE_H_
#define BADGE_H_

/// Bling animation interval in seconds. preferably a power of 2.
#define BADGE_BLING_SECS 64
/// Temperature in Fahrenheit over which to unlock this animation.
#define BADGE_UNLOCK_TEMP_OVER_S00 95
/// Temperature in Fahrenheit under which to unlock this animation.
#define BADGE_UNLOCK_TEMP_UNDER_S01 55

/// MCLK rate in MHZ.
#define MCLK_FREQ_MHZ 8
/// SMCLK rate in Hz.
#define SMCLK_RATE_HZ 8000000

extern uint8_t button_state;
extern volatile uint8_t f_long_press;
extern volatile uint8_t f_second;
extern volatile uint8_t f_time_loop;

void badge_init();
void badge_button_press_short();

#endif /* BADGE_H_ */
