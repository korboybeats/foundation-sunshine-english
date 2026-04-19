// standard includes
#include <boost/optional/optional_io.hpp>
#include <boost/process/v1.hpp>
#include <future>
#include <thread>

// local includes
#include "session.h"
#include "src/confighttp.h"
#include "src/globals.h"
#include "src/platform/common.h"
#include "src/platform/windows/display_device/session_listener.h"
#include "src/platform/windows/display_device/windows_utils.h"
#include "src/rtsp.h"
#include "to_string.h"
#include "vdd_utils.h"

namespace display_device {

  class session_t::StateRetryTimer {
  public:
    /**
     * @brief A constructor for the timer.
     * @param mutex A shared mutex for synchronization.
     * @warning Because we are keeping references to shared parameters, we MUST ensure they outlive this object!
     */
    StateRetryTimer(std::mutex &mutex, std::chrono::seconds timeout = std::chrono::seconds { 5 }):
        mutex { mutex }, timeout_duration { timeout }, timer_thread {
          std::thread { [this]() {
            std::unique_lock<std::mutex> lock { this->mutex };
            while (keep_alive) {
              can_wake_up = false;
              if (next_wake_up_time) {
                // We're going to sleep forever until manually woken up or the time elapses
                sleep_cv.wait_until(lock, *next_wake_up_time, [this]() { return can_wake_up; });
              }
              else {
                // We're going to sleep forever until manually woken up
                sleep_cv.wait(lock, [this]() { return can_wake_up; });
              }

              if (next_wake_up_time) {
                // Timer has just been started, or we have waited for the required amount of time.
                // We can check which case it is by comparing time points.

                const auto now { std::chrono::steady_clock::now() };
                if (now < *next_wake_up_time) {
                  // Thread has been woken up manually to synchronize the time points.
                  // We do nothing and just go back to waiting with a new time point.
                }
                else {
                  next_wake_up_time = boost::none;

                  const auto result { !this->retry_function || this->retry_function() };
                  if (!result) {
                    next_wake_up_time = now + this->timeout_duration;
                  }
                }
              }
              else {
                // Timer has been stopped.
                // We do nothing and just go back to waiting until notified (unless we are killing the thread).
              }
            }
          } }
        } {
    }

    /**
     * @brief A destructor for the timer that gracefully shuts down the thread.
     */
    ~StateRetryTimer() {
      {
        std::lock_guard lock { mutex };
        keep_alive = false;
        next_wake_up_time = boost::none;
        wake_up_thread();
      }

      timer_thread.join();
    }

    /**
     * @brief Start or stop the timer thread.
     * @param retry_function Function to be executed every X seconds.
     *                       If the function returns true, the loop is stopped.
     *                       If the function is of type nullptr_t, the loop is stopped.
     * @warning This method does NOT acquire the mutex! It is intended to be used from places
     *          where the mutex has already been locked.
     */
    void
    setup_timer(std::function<bool()> retry_function) {
      this->retry_function = std::move(retry_function);

      if (this->retry_function) {
        next_wake_up_time = std::chrono::steady_clock::now() + timeout_duration;
      }
      else {
        if (!next_wake_up_time) {
          return;
        }

        next_wake_up_time = boost::none;
      }

      wake_up_thread();
    }

  private:
    /**
     * @brief Manually wake up the thread.
     */
    void
    wake_up_thread() {
      can_wake_up = true;
      sleep_cv.notify_one();
    }

    std::mutex &mutex; /**< A reference to a shared mutex. */
    std::chrono::seconds timeout_duration { 5 }; /**< A retry time for the timer. */
    std::function<bool()> retry_function; /**< Function to be executed until it succeeds. */

    std::thread timer_thread; /**< A timer thread. */
    std::condition_variable sleep_cv; /**< Condition variable for waking up thread. */

