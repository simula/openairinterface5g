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

/*
* @defgroup _PHY_MODULATION_
* @ingroup _physical_layer_ref_implementation_
* @{
\section _phy_modulation_ OFDM Modulation Blocks
This section deals with basic functions for OFDM Modulation.



*/

#include "PHY/defs_eNB.h"
#include "PHY/defs_gNB.h"
#include "PHY/impl_defs_top.h"
#include "PHY/impl_defs_nr.h"
#include "common/utils/LOG/log.h"
#include "common/utils/LOG/vcd_signal_dumper.h"
#include "modulation_common.h"
#include "PHY/LTE_TRANSPORT/transport_common_proto.h"
//#define DEBUG_OFDM_MOD

// Use 64-byte alignment for IDFT output buffer to ensure no
// runtime error in case IDFT implementation uses AVX-512.
#define IDFT_OUTPUT_BUFFER_ALIGNMENT 64

void normal_prefix_mod(int32_t *txdataF,int32_t *txdata,uint8_t nsymb,LTE_DL_FRAME_PARMS *frame_parms)
{


  
  PHY_ofdm_mod((int *)txdataF,        // input
               (int *)txdata,         // output
               frame_parms->ofdm_symbol_size,

               1,                 // number of symbols
               frame_parms->nb_prefix_samples0,               // number of prefix samples
               CYCLIC_PREFIX);
  PHY_ofdm_mod((int *)txdataF+frame_parms->ofdm_symbol_size,        // input
               (int *)txdata+OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES0,         // output
               frame_parms->ofdm_symbol_size,
               nsymb-1,
               frame_parms->nb_prefix_samples,               // number of prefix samples
               CYCLIC_PREFIX);
  

  
}

void nr_normal_prefix_mod(c16_t *txdataF,
                          c16_t *txdata,
                          uint8_t nsymb,
                          const NR_DL_FRAME_PARMS *frame_parms,
                          uint32_t slot,
                          bool was_symbol_used[NR_NUMBER_OF_SYMBOLS_PER_SLOT])
{
  // This function works only slot wise. For more generic symbol generation refer nr_feptx0()
  if (frame_parms->numerology_index != 0) { // case where numerology != 0
    if (!(slot%(frame_parms->slots_per_subframe/2))) {
      if (was_symbol_used[0]) {
        PHY_ofdm_mod((int *)txdataF,
                    (int *)txdata,
                    frame_parms->ofdm_symbol_size,
                    1,
                    frame_parms->nb_prefix_samples0,
                    CYCLIC_PREFIX);
      } else {
        memset(txdata, 0, (frame_parms->nb_prefix_samples0 +  frame_parms->ofdm_symbol_size) * sizeof(c16_t));
      }
      for (int i = 1; i < nsymb; i++) {
        c16_t* tx_data_ptr = txdata + (i - 1) * (frame_parms->ofdm_symbol_size + frame_parms->nb_prefix_samples) +
                            frame_parms->ofdm_symbol_size + frame_parms->nb_prefix_samples0;
        if (was_symbol_used[i]) {
          PHY_ofdm_mod((int *)txdataF + frame_parms->ofdm_symbol_size * i,
                      (int *)tx_data_ptr,
                      frame_parms->ofdm_symbol_size,
                      1,
                      frame_parms->nb_prefix_samples,
                      CYCLIC_PREFIX);
        } else {
          memset(tx_data_ptr, 0, (frame_parms->nb_prefix_samples + frame_parms->ofdm_symbol_size) * sizeof(c16_t));
        }
      }
    }
    else {
      for (int i = 0; i < nsymb; i++) {
        c16_t* tx_data_ptr = txdata + i * (frame_parms->ofdm_symbol_size + frame_parms->nb_prefix_samples);
        if (was_symbol_used[i]) {
          PHY_ofdm_mod((int *)txdataF + frame_parms->ofdm_symbol_size * i,
                      (int *)tx_data_ptr,
                      frame_parms->ofdm_symbol_size,
                      1,
                      frame_parms->nb_prefix_samples,
                      CYCLIC_PREFIX);
        } else {
          memset(tx_data_ptr, 0, (frame_parms->nb_prefix_samples + frame_parms->ofdm_symbol_size) * sizeof(c16_t));
        }
      }
    }
  }
  else { // numerology = 0, longer CP for every 7th symbol
      PHY_ofdm_mod((int *)txdataF,
                   (int *)txdata,
                   frame_parms->ofdm_symbol_size,
                   1,
                   frame_parms->nb_prefix_samples0,
                   CYCLIC_PREFIX);
      PHY_ofdm_mod((int *)txdataF + frame_parms->ofdm_symbol_size,
                  (int *)txdata + frame_parms->ofdm_symbol_size + frame_parms->nb_prefix_samples0,
                  frame_parms->ofdm_symbol_size,
                  6,
                  frame_parms->nb_prefix_samples,
                  CYCLIC_PREFIX);
      PHY_ofdm_mod((int *)txdataF + 7*frame_parms->ofdm_symbol_size,
                   (int *)txdata + 6*(frame_parms->ofdm_symbol_size+frame_parms->nb_prefix_samples)
                                 + frame_parms->ofdm_symbol_size + frame_parms->nb_prefix_samples0,
                   frame_parms->ofdm_symbol_size,
                   1,
                   frame_parms->nb_prefix_samples0,
                   CYCLIC_PREFIX);
      PHY_ofdm_mod((int *)txdataF + 8 * frame_parms->ofdm_symbol_size,
                   (int *)txdata + 6 * (frame_parms->ofdm_symbol_size + frame_parms->nb_prefix_samples)
                                 + 2*(frame_parms->ofdm_symbol_size + frame_parms->nb_prefix_samples0),
                   frame_parms->ofdm_symbol_size,
                   6,
                   frame_parms->nb_prefix_samples,
                   CYCLIC_PREFIX);
  }

}

