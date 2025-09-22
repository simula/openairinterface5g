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

/*! \file telnetsrv_ci.c
 * \brief Implementation of telnet CI functions for gNB
 * \note  This file contains telnet-related functions specific to 5G gNB.
 */

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "openair2/RRC/NR/rrc_gNB_UE_context.h"
#include "openair2/LAYER2/NR_MAC_gNB/nr_mac_gNB.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_oai_api.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_ue_manager.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_entity_am.h"
#include "openair2/LAYER2/NR_MAC_gNB/mac_proto.h"
#include "openair2/RRC/NR/rrc_gNB_mobility.h"

#define TELNETSERVERCODE
#include "telnetsrv.h"

#define ERROR_MSG_RET(mSG, aRGS...) do { prnt(mSG, ##aRGS); return -1; } while (0)

static int get_single_ue_rnti_mac(void)
{
  NR_UE_info_t *ue = NULL;
  UE_iterator(RC.nrmac[0]->UE_info.connected_ue_list, it) {
    if (it && ue)
      return -1;
    if (it)
      ue = it;
  }
  if (!ue)
    return -1;

  return ue->rnti;
}

int get_single_rnti(char *buf, int debug, telnet_printfunc_t prnt)
{
  if (buf)
    ERROR_MSG_RET("no parameter allowed\n");

  int rnti = get_single_ue_rnti_mac();
  if (rnti < 1)
    ERROR_MSG_RET("different number of UEs\n");

  prnt("single UE RNTI %04x\n", rnti);
  return 0;
}

rrc_gNB_ue_context_t *get_single_rrc_ue(void)
{
  rrc_gNB_ue_context_t *ue = NULL;
  rrc_gNB_ue_context_t *l = NULL;
  int n = 0;
  RB_FOREACH (l, rrc_nr_ue_tree_s, &RC.nrrrc[0]->rrc_ue_head) {
    if (ue == NULL)
      ue = l;
    n++;
  }
  if (!ue) {
    printf("could not find any UE in RRC\n");
  }
  if (n > 1) {
    printf("more than one UE in RRC present\n");
    ue = NULL;
  }

  return ue;
}

int get_reestab_count(char *buf, int debug, telnet_printfunc_t prnt)
{
  if (!RC.nrrrc)
    ERROR_MSG_RET("no RRC present, cannot list counts\n");
  rrc_gNB_ue_context_t *ue = NULL;
  if (!buf) {
    ue = get_single_rrc_ue();
    if (!ue)
      ERROR_MSG_RET("no single UE in RRC present\n");
  } else {
    ue_id_t ue_id = strtol(buf, NULL, 10);
    ue = rrc_gNB_get_ue_context(RC.nrrrc[0], ue_id);
    if (!ue)
      ERROR_MSG_RET("could not find UE with ue_id %d in RRC\n");
  }

  prnt("UE RNTI %04x reestab %d reconfig %d\n",
       ue->ue_context.rnti,
       ue->ue_context.ue_reestablishment_counter,
       ue->ue_context.ue_reconfiguration_counter);
  return 0;
}

int fetch_rnti(char *buf, telnet_printfunc_t prnt)
{
  int rnti = -1;
  if (!buf) {
    rnti = get_single_ue_rnti_mac();
    if (rnti < 1)
      ERROR_MSG_RET("no UE found\n");
  } else {
    rnti = strtol(buf, NULL, 16);
    if (rnti < 1 || rnti >= 0xfffe)
      ERROR_MSG_RET("RNTI needs to be [1,0xfffe]\n");
  }
  return rnti;
}

int trigger_reestab(char *buf, int debug, telnet_printfunc_t prnt)
{
  if (!RC.nrmac)
    ERROR_MSG_RET("no MAC/RLC present, cannot trigger reestablishment\n");
  int rnti = fetch_rnti(buf, prnt);
  if (rnti < 0)
    ERROR_MSG_RET("could not identify UE (no UE, no such RNTI, or multiple UEs)\n");
  nr_rlc_test_trigger_reestablishment(rnti);
  prnt("Reset RLC counters of UE RNTI %04x to trigger reestablishment\n", rnti);
  return 0;
}