    bool can_wake_up { false }; /**< Safeguard for the condition variable to prevent sporadic thread wake ups. */
    bool keep_alive { true }; /**< A kill switch for the thread when it has been woken up. */
    boost::optional<std::chrono::steady_clock::time_point> next_wake_up_time; /**< Next time point for thread to wake up. */
  };

  session_t::deinit_t::~deinit_t() {
    // Clean up the event listener
    SessionEventListener::deinit();

    // Safety net: on shutdown, if a VDD still exists and vdd_keep_enabled=false, destroy it directly.
    // Use the nolog version since boost::log may have already been destroyed during destruction.
    if (!config::video.vdd_keep_enabled) {
      vdd_utils::destroy_vdd_monitor_nolog();
    }
  }

  session_t &
  session_t::get() {
    static session_t session;
    return session;
  }

  std::unique_ptr<session_t::deinit_t>
  session_t::init() {
    session_t::get().settings.set_filepath(platf::appdata() / "original_display_settings.json");
    
    // Initialize the session event listener (for detecting unlock events)
    SessionEventListener::init();
    
    session_t::get().restore_state();
    return std::make_unique<deinit_t>();
  }

  void
  session_t::clear_vdd_state() {
    last_vdd_setting.clear();
    current_device_prep.reset();
    current_vdd_prep.reset();
    current_use_vdd.reset();
    // Restore the original output_name so the next session doesn't use a destroyed VDD device ID
    if (!original_output_name.empty()) {
      config::video.output_name = original_output_name;
      original_output_name.clear();
      BOOST_LOG(debug) << "Restored original output_name: " << config::video.output_name;
    }
  }

  void
  session_t::stop_timer_and_clear_vdd_state() {
    timer->setup_timer(nullptr);
    clear_vdd_state();
  }

  namespace {
    /**
     * @brief Get client identifier from session.
     * @details Prioritizes client certificate UUID (stored in env) over client_name as it is more stable.
     * @param session The launch session containing client information.
     * @return Client identifier string, or empty string if not available.
     */
    std::string
    get_client_id_from_session(const rtsp_stream::launch_session_t &session) {
      if (auto cert_uuid_it = session.env.find("SUNSHINE_CLIENT_CERT_UUID");
        cert_uuid_it != session.env.end()) {
        if (std::string cert_uuid = cert_uuid_it->to_string(); !cert_uuid.empty()) {
          return cert_uuid;
        }
      }

      if (!session.client_name.empty() && session.client_name != "unknown") {
        return session.client_name;
      }

      return {};
    }

    /**
     * @brief Wait for VDD device to be available (active or inactive).
     * @param device_zako Output parameter for the device ID.
     * @param max_attempts Maximum number of retry attempts.
     * @param initial_delay Initial delay between retries.
     * @param max_delay Maximum delay between retries.
     * @return true if device was found (active or inactive), false otherwise.
     */
    bool
    wait_for_vdd_device(std::string &device_zako, int max_attempts,
      std::chrono::milliseconds initial_delay,
      std::chrono::milliseconds max_delay) {
      return vdd_utils::retry_with_backoff(
        [&device_zako]() {
          device_zako = display_device::find_device_by_friendlyname(ZAKO_NAME);
          if (device_zako.empty()) {
            BOOST_LOG(debug) << "VDD device not found by friendly name";
            return false;
          }

          // Device found by friendly name - that's all we need
          // It can be activated later during display configuration
          BOOST_LOG(debug) << "VDD device found: " << device_zako;
          return true;
        },
        { .max_attempts = max_attempts,
          .initial_delay = initial_delay,
          .max_delay = max_delay,
          .context = "Waiting for VDD device availability" });
    }

