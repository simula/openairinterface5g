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

/*! \file common/utils/T/tracer/t_tracer_app_gnb.c
 * \brief T-Tracer gnb service to capture tracee Messages from gNB, it is used by data recording application
 * \author Abdo Gaber
 * \date 2025
 * \version 1.0
 * \company Emerson, NI Test and Measurement
 * \email:
 * \note
 * \warning
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include "database.h"
#include "event.h"
#include "handler.h"
#include "logger/logger.h"
#include "utils.h"
#include "event_selector.h"
#include "configuration.h"
#include "shared_memory_config.h"
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <poll.h>
#define MAX_FRAME_INDEX 1023
#define MAX_SLOT_INDEX 19
#define DEBUG_T_Tracer
#define DEBUG_BUFFER
// Duration to discard recording in milliseconds to mitigate reading data from
// the buffer in the stack from the previous record
#define DISCARD_RECORD_DURATION_MS 10

// Combine bytes in - little-endian format
int combine_bytes(const uint8_t *bytes, size_t num_bytes)
{
  int result = 0;
  for (size_t i = 0; i < num_bytes; ++i) {
    result |= (bytes[i] << (i * 8));
  }
  return result;
}

// Convert an integer to an array of bytes - little-endian format
void int_to_bytes(int num, uint8_t *bytes, size_t num_bytes)
{
  for (size_t i = 0; i < num_bytes; ++i) {
    bytes[i] = (num >> (i * 8)) & 0xFF;
  }
}

// Check if the message is in the list of bits messages
bool is_bits_messages(int traces_bits_support_data_Collection_format_idx[], int n_bits_msgs, int msg_id)
{
  for (int i = 0; i < n_bits_msgs; i++) {
    if (msg_id == traces_bits_support_data_Collection_format_idx[i]) {
      return true;
    }
  }
  return false;
}

// Get the current time
struct timespec get_current_time()
{
  struct timespec time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  return time;
}

// Function to convert timespec to microseconds
long long timespec_to_microseconds(struct timespec time)
{
  return (time.tv_sec * 1000000LL) + (time.tv_nsec / 1000);
}

// Calculate the time difference in milliseconds
double calculate_time_difference(struct timespec start, struct timespec end)
{
  double start_ms = start.tv_sec * 1000.0 + start.tv_nsec / 1000000.0;
  double end_ms = end.tv_sec * 1000.0 + end.tv_nsec / 1000000.0;
  return end_ms - start_ms;
}

// Get Time Stamp in microseconds in YYYYMMDDHHMMSSmmmuuu format
char *get_time_stamp_usec(char time_stamp_str[])
{
  // initialization to measure time stamp --This part should be moved to inilization part
  time_t my_time;
  struct tm *timeinfo;
  time(&my_time);
  struct timeval tv;

  // get time stamp
  timeinfo = localtime(&my_time);
  gettimeofday(&tv, NULL);
  // Add time stamp: yyyy mm dd hh mm ss usec
  uint16_t year = timeinfo->tm_year + 1900;
  uint8_t mon = timeinfo->tm_mon + 1;
  uint8_t mday = timeinfo->tm_mday;
  uint8_t hour = timeinfo->tm_hour;
  uint8_t min = timeinfo->tm_min;
  uint8_t sec = timeinfo->tm_sec;
  uint16_t usec = (tv.tv_usec);
  // printf ("Time stamp: %d_%d_%d_%d_%d_%d_%d \n",year,mon,mday,hour,min,sec,usec);
  // sprintf(time_stamp_str, "%d_%d_%d_%d_%d_%d_%d",year,mon,mday,hour,min,sec,usec);
  sprintf(time_stamp_str, "%04d%02d%02d%02d%02d%02d%06d", year, mon, mday, hour, min, sec, usec);

  return time_stamp_str;
}

// Convert timestamp string to integer
int convert_time_stamp_to_int(const char *timestamp)
{
  return atoi(timestamp);
}

// Split timestamp string and convert to integer
int split_time_stamp_and_convert_to_int(char time_stamp_str[], int shift, int length)
{
  char time_part[length + 1]; // Buffer to hold the date part YYYYMMDD or HHMMSSmmm
  // Copy the first 8 or 9 characters (YYYYMMDD) to HHMMSSmmm
  strncpy(time_part, time_stamp_str + shift, length);
  time_part[length] = '\0'; // Null-terminate the string
  // Convert timestamp string to integer
  return convert_time_stamp_to_int(time_part);
}

void err_exit(char *buf)
{
  fprintf(stderr, "%s\n", buf);
  exit(1);
}

// create shared memory
int create_shm(char **addrN, const char *shm_path, int projectId)
{
  key_t key;
  if (-1 != open(shm_path, O_CREAT, 0777)) {
    key = ftok(shm_path, projectId);
  } else {
    err_exit("Error: open shared memory");
  }

  if (key < 0) {
    err_exit("Error: ftok error");
  }

  int shm_id;
  shm_id = shmget(key, SHMSIZE, IPC_CREAT | IPC_EXCL | 0664);
  if (shm_id == -1) {
    if (errno == EEXIST) {
      printf("Error: shared memeory already exist\n");
      shm_id = shmget(key, 0, 0);
      printf("reference shm_id = %d\n", shm_id);
    } else {
      perror("errno");
      err_exit("shmget error");
    }
  }
  char *addr;
  // address to attach - attach for read & write
  if ((addr = shmat(shm_id, 0, 0)) == (void *)-1) {
    if (shmctl(shm_id, IPC_RMID, NULL) == -1)
      err_exit("Error: shmctl error");
    else {
      printf("Attach shared memory failed\n");
      printf("remove shared memory identifier successful\n");
    }
    err_exit("shmat error");
  }
  *addrN = addr;
  return shm_id;
}

// delete shared memory
void del_shm(char *addr, int shm_id)
{
  if (shmdt(addr) < 0)
    err_exit("shmdt error");

  if (shmctl(shm_id, IPC_RMID, NULL) == -1)
    err_exit("shmctl error");
  else {
    printf("Remove shared memory identifier successful\n");
  }
}

/* this function sends the activated traces to the nr-softmodem */
void activate_traces(int socket, int number_of_events, int *is_on)
{
  char t = 1;
  if (socket_send(socket, &t, 1) == -1 || socket_send(socket, &number_of_events, sizeof(int)) == -1
      || socket_send(socket, is_on, number_of_events * sizeof(int)) == -1)
    abort();
}

