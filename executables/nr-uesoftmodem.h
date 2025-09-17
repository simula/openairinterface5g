#ifndef NR_UESOFTMODEM_H
#define NR_UESOFTMODEM_H
#include <executables/nr-softmodem-common.h>
#include <executables/softmodem-common.h>
#include "PHY/defs_nr_UE.h"

#define  CONFIG_HLP_IF_FREQ                "IF frequency for RF, if needed\n"
#define  CONFIG_HLP_IF_FREQ_OFF            "UL IF frequency offset for RF, if needed\n"
#define  CONFIG_HLP_DLSCH_PARA             "number of threads for dlsch processing 0 for no parallelization\n"
#define  CONFIG_HLP_OFFSET_DIV             "Divisor for computing OFDM symbol offset in Rx chain (num samples in CP/<the value>). Default value is 8. To set the sample offset to 0, set this value ~ 10e6\n"
#define  CONFIG_HLP_MAX_LDPC_ITERATIONS    "Maximum LDPC decoder iterations\n"
#define  CONFIG_HLP_TIME_SYNC_P            "coefficient for Proportional part of time sync PI controller\n"
#define  CONFIG_HLP_TIME_SYNC_I            "coefficient for Integrating part of time sync PI controller\n"
#define  CONFIG_HLP_NTN_KOFFSET            "NTN cellSpecificKoffset-r17 (number of slots for a given subcarrier spacing of 15 kHz)\n"
#define  CONFIG_HLP_NTN_TA_COMMON          "NTN ta-Common, but given in ms\n"
#define  CONFIG_HLP_NTN_TA_COMMONDRIFT     "NTN ta-CommonDrift, but given in µs/s\n"
#define  CONFIG_HLP_AUTONOMOUS_TA          "Autonomously update TA based on DL drift (useful if main contribution to DL drift is movement, e.g. LEO satellite)\n"
#define  CONFIG_HLP_AGC                    "Rx Gain control used for UE\n"

/***************************************************************************************************************************************/
/* command line options definitions, CMDLINE_XXXX_DESC macros are used to initialize paramdef_t arrays which are then used as argument
   when calling config_get or config_getlist functions                                                                                 */

#define CALIBRX_OPT       "calib-ue-rx"
#define CALIBRXMED_OPT    "calib-ue-rx-med"
#define CALIBRXBYP_OPT    "calib-ue-rx-byp"
#define DBGPRACH_OPT      "debug-ue-prach"
#define NOL2CONNECT_OPT   "no-L2-connect"
#define CALIBPRACH_OPT    "calib-prach-tx"
#define DUMPFRAME_OPT     "ue-dump-frame"

