/**
  ******************************************************************************
  * @file    mw_log_conf.h
  * @author  MCD Application Team
  * @brief   Configure (enable/disable) traces for CM0
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * The Clear BSD License
  *
  * Redistribution and use in source and binary forms, with or without
  * modification, are permitted (subject to the limitations in the disclaimer
  * below) provided that the following conditions are met:
  *     * Redistributions of source code must retain the above copyright
  *       notice, this list of conditions and the following disclaimer.
  *     * Redistributions in binary form must reproduce the above copyright
  *       notice, this list of conditions and the following disclaimer in the
  *       documentation and/or other materials provided with the distribution.
  *     * Neither the name of the Semtech corporation nor the
  *       names of its contributors may be used to endorse or promote products
  *       derived from this software without specific prior written permission.
  *
  * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
  * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
  * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
  * NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
  * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SEMTECH CORPORATION BE
  * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  * POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

#ifndef __MW_LOG_CONF_H__
#define __MW_LOG_CONF_H__

#ifdef __cplusplus
extern "C" {
#endif

    #define MW_LOG_ENABLED

    // These enums were defines in utilities_conf.h in STM32CubeWL
    //
    // They are defined here for compatibility with existing code, but
    // are ignored when passed to MW_LOG, which just does a dumb forward
    // of the message.
    typedef enum {
        VLEVEL_ALWAYS = 0, /*!< used as message params, if this level is given
                              trace will be printed even when UTIL_ADV_TRACE_SetVerboseLevel(OFF) */
        VLEVEL_L = 1,      /*!< just essential traces */
        VLEVEL_M = 2,      /*!< functional traces */
        VLEVEL_H = 3,      /*!< all traces */
    } MwLogLevel_t;

    typedef enum {
        TS_OFF = 0,        /*!< Log without TimeStamp */
        TS_ON = 1,         /*!< Log with TimeStamp */
    } MwLogTimestamp_t;

    void MW_LOG(MwLogTimestamp_t ts, MwLogLevel_t level, const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /*__MW_LOG_CONF_H__ */
