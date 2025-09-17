/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.0  (the "License"); you may not use this file
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

/*! \file PHY/NR_UE_TRANSPORT/nr_dlsch_decoding.c
* \brief Top-level routines for decoding  Turbo-coded (DLSCH) transport channels from 36-212, V8.6 2009-03
* \author R. Knopp
* \date 2011
* \version 0.1
* \company Eurecom
* \email: knopp@eurecom.fr
* \note
* \warning
*/

#include "common/utils/LOG/vcd_signal_dumper.h"
#include "PHY/defs_nr_UE.h"
#include "SCHED_NR_UE/harq_nr.h"
#include "PHY/phy_extern_nr_ue.h"
#include "PHY/CODING/coding_extern.h"
#include "PHY/CODING/coding_defs.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "SCHED_NR_UE/defs.h"
#include "SIMULATION/TOOLS/sim.h"
#include "executables/nr-uesoftmodem.h"
#include "PHY/CODING/nrLDPC_extern.h"
#include "common/utils/nr/nr_common.h"
#include "openair1/PHY/TOOLS/phy_scope_interface.h"
#include "nfapi/open-nFAPI/nfapi/public_inc/nfapi_nr_interface.h"

//#define ENABLE_PHY_PAYLOAD_DEBUG 1

#define OAI_UL_LDPC_MAX_NUM_LLR 27000//26112 // NR_LDPC_NCOL_BG1*NR_LDPC_ZMAX = 68*384
//#define OAI_LDPC_MAX_NUM_LLR 27000//26112 // NR_LDPC_NCOL_BG1*NR_LDPC_ZMAX

static extended_kpi_ue kpiStructure = {0};

extended_kpi_ue* getKPIUE(void) {
  return &kpiStructure;
}

void nr_ue_dlsch_init(NR_UE_DLSCH_t *dlsch_list, int num_dlsch, uint8_t max_ldpc_iterations) {
  for (int i=0; i < num_dlsch; i++) {
    NR_UE_DLSCH_t *dlsch = dlsch_list + i;
    memset(dlsch, 0, sizeof(NR_UE_DLSCH_t));
    dlsch->max_ldpc_iterations = max_ldpc_iterations;
  }
}

void nr_dlsch_unscrambling(int16_t *llr, uint32_t size, uint8_t q, uint32_t Nid, uint32_t n_RNTI)
{
  nr_codeword_unscrambling(llr, size, q, Nid, n_RNTI);
}