/*------------------------------------------------------------------------------------------------------------------------------------------*/
/*                                            command line parameters defining UE running mode                                              */
/*   optname                     helpstr                paramflags                      XXXptr        defXXXval         type       numelt   */
/*------------------------------------------------------------------------------------------------------------------------------------------*/
// clang-format off
#define CMDLINE_NRUEPARAMS_DESC {  \
  {"usrp-args",                CONFIG_HLP_USRP_ARGS,           0,               .strptr=&nrUE_params.usrp_args,           .defstrval="type=b200",          TYPE_STRING,   0}, \
  {"tx_subdev",                CONFIG_HLP_TX_SUBDEV,           0,               .strptr=&nrUE_params.tx_subdev,           .defstrval=NULL,                 TYPE_STRING,   0}, \
  {"rx_subdev",                CONFIG_HLP_RX_SUBDEV,           0,               .strptr=&nrUE_params.rx_subdev,           .defstrval=NULL,                 TYPE_STRING,   0}, \
  {"dlsch-parallel",           CONFIG_HLP_DLSCH_PARA,          0,               .u8ptr=NULL,                              .defintval=0,                    TYPE_UINT8,    0}, \
  {"offset-divisor",           CONFIG_HLP_OFFSET_DIV,          0,               .uptr=&nrUE_params.ofdm_offset_divisor,   .defuintval=8,                   TYPE_UINT32,   0}, \
  {"max-ldpc-iterations",      CONFIG_HLP_MAX_LDPC_ITERATIONS, 0,               .iptr=&nrUE_params.max_ldpc_iterations,   .defuintval=8,                  TYPE_UINT8,    0}, \
  {"ldpc-offload-enable",      CONFIG_HLP_LDPC_OFFLOAD,        PARAMFLAG_BOOL,  .iptr=&(nrUE_params.ldpc_offload_flag),   .defintval=0,                   TYPE_INT,      0}, \
  {"V" ,                       CONFIG_HLP_VCD,                 PARAMFLAG_BOOL,  .iptr=&nrUE_params.vcdflag,                 .defintval=0,                    TYPE_INT,      0}, \
  {"uecap_file",               CONFIG_HLP_UECAP_FILE,          0,               .strptr=&nrUE_params.uecap_file,            .defstrval="./uecap_ports1.xml", TYPE_STRING,   0}, \
  {"reconfig-file",            CONFIG_HLP_RE_CFG_FILE,         0,               .strptr=&nrUE_params.reconfig_file,         .defstrval="./reconfig.raw",     TYPE_STRING,   0}, \
  {"rbconfig-file",            CONFIG_HLP_RB_CFG_FILE,         0,               .strptr=&nrUE_params.rbconfig_file,         .defstrval="./rbconfig.raw",     TYPE_STRING,   0}, \
  {"ue-idx-standalone",        NULL,                           0,               .u16ptr=&ue_idx_standalone,                 .defuintval=0xFFFF,              TYPE_UINT16,   0}, \
  {"ue-rxgain",                    CONFIG_HLP_UERXG,           0,               .dblptr=&nrUE_params.rx_gain,               .defdblval=110,    TYPE_DOUBLE,   0}, \
  {"ue-rxgain-off",                CONFIG_HLP_UERXGOFF,        0,               .dblptr=&nrUE_params.rx_gain_off,           .defdblval=0,      TYPE_DOUBLE,   0}, \
  {"ue-txgain",                    CONFIG_HLP_UETXG,           0,               .dblptr=&nrUE_params.tx_gain,               .defdblval=0,      TYPE_DOUBLE,   0}, \
  {"ue-nb-ant-rx",                 CONFIG_HLP_UENANTR,         0,               .iptr=&(nrUE_params.nb_antennas_rx),        .defuintval=1,     TYPE_UINT8,    0}, \
  {"ue-nb-ant-tx",                 CONFIG_HLP_UENANTT,         0,               .iptr=&(nrUE_params.nb_antennas_tx),        .defuintval=1,     TYPE_UINT8,    0}, \
  {"ue-scan-carrier",              CONFIG_HLP_UESCAN,          PARAMFLAG_BOOL,  .iptr=&(nrUE_params.UE_scan_carrier),        .defintval=0,      TYPE_INT,      0}, \
  {"ue-fo-compensation",           CONFIG_HLP_UEFO,            PARAMFLAG_BOOL,  .iptr=&(nrUE_params.UE_fo_compensation),     .defintval=0,      TYPE_INT,      0}, \
  {"ue-max-power",                 NULL,                       0,               .iptr=&(nrUE_params.tx_max_power),            .defintval=90,     TYPE_INT,      0}, \
  {"r"  ,                          CONFIG_HLP_PRB_SA,          0,               .iptr=&(nrUE_params.N_RB_DL),                .defintval=106,    TYPE_UINT,     0}, \
  {"ssb",                          CONFIG_HLP_SSC,             0,               .iptr=&(nrUE_params.ssb_start_subcarrier), .defintval=516,    TYPE_UINT16,   0}, \
  {"if_freq" ,                     CONFIG_HLP_IF_FREQ,         0,               .u64ptr=&(nrUE_params.if_freq),              .defuintval=0,     TYPE_UINT64,   0}, \
  {"if_freq_off" ,                 CONFIG_HLP_IF_FREQ_OFF,     0,               .iptr=&(nrUE_params.if_freq_off),            .defuintval=0,     TYPE_INT,      0}, \
  {"chest-freq",                   CONFIG_HLP_CHESTFREQ,       0,               .iptr=&(nrUE_params.chest_freq),             .defintval=0,      TYPE_INT,      0}, \
  {"chest-time",                   CONFIG_HLP_CHESTTIME,       0,               .iptr=&(nrUE_params.chest_time),             .defintval=0,      TYPE_INT,      0}, \
  {"ue-timing-correction-disable", CONFIG_HLP_DISABLETIMECORR, PARAMFLAG_BOOL,  .iptr=&(nrUE_params.no_timing_correction),   .defintval=0,      TYPE_INT,      0}, \
  {"SLC",                          CONFIG_HLP_SLF,             0,               .u64ptr=&(sidelink_frequency[0][0]),         .defuintval=2600000000,TYPE_UINT64,0}, \
  {"num-ues",                      NULL,                       0,               .iptr=&(NB_UE_INST),                         .defuintval=1,     TYPE_INT,      0}, \
  {"time-sync-P",                  CONFIG_HLP_TIME_SYNC_P,     0,               .dblptr=&(nrUE_params.time_sync_P),          .defdblval=0.5,    TYPE_DOUBLE,   0}, \
  {"time-sync-I",                  CONFIG_HLP_TIME_SYNC_I,     0,               .dblptr=&(nrUE_params.time_sync_I),          .defdblval=0.0,    TYPE_DOUBLE,   0}, \
  {"ntn-koffset",                  CONFIG_HLP_NTN_KOFFSET,     0,               .uptr=&(nrUE_params.ntn_koffset),            .defuintval=0,     TYPE_UINT,     0}, \
  {"ntn-ta-common",                CONFIG_HLP_NTN_TA_COMMON,   0,               .dblptr=&(nrUE_params.ntn_ta_common),        .defdblval=0.0,    TYPE_DOUBLE,   0}, \
  {"ntn-ta-commondrift",           CONFIG_HLP_NTN_TA_COMMONDRIFT, 0,            .dblptr=&(nrUE_params.ntn_ta_commondrift),   .defdblval=0.0,    TYPE_DOUBLE,   0}, \
  {"autonomous-ta",                CONFIG_HLP_AUTONOMOUS_TA,   PARAMFLAG_BOOL,  .iptr=&(nrUE_params.autonomous_ta),          .defintval=0,      TYPE_INT,      0}, \
  {"agc",                          CONFIG_HLP_AGC,             PARAMFLAG_BOOL,  .iptr=&(nrUE_params.agc),                    .defintval=0,      TYPE_INT,      0}, \
}
// clang-format on

