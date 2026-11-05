# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 Yizhan Wu
idf_component_register(SRCS "main.cpp" INCLUDE_DIRS "." REQUIRES reference nvs_flash esp_timer bt)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
