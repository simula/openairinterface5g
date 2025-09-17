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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>
#include <linux/sched.h>
#include <sys/sysinfo.h>
#include <math.h>

#include "common/utils/nr/nr_common.h"
#include "common/utils/assertions.h"
#include "common/utils/system.h"
#include "common/ran_context.h"

#include "radio/COMMON/common_lib.h"
#include "radio/ETHERNET/ethernet_lib.h"

#include "PHY/LTE_TRANSPORT/if4_tools.h"

#include "PHY/types.h"
#include "PHY/defs_nr_common.h"
#include "PHY/phy_extern.h"
#include "PHY/NR_TRANSPORT/nr_transport_proto.h"
#include "PHY/INIT/nr_phy_init.h"
#include "SCHED_NR/sched_nr.h"

#include "common/utils/LOG/log.h"
#include "common/utils/time_manager/time_manager.h"
#include "common/utils/LOG/vcd_signal_dumper.h"

#include <executables/softmodem-common.h>
/* these variables have to be defined before including ENB_APP/enb_paramdef.h and GNB_APP/gnb_paramdef.h */
static int DEFBANDS[] = {7};
static int DEFENBS[] = {0};
static int DEFBFW[] = {0x00007fff};
static int DEFRUTPCORES[] = {-1,-1,-1,-1};

#include "ENB_APP/enb_paramdef.h"
#include "GNB_APP/gnb_paramdef.h"
#include "common/config/config_userapi.h"

#include <openair1/PHY/TOOLS/phy_scope_interface.h>

#include "T.h"
#include "nfapi_interface.h"
#include <nfapi/oai_integration/vendor_ext.h>
#include "executables/nr-softmodem-common.h"

static void NRRCconfig_RU(configmodule_interface_t *cfg);

/*************************************************************/
/* Functions to attach and configure RRU                     */

int attach_rru(RU_t *ru)
{
  RRU_CONFIG_msg_t rru_config_msg;
  int received_capabilities=0;
  wait_gNBs();

  // Wait for capabilities
  while (received_capabilities==0) {
    rru_config_msg = (RRU_CONFIG_msg_t){.type = RAU_tick, .len = sizeof(rru_config_msg.msg)};
    LOG_D(PHY, "Sending RAU tick to RRU %d\n", ru->idx);
    AssertFatal((ru->ifdevice.trx_ctlsend_func(&ru->ifdevice,&rru_config_msg,rru_config_msg.len)!=-1),
                "RU %d cannot access remote radio\n",ru->idx);
    ssize_t msg_len = rru_config_msg.len + sizeof(RRU_capabilities_t);
    // wait for answer with timeout
    ssize_t len = ru->ifdevice.trx_ctlrecv_func(&ru->ifdevice, &rru_config_msg, msg_len);
    if (len < 0) {
      LOG_D(PHY, "Waiting for RRU %d\n", ru->idx);
    } else if (rru_config_msg.type == RRU_capabilities) {
      AssertFatal(rru_config_msg.len == msg_len,
                  "Received capabilities with incorrect length (%ld!=%ld)\n",
                  rru_config_msg.len,
                  msg_len);
      RRU_capabilities_t *cap = (RRU_capabilities_t *)rru_config_msg.msg;
      LOG_I(PHY,
            "Received capabilities from RRU %d (len %ld/%ld, num_bands %d,max_pdschReferenceSignalPower %d, max_rxgain %d, nb_tx "
            "%d, nb_rx %d)\n",
            ru->idx,
            rru_config_msg.len,
            msg_len,
            cap->num_bands,
            cap->max_pdschReferenceSignalPower[0],
            cap->max_rxgain[0],
            cap->nb_tx[0],
            cap->nb_rx[0]);
      received_capabilities=1;
    } else {
      LOG_E(PHY,"Received incorrect message %d from RRU %d\n",rru_config_msg.type,ru->idx);
    }
  }

  configure_ru(ru, (RRU_capabilities_t *)rru_config_msg.msg);
  rru_config_msg.type = RRU_config;
  rru_config_msg.len  = sizeof(RRU_CONFIG_msg_t)-MAX_RRU_CONFIG_SIZE+sizeof(RRU_config_t);
  RRU_config_t *conf = (RRU_config_t *)rru_config_msg.msg;
  LOG_I(PHY,
        "Sending Configuration to RRU %d (num_bands %d,band0 %d,txfreq %u,rxfreq %u,att_tx %d,att_rx %d,N_RB_DL %d,N_RB_UL "
        "%d,3/4FS %d, prach_FO %d, prach_CI %d)\n",
        ru->idx,
        conf->num_bands,
        conf->band_list[0],
        conf->tx_freq[0],
        conf->rx_freq[0],
        conf->att_tx[0],
        conf->att_rx[0],
        conf->N_RB_DL[0],
        conf->N_RB_UL[0],
        conf->threequarter_fs[0],
        conf->prach_FreqOffset[0],
        conf->prach_ConfigIndex[0]);
  AssertFatal((ru->ifdevice.trx_ctlsend_func(&ru->ifdevice,&rru_config_msg,rru_config_msg.len)!=-1),
              "RU %d failed send configuration to remote radio\n",ru->idx);
  int len = ru->ifdevice.trx_ctlrecv_func(&ru->ifdevice, &rru_config_msg, sizeof(rru_config_msg.msg));
  if (len < 0) {
    LOG_I(PHY,"Waiting for RRU %d\n",ru->idx);
  } else if (rru_config_msg.type == RRU_config_ok) {
    LOG_I(PHY, "RRU_config_ok received\n");
  } else {
    LOG_E(PHY,"Received incorrect message %d from RRU %d\n",rru_config_msg.type,ru->idx);
  }

  return 0;
}

int connect_rau(RU_t *ru) {
  RRU_CONFIG_msg_t rru_config_msg;

  // wait for RAU_tick
  int tick_received = 0;
  while (tick_received == 0) {
    ssize_t msg_len = sizeof(rru_config_msg.msg);
    int len = ru->ifdevice.trx_ctlrecv_func(&ru->ifdevice, &rru_config_msg, msg_len);
    if (len < 0) {
      LOG_I(PHY,"Waiting for RAU\n");
    } else {
      if (rru_config_msg.type == RAU_tick) {
        LOG_I(PHY,"Tick received from RAU\n");
        tick_received = 1;
      } else
        LOG_E(PHY, "Received erroneous message (%d)from RAU, expected RAU_tick\n", rru_config_msg.type);
    }
  }

  // send capabilities
  rru_config_msg.type = RRU_capabilities;
  rru_config_msg.len  = sizeof(RRU_CONFIG_msg_t)-MAX_RRU_CONFIG_SIZE+sizeof(RRU_capabilities_t);
  RRU_capabilities_t *cap = (RRU_capabilities_t *)rru_config_msg.msg;
  LOG_I(PHY,
        "Sending Capabilities (len %ld, num_bands %d,max_pdschReferenceSignalPower %d, max_rxgain %d, nb_tx %d, nb_rx %d)\n",
        rru_config_msg.len,
        ru->num_bands,
        ru->max_pdschReferenceSignalPower,
        ru->max_rxgain,
        ru->nb_tx,
        ru->nb_rx);

  switch (ru->function) {
    case NGFI_RRU_IF4p5:
      cap->FH_fmt = OAI_IF4p5_only;
      break;

    case NGFI_RRU_IF5:
      cap->FH_fmt = OAI_IF5_only;
      break;

    case MBP_RRU_IF5:
      cap->FH_fmt = MBP_IF5;
      break;

    default:
      AssertFatal(false, "RU_function is unknown %d\n", RC.ru[0]->function);
      break;
  }

  cap->num_bands = ru->num_bands;
  for (int i = 0; i < ru->num_bands; i++) {
    LOG_I(PHY,"Band %d: nb_rx %d nb_tx %d pdschReferenceSignalPower %d rxgain %d\n",
          ru->band[i],ru->nb_rx,ru->nb_tx,ru->max_pdschReferenceSignalPower,ru->max_rxgain);
    cap->band_list[i]                             = ru->band[i];
    cap->nb_rx[i]                                 = ru->nb_rx;
    cap->nb_tx[i]                                 = ru->nb_tx;
    cap->max_pdschReferenceSignalPower[i]         = ru->max_pdschReferenceSignalPower;
    cap->max_rxgain[i]                            = ru->max_rxgain;
  }

  AssertFatal((ru->ifdevice.trx_ctlsend_func(&ru->ifdevice,&rru_config_msg,rru_config_msg.len)!=-1),
              "RU %d failed send capabilities to RAU\n",ru->idx);
  // wait for configuration
  rru_config_msg.len  = sizeof(RRU_CONFIG_msg_t)-MAX_RRU_CONFIG_SIZE+sizeof(RRU_config_t);

  int configuration_received = 0;
  while (configuration_received == 0) {
    int len = ru->ifdevice.trx_ctlrecv_func(&ru->ifdevice, &rru_config_msg, rru_config_msg.len);
    if (len < 0) {
      LOG_I(PHY,"Waiting for configuration from RAU\n");
    } else {
      RRU_config_t *conf = (RRU_config_t *)rru_config_msg.msg;
      LOG_I(PHY,
            "Configuration received from RAU  (num_bands %d,band0 %d,txfreq %u,rxfreq %u,att_tx %d,att_rx %d,N_RB_DL %d,N_RB_UL "
            "%d,3/4FS %d, prach_FO %d, prach_CI %d)\n",
            conf->num_bands,
            conf->band_list[0],
            conf->tx_freq[0],
            conf->rx_freq[0],
            conf->att_tx[0],
            conf->att_rx[0],
            conf->N_RB_DL[0],
            conf->N_RB_UL[0],
            conf->threequarter_fs[0],
            conf->prach_FreqOffset[0],
            conf->prach_ConfigIndex[0]);
      configure_rru(ru, (void *)rru_config_msg.msg);
      configuration_received = 1;
    }
  }

  return 0;
}
/*************************************************************/
/* Southbound Fronthaul functions, RCC/RAU                   */

// southbound IF5 fronthaul for 16-bit OAI format
void fh_if5_south_out(RU_t *ru, int frame, int slot, uint64_t timestamp) {
  if (ru == RC.ru[0])
    VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME(VCD_SIGNAL_DUMPER_VARIABLES_TRX_TST, ru->proc.timestamp_tx & 0xffffffff);
  int offset = ru->nr_frame_parms->get_samples_slot_timestamp(slot,ru->nr_frame_parms,0);
  void *buffs[ru->nb_tx];
  for (int aid = 0; aid < ru->nb_tx; aid++)
    buffs[aid] = (void*)&ru->common.txdata[aid][offset];
  struct timespec txmeas;
  clock_gettime(CLOCK_MONOTONIC, &txmeas);
  LOG_D(NR_PHY,
        "IF5 TX %d.%d, TS %lu, buffs[0] %p, buffs[1] %p ener0 %f dB, tx start %d\n",
        frame,
        slot,
        timestamp,
        buffs[0],
        buffs[1],
        10 * log10((double)signal_energy(buffs[0], ru->nr_frame_parms->get_samples_per_slot(slot, ru->nr_frame_parms))),
        (int)txmeas.tv_nsec);
  ru->ifdevice.trx_write_func2(&ru->ifdevice,
                               timestamp,
                               buffs,
                               0,
                               ru->nr_frame_parms->get_samples_per_slot(slot,ru->nr_frame_parms),
                               0,
                               ru->nb_tx);
}