typedef struct {
  uint64_t optmask; // mask to store boolean config options
  uint32_t ofdm_offset_divisor; // Divisor for sample offset computation for each OFDM symbol
  int max_ldpc_iterations; // number of maximum LDPC iterations
  tpool_t Tpool; // thread pool
  int UE_scan_carrier;
  int UE_fo_compensation;
  uint64_t if_freq;
  int if_freq_off;
  int chest_freq;
  int chest_time;
  int no_timing_correction;
  int nb_antennas_rx;
  int nb_antennas_tx;
  int N_RB_DL;
  int ssb_start_subcarrier;
  int ldpc_offload_flag;
  double time_sync_P;
  double time_sync_I;
  unsigned int ntn_koffset;
  double ntn_ta_common;
  double ntn_ta_commondrift;
  int autonomous_ta;
  int agc;
  char *usrp_args;
  char *tx_subdev;
  char *rx_subdev;
  char *reconfig_file;
  char *rbconfig_file;
  char *uecap_file;
  double tx_gain;
  double rx_gain;
  double rx_gain_off;
  int vcdflag;
  int tx_max_power;
} nrUE_params_t;
extern uint64_t get_nrUE_optmask(void);
extern uint64_t set_nrUE_optmask(uint64_t bitmask);
extern nrUE_params_t *get_nrUE_params(void);


// In nr-ue.c
extern int setup_nr_ue_buffers(PHY_VARS_NR_UE **phy_vars_ue, openair0_config_t *openair0_cfg);
extern void fill_ue_band_info(void);
extern void init_NR_UE(int, char *, char *, char *);
extern void init_NR_UE_threads(PHY_VARS_NR_UE *ue);
void start_oai_nrue_threads(void);
void *UE_thread(void *arg);
void init_nr_ue_vars(PHY_VARS_NR_UE *ue, uint8_t UE_id);
void init_nrUE_standalone_thread(int ue_idx);
#endif