static bool nr_ue_postDecode(PHY_VARS_NR_UE *phy_vars_ue,
                             notifiedFIFO_elt_t *req,
                             notifiedFIFO_t *nf_p,
                             const bool last,
                             int b_size,
                             uint8_t b[b_size],
                             int *num_seg_ok,
                             const UE_nr_rxtx_proc_t *proc)
{
  ldpcDecode_ue_t *rdata = (ldpcDecode_ue_t*) NotifiedFifoData(req);
  NR_DL_UE_HARQ_t *harq_process = rdata->harq_process;
  NR_UE_DLSCH_t *dlsch = (NR_UE_DLSCH_t *) rdata->dlsch;
  int r = rdata->segment_r;

  merge_meas(&phy_vars_ue->phy_cpu_stats.cpu_time_stats[DLSCH_DEINTERLEAVING_STATS], &rdata->ts_deinterleave);
  merge_meas(&phy_vars_ue->phy_cpu_stats.cpu_time_stats[DLSCH_RATE_UNMATCHING_STATS], &rdata->ts_rate_unmatch);
  merge_meas(&phy_vars_ue->phy_cpu_stats.cpu_time_stats[DLSCH_LDPC_DECODING_STATS], &rdata->ts_ldpc_decode);

  bool decodeSuccess = (rdata->decodeIterations < (1+dlsch->max_ldpc_iterations));

  if (decodeSuccess) {
    memcpy(b+rdata->offset,
           harq_process->c[r],
           rdata->Kr_bytes - (harq_process->F>>3) -((harq_process->C>1)?3:0));

    (*num_seg_ok)++;
  } else {
    LOG_D(PHY, "DLSCH %d in error\n", rdata->dlsch_id);
  }

  if (!last)
    return false; // continue decoding

  // all segments are done
  kpiStructure.nb_total++;
  kpiStructure.blockSize = dlsch->dlsch_config.TBS;
  kpiStructure.dl_mcs = dlsch->dlsch_config.mcs;
  kpiStructure.nofRBs = dlsch->dlsch_config.number_rbs;

  harq_process->decodeResult = *num_seg_ok == harq_process->C;

  if (harq_process->decodeResult && harq_process->C > 1) {
    /* check global CRC */
    int A = dlsch->dlsch_config.TBS;
    // we have regrouped the transport block
    if (!check_crc(b, lenWithCrc(1, A), crcType(1, A))) {
      LOG_E(PHY,
            " Frame %d.%d LDPC global CRC fails, but individual LDPC CRC succeeded. %d segs\n",
            proc->frame_rx,
            proc->nr_slot_rx,
            harq_process->C);
      harq_process->decodeResult = false;
    }
  }

  if (harq_process->decodeResult) {
    // We search only a reccuring OAI error that propagates all 0 packets with a 0 CRC, so we
    const int sz = dlsch->dlsch_config.TBS / 8;
    if (b[sz] == 0 && b[sz + 1] == 0) {
      // do the check only if the 2 first bytes of the CRC are 0 (it can be CRC16 or CRC24)
      int i = 0;
      while (b[i] == 0 && i < sz)
        i++;
      if (i == sz) {
        LOG_E(PHY,
              "received all 0 pdu, consider it false reception, even if the TS 38.212 7.2.1 says only we should attach the "
              "corresponding CRC, and nothing prevents to have a all 0 packet\n");
        harq_process->decodeResult = false;
      }
    }
  }

  if (harq_process->decodeResult) {
    LOG_D(PHY, "DLSCH received ok \n");
    harq_process->status = SCH_IDLE;
    dlsch->last_iteration_cnt = rdata->decodeIterations;
  } else {
    LOG_D(PHY, "DLSCH received nok \n");
    kpiStructure.nb_nack++;
    dlsch->last_iteration_cnt = dlsch->max_ldpc_iterations + 1;
  }
  return true; // end TB decoding
}

