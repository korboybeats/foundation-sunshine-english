/**
 * @file src/platform/windows/dsu_server.h
 * @brief DSU server header. Receives client connections and sends Switch Pro controller motion sensor data.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <atomic>
#include <chrono>
#include <boost/asio.hpp>

namespace platf {

  // DSU protocol base structures
  #pragma pack(push, 1)  // Ensure byte alignment

  // DSU protocol header (16 bytes) - matches the cemuhook standard
  struct dsu_header {
    uint32_t magic;          // 0x53555344 (DSUS)
    uint16_t version;        // Protocol version (1001)
    uint16_t length;         // Data length
    uint32_t crc32;          // CRC32 checksum
    uint32_t client_id;      // Client ID
  };

  // SharedResponse structure
  struct dsu_shared_response {
    uint32_t message_type;   // MessageType (0x100001 INFO, 0x100002 DATA)
    uint8_t slot;            // Slot
    uint8_t slot_state;      // SlotState (0=Disconnected, 1=Reserved, 2=Connected)
    uint8_t device_model;    // DeviceModelType (0=None, 1=PartialGyro, 2=FullGyro)
    uint8_t connection_type; // ConnectionType (0=None, 1=USB, 2=Bluetooth)
    uint8_t mac_address[6];  // Array6<byte> MacAddress (6-byte array)
    uint8_t battery_status;  // BatteryStatus (0=NA, 1=Dying, 2=Low, 3=Medium, 4=High, 5=Full, 6=Charging, 7=Charged)
  };

  // Motion data structure
  struct dsu_motion_data {
    uint64_t motion_timestamp; // Motion timestamp
    float accelerometer_x;   // X-axis acceleration
    float accelerometer_y;   // Y-axis acceleration
    float accelerometer_z;   // Z-axis acceleration
    float gyroscope_pitch;   // X-axis angular velocity
    float gyroscope_yaw;     // Y-axis angular velocity (note: Yaw comes before Roll)
    float gyroscope_roll;    // Z-axis angular velocity
  };

  // INFO response structure - combines header and shared response
  struct dsu_info_response {
    dsu_header header;           // DSU protocol header
    dsu_shared_response shared;  // Shared response section
    uint8_t padding;             // 1-byte padding for the expected 32-byte total size
  };

  // DATA response structure - combines header, shared response, and controller data
  struct dsu_data_packet {
    dsu_header header;           // DSU protocol header
    dsu_shared_response shared;  // Shared response section

    // ControllerDataResponse structure
    uint8_t connected;       // Connection state
    uint32_t packet_id;      // Packet ID
    uint8_t extra_buttons;   // Extra buttons
    uint8_t main_buttons;    // Main buttons
    uint16_t ps_extra_input; // PS extra input
    uint16_t left_stick_xy;  // Left stick XY
    uint16_t right_stick_xy; // Right stick XY
    uint32_t dpad_analog;    // D-pad analog
    uint64_t main_buttons_analog; // Main buttons analog

    uint8_t touch1[6];       // Touch 1 data
    uint8_t touch2[6];       // Touch 2 data

    // Motion data - reuses the motion data struct
    dsu_motion_data motion;  // Motion data section
  };
  #pragma pack(pop)  // Restore default byte alignment

  /**
   * @brief DSU Server. Receives client connections and sends Switch Pro controller motion sensor data.
   * @details Implements a DSU (cemuhook protocol) server that accepts client connection requests and sends motion data.
   */
  class dsu_server_t {
  public:
    /**
     * @brief Constructor
     * @param port Server listen port; defaults to 26760 (DSU standard port)
     */
    explicit dsu_server_t(uint16_t port = 26760);

    /**
     * @brief Destructor
     */
    ~dsu_server_t();

    /**
     * @brief Start the DSU server
     * @return 0 on success, -1 on failure
     */
    int start();

    /**
     * @brief Stop the DSU server
     */
    void stop();

    /**
     * @brief Send motion sensor data to all connected clients
     * @param controller_id Controller ID
     * @param accel_x X-axis acceleration (m/s²)
     * @param accel_y Y-axis acceleration (m/s²)
     * @param accel_z Z-axis acceleration (m/s²)
     * @param gyro_x X-axis angular velocity (deg/s)
     * @param gyro_y Y-axis angular velocity (deg/s)
     * @param gyro_z Z-axis angular velocity (deg/s)
     */
    void send_motion_data(uint32_t controller_id,
                          float accel_x, float accel_y, float accel_z,
                          float gyro_x, float gyro_y, float gyro_z);


    /**
     * @brief Build a client key (in IP:port format)
     * @param client_endpoint Client endpoint
     * @return Client key string
     */
    std::string generate_client_key(const boost::asio::ip::udp::endpoint &client_endpoint) const;

    /**
     * @brief Bulk-update the last-seen time for clients
     * @param client_endpoints List of client endpoints to update
     */
    void update_clients_activity(const std::vector<boost::asio::ip::udp::endpoint> &client_endpoints);

    /**
     * @brief Check whether the server is running
     * @return true if running, false if stopped
     */
    bool is_running() const { return running_; }

    /**
     * @brief Get the number of connected clients
     * @return Number of connected clients
     */
    size_t get_client_count() const { return clients_.size(); }

  private:
    /**
     * @brief Client connection info
     */
    struct client_info_t {
      boost::asio::ip::udp::endpoint endpoint;
      std::chrono::steady_clock::time_point last_seen;
      uint32_t controller_id;
      uint32_t client_id;
      int sendTimeout;  // Timeout counter, matches cemuhook behavior

      // Constructors for convenient initialization
      client_info_t() = default;
      client_info_t(const boost::asio::ip::udp::endpoint &ep, uint32_t ctrl_id, uint32_t cli_id)
        : endpoint(ep), last_seen(std::chrono::steady_clock::now()),
          controller_id(ctrl_id), client_id(cli_id), sendTimeout(0) {}
    };

    /**
     * @brief Motion data structure
     */
    struct motion_data_t {
      float accel_x = 0.0f;
      float accel_y = 0.0f;
      float accel_z = 0.0f;
      float gyro_x = 0.0f;
      float gyro_y = 0.0f;
      float gyro_z = 0.0f;
      std::chrono::steady_clock::time_point last_update = std::chrono::steady_clock::now();
      bool has_accel = false;
      bool has_gyro = false;
    };

    /**
     * @brief Start the receive loop
     */
    void start_receive();

    /**
     * @brief Handle a received packet
     * @param error Error info
     * @param bytes_transferred Number of bytes transferred
     */
    void handle_receive_sync(const boost::system::error_code& error, std::size_t bytes_transferred);

    /**
     * @brief Handle a controller info request
     * @param client_endpoint Client endpoint
     * @param data Packet contents
     * @param size Packet size
     */
    void handle_info_request(const boost::asio::ip::udp::endpoint& client_endpoint,
                            const uint8_t* data, std::size_t size);

    /**
     * @brief Handle a data request
     * @param client_endpoint Client endpoint
     * @param data Packet contents
     * @param size Packet size
     */
    void handle_data_request(const boost::asio::ip::udp::endpoint& client_endpoint,
                            const uint8_t* data, std::size_t size);


    /**
     * @brief Send a packet to the specified client
     * @param client_endpoint Client endpoint
     * @param data Data pointer
     * @param size Data size
     */
    void send_packet_to_client(const boost::asio::ip::udp::endpoint& client_endpoint,
                               const uint8_t* data, size_t size);

    /**
     * @brief Clean up timed-out client connections
     */
    void cleanup_timeout_clients();

    /**
     * @brief Compute the CRC32 checksum
     * @param s Data pointer
     * @param n Data length
     * @return CRC32 value
     */
    uint32_t crc32(const unsigned char *s, size_t n);

    /**
     * @brief Common helper to parse the client ID
     * @param data Packet pointer
     * @return Client ID
     */
    uint32_t parse_client_id(const uint8_t *data);

    /**
     * @brief Common helper that computes the CRC32 and sends the packet
     * @param client_endpoint Client endpoint
     * @param packet Packet pointer
     * @param packet_size Packet size
     */
    void send_packet_with_crc(const boost::asio::ip::udp::endpoint &client_endpoint,
                             void *packet, size_t packet_size);

    /**
     * @brief Check whether a port is available
     * @param port Port number to check
     * @return true if the port is available, false if it is in use
     */
    bool is_port_available(uint16_t port);

    /**
     * @brief Server main loop
     */
    void server_loop();

    boost::asio::io_context io_context_;
    boost::asio::ip::udp::socket socket_;
    boost::asio::ip::udp::endpoint remote_endpoint_;

    std::vector<uint8_t> recv_buffer_;
    std::map<std::string, client_info_t> clients_;
    std::map<uint32_t, motion_data_t> motion_data_;  // Controller ID -> motion data

    // Performance optimization: pre-allocated packet structures so we avoid stack allocations on every call
    dsu_info_response info_packet_;  // Pre-allocated INFO response struct
    dsu_data_packet data_packet_;    // Pre-allocated DATA response struct

    std::thread server_thread_;
    std::atomic<bool> running_;
    uint16_t port_;
    uint32_t packet_counter_;

    static constexpr size_t MAX_PACKET_SIZE = 100;
    static constexpr int CLIENT_TIMEOUT = 40;  // Matches the cemuhook timeout threshold

    // DSU protocol constants - per cemuhook
    static constexpr uint32_t DSU_PROTOCOL_VERSION = 1001;
    static constexpr uint32_t DSU_MESSAGE_TYPE_INFO = 0x100001;     // Controller info request
    static constexpr uint32_t DSU_MESSAGE_TYPE_DATA = 0x100002;     // Data request
  };

} // namespace platf
