#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "PHY/defs_common.h"

int estimate_DLCSI_from_ULCSI(int32_t **calib_dl_ch_estimates, int32_t **ul_ch_estimates, int32_t **tdd_calib_coeffs, int nb_ant, int nb_freq) {

  /* TODO: what to return? is this code used at all? */
  return 0;

}

int compute_BF_weights(int32_t **beam_weights, int32_t **calib_dl_ch_estimates, PRECODE_TYPE_t precode_type, int nb_ant, int nb_freq) {
  switch (precode_type) {
  //case MRT
  case 0 :
  //case ZF
  break;
  case 1 :
  //case MMSE
  break;
  case 2 :
  break;
  default :
  break;  
}
  /* TODO: what to return? is this code used at all? */
  return 0;
} 

// temporal test function
/*
void main(){
  // initialization
  // compare
  printf("Hello world!\n");
}
*/