void usage(void)
{
  printf(
      "options:\n"
      "    -d <database file>        this option is mandatory\n"
      "    -ip <host>                connect to given IP address (default %s)\n"
      "    -p <port>                 connect to given port (default %d)\n",
      DEFAULT_REMOTE_IP,
      DEFAULT_REMOTE_PORT);
  exit(1);
}

// class to store the message and the action to enable capture selected data
// 0: do not record message
// 1: record message
struct trace_struct {
  char on_off_name[100];
  int on_off_action;
};

// struct for trace message based on Data Collection Trace Messages Structure
// you need to define the vararibles of each message
typedef struct {
  /* Data Collection Trace Message Structure  */
  int frame;
  int slot;
  int datetime_yyyymmdd;
  int datetime_hhmmssmmm;
  int frame_type, freq_range, subcarrier_spacing, cyclic_prefix, symbols_per_slot;
  int Nid_cell, rnti;
  int rb_size, rb_start, start_symbol_index, nr_of_symbols;
  int qam_mod_order, mcs_index, mcs_table, nrOfLayers, transform_precoding;
  int dmrs_config_type, ul_dmrs_symb_pos, number_dmrs_symbols;
  int dmrs_port, dmrs_nscid, nb_antennas_rx, number_of_bits;
  int data_size, data;
} event_trace_msg_data;

