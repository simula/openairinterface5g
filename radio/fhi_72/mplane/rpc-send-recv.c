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

#include "rpc-send-recv.h"

#include "common/utils/assertions.h"


#ifdef MPLANE_V1
static bool recv_v1(struct nc_session *session, struct nc_rpc *rpc, NC_MSG_TYPE msgtype, const uint64_t msgid, const NC_WD_MODE wd_mode, const int timeout_ms, char **answer)
{
  struct nc_reply *reply;
  struct nc_reply_data *data_rpl;
  struct nc_reply_error *error;
  LYD_FORMAT output_format = LYD_XML;
  uint32_t ly_wd;

  while (1) {
    msgtype = nc_recv_reply(session, rpc, msgid, timeout_ms, 0, &reply);
    if (msgtype == NC_MSG_ERROR) {
      AssertError(false, return false, "[MPLANE] Failed to receive a reply.\n");
    } else if (msgtype == NC_MSG_WOULDBLOCK) {
      AssertError(false, return false, "[MPLANE] Timeout for receiving a reply expired.\n");
    } else if (msgtype == NC_MSG_NOTIF) {
      /* read again */
      continue;
    } else if (msgtype == NC_MSG_REPLY_ERR_MSGID) {
      /* unexpected message, try reading again to get the correct reply */
      MP_LOG_I("Unexpected reply received - ignoring and waiting for the correct reply.\n");
      nc_reply_free(reply);
      continue;
    }
    break;
  }

  switch (reply->type) {
    case NC_RPL_OK:
      MP_LOG_I("RPC reply = OK.\n");
      break;
    case NC_RPL_DATA:
      data_rpl = (struct nc_reply_data *)reply;

      switch (wd_mode) {
        case NC_WD_ALL:
          ly_wd = LYP_WD_ALL;
          break;
        case NC_WD_ALL_TAG:
          ly_wd = LYP_WD_ALL_TAG;
          break;
        case NC_WD_TRIM:
          ly_wd = LYP_WD_TRIM;
          break;
        case NC_WD_EXPLICIT:
          ly_wd = LYP_WD_EXPLICIT;
          break;
        default:
          ly_wd = 0; // when wd_mode = NC_WD_UNKNOWN
          break;
      }

      if (nc_rpc_get_type(rpc) == NC_RPC_GETSCHEMA) {
        AssertError((!data_rpl->data || (data_rpl->data->schema->nodetype != LYS_RPC) || (data_rpl->data->child == NULL)
               || (data_rpl->data->child->schema->nodetype != LYS_ANYXML)), return false, "[MPLANE] Cannot get schema.\n");

        struct lyd_node_anydata *any = (struct lyd_node_anydata *)data_rpl->data->child;
        switch (any->value_type) {
          case LYD_ANYDATA_CONSTSTRING:
          case LYD_ANYDATA_STRING:
            *answer = strdup(any->value.str);
            break;
          case LYD_ANYDATA_DATATREE:
            lyd_print_mem(answer, any->value.tree, output_format, LYP_WITHSIBLINGS);
            break;
          case LYD_ANYDATA_XML:
            lyxml_print_mem(answer, any->value.xml, LYXML_PRINT_SIBLINGS);
            break;
          default:
            AssertError(false, return false, "[MPLANE] Unknown yang type.\n");
          }
          break;
      } else if (nc_rpc_get_type(rpc) == NC_RPC_GETCONFIG || nc_rpc_get_type(rpc) == NC_RPC_GET) {
        const char *tag = nc_rpc_get_type(rpc) == NC_RPC_GETCONFIG ? "config" : "data";
        char *buffer = NULL;
        lyd_print_mem(&buffer, data_rpl->data, output_format, LYP_WITHSIBLINGS | LYP_NETCONF | ly_wd);
        *answer = calloc(strlen(buffer)+128, sizeof(char));
        sprintf(*answer, "<%s xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">\n%s</%s>", tag, buffer, tag);
      } else {
        AssertError(false, return false, "[MPLANE] Unknown RPC type.\n");
      }

      break;
    case NC_RPL_ERROR:
      error = (struct nc_reply_error *)reply;
      for (int i = 0; i < error->count; ++i) {
        const struct nc_err *err = &error->err[i];
        MP_LOG_I("RPC reply = ERROR\n\
                  \ttype:     %s\n\
                  \ttag:      %s\n\
                  \tseverity: %s\n\
                  \tapp-tag:  %s\n\
                  \tpath:     %s\n\
                  \tmessage:  %s\n\
                  \tSID:      %s\n",
                  err->type,
                  err->tag,
                  err->severity,
                  err->apptag,
                  err->path,
                  err->message,
                  err->sid);

        for (int j = 0; j < err->attr_count; ++j) {
          MP_LOG_I("\tbad-attr #%d: %s\n", j + 1, err->attr[j]);
        }
        for (int j = 0; j < err->elem_count; ++j) {
          MP_LOG_I("\tbad-elem #%d: %s\n", j + 1, err->elem[j]);
        }
        for (int j = 0; j < err->ns_count; ++j) {
          MP_LOG_I("\tbad-ns #%d:   %s\n", j + 1, err->ns[j]);
        }
        for (int j = 0; j < err->other_count; ++j) {
          char *str = NULL;
          lyxml_print_mem(&str, err->other[j], 0);
          MP_LOG_I("\tother #%d:\n%s\n", j + 1, str);
          free(str);
        }
      }
      AssertError(false, return false, "[MPLANE] Unable to continue.\n");
      break;
    default:
      AssertError(false, return false, "[MPLANE] Internal error.\n");
      nc_reply_free(reply);
  }
  nc_reply_free(reply);

  return true;
}
#elif defined MPLANE_V2
static bool recv_v2(struct nc_session *session, struct nc_rpc *rpc, NC_MSG_TYPE msgtype, const uint64_t msgid, const NC_WD_MODE wd_mode, const int timeout_ms, char **answer)
{
  struct lyd_node *envp, *op, *err;
  LYD_FORMAT output_format = LYD_XML;
  uint32_t ly_wd;

  while (1) {
    msgtype = nc_recv_reply(session, rpc, msgid, timeout_ms, &envp, &op);
    if (msgtype == NC_MSG_ERROR) {
      AssertError(false, return false, "[MPLANE] Failed to receive a reply.\n");
    } else if (msgtype == NC_MSG_WOULDBLOCK) {
      AssertError(false, return false, "[MPLANE] Timeout for receiving a reply for RPC expired.\n");
    } else if (msgtype == NC_MSG_NOTIF) { // for SUBSCRIBE
      /* read again */
      continue;
    } else if (msgtype == NC_MSG_REPLY_ERR_MSGID) {
      /* unexpected message, try reading again to get the correct reply */
      MP_LOG_I("Unexpected reply received - ignoring and waiting for the correct reply.\n");
      lyd_free_tree(envp);
      lyd_free_tree(op);
      continue;
    }
    break;
  }

  /* get functionality */
  if (op) {
  /* data reply */
    if (nc_rpc_get_type(rpc) == NC_RPC_GETSCHEMA) {
      /* special case */
      if (!lyd_child(op) || (lyd_child(op)->schema->nodetype != LYS_ANYXML)) {
        AssertError(false, return false, "[MPLANE] Cannot get schema.");
      }
      struct lyd_node_any *any = (struct lyd_node_any *)lyd_child(op);
      switch (any->value_type) {
      case LYD_ANYDATA_STRING:
      case LYD_ANYDATA_XML:
        *answer = strdup(any->value.str);
        break;
      case LYD_ANYDATA_DATATREE:
        lyd_print_mem(answer, any->value.tree, output_format, LYD_PRINT_WITHSIBLINGS);
        break;
      default:
        AssertError(false, return false, "[MPLANE] Unexpected yang type.");
      }
    } else {
      switch (wd_mode) {
        case NC_WD_ALL:
          ly_wd = LYD_PRINT_WD_ALL;
          break;
        case NC_WD_ALL_TAG:
          ly_wd = LYD_PRINT_WD_ALL_TAG;
          break;
        case NC_WD_TRIM:
          ly_wd = LYD_PRINT_WD_TRIM;
          break;
        case NC_WD_EXPLICIT:
          ly_wd = LYD_PRINT_WD_EXPLICIT;
          break;
        default:
          ly_wd = 0; // when wd_mode = NC_WD_UNKNOWN
          break;
      }
      lyd_print_mem(answer, lyd_child(op), output_format, LYD_PRINT_WITHSIBLINGS | ly_wd);
    }
  /* edit/validate/commit functionalities */
  } else if (!strcmp(LYD_NAME(lyd_child(envp)), "ok")) {
    MP_LOG_I("RPC reply = OK.\n");
  } else {
    LY_LIST_FOR(lyd_child(envp), err) {
      struct lyd_node *type, *tag, *severity, *app_tag, *path, *er_msg, *info = NULL;
      lyd_find_sibling_opaq_next(lyd_child(err), "error-type", &type);
      lyd_find_sibling_opaq_next(lyd_child(err), "error-tag", &tag);
      lyd_find_sibling_opaq_next(lyd_child(err), "error-severity", &severity);
      lyd_find_sibling_opaq_next(lyd_child(err), "error-app-tag", &app_tag);
      lyd_find_sibling_opaq_next(lyd_child(err), "error-path", &path);
      lyd_find_sibling_opaq_next(lyd_child(err), "error-message", &er_msg);

      MP_LOG_I("RPC reply = ERROR\n\
                \ttype:     %s\n\
                \ttag:      %s\n\
                \tseverity: %s\n\
                \tapp-tag:  %s\n\
                \tpath:     %s\n\
                \tmessage:  %s\n",
                (type ? ((struct lyd_node_opaq *)type)->value : NULL),
                (tag ? ((struct lyd_node_opaq *)tag)->value : NULL),
                (severity ? ((struct lyd_node_opaq *)severity)->value : NULL),
                (app_tag ? ((struct lyd_node_opaq *)app_tag)->value : NULL),
                (path ? ((struct lyd_node_opaq *)path)->value : NULL),
                (er_msg ? ((struct lyd_node_opaq *)er_msg)->value : NULL));

      info = lyd_child(err);
      while (!lyd_find_sibling_opaq_next(info, "error-info", &info)) {
        char *str = NULL;
        lyd_print_mem(&str, lyd_child(info), output_format, LYD_PRINT_WITHSIBLINGS);
        MP_LOG_I("\tinfo:   %s\n", str);
        info = info->next;
        free(str);
      }
      lyd_free_tree(type);
      lyd_free_tree(tag);
      lyd_free_tree(severity);
      lyd_free_tree(app_tag);
      lyd_free_tree(path);
      lyd_free_tree(er_msg);
      lyd_free_tree(info);
    }
    lyd_free_tree(envp);
    lyd_free_tree(op);
    AssertError(false, return false, "[MPLANE] Unable to continue.\n");
  }

  lyd_free_tree(envp);
  lyd_free_tree(op);

  return true;
}
#endif

bool rpc_send_recv(struct nc_session *session, struct nc_rpc *rpc, const NC_WD_MODE wd_mode, const int timeout_ms, char **answer)
{
  uint64_t msgid;
  NC_MSG_TYPE msgtype;

  msgtype = nc_send_rpc(session, rpc, 1000, &msgid);
  if (msgtype == NC_MSG_ERROR) {
    AssertError(false, return false, "[MPLANE] Failed to send the RPC.\n");
  } else if (msgtype == NC_MSG_WOULDBLOCK) {
    AssertError(false, return false, "[MPLANE] Timeout for sending the RPC expired.\n");
  }

#ifdef MPLANE_V1
  return recv_v1(session, rpc, msgtype, msgid, wd_mode, timeout_ms, answer);
#elif defined MPLANE_V2
  return recv_v2(session, rpc, msgtype, msgid, wd_mode, timeout_ms, answer);
#else
  AssertError(false, return false, "[MPLANE] Unknown M-plane version found. Tried \"MPLANE_V1\" and \"MPLANE_V2\".\n");
#endif
}
