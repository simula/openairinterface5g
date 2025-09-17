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

#include "yaml-cpp/yaml.h"
extern "C" {
#include "common/config/config_load_configmodule.h"
#include "common/config/config_userapi.h"
void *config_allocate_new(configmodule_interface_t *cfg, int sz, bool autoFree);
void config_check_valptr(configmodule_interface_t *cfg, paramdef_t *cfgoptions, int elt_sz, int nb_elt);
}
#include <cstring>
#include <string>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace config_yaml {
class YamlConfig {
 public:
  YAML::Node config;
  YamlConfig(std::string filename)
  {
    config = YAML::LoadFile(filename);
  }
};

void SetDefault(configmodule_interface_t *cfg, paramdef_t *param)
{
  switch (param->type) {
    case TYPE_INT:
      *param->iptr = param->defintval;
      break;
    case TYPE_UINT:
      *param->uptr = param->defuintval;
      break;
    case TYPE_STRING:
      if (param->defstrval != nullptr) {
        if (param->numelt == 0) {
          config_check_valptr(cfg, param, 1, strlen(param->defstrval) + 1);
        }
        sprintf(*param->strptr, "%s", param->defstrval);
      }
      break;
    case TYPE_INT8:
      *param->i8ptr = param->defintval;
      break;
    case TYPE_UINT8:
      *param->i8ptr = param->defuintval;
      break;
    case TYPE_INT16:
      *param->i16ptr = param->defintval;
      break;
    case TYPE_UINT16:
      *param->u16ptr = param->defuintval;
      break;
    case TYPE_INT64:
      *param->i64ptr = param->defint64val;
      break;
    case TYPE_UINT64:
      *param->u64ptr = param->defint64val;
      break;
    case TYPE_DOUBLE:
      *param->dblptr = param->defdblval;
      break;
    case TYPE_MASK:
      *param->uptr = param->defuintval;
      break;
    case TYPE_STRINGLIST:
      if (param->defstrlistval != nullptr) {
        param->strlistptr = param->defstrlistval;
      }
      break;
    case TYPE_INTARRAY:
      if (param->defintarrayval) {
        param->iptr = param->defintarrayval;
      }
      break;
    case TYPE_UINTARRAY:
      if (param->defintarrayval) {
        param->uptr = (uint32_t *)param->defintarrayval;
      }
      break;
    default:
      AssertFatal(false, "Unhandled type %d", param->type);
  }
  param->paramflags |= PARAMFLAG_PARAMSETDEF;
}

void SetNonDefault(configmodule_interface_t *cfg, const YAML::Node &node, paramdef_t *param)
{
  auto optname = std::string(param->optname);
  switch (param->type) {
    case TYPE_INT:
      *param->iptr = node[optname].as<int>();
      break;
    case TYPE_UINT:
      *param->uptr = node[optname].as<uint>();
      break;
    case TYPE_STRING: {
      std::string setting = node[optname].as<std::string>();
      config_check_valptr(cfg, param, 1, setting.length() + 1);
      sprintf(*param->strptr, "%s", setting.c_str());
      break;
    }
    case TYPE_UINT8:
      *param->i8ptr = node[optname].as<uint8_t>();
      break;
    case TYPE_INT16:
      *param->i16ptr = node[optname].as<int16_t>();
      break;
    case TYPE_UINT16:
      *param->u16ptr = node[optname].as<uint16_t>();
      break;
    case TYPE_INT64:
      *param->i64ptr = node[optname].as<int64_t>();
      break;
    case TYPE_UINT64:
      *param->u64ptr = node[optname].as<uint64_t>();
      break;
    case TYPE_DOUBLE:
      *param->dblptr = node[optname].as<double>();
      break;
    case TYPE_MASK:
      *param->uptr = node[optname].as<uint>();
      break;
    case TYPE_STRINGLIST: {
      if (node[optname].IsSequence()) {
        config_check_valptr(cfg, param, sizeof(char *), node[optname].size());
        int i = 0;
        for (const auto &elem : node[optname]) {
          sprintf(param->strlistptr[i], "%s", elem.as<std::string>().c_str());
          i++;
        }
        param->numelt = i;
      } else {
        param->numelt = 0;
      }
      break;
    }
    case TYPE_INTARRAY: {
      if (node[optname].IsSequence() && node[optname].size() > 0) {
        param->numelt = node[optname].size();
        param->iptr =
            (int32_t *)config_allocate_new(cfg, param->numelt * sizeof(*param->iptr), !(param->paramflags & PARAMFLAG_NOFREE));

        for (int i = 0; i < param->numelt; i++) {
          param->iptr[i] = node[optname][i].as<int32_t>();
        }
      } else {
        param->numelt = 0;
      }
      break;
    }
    case TYPE_UINTARRAY: {
      if (node[optname].IsSequence() && node[optname].size() > 0) {
        param->numelt = node[optname].size();
        param->uptr =
            (uint32_t *)config_allocate_new(cfg, param->numelt * sizeof(*param->uptr), !(param->paramflags & PARAMFLAG_NOFREE));

        for (int i = 0; i < param->numelt; i++) {
          param->uptr[i] = node[optname][i].as<uint32_t>();
        }
      } else {
        param->numelt = 0;
      }
      break;
    }
    default:
      AssertFatal(false, "Unhandled type %d", param->type);
  }
  param->paramflags |= PARAMFLAG_PARAMSET;
}

