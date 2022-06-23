/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lorawan_conf.h
  * @author  MCD Application Team
  * @brief   Header for LoRaWAN middleware instances
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
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __LORAWAN_CONF_H__
#define __LORAWAN_CONF_H__

/*!
 * @brief LoRaWAN version definition
 * @note  possible values: 0x01000300 or 0x01000400
 */
#define LORAMAC_SPECIFICATION_VERSION                   0x01000300
// TODO: What do we need here? 1.0.3 for random nonces?
//  - It seems this value defines the default value for the LoRaWAN
//    version to use, which can be changed using
//    MIB_ABP_LORAWAN_VERSION.  That seems to apply that for OTAA, the
//    version is autodetected based on the joinAccept, but that does not
//    seem to be the case. Also the value of this define also controls
//    a lot of conditional compilation, so it is not *just* the default
//    for MIB_ABP_LORAWAN_VERSION.
//  - This value also controls USE_RANDOM_DEV_NONCE in LoRaMacCrypto.h
//    (random for 1.0.3, sequential for 1.0.4)

/* These regions are enabled, one of them can be selected at runtime. */
#define REGION_AS923
#define REGION_AU915
#define REGION_CN470
#define REGION_CN779
#define REGION_EU433
#define REGION_EU868
#define REGION_KR920
#define REGION_IN865
#define REGION_US915
#define REGION_RU864

/**
  * \brief Limits the number usable channels by default for AU915, CN470 and US915 regions
  * \note the default channel mask with this option activates the first 8 channels. \
  *       this default mask can be modified in the RegionXXXXXInitDefaults function associated with the active region.
  */
#define HYBRID_ENABLED                                  0

/**
  * \brief Define the read access of the keys in memory
  * Enabled to allow reading session keys from the main application.
  */
#define KEY_EXTRACTABLE                                 1

/*!
 * Enables/Disables the context storage management storage.
 * Must be enabled for LoRaWAN 1.0.4 or later.
 * TODO: What do we need? This enables a DeleteAllDynamicKeys() in
 * SecureElementInit() (but only if LORAWAN_KMS), and enables LmHander NVM
 * storage handling.
 */
#define CONTEXT_MANAGEMENT_ENABLED                      1

/* Class B ------------------------------------*/
#define LORAMAC_CLASSB_ENABLED                          0

#if ( LORAMAC_CLASSB_ENABLED == 1 )
    /* CLASS B LSE crystal calibration*/
    /**
      * \brief Temperature coefficient of the clock source
      */
    #define RTC_TEMP_COEFFICIENT                            ( -0.035 )

    /**
      * \brief Temperature coefficient deviation of the clock source
      */
    #define RTC_TEMP_DEV_COEFFICIENT                        ( 0.0035 )

    /**
      * \brief Turnover temperature of the clock source
      */
    #define RTC_TEMP_TURNOVER                               ( 25.0 )

    /**
      * \brief Turnover temperature deviation of the clock source
      */
    #define RTC_TEMP_DEV_TURNOVER                           ( 5.0 )
#endif /* LORAMAC_CLASSB_ENABLED == 1 */

/**
  * \brief Disable the ClassA receive windows after Tx (after the Join Accept if OTAA mode defined)
  * \note  Behavior to reduce power consumption but not compliant with LoRa Alliance recommendations.
  *        All device parameters (Spreading Factor, channels selection, Tx Power, ...) should be fixed
  *        and the adaptive datarate should be disabled.
  * /warning This limitation may have consequences for the proper functioning of the device,
             if the LoRaMac ever generates MAC commands that require a response.
  */
#define DISABLE_LORAWAN_RX_WINDOW                       0

// TODO
/* Exported macro ------------------------------------------------------------*/
#ifndef CRITICAL_SECTION_BEGIN
#define CRITICAL_SECTION_BEGIN( )      UTILS_ENTER_CRITICAL_SECTION( )
#endif /* !CRITICAL_SECTION_BEGIN */
#ifndef CRITICAL_SECTION_END
#define CRITICAL_SECTION_END( )        UTILS_EXIT_CRITICAL_SECTION( )
#endif /* !CRITICAL_SECTION_END */

#endif /* __LORAWAN_CONF_H__ */
