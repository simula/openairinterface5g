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


#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <sched.h>
#include <stdbool.h>
#include <signal.h>

#include "T.h"
#include "common/oai_version.h"
#include "assertions.h"
#include "PHY/types.h"
#include "PHY/defs_nr_UE.h"
#include "SCHED_NR_UE/defs.h"
#include "common/ran_context.h"
#include "common/config/config_userapi.h"
//#include "common/utils/threadPool/thread-pool.h"
#include "common/utils/load_module_shlib.h"
//#undef FRAME_LENGTH_COMPLEX_SAMPLES //there are two conflicting definitions, so we better make sure we don't use it at all
#include "common/utils/nr/nr_common.h"

#include "radio/COMMON/common_lib.h"
#include "radio/ETHERNET/if_defs.h"

//#undef FRAME_LENGTH_COMPLEX_SAMPLES //there are two conflicting definitions, so we better make sure we don't use it at all
#include "openair1/PHY/MODULATION/nr_modulation.h"
#include "PHY/phy_vars_nr_ue.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "PHY/NR_TRANSPORT/nr_dlsch.h"
//#include "../../SIMU/USER/init_lte.h"

#include "RRC/LTE/rrc_vars.h"
#include "PHY_INTERFACE/phy_interface_vars.h"
#include "NR_IF_Module.h"
#include "openair1/SIMULATION/TOOLS/sim.h"

#ifdef SMBV
#include "PHY/TOOLS/smbv.h"
unsigned short config_frames[4] = {2,9,11,13};
#endif
#include "common/utils/LOG/log.h"
#include "common/utils/LOG/vcd_signal_dumper.h"

#include "UTIL/OPT/opt.h"
#include "enb_config.h"
#include "LAYER2/nr_pdcp/nr_pdcp_oai_api.h"

#include "intertask_interface.h"

#include "PHY/INIT/nr_phy_init.h"
#include "system.h"
#include <openair2/RRC/NR_UE/rrc_proto.h>
#include <openair2/LAYER2/NR_MAC_UE/mac_defs.h>
#include <openair2/LAYER2/NR_MAC_UE/mac_proto.h>
#include <openair2/NR_UE_PHY_INTERFACE/NR_IF_Module.h>
#include <openair1/SCHED_NR_UE/fapi_nr_ue_l1.h>

/* Callbacks, globals and object handlers */

//#include "stats.h"
// current status is that every UE has a DL scope for a SINGLE eNB (eNB_id=0)
#include "PHY/TOOLS/phy_scope_interface.h"
#include "PHY/TOOLS/nr_phy_scope.h"
#include <executables/nr-uesoftmodem.h>
#include "executables/softmodem-common.h"
#include "executables/thread-common.h"

#include "nr_nas_msg_sim.h"
#include <openair1/PHY/MODULATION/nr_modulation.h>
#include "openair2/GNB_APP/gnb_paramdef.h"

extern const char *duplex_mode[];
THREAD_STRUCT thread_struct;
nrUE_params_t nrUE_params = {0};

// Thread variables
pthread_cond_t nfapi_sync_cond;
pthread_mutex_t nfapi_sync_mutex;
int nfapi_sync_var=-1; //!< protected by mutex \ref nfapi_sync_mutex
uint16_t sf_ahead=6; //??? value ???
pthread_cond_t sync_cond;
pthread_mutex_t sync_mutex;
int sync_var=-1; //!< protected by mutex \ref sync_mutex.
int config_sync_var=-1;

// not used in UE
instance_t CUuniqInstance=0;
instance_t DUuniqInstance=0;

int get_node_type() {return -1;}

RAN_CONTEXT_t RC;
int oai_exit = 0;


static int      tx_max_power[MAX_NUM_CCs] = {0};

double          rx_gain_off = 0.0;

uint64_t        downlink_frequency[MAX_NUM_CCs][4];
int32_t         uplink_frequency_offset[MAX_NUM_CCs][4];
uint64_t        sidelink_frequency[MAX_NUM_CCs][4];

// UE and OAI config variables

openair0_config_t openair0_cfg[MAX_CARDS];
int16_t           node_synch_ref[MAX_NUM_CCs];
int               otg_enabled;
double            cpuf;
uint32_t       N_RB_DL    = 106;

