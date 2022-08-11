/**
  ******************************************************************************
  * @file    radio_board_if.c
  * @author  Matthijs Kooijman
  * @brief   This file provides an interface layer between MW and Radio Board
  ******************************************************************************
  * Copyright (c) 2022 STMicroelectronics.
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
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "radio_board_if.h"
#include "mw_log_conf.h"
#include "Arduino.h"

/* Board configuration --------------------------------------------------------*/

/* This section defines the board configuration values used. This
 * initially just hardcodes the values for the Nucleo WL55JC1 board, but
 * by using defines, these can potentially be overridden by
 * board-specifc defines on the commandline or in the variant files.
 */

// Is a TCXO present on the board?
#if !defined(LORAWAN_BOARD_HAS_TCXO)
  #define LORAWAN_BOARD_HAS_TCXO 1U
#endif

// Is circuitry for DCDC (SMPS) mode present on the board?
#if !defined(LORAWAN_BOARD_HAS_DCDC)
  #define LORAWAN_BOARD_HAS_DCDC 1U
#endif

// Maximum output power supported by output circuitry in LP mode
#if !defined(LORAWAN_RFO_LP_MAX_POWER )
  #define LORAWAN_RFO_LP_MAX_POWER 15 /* dBm */
#endif

// Maximum output power supported by output circuitry in HP mode
#if !defined(LORAWAN_RFO_HP_MAX_POWER )
  #define LORAWAN_RFO_HP_MAX_POWER 22 /* dBm */
#endif

// Supported TX modes (LP/HP or both)
#if !defined(LORAWAN_TX_CONFIG)
  #define LORAWAN_TX_CONFIG RBI_CONF_RFO_LP_HP
#endif

#if !defined(LORAWAN_RFSWITCH_PINS)
  #define LORAWAN_RFSWITCH_PINS PC3,PC4,PC5
  #define LORAWAN_RFSWITCH_PIN_COUNT 3
  #define LORAWAN_RFSWITCH_OFF_VALUES LOW,LOW,LOW
  #define LORAWAN_RFSWITCH_RX_VALUES HIGH,HIGH,LOW
  #define LORAWAN_RFSWITCH_RFO_LP_VALUES HIGH,HIGH,HIGH
  #define LORAWAN_RFSWITCH_RFO_HP_VALUES HIGH,LOW,HIGH
#endif

/* Static variables --------------------------------------------------------*/
static unsigned lorawan_rfswitch_pins[LORAWAN_RFSWITCH_PIN_COUNT] = {LORAWAN_RFSWITCH_PINS};
static unsigned lorawan_rfswitch_values[][LORAWAN_RFSWITCH_PIN_COUNT] = {
  [RBI_SWITCH_OFF] = {LORAWAN_RFSWITCH_OFF_VALUES},
  [RBI_SWITCH_RX] = {LORAWAN_RFSWITCH_RX_VALUES},
  [RBI_SWITCH_RFO_LP] = {LORAWAN_RFSWITCH_RFO_LP_VALUES},
  [RBI_SWITCH_RFO_HP] = {LORAWAN_RFSWITCH_RFO_HP_VALUES},
};

/* Exported functions --------------------------------------------------------*/
int32_t RBI_Init(void)
{
  for (unsigned i = 0; i < LORAWAN_RFSWITCH_PIN_COUNT; ++i) {
    pinMode(lorawan_rfswitch_pins[i], OUTPUT);
    digitalWrite(lorawan_rfswitch_pins[i], lorawan_rfswitch_values[RBI_SWITCH_OFF][i]);
  }

  return 0;
}

int32_t RBI_DeInit(void)
{
  return 0;
}

static const char *ConfigToString(RBI_Switch_TypeDef Config)
{
  switch (Config) {
    case RBI_SWITCH_OFF: return "OFF";
    case RBI_SWITCH_RX: return "RX";
    case RBI_SWITCH_RFO_LP: return "RFO_LP";
    case RBI_SWITCH_RFO_HP: return "RFO_HP";
    default: return "INVALID";
  }
}

int32_t RBI_ConfigRFSwitch(RBI_Switch_TypeDef Config)
{
  MW_LOG(TS_OFF, VLEVEL_M, "Configuring RFSwitch for %s\r\n", ConfigToString(Config));
  for (unsigned i = 0; i < LORAWAN_RFSWITCH_PIN_COUNT; ++i) {
    MW_LOG(TS_OFF, VLEVEL_M, "> Setting pin %u to %u\r\n", lorawan_rfswitch_pins[i], lorawan_rfswitch_values[Config][i]);
    digitalWrite(lorawan_rfswitch_pins[i], lorawan_rfswitch_values[Config][i]);
  }
  return 0;
}

int32_t RBI_GetTxConfig(void)
{
  return LORAWAN_TX_CONFIG;
}

int32_t RBI_IsTCXO(void)
{
  return LORAWAN_BOARD_HAS_TCXO;
}

int32_t RBI_IsDCDC(void)
{
  return LORAWAN_BOARD_HAS_DCDC;
}

int32_t RBI_GetRFOMaxPowerConfig(RBI_RFOMaxPowerConfig_TypeDef Config)
{
  if (Config == RBI_RFO_LP_MAXPOWER) {
    return LORAWAN_RFO_LP_MAX_POWER;
  } else {
    return LORAWAN_RFO_HP_MAX_POWER;
  }
}