extern nr_rrc_du_container_t *get_du_for_ue(gNB_RRC_INST *rrc, uint32_t ue_id);

/** @brief Get connected DU by the UE ID */
int fetch_du_by_ue_id(char *buf, int debug, telnet_printfunc_t prnt)
{
  if (!RC.nrrrc)
    ERROR_MSG_RET("no RRC present, cannot list counts\n");

  ue_id_t ue_id = 1;
  if (buf) {
    ue_id = strtol(buf, NULL, 10);
  }
  nr_rrc_du_container_t *du = get_du_for_ue(RC.nrrrc[0], ue_id);

  if (du) {
    prnt("gNB_DU_id %d is connected to ue_id %ld\n", du->setup_req->gNB_DU_id, ue_id);
    return 0;
  } else {
    ERROR_MSG_RET("No DU connected\n");
    return -1;
  }
}

extern void nr_HO_F1_trigger_telnet(gNB_RRC_INST *rrc, uint32_t rrc_ue_id);
/**
 * @brief Trigger F1 handover for UE
 * @param buf: RRC UE ID or NULL for the first UE in list
 * @param debug: Debug flag
 * @param prnt: Print function
 * @return 0 on success, -1 on failure
 */
int rrc_gNB_trigger_f1_ho(char *buf, int debug, telnet_printfunc_t prnt)
{
  if (!RC.nrrrc)
    ERROR_MSG_RET("no RRC present, cannot list counts\n");
  rrc_gNB_ue_context_t *ue = NULL;
  if (!buf) {
    ue = get_single_rrc_ue();
    if (!ue)
      ERROR_MSG_RET("no single UE in RRC present\n");
  } else {
    ue_id_t ue_id = strtol(buf, NULL, 10);
    ue = rrc_gNB_get_ue_context(RC.nrrrc[0], ue_id);
    if (!ue)
      ERROR_MSG_RET("could not find UE with ue_id %d in RRC\n", ue_id);
  }

  gNB_RRC_UE_t *UE = &ue->ue_context;
  nr_HO_F1_trigger_telnet(RC.nrrrc[0], UE->rrc_ue_id);
  prnt("RRC F1 handover triggered for UE %u\n", UE->rrc_ue_id);
  return 0;
}

extern void nr_HO_N2_trigger_telnet(gNB_RRC_INST *rrc, uint32_t neighbour_pci, uint32_t rrc_ue_id);

/** @brief Trigger N2 handover for UE
 *  @param buf: Neighbour PCI, SCell PCI, RRC UE ID
 *  @param debug: Debug flag
 *  @param prnt: Print function
 *  @return 0 on success, -1 on failure */
int rrc_gNB_trigger_n2_ho(char *buf, int debug, telnet_printfunc_t prnt)
{
  if (!RC.nrrrc)
    ERROR_MSG_RET("no RRC present, cannot list counts\n");

  if (!buf) {
    ERROR_MSG_RET("Please provide neighbour cell id and ue id\n");
  } else {
    // Parse neighbour cell PCI
    char *token = strtok(buf, ",");
    if (!token) {
      ERROR_MSG_RET("Invalid input. Expected format: Neighbour PCI, ueId\n");
    }
    uint32_t neighbour_pci = strtol(token, NULL, 10);

    // Parse ueId
    token = strtok(NULL, ",");
    if (!token) {
      ERROR_MSG_RET("Missing UE ID\n");
    }
    uint32_t ueId = strtol(token, NULL, 10);

    // Retrieve UE context
    rrc_gNB_ue_context_t *ue_p = rrc_gNB_get_ue_context(RC.nrrrc[0], ueId);
    if (!ue_p) {
      ERROR_MSG_RET("UE with id %u not found\n", ueId);
    }
    gNB_RRC_UE_t *UE = &ue_p->ue_context;

    // Trigger N2 handover
    nr_HO_N2_trigger_telnet(RC.nrrrc[0], neighbour_pci, UE->rrc_ue_id);

    // Print success message
    prnt("RRC N2 handover triggered for UE %u with neighbour cell id %u\n",
         ueId,
         neighbour_pci);
  }
  return 0;
}

