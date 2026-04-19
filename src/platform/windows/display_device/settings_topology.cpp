// standard includes
#include <thread>

// local includes
#include "settings_topology.h"
#include "src/display_device/to_string.h"
#include "src/globals.h"
#include "src/logging.h"

namespace display_device {

  namespace {
    /**
     * @brief Based on the initial topology, fill in devices that are currently inactive but should be restored
     * @param base_topology Base topology (typically the current topology)
     * @param requested_device_id Requested device ID
     * @param initial_topology_devices List of devices in the initial topology (only fill in these devices)
     * @return The augmented topology
     */
    active_topology_t
    augment_topology_with_inactive_devices(
      const active_topology_t &base_topology,
      const std::string &requested_device_id,
      const boost::optional<std::unordered_set<std::string>> &initial_topology_devices = boost::none) {

      // First copy the base as a candidate result
      active_topology_t augmented_topology { base_topology };

      // Collect device ids already in the topology to avoid duplicates
      const auto existing_ids { get_device_ids_from_topology(augmented_topology) };

      const auto available_devices { enum_available_devices() };
      if (available_devices.empty()) {
        return base_topology;
      }

      // If a list of initial topology devices was provided, only fill in those devices.
      // Otherwise fill in all inactive devices (legacy behavior).
      if (initial_topology_devices && !initial_topology_devices->empty()) {
        BOOST_LOG(debug) << "Augmenting topology based on initial topology devices (respecting user's original configuration)";

        for (const auto &device_id : *initial_topology_devices) {
          // Devices already in the topology don't need further processing
          if (existing_ids.count(device_id) > 0) {
            continue;
          }

          // Check whether the device is available and currently inactive
          auto device_it = available_devices.find(device_id);
          if (device_it == available_devices.end()) {
            BOOST_LOG(debug) << "Device from initial topology not available: " << device_id;
            continue;
          }

          if (device_it->second.device_state != device_state_e::inactive) {
            // Device is already active or in another state; nothing to fill in
            continue;
          }

          BOOST_LOG(debug) << "Augmenting topology with device from initial topology: " << device_id;
          augmented_topology.push_back({ device_id });
        }
      }
      else {
        // Scenario without an initial topology constraint (non-VDD).
        // The real purpose of augment_topology should be to re-activate devices that ended up inactive in
        // final_topology for some reason. But determine_final_topology has already decided which devices to
        // activate; we shouldn't automatically add new devices here.
        // So just return base_topology without any augmentation.
        BOOST_LOG(debug) << "No initial topology constraint, relying on determine_final_topology result without augmentation";
        return base_topology;
      }

      // If the augmented topology is invalid, conservatively fall back to the original topology
      // to avoid putting the system into a strange state
      if (!augmented_topology.empty() && !is_topology_valid(augmented_topology)) {
        BOOST_LOG(warning) << "Augmented display topology is invalid, falling back to original topology.";
        return base_topology;
      }

      return augmented_topology;
    }

    /**
     * @brief Get all device ids that belong in the same group as provided ids (duplicated displays).
     * @param device_id Device id to search for in the topology.
     * @param topology Topology to search.
     * @return A list of device ids, with the provided device id always at the front.
     *
     * EXAMPLES:
     * ```cpp
     * const auto duplicated_devices = get_duplicate_devices("MY_DEVICE_ID", get_current_topology());
     * ```
     */
    std::vector<std::string>
    get_duplicate_devices(const std::string &device_id, const active_topology_t &topology) {
      std::vector<std::string> duplicated_devices;

      duplicated_devices.clear();
      duplicated_devices.push_back(device_id);

      for (const auto &group : topology) {
        for (const auto &group_device_id : group) {
          if (device_id == group_device_id) {
            std::copy_if(std::begin(group), std::end(group), std::back_inserter(duplicated_devices), [&](const auto &id) {
              return id != device_id;
            });
            break;
          }
        }
      }

      return duplicated_devices;
    }