// southbound IF4p5 fronthaul
void fh_if4p5_south_out(RU_t *ru, int frame, int slot, uint64_t timestamp) {
  if (ru == RC.ru[0])
    VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME(VCD_SIGNAL_DUMPER_VARIABLES_TRX_TST, ru->proc.timestamp_tx & 0xffffffff);

  LOG_D(PHY,"Sending IF4p5 for frame %d subframe %d\n",ru->proc.frame_tx,ru->proc.tti_tx);

  if ((nr_slot_select(&ru->config, ru->proc.frame_tx, ru->proc.tti_tx) & NR_DOWNLINK_SLOT) > 0)
    send_IF4p5(ru,frame, slot, IF4p5_PDLFFT);
}

/*************************************************************/
/* Input Fronthaul from south RCC/RAU                        */

// Synchronous if5 from south

void fh_if5_south_in(RU_t *ru,
                     int *frame,
                     int *tti) {
  NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;
  RU_proc_t *proc = &ru->proc;
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME( VCD_SIGNAL_DUMPER_FUNCTIONS_RECV_IF5, 1 );   
  start_meas(&ru->rx_fhaul);

  ru->ifdevice.trx_read_func2(&ru->ifdevice, &proc->timestamp_rx, NULL, fp->get_samples_per_slot(*tti, fp));
  if (proc->first_rx == 1)
    ru->ts_offset = proc->timestamp_rx;
  proc->frame_rx = ((proc->timestamp_rx - ru->ts_offset) / (fp->samples_per_subframe * 10)) & 1023;
  proc->tti_rx = fp->get_slot_from_timestamp(proc->timestamp_rx - ru->ts_offset, fp);

  if (proc->first_rx == 0) {
    if (proc->tti_rx != *tti) {
      LOG_E(PHY,"Received Timestamp doesn't correspond to the time we think it is (proc->tti_rx %d, subframe %d)\n",proc->tti_rx,*tti);
      if (!oai_exit)
        exit_fun("Exiting");
      return;
    }

    if (proc->frame_rx != *frame) {
      LOG_E(PHY,"Received Timestamp doesn't correspond to the time we think it is (proc->frame_rx %d frame %d proc->tti_rx %d tti %d)\n",proc->frame_rx,*frame,proc->tti_rx,*tti);
      if (!oai_exit)
        exit_fun("Exiting");
      return;
    }
  } else {
    proc->first_rx = 0;
    *frame = proc->frame_rx;
    *tti = proc->tti_rx;
  }

  stop_meas(&ru->rx_fhaul);
  struct timespec rxmeas;
  clock_gettime(CLOCK_MONOTONIC, &rxmeas);
  double fhtime = ru->rx_fhaul.p_time/(cpu_freq_GHz*1000.0);
  if (fhtime > 800)
    LOG_W(PHY,
          "IF5 %d.%d => RX %d.%d first_rx %d: time %f, rxstart %ld\n",
          *frame,
          *tti,
          proc->frame_rx,
          proc->tti_rx,
          proc->first_rx,
          ru->rx_fhaul.p_time / (cpu_freq_GHz * 1000.0),
          rxmeas.tv_nsec);
  else
    LOG_D(PHY,
          "IF5 %d.%d => RX %d.%d first_rx %d: time %f, rxstart %ld\n",
          *frame,
          *tti,
          proc->frame_rx,
          proc->tti_rx,
          proc->first_rx,
          ru->rx_fhaul.p_time / (cpu_freq_GHz * 1000.0),
          rxmeas.tv_nsec);
  VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME( VCD_SIGNAL_DUMPER_VARIABLES_TRX_TS, proc->timestamp_rx&0xffffffff );
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME( VCD_SIGNAL_DUMPER_FUNCTIONS_RECV_IF5, 0 );

}

// Synchronous if4p5 from south
void fh_if4p5_south_in(RU_t *ru,
                       int *frame,
                       int *slot) {
  NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;
  RU_proc_t *proc = &ru->proc;
  int f,sl;
  uint16_t packet_type;
  uint32_t symbol_number=0;
  uint32_t symbol_mask_full=0;

  do {   // Blocking, we need a timeout on this !!!!!!!!!!!!!!!!!!!!!!!
    recv_IF4p5(ru, &f, &sl, &packet_type, &symbol_number);

    if (packet_type == IF4p5_PULFFT) proc->symbol_mask[sl] = proc->symbol_mask[sl] | (1<<symbol_number);
    else if (packet_type == IF4p5_PULTICK) {
      if ((proc->first_rx == 0) && (f != *frame))
        LOG_E(PHY, "rx_fh_if4p5: PULTICK received frame %d != expected %d\n", f, *frame);

      if ((proc->first_rx == 0) && (sl != *slot))
        LOG_E(PHY, "rx_fh_if4p5: PULTICK received subframe %d != expected %d (first_rx %d)\n", sl, *slot, proc->first_rx);

      break;
    } else if (packet_type == IF4p5_PRACH) {
      // nothing in RU for RAU
    }

    LOG_D(PHY,"rx_fh_if4p5: subframe %d symbol mask %x\n",*slot,proc->symbol_mask[sl]);
  } while(proc->symbol_mask[sl] != symbol_mask_full);

  //caculate timestamp_rx, timestamp_tx based on frame and subframe
  proc->tti_rx   = sl;
  proc->frame_rx = f;
  proc->timestamp_rx = (proc->frame_rx * fp->samples_per_subframe * 10)  + fp->get_samples_slot_timestamp(proc->tti_rx, fp, 0);
  //  proc->timestamp_tx = proc->timestamp_rx +  (4*fp->samples_per_subframe);
  proc->tti_tx   = (sl+ru->sl_ahead)%fp->slots_per_frame;
  proc->frame_tx = (sl > (fp->slots_per_frame - 1 - (ru->sl_ahead))) ? (f + 1) & 1023 : f;

  if (proc->first_rx == 0) {
    if (proc->tti_rx != *slot) {
      LOG_E(PHY,"Received Timestamp (IF4p5) doesn't correspond to the time we think it is (proc->tti_rx %d, subframe %d)\n",proc->tti_rx,*slot);
      exit_fun("Exiting");
    }

    if (proc->frame_rx != *frame) {
      LOG_E(PHY,"Received Timestamp (IF4p5) doesn't correspond to the time we think it is (proc->frame_rx %d frame %d)\n",proc->frame_rx,*frame);
      exit_fun("Exiting");
    }
  } else {
    proc->first_rx = 0;
    *frame = proc->frame_rx;
    *slot = proc->tti_rx;
  }

  if (ru == RC.ru[0]) {
    VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME( VCD_SIGNAL_DUMPER_VARIABLES_FRAME_NUMBER_RX0_RU, f );
    VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME( VCD_SIGNAL_DUMPER_VARIABLES_TTI_NUMBER_RX0_RU,  sl);
    VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME( VCD_SIGNAL_DUMPER_VARIABLES_FRAME_NUMBER_TX0_RU, proc->frame_tx );
    VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME( VCD_SIGNAL_DUMPER_VARIABLES_TTI_NUMBER_TX0_RU, proc->tti_tx );
  }

  proc->symbol_mask[proc->tti_rx] = 0;
  VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME( VCD_SIGNAL_DUMPER_VARIABLES_TRX_TS, proc->timestamp_rx&0xffffffff );
  LOG_D(PHY,"RU %d: fh_if4p5_south_in sleeping ...\n",ru->idx);
}

// asynchronous inbound if4p5 fronthaul from south
void fh_if4p5_south_asynch_in(RU_t *ru,int *frame,int *slot) {
  NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;
  RU_proc_t *proc       = &ru->proc;
  uint16_t packet_type;
  uint32_t symbol_number = 0;
  uint32_t symbol_mask = (1 << fp->symbols_per_slot) - 1;
  uint32_t prach_rx = 0;

  do {   // Blocking, we need a timeout on this !!!!!!!!!!!!!!!!!!!!!!!
    recv_IF4p5(ru, &proc->frame_rx, &proc->tti_rx, &packet_type, &symbol_number);
    if (proc->first_rx != 0) {
      *frame = proc->frame_rx;
      *slot = proc->tti_rx;
      proc->first_rx = 0;
    } else {
      if (proc->frame_rx != *frame) {
        LOG_E(PHY,"frame_rx %d is not what we expect %d\n",proc->frame_rx,*frame);
        exit_fun("Exiting");
      }
      if (proc->tti_rx != *slot) {
        LOG_E(PHY,"tti_rx %d is not what we expect %d\n",proc->tti_rx,*slot);
        exit_fun("Exiting");
      }
    }

    if (packet_type == IF4p5_PULFFT)
      symbol_mask &= ~(1 << symbol_number);
    else if (packet_type == IF4p5_PRACH)
      prach_rx &= ~0x1;
  } while (symbol_mask > 0 || prach_rx > 0); // haven't received all PUSCH symbols and PRACH information
}

/*************************************************************/
/* Input Fronthaul from North RRU                            */

// RRU IF4p5 TX fronthaul receiver. Assumes an if_device on input and if or rf device on output
// receives one subframe's worth of IF4p5 OFDM symbols and OFDM modulates
void fh_if4p5_north_in(RU_t *ru,int *frame,int *slot) {
  uint32_t symbol_number=0;
  uint32_t symbol_mask, symbol_mask_full;
  uint16_t packet_type;
  /// **** incoming IF4p5 from remote RCC/RAU **** ///
  symbol_number = 0;
  symbol_mask = 0;
  symbol_mask_full = (1<<(ru->nr_frame_parms->symbols_per_slot))-1;

  do {
    recv_IF4p5(ru, frame, slot, &packet_type, &symbol_number);
    symbol_mask = symbol_mask | (1<<symbol_number);
  } while (symbol_mask != symbol_mask_full);

  // dump VCD output for first RU in list
  if (ru == RC.ru[0]) {
    VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME( VCD_SIGNAL_DUMPER_VARIABLES_FRAME_NUMBER_TX0_RU, *frame );
    VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME( VCD_SIGNAL_DUMPER_VARIABLES_TTI_NUMBER_TX0_RU, *slot );
  }
}

void fh_if5_north_asynch_in(RU_t *ru, int *frame, int *slot)
{
  AssertFatal(1 == 0, "Shouldn't get here\n");
}

