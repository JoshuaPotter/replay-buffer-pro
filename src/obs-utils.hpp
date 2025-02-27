/**
 * @file obs_utils.hpp
 * @brief Utility classes and functions for OBS Studio integration
 * @author Joshua Potter
 * @copyright GPL v2 or later
 *
 * This file provides utility classes and functions for working with
 * OBS Studio APIs, including RAII wrappers for OBS data structures.
 */

#pragma once

// OBS includes
#include <obs-module.h>

namespace ReplayBufferPro
{

  /**
   * @brief RAII wrapper for obs_data_t structures
   *
   * This class provides automatic resource management for OBS data objects,
   * ensuring they are properly released when no longer needed.
   */
  class OBSDataRAII
  {
  private:
    obs_data_t *data;

  public:
    /**
     * @brief Constructor that takes ownership of an OBS data object
     * @param d Pointer to the OBS data object to manage
     */
    explicit OBSDataRAII(obs_data_t *d);

    /**
     * @brief Destructor that releases the managed OBS data object
     */
    ~OBSDataRAII();

    /**
     * @brief Get the managed OBS data object
     * @return Pointer to the managed OBS data object
     */
    obs_data_t *get() const;

    /**
     * @brief Check if the managed OBS data object is valid
     * @return true if the managed object is not null, false otherwise
     */
    bool isValid() const;

    // Prevent copying
    OBSDataRAII(const OBSDataRAII &) = delete;
    OBSDataRAII &operator=(const OBSDataRAII &) = delete;
  };

} // namespace ReplayBufferPro