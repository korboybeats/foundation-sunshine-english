#define WIN32_LEAN_AND_MEAN

#include "vdd_utils.h"

#include <algorithm>
#include <boost/filesystem.hpp>
#include <boost/process/v1.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/uuid/name_generator_sha1.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <filesystem>
#include <future>
#include <sstream>
#include <thread>
#include <unordered_set>
#include <vector>

#include "src/confighttp.h"
#include "src/globals.h"
#include "src/platform/common.h"
#include "src/platform/run_command.h"
#include "src/platform/windows/display_device/windows_utils.h"
#include "src/rtsp.h"
#include "src/system_tray.h"
#include "src/system_tray_i18n.h"
#include "to_string.h"

namespace pt = boost::property_tree;

namespace display_device {
  namespace vdd_utils {

    const wchar_t *kVddPipeName = L"\\\\.\\pipe\\ZakoVDDPipe";
    const DWORD kPipeTimeoutMs = 3000;
    const DWORD kPipeBufferSize = 4096;
    const std::chrono::milliseconds kDefaultDebounceInterval { 2000 };

    // Timestamp of the most recent display toggle
    static std::chrono::steady_clock::time_point last_toggle_time { std::chrono::steady_clock::now() };
    // Debounce interval
    static std::chrono::milliseconds debounce_interval { kDefaultDebounceInterval };
    // The most recently used client UUID; reused when none is provided
    static std::string last_used_client_uuid;

    std::chrono::milliseconds
    calculate_exponential_backoff(int attempt) {
      auto delay = kInitialRetryDelay * (1 << attempt);
      return std::min(delay, kMaxRetryDelay);
    }

    /**
     * @brief Allowed DevManView actions for VDD driver management.
     */
    enum class vdd_action_e {
      enable,
      disable,
      disable_enable
    };

    /**
     * @brief Get the command-line argument string for a VDD action.
     */
    const char *
    vdd_action_to_string(vdd_action_e action) {
      switch (action) {
        case vdd_action_e::enable: return "enable";
        case vdd_action_e::disable: return "disable";
        case vdd_action_e::disable_enable: return "disable_enable";
        default: return nullptr;
      }
    }

    bool
    execute_vdd_command(vdd_action_e action) {
      static const std::string kDevManPath = (std::filesystem::path(SUNSHINE_ASSETS_DIR).parent_path() / "tools" / "DevManView.exe").string();
      static const std::string kDriverName = "Zako Display Adapter";

      const char *action_str = vdd_action_to_string(action);
      if (!action_str) {
        BOOST_LOG(error) << "Unknown VDD command action";
        return false;
      }

      boost::process::v1::environment _env = boost::this_process::environment();
      auto working_dir = boost::filesystem::path();
      std::error_code ec;

      std::string cmd = kDevManPath + " /" + action_str + " \"" + kDriverName + "\"";

      for (int attempt = 0; attempt < kMaxRetryCount; ++attempt) {
        auto child = platf::run_command(true, true, cmd, working_dir, _env, nullptr, ec, nullptr);
        if (!ec) {
          BOOST_LOG(info) << "Successfully executed VDD " << action_str << " command";
          child.detach();
          return true;
        }

        auto delay = calculate_exponential_backoff(attempt);
        BOOST_LOG(warning) << "Failed to execute VDD " << action_str << " command (attempt "
                           << (attempt + 1) << "/" << kMaxRetryCount
                           << "): " << ec.message() << ". Retrying in "
                           << delay.count() << "ms";
        std::this_thread::sleep_for(delay);
      }

      BOOST_LOG(error) << "Failed to execute VDD " << action_str << " command after maximum retries";
      return false;
    }