void fh_if4p5_north_asynch_in(RU_t *ru,int *frame,int *slot) {
  NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;
  nfapi_nr_config_request_scf_t *cfg = &ru->config;
  RU_proc_t *proc        = &ru->proc;
  uint16_t packet_type;
  uint32_t symbol_mask_full = 0;
  int slot_tx,frame_tx;
  LOG_D(PHY, "%s(ru:%p frame, subframe)\n", __FUNCTION__, ru);
  uint32_t symbol_number = 0;
  uint32_t symbol_mask = 0;

  //  symbol_mask_full = ((subframe_select(fp,*slot) == SF_S) ? (1<<fp->dl_symbols_in_S_subframe) : (1<<fp->symbols_per_slot))-1;
  do {
    recv_IF4p5(ru, &frame_tx, &slot_tx, &packet_type, &symbol_number);

    if (((nr_slot_select(cfg, frame_tx, slot_tx) & NR_DOWNLINK_SLOT) > 0) && (symbol_number == 0))
      start_meas(&ru->rx_fhaul);

    LOG_D(PHY,"slot %d (%d): frame %d, slot %d, symbol %d\n",
          *slot,nr_slot_select(cfg,frame_tx,*slot),frame_tx,slot_tx,symbol_number);

    if (proc->first_tx != 0) {
      *frame         = frame_tx;
      *slot          = slot_tx;
      proc->first_tx = 0;
    } else {
      AssertFatal(frame_tx == *frame,
                  "frame_tx %d is not what we expect %d\n",frame_tx,*frame);
      AssertFatal(slot_tx == *slot,
                  "slot_tx %d is not what we expect %d\n",slot_tx,*slot);
    }

    if (packet_type == IF4p5_PDLFFT) {
      symbol_mask = symbol_mask | (1<<symbol_number);
    } else
      AssertFatal(false, "Illegal IF4p5 packet type (should only be IF4p5_PDLFFT%d\n", packet_type);
  } while (symbol_mask != symbol_mask_full);

  if ((nr_slot_select(cfg, frame_tx, slot_tx) & NR_DOWNLINK_SLOT) > 0)
    stop_meas(&ru->rx_fhaul);

  proc->tti_tx = slot_tx;
  proc->frame_tx = frame_tx;

  if (frame_tx == 0 && slot_tx == 0)
    proc->frame_tx_unwrap += 1024;

  proc->timestamp_tx =
      ((uint64_t)frame_tx + proc->frame_tx_unwrap) * fp->samples_per_subframe * 10 + fp->get_samples_slot_timestamp(slot_tx, fp, 0);
  LOG_D(PHY, "RU %d/%d TST %lu, frame %d, subframe %d\n", ru->idx, 0, proc->timestamp_tx, frame_tx, slot_tx);

  // dump VCD output for first RU in list
  if (ru == RC.ru[0]) {
    VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME( VCD_SIGNAL_DUMPER_VARIABLES_FRAME_NUMBER_TX0_RU, frame_tx );
    VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME( VCD_SIGNAL_DUMPER_VARIABLES_TTI_NUMBER_TX0_RU, slot_tx );
  }

  if (ru->feptx_ofdm)
    ru->feptx_ofdm(ru, frame_tx, slot_tx);

  if (ru->fh_south_out)
    ru->fh_south_out(ru, frame_tx, slot_tx, proc->timestamp_tx);
}

void fh_if5_north_out(RU_t *ru) {
  /// **** send_IF5 of rxdata to BBU **** ///
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME( VCD_SIGNAL_DUMPER_FUNCTIONS_SEND_IF5, 1 );
  AssertFatal(1 == 0, "Shouldn't get here\n");
}

// RRU IF4p5 northbound interface (RX)
void fh_if4p5_north_out(RU_t *ru) {
  RU_proc_t *proc=&ru->proc;
  if (ru->idx == 0)
    VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME(VCD_SIGNAL_DUMPER_VARIABLES_TTI_NUMBER_RX0_RU, proc->tti_rx);

  start_meas(&ru->tx_fhaul);
  send_IF4p5(ru, proc->frame_rx, proc->tti_rx, IF4p5_PULFFT);
  stop_meas(&ru->tx_fhaul);
}

static void rx_rf(RU_t *ru, int *frame, int *slot)
{
  RU_proc_t *proc = &ru->proc;
  NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;
  openair0_config_t *cfg   = &ru->openair0_cfg;
  uint32_t samples_per_slot = fp->get_samples_per_slot(*slot, fp);
  AssertFatal(*slot < fp->slots_per_frame && *slot >= 0, "slot %d is illegal (%d)\n", *slot, fp->slots_per_frame);

  start_meas(&ru->rx_fhaul);
  int nb = ru->nb_rx * ru->num_beams_period;
  void *rxp[nb];
  for (int i = 0; i < nb; i++)
    rxp[i] = (void *)&ru->common.rxdata[i][fp->get_samples_slot_timestamp(*slot, fp, 0)];

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_TRX_READ, 1);
  openair0_timestamp old_ts = proc->timestamp_rx;
  LOG_D(PHY,"Reading %d samples for slot %d (%p)\n", samples_per_slot, *slot, rxp[0]);

  openair0_timestamp ts;
  unsigned int rxs;
  rxs = ru->rfdevice.trx_read_func(&ru->rfdevice, &ts, rxp, samples_per_slot, nb);

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME( VCD_SIGNAL_DUMPER_FUNCTIONS_TRX_READ, 0 );
  proc->timestamp_rx = ts-ru->ts_offset;

  if (rxs != samples_per_slot)
    LOG_E(PHY, "rx_rf: Asked for %d samples, got %d from USRP\n", samples_per_slot, rxs);

  if (proc->first_rx != 1) {
    uint32_t samples_per_slot_prev = fp->get_samples_per_slot((*slot - 1) % fp->slots_per_frame, fp);

    if (proc->timestamp_rx - old_ts != samples_per_slot_prev) {
      LOG_D(PHY,
            "rx_rf: rfdevice timing drift of %" PRId64 " samples (ts_off %" PRId64 ")\n",
            proc->timestamp_rx - old_ts - samples_per_slot_prev,
            ru->ts_offset);
      ru->ts_offset += (proc->timestamp_rx - old_ts - samples_per_slot_prev);
      proc->timestamp_rx = ts-ru->ts_offset;
    }
  }

  // compute system frame number (SFN) according to O-RAN-WG4-CUS.0-v02.00 (using alpha=beta=0)
  //  this assumes that the USRP has been synchronized to the GPS time
  //  OAI uses timestamps in sample time stored in int64_t, but it will fit in double precision for many years to come.
  double gps_sec = ((double)ts) / cfg->sample_rate;

  // in fact the following line is the same as long as the timestamp_rx is synchronized to GPS. 
  proc->frame_rx    = (proc->timestamp_rx / (fp->samples_per_subframe*10))&1023;
  proc->tti_rx = fp->get_slot_from_timestamp(proc->timestamp_rx,fp);
  // synchronize first reception to frame 0 subframe 0
  LOG_D(PHY,
        "RU %d/%d TS %ld, GPS %f, SR %f, frame %d, slot %d.%d / %d\n",
        ru->idx,
        0,
        ts,
        gps_sec,
        cfg->sample_rate,
        proc->frame_rx,
        proc->tti_rx,
        proc->tti_tx,
        fp->slots_per_frame);

  // dump VCD output for first RU in list
  if (ru == RC.ru[0]) {
    VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME( VCD_SIGNAL_DUMPER_VARIABLES_FRAME_NUMBER_RX0_RU, proc->frame_rx );
    VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME( VCD_SIGNAL_DUMPER_VARIABLES_TTI_NUMBER_RX0_RU, proc->tti_rx );
  }

  if (proc->first_rx == 0) {
    if (proc->tti_rx != *slot) {
      LOG_E(PHY,
            "Received Timestamp (%lu) doesn't correspond to the time we think it is (proc->tti_rx %d, slot %d)\n",
            proc->timestamp_rx,
            proc->tti_rx,
            *slot);
      exit_fun("Exiting");
    }

    if (proc->frame_rx != *frame) {
      LOG_E(PHY,
            "Received Timestamp (%lu) doesn't correspond to the time we think it is (proc->frame_rx %d frame %d, proc->tti_rx %d, "
            "slot %d)\n",
            proc->timestamp_rx,
            proc->frame_rx,
            *frame,
            proc->tti_rx,
            *slot);
      exit_fun("Exiting");
    }
  } else {
    proc->first_rx = 0;
    *frame = proc->frame_rx;
    *slot  = proc->tti_rx;
  }

  metadata mt = {.slot = *slot, .frame = *frame};
  gNBscopeCopyWithMetadata(ru, gNbTimeDomainSamples, rxp[0], sizeof(c16_t), 1, samples_per_slot, 0, &mt);

  VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME( VCD_SIGNAL_DUMPER_VARIABLES_TRX_TS, (proc->timestamp_rx+ru->ts_offset)&0xffffffff );

  if (rxs != samples_per_slot) {
    //exit_fun( "problem receiving samples" );
    LOG_E(PHY, "problem receiving samples\n");
  }

  stop_meas(&ru->rx_fhaul);
}

static radio_tx_gpio_flag_t get_gpio_flags(RU_t *ru, int slot)
{
  radio_tx_gpio_flag_t flags_gpio = 0;
  NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;
  openair0_config_t *cfg0 = &ru->openair0_cfg;

  switch (cfg0->gpio_controller) {
    case RU_GPIO_CONTROL_GENERIC:
      // currently we switch beams at the beginning of a slot and we take the beam index of the first symbol of this slot
      // we only send the beam to the gpio if the beam is different from the previous slot

      if (ru->common.beam_id) {
        int prev_slot = (slot - 1 + fp->slots_per_frame) % fp->slots_per_frame;
        const int *beam_ids = ru->common.beam_id[0];
        int prev_beam = beam_ids[prev_slot * fp->symbols_per_slot];
        int beam = beam_ids[slot * fp->symbols_per_slot];
        if (prev_beam != beam) {
          flags_gpio = beam | TX_GPIO_CHANGE; // enable change of gpio
          LOG_I(HW, "slot %d, beam %d\n", slot, ru->common.beam_id[0][slot * fp->symbols_per_slot]);
        }
      }
      break;

    case RU_GPIO_CONTROL_INTERDIGITAL: {
      // the beam index is written in bits 8-10 of the flags
      // bit 11 enables the gpio programming
      int beam = 0;
      if ((slot % 10 == 0) && ru->common.beam_id && (ru->common.beam_id[0][slot * fp->symbols_per_slot] < 64)) {
        // beam = ru->common.beam_id[0][slot*fp->symbols_per_slot] | 64;
        beam = 1024; // hardcoded now for beam32 boresight
        // beam = 127; //for the sake of trying beam63
        LOG_D(HW, "slot %d, beam %d\n", slot, beam);
      }
      flags_gpio = beam | TX_GPIO_CHANGE;
      // flags_gpio |= beam << 8; // MSB 8 bits are used for beam
      LOG_I(HW, "slot %d, beam %d, flags_gpio %d\n", slot, beam, flags_gpio);
      break;
    }
    default:
      AssertFatal(false, "illegal GPIO controller %d\n", cfg0->gpio_controller);
  }

  return flags_gpio;
}