    /**
     * @brief Attempt to recover VDD device with retries.
     * @param client_id Client identifier for the VDD monitor.
     * @param client_name Client name for getting physical size from config.
     * @param hdr_brightness hdr_brightness_t.
     * @param device_zako Output parameter for the device ID.
     * @return true if recovery succeeded, false otherwise.
     */
    bool
    try_recover_vdd_device(const std::string &client_id, const std::string &client_name, const vdd_utils::hdr_brightness_t &hdr_brightness, std::string &device_zako) {
      constexpr int max_retries = 3;
      const vdd_utils::physical_size_t physical_size = vdd_utils::get_client_physical_size(client_name);

      // Reuse mode uses a fixed identifier, otherwise use the client ID
      const std::string vdd_identifier = config::video.vdd_reuse
        ? "shared_vdd"
        : client_id;

      for (int retry = 1; retry <= max_retries; ++retry) {
        BOOST_LOG(info) << "Performing VDD recovery attempt " << retry;

        if (!vdd_utils::create_vdd_monitor(vdd_identifier, hdr_brightness, physical_size)) {
          BOOST_LOG(error) << "Failed to create virtual display, attempt " << retry << "/" << max_retries;
          if (retry < max_retries) {
            std::this_thread::sleep_for(std::chrono::seconds(1 << retry));
          }
          continue;
        }

        if (wait_for_vdd_device(device_zako, 5, 233ms, 2000ms)) {
          BOOST_LOG(info) << "VDD device recovered successfully!";
          return true;
        }

        BOOST_LOG(error) << "VDD device check failed; retrying " << retry << "/" << max_retries;
        if (retry < max_retries) {
          std::this_thread::sleep_for(std::chrono::seconds(1 << retry));
        }
      }

      return false;
    }
  }  // namespace