// NTN cellSpecificKoffset-r17, but in slots for DL SCS
unsigned int NTN_UE_Koffset = 0;

int create_tasks_nrue(uint32_t ue_nb) {
  LOG_D(NR_RRC, "%s(ue_nb:%d)\n", __FUNCTION__, ue_nb);
  itti_wait_ready(1);

  if (ue_nb > 0) {
    LOG_I(NR_RRC,"create TASK_RRC_NRUE \n");
    const ittiTask_parms_t parmsRRC = {NULL, rrc_nrue};
    if (itti_create_task(TASK_RRC_NRUE, rrc_nrue_task, &parmsRRC) < 0) {
      LOG_E(NR_RRC, "Create task for RRC UE failed\n");
      return -1;
    }
    if (get_softmodem_params()->nsa) {
      init_connections_with_lte_ue();
      if (itti_create_task (TASK_RRC_NSA_NRUE, recv_msgs_from_lte_ue, NULL) < 0) {
        LOG_E(NR_RRC, "Create task for RRC NSA nr-UE failed\n");
        return -1;
      }
    }
    const ittiTask_parms_t parmsNAS = {NULL, nas_nrue};
    if (itti_create_task(TASK_NAS_NRUE, nas_nrue_task, &parmsNAS) < 0) {
      LOG_E(NR_RRC, "Create task for NAS UE failed\n");
      return -1;
    }
  }

  itti_wait_ready(0);

  return 0;
}

void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  int CC_id;

  if (s != NULL) {
    printf("%s:%d %s() Exiting OAI softmodem: %s\n",file,line, function, s);
  }

  oai_exit = 1;

  if (PHY_vars_UE_g && PHY_vars_UE_g[0]) {
    for(CC_id=0; CC_id<MAX_NUM_CCs; CC_id++) {
      if (PHY_vars_UE_g[0][CC_id] && PHY_vars_UE_g[0][CC_id]->rfdevice.trx_end_func)
        PHY_vars_UE_g[0][CC_id]->rfdevice.trx_end_func(&PHY_vars_UE_g[0][CC_id]->rfdevice);
    }
  }

  if (assert) {
    abort();
  } else {
    sleep(1); // allow lte-softmodem threads to exit first
    exit(EXIT_SUCCESS);
  }
}

uint64_t get_nrUE_optmask(void) {
  return nrUE_params.optmask;
}

uint64_t set_nrUE_optmask(uint64_t bitmask) {
  nrUE_params.optmask = nrUE_params.optmask | bitmask;
  return nrUE_params.optmask;
}

nrUE_params_t *get_nrUE_params(void) {
  return &nrUE_params;
}
static void get_options(configmodule_interface_t *cfg)
{
  paramdef_t cmdline_params[] =CMDLINE_NRUEPARAMS_DESC ;
  int numparams = sizeofArray(cmdline_params);
  config_get(cfg, cmdline_params, numparams, NULL);
  if (nrUE_params.vcdflag > 0)
    ouput_vcd = 1;
}