    /**
     * @brief Check if device id is found in the active topology.
     * @param device_id Device id to search for in the topology.
     * @param topology Topology to search.
     * @return True if device id is in the topology, false otherwise.
     *
     * EXAMPLES:
     * ```cpp
     * const bool is_in_topology = is_device_found_in_active_topology("MY_DEVICE_ID", get_current_topology());
     * ```
     */
    bool
    is_device_found_in_active_topology(const std::string &device_id, const active_topology_t &topology) {
      for (const auto &group : topology) {
        for (const auto &group_device_id : group) {
          if (device_id == group_device_id) {
            return true;
          }
        }
      }

      return false;
    }

    /**
     * @brief Compute the final topology based on the information we have.
     * @param device_prep The device preparation setting from user configuration.
     * @param primary_device_requested  Indicates that the user did NOT specify device id to be used.
     * @param duplicated_devices Devices that we need to handle.
     * @param topology The current topology that we are evaluating.
     * @return Topology that matches requirements and should be set.
     */
    active_topology_t
    determine_final_topology(parsed_config_t::device_prep_e device_prep, const bool primary_device_requested, const std::vector<std::string> &duplicated_devices, const active_topology_t &topology) {
      boost::optional<active_topology_t> final_topology;

      const bool topology_change_requested { device_prep != parsed_config_t::device_prep_e::no_operation };
      if (topology_change_requested) {
        if (device_prep == parsed_config_t::device_prep_e::ensure_only_display) {
          // Device needs to be the only one that's active or if it's a PRIMARY device,
          // only the whole PRIMARY group needs to be active (in case they are duplicated)

          if (primary_device_requested) {
            if (topology.size() > 1) {
              // There are other topology groups other than the primary devices,
              // so we need to change that
              final_topology = active_topology_t { { duplicated_devices } };
            }
            else {
              // Primary device group is the only one active, nothing to do
            }
          }
          else {
            // Since primary_device_requested == false, it means a device was specified via config by the user
            // and is the only device that needs to be enabled

            if (is_device_found_in_active_topology(duplicated_devices.front(), topology)) {
              // Device is currently active in the active topology group

              if (duplicated_devices.size() > 1 || topology.size() > 1) {
                // We have more than 1 device in the group, or we have more than 1 topology groups.
                // We need to disable all other devices
                final_topology = active_topology_t { { duplicated_devices.front() } };
              }
              else {
                // Our device is the only one that's active, nothing to do
              }
            }
            else {
              // Our device is not active, we need to activate it and ONLY it
              final_topology = active_topology_t { { duplicated_devices.front() } };
            }
          }
        }
        // device_prep_e::ensure_active || device_prep_e::ensure_primary
        else {
          //  The device needs to be active at least.

          if (primary_device_requested || is_device_found_in_active_topology(duplicated_devices.front(), topology)) {
            // Device is already active, nothing to do here
          }
          else {
            // Create the extended topology as it's probably what makes sense the most...
            final_topology = topology;
            final_topology->push_back({ duplicated_devices.front() });
          }
        }
      }

      return final_topology ? *final_topology : topology;
    }

  }  // namespace

