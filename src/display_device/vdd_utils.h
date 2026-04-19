#pragma once

#define WIN32_LEAN_AND_MEAN

#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <windows.h>

#include "parsed_config.h"

namespace display_device::vdd_utils {

  using namespace std::chrono_literals;

  // Constant definitions
  inline constexpr int kMaxRetryCount = 3;
  inline constexpr auto kInitialRetryDelay = 500ms;
  inline constexpr auto kMaxRetryDelay = 3000ms;

  extern const wchar_t *kVddPipeName;
  extern const DWORD kPipeTimeoutMs;
  extern const DWORD kPipeBufferSize;
  extern const std::chrono::milliseconds kDefaultDebounceInterval;

  // HDR brightness range struct
  struct hdr_brightness_t {
    float max_nits = 1000.0f;
    float min_nits = 0.001f;
    float max_full_nits = 1000.0f;
  };

  // Physical size struct (centimeters)
  struct physical_size_t {
    float width_cm = 0.0f;   // Width in cm; 0 means unspecified
    float height_cm = 0.0f;  // Height in cm; 0 means unspecified
  };

  // Retry configuration struct
  struct RetryConfig {
    int max_attempts = kMaxRetryCount;
    std::chrono::milliseconds initial_delay = kInitialRetryDelay;
    std::chrono::milliseconds max_delay = kMaxRetryDelay;
    std::string_view context;
  };

  // VDD settings struct
  struct VddSettings {
    std::string resolutions;
    std::string fps;
    bool needs_update = false;
  };

  // Exponential backoff calculation
  std::chrono::milliseconds
  calculate_exponential_backoff(int attempt);

  // VDD command execution
  bool
  execute_vdd_command(const std::string &action);

  // Pipe-related functions
  HANDLE
  connect_to_pipe_with_retry(const wchar_t *pipe_name, int max_retries = 3);

  bool
  execute_pipe_command(const wchar_t *pipe_name, const wchar_t *command, std::string *response = nullptr, bool *timed_out = nullptr);

  // Driver reload function
  bool
  reload_driver();

  /**
   * @brief Generate a GUID string from a client identifier (used for driver identification)
   * @param identifier Client identifier; returns an empty string if empty
   * @return GUID-formatted string: {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}, or empty if identifier is empty
   */
  std::string
  generate_client_guid(const std::string &identifier);

  /**
   * @brief Get the physical size from the client configuration
   * @param client_name Client name
   * @return Physical size struct, or default (0,0) if not found
   */
  physical_size_t
  get_client_physical_size(const std::string &client_name);

  /**
   * @brief Create the VDD monitor
   * @param client_identifier Client identifier (optional); used by the driver to identify the client and start the corresponding display
   * @param hdr_brightness HDR brightness configuration
   * @param physical_size Physical size configuration in centimeters (optional)
   * @return Whether creation succeeded
   */
  bool
  create_vdd_monitor(const std::string &client_identifier = "", const hdr_brightness_t &hdr_brightness = {}, const physical_size_t &physical_size = {});

  bool
  destroy_vdd_monitor();

  /**
   * @brief Shutdown-safe VDD destroy. Uses raw Win32 pipe API without BOOST_LOG.
   * Safe to call from destructors where boost::log may already be destroyed.
   */
  void
  destroy_vdd_monitor_nolog();

  void
  enable_vdd();

  void
  disable_vdd();

  void
  disable_enable_vdd();

  bool
  toggle_display_power();

  bool
  is_display_on();

  bool
  set_hdr_state(bool enable_hdr);

  bool
  ensure_vdd_extended_mode(const std::string &device_id, const std::unordered_set<std::string> &physical_devices_to_preserve = {});

  /**
   * @brief Apply VDD prep settings to handle physical displays.
   * @param vdd_device_id The VDD device ID.
   * @param vdd_prep The vdd_prep_e value specifying how to handle physical displays.
   * @param pre_vdd_devices Physical device info captured BEFORE VDD creation.
   *        Used to reliably identify physical displays even if VDD creation
   *        caused them to become inactive. If empty, falls back to current device enumeration.
   * @returns True if the operation succeeded.
   * @note This operation modifies topology without saving/restoring state,
   *       as Windows automatically handles topology memory when displays change.
   */
  bool
  apply_vdd_prep(const std::string &vdd_device_id, parsed_config_t::vdd_prep_e vdd_prep,
    const device_info_map_t &pre_vdd_devices = {});

  VddSettings
  prepare_vdd_settings(const parsed_config_t &config);

  // Retry function template
  template <typename Func>
  bool
  retry_with_backoff(Func &&check_func, const RetryConfig &config) {
    auto delay = config.initial_delay;

    for (int attempt = 0; attempt < config.max_attempts; ++attempt) {
      if (check_func()) {
        return true;
      }

      if (attempt + 1 < config.max_attempts) {
        std::this_thread::sleep_for(delay);
        delay = std::min(config.max_delay, delay * 2);
      }
    }
    return false;
  }

}  // namespace display_device::vdd_utils