/**
 * @file src/platform/windows/dsu_server.cpp
 * @brief DSU server implementation. Receives client connections and sends Switch Pro controller motion sensor data.
 */

#include "dsu_server.h"
#include "src/logging.h"
#include <iomanip>
#include <sstream>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #ifdef _MSC_VER
    #pragma comment(lib, "ws2_32.lib")
  #endif
#endif

namespace platf {

  dsu_server_t::dsu_server_t(uint16_t port):
      socket_(io_context_), recv_buffer_(MAX_PACKET_SIZE), running_(false), port_(port), packet_counter_(0) {
  }

  dsu_server_t::~dsu_server_t() {
    stop();
  }

  int
  dsu_server_t::start() {
    if (running_) {
      BOOST_LOG(warning) << "DSU server is already running";
      return 0;
    }

    try {
      // Check whether the port is available
      BOOST_LOG(info) << "DSU server starting on port: " << port_;

      if (!is_port_available(port_)) {
        BOOST_LOG(warning) << "Port " << port_ << " may be in use; attempting to start anyway...";
      }

      // Bind to the specified port
      socket_.open(boost::asio::ip::udp::v4());

      // Set socket options
      socket_.set_option(boost::asio::ip::udp::socket::reuse_address(true));

      // Set the socket to non-blocking, matching cemuhook behavior
      socket_.non_blocking(true);

      // Workaround for Windows UDP socket error 10054 (remote host forcibly closed connection).
      // This is a known Windows bug; disable connection reset.
      BOOL bNewBehavior = FALSE;
      DWORD dwBytesReturned = 0;
      SOCKET native_socket = socket_.native_handle();
      WSAIoctl(native_socket, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior),
        NULL, 0, &dwBytesReturned, NULL, NULL);
      BOOST_LOG(debug) << "DSU server disabled Windows UDP connection reset (SIO_UDP_CONNRESET)";

      // Try binding the port
      boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::udp::v4(), port_);
      socket_.bind(endpoint);

      running_ = true;

      // Start the server thread
      server_thread_ = std::thread(&dsu_server_t::server_loop, this);