  std::unordered_set<std::string>
  remove_vdd_from_topology(active_topology_t &topology) {
    std::unordered_set<std::string> removed_device_ids;
    
    // Get list of available devices (includes both active and inactive devices)
    // This ensures we don't remove inactive devices that can be re-enabled
    const auto available_devices = enum_available_devices();
    std::unordered_set<std::string> available_device_ids;
    for (const auto &[device_id, info] : available_devices) {
      // Include all devices (active, inactive, primary) - they all can potentially be used
      available_device_ids.insert(device_id);
    }

    for (auto &group : topology) {
      auto new_end = std::remove_if(group.begin(), group.end(),
        [&removed_device_ids, &available_device_ids](const std::string &device_id) {
          // First check if device exists in available devices
          // Note: available_devices includes inactive devices, so inactive devices will pass this check
          const bool device_exists = available_device_ids.count(device_id) > 0;
          
          if (!device_exists) {
            // Device doesn't exist in available devices at all - remove it
            // This means the device was truly destroyed (e.g., VDD uninstalled, physical display disconnected)
            // It's safe to remove as it cannot be re-enabled
            BOOST_LOG(debug) << "Removing non-existent device from topology: " << device_id;
            removed_device_ids.insert(device_id);  // Track removed ID
            return true;
          }
          
          // Device exists (could be active or inactive), check if it's VDD by friendly name
          // Only remove if it's VDD - inactive physical displays will be preserved
          const auto friendly_name = get_display_friendly_name(device_id);
          if (friendly_name == ZAKO_NAME) {
            BOOST_LOG(debug) << "Removing VDD device from topology: " << device_id;
            removed_device_ids.insert(device_id);  // Track removed ID
            return true;
          }
          
          // Device exists and is not VDD - preserve it (even if inactive, it can be re-enabled)
          return false;
        });
      group.erase(new_end, group.end());
    }

    // Remove empty groups
    topology.erase(
      std::remove_if(topology.begin(), topology.end(),
        [](const auto &group) { return group.empty(); }),
      topology.end());

    return removed_device_ids;
  }

  /**
   * @brief Enumerate and get one of the devices matching the id or
   *        any of the primary devices if id is unspecified.
   * @param device_id Id to find in enumerated devices.
   * @return Device id, or empty string if an error has occurred.
   *
   * EXAMPLES:
   * ```cpp
   * const std::string primary_device = find_one_of_the_available_devices("");
   * const std::string id_that_matches_provided_id = find_one_of_the_available_devices(primary_device);
   * ```
   */
  std::string
  find_one_of_the_available_devices(const std::string &device_id) {
    const auto devices { enum_available_devices() };
    if (devices.empty()) {
      // Transient during display reinit or right after VDD create; avoid error level
      BOOST_LOG(warning) << "Find one of the available devices: display device list is empty!";
      return {};
    }
    BOOST_LOG(info) << "Available display devices: " << to_string(devices);

    const auto device_it { std::find_if(std::begin(devices), std::end(devices), [&device_id](const auto &entry) {
      return device_id.empty() ? entry.second.device_state == device_state_e::primary : entry.first == device_id;
    }) };
    if (device_it == std::end(devices)) {
      BOOST_LOG(warning) << "Device " << (device_id.empty() ? "PRIMARY" : device_id) << " not found in the list of available devices!";
      return {};
    }

    return device_it->first;
  }

  std::unordered_set<std::string>
  get_device_ids_from_topology(const active_topology_t &topology) {
    std::unordered_set<std::string> device_ids;
    for (const auto &group : topology) {
      for (const auto &device_id : group) {
        device_ids.insert(device_id);
      }
    }

    return device_ids;
  }

  std::unordered_set<std::string>
  get_newly_enabled_devices_from_topology(const active_topology_t &previous_topology, const active_topology_t &new_topology) {
    const auto prev_ids { get_device_ids_from_topology(previous_topology) };
    auto new_ids { get_device_ids_from_topology(new_topology) };

    for (auto &id : prev_ids) {
      new_ids.erase(id);
    }

    return new_ids;
  }

