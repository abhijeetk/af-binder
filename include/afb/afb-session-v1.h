/*
 * Copyright (C) 2016, 2017 "IoT.bzh"
 * Author: José Bollo <jose.bollo@iot.bzh>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

/*
 * Enum for Session/Token/Assurance middleware.
 */
enum afb_session_flags_v1
{
       AFB_SESSION_NONE_V1 = 0,   /* nothing required */
       AFB_SESSION_CREATE_V1 = 1, /* Obsolete */
       AFB_SESSION_CLOSE_V1 = 2,  /* After token authentification, closes the session at end */
       AFB_SESSION_RENEW_V1 = 4,  /* After token authentification, refreshes the token at end */
       AFB_SESSION_CHECK_V1 = 8,  /* Requires token authentification */

       AFB_SESSION_LOA_GE_V1 = 16, /* check that the LOA is greater or equal to the given value */
       AFB_SESSION_LOA_LE_V1 = 32, /* check that the LOA is lesser or equal to the given value */
       AFB_SESSION_LOA_EQ_V1 = 48, /* check that the LOA is equal to the given value */

       AFB_SESSION_LOA_SHIFT_V1 = 6, /* shift for LOA */
       AFB_SESSION_LOA_MASK_V1 = 7,  /* mask for LOA */

       AFB_SESSION_LOA_0_V1 = 0,   /* value for LOA of 0 */
       AFB_SESSION_LOA_1_V1 = 64,  /* value for LOA of 1 */
       AFB_SESSION_LOA_2_V1 = 128, /* value for LOA of 2 */
       AFB_SESSION_LOA_3_V1 = 192, /* value for LOA of 3 */
       AFB_SESSION_LOA_4_V1 = 256, /* value for LOA of 4 */

       AFB_SESSION_LOA_LE_0_V1 = AFB_SESSION_LOA_LE_V1 | AFB_SESSION_LOA_0_V1, /* check LOA <= 0 */
       AFB_SESSION_LOA_LE_1_V1 = AFB_SESSION_LOA_LE_V1 | AFB_SESSION_LOA_1_V1, /* check LOA <= 1 */
       AFB_SESSION_LOA_LE_2_V1 = AFB_SESSION_LOA_LE_V1 | AFB_SESSION_LOA_2_V1, /* check LOA <= 2 */
       AFB_SESSION_LOA_LE_3_V1 = AFB_SESSION_LOA_LE_V1 | AFB_SESSION_LOA_3_V1, /* check LOA <= 3 */

       AFB_SESSION_LOA_EQ_0_V1 = AFB_SESSION_LOA_EQ_V1 | AFB_SESSION_LOA_0_V1, /* check LOA == 0 */
       AFB_SESSION_LOA_EQ_1_V1 = AFB_SESSION_LOA_EQ_V1 | AFB_SESSION_LOA_1_V1, /* check LOA == 1 */
       AFB_SESSION_LOA_EQ_2_V1 = AFB_SESSION_LOA_EQ_V1 | AFB_SESSION_LOA_2_V1, /* check LOA == 2 */
       AFB_SESSION_LOA_EQ_3_V1 = AFB_SESSION_LOA_EQ_V1 | AFB_SESSION_LOA_3_V1, /* check LOA == 3 */

       AFB_SESSION_LOA_GE_0_V1 = AFB_SESSION_LOA_GE_V1 | AFB_SESSION_LOA_0_V1, /* check LOA >= 0 */
       AFB_SESSION_LOA_GE_1_V1 = AFB_SESSION_LOA_GE_V1 | AFB_SESSION_LOA_1_V1, /* check LOA >= 1 */
       AFB_SESSION_LOA_GE_2_V1 = AFB_SESSION_LOA_GE_V1 | AFB_SESSION_LOA_2_V1, /* check LOA >= 2 */
       AFB_SESSION_LOA_GE_3_V1 = AFB_SESSION_LOA_GE_V1 | AFB_SESSION_LOA_3_V1  /* check LOA >= 3 */
};