// set PHY vars from command line
void set_options(int CC_id, PHY_VARS_NR_UE *UE){
  NR_DL_FRAME_PARMS *fp = &UE->frame_parms;

  // Set UE variables
  UE->rx_total_gain_dB     = (int)nrUE_params.rx_gain + rx_gain_off;
  UE->tx_total_gain_dB     = (int)nrUE_params.tx_gain;
  UE->tx_power_max_dBm     = nrUE_params.tx_max_power;
  UE->rf_map.card          = 0;
  UE->rf_map.chain         = CC_id + 0;
  UE->max_ldpc_iterations  = nrUE_params.max_ldpc_iterations;
  UE->ldpc_offload_enable  = nrUE_params.ldpc_offload_flag;
  UE->UE_scan_carrier      = nrUE_params.UE_scan_carrier;
  UE->UE_fo_compensation   = nrUE_params.UE_fo_compensation;
  UE->if_freq              = nrUE_params.if_freq;
  UE->if_freq_off          = nrUE_params.if_freq_off;
  UE->chest_freq           = nrUE_params.chest_freq;
  UE->chest_time           = nrUE_params.chest_time;
  UE->no_timing_correction = nrUE_params.no_timing_correction;

  LOG_I(PHY,"Set UE_fo_compensation %d, UE_scan_carrier %d, UE_no_timing_correction %d \n, chest-freq %d, chest-time %d\n",
        UE->UE_fo_compensation, UE->UE_scan_carrier, UE->no_timing_correction, UE->chest_freq, UE->chest_time);

  // Set FP variables

  fp->nb_antennas_rx       = nrUE_params.nb_antennas_rx;
  fp->nb_antennas_tx       = nrUE_params.nb_antennas_tx;
  fp->threequarter_fs = get_softmodem_params()->threequarter_fs;
  fp->N_RB_DL              = nrUE_params.N_RB_DL;
  fp->ssb_start_subcarrier = nrUE_params.ssb_start_subcarrier;
  fp->ofdm_offset_divisor  = nrUE_params.ofdm_offset_divisor;

  LOG_I(PHY, "Set UE nb_rx_antenna %d, nb_tx_antenna %d, threequarter_fs %d, ssb_start_subcarrier %d\n", fp->nb_antennas_rx, fp->nb_antennas_tx, fp->threequarter_fs, fp->ssb_start_subcarrier);

}

void init_openair0()
{
  int card;
  int freq_off = 0;
  NR_DL_FRAME_PARMS *frame_parms = &PHY_vars_UE_g[0][0]->frame_parms;
  bool is_sidelink = (get_softmodem_params()->sl_mode) ? true : false;
  if (is_sidelink)
    frame_parms = &PHY_vars_UE_g[0][0]->SL_UE_PHY_PARAMS.sl_frame_params;

  for (card=0; card<MAX_CARDS; card++) {
    uint64_t dl_carrier, ul_carrier;
    openair0_cfg[card].configFilename    = NULL;
    openair0_cfg[card].threequarter_fs   = frame_parms->threequarter_fs;
    openair0_cfg[card].sample_rate       = frame_parms->samples_per_subframe * 1e3;
    openair0_cfg[card].samples_per_frame = frame_parms->samples_per_frame;

    if (frame_parms->frame_type==TDD)
      openair0_cfg[card].duplex_mode = duplex_mode_TDD;
    else
      openair0_cfg[card].duplex_mode = duplex_mode_FDD;

    openair0_cfg[card].Mod_id = 0;
    openair0_cfg[card].num_rb_dl = frame_parms->N_RB_DL;
    openair0_cfg[card].clock_source = get_softmodem_params()->clock_source;
    openair0_cfg[card].time_source = get_softmodem_params()->timing_source;
    openair0_cfg[card].tune_offset = get_softmodem_params()->tune_offset;
    openair0_cfg[card].tx_num_channels = min(4, frame_parms->nb_antennas_tx);
    openair0_cfg[card].rx_num_channels = min(4, frame_parms->nb_antennas_rx);

    LOG_I(PHY, "HW: Configuring card %d, sample_rate %f, tx/rx num_channels %d/%d, duplex_mode %s\n",
      card,
      openair0_cfg[card].sample_rate,
      openair0_cfg[card].tx_num_channels,
      openair0_cfg[card].rx_num_channels,
      duplex_mode[openair0_cfg[card].duplex_mode]);

    if (is_sidelink) {
      dl_carrier = frame_parms->dl_CarrierFreq;
      ul_carrier = frame_parms->ul_CarrierFreq;
    } else
      nr_get_carrier_frequencies(PHY_vars_UE_g[0][0], &dl_carrier, &ul_carrier);

    nr_rf_card_config_freq(&openair0_cfg[card], ul_carrier, dl_carrier, freq_off);

    nr_rf_card_config_gain(&openair0_cfg[card], rx_gain_off);

    openair0_cfg[card].configFilename = get_softmodem_params()->rf_config_file;

    if (get_nrUE_params()->usrp_args) openair0_cfg[card].sdr_addrs = get_nrUE_params()->usrp_args;
    if (get_nrUE_params()->tx_subdev) openair0_cfg[card].tx_subdev = get_nrUE_params()->tx_subdev;
    if (get_nrUE_params()->rx_subdev) openair0_cfg[card].rx_subdev = get_nrUE_params()->rx_subdev;

  }
}

