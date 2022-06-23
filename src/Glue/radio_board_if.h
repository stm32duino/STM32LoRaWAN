/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    radio_board_if.h
  * @author  MCD Application Team
  * @brief   Header for Radio interface configuration
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef RADIO_BOARD_IF_H
#define RADIO_BOARD_IF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
//#include "platform.h"
//TODO

/* Exported types ------------------------------------------------------------*/

// These were defines in STM32CubeWL for unclear reasons, converted to
// cleaner enum for STM32LoRaWAN Arduino library.
typedef enum {
  RBI_CONF_RFO_LP_HP = 0,
  RBI_CONF_RFO_LP    = 1,
  RBI_CONF_RFO_HP    = 2,
} RBI_TxConfig_TypeDef;

typedef enum
{
  RBI_SWITCH_OFF    = 0,
  RBI_SWITCH_RX     = 1,
  RBI_SWITCH_RFO_LP = 2,
  RBI_SWITCH_RFO_HP = 3,
} RBI_Switch_TypeDef;

typedef enum
{
  RBI_RFO_LP_MAXPOWER = 0,
  RBI_RFO_HP_MAXPOWER = 1,
} RBI_RFOMaxPowerConfig_TypeDef;

/* Exported functions ------------------------------------------------------- */
/**
  * @brief  Init Radio Switch
  * @return BSP status
  */
int32_t RBI_Init(void);

/**
  * @brief  DeInit Radio Switch
  * @return BSP status
  */
int32_t RBI_DeInit(void);

/**
  * @brief  Configure Radio Switch.
  * @param  Config: Specifies the Radio RF switch path to be set.
  *         This parameter can be one of following parameters:
  *           @arg RBI_SWITCH_OFF
  *           @arg RBI_SWITCH_RX
  *           @arg RBI_SWITCH_RFO_LP
  *           @arg RBI_SWITCH_RFO_HP
  * @return BSP status
  */
int32_t RBI_ConfigRFSwitch(RBI_Switch_TypeDef Config);

/**
  * @brief  Return Board Configuration
  * @retval RBI_CONF_RFO_LP_HP
  * @retval RBI_CONF_RFO_LP
  * @retval RBI_CONF_RFO_HP
  */
int32_t RBI_GetTxConfig(void);

/**
  * @brief  Get If TCXO is to be present on board
  * @retval return 1 if present, 0 if not present
  */
int32_t RBI_IsTCXO(void);

/**
  * @brief  Get If DCDC is to be present on board
  * @retval return 1 if present, 0 if not present
  */
int32_t RBI_IsDCDC(void);

/**
  * @brief  Return RF Output Max Power Configuration of matching circuit
  * @retval return Max Power configuration of matching circuit for Low Power or High Power mode in dBm
  */
int32_t RBI_GetRFOMaxPowerConfig(RBI_RFOMaxPowerConfig_TypeDef Config);


#ifdef __cplusplus
}
#endif

#endif /* RADIO_BOARD_IF_H */
