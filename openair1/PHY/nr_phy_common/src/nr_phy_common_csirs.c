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

#include "PHY/nr_phy_common/inc/nr_phy_common.h"

void csi_rs_resource_mapping(int32_t **dataF,
                             int csi_rs_length,
                             int16_t mod_csi[][csi_rs_length >> 1],
                             int ofdm_symbol_size,
                             int dataF_offset,
                             int start_sc,
                             const csi_mapping_parms_t *mapping_parms,
                             int start_rb,
                             int nb_rbs,
                             double alpha,
                             int beta,
                             double rho,
                             int gs,
                             int freq_density)
{
  // resource mapping according to 38.211 7.4.1.5.3
  for (int n = start_rb; n < (start_rb + nb_rbs); n++) {
    if ((freq_density > 1) || (freq_density == (n % 2))) {  // for freq density 0.5 checks if even or odd RB
      for (int ji = 0; ji < mapping_parms->size; ji++) { // loop over CDM groups
        for (int s = 0 ; s < gs; s++)  { // loop over each CDM group size
          int p = s + mapping_parms->j[ji] * gs; // port index
          for (int kp = 0; kp <= mapping_parms->kprime; kp++) { // loop over frequency resource elements within a group
            // frequency index of current resource element
            int k = (start_sc + (n * NR_NB_SC_PER_RB) + mapping_parms->koverline[ji] + kp) % (ofdm_symbol_size);
            // wf according to tables 7.4.5.3-2 to 7.4.5.3-5
            int wf = kp == 0 ? 1 : (-2 * (s % 2) + 1);
            int na = n * alpha;
            int kpn = (rho * mapping_parms->koverline[ji]) / NR_NB_SC_PER_RB;
            int mprime = na + kp + kpn; // sequence index
            for (int lp = 0; lp <= mapping_parms->lprime; lp++) { // loop over frequency resource elements within a group
              int l = lp + mapping_parms->loverline[ji];
              // wt according to tables 7.4.5.3-2 to 7.4.5.3-5
              int wt;
              if (s < 2)
                wt = 1;
              else if (s < 4)
                wt = -2 * (lp % 2) + 1;
              else if (s < 6)
                wt = -2 * (lp / 2) + 1;
              else {
                if ((lp == 0) || (lp == 3))
                  wt = 1;
                else
                  wt = -1;
              }
              int index = ((l * ofdm_symbol_size + k) << 1) + (2 * dataF_offset);
              ((int16_t*)dataF[p])[index] = (beta * wt * wf * mod_csi[l][mprime << 1]) >> 15;
              ((int16_t*)dataF[p])[index + 1] = (beta * wt * wf * mod_csi[l][(mprime << 1) + 1]) >> 15;
              LOG_D(PHY,
                    "l,k (%d,%d)  seq. index %d \t port %d \t (%d,%d)\n",
                    l,
                    k,
                    mprime,
                    p + 3000,
                    ((int16_t*)dataF[p])[index],
                    ((int16_t*)dataF[p])[index + 1]);
            }
          }
        }
      }
    }
  }
}

void get_modulated_csi_symbols(int symbols_per_slot,
                               int slot,
                               int N_RB_DL,
                               int mod_length,
                               int16_t mod_csi[][(N_RB_DL << 4) >> 1],
                               int lprime,
                               int l0,
                               int l1,
                               int row,
                               int scramb_id)
{
  for (int lp = 0; lp <= lprime; lp++) {
    int symb = l0;
    const uint32_t *gold =
        nr_gold_csi_rs(N_RB_DL, symbols_per_slot, slot, symb + lp, scramb_id);
    nr_modulation(gold, mod_length, DMRS_MOD_ORDER, mod_csi[symb + lp]);
    if ((row == 5) || (row == 7) || (row == 11) || (row == 13) || (row == 16)) {
      const uint32_t *gold =
          nr_gold_csi_rs(N_RB_DL, symbols_per_slot, slot, symb + 1, scramb_id);
      nr_modulation(gold, mod_length, DMRS_MOD_ORDER, mod_csi[symb + 1]);
    }
    if ((row == 14) || (row == 13) || (row == 16) || (row == 17)) {
      symb = l1;
      const uint32_t *gold =
          nr_gold_csi_rs(N_RB_DL, symbols_per_slot, slot, symb + lp, scramb_id);
      nr_modulation(gold, mod_length, DMRS_MOD_ORDER, mod_csi[symb + lp]);
      if ((row == 13) || (row == 16)) {
        const uint32_t *gold =
            nr_gold_csi_rs(N_RB_DL, symbols_per_slot, slot, symb + 1, scramb_id);
        nr_modulation(gold, mod_length, DMRS_MOD_ORDER, mod_csi[symb + 1]);
      }
    }
  }
}