void PHY_ofdm_mod(const int *input, /// pointer to complex input
                  int *output, /// pointer to complex output
                  int fftsize, /// FFT_SIZE
                  unsigned char nb_symbols, /// number of OFDM symbols
                  unsigned short nb_prefix_samples, /// cyclic prefix length
                  Extension_t etype /// type of extension
)
{
  if (nb_symbols == 0)
    return;

  idft_size_idx_t idft_size = get_idft(fftsize);

#ifdef DEBUG_OFDM_MOD
  printf("[PHY] OFDM mod (size %d,prefix %d) Symbols %d, input %p, output %p\n",
         fftsize,
         nb_prefix_samples,
         nb_symbols,
         input,
         output);
#endif

  for (int i = 0; i < nb_symbols; i++) {
#ifdef DEBUG_OFDM_MOD
    printf("[PHY] symbol %d/%d offset %d (%p,%p -> %p)\n",
           i,
           nb_symbols,
           i * fftsize + (i * nb_prefix_samples),
           input,
           &input[i * fftsize],
           &output[(i * fftsize) + ((i)*nb_prefix_samples)]);
#endif

    // on AVX2 need 256-bit alignment

    // Copy to frame buffer with Cyclic Extension
    // Note:  will have to adjust for synchronization offset!

    switch (etype) {
      case CYCLIC_PREFIX: {
        int *output_ptr = &output[(i * fftsize) + ((1 + i) * nb_prefix_samples)];
        // Current idft implementation uses AVX-256: Check if buffer is already aligned to 256 bits (32 bytes)
        if ((uintptr_t)output_ptr % 32 == 0) {
          // output ptr is aligned, do ifft inplace
          idft(idft_size, (int16_t *)&input[i * fftsize], (int16_t *)output_ptr, 1);
        } else {
          // output ptr is not aligned, needs an extra memcpy
          c16_t temp[fftsize] __attribute__((aligned(IDFT_OUTPUT_BUFFER_ALIGNMENT)));
          idft(idft_size, (int16_t *)&input[i * fftsize], (int16_t *)temp, 1);
          memcpy((void *)output_ptr, (void *)temp, sizeof(temp));
        }
        // perform cyclic prefix insertion
        memcpy((void *)&output_ptr[-nb_prefix_samples], (void *)&output_ptr[fftsize - nb_prefix_samples], nb_prefix_samples * sizeof(c16_t));
        break;
      }

      case CYCLIC_SUFFIX: {
        // Use alignment of 64 bytes
        c16_t temp[fftsize] __attribute__((aligned(IDFT_OUTPUT_BUFFER_ALIGNMENT)));
        idft(idft_size, (int16_t *)&input[i * fftsize], (int16_t *)temp, 1);
        int *output_ptr = &output[(i * fftsize) + (i * nb_prefix_samples)];
        memcpy(output_ptr, temp, sizeof(temp));
        memcpy(&output_ptr[fftsize], temp, nb_prefix_samples * sizeof(c16_t));
        break;
      }

      case ZEROS:

        break;

      case NONE: {
        c16_t temp[fftsize] __attribute__((aligned(IDFT_OUTPUT_BUFFER_ALIGNMENT)));
        idft(idft_size, (int16_t *)&input[i * fftsize], (int16_t *)temp, 1);
        int *output_ptr = &output[i * fftsize];
        memcpy(output_ptr, temp, sizeof(temp));
        break;
      }

      default:
        break;
    }
  }
}