void setup_trace_msg_data(event_trace_msg_data *d, void *database)
{
  database_event_format f;
  int i;

  /* Data Collection Trace Message Structure */
  // FORMAT = int,frame : int,slot : int,datetime_yyyymmdd : int,datetime_hhmmssmmm :
  // int,frame_type : int,freq_range : int,subcarrier_spacing : int,cyclic_prefix : int,symbols_per_slot :
  // int,Nid_cell : int,rnti :
  // int,rb_size : int,rb_start : int,start_symbol_index : int,nr_of_symbols :
  // int,qam_mod_order : int,mcs_index : int,mcs_table : int,nrOfLayers :
  // int,transform_precoding : int,dmrs_config_type : int,ul_dmrs_symb_pos :  int,number_dmrs_symbols :
  // int,dmrs_port : int,dmrs_nscid :
  // int,nb_antennas_rx : int,number_of_bits : buffer,data

  // Initialize the data structure
  d->frame = -1;
  d->slot = -1;
  d->datetime_yyyymmdd = -1;
  d->datetime_hhmmssmmm = -1;
  d->frame_type = -1;
  d->freq_range = -1;
  d->subcarrier_spacing = -1;
  d->cyclic_prefix = -1;
  d->symbols_per_slot = -1;
  d->Nid_cell = -1;
  d->rnti = -1;
  d->rb_size = -1;
  d->rb_start = -1;
  d->start_symbol_index = -1;
  d->nr_of_symbols = -1;
  d->qam_mod_order = -1;
  d->mcs_index = -1;
  d->mcs_table = -1;
  d->nrOfLayers = -1;
  d->transform_precoding = -1;
  d->dmrs_config_type = -1;
  d->ul_dmrs_symb_pos = -1;
  d->number_dmrs_symbols = -1;
  d->dmrs_port = -1;
  d->dmrs_nscid = -1;
  d->nb_antennas_rx = -1;
  d->number_of_bits = -1;
  d->data = -1;

/* this macro looks for a particular element and checks its type */
#define G(var_name, var_type, var)                                      \
  if (!strcmp(f.name[i], var_name)) {                                   \
    if (strcmp(f.type[i], var_type)) {                                  \
      printf("Error: Trace Message has a bad type for %s\n", var_name); \
      exit(1);                                                          \
    }                                                                   \
    var = i;                                                            \
    continue;                                                           \
  }

  /* ------------------------------*/
  /* Create Macro for Data Collection Trace Message */
  /* ------------------------------*/
  // Data Collection Trace Message Structure
  // Example: GNB_PHY_UL_FD_PUSCH_IQ,  GNB_PHY_UL_FD_DMRS_ID,
  //          GNB_PHY_UL_FD_CHAN_EST_DMRS_POS, GNB_PHY_UL_FD_CHAN_EST_DMRS_INTERPL
  //          GNB_PHY_UL_PAYLOAD_RX_BITS
  //          UE_PHY_UL_SCRAMBLED_TX_BITS
  //          UE_PHY_UL_PAYLOAD_TX_BITS
  // Get a template of any message based on Data Collection Trace Messages Structure
  int Trace_MSG_ID = event_id_from_name(database, "GNB_PHY_UL_FD_PUSCH_IQ");
  f = get_format(database, Trace_MSG_ID);

  /* get the elements of the trace
   * the value is an index in the event, see below */
  // FORMAT = int,frame : int,slot : int,datetime_yyyymmdd : int,datetime_hhmmssmmm :
  // int,frame_type : int,freq_range : int,subcarrier_spacing : int,cyclic_prefix : int,symbols_per_slot :
  // int,Nid_cell : int,rnti :
  // int,rb_size : int,rb_start : int,start_symbol_index : int,nr_of_symbols :
  // int,qam_mod_order : int,mcs_index : int,mcs_table : int,nrOfLayers :
  // int,transform_precoding : int,dmrs_config_type : int,ul_dmrs_symb_pos :  int,number_dmrs_symbols :
  // int,dmrs_port : int,dmrs_nscid :
  // int,nb_antennas_rx : int,number_of_bits : buffer,data

  for (i = 0; i < f.count; i++) {
    G("frame", "int", d->frame)
    G("slot", "int", d->slot)
    G("datetime_yyyymmdd", "int", d->datetime_yyyymmdd)
    G("datetime_hhmmssmmm", "int", d->datetime_hhmmssmmm)
    G("frame_type", "int", d->frame_type)
    G("freq_range", "int", d->freq_range)
    G("subcarrier_spacing", "int", d->subcarrier_spacing)
    G("cyclic_prefix", "int", d->cyclic_prefix)
    G("symbols_per_slot", "int", d->symbols_per_slot)
    G("Nid_cell", "int", d->Nid_cell)
    G("rnti", "int", d->rnti)
    G("rb_size", "int", d->rb_size)
    G("rb_start", "int", d->rb_start)
    G("start_symbol_index", "int", d->start_symbol_index)
    G("nr_of_symbols", "int", d->nr_of_symbols)
    G("qam_mod_order", "int", d->qam_mod_order)
    G("mcs_index", "int", d->mcs_index)
    G("mcs_table", "int", d->mcs_table)
    G("nrOfLayers", "int", d->nrOfLayers)
    G("transform_precoding", "int", d->transform_precoding)
    G("dmrs_config_type", "int", d->dmrs_config_type)
    G("ul_dmrs_symb_pos", "int", d->ul_dmrs_symb_pos)
    G("number_dmrs_symbols", "int", d->number_dmrs_symbols)
    G("dmrs_port", "int", d->dmrs_port)
    G("dmrs_nscid", "int", d->dmrs_nscid)
    G("nb_antennas_rx", "int", d->nb_antennas_rx)
    G("number_of_bits", "int", d->number_of_bits)
    G("data", "buffer", d->data)
  }
  // if (d->frame == -1 || d->slot == -1) goto error;
#undef G
  return;
}

// Function to check if a value is in the array
int isValueInArray(int value, int arr[], int size)
{
  for (int i = 0; i < size; i++) {
    if (arr[i] == value) {
      return 1; // Value found
    }
  }
  return 0; // Value not found
}

void reestablish_connection(int *socket, char *ip, int port, int number_of_events, int *is_on)
{
  clear_remote_config();
  if (*socket != -1)
    close(*socket);

  /* connect to the nr-softmodem */
  *socket = connect_to(ip, port);
  if (*socket == -1) {
    printf("\n Failed to connect to nr-softmodem. Retrying...\n");
    return;
  }
  printf("\n Connected");

  /* activate the traces in the nr-softmodem */
  activate_traces(*socket, number_of_events, is_on);
  printf("\n Activated Traces in nr-softmodem");
}

