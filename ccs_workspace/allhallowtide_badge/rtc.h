/// Header for real-time clock module.
/**
 ** \file rtc.h
 ** \author George Louthan
 ** \date   2022
 ** \copyright (c) 2022 George Louthan @duplico. MIT License.
 */

#ifndef RTC_H_
#define RTC_H_

extern volatile uint32_t rtc_seconds;
extern volatile uint8_t rtc_centiseconds;
extern uint8_t rtc_button_csecs;

void rtc_init();

#endif /* RTC_H_ */