void tx_rf(RU_t *ru, int frame,int slot, uint64_t timestamp)
{
  RU_proc_t *proc = &ru->proc;
  NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;
  nfapi_nr_config_request_scf_t *cfg = &ru->config;
  T(T_ENB_PHY_OUTPUT_SIGNAL,
    T_INT(0),
    T_INT(0),
    T_INT(frame),
    T_INT(slot),
    T_INT(0),
    T_BUFFER(&ru->common.txdata[0][fp->get_samples_slot_timestamp(slot, fp, 0)], fp->get_samples_per_slot(slot, fp) * 4));
  int sf_extension = 0;
  int siglen=fp->get_samples_per_slot(slot,fp);
  radio_tx_burst_flag_t flags_burst = TX_BURST_INVALID;
  radio_tx_gpio_flag_t flags_gpio = 0;

  if (cfg->cell_config.frame_duplex_type.value == TDD && !get_softmodem_params()->continuous_tx && !IS_SOFTMODEM_RFSIM) {
    int slot_type = nr_slot_select(cfg,frame,slot%fp->slots_per_frame);
    if(slot_type == NR_MIXED_SLOT) {
      int txsymb = 0;

      for(int symbol_count = 0; symbol_count<NR_NUMBER_OF_SYMBOLS_PER_SLOT; symbol_count++) {
        if (cfg->tdd_table.max_tdd_periodicity_list[slot].max_num_of_symbol_per_slot_list[symbol_count].slot_config.value == 0)
          txsymb++;
      }

      AssertFatal(txsymb>0,"illegal txsymb %d\n",txsymb);

      if (fp->slots_per_subframe == 1) {
        if (txsymb <= 7)
          siglen = (fp->ofdm_symbol_size + fp->nb_prefix_samples0) + (txsymb - 1) * (fp->ofdm_symbol_size + fp->nb_prefix_samples);
        else
          siglen = 2 * (fp->ofdm_symbol_size + fp->nb_prefix_samples0) + (txsymb - 2) * (fp->ofdm_symbol_size + fp->nb_prefix_samples);
      } else {
        if(slot%(fp->slots_per_subframe/2))
          siglen = txsymb * (fp->ofdm_symbol_size + fp->nb_prefix_samples);
        else
          siglen = (fp->ofdm_symbol_size + fp->nb_prefix_samples0) + (txsymb - 1) * (fp->ofdm_symbol_size + fp->nb_prefix_samples);
      }

      //+ ru->end_of_burst_delay;
      flags_burst = TX_BURST_END;
    } else if (slot_type == NR_DOWNLINK_SLOT) {
      int prevslot_type = nr_slot_select(cfg,frame,(slot+(fp->slots_per_frame-1))%fp->slots_per_frame);
      int nextslot_type = nr_slot_select(cfg,frame,(slot+1)%fp->slots_per_frame);
      if (prevslot_type == NR_UPLINK_SLOT) {
        flags_burst = TX_BURST_START;
        sf_extension = ru->sf_extension;
      } else if (nextslot_type == NR_UPLINK_SLOT) {
        flags_burst = TX_BURST_END;
      } else {
        flags_burst = proc->first_tx == 1 ? TX_BURST_START : TX_BURST_MIDDLE;
      }
    }
  } else { // FDD
    flags_burst = proc->first_tx == 1 ? TX_BURST_START : TX_BURST_MIDDLE;
  }

  if (ru->openair0_cfg.gpio_controller != RU_GPIO_CONTROL_NONE)
    flags_gpio = get_gpio_flags(ru, slot);

  const int flags = flags_burst | (flags_gpio << 4);
  proc->first_tx = 0;

  VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME(VCD_SIGNAL_DUMPER_VARIABLES_TRX_WRITE_FLAGS, flags);
  VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME(VCD_SIGNAL_DUMPER_VARIABLES_FRAME_NUMBER_TX0_RU, frame);
  VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME(VCD_SIGNAL_DUMPER_VARIABLES_TTI_NUMBER_TX0_RU, slot);

  int nt = ru->nb_tx * ru->num_beams_period;
  void *txp[nt];
  for (int i = 0; i < nt; i++)
    txp[i] = (void *)&ru->common.txdata[i][fp->get_samples_slot_timestamp(slot, fp, 0)] - sf_extension * sizeof(int32_t);

  VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME(VCD_SIGNAL_DUMPER_VARIABLES_TRX_TST, (timestamp + ru->ts_offset) & 0xffffffff);
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_TRX_WRITE, 1);
  // prepare tx buffer pointers
  uint32_t txs = ru->rfdevice.trx_write_func(&ru->rfdevice,
                                             timestamp + ru->ts_offset - sf_extension,
                                             txp,
                                             siglen + sf_extension,
                                             nt,
                                             flags);
  LOG_D(PHY,
        "[TXPATH] RU %d tx_rf, writing to TS %lu, %d.%d, unwrapped_frame %d, slot %d, flags %d, siglen+sf_extension %d, "
        "returned %d, E %f\n",
        ru->idx,
        timestamp + ru->ts_offset - sf_extension,
        frame,
        slot,
        proc->frame_tx_unwrap,
        slot,
        flags,
        siglen + sf_extension,
        txs,
        10 * log10((double)signal_energy(txp[0], siglen + sf_extension)));
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_TRX_WRITE, 0);
}

static void fill_rf_config(RU_t *ru, char *rf_config_file)
{
  NR_DL_FRAME_PARMS *fp   = ru->nr_frame_parms;
  nfapi_nr_config_request_scf_t *config = &ru->config; //tmp index
  openair0_config_t *cfg   = &ru->openair0_cfg;
  int mu = config->ssb_config.scs_common.value;
  int N_RB = config->carrier_config.dl_grid_size[config->ssb_config.scs_common.value].value;

  get_samplerate_and_bw(mu,
                        N_RB,
                        fp->threequarter_fs,
                        &cfg->sample_rate,
                        &cfg->samples_per_frame,
                        &cfg->tx_bw,
                        &cfg->rx_bw);

  if (config->cell_config.frame_duplex_type.value==TDD)
    cfg->duplex_mode = duplex_mode_TDD;
  else //FDD
    cfg->duplex_mode = duplex_mode_FDD;

  cfg->configFilename = rf_config_file;

  AssertFatal(ru->nb_tx > 0 && ru->nb_tx <= 8, "openair0 does not support more than 8 antennas\n");
  AssertFatal(ru->nb_rx > 0 && ru->nb_rx <= 8, "openair0 does not support more than 8 antennas\n");

  cfg->Mod_id = 0;
  cfg->num_rb_dl = N_RB;
  cfg->tx_num_channels = ru->nb_tx * ru->num_beams_period;
  cfg->rx_num_channels = ru->nb_rx * ru->num_beams_period;
  cfg->num_distributed_ru = ru->num_beams_period;
  LOG_I(PHY,"Setting RF config for N_RB %d, NB_RX %d, NB_TX %d\n",cfg->num_rb_dl,cfg->rx_num_channels,cfg->tx_num_channels);
  LOG_I(PHY,"tune_offset %.0f Hz, sample_rate %.0f Hz\n",cfg->tune_offset,cfg->sample_rate);

  for (int i = 0; i < ru->nb_tx * ru->num_beams_period; i++) {
    if (ru->if_frequency == 0) {
      cfg->tx_freq[i] = fp->dl_CarrierFreq;
    } else if (ru->if_freq_offset) {
      cfg->tx_freq[i] = ru->if_frequency;
      LOG_I(PHY, "Setting IF TX frequency to %lu Hz with IF TX frequency offset %d Hz\n", ru->if_frequency, ru->if_freq_offset);
    } else {
      cfg->tx_freq[i] = ru->if_frequency;
    }

    cfg->tx_gain[i] = ru->att_tx;
    LOG_I(PHY, "Channel %d: setting tx_gain offset %.0f, tx_freq %.0f Hz\n", 
          i, cfg->tx_gain[i],cfg->tx_freq[i]);
  }

  for (int i = 0; i < ru->nb_rx * ru->num_beams_period; i++) {
    if (ru->if_frequency == 0) {
      cfg->rx_freq[i] = fp->ul_CarrierFreq;
    } else if (ru->if_freq_offset) {
      cfg->rx_freq[i] = ru->if_frequency + ru->if_freq_offset;
      LOG_I(PHY, "Setting IF RX frequency to %lu Hz with IF RX frequency offset %d Hz\n", ru->if_frequency, ru->if_freq_offset);
    } else {
      cfg->rx_freq[i] = ru->if_frequency + fp->ul_CarrierFreq - fp->dl_CarrierFreq;
    }

    cfg->rx_gain[i] = ru->max_rxgain-ru->att_rx;
    LOG_I(PHY, "Channel %d: setting rx_gain offset %.0f, rx_freq %.0f Hz\n",
          i,cfg->rx_gain[i],cfg->rx_freq[i]);
  }
}

static void fill_split7_2_config(split7_config_t *split7, const nfapi_nr_config_request_scf_t *config, const NR_DL_FRAME_PARMS *fp)
{
  const nfapi_nr_prach_config_t *prach_config = &config->prach_config;
  const nfapi_nr_tdd_table_t *tdd_table = &config->tdd_table;
  const nfapi_nr_cell_config_t *cell_config = &config->cell_config;
  const nfapi_nr_carrier_config_t *carrier_config = &config->carrier_config;

  DevAssert(prach_config->prach_ConfigurationIndex.tl.tag == NFAPI_NR_CONFIG_PRACH_CONFIG_INDEX_TAG);
  split7->prach_index = prach_config->prach_ConfigurationIndex.value;
  AssertFatal(prach_config->num_prach_fd_occasions.value == 1, "cannot handle more than one PRACH occasion\n");
  split7->prach_freq_start = prach_config->num_prach_fd_occasions_list[0].k1.value;

  DevAssert(cell_config->frame_duplex_type.tl.tag == NFAPI_NR_CONFIG_FRAME_DUPLEX_TYPE_TAG);
  if (cell_config->frame_duplex_type.value == 1 /* TDD */) {
    DevAssert(tdd_table->tdd_period.tl.tag == NFAPI_NR_CONFIG_TDD_PERIOD_TAG);
    int nb_periods_per_frame = get_nb_periods_per_frame(tdd_table->tdd_period.value);
    split7->n_tdd_period = fp->slots_per_frame / nb_periods_per_frame;
    for (int slot = 0; slot < split7->n_tdd_period; ++slot) {
      for (int sym = 0; sym < 14; ++sym) {
        split7->slot_dirs[slot].sym_dir[sym] = tdd_table->max_tdd_periodicity_list[slot].max_num_of_symbol_per_slot_list[sym].slot_config.value;
      }
    }
  }

  split7->fftSize = log2(fp->ofdm_symbol_size);

  // M-plane related parameters
  for (size_t i = 0; i < 5 ; i++) {
    split7->dl_k0[i] = carrier_config->dl_k0[i].value;
    split7->ul_k0[i] = carrier_config->ul_k0[i].value;
  }
  split7->cp_prefix0 = fp->nb_prefix_samples0;
  split7->cp_prefix_other = fp->nb_prefix_samples;
}

/* this function maps the RU tx and rx buffers to the available rf chains.
   Each rf chain is is addressed by the card number and the chain on the card. The
   rf_map specifies for each antenna port, on which rf chain the mapping should start. Multiple
   antennas are mapped to successive RF chains on the same card. */
int setup_RU_buffers(RU_t *ru)
{
  if (!ru)
    return (-1);

  NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;
  nfapi_nr_config_request_scf_t *config = &ru->config;
  int mu = config->ssb_config.scs_common.value;
  int N_RB = config->carrier_config.dl_grid_size[config->ssb_config.scs_common.value].value;

  ru->N_TA_offset = set_default_nta_offset(fp->freq_range, fp->samples_per_subframe);
  LOG_I(PHY,
        "RU %d Setting N_TA_offset to %d samples (UL Freq %d, N_RB %d, mu %d)\n",
        ru->idx, ru->N_TA_offset, config->carrier_config.uplink_frequency.value, N_RB, mu);

  if (ru->openair0_cfg.mmapped_dma == 1) {
    // replace RX signal buffers with mmaped HW versions
    for (int i = 0; i < ru->nb_rx * ru->num_beams_period; i++) {
      int card = i / 4;
      int ant = i % 4;
      LOG_D(PHY, "Mapping RU id %u, rx_ant %d, on card %d, chain %d\n", ru->idx, i, ru->rf_map.card + card, ru->rf_map.chain + ant);
      free(ru->common.rxdata[i]);
      ru->common.rxdata[i] = ru->openair0_cfg.rxbase[ru->rf_map.chain + ant];

      for (int j = 0; j < 16; j++) {
        ru->common.rxdata[i][j] = 16 - j;
      }
    }

    for (int i = 0; i < ru->nb_tx * ru->num_beams_period; i++) {
      int card = i / 4;
      int ant = i % 4;
      LOG_D(PHY, "Mapping RU id %u, tx_ant %d, on card %d, chain %d\n", ru->idx, i, ru->rf_map.card + card, ru->rf_map.chain + ant);
      free(ru->common.txdata[i]);
      ru->common.txdata[i] = ru->openair0_cfg.txbase[ru->rf_map.chain + ant];

      for (int j = 0; j < 16; j++) {
        ru->common.txdata[i][j] = 16 - j;
      }
    }
  } else {
    // not memory-mapped DMA
    // nothing to do, everything already allocated in lte_init
  }

  return(0);
}