static void init_pdcp(int ue_id)
{
  uint32_t pdcp_initmask = (!IS_SOFTMODEM_NOS1) ? LINK_ENB_PDCP_TO_GTPV1U_BIT : LINK_ENB_PDCP_TO_GTPV1U_BIT;

  /*if (IS_SOFTMODEM_RFSIM || (nfapi_getmode()==NFAPI_UE_STUB_PNF)) {
    pdcp_initmask = pdcp_initmask | UE_NAS_USE_TUN_BIT;
  }*/

  // previous code was:
  //   if (IS_SOFTMODEM_NOKRNMOD)
  //     pdcp_initmask = pdcp_initmask | UE_NAS_USE_TUN_BIT;
  // The kernel module (KRNMOD) has been removed from the project, so the 'if'
  // was removed but the flag 'pdcp_initmask' was kept, as "no kernel module"
  // was always set. further refactoring could take it out
  pdcp_initmask = pdcp_initmask | UE_NAS_USE_TUN_BIT;

  if (get_softmodem_params()->nsa && rlc_module_init(0) != 0) {
    LOG_I(RLC, "Problem at RLC initiation \n");
  }
  nr_pdcp_layer_init();
  nr_pdcp_module_init(pdcp_initmask, ue_id);
}

// Stupid function addition because UE itti messages queues definition is common with eNB
void *rrc_enb_process_msg(void *notUsed) {
  return NULL;
}

static bool stop_immediately = false;
static void trigger_stop(int sig)
{
  if (!oai_exit)
    itti_wait_tasks_unblock();
}
static void trigger_deregistration(int sig)
{
  if (!stop_immediately) {
    MessageDef *msg = itti_alloc_new_message(TASK_RRC_UE_SIM, 0, NAS_DEREGISTRATION_REQ);
    NAS_DEREGISTRATION_REQ(msg).cause = AS_DETACH;
    itti_send_msg_to_task(TASK_NAS_NRUE, 0, msg);
    stop_immediately = true;
    static const char m[] = "Press ^C again to trigger immediate shutdown\n";
    __attribute__((unused)) int unused = write(STDOUT_FILENO, m, sizeof(m) - 1);
    signal(SIGALRM, trigger_stop);
    alarm(5);
  } else {
    itti_wait_tasks_unblock();
  }
}

static void get_channel_model_mode(configmodule_interface_t *cfg)
{
  paramdef_t GNBParams[]  = GNBPARAMS_DESC;
  config_get(cfg, GNBParams, sizeofArray(GNBParams), NULL);
  int num_xp_antennas = *GNBParams[GNB_PDSCH_ANTENNAPORTS_XP_IDX].iptr;

  if (num_xp_antennas == 2)
    init_nr_bler_table("NR_MIMO2x2_AWGN_RESULTS_DIR");
  else
    init_nr_bler_table("NR_AWGN_RESULTS_DIR");
}

void start_oai_nrue_threads()
{
    init_queue(&nr_rach_ind_queue);
    init_queue(&nr_rx_ind_queue);
    init_queue(&nr_crc_ind_queue);
    init_queue(&nr_uci_ind_queue);
    init_queue(&nr_sfn_slot_queue);
    init_queue(&nr_chan_param_queue);
    init_queue(&nr_dl_tti_req_queue);
    init_queue(&nr_tx_req_queue);
    init_queue(&nr_ul_dci_req_queue);
    init_queue(&nr_ul_tti_req_queue);

    if (sem_init(&sfn_slot_semaphore, 0, 0) != 0)
    {
      LOG_E(MAC, "sem_init() error\n");
      abort();
    }

    init_nrUE_standalone_thread(ue_id_g);
}

int NB_UE_INST = 1;
configmodule_interface_t *uniqCfg = NULL;

// A global var to reduce the changes size
ldpc_interface_t ldpc_interface = {0}, ldpc_interface_offload = {0};