    HANDLE
    connect_to_pipe_with_retry(const wchar_t *pipe_name, int max_retries) {
      HANDLE hPipe = INVALID_HANDLE_VALUE;
      int attempt = 0;
      auto retry_delay = kInitialRetryDelay;

      while (attempt < max_retries) {
        hPipe = CreateFileW(
          pipe_name,
          GENERIC_READ | GENERIC_WRITE,
          0,
          NULL,
          OPEN_EXISTING,
          FILE_FLAG_OVERLAPPED,  // Use asynchronous IO
          NULL);

        if (hPipe != INVALID_HANDLE_VALUE) {
          DWORD mode = PIPE_READMODE_MESSAGE;
          if (SetNamedPipeHandleState(hPipe, &mode, NULL, NULL)) {
            return hPipe;
          }
          CloseHandle(hPipe);
        }

        ++attempt;
        retry_delay = calculate_exponential_backoff(attempt);
        std::this_thread::sleep_for(retry_delay);
      }
      return INVALID_HANDLE_VALUE;
    }

    bool
    execute_pipe_command(const wchar_t *pipe_name, const wchar_t *command, std::string *response, bool *timed_out) {
      auto hPipe = connect_to_pipe_with_retry(pipe_name);
      if (hPipe == INVALID_HANDLE_VALUE) {
        BOOST_LOG(error) << "Failed to connect to the MTT virtual display pipe after several retries";
        return false;
      }

      // RAII guard for pipe handle to prevent handle leak
      struct HandleGuard {
        HANDLE handle;
        ~HandleGuard() {
          if (handle && handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
        }
      } pipe_guard { hPipe };

      // Async IO structure
      OVERLAPPED overlapped = { 0 };
      overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

      HandleGuard event_guard { overlapped.hEvent };

      // Send the command (wide-character form)
      DWORD bytesWritten;
      size_t cmd_len = (wcslen(command) + 1) * sizeof(wchar_t);  // Includes the terminator
      if (!WriteFile(hPipe, command, (DWORD) cmd_len, &bytesWritten, &overlapped)) {
        if (GetLastError() != ERROR_IO_PENDING) {
          BOOST_LOG(error) << L"Failed to send command " << command << L", error code: " << GetLastError();
          return false;
        }

        // Wait for the write to complete
        DWORD waitResult = WaitForSingleObject(overlapped.hEvent, kPipeTimeoutMs);
        if (waitResult != WAIT_OBJECT_0) {
          BOOST_LOG(error) << L"Timed out sending command " << command;
          return false;
        }
      }

      // Read the response
      bool read_timed_out = false;
      if (response) {
        char buffer[kPipeBufferSize];
        DWORD bytesRead = 0;
        if (!ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, &overlapped)) {
          if (GetLastError() != ERROR_IO_PENDING) {
            BOOST_LOG(warning) << "Failed to read response, error code: " << GetLastError();
            return false;
          }

          DWORD waitResult = WaitForSingleObject(overlapped.hEvent, kPipeTimeoutMs);
          if (waitResult == WAIT_OBJECT_0 && GetOverlappedResult(hPipe, &overlapped, &bytesRead, FALSE)) {
            buffer[bytesRead] = '\0';
            *response = std::string(buffer, bytesRead);
          }
          else {
            read_timed_out = true;
            CancelIo(hPipe);
          }
        }
        else {
          // ReadFile completed synchronously
          buffer[bytesRead] = '\0';
          *response = std::string(buffer, bytesRead);
        }
      }

      if (timed_out) {
        *timed_out = read_timed_out;
      }
      return true;
    }

    bool
    reload_driver() {
      std::string response;
      return execute_pipe_command(kVddPipeName, L"RELOAD_DRIVER", &response);
    }

    std::string
    generate_client_guid(const std::string &identifier) {
      if (identifier.empty()) {
        return "";
      }

      // Use a SHA1 name generator so the same identifier always yields the same GUID
      static constexpr boost::uuids::uuid ns_id {};
      const auto boost_uuid = boost::uuids::name_generator_sha1 { ns_id }(
        reinterpret_cast<const unsigned char *>(identifier.c_str()),
        identifier.size());

      return "{" + boost::uuids::to_string(boost_uuid) + "}";
    }

