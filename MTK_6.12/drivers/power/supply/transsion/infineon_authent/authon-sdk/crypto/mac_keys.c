/***************************************************************************************
 * Copyright 2021 Infineon Technologies AG ( www.infineon.com ).                       *
 * All rights reserved.                                                                *
 *                                                                                     *
 * Licensed  Material-Property of Infineon Technologies AG.                            *
 * This software is made available solely pursuant to the terms of Infineon            *
 * Technologies AG agreement which governs its use. This code and the information      *
 * contained in it are proprietary and confidential to Infineon Technologies AG.       *
 * No person is allowed to copy, reprint, reproduce or publish any part of this code,  *
 * nor disclose its contents to others, nor make any use of it, nor allow or assist    *
 * others to make any use of it - unless by prior Written express authorization of     *
 * Infineon Technologies AG and then only to the extent authorized.                    *
 *                                                                                     *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,            *
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,           *
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE DISCLAIMED.  IN NO       *
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,     *
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,                 *
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;         *
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY             *
 * WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR            *
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF              *
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                                          *
 *                                                                                     *
 ***************************************************************************************/

/**
 * @file   mac_keys.c
 * @date   January, 2021
 * @brief  Implementation of MAC keys
 */

#include "helper/interface_helper.h"
#include "authon_config.h"
#include "crypto/crypto_lib.h"

/** !!!!!!!!!! W A R N I N G !!!!!!!!!!
  *
  * Password should be protected in host system. password should be stored in
  * host secure Memory.
  *
  */
// Host authentication host key

#if (ENGINEERING_SAMPLE_MAC_KEY==1)
const dwordvec_t HA_MAC_key = {0x2505B39B, 0x4BF8FA1B, 0xECD0, 0x0, 0x0, 0x0, 0x0};
#endif

#if (PERSONALIZED_MAC_KEY==1)
const dwordvec_t HA_MAC_key = {0x00000000, 0x00000000,0x00000000, 0x0, 0x0, 0x0, 0x0};
#endif

#if (ENGINEERING_SAMPLE_PASSWORD==1)
const uint32_t AUTHKILL_PASSWORD = 0xE1B57CFD;
const mac_t  RESET_PASSWORD = {0xFAB84BD4, 0xE347FDDD, 0xC706};

#endif

#if (PRODUCTIVE_PASSWORD==1)
const uint32_t AUTHKILL_PASSWORD = 0x00000000;
const mac_t  RESET_PASSWORD = {0x00000000, 0x00000000, 0x0000};
#endif

/**
* @brief Returns the host MAC key
* @param ha_mac_key host authentication mac key
*/
void Get_HA_MAC_key(dwordvec_t ha_mac_key)
{
	memcpy(ha_mac_key, HA_MAC_key, sizeof(HA_MAC_key));
}

/**
* @brief Returns the authentication kill password
* @param password password used for killing authentication
*/
void Get_AUTHKILL_PASSWORD(uint32_t *password)
{
	memcpy(password, &AUTHKILL_PASSWORD, sizeof(AUTHKILL_PASSWORD));
}

/**
* @brief Returns the reset password
* @param reset_password password used for reset
*/
void Get_Reset_Password(uint32_t *reset_password)
{
	memcpy(reset_password, RESET_PASSWORD, sizeof(RESET_PASSWORD));
}
