/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements. See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1 (the "License"); you may not use this file
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
 * contact@openairinterface.org
 */

/*! \file telnetsrv_ciUE.c
 * \brief Implementation of telnet CI functions for nrUE
 * \author Guido Casati
 * \date 2024
 * \version 0.1
 * \note This file contains telnet-related functions specific to 5G NR UE (nrUE).
 */

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "openair2/LAYER2/NR_MAC_UE/mac_defs.h"
#include "openair2/LAYER2/NR_MAC_UE/mac_proto.h"
#include "openair2/RRC/NR_UE/rrc_proto.h"

#define TELNETSERVERCODE
#include "telnetsrv.h"

#define ERROR_MSG_RET(mSG, aRGS...) do { prnt(mSG, ##aRGS); return 1; } while (0)

/* UE L2 state string */
const char* NR_UE_L2_STATE_STR[] = {
#define UE_STATE(state) #state,
  NR_UE_L2_STATES
#undef UE_STATE
};

/**
 * Get the synchronization state of a UE.
 *
 * @param buf    User input buffer containing UE ID
 * @param debug  Debug flag (not used)
 * @param prnt   Function to print output
 * @return       0 on success, error code otherwise
 */
int get_sync_state(char *buf, int debug, telnet_printfunc_t prnt)
{
  int ue_id = -1;
  if (!buf) {
    ERROR_MSG_RET("no UE ID provided to telnet command\n");
  } else {
    ue_id = strtol(buf, NULL, 10);
    if (ue_id < 0)
      ERROR_MSG_RET("UE ID needs to be positive\n");
  }
  /* get sync state */
  int sync_state = nr_ue_get_sync_state(ue_id);
  prnt("UE sync state = %s\n", NR_UE_L2_STATE_STR[sync_state]);
  return 0;
}

/**
 * Force RLF on UE
 */
int force_rlf(char *buf, int debug, telnet_printfunc_t prnt)
{
  NR_UE_RRC_INST_t *rrc = get_NR_UE_rrc_inst(0);
  handle_rlf_detection(rrc);
  return 0;
}

/**
 * Send UE to RRC_IDLE
 */
int force_RRC_IDLE(char *buf, int debug, telnet_printfunc_t prnt)
{
  NR_UE_RRC_INST_t *rrc = get_NR_UE_rrc_inst(0);
  nr_rrc_going_to_IDLE(rrc, OTHER, NULL);
  return 0;
}

/** @brief Trigger RA with Msg3 C-RNTI */
int force_crnti_ra(char *buf, int debug, telnet_printfunc_t prnt)
{
  NR_UE_MAC_INST_t *mac = get_mac_inst(0);
  trigger_MAC_UE_RA(mac, NULL);
  return 0;
}

static int force_deregistration(char *buf, int debug, telnet_printfunc_t prnt)
{
  MessageDef *msg = itti_alloc_new_message(TASK_NAS_NRUE, 0, NAS_DEREGISTRATION_REQ);
  NAS_DEREGISTRATION_REQ(msg).cause = AS_DETACH;
  itti_send_msg_to_task(TASK_NAS_NRUE, 0, msg);
  return 0;
}

/* Telnet shell command definitions */
static telnetshell_cmddef_t cicmds[] = {
  {"sync_state", "[UE_ID(int,opt)]", get_sync_state},
  {"force_rlf", "", force_rlf},
  {"force_RRC_IDLE", "", force_RRC_IDLE},
  {"force_crnti_ra", "", force_crnti_ra},
  {"deregistration", "", force_deregistration},
  {"", "", NULL},
};

/* Telnet shell variable definitions (if needed) */
static telnetshell_vardef_t civars[] = {
  {"", 0, 0, NULL}
};

/* Add CI UE commands */
void add_ciUE_cmds(void) {
  add_telnetcmd("ciUE", civars, cicmds);
}