int force_ul_failure(char *buf, int debug, telnet_printfunc_t prnt)
{
  if (!RC.nrmac)
    ERROR_MSG_RET("no MAC/RLC present, force_ul_failure failed\n");
  int rnti = fetch_rnti(buf, prnt);
  if (rnti < 0)
    ERROR_MSG_RET("could not identify UE (no UE, no such RNTI, or multiple UEs)\n");
  NR_UE_info_t *UE = find_nr_UE(&RC.nrmac[0]->UE_info, rnti);
  nr_mac_trigger_ul_failure(&UE->UE_sched_ctrl, UE->current_UL_BWP.scs);
  return 0;
}

int force_ue_release(char *buf, int debug, telnet_printfunc_t prnt)
{
  force_ul_failure(buf, debug, prnt);
  int rnti = fetch_rnti(buf, prnt);
  if (rnti < 0)
    ERROR_MSG_RET("could not identify UE (no UE, no such RNTI, or multiple UEs)\n");
  NR_UE_info_t *UE = find_nr_UE(&RC.nrmac[0]->UE_info, rnti);
  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  sched_ctrl->ul_failure_timer = 2;
  nr_mac_check_ul_failure(RC.nrmac[0], UE->rnti, sched_ctrl);
  return 0;
}

static int get_current_bwp(char *buf, int debug, telnet_printfunc_t prnt)
{
  int rnti = fetch_rnti(buf, prnt);
  if (rnti < 0)
    ERROR_MSG_RET("could not identify UE (no UE, no such RNTI, or multiple UEs)\n");
  NR_UE_info_t *UE = find_nr_UE(&RC.nrmac[0]->UE_info, rnti);
  int dl_bwp = UE->current_DL_BWP.bwp_id;
  const char *dl_bwp_text = dl_bwp > 0 ? "dedicated" : "initial";
  int ul_bwp = UE->current_UL_BWP.bwp_id;
  const char *ul_bwp_text = ul_bwp > 0 ? "dedicated" : "initial";

  prnt("UE %04x DL BWP ID %d (%s) UL BWP ID %d (%s)\n", UE->rnti, dl_bwp, dl_bwp_text, ul_bwp, ul_bwp_text);
  return 0;
}

static telnetshell_cmddef_t cicmds[] = {
    {"get_single_rnti", "", get_single_rnti},
    {"force_reestab", "[rnti(hex,opt)]", trigger_reestab},
    {"get_reestab_count", "[rnti(hex,opt)]", get_reestab_count},
    {"force_ue_release", "[rnti(hex,opt)]", force_ue_release},
    {"force_ul_failure", "[rnti(hex,opt)]", force_ul_failure},
    {"trigger_f1_ho", "[rrc_ue_id(int,opt)]", rrc_gNB_trigger_f1_ho},
    {"fetch_du_by_ue_id", "[rrc_ue_id(int,opt)]", fetch_du_by_ue_id},
    {"get_current_bwp", "[rnti(hex,opt)]", get_current_bwp},
    {"trigger_n2_ho", "[neighbour_pci(uint32_t),ueId(uint32_t)]", rrc_gNB_trigger_n2_ho},
    {"", "", NULL},
};

static telnetshell_vardef_t civars[] = {

  {"", 0, 0, NULL}
};

void add_ci_cmds(void) {
  add_telnetcmd("ci", civars, cicmds);
}