int main(int argc, char **argv)
{
  start_background_system();

  if ((uniqCfg = load_configmodule(argc, argv, CONFIG_ENABLECMDLINEONLY)) == NULL) {
    exit_fun("[SOFTMODEM] Error, configuration module init failed\n");
  }
  //set_softmodem_sighandler();
  CONFIG_SETRTFLAG(CONFIG_NOEXITONHELP);
  memset(openair0_cfg,0,sizeof(openair0_config_t)*MAX_CARDS);
  memset(tx_max_power,0,sizeof(int)*MAX_NUM_CCs);
  // initialize logging
  logInit();
  // get options and fill parameters from configuration file

  get_options(uniqCfg); // Command-line options specific for NRUE

  get_common_options(uniqCfg, SOFTMODEM_5GUE_BIT);
  CONFIG_CLEARRTFLAG(CONFIG_NOEXITONHELP);
#if T_TRACER
  T_Config_Init();
#endif
  initTpool(get_softmodem_params()->threadPoolConfig, &(nrUE_params.Tpool), cpumeas(CPUMEAS_GETSTATE));
  //randominit (0);
  set_taus_seed (0);

  if (!has_cap_sys_nice())
    LOG_W(UTIL,
          "no SYS_NICE capability: cannot set thread priority and affinity, consider running with sudo for optimum performance\n");

  cpuf=get_cpu_freq_GHz();
  itti_init(TASK_MAX, tasks_info);

  init_opt();

  if (nrUE_params.ldpc_offload_flag)
    load_LDPClib("_t2", &ldpc_interface_offload);

  load_LDPClib(NULL, &ldpc_interface);

  if (ouput_vcd) {
    vcd_signal_dumper_init("/tmp/openair_dump_nrUE.vcd");
  }
  // strdup to put the sring in the core file for post mortem identification
  char *pckg = strdup(OAI_PACKAGE_VERSION);
  LOG_I(HW, "Version: %s\n", pckg);

  PHY_vars_UE_g = malloc(sizeof(*PHY_vars_UE_g) * NB_UE_INST);
  for (int inst = 0; inst < NB_UE_INST; inst++) {
    PHY_vars_UE_g[inst] = malloc(sizeof(*PHY_vars_UE_g[inst]) * MAX_NUM_CCs);
    for (int CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
      PHY_vars_UE_g[inst][CC_id] = malloc(sizeof(*PHY_vars_UE_g[inst][CC_id]));
      memset(PHY_vars_UE_g[inst][CC_id], 0, sizeof(*PHY_vars_UE_g[inst][CC_id]));
    }
  }

  int mode_offset = get_softmodem_params()->nsa ? NUMBER_OF_UE_MAX : 1;
  uint16_t node_number = get_softmodem_params()->node_number;
  ue_id_g = (node_number == 0) ? 0 : node_number - 2;
  AssertFatal(ue_id_g >= 0, "UE id is expected to be nonnegative.\n");

  if (node_number == 0)
    init_pdcp(0);
  else
    init_pdcp(mode_offset + ue_id_g);

  init_NR_UE(NB_UE_INST, get_nrUE_params()->uecap_file, get_nrUE_params()->reconfig_file, get_nrUE_params()->rbconfig_file);

  if (get_softmodem_params()->emulate_l1) {
    RCconfig_nr_ue_macrlc();
    get_channel_model_mode(uniqCfg);
  }

  if (get_softmodem_params()->do_ra)
    AssertFatal(get_softmodem_params()->phy_test == 0,"RA and phy_test are mutually exclusive\n");

  if (get_softmodem_params()->sa)
    AssertFatal(get_softmodem_params()->phy_test == 0,"Standalone mode and phy_test are mutually exclusive\n");

  if (!get_softmodem_params()->nsa && get_softmodem_params()->emulate_l1)
    start_oai_nrue_threads();

  if (!get_softmodem_params()->emulate_l1) {
    for (int inst = 0; inst < NB_UE_INST; inst++) {
      PHY_VARS_NR_UE *UE[MAX_NUM_CCs];
      for (int CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
        UE[CC_id] = PHY_vars_UE_g[inst][CC_id];

        set_options(CC_id, UE[CC_id]);
        NR_UE_MAC_INST_t *mac = get_mac_inst(inst);
        init_nr_ue_phy_cpu_stats(&UE[CC_id]->phy_cpu_stats);

        if (get_softmodem_params()->sa || get_softmodem_params()->sl_mode) { // set frame config to initial values from command line
                                                                            // and assume that the SSB is centered on the grid
          uint16_t nr_band = get_softmodem_params()->band;
          mac->nr_band = nr_band;
          mac->ssb_start_subcarrier = UE[CC_id]->frame_parms.ssb_start_subcarrier;
          nr_init_frame_parms_ue_sa(&UE[CC_id]->frame_parms,
                                    downlink_frequency[CC_id][0],
                                    uplink_frequency_offset[CC_id][0],
                                    get_softmodem_params()->numerology,
                                    nr_band);
        } else {
          fapi_nr_config_request_t *nrUE_config = &UE[CC_id]->nrUE_config;
          nr_init_frame_parms_ue(&UE[CC_id]->frame_parms, nrUE_config, mac->nr_band);
        }

        UE[CC_id]->sl_mode = get_softmodem_params()->sl_mode;
        init_nr_ue_vars(UE[CC_id], inst);

        if (UE[CC_id]->sl_mode) {
          AssertFatal(UE[CC_id]->sl_mode == 2, "Only Sidelink mode 2 supported. Mode 1 not yet supported\n");
          DevAssert(mac->if_module != NULL && mac->if_module->sl_phy_config_request != NULL);
          nr_sl_phy_config_t *phycfg = &mac->SL_MAC_PARAMS->sl_phy_config;
          phycfg->sl_config_req.sl_carrier_config.sl_num_rx_ant = get_nrUE_params()->nb_antennas_rx;
          phycfg->sl_config_req.sl_carrier_config.sl_num_tx_ant = get_nrUE_params()->nb_antennas_tx;
          mac->if_module->sl_phy_config_request(phycfg);
          sl_nr_ue_phy_params_t *sl_phy = &UE[CC_id]->SL_UE_PHY_PARAMS;
          nr_init_frame_parms_ue_sl(&sl_phy->sl_frame_params,
                                    &sl_phy->sl_config,
                                    get_softmodem_params()->threequarter_fs,
                                    get_nrUE_params()->ofdm_offset_divisor);
          sl_ue_phy_init(UE[CC_id]);
        }
      }
    }

    // NTN cellSpecificKoffset-r17, but in slots for DL SCS
    NTN_UE_Koffset = nrUE_params.ntn_koffset << PHY_vars_UE_g[0][0]->frame_parms.numerology_index;

    init_openair0();
    lock_memory_to_ram();

    if (IS_SOFTMODEM_DOSCOPE) {
      load_softscope("nr", PHY_vars_UE_g[0][0]);
    }
    if (IS_SOFTMODEM_IMSCOPE_ENABLED) {
      load_softscope("im", PHY_vars_UE_g[0][0]);
    }

    for (int inst = 0; inst < NB_UE_INST; inst++) {
      LOG_I(PHY,"Intializing UE Threads for instance %d ...\n", inst);
      init_NR_UE_threads(PHY_vars_UE_g[inst][0]);
    }
    printf("UE threads created by %ld\n", gettid());
  }

  // wait for end of program
  printf("TYPE <CTRL-C> TO TERMINATE\n");

  if (create_tasks_nrue(1) < 0) {
    printf("cannot create ITTI tasks\n");
    exit(-1); // need a softer mode
  }

  // Sleep a while before checking all parameters have been used
  // Some are used directly in external threads, asynchronously
  sleep(2);
  config_check_unknown_cmdlineopt(uniqCfg, CONFIG_CHECKALLSECTIONS);

  // wait for end of program
  printf("Entering ITTI signals handler\n");
  printf("TYPE <CTRL-C> TO TERMINATE\n");
  itti_wait_tasks_end(trigger_deregistration);
  printf("Returned from ITTI signal handler\n");
  oai_exit=1;
  printf("oai_exit=%d\n",oai_exit);

  if (ouput_vcd)
    vcd_signal_dumper_close();

  if (PHY_vars_UE_g && PHY_vars_UE_g[0]) {
    for (int CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
      PHY_VARS_NR_UE *phy_vars = PHY_vars_UE_g[0][CC_id];
      if (phy_vars && phy_vars->rfdevice.trx_end_func)
        phy_vars->rfdevice.trx_end_func(&phy_vars->rfdevice);
    }
  }

  free(pckg);
  return 0;
}

