/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "codec_defaults.h"

#include "codec_contract.h"
#include "codec_program.h"
#include "sigma_exports.h"

#include <errno.h>
#include <string.h>

static int copy_from_sigma(sub_addr_t start_address, uint8_t* destination, size_t size)
{
  size_t offset;

  if (start_address < sigma_parameter_address) {
    return -ERANGE;
  }

  offset = (size_t)(start_address - sigma_parameter_address);
  if (offset > sigma_parameter_size || size > sigma_parameter_size - offset) {
    return -ERANGE;
  }

  memcpy(destination, &Param_Data_IC_1_Sigma[offset], size);
  return 0;
}

int codec_defaults_copy(uint8_t id, uint8_t* destination, size_t size)
{
  const struct codec_parameter* parameter = codec_contract_find(id);
  sub_addr_t start_address;
  int ret;

  if (parameter == NULL) {
    return -ENOENT;
  }
  if (destination == NULL || size != parameter->byte_count) {
    return -EINVAL;
  }

  ret = get_param_address(parameter->id, &start_address);
  if (ret != 0) {
    return ret;
  }

  return copy_from_sigma(start_address, destination, size);
}
