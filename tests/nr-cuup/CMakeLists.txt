add_executable(nr-cuup-load-test nr-cuup-load-test.c)
target_link_libraries(nr-cuup-load-test PRIVATE GTPV1U ${T_LIB} utils ITTI e1ap
  SCTP_CLIENT)
target_link_libraries(nr-cuup-load-test PRIVATE asn1_lte_rrc_hdrs asn1_nr_rrc_hdrs) # ITTI