int main(int n, char **v)
{
  // Define and initialize an array of strings to list
  // trace_msgs_support_data_Collection_format
  // it is used to parse the requested messages if it is based
  // on Data Collection Trace Messages Structure and supported tracer messages indices
  char *traces_iq_support_data_Collection_format[] = {"GNB_PHY_UL_FD_PUSCH_IQ",
                                                            "GNB_PHY_UL_FD_DMRS",
                                                            "GNB_PHY_UL_FD_CHAN_EST_DMRS_POS",
                                                            "GNB_PHY_UL_FD_CHAN_EST_DMRS_INTERPL"};
  char *traces_bits_support_data_Collection_format[] = {"GNB_PHY_UL_PAYLOAD_RX_BITS",
                                                              "UE_PHY_UL_SCRAMBLED_TX_BITS",
                                                              "UE_PHY_UL_PAYLOAD_TX_BITS"};
  // extra number of records to simlify Sync between base station and UE synchronization records.
  // if you have network delay, you can increase the number of records to capture
  int max_sync_offset = 6; // 6 frames ~ 60 ms
  // all supported messages
  // Calculate the size of the combined array
  int n_iq_msgs = sizeof(traces_iq_support_data_Collection_format) / sizeof(traces_iq_support_data_Collection_format[0]);
  int n_bits_msgs = sizeof(traces_bits_support_data_Collection_format) / sizeof(traces_bits_support_data_Collection_format[0]);
  int n_msgs_based_data_Collection_format = n_iq_msgs + n_bits_msgs;

  // Create the combined array
  char *traces_support_data_Collection_format[n_msgs_based_data_Collection_format];
  // Copy IQ messages to the combined array
  for (int i = 0; i < n_iq_msgs; i++) {
    traces_support_data_Collection_format[i] = traces_iq_support_data_Collection_format[i];
  }

  // Copy Bits messages to the combined array
  for (int i = 0; i < n_bits_msgs; i++) {
    traces_support_data_Collection_format[i + n_iq_msgs] = traces_bits_support_data_Collection_format[i];
  }

  uint16_t msg_id = 0;
  uint16_t start_frame_number = 0;
  uint32_t number_records = 0; // number of records to capture, it is number of slots
  // array to store the requested tracer messages indices
  int req_tracer_msgs_indices[100] = {0};
  // define variables --> to do: add all of them to be class of pointers
  char *database_filename = NULL;
  void *database;
  char *ip = DEFAULT_REMOTE_IP;
  char ip_address[16]; // max IP address length is 15 + 1 for null terminator
  int port = DEFAULT_REMOTE_PORT;
  int *is_on;
  int number_of_events;
  int i;
  int socket = -1;
  char trace_time_stamp_str[30];

  // data structure for the trace messages based on Data Collection Trace Messages Structure
  event_trace_msg_data trace_msg_data;

  // initlization variables
  unsigned int bufIdx_wr = 0;
  unsigned int bufIdx_rd = 0;
  uint8_t num_req_tracer_msgs = 0;

  // initilaze shared memory
  char *addr_wr, *addr_rd;
  printf("\n Data Collection Service: Initializing shared memory ...");
  printf("\n Directory 1: %s, Directory 2: %s", GETKEYDIR1_gNB, GETKEYDIR2_gNB);
  printf("\n Project ID: %d\n", PROJECTID_gNB);
  int shm_id_wr = create_shm(&addr_wr, GETKEYDIR1_gNB, PROJECTID_gNB);
  int shm_id_rd = create_shm(&addr_rd, GETKEYDIR2_gNB, PROJECTID_gNB);
  del_shm(addr_wr, shm_id_wr);
  del_shm(addr_rd, shm_id_rd);
  shm_id_wr = create_shm(&addr_wr, GETKEYDIR1_gNB, PROJECTID_gNB);
  shm_id_rd = create_shm(&addr_rd, GETKEYDIR2_gNB, PROJECTID_gNB);

  /* write on a socket fails if the other end is closed and we get SIGPIPE */
  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    abort();

  /* parse command line options */
  // Port number and IP address are given via API and not via command line
  for (i = 1; i < n; i++) {
    if (!strcmp(v[i], "-h") || !strcmp(v[i], "--help"))
      usage();
    if (!strcmp(v[i], "-d")) {
      if (i > n - 2)
        usage();
      database_filename = v[++i];
      continue;
    }
    if (!strcmp(v[i], "-ip")) {
      if (i > n - 2)
        usage();
      ip = v[++i];
      continue;
    }
    if (!strcmp(v[i], "-p")) {
      if (i > n - 2)
        usage();
      port = atoi(v[++i]);
      continue;
    }
    usage();
  }

  if (database_filename == NULL) {
    printf("ERROR: Provide a database file (-d)\n");
    exit(1);
  }

  /* load the database T_messages.txt */
  database = parse_database(database_filename);
  load_config_file(database_filename);

  /* an array of int for all the events defined in the database is needed */
  number_of_events = number_of_ids(database);
  is_on = calloc(number_of_events, sizeof(int));
  if (is_on == NULL)
    abort();

  // Set first byte of the shared memory to 0
  addr_rd[0] = 0; // important to check if we have new requested messages

  // read requested tracer msg indices from memory
  printf("\n Data Collection Service: Waiting for messages request ...\n");
  // 0: Wait
  // 1: config
  // 2: record
  // 3: stop
  // Wait for Action Config
  while (1) {
    if ((uint8_t)(addr_rd[0]) == 0) {
      usleep(50); // sleep for 50 us: 0.5 ms slot duration
    }
    // config state
    else if ((uint8_t)(addr_rd[0]) == 1) {
      get_time_stamp_usec(trace_time_stamp_str);
      printf("\n Received config message. Time Stamp: %s", trace_time_stamp_str);

      // get the IP address length in bytes
      bufIdx_rd = 1;
      uint8_t ip_address_length = addr_rd[bufIdx_rd];
      bufIdx_rd += 1; // + 1 byte = IP address length

      // read the IP address
      for (int i = 0; i < ip_address_length; i++) {
        ip_address[i] = addr_rd[bufIdx_rd];
        bufIdx_rd += 1;
      }

      ip = ip_address;

      // get the array bytes of the port number : 2 bytes
      uint8_t port_number_bytes[2] = {addr_rd[bufIdx_rd], addr_rd[bufIdx_rd + 1]};
      bufIdx_rd += 2; // + 2 bytes = start frame number

      port = combine_bytes(port_number_bytes, 2);
      printf("\n Parameters: IP Address length: %d, IP Address: %s, Port Number: %d \n", ip_address_length, ip_address, port);
      addr_rd[0] = 0; // reset memory : to wait for record action
      break;
    }
  }
  // Establish connection to the nr-softmodem
  clear_remote_config();
  if (socket != -1)
    close(socket);

  /* connect to the nr-softmodem */
  socket = connect_to(ip, port);
  printf("\n Connected to nr-softmodem");

  // Read Action record or stop
  printf("\n Data Collection Service: Waiting for record message ...\n");
  while (1) {
    // wait for Action record
    if ((uint8_t)(addr_rd[0]) == 0) {
      usleep(50); // sleep for 50 us: 0.5 ms slot duration
    }

    // quit state
    else if ((uint8_t)(addr_rd[0]) == 3) {
      printf("\n Received 'stop' command. Exiting...");
      printf("\n ");
      // Clean up and exit
      break;
    }
    // record state
    else if ((uint8_t)(addr_rd[0]) == 2) {
      // clear remote buffer if there is
      clear_remote_config();
      get_time_stamp_usec(trace_time_stamp_str);
      printf("\n Received record message. Time Stamp: %s", trace_time_stamp_str);

      // read number of requested messages
      bufIdx_rd = 1;
      num_req_tracer_msgs = addr_rd[bufIdx_rd];
      bufIdx_rd += 1;
      printf("\n Number of requested tracer messages: %d,", num_req_tracer_msgs);
      // reset memory : action to wait for next record action
      addr_rd[0] = 0;

      // read tracer msg IDs - every message ID has been stored in two bytes
      for (uint8_t msg_n = 0; msg_n < num_req_tracer_msgs; msg_n++) {
        // get the array bytes of the tracer message ID: 2 bytes
        uint8_t msg_id_bytes[2] = {addr_rd[bufIdx_rd], addr_rd[bufIdx_rd + 1]};
        bufIdx_rd += 2; // + 2 bytes = message ID
        msg_id = combine_bytes(msg_id_bytes, 2);

        req_tracer_msgs_indices[msg_n] = msg_id;
        printf(" msg_id: %d, ", msg_id);
      }
      // get the array bytes of the number of records: 4 bytes
      uint8_t number_records_bytes[4] = {addr_rd[bufIdx_rd],
                                         addr_rd[bufIdx_rd + 1],
                                         addr_rd[bufIdx_rd + 2],
                                         addr_rd[bufIdx_rd + 3]};
      bufIdx_rd += 4; // + 4 bytes = number of records

      number_records = combine_bytes(number_records_bytes, 4);

      printf("num_records: %d, ", number_records);

      // get the array bytes of the start frame number : 2 bytes
      uint8_t start_frame_number_bytes[2] = {addr_rd[bufIdx_rd], addr_rd[bufIdx_rd + 1]};
      bufIdx_rd += 2; // + 2 bytes = start frame number

      start_frame_number = combine_bytes(start_frame_number_bytes, 2);

      printf("start_frame: %d\n", start_frame_number);

      /* activate the trace in this array */
      printf("\n Activate Tracer messages in on_off array: ");
      for (i = 0; i < num_req_tracer_msgs; i++) {
        char *on_off_name = event_name_from_id(database, req_tracer_msgs_indices[i]);
        on_off(database, on_off_name, is_on, 1);
        printf("%d, %s, ", req_tracer_msgs_indices[i], on_off_name);
        // on_off(database, "GNB_PHY_DL_OUTPUT_SIGNAL", is_on, 1);
      }

      // get the event IDs for the bit messages
      int traces_bits_support_data_Collection_format_idx[n_bits_msgs];
      for (int i = 0; i < n_bits_msgs; i++) {
        traces_bits_support_data_Collection_format_idx[i] =
            event_id_from_name(database, traces_bits_support_data_Collection_format[i]);
      }

      // all supported messages
      int traces_support_data_Collection_format_idx[n_msgs_based_data_Collection_format];
      for (int i = 0; i < n_msgs_based_data_Collection_format; i++) {
        traces_support_data_Collection_format_idx[i] = event_id_from_name(database, traces_support_data_Collection_format[i]);
      }

      // setup data for the trace messages
      setup_trace_msg_data(&trace_msg_data, database);
      printf("\n Setup Trace Data Done");

      // Get the start time
      struct timespec start_time = get_current_time();

      /* activate the tracee in the nr-softmodem */
      activate_traces(socket, number_of_events, is_on);
      printf("\n Activated Traces in nr-softmodem");

      /* a buffer needed to receive events from the nr-softmodem */
      OBUF ebuf = {osize : 0, omaxsize : 0, obuf : NULL};

      /* read events */
      int nrecord_idx = 0;
      bufIdx_wr = 0;
      // logic to be sure that we have a complete slot record
      bool start_recording = false;
      bool got_ref_frame_slot = false;
      int ref_slot = 0;
      int ref_frame = 0;
      int slot_difference = 0;
      int frame_difference = 0;
      int current_frame = 0, prev_frame = 0, current_slot = 0, prev_slot = 0;
      // offset to sync between base station and UE synchronization records or power measurements
      int sync_offset_index = 0; // increase the index only if the index of frame changes after getting all records
      // since we will use the frame differece to do extra records, we should be sure that the last slot is recorded completely
      printf("\n\n Data Collection Service: Start reading messages ...");
      struct pollfd event_poll_fd;
      event_poll_fd.fd = socket;
      event_poll_fd.events = POLLIN;
      while (1) {
        // stop if number of records is done
        if ((nrecord_idx >= number_records) && (sync_offset_index >= max_sync_offset)) {
          // We added one to the number of records to capture the last record completely if
          // we have several messages enabled per slot
          break;
        }
        int poll_ret = poll(&event_poll_fd, 1, 1); // 1 ms timeout for poll
        if (poll_ret > 0 && (event_poll_fd.revents & POLLIN)) {
          event e = get_event(socket, &ebuf, database);

          if (e.type == -1) {
            printf("\n Link broken or unexpected message received. Re-establishing connection...\n");
            reestablish_connection(&socket, ip, port, number_of_events, is_on);
            continue; // Skip further processing and retry
          }
          //-------------------------
          // GNB_PHY_UL_FD_PUSCH_IQ,  GNB_PHY_UL_FD_DMRS_ID, GNB_PHY_UL_FD_CHAN_EST_DMRS_POS,
          // UE_PHY_UL_SCRAMBLED_TX_BITS, GNB_PHY_UL_PAYLOAD_RX_BITS, UE_PHY_UL_PAYLOAD_TX_BITS
          //-------------------------
          // is it a requested message
          if (isValueInArray(e.type, req_tracer_msgs_indices, num_req_tracer_msgs)) {
            // is it based on Data Collection Trace Messages Structure
            if (isValueInArray(e.type, traces_support_data_Collection_format_idx, n_msgs_based_data_Collection_format)) {
              // Start recording from the next slot to mitigate capturing partial data
              // check if the current frame and slot are different from the previous frame and slot
              // Then, increase the record index
              if (start_recording == false) {
                if (got_ref_frame_slot == false) {
                  ref_frame = e.e[trace_msg_data.frame].i;
                  ref_slot = e.e[trace_msg_data.slot].i;
                  printf("\nMessage Info: msg_id %s (%d) \n", event_name_from_id(database, e.type), e.type);
                  got_ref_frame_slot = true;
                }

                current_frame = e.e[trace_msg_data.frame].i;
                current_slot = e.e[trace_msg_data.slot].i;
                frame_difference = (current_frame - ref_frame + MAX_FRAME_INDEX + 1) % (MAX_FRAME_INDEX + 1);
                slot_difference = (current_slot - ref_slot + MAX_SLOT_INDEX + 1) % (MAX_SLOT_INDEX + 1);
                printf("\n First frame.slot: %d.%d, current frame.slot: %d.%d, diff frame.slot: %d.%d",
                       ref_frame,
                       ref_slot,
                       current_frame,
                       current_slot,
                       frame_difference,
                       slot_difference);

                if ((ref_frame != current_frame) || (ref_slot != current_slot)) {
                  start_recording = true;
                  printf("\n Start recording from frame: %d, slot: %d ", e.e[trace_msg_data.frame].i, e.e[trace_msg_data.slot].i);
                }
              }
              // start recording from the next frame to mitigate capturing partial data
              if (start_recording == true) {
                /* this is how to access the elements of the Data Collection trace messages.
                 * we use e.e[<element>] and then the correct suffix, here
                 * it's .i for the integer and .b for the buffer and .bsize for the buffer size
                 * see in event.h the structure event_arg
                 */
                unsigned char *buf = e.e[trace_msg_data.data].b;

                printf("\n\nRecord number: %d", nrecord_idx);
#ifdef DEBUG_BUFFER
                printf("\nBuffer index in bytes: %d", bufIdx_wr);
#endif

                // add general message header:  message ID,
                // T-Tracer Message format
                // FORMAT = int,frame : int,slot : int,datetime_yyyymmdd : int,datetime_hhmmssmmm :
                // int,frame_type : int,freq_range : int,subcarrier_spacing : int,cyclic_prefix : int,symbols_per_slot :
                // int,Nid_cell : int,rnti :
                // int,rb_size : int,rb_start : int,start_symbol_index : int,nr_of_symbols :
                // int,qam_mod_order : int,mcs_index : int,mcs_table : int,nrOfLayers :
                // int,transform_precoding : int,dmrs_config_type : int,ul_dmrs_symb_pos :  int,number_dmrs_symbols :
                // int,dmrs_port : int,dmrs_nscid :
                // int,nb_antennas_rx : int,number_of_bits : buffer,data
                memcpy(&addr_wr[bufIdx_wr], &e.type, sizeof(uint16_t));
                bufIdx_wr += sizeof(uint16_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.frame].i, sizeof(uint16_t));
                bufIdx_wr += sizeof(uint16_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.slot].i, sizeof(uint8_t));
                bufIdx_wr += sizeof(uint8_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.datetime_yyyymmdd].i, sizeof(uint32_t));
                bufIdx_wr += sizeof(uint32_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.datetime_hhmmssmmm].i, sizeof(uint32_t));
                bufIdx_wr += sizeof(uint32_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.frame_type].i, sizeof(uint8_t));
                bufIdx_wr += sizeof(uint8_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.freq_range].i, sizeof(uint8_t));
                bufIdx_wr += sizeof(uint8_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.subcarrier_spacing].i, sizeof(uint8_t));
                bufIdx_wr += sizeof(uint8_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.cyclic_prefix].i, sizeof(uint8_t));
                bufIdx_wr += sizeof(uint8_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.symbols_per_slot].i, sizeof(uint8_t));
                bufIdx_wr += sizeof(uint8_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.Nid_cell].i, sizeof(uint16_t));
                bufIdx_wr += sizeof(uint16_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.rnti].i, sizeof(uint16_t));
                bufIdx_wr += sizeof(uint16_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.rb_size].i, sizeof(uint16_t));
                bufIdx_wr += sizeof(uint16_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.rb_start].i, sizeof(uint16_t));
                bufIdx_wr += sizeof(uint16_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.start_symbol_index].i, sizeof(uint8_t));
                bufIdx_wr += sizeof(uint8_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.nr_of_symbols].i, sizeof(uint8_t));
                bufIdx_wr += sizeof(uint8_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.qam_mod_order].i, sizeof(uint8_t));
                bufIdx_wr += sizeof(uint8_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.mcs_index].i, sizeof(uint8_t));
                bufIdx_wr += sizeof(uint8_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.mcs_table].i, sizeof(uint8_t));
                bufIdx_wr += sizeof(uint8_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.nrOfLayers].i, sizeof(uint8_t));
                bufIdx_wr += sizeof(uint8_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.transform_precoding].i, sizeof(uint8_t));
                bufIdx_wr += sizeof(uint8_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.dmrs_config_type].i, sizeof(uint8_t));
                bufIdx_wr += sizeof(uint8_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.ul_dmrs_symb_pos].i, sizeof(uint16_t));
                bufIdx_wr += sizeof(uint16_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.number_dmrs_symbols].i, sizeof(uint8_t));
                bufIdx_wr += sizeof(uint8_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.dmrs_port].i, sizeof(uint16_t));
                bufIdx_wr += sizeof(uint16_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.dmrs_nscid].i, sizeof(uint16_t));
                bufIdx_wr += sizeof(uint16_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.nb_antennas_rx].i, sizeof(uint8_t));
                bufIdx_wr += sizeof(uint8_t);
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.number_of_bits].i, sizeof(uint32_t));
                bufIdx_wr += sizeof(uint32_t);

                printf("\nTime Stamp: %d_%d", e.e[trace_msg_data.datetime_yyyymmdd].i, e.e[trace_msg_data.datetime_hhmmssmmm].i);

                // add message body: length in bytes + recorded data
                memcpy(&addr_wr[bufIdx_wr], &e.e[trace_msg_data.data].bsize, sizeof(uint32_t));
                bufIdx_wr += sizeof(uint32_t);

                // read data from buffer and convert from unsigned char * array to int 16 using the right endianness
                // T-tracer: Little Endian
                // For BITS Messages, example: UE_PHY_UL_SCRAMBLED_TX_BITS: data in bytes

                if (is_bits_messages(traces_bits_support_data_Collection_format_idx, n_bits_msgs, e.type)) {
                  for (int byte_idx = 0; byte_idx < e.e[trace_msg_data.data].bsize; byte_idx += 1) {
                    // printf("%d, ", buf[byte_idx]);
                    memcpy(&addr_wr[bufIdx_wr], &buf[byte_idx], sizeof(uint8_t));
                    bufIdx_wr += sizeof(uint8_t);
                  }

                } else {
                  for (int byte_idx = 0; byte_idx < e.e[trace_msg_data.data].bsize; byte_idx += 2) {
                    // For a little-endian system:
                    memcpy(&addr_wr[bufIdx_wr], &buf[byte_idx], sizeof(uint8_t));
                    bufIdx_wr += sizeof(uint8_t);
                    memcpy(&addr_wr[bufIdx_wr], &buf[byte_idx + 1], sizeof(uint8_t));
                    bufIdx_wr += sizeof(uint8_t);
                  }
                }
                /*
                for (int byte_idx_i = 0; byte_idx_i < e.e[d.nr_ul_fd_dmrs_data].bsize; byte_idx_i+=4) {
                  int16_t I = buf[byte_idx_i] | (buf[byte_idx_i+1] << 8);
                  int16_t Q = buf[byte_idx_i+2] | (buf[byte_idx_i+3] << 8);
                  printf ("idx %d, ", byte_idx_i);
                  printf ("\n%d", I);
                  printf ("\n%d", Q);
                }
                */
                // check if the current frame and slot are different from the previous frame and slot
                // Then, increase the record index
                current_frame = e.e[trace_msg_data.frame].i;
                current_slot = e.e[trace_msg_data.slot].i;

                // increase sync offset index if the current frame is different from the previous frame
                if ((current_frame != prev_frame) && nrecord_idx >= number_records) {
                  sync_offset_index++;
                }

                if (current_frame != prev_frame || current_slot != prev_slot) {
                  nrecord_idx++;
                  // Update previous frame and slot numbers
                  prev_frame = current_frame;
                  prev_slot = current_slot;
                }
