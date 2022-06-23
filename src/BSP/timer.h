/**
  ******************************************************************************
  * @file    timer.h
  * @author  MCD Application Team
  * @brief   Wrapper to timer server
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __TIMER_H__
#define __TIMER_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "../STM32CubeWL/Utilities/timer/stm32_timer.h"

/**
  * @brief Max timer mask
  */
#define TIMERTIME_T_MAX ( ( uint32_t )~0 )

/* Exported macro ------------------------------------------------------------*/

/**
  * @brief Timer value on 32 bits
  */
#define TimerTime_t UTIL_TIMER_Time_t

/**
  * @brief Timer object description
  */
#define TimerEvent_t UTIL_TIMER_Object_t

/**
  * @brief Create the timer object
  */
#define TimerInit(HANDLE, CB) do {\
                                   UTIL_TIMER_Create( HANDLE, TIMERTIME_T_MAX, UTIL_TIMER_ONESHOT, CB, NULL);\
                                 } while(0)

/**
  * @brief update the period and start the timer
  */
#define TimerSetValue(HANDLE, TIMEOUT) do{ \
                                           UTIL_TIMER_SetPeriod(HANDLE, TIMEOUT);\
                                         } while(0)

/**
  * @brief Start and adds the timer object to the list of timer events
  */
#define TimerStart(HANDLE)   do {\
                                  UTIL_TIMER_Start(HANDLE);\
                                } while(0)

/**
  * @brief Stop and removes the timer object from the list of timer events
  */
#define TimerStop(HANDLE)   do {\
                                 UTIL_TIMER_Stop(HANDLE);\
                               } while(0)

/**
  * @brief return the current time
  */
#define TimerGetCurrentTime  UTIL_TIMER_GetCurrentTime

/**
  * @brief return the elapsed time
  */
#define TimerGetElapsedTime UTIL_TIMER_GetElapsedTime

#ifdef __cplusplus
}
#endif

#endif /* __TIMER_H__*/