void ru_tx_func(void *param)
{
  processingData_RU_t *info = (processingData_RU_t *) param;
  RU_t *ru = info->ru;
  int frame_tx = info->frame_tx;
  int slot_tx = info->slot_tx;

  // do TX front-end processing if needed (precoding and/or IDFTs)
  if (ru->feptx_prec)
    ru->feptx_prec(ru,frame_tx,slot_tx);

  // do OFDM with/without TX front-end processing  if needed
  if (ru->fh_north_asynch_in == NULL && ru->feptx_ofdm)
    ru->feptx_ofdm(ru, frame_tx, slot_tx);

  if (ru->fh_north_asynch_in == NULL && ru->fh_south_out)
    ru->fh_south_out(ru, frame_tx, slot_tx, info->timestamp_tx);
  if (ru->fh_north_out)
    ru->fh_north_out(ru);
}

/* @brief wait for the next RX TTI to be free
 *
 * Certain radios, e.g., RFsim, can run faster than real-time. This might
 * create problems, e.g., if RX and TX get too far from each other. This
 * function ensures that a maximum of 4 RX slots are processed at a time (and
 * not more than those four are started).
 *
 * Through the queue L1_rx_out, we are informed about completed RX jobs.
 * rx_tti_busy keeps track of individual slots that have been started; this
 * function blocks until the current frame/slot is completed, signaled through
 * a message.
 *
 * @param L1_rx_out the queue from which to read completed RX jobs
 * @param rx_tti_busy array to mark RX job completion
 * @param frame_rx the frame to wait for
 * @param slot_rx the slot to wait for
 */
static bool wait_free_rx_tti(notifiedFIFO_t *L1_rx_out, bool rx_tti_busy[RU_RX_SLOT_DEPTH], int frame_rx, int slot_rx)
{
  int idx = slot_rx % RU_RX_SLOT_DEPTH;
  if (rx_tti_busy[idx]) {
    bool not_done = true;
    LOG_D(NR_PHY, "%d.%d Waiting to access RX slot %d\n", frame_rx, slot_rx, idx);
    // block and wait for frame_rx/slot_rx free from previous slot processing.
    // as we can get other slots, we loop on the queue
    while (not_done) {
      notifiedFIFO_elt_t *res = pullNotifiedFIFO(L1_rx_out);
      if (!res)
        return false;
      processingData_L1_t *info = NotifiedFifoData(res);
      LOG_D(NR_PHY, "%d.%d Got access to RX slot %d.%d (%d)\n", frame_rx, slot_rx, info->frame_rx, info->slot_rx, idx);
      rx_tti_busy[info->slot_rx % RU_RX_SLOT_DEPTH] = false;
      if ((info->slot_rx % RU_RX_SLOT_DEPTH) == idx)
        not_done = false;
      delNotifiedFIFO_elt(res);
    }
  }
  // set the tti to busy: the caller will process this slot now
  rx_tti_busy[idx] = true;
  return true;
}

void *ru_thread(void *param)
{
  static int ru_thread_status;
  RU_t               *ru      = (RU_t *)param;
  RU_proc_t          *proc    = &ru->proc;
  NR_DL_FRAME_PARMS  *fp      = ru->nr_frame_parms;
  PHY_VARS_gNB       *gNB     = RC.gNB[0];
  int                ret;
  int                slot     = fp->slots_per_frame-1;
  int                frame    = 1023;
  char               threadname[40];
  int initial_wait = 0;

  bool rx_tti_busy[RU_RX_SLOT_DEPTH] = {false};
  // set default return value
  ru_thread_status = 0;
  // set default return value
  sprintf(threadname,"ru_thread %u",ru->idx);
  LOG_I(PHY,"Starting RU %d (%s,%s) on cpu %d\n",ru->idx,NB_functions[ru->function],NB_timing[ru->if_timing],sched_getcpu());
  ru->config = RC.gNB[0]->gNB_config;

  nr_init_frame_parms(&ru->config, fp);
  nr_dump_frame_parms(fp);
  nr_phy_init_RU(ru);
  fill_rf_config(ru, ru->rf_config_file);
  fill_split7_2_config(&ru->openair0_cfg.split7, &ru->config, fp);

  // Start IF device if any
  if (ru->nr_start_if) {
    LOG_I(PHY, "starting transport\n");
    ret = openair0_transport_load(&ru->ifdevice, &ru->openair0_cfg, &ru->eth_params);
    AssertFatal(ret == 0, "RU %u: openair0_transport_init() ret %d: cannot initialize transport protocol\n", ru->idx, ret);

    if (ru->ifdevice.get_internal_parameter != NULL) {
      /* it seems the device can "overwrite" (request?) to set the callbacks
       * for fh_south_in()/fh_south_out() differently */
      void *t = ru->ifdevice.get_internal_parameter("fh_if4p5_south_in");
      if (t != NULL)
        ru->fh_south_in = t;
      t = ru->ifdevice.get_internal_parameter("fh_if4p5_south_out");
      if (t != NULL)
        ru->fh_south_out = t;
    } else {
      malloc_IF4p5_buffer(ru);
    }

    int cpu = sched_getcpu();
    if (ru->ru_thread_core > -1 && cpu != ru->ru_thread_core) {
      /* we start the ru_thread using threadCreate(), which already sets CPU
       * affinity; let's force it here again as per feature request #732 */
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(ru->ru_thread_core, &cpuset);
      int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
      AssertFatal(ret == 0, "Error in pthread_getaffinity_np(): ret: %d, errno: %d", ret, errno);
      LOG_I(PHY, "RU %d: manually set CPU affinity to CPU %d\n", ru->idx, ru->ru_thread_core);
    }

    LOG_I(PHY, "Starting IF interface for RU %d, nb_rx %d\n", ru->idx, ru->nb_rx);
    AssertFatal(ru->nr_start_if(ru, NULL) == 0, "Could not start the IF device\n");

    if (ru->has_ctrl_prt > 0) {
      if (ru->if_south == LOCAL_RF)
        ret = connect_rau(ru);
      else
        ret = attach_rru(ru);

      AssertFatal(ret == 0, "Cannot connect to remote radio\n");
    }

  } else if (ru->if_south == LOCAL_RF) { // configure RF parameters only
    ret = openair0_device_load(&ru->rfdevice,&ru->openair0_cfg);
    AssertFatal(ret==0,"Cannot connect to local radio\n");
  }

  if (setup_RU_buffers(ru)!=0) {
    LOG_E(PHY, "Exiting, cannot initialize RU Buffers\n");
    exit(-1);
  }

  LOG_I(PHY, "Signaling main thread that RU %d is ready, sl_ahead %d\n",ru->idx,ru->sl_ahead);
  pthread_mutex_lock(&RC.ru_mutex);
  RC.ru_mask &= ~(1<<ru->idx);
  pthread_cond_signal(&RC.ru_cond);
  pthread_mutex_unlock(&RC.ru_mutex);
  wait_sync("ru_thread");

  // Start RF device if any
  if (ru->start_rf) {
    if (ru->start_rf(ru) != 0)
      LOG_E(HW, "Could not start the RF device\n");
    else
      LOG_I(PHY, "RU %d rf device ready\n", ru->idx);
  } else
    LOG_I(PHY, "RU %d no rf device\n", ru->idx);

  LOG_I(PHY, "RU %d RF started cpu_meas_enabled %d\n", ru->idx, cpu_meas_enabled);
  // start trx write thread
  if (usrp_tx_thread == 1) {
    if (ru->start_write_thread) {
      if (ru->start_write_thread(ru) != 0) {
        LOG_E(HW, "Could not start tx write thread\n");
      } else {
        LOG_I(PHY, "tx write thread ready\n");
      }
    }
  }

  // This is a forever while loop, it loops over subframes which are scheduled by incoming samples from HW devices
  struct timespec slot_start;
  clock_gettime(CLOCK_MONOTONIC, &slot_start);

  while (!oai_exit) {
    if (slot==(fp->slots_per_frame-1)) {
      slot=0;
      frame++;
      frame&=1023;
    } else {
      slot++;
    }

    // pretend we have 1 iq sample per slot
    // and so slots_per_frame * 100 iq samples per second (1 frame being 10ms)
    time_manager_iq_samples(1, fp->slots_per_frame * 100);

    // synchronization on input FH interface, acquire signals/data and block
    LOG_D(PHY,"[RU_thread] read data: frame_rx = %d, tti_rx = %d\n", frame, slot);

    AssertFatal(ru->fh_south_in, "No fronthaul interface at south port");
    ru->fh_south_in(ru, &frame, &slot);

    if (initial_wait == 1 && proc->frame_rx < 300) {
      if (proc->frame_rx > 0 && ((proc->frame_rx % 100) == 0) && proc->tti_rx == 0) {
        LOG_D(PHY, "delay processing to let RX stream settle, frame %d (trials %d)\n", proc->frame_rx, ru->rx_fhaul.trials);
        print_meas(&ru->rx_fhaul, "rx_fhaul", NULL, NULL);
        reset_meas(&ru->rx_fhaul);
      }
      continue;
    }
    if (proc->frame_rx>=300)  {
      initial_wait = 0;
    }
    if (initial_wait == 0 && ru->rx_fhaul.trials > 1000) {
        reset_meas(&ru->rx_fhaul);
        reset_meas(&ru->tx_fhaul);
    }
    proc->timestamp_tx = proc->timestamp_rx;
    for (int i = proc->tti_rx; i < proc->tti_rx + ru->sl_ahead; i++)
      proc->timestamp_tx += fp->get_samples_per_slot(i % fp->slots_per_frame, fp);
    proc->tti_tx = (proc->tti_rx + ru->sl_ahead) % fp->slots_per_frame;
    proc->frame_tx = proc->tti_rx > proc->tti_tx ? (proc->frame_rx + 1) & 1023 : proc->frame_rx;
    LOG_D(PHY,"AFTER fh_south_in - SFN/SL:%d%d RU->proc[RX:%d.%d TX:%d.%d] RC.gNB[0]:[RX:%d%d TX(SFN):%d]\n",
          frame,slot,
          proc->frame_rx,proc->tti_rx,
          proc->frame_tx,proc->tti_tx,
          RC.gNB[0]->proc.frame_rx,RC.gNB[0]->proc.slot_rx,
          RC.gNB[0]->proc.frame_tx);

    if (ru->idx != 0)
      proc->frame_tx = (proc->frame_tx + proc->frame_offset) & 1023;

    // do RX front-end processing (frequency-shift, dft) if needed
    int slot_type = nr_slot_select(&ru->config, proc->frame_rx, proc->tti_rx);
    if (slot_type == NR_UPLINK_SLOT || slot_type == NR_MIXED_SLOT) {
      if (!wait_free_rx_tti(&gNB->L1_rx_out, rx_tti_busy, proc->frame_rx, proc->tti_rx))
        break; // nothing to wait for: we have to stop
      if (ru->feprx) {
        ru->feprx(ru,proc->tti_rx);
        LOG_D(NR_PHY, "Setting %d.%d (%d) to busy\n", proc->frame_rx, proc->tti_rx, proc->tti_rx % RU_RX_SLOT_DEPTH);
        //LOG_M("rxdata.m","rxs",ru->common.rxdata[0],1228800,1,1);
        LOG_D(PHY,"RU proc: frame_rx = %d, tti_rx = %d\n", proc->frame_rx, proc->tti_rx);
        gNBscopeCopy(RC.gNB[0],
                     gNBRxdataF,
                     ru->common.rxdataF[0],
                     sizeof(c16_t),
                     1,
                     gNB->frame_parms.samples_per_slot_wCP,
                     proc->tti_rx * gNB->frame_parms.samples_per_slot_wCP);

        // Do PRACH RU processing
        int prach_id = find_nr_prach_ru(ru, proc->frame_rx, proc->tti_rx, SEARCH_EXIST);
        if (prach_id >= 0) {
          VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME( VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_RU_PRACH_RX, 1 );

          T(T_GNB_PHY_PRACH_INPUT_SIGNAL,
            T_INT(proc->frame_rx),
            T_INT(proc->tti_rx),
            T_INT(0),
            T_BUFFER(&ru->common.rxdata[0][fp->get_samples_slot_timestamp(proc->tti_rx - 1, fp, 0) /*-ru->N_TA_offset*/],
                     (fp->get_samples_per_slot(proc->tti_rx - 1, fp) + fp->get_samples_per_slot(proc->tti_rx, fp)) * 4));
          RU_PRACH_list_t *p = ru->prach_list + prach_id;
          int N_dur = get_nr_prach_duration(p->fmt);

          for (int prach_oc = 0; prach_oc < p->num_prach_ocas; prach_oc++) {
            int prachStartSymbol = p->prachStartSymbol + prach_oc * N_dur;
            int beam_id = ru->prach_list[prach_id].beam ? ru->prach_list[prach_id].beam[prach_oc] : 0;
            //comment FK: the standard 38.211 section 5.3.2 has one extra term +14*N_RA_slot. This is because there prachStartSymbol is given wrt to start of the 15kHz slot or 60kHz slot. Here we work slot based, so this function is anyway only called in slots where there is PRACH. Its up to the MAC to schedule another PRACH PDU in the case there are there N_RA_slot \in {0,1}.
            rx_nr_prach_ru(ru,
                           p->fmt, // could also use format
                           p->numRA,
                           beam_id,
                           prachStartSymbol,
                           p->slot,
                           prach_oc,
                           proc->frame_rx,
                           proc->tti_rx);
          }
          free_nr_ru_prach_entry(ru,prach_id);
          VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_RU_PRACH_RX, 0);
        } // end if (prach_id >= 0)
      } // end if (ru->feprx)
    } // end if (slot_type == NR_UPLINK_SLOT || slot_type == NR_MIXED_SLOT) {

    notifiedFIFO_elt_t *resTx = newNotifiedFIFO_elt(sizeof(processingData_L1tx_t), 0, &gNB->L1_tx_out, NULL);
    resTx->key = proc->tti_tx;
    processingData_L1tx_t *syncMsgTx = NotifiedFifoData(resTx);
    *syncMsgTx = (processingData_L1tx_t){.gNB = gNB,
                                         .frame = proc->frame_tx,
                                         .slot = proc->tti_tx,
                                         .frame_rx = proc->frame_rx,
                                         .slot_rx = proc->tti_rx,
                                         .timestamp_tx = proc->timestamp_tx};
    pushNotifiedFIFO(&gNB->L1_tx_out, resTx);
  }

  ru_thread_status = 0;
  return &ru_thread_status;
}