#ifdef DEBUG_T_Tracer
                printf("\nMessage Info: msg_id %s (%d) \n", event_name_from_id(database, e.type), e.type);
                printf("frame %d, slot %d, datetime %d_%d\n",
                       e.e[trace_msg_data.frame].i,
                       e.e[trace_msg_data.slot].i,
                       e.e[trace_msg_data.datetime_yyyymmdd].i,
                       e.e[trace_msg_data.datetime_hhmmssmmm].i);
                printf("frame_type %d, freq_range %d, subcarrier_spacing %d, cyclic_prefix %d, symbols_per_slot %d\n",
                       e.e[trace_msg_data.frame_type].i,
                       e.e[trace_msg_data.freq_range].i,
                       e.e[trace_msg_data.subcarrier_spacing].i,
                       e.e[trace_msg_data.cyclic_prefix].i,
                       e.e[trace_msg_data.symbols_per_slot].i);
                printf("Nid_cell %d, rnti %d, rb_size %d, rb_start %d, start_symbol_index %d, nr_of_symbols %d\n",
                       e.e[trace_msg_data.Nid_cell].i,
                       e.e[trace_msg_data.rnti].i,
                       e.e[trace_msg_data.rb_size].i,
                       e.e[trace_msg_data.rb_start].i,
                       e.e[trace_msg_data.start_symbol_index].i,
                       e.e[trace_msg_data.nr_of_symbols].i);
                printf("qam_mod_order %d, mcs_index %d, mcs_table %d, nrOfLayers %d, transform_precoding %d\n",
                       e.e[trace_msg_data.qam_mod_order].i,
                       e.e[trace_msg_data.mcs_index].i,
                       e.e[trace_msg_data.mcs_table].i,
                       e.e[trace_msg_data.nrOfLayers].i,
                       e.e[trace_msg_data.transform_precoding].i);
                printf("dmrs_config_type %d, ul_dmrs_symb_pos %d, number_dmrs_symbols %d, dmrs_port %d, dmrs_nscid %d\n",
                       e.e[trace_msg_data.dmrs_config_type].i,
                       e.e[trace_msg_data.ul_dmrs_symb_pos].i,
                       e.e[trace_msg_data.number_dmrs_symbols].i,
                       e.e[trace_msg_data.dmrs_port].i,
                       e.e[trace_msg_data.dmrs_nscid].i);
                printf("nb_antennas_rx %d, number_of_bits %d, data size %d\n",
                       e.e[trace_msg_data.nb_antennas_rx].i,
                       e.e[trace_msg_data.number_of_bits].i,
                       e.e[trace_msg_data.data].bsize);
