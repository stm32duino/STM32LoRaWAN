/**
  ******************************************************************************
  * @file    subghz.h
  * @brief   This file contains all the function prototypes for
  *          the subghz.c file
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

#ifndef __SUBGHZ_H__
#define __SUBGHZ_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stm32_def.h>

extern SUBGHZ_HandleTypeDef hsubghz;

void MX_SUBGHZ_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __SUBGHZ_H__ */