void GetParams(configmodule_interface_t *cfg, const YAML::Node &node, paramdef_t *params, int num_params)
{
  for (auto i = 0; i < num_params; i++) {
    if (node && node[std::string(params[i].optname)]) {
      SetNonDefault(cfg, node, &params[i]);
    } else {
      SetDefault(cfg, &params[i]);
    }
  }
}

static YamlConfig *config;
static YAML::Node invalid_node;
} // namespace config_yaml

extern "C" int config_yaml_init(configmodule_interface_t *cfg)
{
  char **cfgP = cfg->cfgP;
  cfg->numptrs = 0;
  pthread_mutex_init(&cfg->memBlocks_mutex, NULL);
  memset(cfg->oneBlock, 0, sizeof(cfg->oneBlock));

  config_yaml::config = new config_yaml::YamlConfig(std::string(cfgP[0]));
  return 0;
}

extern "C" void config_yaml_end(configmodule_interface_t *cfg)
{
  delete config_yaml::config;
}

YAML::Node find_node(const std::string &prefix, YAML::Node node)
{
  // Iterate through each key in the prefix
  std::stringstream ss(prefix);
  std::string key;
  YAML::Node current = node;

  while (std::getline(ss, key, '.')) {
    if (key.at(0) == '[' && key.back() == ']') {
      // The key is an index to a sequence
      if (!current.IsSequence()) {
        throw std::invalid_argument("Incorrect yaml file structure");
      }
      int index = std::stoi(key.substr(1, key.size() - 2));
      current = current[index];
      continue;
    }
    if (!current.IsMap()) {
      throw std::invalid_argument("Incorrect yaml file structure");
    }
    if (!current[key]) {
      return config_yaml::invalid_node;
    }
    current = current[key];
  }
  return current;
}

extern "C" int config_yaml_get(configmodule_interface_t *cfg, paramdef_t *cfgoptions, int numoptions, char *prefix)
{
  std::string p;
  if (prefix != nullptr) {
    p = std::string(prefix);
  }
  auto node = find_node(p, YAML::Clone(config_yaml::config->config));
  if (node == config_yaml::invalid_node) {
    return -1;
  }
  for (auto i = 0; i < numoptions; i++) {
    if (cfgoptions[i].type != TYPE_STRING && cfgoptions[i].type != TYPE_INTARRAY && cfgoptions[i].type != TYPE_UINTARRAY
        && cfgoptions[i].voidptr == nullptr) {
      config_check_valptr(cfg, &cfgoptions[i], sizeof(void *), 1);
    }
  }
  config_yaml::GetParams(cfg, node, cfgoptions, numoptions);
  return 0;
}

extern "C" int config_yaml_getlist(configmodule_interface_t *cfg,
                                   paramlist_def_t *ParamList,
                                   paramdef_t *params,
                                   int numparams,
                                   char *prefix)
{
  char path[512];
  if (prefix != nullptr) {
    sprintf(path, "%s.%s", prefix, ParamList->listname);
  } else {
    sprintf(path, "%s", ParamList->listname);
  }
  ParamList->numelt = 0;
  auto node = find_node(path, YAML::Clone(config_yaml::config->config));
  if (node == config_yaml::invalid_node) {
    return -1;
  }

  if (!node.IsSequence()) {
    return -1;
  }
  ParamList->numelt = node.size();

  if (ParamList->numelt > 0 && params != NULL) {
    ParamList->paramarray = static_cast<paramdef_t **>(config_allocate_new(cfg, ParamList->numelt * sizeof(paramdef_t *), true));

    for (int i = 0; i < ParamList->numelt; i++) {
      ParamList->paramarray[i] = static_cast<paramdef *>(config_allocate_new(cfg, numparams * sizeof(paramdef_t), true));
      memcpy(ParamList->paramarray[i], params, sizeof(paramdef_t) * numparams);

      for (int j = 0; j < numparams; j++) {
        ParamList->paramarray[i][j].strptr = NULL;
        if (ParamList->paramarray[i][j].type != TYPE_STRING) {
          config_check_valptr(cfg, &ParamList->paramarray[i][j], sizeof(void *), 1);
        }
      }

      config_yaml::GetParams(cfg, node[i], ParamList->paramarray[i], numparams);
    }
  }

  return ParamList->numelt;
}

extern "C" void config_yaml_write_parsedcfg(configmodule_interface_t *cfg)
{
  (void)cfg;
}

extern "C" int config_yaml_set(configmodule_interface_t *cfg, paramdef_t *cfgoptions, int numoptions, char *prefix)
{
  (void)cfg;
  (void)cfgoptions;
  (void)numoptions;
  (void)prefix;
  return 0;
}
