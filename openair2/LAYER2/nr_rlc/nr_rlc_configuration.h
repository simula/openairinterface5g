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

#ifndef _NR_RLC_CONFIGURATION_H_
#define _NR_RLC_CONFIGURATION_H_

#ifndef INT_LIST_SIZE
#define INT_LIST_SIZE(x) (sizeof((int[]){x})/sizeof(int))
#endif

/* values taken from 3GPP TS 38.331 version 17.3.0 */

#define VALUES_NR_RLC_T_POLL_RETRANSMIT \
    5,    10,   15,   20,  25,  30,  35,  40,  45,  50,  55,  60,  65,  70, \
    75,   80,   85,   90,  95,  100, 105, 110, 115, 120, 125, 130, 135, 140, \
    145,  150,  155,  160, 165, 170, 175, 180, 185, 190, 195, 200, 205, 210, \
    215,  220,  225,  230, 235, 240, 245, 250, 300, 350, 400, 450, 500, 800, \
    1000, 2000, 4000, 1,   2,   4

#define VALUES_NR_RLC_T_POLL_RETRANSMIT_STR \
    "ms5", "ms10", "ms15", "ms20", "ms25", "ms30", "ms35", "ms40", "ms45", "ms50", "ms55", "ms60", "ms65", "ms70", \
    "ms75", "ms80", "ms85", "ms90", "ms95", "ms100", "ms105", "ms110", "ms115", "ms120", "ms125", "ms130", "ms135", "ms140", \
    "ms145", "ms150", "ms155", "ms160", "ms165", "ms170", "ms175", "ms180", "ms185", "ms190", "ms195", "ms200", "ms205", "ms210", \
    "ms215", "ms220", "ms225", "ms230", "ms235", "ms240", "ms245", "ms250", "ms300", "ms350", "ms400", "ms450", "ms500", "ms800", \
    "ms1000", "ms2000", "ms4000", "ms1", "ms2", "ms4"

#define SIZEOF_NR_RLC_T_POLL_RETRANSMIT INT_LIST_SIZE(VALUES_NR_RLC_T_POLL_RETRANSMIT)

#define VALUES_NR_RLC_T_REASSEMBLY \
    0,  5,  10, 15, 20,  25,  30,  35,  40,  45,  50,  55,  60,  65,  70, 75, \
    80, 85, 90, 95, 100, 110, 120, 130, 140, 150, 160, 170, 180, 190, 200

#define VALUES_NR_RLC_T_REASSEMBLY_STR \
    "ms0", "ms5", "ms10", "ms15", "ms20", "ms25", "ms30", "ms35", "ms40", "ms45", "ms50", "ms55", "ms60", "ms65", "ms70", "ms75", \
    "ms80", "ms85", "ms90", "ms95", "ms100", "ms110", "ms120", "ms130", "ms140", "ms150", "ms160", "ms170", "ms180", "ms190", "ms200"

#define SIZEOF_NR_RLC_T_REASSEMBLY INT_LIST_SIZE(VALUES_NR_RLC_T_REASSEMBLY)

#define VALUES_NR_RLC_T_STATUS_PROHIBIT \
    0,    5,   10,  15,  20,  25,  30,  35,  40,  45,   50,   55, \
    60,   65,  70,  75,  80,  85,  90,  95,  100, 105,  110,  115, \
    120,  125, 130, 135, 140, 145, 150, 155, 160, 165,  170,  175, \
    180,  185, 190, 195, 200, 205, 210, 215, 220, 225,  230,  235, \
    240,  245, 250, 300, 350, 400, 450, 500, 800, 1000, 1200, 1600, \
    2000, 2400

#define VALUES_NR_RLC_T_STATUS_PROHIBIT_STR \
    "ms0", "ms5", "ms10", "ms15", "ms20", "ms25", "ms30", "ms35", "ms40", "ms45", "ms50", "ms55", \
    "ms60", "ms65", "ms70", "ms75", "ms80", "ms85", "ms90", "ms95", "ms100", "ms105", "ms110", "ms115", \
    "ms120", "ms125", "ms130", "ms135", "ms140", "ms145", "ms150", "ms155", "ms160", "ms165", "ms170", "ms175", \
    "ms180", "ms185", "ms190", "ms195", "ms200", "ms205", "ms210", "ms215", "ms220", "ms225", "ms230", "ms235", \
    "ms240", "ms245", "ms250", "ms300", "ms350", "ms400", "ms450", "ms500", "ms800", "ms1000", "ms1200", "ms1600", \
    "ms2000", "ms2400"

#define SIZEOF_NR_RLC_T_STATUS_PROHIBIT INT_LIST_SIZE(VALUES_NR_RLC_T_STATUS_PROHIBIT)

