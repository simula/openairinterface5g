/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file lttng-log.h
* \brief LTTng Log interface
* \author Anurag Asokan
* \date 2024
* \version 0.5
* @ingroup util

*/

#ifndef __LTTNG_LOG_H__
#define __LTTNG_LOG_H__
#if ENABLE_LTTNG
#include "lttng-tp.h"

#define LOG_FC(component, func, line, log)                        \
  do {                                                            \
    tracepoint(OAI, gNB, component, -1, -1, -1, func, line, log); \
  } while (0)
#else
#define LOG_FC(component, func, line, log)
#endif

#endif /** __LTTNG_LOG_H__ */
