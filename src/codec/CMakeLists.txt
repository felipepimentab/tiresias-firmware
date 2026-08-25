#
# Copyright (c) 2026 Tiresias Firmware Team
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

target_sources(app PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/codec_controller.c
               ${CMAKE_CURRENT_SOURCE_DIR}/codec_controller_actions.c
               ${CMAKE_CURRENT_SOURCE_DIR}/dsp_parameter_catalog.c
               ${CMAKE_CURRENT_SOURCE_DIR}/dsp_parameter_controller.c
               ${CMAKE_CURRENT_SOURCE_DIR}/dsp_parameter_settings.c
)

target_sources_ifdef(CONFIG_AUDIO_CODEC_ADAU1787 app PRIVATE
                     ${CMAKE_CURRENT_SOURCE_DIR}/codec_adapter.c
                     ${CMAKE_CURRENT_SOURCE_DIR}/hw_codec.c
                     ${CMAKE_CURRENT_SOURCE_DIR}/param_defaults.c)