  void
  session_t::configure_display(const config::video_t &config,
    const rtsp_stream::launch_session_t &session,
    bool is_reconfigure) {
    std::lock_guard lock { mutex };

    // Clean up VDD state if this is a new session with a different client
    if (!is_reconfigure) {
      if (const std::string new_client_id = get_client_id_from_session(session);
        !current_vdd_client_id.empty() && !new_client_id.empty() &&
        current_vdd_client_id != new_client_id) {
        BOOST_LOG(info) << "New session detected with different client ID, cleaning up VDD state";
        // Cancel any pending restore from the old session before it can interfere
        pending_restore_ = false;
        SessionEventListener::clear_unlock_task();
        stop_timer_and_clear_vdd_state();
      }
    }

    // Save the real initial topology before make_parsed_config.
    // make_parsed_config internally calls prepare_vdd, which creates the VDD and switches to extended mode,
    // causing the original displays to become inactive.
    boost::optional<active_topology_t> pre_saved_initial_topology;

    // Check whether VDD will be used
    std::string device_id_to_use = config.output_name;
    if (auto it = session.env.find("SUNSHINE_CLIENT_DISPLAY_NAME"); it != session.env.end()) {
      const std::string client_display_name = it->to_string();
      if (!client_display_name.empty()) {
        device_id_to_use = client_display_name;
      }
    }

    // Check whether a VDD already exists
    const auto existing_vdd_id = display_device::find_device_by_friendlyname(ZAKO_NAME);
    const bool vdd_already_exists = !existing_vdd_id.empty();

    // If VDD will be used and no VDD currently exists, save the topology before creation.
    // If a VDD already exists the topology is already disturbed, so don't save the current one.
    const auto requested_device_id = display_device::find_one_of_the_available_devices(device_id_to_use);
    const bool is_vdd_device = (display_device::get_display_friendly_name(device_id_to_use) == ZAKO_NAME);

    const bool needs_vdd = session.use_vdd || requested_device_id.empty() || is_vdd_device;

    // - If VDD is not needed: skip VDD-related logic
    // - If not running as SYSTEM and inside an RDP session: use the RDP virtual display, don't create a VDD
    // - All other cases (including SYSTEM): prepare the VDD device
    const bool is_rdp_blocking_vdd = !is_running_as_system_user && display_device::w_utils::is_any_rdp_session_active();
    const bool will_use_vdd = needs_vdd && !is_rdp_blocking_vdd;

    if (will_use_vdd && !vdd_already_exists) {

      // If there is a pending restore, keep the old initial topology; do not overwrite it
      if (pending_restore_ && settings.has_persistent_data()) {
        BOOST_LOG(info) << "Pending restore present; keeping the existing initial topology";
        // Clear the pending-restore flag because a new stream is about to start
        pending_restore_ = false;
        SessionEventListener::clear_unlock_task();
        timer->setup_timer(nullptr);
        // Do not set pre_saved_initial_topology; let apply_config reuse what is already there
      }
      else {
        pre_saved_initial_topology = get_current_topology();
        BOOST_LOG(debug) << "Pre-saved initial topology before VDD creation: " << to_string(*pre_saved_initial_topology);
      }
    }
    else if (will_use_vdd && vdd_already_exists) {
      if (pending_restore_ && settings.has_persistent_data()) {
        // Pending restore exists and VDD is still present (CCD previously failed); keep the existing initial topology
        BOOST_LOG(info) << "Pending restore present and VDD still exists; keeping the existing initial topology";
        pending_restore_ = false;
        SessionEventListener::clear_unlock_task();
        timer->setup_timer(nullptr);
      }
      else {
        BOOST_LOG(debug) << "VDD already exists, skipping initial topology save (topology may be corrupted)";
      }
    }

    const auto parsed_config = make_parsed_config(config, session, is_reconfigure);
    if (!parsed_config) {
      BOOST_LOG(error) << "Failed to parse configuration for the display device settings!";
      return;
    }

    // Save the configuration modes for the current session (may include client overrides)
    current_device_prep = parsed_config->device_prep;
    current_vdd_prep = parsed_config->vdd_prep;
    current_use_vdd = parsed_config->use_vdd;

    if (settings.is_changing_settings_going_to_fail()) {
      timer->setup_timer([this, config_copy = *parsed_config, &session, pre_saved_initial_topology]() {
        if (settings.is_changing_settings_going_to_fail()) {
          BOOST_LOG(warning) << "Applying display settings will fail - retrying later...";
          return false;
        }

        if (!settings.apply_config(config_copy, session, pre_saved_initial_topology)) {
          BOOST_LOG(warning) << "Failed to apply display settings - will stop trying, but will allow stream to continue.";
          // WARNING! After call to the method below, this lambda function is no longer valid!
          // DO NOT access anything from the capture list!
          restore_state_impl(revert_reason_e::config_cleanup);
        }
        return true;
      });

      BOOST_LOG(warning) << "It is already known that display settings cannot be changed. Allowing stream to start without changing the settings, but will retry changing settings later...";
      return;
    }

    if (settings.apply_config(*parsed_config, session, pre_saved_initial_topology)) {
      timer->setup_timer(nullptr);
    }
    else {
      restore_state_impl(revert_reason_e::config_cleanup);
    }
  }

  bool
  session_t::create_vdd_monitor(const std::string &client_name) {
    const vdd_utils::physical_size_t physical_size = vdd_utils::get_client_physical_size(client_name);
    // Reuse mode uses a fixed identifier, otherwise use the client name
    const std::string vdd_identifier = config::video.vdd_reuse
      ? "shared_vdd"
      : client_name;
    return vdd_utils::create_vdd_monitor(vdd_identifier, vdd_utils::hdr_brightness_t { 1000.0f, 0.001f, 1000.0f }, physical_size);
  }

  bool
  session_t::destroy_vdd_monitor() {
    current_vdd_client_id.clear();
    return vdd_utils::destroy_vdd_monitor();
  }

  bool
  session_t::is_display_on() {
    return vdd_utils::is_display_on();
  }

  bool
  session_t::toggle_display_power() {
    return vdd_utils::toggle_display_power();
  }

