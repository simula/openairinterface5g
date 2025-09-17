add_library(nr_phy_common src/nr_phy_common.c)
target_link_libraries(nr_phy_common PRIVATE UTIL PHY_COMMON)
target_include_directories(nr_phy_common PUBLIC inc/)

add_library(nr_ue_phy_meas src/nr_ue_phy_meas.c)
target_include_directories(nr_ue_phy_meas PUBLIC inc/)
target_link_libraries(nr_ue_phy_meas PUBLIC utils)
