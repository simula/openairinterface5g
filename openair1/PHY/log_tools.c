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

/*! \file openair1/PHY/log_tools.c
 * \brief log tools used by data recording application (to be merged to existing OAI log tools)
 * \author Abdo Gaber
 * \date 2024
 * \version 1.0
 * \company Emerson, NI Test and Measurement
 * \email:
 * \note
 * \warning
 */

#include "log_tools.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include "PHY/TOOLS/tools_defs.h"

// Get Time Stamp in microseconds in YYYYMMDDHHMMSSmmmuuu format
char* get_time_stamp_usec(char time_stamp_str[])
{
  // initialization to measure time stamp --This part should be moved to inilization part
  time_t my_time;
  struct tm* timeinfo;
  time(&my_time);
  struct timeval tv;

  // get time stamp
  timeinfo = localtime(&my_time);
  gettimeofday(&tv, NULL);
  // Add time stamp: yyyy mm dd hh mm ss msec
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

// Function to convert timestamp string to integer
int convert_time_stamp_to_int(const char* timestamp)
{
  return atoi(timestamp);
}

int split_time_stamp_and_convert_to_int(char time_stamp_str[], int shift, int length)
{
  char time_part[length + 1]; // Buffer to hold the date part YYYYMMDD or HHMMSSmmm
  // Copy the first 8 or 9 characters (YYYYMMDD) to HHMMSSmmm
  strncpy(time_part, time_stamp_str + shift, length);
  time_part[length] = '\0'; // Null-terminate the string
  // Convert timestamp string to integer
  return convert_time_stamp_to_int(time_part);
}