  void
  session_t::update_vdd_resolution(const parsed_config_t &config,
    const vdd_utils::VddSettings &vdd_settings) {
    const auto new_setting = to_string(*config.resolution) + "@" + to_string(*config.refresh_rate);

    if (last_vdd_setting == new_setting) {
      BOOST_LOG(debug) << "VDD config unchanged: " << new_setting;
      return;
    }

    if (!confighttp::saveVddSettings(vdd_settings.resolutions, vdd_settings.fps, config::video.adapter_name)) {
      BOOST_LOG(error) << "VDD config save failed [resolutions: " << vdd_settings.resolutions
                       << " fps: " << vdd_settings.fps << "]";
      return;
    }

    last_vdd_setting = new_setting;
    BOOST_LOG(info) << "VDD config updated: " << new_setting;

    BOOST_LOG(info) << "Reloading the VDD driver...";
    vdd_utils::reload_driver();
    std::this_thread::sleep_for(1200ms);
  }

  void
  session_t::prepare_vdd(parsed_config_t &config, const rtsp_stream::launch_session_t &session) {
    const std::string current_client_id = get_client_id_from_session(session);
    const vdd_utils::hdr_brightness_t hdr_brightness { session.max_nits, session.min_nits, session.max_full_nits };
    const vdd_utils::physical_size_t physical_size = vdd_utils::get_client_physical_size(session.client_name);

    auto device_zako = display_device::find_device_by_friendlyname(ZAKO_NAME);

    // pre_vdd_devices: snapshot of physical displays captured right before VDD creation.
    // Defer the capture until just before VDD creation so we get the correct state for both creation and re-creation.
    device_info_map_t pre_vdd_devices;

    // Rebuild VDD device on client switch
    if (!device_zako.empty() && !current_vdd_client_id.empty() &&
        !current_client_id.empty() && current_vdd_client_id != current_client_id) {
      
      // Whether to reuse the VDD (controlled by a dedicated config option)
      const bool reuse_vdd = config::video.vdd_reuse;

      if (reuse_vdd) {
        // Shared VDD: all clients share the same VDD; just update the client ID
        BOOST_LOG(info) << "Shared VDD mode, reusing existing VDD (client: " << current_vdd_client_id << " -> " << current_client_id << ")";
        current_vdd_client_id = current_client_id;
      }
      else {
        // Per-client VDD: destroy and rebuild the VDD
        BOOST_LOG(info) << "Per-client VDD mode, rebuilding VDD device (client: " << current_vdd_client_id << " -> " << current_client_id << ")";

        const auto old_vdd_id = device_zako;
        destroy_vdd_monitor();
        clear_vdd_state();
        device_zako.clear();

        // Handle VDD ID in persistent_data
        if (config::video.vdd_keep_enabled) {
          // Keep-enabled mode: need to replace the ID (VDD stays in persistent_data)
          should_replace_vdd_id_ = true;
          old_vdd_id_ = old_vdd_id;
          BOOST_LOG(debug) << "Marked VDD ID for replacement: " << old_vdd_id;
        }
        else {
          // Non keep-enabled mode: remove the VDD from initial
          BOOST_LOG(debug) << "Removing VDD from initial topology: " << old_vdd_id;
          settings.remove_vdd_from_initial_topology(old_vdd_id);
        }
        
        std::this_thread::sleep_for(500ms);
      }
    }

    // Update VDD resolution configuration
    if (auto vdd_settings = vdd_utils::prepare_vdd_settings(config);
      vdd_settings.needs_update && config.resolution) {
      update_vdd_resolution(config, vdd_settings);
    }

    // Create VDD device if not present
    if (device_zako.empty()) {
      // Capture a snapshot of physical displays before creating the VDD.
      // No VDD exists right now (either new or just rebuilt), so the physical screens should be in their normal state.
      pre_vdd_devices = display_device::enum_available_devices();
      BOOST_LOG(info) << "Saved pre-VDD device list: " << display_device::to_string(pre_vdd_devices);

      BOOST_LOG(info) << "Creating virtual display...";
      // Reuse mode uses a fixed identifier, otherwise generate a unique GUID from the client ID
      const std::string vdd_identifier = config::video.vdd_reuse
        ? "shared_vdd"  // Fixed identifier; all clients share the same GUID
        : current_client_id;  // Unique GUID per client
      vdd_utils::create_vdd_monitor(vdd_identifier, hdr_brightness, physical_size);
      std::this_thread::sleep_for(200ms);
    }

    // Wait for device to be ready
    if (!wait_for_vdd_device(device_zako, 5, 200ms, 1000ms)) {
      BOOST_LOG(error) << "VDD device initialization failed; attempting recovery";
      vdd_utils::disable_enable_vdd();
      std::this_thread::sleep_for(2s);

      if (!try_recover_vdd_device(current_client_id, session.client_name, hdr_brightness, device_zako)) {
        BOOST_LOG(error) << "VDD device initialization ultimately failed";
        vdd_utils::disable_enable_vdd();
        return;
      }
    }

    if (device_zako.empty()) {
      return;
    }

    if (original_output_name.empty()) {
      original_output_name = config::video.output_name;
      BOOST_LOG(debug) << "Saved original output_name: " << original_output_name;
    }

    // Replace VDD ID if needed (after client switch in keep_enabled mode)
    if (should_replace_vdd_id_ && !old_vdd_id_.empty()) {
      BOOST_LOG(info) << "Replacing VDD ID in persistent_data: " << old_vdd_id_ << " -> " << device_zako;
      settings.replace_vdd_id(old_vdd_id_, device_zako);
      should_replace_vdd_id_ = false;
      old_vdd_id_.clear();
    }
    
    // Update configuration and state
    config.device_id = device_zako;
    config::video.output_name = device_zako;
    current_vdd_client_id = current_client_id;
    BOOST_LOG(info) << "Successfully configured VDD device: " << device_zako;

    // Apply VDD prep settings to handle display topology
    // This determines how VDD interacts with physical displays
    // Topology control in VDD mode is handled separately from normal mode
    if (config.vdd_prep != parsed_config_t::vdd_prep_e::no_operation) {
      // User has specified a display configuration, apply it
      if (vdd_utils::apply_vdd_prep(device_zako, config.vdd_prep, pre_vdd_devices)) {
        BOOST_LOG(info) << "Applied VDD screen layout settings";
        std::this_thread::sleep_for(200ms);
      }
    }
    else {
      // No specific configuration, ensure VDD is in extended mode (default behavior)
      if (vdd_utils::ensure_vdd_extended_mode(device_zako)) {
        BOOST_LOG(info) << "Switched VDD to extended mode";
        std::this_thread::sleep_for(500ms);
      }
    }

    // Set HDR state with retry
    if (!vdd_utils::set_hdr_state(false)) {
      BOOST_LOG(debug) << "Initial HDR state set failed; retrying after device stabilizes";
      std::this_thread::sleep_for(500ms);
      vdd_utils::set_hdr_state(false);
    }
  }

