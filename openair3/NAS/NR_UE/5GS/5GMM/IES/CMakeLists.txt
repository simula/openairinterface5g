add_library(fgs_5gmm_ies_lib OBJECT
  FGSMobileIdentity.c
  FGSRegistrationType.c
  FGMMCapability.c
  NrUESecurityCapability.c
  FGCNasMessageContainer.c
  SORTransparentContainer.c
)

target_link_libraries(fgs_5gmm_ies_lib
	                PUBLIC nas_common_ies_lib
			PRIVATE log_headers
)

target_include_directories(fgs_5gmm_ies_lib PRIVATE ../../)