    /**
     * @brief Get the physical size from the client configuration
     * @param client_name Client name
     * @return Physical size struct, or default (0,0) if not found
     */
    physical_size_t
    get_client_physical_size(const std::string &client_name) {
      if (client_name.empty()) {
        return {};
      }

      // Predefined size map
      static const std::unordered_map<std::string, physical_size_t> size_map = {
        { "small", { 13.3f, 7.5f } },  // Small device: ~6 inches, 16:9 ratio
        { "medium", { 34.5f, 19.4f } },  // Medium device: ~15.6 inches, 16:9 ratio
        { "large", { 70.8f, 39.8f } }  // Large device: ~32 inches, 16:9 ratio
      };

      try {
        pt::ptree clientArray;
        std::stringstream ss(config::nvhttp.clients);
        pt::read_json(ss, clientArray);

        for (const auto &client : clientArray) {
          if (client.second.get<std::string>("name", "") == client_name) {
            const std::string device_size = client.second.get<std::string>("deviceSize", "medium");
            auto it = size_map.find(device_size);
            return (it != size_map.end()) ? it->second : size_map.at("medium");
          }
        }
      }
      catch (const std::exception &e) {
        BOOST_LOG(debug) << "Failed to get client physical size: " << e.what();
      }

      return {};
    }

    bool
    create_vdd_monitor(const std::string &client_identifier, const hdr_brightness_t &hdr_brightness, const physical_size_t &physical_size) {
      std::string response;
      std::wstring command = L"CREATEMONITOR";

      // If no UUID was supplied, fall back to the most recently used UUID
      std::string identifier_to_use = client_identifier.empty() && !last_used_client_uuid.empty() ? last_used_client_uuid : client_identifier;

      if (identifier_to_use != client_identifier && !identifier_to_use.empty()) {
        BOOST_LOG(info) << "No client identifier provided; using last UUID: " << identifier_to_use;
      }

      // Generate the GUID and build the command
      std::string guid_str = generate_client_guid(identifier_to_use);
      if (!guid_str.empty()) {
        // Build the full parameter string: {GUID}:[max_nits,min_nits,maxFALL][widthCm,heightCm]
        std::ostringstream param_stream;
        param_stream << guid_str << ":[" << hdr_brightness.max_nits << "," << hdr_brightness.min_nits << "," << hdr_brightness.max_full_nits << "]";

        // Append the physical size if provided
        if (physical_size.width_cm > 0.0f && physical_size.height_cm > 0.0f) {
          param_stream << "[" << physical_size.width_cm << "," << physical_size.height_cm << "]";
        }

        std::string param_str = param_stream.str();

        // Convert to wide characters and append to the command
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, param_str.c_str(), -1, NULL, 0);
        if (size_needed > 0) {
          std::vector<wchar_t> param_wide(size_needed);
          MultiByteToWideChar(CP_UTF8, 0, param_str.c_str(), -1, param_wide.data(), size_needed);
          command += L" " + std::wstring(param_wide.data());
        }

        std::ostringstream log_stream;
        log_stream << "Creating virtual display, client identifier: " << identifier_to_use
                   << ", GUID: " << guid_str
                   << ", HDR brightness range: [" << hdr_brightness.max_nits << ", " << hdr_brightness.min_nits << ", " << hdr_brightness.max_full_nits << "]";
        if (physical_size.width_cm > 0.0f && physical_size.height_cm > 0.0f) {
          log_stream << ", physical size: [" << physical_size.width_cm << "cm, " << physical_size.height_cm << "cm]";
        }
        BOOST_LOG(info) << log_stream.str();
      }

      // If a valid UUID was used, remember it for future calls
      if (!identifier_to_use.empty()) {
        last_used_client_uuid = identifier_to_use;
      }

      // Try sending the command (with or without GUID)
      bool read_timed_out = false;
      bool success = execute_pipe_command(kVddPipeName, command.c_str(), &response, &read_timed_out);

      // If the GUID-form command failed, fall back to the non-GUID form (compatibility with older drivers)
      if (!success && !guid_str.empty()) {
        BOOST_LOG(warning) << "Command with GUID failed; falling back to the no-GUID command";
        read_timed_out = false;
        success = execute_pipe_command(kVddPipeName, L"CREATEMONITOR", &response, &read_timed_out);
      }

      if (!success) {
        BOOST_LOG(error) << "Failed to create virtual display";
        return false;
      }

#if defined SUNSHINE_TRAY && SUNSHINE_TRAY >= 1
      system_tray::update_vdd_menu();
#endif
      BOOST_LOG(info) << "Virtual display creation complete, response: " << response << " [return=" << (read_timed_out ? 1 : 0) << "]";
      return true;
    }