int start_streaming(RU_t *ru) {
  LOG_I(PHY,"Starting streaming on third-party RRU\n");
  return ru->ifdevice.thirdparty_startstreaming(&ru->ifdevice);
}

int nr_start_if(struct RU_t_s *ru, struct PHY_VARS_gNB_s *gNB) {
  if (ru->if_south <= REMOTE_IF5)
    for (int i = 0; i < ru->nb_rx; i++)
      ru->openair0_cfg.rxbase[i] = ru->common.rxdata[i];
  ru->openair0_cfg.rxsize = ru->nr_frame_parms->samples_per_subframe*10;
  reset_meas(&ru->ifdevice.tx_fhaul);
  return ru->ifdevice.trx_start_func(&ru->ifdevice);
}

int start_rf(RU_t *ru) {
  return(ru->rfdevice.trx_start_func(&ru->rfdevice));
}

int stop_rf(RU_t *ru) {
  if (ru->rfdevice.trx_get_stats_func) {
    ru->rfdevice.trx_get_stats_func(&ru->rfdevice);
  }
  ru->rfdevice.trx_end_func(&ru->rfdevice);
  return 0;
}

int start_write_thread(RU_t *ru) {
  return ru->rfdevice.trx_write_init(&ru->rfdevice);
}

void init_RU_proc(RU_t *ru)
{
  ru->proc = (RU_proc_t){.ru = ru, .first_rx = 1, .first_tx = 1};
  LOG_I(PHY, "Initialized RU proc %d (%s,%s),\n", ru->idx, NB_functions[ru->function], NB_timing[ru->if_timing]);
}

void start_RU_proc(RU_t *ru)
{
  threadCreate(&ru->proc.pthread_FH, ru_thread, (void *)ru, "ru_thread", ru->ru_thread_core, OAI_PRIORITY_RT_MAX);
}


void kill_NR_RU_proc(int inst) {
  RU_t *ru = RC.ru[inst];
  RU_proc_t *proc = &ru->proc;

  if (ru->if_south != REMOTE_IF4p5) {
    abortTpool(ru->threadPool);
    abortNotifiedFIFO(ru->respfeprx);
    abortNotifiedFIFO(ru->respfeptx);
  }

  /* Note: it seems pthread_FH and and FEP thread below both use
   * mutex_fep/cond_fep. Thus, we unlocked above for pthread_FH above and do
   * the same for FEP thread below again (using broadcast() to ensure both
   * threads get the signal). This one will also destroy the mutex and cond. */
  pthread_mutex_lock(proc->mutex_fep);
  proc->instance_cnt_fep[0] = 0;
  pthread_cond_broadcast(proc->cond_fep);
  pthread_mutex_unlock(proc->mutex_fep);
  pthread_join(proc->pthread_FH, NULL);

  // everything should be stopped now, we can safely stop the RF device
  if (ru->stop_rf == NULL) {
    LOG_W(PHY, "No stop_rf() for RU %d defined, cannot stop RF!\n", ru->idx);
    return;
  }
  int rc = ru->stop_rf(ru);
  if (rc != 0) {
    LOG_W(PHY, "stop_rf() returned %d, RU %d RF device did not stop properly!\n", rc, ru->idx);
    return;
  }
  LOG_I(PHY, "RU %d RF device stopped\n",ru->idx);
}

int check_capabilities(RU_t *ru,RRU_capabilities_t *cap) {
  FH_fmt_options_t fmt = cap->FH_fmt;
  int i;
  LOG_I(PHY,"RRU %d, num_bands %d, looking for band %d\n",ru->idx,cap->num_bands,ru->nr_frame_parms->nr_band);

  for (i=0; i<cap->num_bands; i++) {
    LOG_I(PHY,"band %d on RRU %d\n",cap->band_list[i],ru->idx);
    if (ru->nr_frame_parms->nr_band == cap->band_list[i])
      break;
  }

  if (i == cap->num_bands) {
    LOG_I(PHY,"Couldn't find target NR band %d on RRU %d\n",ru->nr_frame_parms->nr_band,ru->idx);
    return(-1);
  }

  switch (ru->if_south) {
    case LOCAL_RF:
      AssertFatal(1==0, "This RU should not have a local RF, exiting\n");
      return(0);
      break;

    case REMOTE_IF5:
      if (fmt == OAI_IF5_only || fmt == OAI_IF5_and_IF4p5)
        return (0);
      break;

    case REMOTE_IF4p5:
      if (fmt == OAI_IF4p5_only || fmt == OAI_IF5_and_IF4p5)
        return (0);
      break;

    case REMOTE_MBP_IF5:
      if (fmt == MBP_IF5)
        return (0);
      break;

    default:
      LOG_I(PHY,"No compatible Fronthaul interface found for RRU %d\n", ru->idx);
      return(-1);
  }

  return(-1);
}

const char rru_format_options[4][20] = {"OAI_IF5_only", "OAI_IF4p5_only", "OAI_IF5_and_IF4p5", "MBP_IF5"};
const char rru_formats[3][20] = {"OAI_IF5", "MBP_IF5", "OAI_IF4p5"};
const char ru_if_formats[4][20] = {"LOCAL_RF", "REMOTE_OAI_IF5", "REMOTE_MBP_IF5", "REMOTE_OAI_IF4p5"};

void configure_ru(void *ruu, void *arg)
{
  RU_t *ru = (RU_t *)ruu;
  nfapi_nr_config_request_scf_t *cfg = &ru->config;
  int ret;
  LOG_I(PHY, "Received capabilities from RRU %d\n", ru->idx);

  RRU_capabilities_t *capabilities = (RRU_capabilities_t *)arg;
  if (capabilities->FH_fmt < MAX_FH_FMTs)
    LOG_I(PHY, "RU FH options %s\n", rru_format_options[capabilities->FH_fmt]);

  ret = check_capabilities(ru,capabilities);
  AssertFatal(ret == 0, "Cannot configure RRU %d, check_capabilities returned %d\n", ru->idx, ret);
  // take antenna capabilities of RRU
  ru->nb_tx = capabilities->nb_tx[0];
  ru->nb_rx = capabilities->nb_rx[0];
  // Pass configuration to RRU
  LOG_I(PHY, "Using %s fronthaul (%d), band %d \n",ru_if_formats[ru->if_south],ru->if_south,ru->nr_frame_parms->nr_band);

  // wait for configuration
  RRU_config_t *config = (RRU_config_t *)arg;
  *config = (RRU_config_t){.FH_fmt = ru->if_south,
                           .num_bands = 1,
                           .band_list[0] = ru->nr_frame_parms->nr_band,
                           .tx_freq[0] = ru->nr_frame_parms->dl_CarrierFreq,
                           .rx_freq[0] = ru->nr_frame_parms->ul_CarrierFreq,
                           .att_tx[0] = ru->att_tx,
                           .att_rx[0] = ru->att_rx,
                           .N_RB_DL[0] = cfg->carrier_config.dl_grid_size[cfg->ssb_config.scs_common.value].value,
                           .N_RB_UL[0] = cfg->carrier_config.dl_grid_size[cfg->ssb_config.scs_common.value].value,
                           .threequarter_fs[0] = ru->nr_frame_parms->threequarter_fs};
  nr_init_frame_parms(&ru->config, ru->nr_frame_parms);
  nr_phy_init_RU(ru);
}

void configure_rru(void *ruu, void *arg)
{
  RRU_config_t *config     = (RRU_config_t *)arg;
  RU_t *ru = (RU_t *)ruu;
  nfapi_nr_config_request_scf_t *cfg = &ru->config;
  NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;

  fp->nr_band = config->band_list[0];
  fp->dl_CarrierFreq = config->tx_freq[0];
  fp->ul_CarrierFreq = config->rx_freq[0];

  if (fp->dl_CarrierFreq == fp->ul_CarrierFreq) {
    cfg->cell_config.frame_duplex_type.value = TDD;
  } else
    cfg->cell_config.frame_duplex_type.value = FDD;

  ru->att_tx = config->att_tx[0];
  ru->att_rx = config->att_rx[0];
  int mu = cfg->ssb_config.scs_common.value;
  cfg->carrier_config.dl_grid_size[mu].value = config->N_RB_DL[0];
  cfg->carrier_config.dl_grid_size[mu].value = config->N_RB_UL[0];
  fp->threequarter_fs = config->threequarter_fs[0];

  if (ru->function==NGFI_RRU_IF4p5) {
    fp->att_rx = ru->att_rx;
    fp->att_tx = ru->att_tx;
  }

  fill_rf_config(ru,ru->rf_config_file);
  nr_init_frame_parms(&ru->config, fp);
  nr_phy_init_RU(ru);
}

