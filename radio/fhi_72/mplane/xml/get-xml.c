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

#include "get-xml.h"

#include <libxml/parser.h>
#include <string.h>

static char *find_ru_xml_node(xmlNode *node, const char *filter)
{
  for (xmlNode *cur_node = node; cur_node; cur_node = cur_node->next) {
    if (cur_node->type != XML_ELEMENT_NODE)
      continue;

    if (strcmp((const char *)cur_node->name, filter) == 0) {
      xmlNode *target_node = cur_node;

      for(xmlNode *cur_node2 = cur_node->children; cur_node2; cur_node2 = cur_node2->next) {
        if (cur_node2->type == XML_ELEMENT_NODE && strcmp((const char *)cur_node2->name, "name") == 0) {
          target_node = cur_node2;
          break;
        }
      }
      return (char *)xmlNodeGetContent(target_node);
    }
    char *answer = find_ru_xml_node(cur_node->children, filter);
    if (answer != NULL) {
      return answer;
    }
  }
  return NULL;
}

char *get_ru_xml_node(const char *buffer, const char *filter)
{
  // Initialize the xml file
  size_t len = strlen(buffer) + 1;
  xmlDoc *doc = xmlReadMemory(buffer, len, NULL, NULL, 0);
  xmlNode *root_element = xmlDocGetRootElement(doc);

  return find_ru_xml_node(root_element->children, filter);
}

static void find_ru_xml_list(xmlNode *node, const char *filter, char ***match_list, size_t *count)
{
  for (xmlNode *cur_node = node; cur_node; cur_node = cur_node->next) {
    if (cur_node->type != XML_ELEMENT_NODE)
      continue;
    if (strcmp((const char *)cur_node->name, filter) == 0) {
      xmlNode *name_node = NULL;

      for (xmlNode *cur_node2 = cur_node->children; cur_node2; cur_node2 = cur_node2->next) {
        const char *name_node_str = (const char *)cur_node2->name;
        if (cur_node2->type == XML_ELEMENT_NODE && (strcmp(name_node_str, "name") == 0 || strcmp(name_node_str, "measurement-object") == 0)) {
          name_node = cur_node2;
          break;
        }
      }
      const char *content = (const char *)xmlNodeGetContent(name_node ? name_node : cur_node);
      if (content) {
        *match_list = realloc(*match_list, (*count + 1) * sizeof(char *));
        (*match_list)[*count] = strdup(content);
        (*count)++;
      }
    }
    find_ru_xml_list(cur_node->children, filter, match_list, count);
  }
}

void get_ru_xml_list(const char *buffer, const char *filter, char ***match_list, size_t *count)
{
  // Initialize the xml file
  size_t len = strlen(buffer) + 1;
  xmlDoc *doc = xmlReadMemory(buffer, len, NULL, NULL, 0);
  xmlNode *root_element = xmlDocGetRootElement(doc);

  find_ru_xml_list(root_element->children, filter, match_list, count);
}