#endif
              } // End of start recording flag
            } // end of if statement for the supported messages based on Data Collection Trace Messages Structure
            else {
              printf("ERROR: Requested Message is not based on Data Collection Trace Messages Structure\n");
              printf("ERROR: Requested Message ID: %d\n", e.type);
            }
          } // end of if statement for the requested messages
        } // end while loop of reading events
        else {
          // No data, just loop and check time
          usleep(100); // optional: avoid busy-waiting
        }
      } // End of while loop to read events

      // de-activate the tracee in the nr-softmodem
      printf("\n De-activated Tracer message:\n");
      for (i = 0; i < num_req_tracer_msgs; i++) {
        char *on_off_name = event_name_from_id(database, req_tracer_msgs_indices[i]);
        on_off(database, on_off_name, is_on, 0);
        // printf("\n %d, %s, ", req_tracer_msgs_indices[i], on_off_name);
        // on_off(database, "GNB_PHY_DL_OUTPUT_SIGNAL", is_on, 1);
      }
      // De-activate the tracee in the nr-softmodem
      activate_traces(socket, number_of_events, is_on);
      printf("\n De-activated Traces");
      // Get the end time
      struct timespec end_time = get_current_time();
      // Calculate the time difference
      double time_diff = calculate_time_difference(start_time, end_time);
      printf("Total Time difference: %.2f ms\n", time_diff);
      printf("Time difference per record: %.2f ms\n", time_diff / (number_records + max_sync_offset));

      // discard stale or previous record data for the first DISCARD_RECORD_DURATION_MS
      struct timespec record_start, record_now;
      clock_gettime(CLOCK_MONOTONIC, &record_start);

      while (1) {
        clock_gettime(CLOCK_MONOTONIC, &record_now);
        double elapsed_ms = calculate_time_difference(record_start, record_now);
        if (elapsed_ms >= DISCARD_RECORD_DURATION_MS) {
          break; // Stop after 10ms
        }
        int poll_ret = poll(&event_poll_fd, 1, 1); // 1 ms timeout for poll
        if (poll_ret > 0 && (event_poll_fd.revents & POLLIN)) {
          event e = get_event(socket, &ebuf, database);
          printf("%d", e.type);
          if (e.type == -1) {
            printf("\n Link broken or unexpected message received. Re-establishing connection...\n");
            reestablish_connection(&socket, ip, port, number_of_events, is_on);
            continue;
          }
        } else {
          // No data, just loop and check time
          usleep(100); // optional: avoid busy-waiting
        }
      } // End of while loop to discard stale records
    }
  } // End a while loop to check for the "stop" command
  // de-activate the tracee in the nr-softmodem

  free_database(database);
  free(is_on);
  close(socket);

  return 0;
}
