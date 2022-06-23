/**
  ******************************************************************************
  * @file    subghz.c
  * @brief   This file provides code for the configuration
  *          of the SUBGHZ instances.
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
/* Includes ------------------------------------------------------------------*/
#include "subghz.h"

SUBGHZ_HandleTypeDef hsubghz;

void MX_SUBGHZ_Init(void)
{
  hsubghz.Init.BaudratePrescaler = SUBGHZSPI_BAUDRATEPRESCALER_4;
  if (HAL_SUBGHZ_Init(&hsubghz) != HAL_OK) {
    Error_Handler();
  }
}

/* Callback called by the HAL_SUBGHZ layer based on its name */
void HAL_SUBGHZ_MspInit(SUBGHZ_HandleTypeDef *subghzHandle)
{
  (void)subghzHandle; // unused

  __HAL_RCC_SUBGHZSPI_CLK_ENABLE();

  HAL_NVIC_SetPriority(SUBGHZ_Radio_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(SUBGHZ_Radio_IRQn);
}

/* Callback called by the HAL_SUBGHZ layer based on its name */
void HAL_SUBGHZ_MspDeInit(SUBGHZ_HandleTypeDef *subghzHandle)
{
  (void)subghzHandle; // unused

  /* Peripheral clock disable */
  __HAL_RCC_SUBGHZSPI_CLK_DISABLE();

  /* SUBGHZ interrupt Deinit */
  HAL_NVIC_DisableIRQ(SUBGHZ_Radio_IRQn);
}

void SUBGHZ_Radio_IRQHandler(void)
{
  HAL_SUBGHZ_IRQHandler(&hsubghz);
}
