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
enum afb_session_flags
{
       AFB_SESSION_NONE = 0,   /* nothing required */
#if defined(AFB_BINDING_PRAGMA_DECLARE_V1) && defined(AFB_BINDING_PRAGMA_KEEP_OBSOLETE_V1)
       AFB_SESSION_CREATE = 1, /* Obsolete */
#endif
       AFB_SESSION_CLOSE = 2,  /* After token authentification, closes the session at end */
       AFB_SESSION_RENEW = 4,  /* After token authentification, refreshes the token at end */
       AFB_SESSION_CHECK = 8,  /* Requires token authentification */

       AFB_SESSION_LOA_GE = 16, /* check that the LOA is greater or equal to the given value */
#if defined(AFB_BINDING_PRAGMA_DECLARE_V1) || defined(AFB_BINDING_PRAGMA_KEEP_OBSOLETE_V2)
       AFB_SESSION_LOA_LE = 32, /* check that the LOA is lesser or equal to the given value */
       AFB_SESSION_LOA_EQ = 48, /* check that the LOA is equal to the given value */
#endif

       AFB_SESSION_LOA_SHIFT = 6, /* shift for LOA */
       AFB_SESSION_LOA_MASK = 7,  /* mask for LOA */

       AFB_SESSION_LOA_0 = 0,   /* value for LOA of 0 */
       AFB_SESSION_LOA_1 = 64,  /* value for LOA of 1 */
       AFB_SESSION_LOA_2 = 128, /* value for LOA of 2 */
       AFB_SESSION_LOA_3 = 192, /* value for LOA of 3 */
       AFB_SESSION_LOA_4 = 256, /* value for LOA of 4 */

#if defined(AFB_BINDING_PRAGMA_DECLARE_V1) || defined(AFB_BINDING_PRAGMA_KEEP_OBSOLETE_V2)
       AFB_SESSION_LOA_LE_0 = AFB_SESSION_LOA_LE | AFB_SESSION_LOA_0, /* check LOA <= 0 */
       AFB_SESSION_LOA_LE_1 = AFB_SESSION_LOA_LE | AFB_SESSION_LOA_1, /* check LOA <= 1 */
       AFB_SESSION_LOA_LE_2 = AFB_SESSION_LOA_LE | AFB_SESSION_LOA_2, /* check LOA <= 2 */
       AFB_SESSION_LOA_LE_3 = AFB_SESSION_LOA_LE | AFB_SESSION_LOA_3, /* check LOA <= 3 */

       AFB_SESSION_LOA_EQ_0 = AFB_SESSION_LOA_EQ | AFB_SESSION_LOA_0, /* check LOA == 0 */
       AFB_SESSION_LOA_EQ_1 = AFB_SESSION_LOA_EQ | AFB_SESSION_LOA_1, /* check LOA == 1 */
       AFB_SESSION_LOA_EQ_2 = AFB_SESSION_LOA_EQ | AFB_SESSION_LOA_2, /* check LOA == 2 */
       AFB_SESSION_LOA_EQ_3 = AFB_SESSION_LOA_EQ | AFB_SESSION_LOA_3, /* check LOA == 3 */
#endif

       AFB_SESSION_LOA_GE_0 = AFB_SESSION_LOA_GE | AFB_SESSION_LOA_0, /* check LOA >= 0 */
       AFB_SESSION_LOA_GE_1 = AFB_SESSION_LOA_GE | AFB_SESSION_LOA_1, /* check LOA >= 1 */
       AFB_SESSION_LOA_GE_2 = AFB_SESSION_LOA_GE | AFB_SESSION_LOA_2, /* check LOA >= 2 */
       AFB_SESSION_LOA_GE_3 = AFB_SESSION_LOA_GE | AFB_SESSION_LOA_3  /* check LOA >= 3 */
};

