//Dummy NR defs to avoid linking errors

#include "PHY/defs_gNB.h"
#include "nfapi/open-nFAPI/nfapi/public_inc/nfapi_nr_interface_scf.h"
#include "openair2/NR_PHY_INTERFACE/NR_IF_Module.h"
#include "openair1/PHY/LTE_TRANSPORT/transport_common.h"
typedef struct pnf_t pnf_t;
typedef struct pnf_p7_t pnf_p7_t;
typedef struct vnf_t vnf_t;
typedef struct vnf_p7_t vnf_p7_t;

void handle_nfapi_nr_pdcch_pdu(PHY_VARS_gNB *gNB,
			       int frame, int slot,
			       nfapi_nr_dl_tti_pdcch_pdu *pdcch_pdu){}

void handle_nr_nfapi_ssb_pdu(PHY_VARS_gNB *gNB,int frame,int slot,
                             nfapi_nr_dl_tti_request_pdu_t *dl_tti_pdu){}

void handle_nr_nfapi_pdsch_pdu(processingData_L1tx_t *msgTx,
			       nfapi_nr_dl_tti_pdsch_pdu *pdsch_pdu,
                            uint8_t *sdu){
}
void handle_nfapi_nr_csirs_pdu(processingData_L1tx_t *msgTx,
			       int frame, int slot,
			       nfapi_nr_dl_tti_csi_rs_pdu *csirs_pdu){
                    }                           
int l1_north_init_gNB(void){return 0;}

int slot_ahead = 6;
//uint8_t nfapi_mode=0;
NR_IF_Module_t *NR_IF_Module_init(int Mod_id) {return NULL;}

void handle_nfapi_nr_ul_dci_pdu(PHY_VARS_gNB *gNB,
			       int frame, int slot,
			       nfapi_nr_ul_dci_request_pdus_t *ul_dci_request_pdu){}
void nr_fill_ulsch(PHY_VARS_gNB *gNB,
                   int frame,
                   int slot,
                   nfapi_nr_pusch_pdu_t *ulsch_pdu){}
void nr_fill_pucch(PHY_VARS_gNB *gNB,
                   int frame,
                   int slot,
                   nfapi_nr_pucch_pdu_t *pucch_pdu){}
void nr_fill_prach(PHY_VARS_gNB *gNB,
                   int SFN,
                   int Slot,
                   nfapi_nr_prach_pdu_t *prach_pdu){}
void nr_fill_prach_ru(RU_t *ru,
                      int SFN,
                      int Slot,
                      nfapi_nr_prach_pdu_t *prach_pdu){}
                   
void  nr_phy_config_request(NR_PHY_Config_t *gNB){}

void install_nr_schedule_handlers(NR_IF_Module_t *if_inst){}

void nr_dump_frame_parms(NR_DL_FRAME_PARMS *fp){}

bool vnf_nr_send_p5_msg(vnf_t *vnf, uint16_t p5_idx, nfapi_nr_p4_p5_message_header_t *msg, uint32_t msg_len)
{
  return false;
}

bool vnf_nr_send_p7_msg(vnf_p7_t *vnf_p7, nfapi_nr_p7_message_header_t *header)
{
  return false;
}

void *vnf_nr_start_p7_thread(void *ptr)
{
  return 0;
}

bool pnf_nr_send_p5_message(pnf_t *pnf, nfapi_nr_p4_p5_message_header_t *msg, uint32_t msg_len)
{
  return false;
}

bool pnf_nr_send_p7_message(pnf_p7_t *pnf_p7, nfapi_nr_p7_message_header_t *header, uint32_t msg_len)
{
  return false;
}

void *pnf_start_p5_thread(void *ptr)
{
  return 0;
}

void vnf_start_p5_thread(void *ptr)
{
}

void *pnf_nr_p7_thread_start(void *ptr)
{
  return 0;
}

bool wls_vnf_nr_send_p7_message(vnf_p7_t *vnf_p7, nfapi_nr_p7_message_header_t *msg)
{
  return false;
}

void wls_vnf_set_p7_config(void *p7_config)
{
}

bool wls_vnf_nr_send_p5_message(vnf_t *vnf,uint16_t p5_idx, nfapi_nr_p4_p5_message_header_t* msg, uint32_t msg_len)
{
  return false;
}

void *wls_fapi_vnf_nr_start_thread(void *ptr)
{
  return NULL;
}

bool wls_pnf_nr_send_p7_message(pnf_p7_t* pnf_p7,nfapi_nr_p7_message_header_t *msg, uint32_t msg_len)
{
  return false;
}

bool wls_pnf_nr_send_p5_message(pnf_t *pnf, nfapi_nr_p4_p5_message_header_t *msg, uint32_t msg_len)
{
  return false;
}

void *wls_fapi_pnf_nr_start_thread(void *ptr)
{
  return NULL;
}

void wls_pnf_set_p7_config(void *p7_config)
{

}
