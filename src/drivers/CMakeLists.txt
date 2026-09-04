#
# Copyright (c) 2022 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

# Compile the driver on every supported board so its API and SigmaStudio export
# remain available during codec integration. CONFIG_AUDIO_CODEC_ADAU1787 gates
# hardware behavior in Codec Controller, not driver availability.
target_sources(app PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/adau1787.c)

# Make header files available to the rest of the project
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