  void
  session_t::restore_state() {
    std::lock_guard lock { mutex };
    restore_state_impl();
  }

  void
  session_t::reset_persistence() {
    std::lock_guard lock { mutex };
    settings.reset_persistence();
    pending_restore_ = false;
    SessionEventListener::clear_unlock_task();
    stop_timer_and_clear_vdd_state();
    current_vdd_client_id.clear();
  }

  void
  session_t::restore_state_impl(revert_reason_e reason) {
    // Unified VDD cleanup logic (runs before topology restore; needs no CCD API and works while locked)
    const auto vdd_id = display_device::find_device_by_friendlyname(ZAKO_NAME);

    // Keep-enabled mode: only affects whether the VDD is destroyed, not topology restore
    const bool is_keep_enabled = config::video.vdd_keep_enabled;

    // If there is no session configuration (current_use_vdd is nullopt) it means:
    //   1. The program just started and is performing crash recovery (init() call), OR
    //   2. The previous session ended normally and already cleaned up state.
    // In that case there's no topology to restore (nothing was modified); only clean up any leftover VDD.
    if (!current_use_vdd.has_value()) {
      BOOST_LOG(debug) << "No session configuration (current_use_vdd=nullopt); only running VDD cleanup";

      if (!vdd_id.empty() && !is_keep_enabled) {
        if (settings.has_persistent_data()) {
          BOOST_LOG(info) << "Not in keep-enabled mode; destroying leftover VDD";
        }
        else {
          BOOST_LOG(info) << "Detected stray VDD (no persistent_data); cleaning up VDD";
        }
        destroy_vdd_monitor();
        std::this_thread::sleep_for(1000ms);
      }

      // Headless host auto-create check
      if (reason == revert_reason_e::stream_ended && config::video.vdd_headless_create_enabled) {
        auto devices = display_device::enum_available_devices();
        if (devices.empty()) {
          BOOST_LOG(info) << "Headless host detected: no display devices found; auto-creating foundation display";
          create_vdd_monitor("");
          constexpr int max_attempts = 5;
          constexpr auto wait_time = std::chrono::milliseconds(233);
          for (int i = 0; i < max_attempts && !is_display_on(); ++i) {
            std::this_thread::sleep_for(wait_time);
          }
        }
      }

      stop_timer_and_clear_vdd_state();
      return;
    }

    // The logic below runs only when there is a session configuration (current_use_vdd has a value)
    const bool is_vdd_mode = *current_use_vdd;

    // Determine the effective configuration modes
    //   VDD mode: map the unified value to vdd_prep
    //   Normal mode: map the unified value to device_prep
    const auto display_prep = current_device_prep.value_or(
      static_cast<parsed_config_t::device_prep_e>(config::video.display_device_prep)
    );
    const auto vdd_prep = current_vdd_prep.value_or(
      parsed_config_t::to_vdd_prep(display_prep)
    );
    const auto device_prep = is_vdd_mode
      ? display_prep
      : parsed_config_t::to_physical_device_prep(display_prep);

    // Determine whether this is a no-operation mode (session configured no_operation, so topology was never modified).
    // VDD mode looks at vdd_prep; normal mode looks at device_prep.
    const bool is_no_operation = is_vdd_mode
      ? (vdd_prep == parsed_config_t::vdd_prep_e::no_operation)
      : (device_prep == parsed_config_t::device_prep_e::no_operation);

    BOOST_LOG(debug) << "restore_state_impl decision parameters:"
                     << " is_vdd_mode=" << is_vdd_mode
                     << " vdd_prep=" << static_cast<int>(vdd_prep)
                     << " device_prep=" << static_cast<int>(device_prep)
                     << " is_no_operation=" << is_no_operation;

    // Check whether apply_config previously succeeded (persistent_data exists)
    const bool has_persistent = settings.has_persistent_data();

    // Execute the full restore immediately
    // VDD destruction logic
    if (!vdd_id.empty()) {
      bool should_destroy = false;

      // Decision 1: keep-enabled mode - keep the VDD
      if (is_keep_enabled) {
        BOOST_LOG(debug) << "Keep-enabled mode; preserving VDD";
      }
      // Decision 2: not keep-enabled - destroy VDD (regardless of no-operation mode)
      else if (has_persistent) {
        BOOST_LOG(info) << "Not in keep-enabled mode; destroying VDD";
        should_destroy = true;
      }
      // Decision 3: no persistent_data - apply_config never succeeded (e.g. stream ended while locked)
      else {
        BOOST_LOG(info) << "apply_config was never executed (no persistent_data); destroying VDD and skipping topology restore";
        should_destroy = true;
      }

      // Headless host protection: if destruction would leave the host headless (VDD is the only display), skip it.
      // This avoids a meaningless destroy/recreate loop (device ID changes invalidate persistent_data).
      if (should_destroy) {
        auto devices = display_device::enum_available_devices();
        bool only_vdd = (devices.size() == 1 && devices.count(vdd_id));
        if (only_vdd || devices.empty()) {
          BOOST_LOG(info) << "Headless host detected: VDD is the only display; skipping destroy";
          should_destroy = false;
        }
      }

      if (should_destroy) {
        destroy_vdd_monitor();
        std::this_thread::sleep_for(1000ms);
      }
    }

    // If apply_config never succeeded, the topology was never modified; nothing to restore
    if (!has_persistent) {
      BOOST_LOG(info) << "apply_config was never executed; skipping topology restore";
      stop_timer_and_clear_vdd_state();
      return;
    }

    // Add diagnostic log
    const bool settings_will_fail = settings.is_changing_settings_going_to_fail();
    BOOST_LOG(debug) << "Checking if reverting settings will fail: " << settings_will_fail;

    // VDD lifecycle was already decided above (destroy or keep); tell revert_settings not to also handle VDD destruction
    const bool vdd_already_handled = true;

    if (!settings_will_fail && settings.revert_settings(reason, vdd_already_handled)) {
      stop_timer_and_clear_vdd_state();
    }
    else {
      // Cannot restore immediately; add the task to the unlock queue
      BOOST_LOG(warning) << "Cannot restore display settings immediately";

      // Set the pending-restore flag
      pending_restore_ = true;

      // Add the restore task (handles lock-screen check and immediate execution automatically)
      SessionEventListener::add_unlock_task([this, reason]() {
        // Quick check whether the restore is still needed (minimize lock-hold time)
        {
          std::lock_guard lock { mutex };
          if (!pending_restore_) {
            BOOST_LOG(info) << "Restore operation cancelled; skipping";
            return;
          }
        }

        // Run the CCD check and restore outside the lock (avoid blocking the tray and other operations)
        if (settings.is_changing_settings_going_to_fail()) {
          BOOST_LOG(warning) << "CCD API still unavailable; starting polling fallback";
          std::lock_guard lock { mutex };
          this->start_polling_restore(reason);
          return;
        }

        // Perform the restore
        auto result = settings.revert_settings(reason, true);
        BOOST_LOG(info) << "Display settings restore " << (result ? "succeeded" : "failed");

        // Clear the flag and state once the restore is done
        {
          std::lock_guard lock { mutex };
          pending_restore_ = false;
          stop_timer_and_clear_vdd_state();
        }
      });
    }
  }