void set_function_spec_param(RU_t *ru)
{
  switch (ru->if_south) {
    case LOCAL_RF:   // this is an RU with integrated RF (RRU, gNB)
      reset_meas(&ru->rx_fhaul);
      if (ru->function ==  NGFI_RRU_IF5) {                 // IF5 RRU
        ru->do_prach              = 0;                      // no prach processing in RU
        ru->fh_north_in           = NULL;                   // no shynchronous incoming fronthaul from north
        ru->fh_north_out          = fh_if5_north_out;       // need only to do send_IF5  reception
        ru->fh_south_out          = tx_rf;                  // send output to RF
        ru->fh_north_asynch_in    = fh_if5_north_asynch_in; // TX packets come asynchronously
        ru->feprx                 = NULL;                   // nothing (this is a time-domain signal)
        ru->feptx_ofdm            = NULL;                   // nothing (this is a time-domain signal)
        ru->feptx_prec            = NULL;                   // nothing (this is a time-domain signal)
        ru->nr_start_if           = nr_start_if;            // need to start the if interface for if5
        ru->ifdevice.host_type    = RRU_HOST;
        ru->rfdevice.host_type    = RRU_HOST;
        ru->ifdevice.eth_params   = &ru->eth_params;
        reset_meas(&ru->rx_fhaul);
        reset_meas(&ru->tx_fhaul);
        reset_meas(&ru->compression);
        reset_meas(&ru->transport);
      } else if (ru->function == NGFI_RRU_IF4p5) {
        ru->do_prach              = 1;                        // do part of prach processing in RU
        ru->fh_north_in           = NULL;                     // no synchronous incoming fronthaul from north
        ru->fh_north_out          = fh_if4p5_north_out;       // send_IF4p5 on reception
        ru->fh_south_out          = tx_rf;                    // send output to RF
        ru->fh_north_asynch_in    = fh_if4p5_north_asynch_in; // TX packets come asynchronously
        ru->feprx                 = nr_fep_tp;     // this is frequency-shift + DFTs
        ru->feptx_ofdm            = nr_feptx_tp; // this is fep with idft only (no precoding in RRU)
        ru->feptx_prec            = NULL;
        ru->nr_start_if           = nr_start_if;              // need to start the if interface for if4p5
        ru->ifdevice.host_type    = RRU_HOST;
        ru->rfdevice.host_type    = RRU_HOST;
        ru->ifdevice.eth_params   = &ru->eth_params;
        reset_meas(&ru->tx_fhaul);
        reset_meas(&ru->compression);
        reset_meas(&ru->transport);
      } else if (ru->function == gNodeB_3GPP) {
        ru->do_prach             = 0;                       // no prach processing in RU
        ru->feprx                = nr_fep_tp;     // this is frequency-shift + DFTs
        ru->feptx_ofdm           = nr_feptx_tp;             // this is fep with idft and precoding
        ru->feptx_prec           = NULL;                    
        ru->fh_north_in          = NULL;                    // no incoming fronthaul from north
        ru->fh_north_out         = NULL;                    // no outgoing fronthaul to north
        ru->nr_start_if          = NULL;                    // no if interface
        ru->rfdevice.host_type   = RAU_HOST;
        ru->fh_south_in            = rx_rf;                 // local synchronous RF RX
        ru->fh_south_out           = tx_rf;                 // local synchronous RF TX
        ru->start_rf               = start_rf;              // need to start the local RF interface
        ru->stop_rf                = stop_rf;
        ru->start_write_thread     = start_write_thread;                  // starting RF TX in different thread
      }
      break;

    case REMOTE_IF5: // the remote unit is IF5 RRU
      ru->do_prach               = 0;
      ru->txfh_in_fep            = 0;
      ru->feprx                  = nr_fep_tp;     // this is frequency-shift + DFTs
      ru->feptx_prec             = NULL;          // need to do transmit Precoding + IDFTs
      ru->feptx_ofdm             = nr_feptx_tp; // need to do transmit Precoding + IDFTs
      ru->fh_south_in            = fh_if5_south_in;     // synchronous IF5 reception
      ru->fh_south_out           = (ru->txfh_in_fep>0) ? NULL : fh_if5_south_out;    // synchronous IF5 transmission
      ru->fh_south_asynch_in     = NULL;                // no asynchronous UL
      ru->start_rf               = ru->eth_params.transp_preference == ETH_UDP_IF5_ECPRI_MODE ? start_streaming : NULL;
      ru->stop_rf                = NULL;
      ru->start_write_thread     = NULL;
      ru->nr_start_if            = nr_start_if;         // need to start if interface for IF5
      ru->ifdevice.host_type     = RAU_HOST;
      ru->ifdevice.eth_params    = &ru->eth_params;
      ru->ifdevice.configure_rru = configure_ru;

      break;

    case REMOTE_IF4p5:
      ru->do_prach               = 0;
      ru->feprx                  = NULL;                // DFTs
      ru->feptx_prec             = nr_feptx_prec;       // Precoding operation
      ru->feptx_ofdm             = NULL;                // no OFDM mod
      ru->fh_south_in            = fh_if4p5_south_in;   // synchronous IF4p5 reception
      ru->fh_south_out           = fh_if4p5_south_out;  // synchronous IF4p5 transmission
      ru->fh_south_asynch_in     = (ru->if_timing == synch_to_other) ? fh_if4p5_south_in : NULL;                // asynchronous UL if synch_to_other
      ru->fh_north_out           = NULL;
      ru->fh_north_asynch_in     = NULL;
      ru->start_rf               = NULL;                // no local RF
      ru->stop_rf                = NULL;
      ru->start_write_thread     = NULL;
      ru->nr_start_if            = nr_start_if;         // need to start if interface for IF4p5
      ru->ifdevice.host_type     = RAU_HOST;
      ru->ifdevice.eth_params    = &ru->eth_params;
      ru->ifdevice.configure_rru = configure_ru;
      break;

    default:
      LOG_E(PHY,"RU with invalid or unknown southbound interface type %d\n",ru->if_south);
      break;
  } // switch on interface type
}

void init_NR_RU(configmodule_interface_t *cfg, char *rf_config_file)
{
  // create status mask
  RC.ru_mask = 0;
  pthread_mutex_init(&RC.ru_mutex,NULL);
  pthread_cond_init(&RC.ru_cond,NULL);
  // read in configuration file)
  NRRCconfig_RU(cfg);
  LOG_I(PHY,"number of L1 instances %d, number of RU %d, number of CPU cores %d\n",RC.nb_nr_L1_inst,RC.nb_RU,get_nprocs());
  LOG_D(PHY,"Process RUs RC.nb_RU:%d\n",RC.nb_RU);

  for (int ru_id = 0; ru_id < RC.nb_RU; ru_id++) {
    LOG_D(PHY,"Process RC.ru[%d]\n",ru_id);
    RU_t *ru = RC.ru[ru_id];
    ru->rf_config_file = rf_config_file;
    ru->idx            = ru_id;
    ru->ts_offset      = 0;
    // use gNB_list[0] as a reference for RU frame parameters
    // NOTE: multiple CC_id are not handled here yet!

    if (ru->num_gNB > 0) {
      LOG_D(PHY, "%s() RC.ru[%d].num_gNB:%d ru->gNB_list[0]:%p RC.gNB[0]:%p rf_config_file:%s\n", __FUNCTION__, ru_id, ru->num_gNB, ru->gNB_list[0], RC.gNB[0], ru->rf_config_file);

      if (ru->gNB_list[0] == 0) {
        LOG_E(PHY,"%s() DJP - ru->gNB_list ru->num_gNB are not initialized - so do it manually\n", __FUNCTION__);
        ru->gNB_list[0] = RC.gNB[0];
        ru->num_gNB=1;
      }
    }

    PHY_VARS_gNB *gNB_RC = RC.gNB[0];
    PHY_VARS_gNB *gNB0 = ru->gNB_list[0];
    LOG_D(PHY, "RU FUnction:%d ru->if_south:%d\n", ru->function, ru->if_south);

    if (gNB0) {
      if (gNB_RC) {
        LOG_D(PHY, "Copying frame parms from gNB in RC to gNB %d in ru %d and frame_parms in ru\n", gNB0->Mod_id, ru->idx);
        *ru->nr_frame_parms = gNB_RC->frame_parms;
        gNB0->frame_parms = gNB_RC->frame_parms;
        // attach all RU to all gNBs in its list/
        LOG_D(PHY,"ru->num_gNB:%d gNB0->num_RU:%d\n", ru->num_gNB, gNB0->num_RU);
        for (int i = 0; i < ru->num_gNB; i++) {
          gNB0 = ru->gNB_list[i];
          gNB0->RU_list[gNB0->num_RU++] = ru;
        }
      }
    }
    set_function_spec_param(ru);
    init_RU_proc(ru);
    if (ru->if_south != REMOTE_IF4p5) {
      int threadCnt = ru->num_tpcores;
      if (threadCnt < 2)
        LOG_E(PHY, "Number of threads for gNB should be more than 1. Allocated only %d\n", threadCnt);
      char pool[80];
      int s_offset = sprintf(pool,"%d",ru->tpcores[0]);
      for (int icpu = 1; icpu < threadCnt; icpu++) {
        s_offset += sprintf(pool + s_offset, ",%d", ru->tpcores[icpu]);
      }
      LOG_I(PHY, "RU thread-pool core string %s (size %d)\n", pool, threadCnt);
      ru->threadPool = malloc(sizeof(tpool_t));
      initTpool(pool, ru->threadPool, cpumeas(CPUMEAS_GETSTATE));
      // FEP RX result FIFO
      ru->respfeprx = malloc(sizeof(notifiedFIFO_t));
      initNotifiedFIFO(ru->respfeprx);
      // FEP TX result FIFO
      ru->respfeptx = malloc(sizeof(notifiedFIFO_t));
      initNotifiedFIFO(ru->respfeptx);
    }
  } // for ru_id

  LOG_D(HW,"[nr-softmodem.c] RU threads created\n");
}

void start_NR_RU()
{
  RU_t *ru = RC.ru[0];
  start_RU_proc(ru);
}

void stop_RU(int nb_ru) {
  for (int inst = 0; inst < nb_ru; inst++) {
    LOG_I(PHY, "Stopping RU %d processing threads\n", inst);
    kill_NR_RU_proc(inst);
  }
}

