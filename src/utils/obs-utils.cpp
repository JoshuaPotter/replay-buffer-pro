/**
 * @file obs-utils.cpp
 * @brief Implementation of utility classes and functions for OBS Studio integration
 * @author Joshua Potter
 * @copyright GPL v2 or later
 *
 * This file implements utility classes and functions for working with
 * OBS Studio APIs, including RAII wrappers for OBS data structures.
 */

#include "obs-utils.hpp"

namespace ReplayBufferPro
{

  OBSDataRAII::OBSDataRAII(obs_data_t *d) : data(d) {}

  OBSDataRAII::~OBSDataRAII()
  {
    if (data)
      obs_data_release(data);
  }

  obs_data_t *OBSDataRAII::get() const
  {
    return data;
  }

  bool OBSDataRAII::isValid() const
  {
    return data != nullptr;
  }

} // namespace ReplayBufferPro