      BOOST_LOG(info) << "DSU server started successfully, listening on port: " << port_
                      << " (IP: " << endpoint.address().to_string() << ")";
      return 0;
    }
    catch (const boost::system::system_error &e) {
      BOOST_LOG(error) << "DSU server failed to start: " << e.what()
                       << " (error code: " << e.code().value() << ")";

      if (e.code() == boost::asio::error::address_in_use) {
        BOOST_LOG(error) << "Port " << port_ << " is already in use; please try a different port";
      }
      else if (e.code() == boost::asio::error::access_denied) {
        BOOST_LOG(error) << "Access denied; check firewall settings or administrator privileges";
      }

      return -1;
    }
    catch (const std::exception &e) {
      BOOST_LOG(error) << "DSU server failed to start: " << e.what();
      return -1;
    }
  }

  void
  dsu_server_t::stop() {
    if (!running_) {
      return;
    }

    running_ = false;

    // Close the socket to interrupt any receive operation
    if (socket_.is_open()) {
      socket_.close();
    }

    // Wait for the server thread to finish
    if (server_thread_.joinable()) {
      server_thread_.join();
    }

    // Clear the client list
    clients_.clear();

    BOOST_LOG(info) << "DSU server stopped";
  }

  void
  dsu_server_t::server_loop() {
    auto last_cleanup = std::chrono::steady_clock::now();
    const auto cleanup_interval = std::chrono::milliseconds(500);  // Clean up every 500ms, matching cemuhook's MAIN_SLEEP_TIME_M

    BOOST_LOG(debug) << "DSU server main loop started";

    while (running_) {
      try {
        // Use synchronous receive, matching cemuhook behavior
        boost::system::error_code ec;
        std::size_t bytes_transferred = socket_.receive_from(
          boost::asio::buffer(recv_buffer_), remote_endpoint_, 0, ec);

        if (!ec) {
          // Process the received packet
          handle_receive_sync(ec, bytes_transferred);
        }
        else if (ec != boost::asio::error::would_block) {
          // Ignore Windows UDP socket error 10054 (remote host forcibly closed connection).
          // This is a known Windows bug, triggered when a client disconnects.
          if (ec.value() != 10054) {
            BOOST_LOG(warning) << "DSU server receive error: " << ec.message()
                               << " (error code: " << ec.value() << ")";
          }
          else {
            BOOST_LOG(debug) << "DSU server ignored Windows UDP connection reset error (10054)";
          }
        }

        // Periodically clean up timed-out clients
        auto now = std::chrono::steady_clock::now();
        if (now - last_cleanup > cleanup_interval) {
          cleanup_timeout_clients();
          last_cleanup = now;
        }

        // Brief sleep to avoid excessive CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
      catch (const std::exception &e) {
        if (running_) {
          BOOST_LOG(error) << "DSU server exception: " << e.what();
        }
      }
    }

    BOOST_LOG(debug) << "DSU server main loop ended";
  }

  void
  dsu_server_t::handle_receive_sync(const boost::system::error_code &ec, std::size_t bytes_transferred) {
    if (!running_) {
      return;
    }

    if (ec) {
      if (ec != boost::asio::error::operation_aborted) {
        BOOST_LOG(warning) << "DSU server receive error: " << ec.message()
                           << " (error code: " << ec.value() << ")";
      }
      return;
    }

    if (bytes_transferred < 4) {
      BOOST_LOG(warning) << "DSU server received an undersized packet: " << bytes_transferred << " bytes";
      return;
    }

    // Parse the header (first 16 bytes)
    if (bytes_transferred < 16) {
      BOOST_LOG(warning) << "DSU server received an undersized packet: " << bytes_transferred << " bytes";
      return;
    }

    // Parse the message type (starting at byte 16)
    uint32_t message_type = *reinterpret_cast<const uint32_t *>(recv_buffer_.data() + 16);

    switch (message_type) {
      case DSU_MESSAGE_TYPE_INFO:
        handle_info_request(remote_endpoint_, recv_buffer_.data(), bytes_transferred);
        break;

      case DSU_MESSAGE_TYPE_DATA:
        handle_data_request(remote_endpoint_, recv_buffer_.data(), bytes_transferred);
        break;

      default:
        BOOST_LOG(debug) << "DSU server received unknown message type: 0x" << std::hex << message_type;
        break;
    }
  }

  void
  dsu_server_t::handle_info_request(const boost::asio::ip::udp::endpoint &client_endpoint,
    const uint8_t *data, std::size_t size) {
    if (size < 20) {  // Need at least 16-byte header + 4-byte message type
      BOOST_LOG(warning) << "DSU server received an undersized INFO request: " << size << " bytes";
      return;
    }

    // Parse the client ID
    uint32_t client_id = parse_client_id(data);

    // Parse the slot from the ControllerInfoRequest (after byte 16)
    uint8_t slot = *(data + 16 + 4);  // Skip the message type, read the slot

    // INFO requests don't manage client connections, just respond with info (matching cemuhook behavior)
    BOOST_LOG(debug) << "DSU server received INFO request - client ID: " << client_id
                     << ", slot: " << (int) slot
                     << ", current client total: " << clients_.size();

    memset(&info_packet_, 0, sizeof(info_packet_));

    // Set DSU protocol header
    info_packet_.header.magic = 0x53555344;  // "DSUS" magic
    info_packet_.header.version = DSU_PROTOCOL_VERSION;
    info_packet_.header.length = sizeof(info_packet_) - sizeof(dsu_header);  // Total length minus header length
    info_packet_.header.client_id = client_id;

    // Set the SharedResponse structure
    info_packet_.shared.message_type = DSU_MESSAGE_TYPE_INFO;  // MessageType
    info_packet_.shared.slot = slot;  // Slot

    info_packet_.shared.slot_state = 2;  // SlotState.Connected
    info_packet_.shared.device_model = 2;  // DeviceModelType.FullGyro (Switch Pro)
    info_packet_.shared.connection_type = 2;  // ConnectionType.Bluetooth

    // Set MAC address (6-byte array, all zeros)
    memset(info_packet_.shared.mac_address, 0, 6);

    // Compatibility with DongGe Helper
    info_packet_.shared.mac_address[0] = 1;

    // Set battery status
    info_packet_.shared.battery_status = 2;  // BatteryStatus.Charging

    info_packet_.padding = 0;

    // Use the helper to compute CRC32 and send
    send_packet_with_crc(client_endpoint, &info_packet_, sizeof(info_packet_));

    BOOST_LOG(debug) << "DSU server sent INFO response - client ID: " << client_id
                     << ", slot: " << (int) slot
                     << ", slot state: " << (int) info_packet_.shared.slot_state
                     << ", device model: " << (int) info_packet_.shared.device_model
                     << ", connection type: " << (int) info_packet_.shared.connection_type
                     << ", battery status: " << (int) info_packet_.shared.battery_status
                     << ", response size: " << sizeof(info_packet_) << " bytes";
  }

  void
  dsu_server_t::handle_data_request(const boost::asio::ip::udp::endpoint &client_endpoint,
    const uint8_t *data, std::size_t size) {
    if (size < 20) {  // Need at least 16-byte header + 4-byte message type
      BOOST_LOG(warning) << "DSU server received an undersized data request: " << size << " bytes";
      return;
    }

    // Use the helper to parse the client ID
    uint32_t client_id = parse_client_id(data);

    // Parse the slot from the ControllerDataRequest (after byte 16)
    uint8_t slot = *(data + 16 + 4);  // Skip the message type, read the slot
    uint32_t controller_id = slot;  // Use the slot as the controller ID

    // Match cemuhook's client management: only manage clients on DATA requests
    std::string client_key = generate_client_key(client_endpoint);
    auto it = clients_.find(client_key);

    if (it == clients_.end()) {
      // New client
      clients_[client_key] = client_info_t(client_endpoint, controller_id, client_id);
      BOOST_LOG(debug) << "DSU server: new client subscribed to data - client ID: " << client_id
                       << ", slot: " << (int) slot
                       << ", client: " << client_endpoint.address().to_string()
                       << ":" << client_endpoint.port()
                       << ", current client total: " << clients_.size();
    }
    else {
      // Existing client; reset the timeout counter (matching cemuhook behavior)
      it->second.sendTimeout = 0;
    }
  }

  void
  dsu_server_t::send_packet_to_client(const boost::asio::ip::udp::endpoint &client_endpoint,
    const uint8_t *data, size_t size) {
    try {
      socket_.send_to(boost::asio::buffer(data, size), client_endpoint);
    }
    catch (const std::exception &e) {
      BOOST_LOG(warning) << "DSU server failed to send packet: " << e.what();
    }
  }

  // Check whether the port is available
  bool
  dsu_server_t::is_port_available(uint16_t port) {
    try {
      boost::asio::io_context io_context;
      boost::asio::ip::udp::socket test_socket(io_context);
      test_socket.open(boost::asio::ip::udp::v4());
      test_socket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port));
      return true;
    }
    catch (const std::exception &) {
      return false;
    }
  }

  // CRC32 calculation - implementation based on cemuhook.cpp
  uint32_t
  dsu_server_t::crc32(const unsigned char *s, size_t n) {
    uint32_t crc = 0xFFFFFFFF;

    int k;
    while (n--) {
      crc ^= *s++;
      for (k = 0; k < 8; k++) {
        crc = crc & 1 ? (crc >> 1) ^ 0xedb88320 : crc >> 1;
      }
    }
    return ~crc;
  }

  // Common helper to parse the client ID
  uint32_t
  dsu_server_t::parse_client_id(const uint8_t *data) {
    return *reinterpret_cast<const uint32_t *>(data + 8);
  }

  // Common helper that computes the CRC32 and sends the packet
  void
  dsu_server_t::send_packet_with_crc(const boost::asio::ip::udp::endpoint &client_endpoint,
    void *packet, size_t packet_size) {
    // Compute the CRC32 checksum
    uint32_t *crc32_ptr = reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(packet) + 8);
    *crc32_ptr = 0;
    *crc32_ptr = crc32(reinterpret_cast<const unsigned char *>(packet), packet_size);

    // Send the packet
    send_packet_to_client(client_endpoint, reinterpret_cast<const uint8_t *>(packet), packet_size);
  }

  void
  dsu_server_t::send_motion_data(uint32_t controller_id,
    float accel_x, float accel_y, float accel_z,
    float gyro_x, float gyro_y, float gyro_z) {
    if (!running_ || clients_.empty()) {
      return;
    }

    // Accumulate motion data
    auto &motion = motion_data_[controller_id];
    motion.last_update = std::chrono::steady_clock::now();

    // Always update accelerometer data when non-zero values are provided
    if (accel_x != 0.0f || accel_y != 0.0f || accel_z != 0.0f) {
      motion.accel_x = accel_x;
      motion.accel_y = accel_y;
      motion.accel_z = accel_z;
      motion.has_accel = true;
      BOOST_LOG(debug) << "DSU server updated accelerometer data - controller ID: " << controller_id
                       << ", acceleration: (" << accel_x << ", " << accel_y << ", " << accel_z << ")";
    }

    // Always update gyroscope data when non-zero values are provided
    if (gyro_x != 0.0f || gyro_y != 0.0f || gyro_z != 0.0f) {
      motion.gyro_x = gyro_x;
      motion.gyro_y = gyro_y;
      motion.gyro_z = gyro_z;
      motion.has_gyro = true;
      BOOST_LOG(debug) << "DSU server updated gyroscope data - controller ID: " << controller_id
                       << ", angular velocity: (" << gyro_x << ", " << gyro_y << ", " << gyro_z << ")";
    }

    // Only send when there is motion data
    if (!motion.has_accel && !motion.has_gyro) {
      return;
    }

    // Use the pre-allocated member to avoid stack allocation
    // Initialize the packet
    memset(&data_packet_, 0, sizeof(data_packet_));

    // Set DSU protocol header - matches the Ryujinx Header layout
    data_packet_.header.magic = 0x53555344;  // "DSUS" magic
    data_packet_.header.version = DSU_PROTOCOL_VERSION;
    data_packet_.header.length = sizeof(data_packet_) - sizeof(dsu_header);  // Total length minus header length
    data_packet_.header.crc32 = 0;  // Computed later
    data_packet_.header.client_id = 0;  // Set later

    // Set SharedResponse - matches Ryujinx expectations
    data_packet_.shared.message_type = DSU_MESSAGE_TYPE_DATA;  // Message type lives inside SharedResponse
    data_packet_.shared.slot = controller_id;
    data_packet_.shared.slot_state = 2;  // Connected
    data_packet_.shared.device_model = 2;  // FullGyro
    data_packet_.shared.connection_type = 1;  // USB
    memset(data_packet_.shared.mac_address, 0, 6);  // Zero out MAC address
    data_packet_.shared.battery_status = 0;  // NA

    // Set the ControllerDataResponse structure
    data_packet_.connected = 1;  // Connected
    data_packet_.packet_id = 0;  // Set later
    data_packet_.extra_buttons = 0;
    data_packet_.main_buttons = 0;
    data_packet_.ps_extra_input = 0;
    data_packet_.left_stick_xy = 0;
    data_packet_.right_stick_xy = 0;
    data_packet_.dpad_analog = 0;
    data_packet_.main_buttons_analog = 0;
    memset(data_packet_.touch1, 0, 6);
    memset(data_packet_.touch2, 0, 6);

    data_packet_.motion.motion_timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
      motion.last_update.time_since_epoch())
                                             .count();
    // Coordinate mapping - matches Ryujinx's expected conversion
    // Ryujinx: X = -AccelerometerX, Y = AccelerometerZ, Z = -AccelerometerY
    data_packet_.motion.accelerometer_x = -motion.accel_x;  // Negate so Ryujinx gets the correct X
    data_packet_.motion.accelerometer_y = -motion.accel_z;  // Negate Z so Ryujinx gets the correct Y
    data_packet_.motion.accelerometer_z = motion.accel_y;   // Map Y directly so Ryujinx gets the correct Z

    // Ryujinx: X = GyroscopePitch, Y = GyroscopeRoll, Z = -GyroscopeYaw
    data_packet_.motion.gyroscope_pitch = motion.gyro_x;    // pitch maps to gyro_x
    data_packet_.motion.gyroscope_yaw = -motion.gyro_y;     // Negate yaw so Ryujinx gets the correct Y
    data_packet_.motion.gyroscope_roll = motion.gyro_z;     // roll maps to gyro_z

    if (clients_.empty()) {
      BOOST_LOG(debug) << "DSU server has no connected clients; skipping motion data send";
      return;
    }

    // Bulk-send to all clients
    for (const auto &[client_key, client_info] : clients_) {
      // Set client ID and packet number
      data_packet_.header.client_id = client_info.client_id;
      data_packet_.packet_id = ++packet_counter_;

      // Use the helper to compute CRC32 and send
      send_packet_with_crc(client_info.endpoint, &data_packet_, sizeof(data_packet_));
    }
  }

  void
  dsu_server_t::cleanup_timeout_clients() {
    auto it = clients_.begin();

    while (it != clients_.end()) {
      it->second.sendTimeout++;
      if (it->second.sendTimeout >= CLIENT_TIMEOUT) {
        BOOST_LOG(debug) << "DSU server cleaning up timed-out client: " << it->first;
        it = clients_.erase(it);
      }
      else {
        ++it;
      }
    }
  }

  std::string
  dsu_server_t::generate_client_key(const boost::asio::ip::udp::endpoint &client_endpoint) const {
    return client_endpoint.address().to_string() + ":" + std::to_string(client_endpoint.port());
  }

  void
  dsu_server_t::update_clients_activity(const std::vector<boost::asio::ip::udp::endpoint> &client_endpoints) {
    auto now = std::chrono::steady_clock::now();

    for (const auto &endpoint : client_endpoints) {
      std::string client_key = generate_client_key(endpoint);
      auto it = clients_.find(client_key);

      if (it != clients_.end()) {
        it->second.last_seen = now;
      }
    }
  }
}  // namespace platf