/* --------------------------------------------------------*/
/* from here function to use configuration module          */
static void NRRCconfig_RU(configmodule_interface_t *cfg)
{
  paramdef_t RUParams[] = RUPARAMS_DESC;
  paramlist_def_t RUParamList = {CONFIG_STRING_RU_LIST, NULL, 0};
  config_getlist(cfg, &RUParamList, RUParams, sizeofArray(RUParams), NULL);

  if (RUParamList.numelt <= 0)
    return;

  RC.ru = (RU_t **)malloc(RC.nb_RU * sizeof(RU_t *));
  RC.ru_mask = (1 << RC.nb_RU) - 1;

  for (int j = 0; j < RC.nb_RU; j++) {
    RU_t *ru = RC.ru[j] = calloc(1, sizeof(*RC.ru[j]));
    ru->idx = j;
    ru->nr_frame_parms = calloc(1, sizeof(*ru->nr_frame_parms));
    ru->frame_parms = calloc(1, sizeof(*ru->frame_parms));
    ru->if_timing = synch_to_ext_device;
    paramdef_t *param = RUParamList.paramarray[j];
    if (RC.nb_nr_L1_inst > 0)
      ru->num_gNB = param[RU_ENB_LIST_IDX].numelt;
    else
      ru->num_gNB = 0;

    for (int i = 0; i < ru->num_gNB; i++)
      ru->gNB_list[i] = RC.gNB[param[RU_ENB_LIST_IDX].iptr[i]];

    if (config_isparamset(param, RU_SDR_ADDRS)) {
      ru->openair0_cfg.sdr_addrs = strdup(*param[RU_SDR_ADDRS].strptr);
    }

    if (config_isparamset(param, RU_GPIO_CONTROL)) {
      char *str = *param[RU_GPIO_CONTROL].strptr;
      if (strcmp(str, "generic") == 0) {
        ru->openair0_cfg.gpio_controller = RU_GPIO_CONTROL_GENERIC;
        LOG_I(PHY, "RU GPIO control set as 'generic'\n");
      } else if (strcmp(str, "interdigital") == 0) {
        ru->openair0_cfg.gpio_controller = RU_GPIO_CONTROL_INTERDIGITAL;
        LOG_I(PHY, "RU GPIO control set as 'interdigital'\n");
      } else {
        AssertFatal(false, "bad GPIO controller in configuration file: '%s'\n", str);
      }
    } else
      ru->openair0_cfg.gpio_controller = RU_GPIO_CONTROL_NONE;

    if (config_isparamset(param, RU_TX_SUBDEV)) {
      ru->openair0_cfg.tx_subdev = strdup(*param[RU_TX_SUBDEV].strptr);
      LOG_I(PHY, "RU USRP tx subdev == %s\n", ru->openair0_cfg.tx_subdev);
    }

    if (config_isparamset(param, RU_RX_SUBDEV)) {
      ru->openair0_cfg.rx_subdev = strdup(*param[RU_RX_SUBDEV].strptr);
      LOG_I(PHY, "RU USRP rx subdev == %s\n", ru->openair0_cfg.rx_subdev);
    }

    if (config_isparamset(param, RU_SDR_CLK_SRC)) {
      char *str = *param[RU_SDR_CLK_SRC].strptr;
      if (strcmp(str, "internal") == 0) {
        ru->openair0_cfg.clock_source = internal;
        LOG_I(PHY, "RU clock source set as internal\n");
      } else if (strcmp(str, "external") == 0) {
        ru->openair0_cfg.clock_source = external;
        LOG_I(PHY, "RU clock source set as external\n");
      } else if (strcmp(str, "gpsdo") == 0) {
        ru->openair0_cfg.clock_source = gpsdo;
        LOG_I(PHY, "RU clock source set as gpsdo\n");
      } else {
        LOG_E(PHY, "Erroneous RU clock source in the provided configuration file: '%s'\n", str);
      }
    } else {
      LOG_D(PHY, "Setting clock source to internal\n");
      ru->openair0_cfg.clock_source = internal;
    }

    if (config_isparamset(param, RU_SDR_TME_SRC)) {
      char *str = *param[RU_SDR_TME_SRC].strptr;
      if (strcmp(str, "internal") == 0) {
        ru->openair0_cfg.time_source = internal;
        LOG_I(PHY, "RU time source set as internal\n");
      } else if (strcmp(str, "external") == 0) {
        ru->openair0_cfg.time_source = external;
        LOG_I(PHY, "RU time source set as external\n");
      } else if (strcmp(str, "gpsdo") == 0) {
        ru->openair0_cfg.time_source = gpsdo;
        LOG_I(PHY, "RU time source set as gpsdo\n");
      } else {
        LOG_E(PHY, "Erroneous RU time source in the provided configuration file: '%s'\n", str);
      }
    } else {
      LOG_D(PHY, "Setting time source to internal\n");
      ru->openair0_cfg.time_source = internal;
    }

    ru->openair0_cfg.tune_offset = get_softmodem_params()->tune_offset;

    if (strcmp(*param[RU_LOCAL_RF_IDX].strptr, "yes") == 0) {
      if (!config_isparamset(param, RU_LOCAL_IF_NAME_IDX)) {
        ru->if_south = LOCAL_RF;
        ru->function = gNodeB_3GPP;
        LOG_D(PHY, "Setting function for RU %d to gNodeB_3GPP\n", j);
      } else {
        ru->eth_params.local_if_name = strdup(*param[RU_LOCAL_IF_NAME_IDX].strptr);
        ru->eth_params.my_addr = strdup(*param[RU_LOCAL_ADDRESS_IDX].strptr);
        ru->eth_params.remote_addr = strdup(*param[RU_REMOTE_ADDRESS_IDX].strptr);
        ru->eth_params.my_portc = *param[RU_LOCAL_PORTC_IDX].uptr;
        ru->eth_params.remote_portc = *param[RU_REMOTE_PORTC_IDX].uptr;
        ru->eth_params.my_portd = *param[RU_LOCAL_PORTD_IDX].uptr;
        ru->eth_params.remote_portd = *param[RU_REMOTE_PORTD_IDX].uptr;
        char *str = *param[RU_TRANSPORT_PREFERENCE_IDX].strptr;
        if (strcmp(str, "udp") == 0) {
          ru->if_south = LOCAL_RF;
          ru->function = NGFI_RRU_IF5;
          ru->eth_params.transp_preference = ETH_UDP_MODE;
          LOG_D(PHY, "Setting function for RU %d to NGFI_RRU_IF5 (udp)\n", j);
        } else if (strcmp(str, "raw") == 0) {
          ru->if_south = LOCAL_RF;
          ru->function = NGFI_RRU_IF5;
          ru->eth_params.transp_preference = ETH_RAW_MODE;
          LOG_D(PHY, "Setting function for RU %d to NGFI_RRU_IF5 (raw)\n", j);
        } else if (strcmp(str, "udp_if4p5") == 0) {
          ru->if_south = LOCAL_RF;
          ru->function = NGFI_RRU_IF4p5;
          ru->eth_params.transp_preference = ETH_UDP_IF4p5_MODE;
          LOG_D(PHY, "Setting function for RU %d to NGFI_RRU_IF4p5 (udp)\n", j);
        } else if (strcmp(str, "raw_if4p5") == 0) {
          ru->if_south = LOCAL_RF;
          ru->function = NGFI_RRU_IF4p5;
          ru->eth_params.transp_preference = ETH_RAW_IF4p5_MODE;
          LOG_D(PHY, "Setting function for RU %d to NGFI_RRU_IF4p5 (raw)\n", j);
        }
      }

      ru->max_pdschReferenceSignalPower = *param[RU_MAX_RS_EPRE_IDX].uptr;
      ru->max_rxgain = *param[RU_MAX_RXGAIN_IDX].uptr;
      ru->sf_extension = *param[RU_SF_EXTENSION_IDX].uptr;
    } // strcmp(local_rf, "yes") == 0
    else {
      char *str = *param[RU_TRANSPORT_PREFERENCE_IDX].strptr;
      LOG_D(PHY, "RU %d: Transport %s\n", j, str);
      ru->eth_params.local_if_name = strdup(*param[RU_LOCAL_IF_NAME_IDX].strptr);
      ru->eth_params.my_addr = strdup(*param[RU_LOCAL_ADDRESS_IDX].strptr);
      ru->eth_params.remote_addr = strdup(*param[RU_REMOTE_ADDRESS_IDX].strptr);
      ru->eth_params.my_portc = *param[RU_LOCAL_PORTC_IDX].uptr;
      ru->eth_params.remote_portc = *param[RU_REMOTE_PORTC_IDX].uptr;
      ru->eth_params.my_portd = *param[RU_LOCAL_PORTD_IDX].uptr;
      ru->eth_params.remote_portd = *param[RU_REMOTE_PORTD_IDX].uptr;

      if (strcmp(str, "udp") == 0) {
        ru->if_south = REMOTE_IF5;
        ru->function = NGFI_RAU_IF5;
        ru->eth_params.transp_preference = ETH_UDP_MODE;
      } else if (strcmp(str, "udp_ecpri_if5") == 0) {
        ru->if_south = REMOTE_IF5;
        ru->function = NGFI_RAU_IF5;
        ru->eth_params.transp_preference = ETH_UDP_IF5_ECPRI_MODE;
      } else if (strcmp(str, "raw") == 0) {
        ru->if_south = REMOTE_IF5;
        ru->function = NGFI_RAU_IF5;
        ru->eth_params.transp_preference = ETH_RAW_MODE;
      } else if (strcmp(str, "udp_if4p5") == 0) {
        ru->if_south = REMOTE_IF4p5;
        ru->function = NGFI_RAU_IF4p5;
        ru->eth_params.transp_preference = ETH_UDP_IF4p5_MODE;
      } else if (strcmp(str, "raw_if4p5") == 0) {
        ru->if_south = REMOTE_IF4p5;
        ru->function = NGFI_RAU_IF4p5;
        ru->eth_params.transp_preference = ETH_RAW_IF4p5_MODE;
      }
    } /* strcmp(local_rf, "yes") != 0 */

    ru->nb_tx = *param[RU_NB_TX_IDX].uptr;
    ru->nb_rx = *param[RU_NB_RX_IDX].uptr;
    ru->att_tx = *param[RU_ATT_TX_IDX].uptr;
    ru->att_rx = *param[RU_ATT_RX_IDX].uptr;
    ru->if_frequency = *param[RU_IF_FREQUENCY].u64ptr;
    ru->if_freq_offset = *param[RU_IF_FREQ_OFFSET].iptr;
    ru->sl_ahead = *param[RU_SL_AHEAD].iptr;
    ru->num_bands = param[RU_BAND_LIST_IDX].numelt;
    for (int i = 0; i < ru->num_bands; i++)
      ru->band[i] = param[RU_BAND_LIST_IDX].iptr[i];
    ru->openair0_cfg.nr_flag = *param[RU_NR_FLAG].iptr;
    ru->openair0_cfg.nr_band = ru->band[0];
    ru->openair0_cfg.nr_scs_for_raster = *param[RU_NR_SCS_FOR_RASTER].iptr;
    LOG_D(PHY,
          "[RU %d] Setting nr_flag %d, nr_band %d, nr_scs_for_raster %d\n",
          j,
          ru->openair0_cfg.nr_flag,
          ru->openair0_cfg.nr_band,
          ru->openair0_cfg.nr_scs_for_raster);
    ru->openair0_cfg.rxfh_cores[0] = *param[RU_RXFH_CORE_ID].iptr;
    ru->openair0_cfg.txfh_cores[0] = *param[RU_TXFH_CORE_ID].iptr;
    ru->num_tpcores = *param[RU_NUM_TP_CORES].iptr;
    ru->half_slot_parallelization = *param[RU_HALF_SLOT_PARALLELIZATION].iptr;
    ru->ru_thread_core = *param[RU_RU_THREAD_CORE].iptr;
    LOG_D(PHY, "[RU %d] Setting half-slot parallelization to %d\n", j, ru->half_slot_parallelization);
    AssertFatal(ru->num_tpcores <= param[RU_TP_CORES].numelt, "Number of TP cores should be <=16\n");
    for (int i = 0; i < ru->num_tpcores; i++)
      ru->tpcores[i] = param[RU_TP_CORES].iptr[i];
  } // j=0..num_rus
  return;
}

