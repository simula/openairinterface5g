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

/*! \file openair1/PHY/log_tools.h
 * \brief log tools used by data recording application (to be merged to existing OAI log tools)
 * \author Abdo Gaber
 * \date 2024
 * \version 1.0
 * \company Emerson, NI Test and Measurement
 * \email:
 * \note
 * \warning
 */

#ifndef __PHY_LOG_TOOLS_H__
#define __PHY_LOG_TOOLS_H__

#include <stdint.h>
#include "PHY/TOOLS/tools_defs.h"

char* get_time_stamp_usec(char time_stamp_str[]);
int convert_time_stamp_to_int(const char* timestamp);
int split_time_stamp_and_convert_to_int(char time_stamp_str[], int shift, int length);

#endif /*__PHY_LOG_TOOLS_H__ */