    bool
    destroy_vdd_monitor() {
      // If the VDD is already gone, return success immediately
      if (find_device_by_friendlyname(ZAKO_NAME).empty()) {
        BOOST_LOG(debug) << "VDD device no longer exists; skipping destroy";
        return true;
      }

      std::string response;
      if (!execute_pipe_command(kVddPipeName, L"DESTROYMONITOR", &response)) {
        BOOST_LOG(error) << "Failed to destroy virtual display";
        return false;
      }

      BOOST_LOG(info) << "Virtual display destroy complete, response: " << response;

      // Wait for the driver to fully unload, to avoid WUDFHost.exe crashes
      // This is necessary because driver unload is asynchronous
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

#if defined SUNSHINE_TRAY && SUNSHINE_TRAY >= 1
      system_tray::update_vdd_menu();
#endif
      return true;
    }

    void
    destroy_vdd_monitor_nolog() {
      HANDLE hPipe = CreateFileW(
        kVddPipeName,
        GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING, 0, NULL);
      if (hPipe != INVALID_HANDLE_VALUE) {
        DWORD mode = PIPE_READMODE_MESSAGE;
        SetNamedPipeHandleState(hPipe, &mode, NULL, NULL);
        const wchar_t cmd[] = L"DESTROYMONITOR";
        DWORD bytesWritten;
        WriteFile(hPipe, cmd, sizeof(cmd), &bytesWritten, NULL);
        CloseHandle(hPipe);
      }
    }

    void
    enable_vdd() {
      execute_vdd_command(vdd_action_e::enable);
    }

    void
    disable_vdd() {
      execute_vdd_command(vdd_action_e::disable);
    }

    void
    disable_enable_vdd() {
      execute_vdd_command(vdd_action_e::disable_enable);
    }

    bool
    is_display_on() {
      return !find_device_by_friendlyname(ZAKO_NAME).empty();
    }

