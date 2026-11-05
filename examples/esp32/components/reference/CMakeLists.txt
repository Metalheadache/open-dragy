# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 Yizhan Wu
set(reference_root "${CMAKE_CURRENT_LIST_DIR}/../../../..")
idf_component_register(
  SRCS "${reference_root}/src/telemetry.cpp" "${reference_root}/src/drag.cpp"
       "${reference_root}/src/speed_filter.cpp" "${reference_root}/src/geometry.cpp"
  INCLUDE_DIRS "${reference_root}/include")
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
