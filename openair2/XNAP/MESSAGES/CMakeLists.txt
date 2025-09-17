set(XNAP_VERSION 16 2 0)
make_version(XNAP_cc ${XNAP_VERSION})
#add_definitions(-DXNAP_VERSION=${XNAP_cc})
string(REPLACE ";" "." XNAP_RELEASE "${XNAP_VERSION}")

if(XNAP_RELEASE VERSION_EQUAL "16.2.0")
  include(ASN1/xnap_R16.2.0.cmake)
else()
  message(FATAL_ERROR "unknown XNAP_RELEASE ${XNAP_RELEASE}")
endif()

run_asn1c(${CMAKE_CURRENT_SOURCE_DIR}/${XNAP_GRAMMAR}
          "XNAP_"
          OUTPUT ${xnap_source} ${xnap_headers}
          OPTIONS -pdu=all -gen-APER -no-gen-BER -no-gen-JER -no-gen-OER -gen-UPER -fcompound-names -no-gen-example -fno-include-deps -findirect-choice
          COMMENT "XnAP source files"
)

add_library(asn1_xnap ${xnap_source})
target_include_directories(asn1_xnap PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(asn1_xnap PRIVATE -DASN_DISABLE_OER_SUPPORT -w)

