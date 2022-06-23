#pragma once

/* Badly named header to supply RTC settings for timer_if.c and rtc.c */

/* Includes ------------------------------------------------------------------*/
#include <stm32_def.h>

/* Private defines -----------------------------------------------------------*/
#define RTC_N_PREDIV_S 10
#define RTC_PREDIV_S ((1<<RTC_N_PREDIV_S)-1)
#define RTC_PREDIV_A ((1<<(15-RTC_N_PREDIV_S))-1)