    bool
    toggle_display_power() {
      auto now = std::chrono::steady_clock::now();

      if (now - last_toggle_time < debounce_interval) {
        BOOST_LOG(debug) << "Ignoring rapid repeated display toggle request; please wait "
                         << std::chrono::duration_cast<std::chrono::seconds>(
                              debounce_interval - (now - last_toggle_time))
                              .count()
                         << " second(s)";
        return false;
      }

      last_toggle_time = now;

      if (is_display_on()) {
        destroy_vdd_monitor();
        return true;
      }

      // Confirm before creating
      std::wstring confirm_title = system_tray_i18n::utf8_to_wstring(system_tray_i18n::get_localized_string(system_tray_i18n::KEY_VDD_CONFIRM_CREATE_TITLE));
      std::wstring confirm_message = system_tray_i18n::utf8_to_wstring(system_tray_i18n::get_localized_string(system_tray_i18n::KEY_VDD_CONFIRM_CREATE_MSG));

      if (MessageBoxW(NULL, confirm_message.c_str(), confirm_title.c_str(), MB_OKCANCEL | MB_ICONQUESTION) == IDCANCEL) {
        BOOST_LOG(info) << system_tray_i18n::get_localized_string(system_tray_i18n::KEY_VDD_CANCEL_CREATE_LOG);
        return false;
      }

      if (!create_vdd_monitor("", vdd_utils::hdr_brightness_t {}, vdd_utils::physical_size_t {})) {
        return false;
      }

      // Save the physical device list captured before creating the virtual display.
      // Also look for physical displays in all available devices (including ones that may be disabled).
      std::unordered_set<std::string> physical_devices_before;
      auto topology_before = get_current_topology();
      auto all_devices_before = enum_available_devices();

      // Pull active physical devices from the current topology
      for (const auto &group : topology_before) {
        for (const auto &device_id : group) {
          if (get_display_friendly_name(device_id) != ZAKO_NAME) {
            physical_devices_before.insert(device_id);
          }
        }
      }

      // If the topology contains no physical devices, search all devices (some may be disabled)
      if (physical_devices_before.empty()) {
        for (const auto &[device_id, device_info] : all_devices_before) {
          if (get_display_friendly_name(device_id) != ZAKO_NAME) {
            physical_devices_before.insert(device_id);
            BOOST_LOG(debug) << "Found physical display in all-devices list: " << device_id;
          }
        }
      }

      // Background thread: ensure the VDD is in extended mode and prompt for second-level confirmation
      std::thread([vdd_device_id = find_device_by_friendlyname(ZAKO_NAME), physical_devices_before]() mutable {
        if (vdd_device_id.empty()) {
          std::this_thread::sleep_for(std::chrono::seconds(2));
          vdd_device_id = find_device_by_friendlyname(ZAKO_NAME);
        }

        if (vdd_device_id.empty()) {
          BOOST_LOG(warning) << "Could not find the foundation display device; skipping configuration";
        }
        else {
          BOOST_LOG(info) << "Found foundation display device: " << vdd_device_id;

          if (ensure_vdd_extended_mode(vdd_device_id, physical_devices_before)) {
            BOOST_LOG(info) << "Foundation display is now confirmed in extended mode";
          }
        }

        // Post-creation confirmation, 20 second timeout
        constexpr auto timeout = std::chrono::seconds(20);
        std::wstring dialog_title = system_tray_i18n::utf8_to_wstring(system_tray_i18n::get_localized_string(system_tray_i18n::KEY_VDD_CONFIRM_KEEP_TITLE));
        std::wstring confirm_message = system_tray_i18n::utf8_to_wstring(system_tray_i18n::get_localized_string(system_tray_i18n::KEY_VDD_CONFIRM_KEEP_MSG));

        auto future = std::async(std::launch::async, [&]() {
          return MessageBoxW(nullptr, confirm_message.c_str(), dialog_title.c_str(), MB_YESNO | MB_ICONQUESTION) == IDYES;
        });

        if (future.wait_for(timeout) == std::future_status::ready && future.get()) {
          BOOST_LOG(info) << "User confirmed keeping the foundation display";
          return;
        }

        BOOST_LOG(info) << "User did not confirm or timed out; auto-destroying the foundation display";

        std::wstring w_dialog_title = system_tray_i18n::utf8_to_wstring(system_tray_i18n::get_localized_string(system_tray_i18n::KEY_VDD_CONFIRM_KEEP_TITLE));
        if (HWND hwnd = FindWindowW(L"#32770", w_dialog_title.c_str()); hwnd && IsWindow(hwnd)) {
          PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDNO, BN_CLICKED), 0);
          PostMessage(hwnd, WM_CLOSE, 0, 0);

          for (int i = 0; i < 5 && IsWindow(hwnd); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
          }

          if (IsWindow(hwnd)) {
            BOOST_LOG(warning) << "Couldn't close the confirmation window normally; attempting to terminate the dialog";
            EndDialog(hwnd, IDNO);
          }
        }

        destroy_vdd_monitor();
      }).detach();

      return true;
    }

    VddSettings
    prepare_vdd_settings(const parsed_config_t &config) {
      auto is_res_cached = false;
      auto is_fps_cached = false;
      std::ostringstream res_stream, fps_stream;

      res_stream << '[';
      fps_stream << '[';

      // Check whether the resolution is already cached
      for (const auto &res : config::nvhttp.resolutions) {
        res_stream << res << ',';
        if (config.resolution && res == to_string(*config.resolution)) {
          is_res_cached = true;
        }
      }

      // Check whether the refresh rate is already cached
      for (const auto &fps : config::nvhttp.fps) {
        fps_stream << fps << ',';
        if (config.refresh_rate && fps == to_string(*config.refresh_rate)) {
          is_fps_cached = true;
        }
      }

      // If settings need to be updated
      bool needs_update = (!is_res_cached || !is_fps_cached) && config.resolution;
      if (needs_update) {
        if (!is_res_cached) {
          res_stream << to_string(*config.resolution);
        }
        if (!is_fps_cached && config.refresh_rate) {
          fps_stream << to_string(*config.refresh_rate);
        }
      }

      // Strip the trailing comma and append the closing bracket
      auto res_str = res_stream.str();
      auto fps_str = fps_stream.str();
      if (res_str.back() == ',') res_str.pop_back();
      if (fps_str.back() == ',') fps_str.pop_back();
      res_str += ']';
      fps_str += ']';

      return { res_str, fps_str, needs_update };
    }

    bool
    ensure_vdd_extended_mode(const std::string &device_id, const std::unordered_set<std::string> &physical_devices_to_preserve) {
      if (device_id.empty()) {
        return false;
      }

      auto current_topology = get_current_topology();
      if (current_topology.empty()) {
        BOOST_LOG(warning) << "Unable to obtain the current display topology";
        return false;
      }

      // Find the topology group containing the VDD
      std::size_t vdd_group_index = SIZE_MAX;
      for (std::size_t i = 0; i < current_topology.size(); ++i) {
        if (std::find(current_topology[i].begin(), current_topology[i].end(), device_id) != current_topology[i].end()) {
          vdd_group_index = i;
          break;
        }
      }

      // Check whether a switch is needed
      bool is_duplicated = (vdd_group_index != SIZE_MAX && current_topology[vdd_group_index].size() > 1);
      bool is_vdd_only = (current_topology.size() == 1 && current_topology[0].size() == 1 && current_topology[0][0] == device_id);

      if (!is_duplicated && !is_vdd_only) {
        BOOST_LOG(debug) << "VDD is already in extended mode";
        return false;
      }

      BOOST_LOG(info) << "VDD is in " << (is_vdd_only ? "VDD-only" : "duplicated") << " mode; switching to extended mode";

      // Build a new topology: split out VDD, keep other devices intact
      active_topology_t new_topology;
      std::unordered_set<std::string> included;

      for (std::size_t i = 0; i < current_topology.size(); ++i) {
        const auto &group = current_topology[i];

        if (i == vdd_group_index) {
          // Split the VDD into its own group
          for (const auto &id : group) {
            new_topology.push_back({ id });
            included.insert(id);
          }
        }
        else {
          for (const auto &id : group) {
            included.insert(id);
          }
          new_topology.push_back(group);
        }
      }

      // Add missing physical displays
      auto all_devices = enum_available_devices();
      for (const auto &physical_id : physical_devices_to_preserve) {
        if (included.count(physical_id) == 0 && all_devices.find(physical_id) != all_devices.end()) {
          new_topology.push_back({ physical_id });
          BOOST_LOG(info) << "Adding physical display to topology: " << physical_id;
        }
      }

      if (!is_topology_valid(new_topology) || !set_topology(new_topology)) {
        BOOST_LOG(error) << "Failed to set topology";
        return false;
      }

      BOOST_LOG(info) << "Successfully switched to extended mode";
      return true;
    }

    bool
    set_hdr_state(bool enable_hdr) {
      auto vdd_device_id = find_device_by_friendlyname(ZAKO_NAME);
      if (vdd_device_id.empty()) {
        BOOST_LOG(info) << "Virtual display device not found; skipping HDR state set";
        return true;
      }

      std::unordered_set<std::string> vdd_device_ids = { vdd_device_id };
      auto current_hdr_states = get_current_hdr_states(vdd_device_ids);

      auto hdr_state_it = current_hdr_states.find(vdd_device_id);
      if (hdr_state_it == current_hdr_states.end()) {
        BOOST_LOG(info) << "Virtual display does not support HDR or state is unknown";
        return true;
      }

      hdr_state_e target_state = enable_hdr ? hdr_state_e::enabled : hdr_state_e::disabled;
      if (hdr_state_it->second == target_state) {
        BOOST_LOG(info) << "Virtual display HDR state already matches target";
        return true;
      }

      hdr_state_map_t new_hdr_states;
      new_hdr_states[vdd_device_id] = target_state;

      const std::string action = enable_hdr ? "enabling" : "disabling";
      BOOST_LOG(info) << "Currently " << action << " HDR on the virtual display...";

      if (set_hdr_states(new_hdr_states)) {
        BOOST_LOG(info) << "Successfully completed " << action << " HDR on the virtual display";
        return true;
      }

      BOOST_LOG(warning) << "Failed " << action << " HDR on the virtual display";
      return false;
    }

    bool
    apply_vdd_prep(const std::string &vdd_device_id, parsed_config_t::vdd_prep_e vdd_prep,
      const device_info_map_t &pre_vdd_devices) {
      if (vdd_device_id.empty()) {
        BOOST_LOG(info) << "VDD device ID is empty; skipping vdd_prep handling";
        return true;
      }

      if (vdd_prep == parsed_config_t::vdd_prep_e::no_operation) {
        BOOST_LOG(info) << "vdd_prep is set to no_operation; skipping physical display handling";
        return true;
      }

      // Read physical displays from pre_vdd_devices (the device list captured before VDD creation),
      // so we still identify them correctly even if VDD creation made the physical screens inactive.
      std::vector<std::string> physical_devices;
      std::string original_primary_id;

      if (!pre_vdd_devices.empty()) {
        // Use the device info captured before VDD creation (reliable)
        for (const auto &[device_id, info] : pre_vdd_devices) {
          if (info.friendly_name != ZAKO_NAME) {
            physical_devices.push_back(device_id);
            if (info.device_state == device_state_e::primary) {
              original_primary_id = device_id;
            }
          }
        }
        BOOST_LOG(info) << "Using pre-VDD device list: " << physical_devices.size() << " physical display(s)"
                        << (original_primary_id.empty() ? "" : ", original primary: " + original_primary_id);
      }
      else {
        // Fallback: read from the current device enumeration (when no pre-VDD list was captured)
        BOOST_LOG(warning) << "No pre-VDD device list provided; finding physical displays from the current enumeration";
        const auto all_devices = enum_available_devices();
        for (const auto &[device_id, info] : all_devices) {
          if (device_id != vdd_device_id && info.friendly_name != ZAKO_NAME) {
            physical_devices.push_back(device_id);
            if (info.device_state == device_state_e::primary) {
              original_primary_id = device_id;
            }
          }
        }
      }

      // Make sure the original primary is at the front (in set_topology, the first group has primary precedence)
      if (!original_primary_id.empty()) {
        auto it = std::find(physical_devices.begin(), physical_devices.end(), original_primary_id);
        if (it != physical_devices.begin() && it != physical_devices.end()) {
          std::rotate(physical_devices.begin(), it, it + 1);
        }
      }

      if (physical_devices.empty()) {
        BOOST_LOG(debug) << "No physical displays to process";
        return true;
      }

      active_topology_t new_topology;

      switch (vdd_prep) {
        case parsed_config_t::vdd_prep_e::vdd_as_primary: {
          // VDD-as-primary mode: VDD goes first (primary), physical displays become extended displays
          BOOST_LOG(info) << "Applying vdd_prep: VDD as primary, physical displays as secondary";
          // VDD in its own group (placed first as the primary display)
          new_topology.push_back({ vdd_device_id });
          // Each physical display in its own group (extended mode)
          for (const auto &physical_id : physical_devices) {
            new_topology.push_back({ physical_id });
          }
          break;
        }

        case parsed_config_t::vdd_prep_e::vdd_as_secondary: {
          // VDD-as-secondary mode: physical displays are primary, VDD becomes the extended display
          BOOST_LOG(info) << "Applying vdd_prep: physical displays as primary, VDD as secondary";
          // Physical displays go first (the first becomes the primary display)
          for (const auto &physical_id : physical_devices) {
            new_topology.push_back({ physical_id });
          }
          // VDD in its own group (as secondary display)
          new_topology.push_back({ vdd_device_id });
          break;
        }

        case parsed_config_t::vdd_prep_e::display_off: {
          // Display-off mode: keep only the VDD; turn off all physical displays
          BOOST_LOG(info) << "Applying vdd_prep: turn off physical displays";
          new_topology.push_back({ vdd_device_id });
          // Don't add physical displays; they will be disabled
          break;
        }

        default:
          return true;
      }

      if (!is_topology_valid(new_topology)) {
        BOOST_LOG(error) << "New topology is invalid";
        return false;
      }

      if (!set_topology(new_topology)) {
        BOOST_LOG(error) << "Failed to set topology";
        return false;
      }

      BOOST_LOG(info) << "Successfully applied vdd_prep settings";
      return true;
    }
  }  // namespace vdd_utils
}  // namespace display_device