  void
  session_t::start_polling_restore(revert_reason_e reason) {
    polling_retry_count_.store(0, boost::memory_order_relaxed);  // Reset the counter
    const int max_retries = 20;

    timer->setup_timer([this, reason, max_retries]() {
      // Check whether the restore is still needed
      if (!pending_restore_) {
        BOOST_LOG(debug) << "Restore operation cancelled; skipping";
        return true;
      }

      if (settings.is_changing_settings_going_to_fail()) {
        const int current_count = polling_retry_count_.fetch_add(1, boost::memory_order_relaxed) + 1;
        if (current_count >= max_retries) {
          BOOST_LOG(warning) << "Reached the maximum retry count; giving up on restoring display settings";
          pending_restore_ = false;
          clear_vdd_state();
          return true;
        }
        BOOST_LOG(warning) << "Timer: still waiting for CCD recovery... (Count: " << current_count << "/" << max_retries << ")";
        return false;
      }

      // VDD lifecycle was already decided in restore_state_impl; skip VDD destruction inside revert_settings
      auto result = settings.revert_settings(reason, true);
      BOOST_LOG(info) << "Polling restore of display settings " << (result ? "succeeded" : "failed") << "; not retrying";
      pending_restore_ = false;
      clear_vdd_state();
      return true;
    });
  }

  session_t::session_t():
      timer { std::make_unique<StateRetryTimer>(mutex) } {
  }
}  // namespace display_device
