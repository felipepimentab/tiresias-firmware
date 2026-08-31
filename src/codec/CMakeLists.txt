#
# Copyright (c) 2026 Tiresias Firmware Team
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

target_sources(app PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/codec_controller.c
               ${CMAKE_CURRENT_SOURCE_DIR}/codec_controller_actions.c
               ${CMAKE_CURRENT_SOURCE_DIR}/codec_contract.c
               ${CMAKE_CURRENT_SOURCE_DIR}/codec_parameters.c
               ${CMAKE_CURRENT_SOURCE_DIR}/codec_settings.c
               ${CMAKE_CURRENT_SOURCE_DIR}/codec_values.c
               ${CMAKE_CURRENT_SOURCE_DIR}/codec_adapter.c
               ${CMAKE_CURRENT_SOURCE_DIR}/sigma_exports.c
               ${CMAKE_CURRENT_SOURCE_DIR}/hw_codec.c
)
