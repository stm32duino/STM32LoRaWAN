/**
  ******************************************************************************
  * @file    radio_conf.h
  * @author  MCD Application Team
  * @brief   Header of Radio configuration
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * Revised BSD License - https://spdx.org/licenses/BSD-3-Clause.html
  *
  * Redistribution and use in source and binary forms, with or without
  * modification, are permitted provided that the following conditions
  * are met:
  *
  *     1. Redistributions of source code must retain the above copyright notice,
  *        this list of conditions and the following disclaimer.
  *     2. Redistributions in binary form must reproduce the above copyright
  *        notice, this list of conditions and the following disclaimer in the
  *        documentation and/or other materials provided with the distribution.
  *     3. Neither the name of the copyright holder nor the names of its
  *        contributors may be used to endorse or promote products derived from this
  *        software without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
#ifndef __RADIO_CONF_H__
#define __RADIO_CONF_H__

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "subghz.h"
#include "radio_board_if.h"
#include "utilities_conf.h" /* For UTILS_*_CRITICAL_SECTION */

/**
  * @brief drive value used anytime radio is NOT in TX low power mode
  * @note override the default configuration of radio_driver.c
  */
#define SMPS_DRIVE_SETTING_DEFAULT  SMPS_DRV_40

/**
  * @brief drive value used anytime radio is in TX low power mode
  *        TX low power mode is the worst case because the PA sinks from SMPS
  *        while in high power mode, current is sunk directly from the battery
  * @note override the default configuration of radio_driver.c
  */
#define SMPS_DRIVE_SETTING_MAX      SMPS_DRV_60

/**
  * @brief Provides the frequency of the chip running on the radio and the frequency step
  * @remark These defines are used for computing the frequency divider to set the RF frequency
  * @note override the default configuration of radio_driver.c
  */
#define XTAL_FREQ                   ( 32000000UL )

/**
  * @brief in XO mode, set internal capacitor (from 0x00 to 0x2F starting 11.2pF with 0.47pF steps)
  * @note override the default configuration of radio_driver.c
  */
#define XTAL_DEFAULT_CAP_VALUE      ( 0x20UL )

/**
  * @brief voltage of vdd tcxo.
  * @note override the default configuration of radio_driver.c
  */
#define TCXO_CTRL_VOLTAGE           TCXO_CTRL_1_7V

/**
  * @brief Radio maximum wakeup time (in ms)
  * @note override the default configuration of radio_driver.c
  */
#define RF_WAKEUP_TIME              ( 1UL )

/**
  * @brief DCDC is enabled
  * @remark this define is only used if the DCDC is present on the board
  * (as indicated by RBI_IsDCDC())
  * @note override the default configuration of radio_driver.c
  */
#define DCDC_ENABLE                 ( 1UL )

/**
  * @brief disable the Sigfox radio modulation
  * @note enabled by default
  */
#define RADIO_SIGFOX_ENABLE 0

/**
  * @brief disable the radio generic features
  * @note enabled by default
  */
#define RADIO_GENERIC_CONFIG_ENABLE 0

/**
  * @brief Set RX pin to high or low level
  */
#define DBG_GPIO_RADIO_RX(set_rst)

/**
  * @brief Set TX pin to high or low level
  */
#define DBG_GPIO_RADIO_TX(set_rst)


/* Exported macros -----------------------------------------------------------*/
#ifndef CRITICAL_SECTION_BEGIN
/**
  * @brief macro used to enter the critical section
  */
#define CRITICAL_SECTION_BEGIN( )      UTILS_ENTER_CRITICAL_SECTION( )
#endif /* !CRITICAL_SECTION_BEGIN */
#ifndef CRITICAL_SECTION_END
/**
  * @brief macro used to exit the critical section
  */
#define CRITICAL_SECTION_END( )        UTILS_EXIT_CRITICAL_SECTION( )
#endif /* !CRITICAL_SECTION_END */

/* Function mapping */
/**
  * @brief SUBGHZ interface init to radio Middleware
  */
#define RADIO_INIT                              MX_SUBGHZ_Init

/**
  * @brief Delay interface to radio Middleware
  */
#define RADIO_DELAY_MS                          HAL_Delay

/**
  * @brief Memset utilities interface to radio Middleware
  */
#define RADIO_MEMSET8( dest, value, size )      memset( dest, value, size )

/**
  * @brief Memcpy utilities interface to radio Middleware
  */
#define RADIO_MEMCPY8( dest, src, size )        memcpy( dest, src, size )

#endif /* __RADIO_CONF_H__*/
