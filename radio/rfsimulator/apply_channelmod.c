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
* Author and copyright: Laurent Thomas, open-cells.com
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


#include <complex.h>
#include <common/utils/LOG/log.h>
#include <openair1/SIMULATION/TOOLS/sim.h>
#include "rfsimulator.h"

/*
  Legacy study:
  The parameters are:
  gain&loss (decay, signal power, ...)
  either a fixed gain in dB, a target power in dBm or ACG (automatic control gain) to a target average
  => don't redo the AGC, as it was used in UE case, that must have a AGC inside the UE
  will be better to handle the "set_gain()" called by UE to apply it's gain (enable test of UE power loop)
  lin_amp = pow(10.0,.05*txpwr_dBm)/sqrt(nb_tx_antennas);
  a lot of operations in legacy, grouped in one simulation signal decay: txgain*decay*rxgain

  multi_path (auto convolution, ISI, ...)
  either we regenerate the channel (call again random_channel(desc,0)), or we keep it over subframes
  legacy: we regenerate each sub frame in UL, and each frame only in DL
*/
void rxAddInput(const c16_t *input_sig,
                c16_t *after_channel_sig,
                int rxAnt,
                channel_desc_t *channelDesc,
                int nbSamples,
                uint64_t TS,
                uint32_t CirSize)
{
  if ((channelDesc->sat_height > 0) && (channelDesc->enable_dynamic_delay || channelDesc->enable_dynamic_Doppler)) { // model for transparent satellite on circular orbit
    /* assumptions:
       - The Earth is spherical, the ground station is static, and that the Earth does not rotate.
       - An access or link is possible from the satellite to the ground station at all times.
       - The ground station is located at the North Pole (positive Zaxis), and the satellite starts from the initial elevation angle 0° in the second quadrant of the YZplane.
       - Satellite moves in the clockwise direction in its circular orbit.
    */
    const double radius_earth = 6371e3; // m
    const double radius_sat = radius_earth + channelDesc->sat_height;
    const double GM_earth = 3.986e14; // m^3/s^2
    const double w_sat = sqrt(GM_earth / (radius_sat * radius_sat * radius_sat)); // rad/s

    // start_time and end_time are when the pos_sat_z == pos_gnb_z (elevation angle == 0 and 180 degree)
    const double start_phase = -acos(radius_earth / radius_sat); // SAT is just rising above the horizon
    const double end_phase = -start_phase; // SAT is just falling behind the horizon
    const double start_time = start_phase / w_sat; // in seconds
    const double end_time = end_phase / w_sat; // in seconds
    const uint64_t duration_samples = (end_time - start_time) * channelDesc->sampling_rate;
    const double t = start_time + ((TS - channelDesc->start_TS) % duration_samples) / channelDesc->sampling_rate;

    const double pos_sat_x = 0;
    const double pos_sat_y = radius_sat * sin(w_sat * t);
    const double pos_sat_z = radius_sat * cos(w_sat * t);

    const double vel_sat_x = 0;
    const double vel_sat_y =  w_sat * radius_sat * cos(w_sat * t);
    const double vel_sat_z = -w_sat * radius_sat * sin(w_sat * t);

    const double pos_ue_x = 0;
    const double pos_ue_y = 0;
    const double pos_ue_z = radius_earth;

    const double dir_sat_ue_x = pos_ue_x - pos_sat_x;
    const double dir_sat_ue_y = pos_ue_y - pos_sat_y;
    const double dir_sat_ue_z = pos_ue_z - pos_sat_z;

    const double dist_sat_ue = sqrt(dir_sat_ue_x * dir_sat_ue_x + dir_sat_ue_y * dir_sat_ue_y + dir_sat_ue_z * dir_sat_ue_z);
    const double vel_sat_ue = (vel_sat_x * dir_sat_ue_x + vel_sat_y * dir_sat_ue_y + vel_sat_z * dir_sat_ue_z) / dist_sat_ue;

    double dist_gnb_sat = 0;
    double vel_gnb_sat = 0;
    if (channelDesc->modelid == SAT_LEO_TRANS) {
      const double pos_gnb_x = 0;
      const double pos_gnb_y = 0;
      const double pos_gnb_z = radius_earth;

      const double dir_gnb_sat_x = pos_sat_x - pos_gnb_x;
      const double dir_gnb_sat_y = pos_sat_y - pos_gnb_y;
      const double dir_gnb_sat_z = pos_sat_z - pos_gnb_z;

      dist_gnb_sat = sqrt(dir_gnb_sat_x * dir_gnb_sat_x + dir_gnb_sat_y * dir_gnb_sat_y + dir_gnb_sat_z * dir_gnb_sat_z);
      vel_gnb_sat = (vel_sat_x * dir_gnb_sat_x + vel_sat_y * dir_gnb_sat_y + vel_sat_z * dir_gnb_sat_z) / dist_gnb_sat;
    }

    const double c = 299792458; // m/s
    const double prop_delay = (dist_gnb_sat + dist_sat_ue) / c;
    if (channelDesc->enable_dynamic_delay)
      channelDesc->channel_offset = prop_delay * channelDesc->sampling_rate;

    const double f_Doppler_shift_sat_ue = (vel_sat_ue / (c - vel_sat_ue)) * channelDesc->center_freq;
    const double f_Doppler_shift_gnb_sat = (-vel_gnb_sat / c) * channelDesc->center_freq;
    if (channelDesc->enable_dynamic_Doppler)
      channelDesc->Doppler_phase_inc = 2 * M_PI * (f_Doppler_shift_gnb_sat + f_Doppler_shift_sat_ue) / channelDesc->sampling_rate;

    static uint64_t last_TS = 0;
    if(TS - last_TS >= channelDesc->sampling_rate) {
      last_TS = TS;
      LOG_I(HW, "Satellite orbit: time %f s, Doppler: gNB->SAT %f kHz, SAT->UE %f kHz, Delay %f ms\n", t, f_Doppler_shift_gnb_sat / 1000, f_Doppler_shift_sat_ue / 1000, prop_delay * 1000);
    }
  }

  // channelDesc->path_loss_dB should contain the total path gain
  // so, in actual RF: tx gain + path loss + rx gain (+antenna gain, ...)
  // UE and NB gain control to be added
  // Fixme: not sure when it is "volts" so dB is 20*log10(...) or "power", so dB is 10*log10(...)
  const double pathLossLinear = pow(10,channelDesc->path_loss_dB/20.0);
  // Energy in one sample to calibrate input noise
  // the normalized OAI value seems to be 256 as average amplitude (numerical amplification = 1)
  const double noise_per_sample = pow(10,channelDesc->noise_power_dB/10.0) * 256;
  const uint64_t dd = channelDesc->channel_offset;
  const int nbTx=channelDesc->nb_tx;

  for (int i=0; i<nbSamples; i++) {
    struct complex16 *out_ptr=after_channel_sig+i;
    struct complexd rx_tmp= {0};

    for (int txAnt=0; txAnt < nbTx; txAnt++) {
      const struct complexd *channelModel= channelDesc->ch[rxAnt+(txAnt*channelDesc->nb_rx)];

      //const struct complex *channelModelEnd=channelModel+channelDesc->channel_length;
      for (int l = 0; l<(int)channelDesc->channel_length; l++) {
        // let's assume TS+i >= l
        // fixme: the rfsimulator current structure is interleaved antennas
        // this has been designed to not have to wait a full block transmission
        // but it is not very usefull
        // it would be better to split out each antenna in a separate flow
        // that will allow to mix ru antennas freely
        // (X + cirSize) % cirSize to ensure that index is positive
        const int idx = ((TS + i - l - dd) * nbTx + txAnt + CirSize) % CirSize;
        const struct complex16 tx16 = input_sig[idx];
        rx_tmp.r += tx16.r * channelModel[l].r - tx16.i * channelModel[l].i;
        rx_tmp.i += tx16.i * channelModel[l].r + tx16.r * channelModel[l].i;
      } //l
    }

    if (channelDesc->Doppler_phase_inc != 0.0) {
#ifdef CMPLX
      double complex in = CMPLX(rx_tmp.r, rx_tmp.i);
#else
      double complex in = rx_tmp.r + rx_tmp.i * I;
#endif
      double complex out = in * cexp(channelDesc->Doppler_phase_cur[rxAnt] * I);
      rx_tmp.r = creal(out);
      rx_tmp.i = cimag(out);
      channelDesc->Doppler_phase_cur[rxAnt] += channelDesc->Doppler_phase_inc;
    }

    out_ptr->r = lround(rx_tmp.r*pathLossLinear + noise_per_sample*gaussZiggurat(0.0,1.0));
    out_ptr->i = lround(rx_tmp.i*pathLossLinear + noise_per_sample*gaussZiggurat(0.0,1.0));
    out_ptr++;
  }

  if ( (TS*nbTx)%CirSize+nbSamples <= CirSize )
    // Cast to a wrong type for compatibility !
    LOG_D(HW,"Input power %f, output power: %f, channel path loss %f, noise coeff: %f \n",
          10*log10((double)signal_energy((int32_t *)&input_sig[(TS*nbTx)%CirSize], nbSamples)),
          10*log10((double)signal_energy((int32_t *)after_channel_sig, nbSamples)),
          channelDesc->path_loss_dB,
          10*log10(noise_per_sample));
}