void do_OFDM_mod(c16_t **txdataF, c16_t **txdata, uint32_t frame,uint16_t next_slot, LTE_DL_FRAME_PARMS *frame_parms)
{

  int aa, slot_offset, slot_offset_F;

  slot_offset_F = (next_slot)*(frame_parms->ofdm_symbol_size)*((frame_parms->Ncp==1) ? 6 : 7);
  slot_offset = (next_slot)*(frame_parms->samples_per_tti>>1);

  for (aa=0; aa<frame_parms->nb_antennas_tx; aa++) {
    if (is_pmch_subframe(frame,next_slot>>1,frame_parms)) {
      if ((next_slot%2)==0) {
        LOG_D(PHY,"Frame %d, subframe %d: Doing MBSFN modulation (slot_offset %d)\n",frame,next_slot>>1,slot_offset);
        PHY_ofdm_mod((int *)&txdataF[aa][slot_offset_F],        // input
                     (int *)&txdata[aa][slot_offset],         // output
                     frame_parms->ofdm_symbol_size,                
                     12,                 // number of symbols
                     frame_parms->ofdm_symbol_size>>2,               // number of prefix samples
                     CYCLIC_PREFIX);

        if (frame_parms->Ncp == EXTENDED)
          PHY_ofdm_mod((int *)&txdataF[aa][slot_offset_F],        // input
                       (int *)&txdata[aa][slot_offset],         // output
                       frame_parms->ofdm_symbol_size,                
                       2,                 // number of symbols
                       frame_parms->nb_prefix_samples,               // number of prefix samples
                       CYCLIC_PREFIX);
        else {
          LOG_D(PHY,"Frame %d, subframe %d: Doing PDCCH modulation\n",frame,next_slot>>1);
          normal_prefix_mod((int32_t *)&txdataF[aa][slot_offset_F],
                            (int32_t *)&txdata[aa][slot_offset],
                            2,
                            frame_parms);
        }
      }
    } else {
      if (frame_parms->Ncp == EXTENDED)
        PHY_ofdm_mod((int *)&txdataF[aa][slot_offset_F],        // input
                     (int *)&txdata[aa][slot_offset],         // output
                     frame_parms->ofdm_symbol_size,                
                     6,                 // number of symbols
                     frame_parms->nb_prefix_samples,               // number of prefix samples
                     CYCLIC_PREFIX);
      else {
        normal_prefix_mod((int32_t *)&txdataF[aa][slot_offset_F],
                          (int32_t *)&txdata[aa][slot_offset],
                          7,
                          frame_parms);
      }
    }
  }

}

void apply_nr_rotation_TX(const NR_DL_FRAME_PARMS *fp,
                          c16_t *txdataF,
                          const c16_t *symbol_rotation,
                          int slot,
                          int nb_rb,
                          int first_symbol,
                          int nsymb)
{
  int symb_offset = (slot % fp->slots_per_subframe) * fp->symbols_per_slot;

  symbol_rotation += symb_offset;

  for (int sidx = first_symbol; sidx < first_symbol + nsymb; sidx++) {
    const c16_t *this_rotation = symbol_rotation + sidx;
    c16_t *this_symbol = (txdataF) + sidx * fp->ofdm_symbol_size;

    LOG_D(PHY,"Rotating symbol %d, slot %d, symbol_subframe_index %d (%d,%d)\n",
      sidx,
      slot,
      sidx + symb_offset,
      this_rotation->r,
      this_rotation->i);

    if (nb_rb & 1) {
      rotate_cpx_vector(this_symbol, this_rotation, this_symbol,
                        (nb_rb + 1) * 6, 15);
      rotate_cpx_vector(this_symbol + fp->first_carrier_offset - 6,
                        this_rotation,
                        this_symbol + fp->first_carrier_offset - 6,
                        (nb_rb + 1) * 6, 15);
    } else {
      rotate_cpx_vector(this_symbol, this_rotation, this_symbol,
                        nb_rb * 6, 15);
      rotate_cpx_vector(this_symbol + fp->first_carrier_offset,
                        this_rotation,
                        this_symbol + fp->first_carrier_offset,
                        nb_rb * 6, 15);
    }
  }
}
                       