#define VALUES_NR_RLC_POLL_PDU \
    4,     8,     16,    32,    64,    128,   256,   512,   1024, \
    2048,  4096,  6144,  8192,  12288, 16384, 20480, 24576, 28672, \
    32768, 40960, 49152, 57344, 65536, -1 /* -1 means infinity */

#define VALUES_NR_RLC_POLL_PDU_STR \
    "p4", "p8", "p16", "p32", "p64", "p128", "p256", "p512", "p1024", \
    "p2048", "p4096", "p6144", "p8192", "p12288", "p16384", "p20480", "p24576", "p28672", \
    "p32768", "p40960", "p49152", "p57344", "p65536", "infinity"

#define SIZEOF_NR_RLC_POLL_PDU INT_LIST_SIZE(VALUES_NR_RLC_POLL_PDU)

#define VALUES_NR_RLC_POLL_BYTE \
    /* KB */ \
    1024 * 1,    1024 * 2,    1024 * 5,    1024 * 8,    1024 * 10,   1024 * 15,   1024 * 25,   1024 * 50,   1024 * 75, \
    1024 * 100,  1024 * 125,  1024 * 250,  1024 * 375,  1024 * 500,  1024 * 750,  1024 * 1000, 1024 * 1250, \
    1024 * 1500, 1024 * 2000, 1024 * 3000, 1024 * 4000, 1024 * 4500, 1024 * 5000, 1024 * 5500, \
    1024 * 6000, 1024 * 6500, 1024 * 7000, 1024 * 7500, \
    /* MB */ \
    1024 * 1024 * 8,  1024 * 1024 * 9,  1024 * 1024 * 10, 1024 * 1024 * 11, 1024 * 1024 * 12, \
    1024 * 1024 * 13, 1024 * 1024 * 14, 1024 * 1024 * 15, 1024 * 1024 * 16, 1024 * 1024 * 17, \
    1024 * 1024 * 18, 1024 * 1024 * 20, 1024 * 1024 * 25, 1024 * 1024 * 30, 1024 * 1024 * 40, \
    -1 /* -1 means infinity */

#define VALUES_NR_RLC_POLL_BYTE_STR \
    "kB1", "kB2", "kB5", "kB8", "kB10", "kB15", "kB25", "kB50", "kB75", \
    "kB100", "kB125", "kB250", "kB375", "kB500", "kB750", "kB1000", "kB1250", \
    "kB1500", "kB2000", "kB3000", "kB4000", "kB4500", "kB5000", "kB5500", \
    "kB6000", "kB6500", "kB7000", "kB7500", \
    "mB8", "mB9", "mB10", "mB11", "mB12", "mB13", "mB14", "mB15", \
    "mB16", "mB17", "mB18", "mB20", "mB25", "mB30", "mB40", \
    "infinity"

#define SIZEOF_NR_RLC_POLL_BYTE INT_LIST_SIZE(VALUES_NR_RLC_POLL_BYTE)

#define VALUES_NR_RLC_MAX_RETX_THRESHOLD \
    1, 2, 3, 4, 6, 8, 16, 32

#define VALUES_NR_RLC_MAX_RETX_THRESHOLD_STR \
    "t1", "t2", "t3", "t4", "t6", "t8", "t16", "t32"

#define SIZEOF_NR_RLC_MAX_RETX_THRESHOLD INT_LIST_SIZE(VALUES_NR_RLC_MAX_RETX_THRESHOLD)

#define VALUES_NR_RLC_SN_FIELD_LENGTH_AM \
  12, 18

#define VALUES_NR_RLC_SN_FIELD_LENGTH_AM_STR \
 "size12", "size18"

#define SIZEOF_NR_RLC_SN_FIELD_LENGTH_AM INT_LIST_SIZE(VALUES_NR_RLC_SN_FIELD_LENGTH_AM)

#define VALUES_NR_RLC_SN_FIELD_LENGTH_UM \
  6, 12

#define VALUES_NR_RLC_SN_FIELD_LENGTH_UM_STR \
 "size6", "size12"

#define SIZEOF_NR_RLC_SN_FIELD_LENGTH_UM INT_LIST_SIZE(VALUES_NR_RLC_SN_FIELD_LENGTH_UM)

/* configuration of NR RLC, coming from configuration file */
typedef struct {
  struct {
    int t_poll_retransmit;
    int t_reassembly;
    int t_status_prohibit;
    int poll_pdu;
    int poll_byte;
    int max_retx_threshold;
    int sn_field_length;
  } srb;
  struct {
    int t_poll_retransmit;
    int t_reassembly;
    int t_status_prohibit;
    int poll_pdu;
    int poll_byte;
    int max_retx_threshold;
    int sn_field_length;
  } drb_am;
  struct {
    int t_reassembly;
    int sn_field_length;
  } drb_um;
} nr_rlc_configuration_t;

#endif /* _NR_RLC_CONFIGURATION_H_ */