uint32_t get_csi_beta_amplitude(const int16_t amp, int power_control_offset_ss)
{
  uint32_t beta = amp;
  // assuming amp is the amplitude of SSB channels
  switch (power_control_offset_ss) {
    case 0:
      beta = (amp * ONE_OVER_SQRT2_Q15) >> 15;
      break;
    case 1:
      beta = amp;
      break;
    case 2:
      beta = (amp * ONE_OVER_SQRT2_Q15) >> 14;
      break;
    case 3:
      beta = amp << 1;
      break;
    default:
      AssertFatal(false, "Invalid SS power offset density index for CSI\n");
  }
  return beta;
}

int get_csi_modulation_length(double rho, int freq_density, int kprime, int start_rb, int nr_rbs)
{
  int csi_length = 0;
  if (rho < 1) {
    if (freq_density == 0) {
      csi_length = (((start_rb + nr_rbs) >> 1) << kprime) << 1;
    } else {
      csi_length = ((((start_rb + nr_rbs) >> 1) << kprime) + 1) << 1;
    }
  } else {
    csi_length = (((uint16_t) rho * (start_rb + nr_rbs)) << kprime) << 1;
  }
  return csi_length;
}

double get_csi_rho(int freq_density)
{
  // setting the frequency density from its index
  double rho = 0;
  switch (freq_density) {
    case 0:
      rho = 0.5;
      break;
    case 1:
      rho = 0.5;
      break;
     case 2:
      rho = 1;
      break;
     case 3:
      rho = 3;
      break;
    default:
      AssertFatal(false, "Invalid frequency density index for CSI\n");
  }
  return rho;
}

int get_cdm_group_size(int cdm_type)
{
  // CDM group size from CDM type index
  int gs = 0;
  switch (cdm_type) {
    case 0:
      gs = 1;
      break;
    case 1:
      gs = 2;
      break;
    case 2:
      gs = 4;
      break;
    case 3:
      gs = 8;
      break;
    default:
      AssertFatal(false, "Invalid cdm type index for CSI\n");
  }
  return gs;
}