  boost::optional<handled_topology_result_t>
  handle_device_topology_configuration(
    const parsed_config_t &config,
    const boost::optional<topology_pair_t> &previously_configured_topology,
    const std::function<bool()> &revert_settings,
    const boost::optional<active_topology_t> &pre_saved_initial_topology) {
    const bool primary_device_requested { config.device_id.empty() };
    const std::string requested_device_id { find_one_of_the_available_devices(config.device_id) };
    if (requested_device_id.empty()) {
      // Error already logged
      return boost::none;
    }

    // If we still have a previously configured topology, we could potentially skip making any changes to the topology.
    // However, it could also mean that we need to revert any previous changes in case the final topology has changed somehow.
    if (previously_configured_topology) {
      // Here we are pretending to be in an initial topology and want to perform reevaluation in case the
      // user has changed the settings while the stream was paused. For the proper "evaluation" order,
      // see logic outside this conditional.
      const auto prev_duplicated_devices { get_duplicate_devices(requested_device_id, previously_configured_topology->initial) };
      auto prev_final_topology { determine_final_topology(config.device_prep, primary_device_requested, prev_duplicated_devices, previously_configured_topology->initial) };

      // Match the current implementation: outside of "ensure-only-display" mode, also augment the "historical
      // expected topology" so that the comparison doesn't trigger a meaningless rollback and re-switch caused
      // by inactive devices we added ourselves.
      if (config.device_prep != parsed_config_t::device_prep_e::ensure_only_display) {
        prev_final_topology = augment_topology_with_inactive_devices(prev_final_topology, requested_device_id);
      }

      // There is also an edge case where we can have a different number of primary duplicated devices, which wasn't the case
      // during the initial topology configuration. If the user requested to use the primary device,
      // the prev_final_topology would not reflect that change in primary duplicated devices. Therefore, we also need
      // to evaluate current topology (which would have the new state of primary devices) and arrive at the
      // same final topology as the prev_final_topology.
      const auto current_topology { get_current_topology() };
      const auto duplicated_devices { get_duplicate_devices(requested_device_id, current_topology) };
      auto final_topology { determine_final_topology(config.device_prep, primary_device_requested, duplicated_devices, current_topology) };

      if (config.device_prep != parsed_config_t::device_prep_e::ensure_only_display) {
        final_topology = augment_topology_with_inactive_devices(final_topology, requested_device_id);
      }

      // If the topology we are switching to is the same as the final topology we had before, that means
      // user did not change anything, and we don't need to revert changes.
      if (!is_topology_the_same(previously_configured_topology->modified, prev_final_topology) ||
          !is_topology_the_same(previously_configured_topology->modified, final_topology)) {
        BOOST_LOG(warning) << "Previous topology does not match the new one. Reverting previous changes!";
        if (!revert_settings()) {
          return boost::none;
        }
      }
    }

    // Regardless of whether the user has made any changes to the user configuration or not, we always
    // need to evaluate the current topology and perform the switch if needed as the user might
    // have been playing around with active displays while the stream was paused.

    const auto current_topology { get_current_topology() };
    if (!is_topology_valid(current_topology)) {
      BOOST_LOG(error) << "Display topology is invalid!";
      return boost::none;
    }

    // When dealing with the "requested device" here and in other functions we need to keep
    // in mind that it could belong to a duplicated display and thus all of them
    // need to be taken into account, which complicates everything...
    
    // In the VDD scenario, use the real initial topology to compute duplicated_devices and final_topology
    // so that the target topology is built from the user's real pre-stream state.
    const auto &topology_for_calculation = pre_saved_initial_topology ? *pre_saved_initial_topology : current_topology;

    auto duplicated_devices { get_duplicate_devices(requested_device_id, topology_for_calculation) };
    auto final_topology { determine_final_topology(config.device_prep, primary_device_requested, duplicated_devices, topology_for_calculation) };

    // Only call augment_topology in specific modes:
    //   - no_operation mode: skip (don't adjust anything)
    //   - ensure_only_display mode: skip (only enable the specified device, no augmentation)
    if (config.device_prep != parsed_config_t::device_prep_e::ensure_only_display &&
        config.device_prep != parsed_config_t::device_prep_e::no_operation) {
      // If a pre-saved initial topology exists (VDD scenario), only augment with devices in the initial topology
      // so we respect the user's original configuration (don't enable displays the user manually disabled)
      if (pre_saved_initial_topology) {
        const auto initial_devices = get_device_ids_from_topology(*pre_saved_initial_topology);
        BOOST_LOG(debug) << "Augmenting topology with constraints from initial topology (VDD scenario)";
        final_topology = augment_topology_with_inactive_devices(final_topology, requested_device_id, initial_devices);
      }
    }

    BOOST_LOG(debug) << "Current display topology: " << to_string(current_topology);
    if (!is_topology_the_same(current_topology, final_topology)) {
      BOOST_LOG(info) << "Changing display topology to: " << to_string(final_topology);
      if (!set_topology(final_topology)) {
        // Error already logged.
        return boost::none;
      }

      // It is possible that we no longer have duplicate displays, so we need to update the list
      duplicated_devices = get_duplicate_devices(requested_device_id, final_topology);
    }

    // This check is mainly to cover the case for "config.device_prep == no_operation" as we at least
    // have to validate that the device exists, but it doesn't hurt to double-check it in all cases.
    if (!is_device_found_in_active_topology(requested_device_id, final_topology)) {
      BOOST_LOG(error) << "Device " << requested_device_id << " is not active!";
      return boost::none;
    }

    // If a pre-saved initial topology exists (saved before VDD creation), use it as the real initial topology.
    // Otherwise use the current topology (which may have been disturbed by VDD).
    const auto real_initial_topology = pre_saved_initial_topology ? *pre_saved_initial_topology : current_topology;

    return handled_topology_result_t {
      topology_pair_t {
        real_initial_topology,  // Use the real initial topology
        final_topology },
      topology_metadata_t {
        final_topology,
        get_newly_enabled_devices_from_topology(current_topology, final_topology),
        primary_device_requested,
        duplicated_devices }
    };
  }

