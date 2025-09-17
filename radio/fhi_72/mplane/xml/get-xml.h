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

#ifndef GET_MPLANE_INFO_XML_H
#define GET_MPLANE_INFO_XML_H

#include <stdio.h>
#include <stdbool.h>

/**
 * @brief retrieves the node value that matches filter.
 *
 * @param[in] buffer Buffer in xml format.
 * @param[in] filter The node name (matching criteria).
 * @return Expected one string that matches the node name.
 */
char *get_ru_xml_node(const char *buffer, const char *filter);

/**
 * @brief retrieves the node values that matches filter.
 *
 * @param[in] buffer Buffer in xml format.
 * @param[in] filter The node name (matching criteria).
 * @param[out] match_list Resulting list containing node values.
 * @param[out] count The number of nodes that match criteria.
 * @return Void.
 */
void get_ru_xml_list(const char *buffer, const char *filter, char ***match_list, size_t *count);

#endif /* GET_MPLANE_INFO_XML_H */