static void nr_processDLSegment(void *arg)
{
  ldpcDecode_ue_t *rdata = (ldpcDecode_ue_t*) arg;
  NR_UE_DLSCH_t *dlsch = rdata->dlsch;
  NR_DL_UE_HARQ_t *harq_process= rdata->harq_process;
  t_nrLDPC_dec_params *p_decoderParms = &rdata->decoderParms;
  int r = rdata->segment_r;
  int E = rdata->E;
  int Qm = rdata->Qm;
  int r_offset = rdata->r_offset;
  uint8_t kc = rdata->Kc;
  uint32_t Tbslbrm = rdata->Tbslbrm;
  short* dlsch_llr = rdata->dlsch_llr;
  int8_t LDPCoutput[OAI_UL_LDPC_MAX_NUM_LLR] __attribute__((aligned(32)));
  int16_t z[68 * 384 + 16] __attribute__((aligned(16)));
  int8_t   l [68*384 + 16] __attribute__ ((aligned(16)));

  const int Kr = harq_process->K;
  const int K_bits_F = Kr - harq_process->F;

  t_nrLDPC_time_stats procTime = {0};

  //if we return before LDPC decoder run, the block is in error
  rdata->decodeIterations = dlsch->max_ldpc_iterations + 1;

  start_meas(&rdata->ts_deinterleave);

  //VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_DLSCH_DEINTERLEAVING, VCD_FUNCTION_IN);
  int16_t w[E];
  nr_deinterleaving_ldpc(E,
                         Qm,
                         w, // [hna] w is e
                         dlsch_llr+r_offset);
  //VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_DLSCH_DEINTERLEAVING, VCD_FUNCTION_OUT);
  stop_meas(&rdata->ts_deinterleave);

  start_meas(&rdata->ts_rate_unmatch);
  /* LOG_D(PHY,"HARQ_PID %d Rate Matching Segment %d (coded bits %d,E %d, F %d,unpunctured/repeated bits %d, TBS %d, mod_order %d, nb_rb %d, Nl %d, rv %d, round %d)...\n",
        harq_pid,r, G,E,harq_process->F,
        Kr*3,
        harq_process->TBS,
        Qm,
        harq_process->nb_rb,
        harq_process->Nl,
        harq_process->rvidx,
        harq_process->round); */
  //VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_DLSCH_RATE_MATCHING, VCD_FUNCTION_IN);

  if (nr_rate_matching_ldpc_rx(Tbslbrm,
                               p_decoderParms->BG,
                               p_decoderParms->Z,
                               harq_process->d[r],
                               w,
                               harq_process->C,
                               dlsch->dlsch_config.rv,
                               harq_process->first_rx,
                               E,
                               harq_process->F,
                               Kr-harq_process->F-2*(p_decoderParms->Z))==-1) {
    //VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_DLSCH_RATE_MATCHING, VCD_FUNCTION_OUT);
    stop_meas(&rdata->ts_rate_unmatch);
    LOG_E(PHY,"dlsch_decoding.c: Problem in rate_matching\n");
    return;
  }
  stop_meas(&rdata->ts_rate_unmatch);

  if (LOG_DEBUGFLAG(DEBUG_DLSCH_DECOD)) {
    LOG_D(PHY,"decoder input(segment %u) :",r);

    for (int i=0; i<E; i++)
      LOG_D(PHY,"%d : %d\n",i,harq_process->d[r][i]);

    LOG_D(PHY,"\n");
  }
  {
    start_meas(&rdata->ts_ldpc_decode);
    //set first 2*Z_c bits to zeros
    memset(z,0,2*harq_process->Z*sizeof(int16_t));
    //set Filler bits
    memset((z+K_bits_F),127,harq_process->F*sizeof(int16_t));
    //Move coded bits before filler bits
    memcpy((z+2*harq_process->Z),harq_process->d[r],(K_bits_F-2*harq_process->Z)*sizeof(int16_t));
    //skip filler bits
    memcpy((z+Kr),harq_process->d[r]+(Kr-2*harq_process->Z),(kc*harq_process->Z-Kr)*sizeof(int16_t));

    //Saturate coded bits before decoding into 8 bits values
    simde__m128i *pv = (simde__m128i*)&z;
    simde__m128i *pl = (simde__m128i*)&l;
    for (int i=0, j=0; j < ((kc*harq_process->Z)>>4)+1;  i+=2, j++) {
      pl[j] = simde_mm_packs_epi16(pv[i],pv[i+1]);
    }

    //VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_DLSCH_LDPC, VCD_FUNCTION_IN);
    uint32_t A = dlsch->dlsch_config.TBS;
    p_decoderParms->E = lenWithCrc(harq_process->C, A);
    p_decoderParms->crc_type = crcType(harq_process->C, A);
    rdata->decodeIterations =
        ldpc_interface.LDPCdecoder(p_decoderParms, 0, 0, 0, l, LDPCoutput, &procTime, &harq_process->abort_decode);
    //VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_DLSCH_LDPC, VCD_FUNCTION_OUT);

    if (rdata->decodeIterations <= dlsch->max_ldpc_iterations)
      memcpy(harq_process->c[r], LDPCoutput, Kr >> 3);
    stop_meas(&rdata->ts_ldpc_decode);
  }
}