  boost::optional<handled_topology_result_t>
  get_current_topology_metadata(const std::string &device_id) {
    const std::string requested_device_id { find_one_of_the_available_devices(device_id) };
    if (requested_device_id.empty()) {
      BOOST_LOG(error) << "Device not found: " << device_id;
      return boost::none;
    }

    // Fetch the active topology and check that the device is available; retry to ride out brief
    // instability following HDR/topology changes
    active_topology_t current_topology;
    bool device_active = false;
    constexpr int max_retries = 3;
    constexpr auto retry_delay = std::chrono::milliseconds(500);

    for (int attempt = 0; attempt < max_retries; ++attempt) {
      current_topology = get_current_topology();
      if (!is_topology_valid(current_topology)) {
        BOOST_LOG(warning) << "Display topology is invalid (attempt " << (attempt + 1) << "/" << max_retries << ")";
        if (attempt + 1 < max_retries) {
          std::this_thread::sleep_for(retry_delay);
          continue;
        }
        BOOST_LOG(error) << "Display topology is invalid after all retries!";
        return boost::none;
      }

      if (is_device_found_in_active_topology(requested_device_id, current_topology)) {
        device_active = true;
        break;
      }

      BOOST_LOG(warning) << "Device " << requested_device_id << " is not active (attempt " << (attempt + 1) << "/" << max_retries << "), waiting for display to stabilize...";
      if (attempt + 1 < max_retries) {
        std::this_thread::sleep_for(retry_delay);
      }
    }

    if (!device_active) {
      BOOST_LOG(error) << "Device " << requested_device_id << " is not active after " << max_retries << " retries!";
      return boost::none;
    }

    const bool primary_device_requested { device_id.empty() };
    const auto duplicated_devices { get_duplicate_devices(requested_device_id, current_topology) };

    // VDD mode: do not modify the topology; use the current topology as both initial and modified
    return handled_topology_result_t {
      topology_pair_t {
        current_topology,
        current_topology },
      topology_metadata_t {
        current_topology,
        {},  // No newly-enabled devices
        primary_device_requested,
        duplicated_devices }
    };
  }

}  // namespace display_device
