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

#ifndef __POSITION_INTERFACE__H__
#define __POSITION_INTERFACE__H__

#include <executables/nr-softmodem-common.h>
#include <executables/softmodem-common.h>
#include "PHY/defs_nr_UE.h"
#include "executables/nr-uesoftmodem.h"

/* position configuration parameters name */
#define CONFIG_STRING_POSITION_X "x"
#define CONFIG_STRING_POSITION_Y "y"
#define CONFIG_STRING_POSITION_Z "z"

#define HELP_STRING_POSITION "Postion coordinates (x, y, z) config params"

// Position parameters config
/*----------------------------------------------------------------------------------------------------------------------------------------------------*/
/*                                            position configuration          parameters */
/*   optname                                         helpstr            paramflags    XXXptr              defXXXval type numelt */
/*----------------------------------------------------------------------------------------------------------------------------------------------------*/
// clang-format off
#define POSITION_CONFIG_PARAMS_DEF { \
  {CONFIG_STRING_POSITION_X,  HELP_STRING_POSITION, 0,        .dblptr=&(position->X),                 .defuintval=0,           TYPE_DOUBLE,     0},    \
  {CONFIG_STRING_POSITION_Y,  HELP_STRING_POSITION, 0,        .dblptr=&(position->Y),                 .defuintval=0,           TYPE_DOUBLE,     0},    \
  {CONFIG_STRING_POSITION_Z,  HELP_STRING_POSITION, 0,        .dblptr=&(position->Z),                 .defuintval=0,           TYPE_DOUBLE,     0}     \
}
// clang-format on

typedef struct position {
  double X;
  double Y;
  double Z;
} position_t;

void get_position_coordinates(int Mod_id, position_t *position);

#endif