uint32_t nr_dlsch_decoding(PHY_VARS_NR_UE *phy_vars_ue,
                           const UE_nr_rxtx_proc_t *proc,
                           int eNB_id,
                           short *dlsch_llr,
                           NR_DL_FRAME_PARMS *frame_parms,
                           NR_UE_DLSCH_t *dlsch,
                           NR_DL_UE_HARQ_t *harq_process,
                           uint32_t frame,
                           uint16_t nb_symb_sch,
                           uint8_t nr_slot_rx,
                           uint8_t harq_pid,
                           int b_size,
                           uint8_t b[b_size],
                           int G)
{
  uint32_t ret,offset;
  uint32_t r,r_offset=0,Kr=8424,Kr_bytes;
  t_nrLDPC_dec_params decParams;
  decParams.check_crc = check_crc;

  if (!harq_process) {
    LOG_E(PHY,"dlsch_decoding.c: NULL harq_process pointer\n");
    return(dlsch->max_ldpc_iterations + 1);
  }

  // HARQ stats
  phy_vars_ue->dl_stats[harq_process->DLround]++;
  LOG_D(PHY, "Round %d RV idx %d\n", harq_process->DLround, dlsch->dlsch_config.rv);
  uint16_t nb_rb;// = 30;
  uint8_t dmrs_Type = dlsch->dlsch_config.dmrsConfigType;
  AssertFatal(dmrs_Type == 0 || dmrs_Type == 1, "Illegal dmrs_type %d\n", dmrs_Type);
  uint8_t nb_re_dmrs;

  if (dmrs_Type==NFAPI_NR_DMRS_TYPE1) {
    nb_re_dmrs = 6*dlsch->dlsch_config.n_dmrs_cdm_groups;
  } else {
    nb_re_dmrs = 4*dlsch->dlsch_config.n_dmrs_cdm_groups;
  }

  uint16_t dmrs_length = get_num_dmrs(dlsch->dlsch_config.dlDmrsSymbPos);
  vcd_signal_dumper_dump_function_by_name(VCD_SIGNAL_DUMPER_FUNCTIONS_DLSCH_SEGMENTATION, VCD_FUNCTION_IN);

  //NR_DL_UE_HARQ_t *harq_process = dlsch->harq_processes[0];

  if (!dlsch_llr) {
    LOG_E(PHY,"dlsch_decoding.c: NULL dlsch_llr pointer\n");
    return(dlsch->max_ldpc_iterations + 1);
  }

  if (!frame_parms) {
    LOG_E(PHY,"dlsch_decoding.c: NULL frame_parms pointer\n");
    return(dlsch->max_ldpc_iterations + 1);
  }

  nb_rb = dlsch->dlsch_config.number_rbs;
  uint32_t A = dlsch->dlsch_config.TBS;
  ret = dlsch->max_ldpc_iterations + 1;
  dlsch->last_iteration_cnt = ret;

  // target_code_rate is in 0.1 units
  float Coderate = (float) dlsch->dlsch_config.targetCodeRate / 10240.0f;

  decParams.BG = dlsch->dlsch_config.ldpcBaseGraph;
  unsigned int kc = decParams.BG == 2 ? 52 : 68;

  if (harq_process->first_rx == 1) {
    // This is a new packet, so compute quantities regarding segmentation
    nr_segmentation(NULL,
                    NULL,
                    lenWithCrc(1, A), // We give a max size in case of 1 segment
                    &harq_process->C,
                    &harq_process->K,
                    &harq_process->Z, // [hna] Z is Zc
                    &harq_process->F,
                    decParams.BG);

    if (harq_process->C > MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * dlsch->Nl) {
      LOG_E(PHY, "nr_segmentation.c: too many segments %d, A %d\n", harq_process->C, A);
      return(-1);
    }

    if (LOG_DEBUGFLAG(DEBUG_DLSCH_DECOD) && (!frame%100))
      LOG_I(PHY,"K %d C %d Z %d nl %d \n", harq_process->K, harq_process->C, harq_process->Z, dlsch->Nl);
    // clear HARQ buffer
    for (int i=0; i <harq_process->C; i++)
      memset(harq_process->d[i],0,5*8448*sizeof(int16_t));
  }

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_DLSCH_SEGMENTATION, VCD_FUNCTION_OUT);
  decParams.Z = harq_process->Z;
  decParams.numMaxIter = dlsch->max_ldpc_iterations;
  decParams.outMode = 0;
  r_offset = 0;
  uint16_t a_segments = MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * dlsch->Nl;  //number of segments to be allocated

  if (nb_rb != 273) {
    a_segments = a_segments*nb_rb;
    a_segments = a_segments/273 +1;
  }

  if (harq_process->C > a_segments) {
    LOG_E(PHY,"Illegal harq_process->C %d > %d\n",harq_process->C,a_segments);
    return((1+dlsch->max_ldpc_iterations));
  }

  if (LOG_DEBUGFLAG(DEBUG_DLSCH_DECOD))
    LOG_I(PHY,"Segmentation: C %d, K %d\n",harq_process->C,harq_process->K);

  Kr = harq_process->K;
  Kr_bytes = Kr>>3;
  offset = 0;
  notifiedFIFO_t nf;
  initNotifiedFIFO(&nf);
  set_abort(&harq_process->abort_decode, false);
  for (r=0; r<harq_process->C; r++) {
    //printf("start rx segment %d\n",r);
    uint32_t E = nr_get_E(G, harq_process->C, dlsch->dlsch_config.qamModOrder, dlsch->Nl, r);
    decParams.R = nr_get_R_ldpc_decoder(dlsch->dlsch_config.rv, E, decParams.BG, decParams.Z, &harq_process->llrLen, harq_process->DLround);
    union ldpcReqUnion id = {.s = {dlsch->rnti, frame, nr_slot_rx, 0, 0}};
    notifiedFIFO_elt_t *req = newNotifiedFIFO_elt(sizeof(ldpcDecode_ue_t), id.p, &nf, &nr_processDLSegment);
    ldpcDecode_ue_t * rdata=(ldpcDecode_ue_t *) NotifiedFifoData(req);

    rdata->phy_vars_ue = phy_vars_ue;
    rdata->harq_process = harq_process;
    rdata->decoderParms = decParams;
    rdata->dlsch_llr = dlsch_llr;
    rdata->Kc = kc;
    rdata->segment_r = r;
    rdata->E = E;
    rdata->Qm = dlsch->dlsch_config.qamModOrder;
    rdata->r_offset = r_offset;
    rdata->Kr_bytes = Kr_bytes;
    rdata->rv_index = dlsch->dlsch_config.rv;
    rdata->Tbslbrm = dlsch->dlsch_config.tbslbrm;
    rdata->offset = offset;
    rdata->dlsch = dlsch;
    rdata->dlsch_id = harq_pid;
    reset_meas(&rdata->ts_deinterleave);
    reset_meas(&rdata->ts_rate_unmatch);
    reset_meas(&rdata->ts_ldpc_decode);
    pushTpool(&get_nrUE_params()->Tpool,req);
    LOG_D(PHY, "Added a block to decode, in pipe: %d\n", r);
    r_offset += E;
    offset += (Kr_bytes - (harq_process->F>>3) - ((harq_process->C>1)?3:0));
    //////////////////////////////////////////////////////////////////////////////////////////
  }
  int num_seg_ok = 0;
  int nbDecode = harq_process->C;
  while (nbDecode) {
    notifiedFIFO_elt_t *req=pullTpool(&nf,  &get_nrUE_params()->Tpool);
    if (req == NULL)
      break; // Tpool has been stopped
    nr_ue_postDecode(phy_vars_ue, req, &nf, nbDecode == 1, b_size, b, &num_seg_ok, proc);
    delNotifiedFIFO_elt(req);
    nbDecode--;
  }
  LOG_D(PHY,
        "%d.%d DLSCH Decoded, harq_pid %d, round %d, result: %d TBS %d (%d) G %d nb_re_dmrs %d length dmrs %d mcs %d Nl %d "
        "nb_symb_sch %d "
        "nb_rb %d Qm %d Coderate %f\n",
        frame,
        nr_slot_rx,
        harq_pid,
        harq_process->DLround,
        harq_process->decodeResult,
        A,
        A / 8,
        G,
        nb_re_dmrs,
        dmrs_length,
        dlsch->dlsch_config.mcs,
        dlsch->Nl,
        nb_symb_sch,
        nb_rb,
        dlsch->dlsch_config.qamModOrder,
        Coderate);
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_DLSCH_COMBINE_SEG, VCD_FUNCTION_OUT);
  ret = dlsch->last_iteration_cnt;
  return(ret);
}
