/**
  ******************************************************************************
  * @file    sys_app.h
  * @author  Matthijs Kooijman
  * @brief   Provide debug output macro for timer_if
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
#ifndef __MW_SYS_APP_H__
#define __MW_SYS_APP_H__

#include "mw_log_conf.h"

// In the original STM32CubeWL code this is the function behind
// MW_LOG, but it is called directly as well from timer_if.c, so
// just redirect it for that particular usecase.
#define UTIL_ADV_TRACE_COND_FSend(level, reg, ts, ...) MW_LOG(ts, level, __VA_ARGS__)

#endif // __MW_SYS_APP_H__