csi_mapping_parms_t get_csi_mapping_parms(int row, int b, int l0, int l1)
{
  AssertFatal(b != 0, "Invalid CSI frequency domain mapping: no bit selected in bitmap\n");
  csi_mapping_parms_t csi_parms = {0};
  int found = 0;
  int fi = 0;
  int k_n[6];
  // implementation of table 7.4.1.5.3-1 of 38.211
  // lprime and kprime are the max value of l' and k'
  switch (row) {
    case 1:
      csi_parms.ports = 1;
      csi_parms.kprime = 0;
      csi_parms.lprime = 0;
      csi_parms.size = 3;
      while (found < 1) {
        if ((b >> fi) & 0x01) {
          k_n[found] = fi;
          found++;
        }
        else
          fi++;
      }
      for (int i = 0; i < csi_parms.size; i++) {
        csi_parms.j[i] = 0;
        csi_parms.loverline[i] = l0;
        csi_parms.koverline[i] = k_n[0] + (i << 2);
      }
      break;
    case 2:
      csi_parms.ports = 1;
      csi_parms.kprime = 0;
      csi_parms.lprime = 0;
      csi_parms.size = 1;
      while (found < 1) {
        if ((b >> fi) & 0x01) {
          k_n[found] = fi;
          found++;
        }
        else
          fi++;
      }
      for (int i = 0; i < csi_parms.size; i++) {
        csi_parms.j[i] = 0;
        csi_parms.loverline[i] = l0;
        csi_parms.koverline[i] = k_n[0];
      }
      break;
    case 3:
      csi_parms.ports = 2;
      csi_parms.kprime = 1;
      csi_parms.lprime = 0;
      csi_parms.size = 1;
      while (found < 1) {
        if ((b >> fi) & 0x01) {
          k_n[found] = fi << 1;
          found++;
        }
        else
          fi++;
      }
      for (int i = 0; i < csi_parms.size; i++) {
        csi_parms.j[i] = 0;
        csi_parms.loverline[i] = l0;
        csi_parms.koverline[i] = k_n[0];
      }
      break;
    case 4:
      csi_parms.ports = 4;
      csi_parms.kprime = 1;
      csi_parms.lprime = 0;
      csi_parms.size = 2;
      while (found < 1) {
        if ((b >> fi) & 0x01) {
          k_n[found] = fi << 2;
          found++;
        }
        else
          fi++;
      }
      for (int i = 0; i < csi_parms.size; i++) {
        csi_parms.j[i] = i;
        csi_parms.loverline[i] = l0;
        csi_parms.koverline[i] = k_n[0] + (i << 1);
      }
      break;
    case 5:
      csi_parms.ports = 4;
      csi_parms.kprime = 1;
      csi_parms.lprime = 0;
      csi_parms.size = 2;
      while (found < 1) {
        if ((b >> fi) & 0x01) {
          k_n[found] = fi << 1;
          found++;
        }
        else
          fi++;
      }
      for (int i = 0; i < csi_parms.size; i++) {
        csi_parms.j[i] = i;
        csi_parms.loverline[i] = l0 + i;
        csi_parms.koverline[i] = k_n[0];
      }
      break;
    case 6:
      csi_parms.ports = 8;
      csi_parms.kprime = 1;
      csi_parms.lprime = 0;
      csi_parms.size = 4;
      while (found < 4) {
        if ((b >> fi) & 0x01) {
          k_n[found] = fi << 1;
          found++;
        }
        fi++;
      }
      for (int i = 0; i < csi_parms.size; i++) {
        csi_parms.j[i] = i;
        csi_parms.loverline[i] = l0;
        csi_parms.koverline[i] = k_n[i];
      }
      break;
    case 7:
      csi_parms.ports = 8;
      csi_parms.kprime = 1;
      csi_parms.lprime = 0;
      csi_parms.size = 4;
      while (found < 2) {
        if ((b >> fi) & 0x01) {
          k_n[found] = fi << 1;
          found++;
        }
        fi++;
      }
      for (int i = 0; i < csi_parms.size; i++) {
        csi_parms.j[i] = i;
        csi_parms.loverline[i] = l0 + (i >> 1);
        csi_parms.koverline[i] = k_n[i % 2];
      }
      break;
    case 8:
      csi_parms.ports = 8;
      csi_parms.kprime = 1;
      csi_parms.lprime = 1;
      csi_parms.size = 2;
      while (found < 2) {
        if ((b >> fi) & 0x01) {
          k_n[found] = fi << 1;
          found++;
        }
        fi++;
      }
      for (int i = 0; i < csi_parms.size; i++) {
        csi_parms.j[i] = i;
        csi_parms.loverline[i] = l0;
        csi_parms.koverline[i] = k_n[i];
      }
      break;
    case 9:
      csi_parms.ports = 12;
      csi_parms.kprime = 1;
      csi_parms.lprime = 0;
      csi_parms.size = 6;
      while (found < 6) {
        if ((b >> fi) & 0x01) {
          k_n[found] = fi << 1;
          found++;
        }
        fi++;
      }
      for (int i = 0; i < csi_parms.size; i++) {
        csi_parms.j[i] = i;
        csi_parms.loverline[i] = l0;
        csi_parms.koverline[i] = k_n[i];
      }
      break;
    case 10:
      csi_parms.ports = 12;
      csi_parms.kprime = 1;
      csi_parms.lprime = 1;
      csi_parms.size = 3;
      while (found < 3) {
        if ((b >> fi) & 0x01) {
          k_n[found] = fi << 1;
          found++;
        }
        fi++;
      }
      for (int i = 0; i < csi_parms.size; i++) {
        csi_parms.j[i] = i;
        csi_parms.loverline[i] = l0;
        csi_parms.koverline[i] = k_n[i];
      }
      break;
    case 11:
      csi_parms.ports = 16;
      csi_parms.kprime = 1;
      csi_parms.lprime = 0;
      csi_parms.size = 8;
      while (found < 4) {
        if ((b >> fi) & 0x01) {
          k_n[found] = fi << 1;
          found++;
        }
        fi++;
      }
      for (int i = 0; i < csi_parms.size; i++) {
        csi_parms.j[i] = i;
        csi_parms.loverline[i] = l0 + (i >> 2);
        csi_parms.koverline[i] = k_n[i % 4];
      }
      break;
    case 12:
      csi_parms.ports = 16;
      csi_parms.kprime = 1;
      csi_parms.lprime = 1;
      csi_parms.size = 4;
      while (found < 4) {
        if ((b >> fi) & 0x01) {
          k_n[found] = fi << 1;
          found++;
        }
        fi++;
      }
      for (int i = 0; i < csi_parms.size; i++) {
        csi_parms.j[i] = i;
        csi_parms.loverline[i] = l0;
        csi_parms.koverline[i] = k_n[i];
      }
      break;
    case 13:
      csi_parms.ports = 24;
      csi_parms.kprime = 1;
      csi_parms.lprime = 0;
      csi_parms.size = 12;
      while (found < 3) {
        if ((b >> fi) & 0x01) {
          k_n[found] = fi << 1;
          found++;
        }
        fi++;
      }
      for (int i = 0; i < csi_parms.size; i++) {
        csi_parms.j[i] = i;
        if (i < 6)
          csi_parms.loverline[i] = l0 + i / 3;
        else
          csi_parms.loverline[i] = l1 + i / 9;
        csi_parms.koverline[i] = k_n[i % 3];
      }
      break;
    case 14:
      csi_parms.ports = 24;
      csi_parms.kprime = 1;
      csi_parms.lprime = 1;
      csi_parms.size = 6;
      while (found < 3) {
        if ((b >> fi) & 0x01) {
          k_n[found] = fi << 1;
          found++;
        }
        fi++;
      }
      for (int i = 0; i < csi_parms.size; i++) {
        csi_parms.j[i] = i;
        if (i < 3)
          csi_parms.loverline[i] = l0;
        else
          csi_parms.loverline[i] = l1;
        csi_parms.koverline[i] = k_n[i % 3];
      }
      break;
    case 15:
      csi_parms.ports = 24;
      csi_parms.kprime = 1;
      csi_parms.lprime = 3;
      csi_parms.size = 3;
      while (found < 3) {
        if ((b >> fi) & 0x01) {
          k_n[found] = fi << 1;
          found++;
        }
        fi++;
      }
      for (int i = 0; i < csi_parms.size; i++) {
        csi_parms.j[i] = i;
        csi_parms.loverline[i] = l0;
        csi_parms.koverline[i] = k_n[i];
      }
      break;
    case 16:
      csi_parms.ports = 32;
      csi_parms.kprime = 1;
      csi_parms.lprime = 0;
      csi_parms.size = 16;
      while (found < 4) {
        if ((b >> fi) & 0x01) {
          k_n[found] = fi << 1;
          found++;
        }
        fi++;
      }
      for (int i = 0; i < csi_parms.size; i++) {
        csi_parms.j[i] = i;
        if (i < 8)
          csi_parms.loverline[i] = l0 + (i >> 2);
        else
          csi_parms.loverline[i] = l1 + (i / 12);
        csi_parms.koverline[i] = k_n[i % 4];
      }
      break;
    case 17:
      csi_parms.ports = 32;
      csi_parms.kprime = 1;
      csi_parms.lprime = 1;
      csi_parms.size = 8;
      while (found < 4) {
        if ((b >> fi) & 0x01) {
          k_n[found] = fi << 1;
          found++;
        }
        fi++;
      }
      for (int i = 0; i < csi_parms.size; i++) {
        csi_parms.j[i] = i;
        if (i < 4)
          csi_parms.loverline[i] = l0;
        else
          csi_parms.loverline[i] = l1;
        csi_parms.koverline[i] = k_n[i % 4];
      }
      break;
    case 18:
      csi_parms.ports = 32;
      csi_parms.kprime = 1;
      csi_parms.lprime = 3;
      csi_parms.size = 4;
      while (found < 4) {
        if ((b >> fi) & 0x01) {
          k_n[found] = fi << 1;
          found++;
        }
        fi++;
      }
      for (int i = 0; i < csi_parms.size; i++) {
        csi_parms.j[i] = i;
        csi_parms.loverline[i] = l0;
        csi_parms.koverline[i] = k_n[i];
      }
      break;
    default:
      AssertFatal(false, "Row %d is not valid for CSI Table 7.4.1.5.3-1\n", row);
  }
  return csi_parms;
}

