/**
 * @file src/platform/windows/mic_write.cpp
 * @brief Implementation for Windows microphone write functionality.
 */
#define INITGUID

// Platform includes
#include <audioclient.h>
#include <avrt.h>
#include <filesystem>
#include <mmdeviceapi.h>
#include <roapi.h>
#include <synchapi.h>
#include <urlmon.h>
#include <winreg.h>

// Local includes
#include "mic_write.h"
#include "misc.h"
#include "src/config.h"
#include "src/logging.h"
#include "src/platform/common.h"

// Lib includes
#include <opus/opus.h>

// Must be the last included file
// clang-format off
#include "PolicyConfig.h"
// clang-format on

// Property key definitions
DEFINE_PROPERTYKEY(PKEY_Device_DeviceDesc, 0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 2);
DEFINE_PROPERTYKEY(PKEY_Device_FriendlyName, 0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);
DEFINE_PROPERTYKEY(PKEY_DeviceInterface_FriendlyName, 0x026e516e, 0xb814, 0x414b, 0x83, 0xcd, 0x85, 0x6d, 0x6f, 0xef, 0x48, 0x22, 2);

namespace platf::audio {

  template <class T>
  void
  co_task_free(T *p) {
    if (p) {
      CoTaskMemFree(p);
    }
  }

  using device_enum_t = util::safe_ptr<IMMDeviceEnumerator, Release<IMMDeviceEnumerator>>;
  using device_t = util::safe_ptr<IMMDevice, Release<IMMDevice>>;
  using collection_t = util::safe_ptr<IMMDeviceCollection, Release<IMMDeviceCollection>>;
  using audio_client_t = util::safe_ptr<IAudioClient, Release<IAudioClient>>;
  using wstring_t = util::safe_ptr<WCHAR, co_task_free<WCHAR>>;
  using policy_t = util::safe_ptr<IPolicyConfig, Release<IPolicyConfig>>;
  using prop_t = util::safe_ptr<IPropertyStore, Release<IPropertyStore>>;

  class prop_var_t {
  public:
    prop_var_t() {
      PropVariantInit(&prop);
    }

    ~prop_var_t() {
      PropVariantClear(&prop);
    }

    PROPVARIANT prop;
  };

  // mic_write_wasapi_t implementation
  mic_write_wasapi_t::~mic_write_wasapi_t() {
    cleanup();
  }

  void
  mic_write_wasapi_t::cleanup() {
    is_cleaning_up.store(true);

    // Wait for audio processing to finish
    if (audio_client) {
      // Stop the audio client
      audio_client->Stop();

      // Wait for the buffer to drain
      UINT32 bufferFrameCount = 0;
      UINT32 padding = 0;
      HRESULT status = audio_client->GetBufferSize(&bufferFrameCount);
      if (SUCCEEDED(status)) {
        // Wait for the buffer to fully drain, up to 500ms
        int max_wait = 50;
        while (SUCCEEDED(audio_client->GetCurrentPadding(&padding)) && padding > 0 && max_wait-- > 0) {
          Sleep(10);
        }
      }
    }

    // COM interface release order matters:
    //   1. audio_render (sub-interface acquired from audio_client)
    //   2. audio_client
    //   3. device_enum
    if (audio_render) {
      audio_render->Release();
      audio_render = nullptr;
    }

    // Explicitly release audio_client and device_enum to ensure the correct release order
    audio_client.reset();
    device_enum.reset();

    if (opus_decoder) {
      opus_decoder_destroy(opus_decoder);
      opus_decoder = nullptr;
    }

    if (mmcss_task_handle) {
      AvRevertMmThreadCharacteristics(mmcss_task_handle);
      mmcss_task_handle = nullptr;
    }

    // Note: do not use BOOST_LOG in the destructor's cleanup to avoid static-object destruction-order issues
  }

  capture_e
  mic_write_wasapi_t::sample(std::vector<float> &sample_out) {
    BOOST_LOG(error) << "mic_write_wasapi_t::sample() should not be called";
    return capture_e::error;
  }

  int
  mic_write_wasapi_t::init() {
    last_seq = 0;
    first_packet = true;
    total_packets = 0;
    packet_loss_count = 0;
    fec_recovered_packets = 0;

    // Initialize the OPUS decoder
    int opus_error;
    opus_decoder = opus_decoder_create(48000, 1, &opus_error);  // 48kHz, mono
    if (opus_error != OPUS_OK) {
      BOOST_LOG(error) << "Failed to create OPUS decoder: " << opus_strerror(opus_error);
      return -1;
    }

    // Initialize the device enumerator
    HRESULT hr = CoCreateInstance(
      CLSID_MMDeviceEnumerator,
      nullptr,
      CLSCTX_ALL,
      IID_IMMDeviceEnumerator,
      (void **) &device_enum);

    if (FAILED(hr)) {
      BOOST_LOG(error) << "Couldn't create Device Enumerator for mic write: [0x" << util::hex(hr).to_string_view() << "]";
      cleanup();
      return -1;
    }

    // Store the original audio device settings
    store_original_audio_settings();

    // Try to create or use a virtual audio device
    if (create_virtual_audio_device() != 0) {
      BOOST_LOG(warning) << "Virtual audio device not available, microphone redirection may not work";
    }

    // Set up loopback
    if (setup_virtual_mic_loopback() != 0) {
      BOOST_LOG(warning) << "Failed to setup virtual microphone loopback";
    }

    // For microphone redirection we need a virtual audio output device
    device_t device;

    auto vb_matched = find_device_id({ { match_field_e::adapter_friendly_name, L"VB-Audio Virtual Cable" } });
    if (vb_matched) {
      hr = device_enum->GetDevice(vb_matched->second.c_str(), &device);
      if (SUCCEEDED(hr) && device) {
        BOOST_LOG(info) << "Using VB-Audio Virtual Cable for client mic redirection";
      }
    }

    // As a last resort, fall back to the default speaker device
    if (FAILED(hr) || !device) {
      hr = device_enum->GetDefaultAudioEndpoint(eRender, eConsole, &device);
      if (SUCCEEDED(hr) && device) {
        BOOST_LOG(info) << "Using default console audio output device for client mic redirection";
      }
    }

    if (FAILED(hr) || !device) {
      BOOST_LOG(error) << "No suitable audio output device available for client mic redirection";
      cleanup();
      return -1;
    }

    // Activate IAudioClient
    auto status = device->Activate(
      IID_IAudioClient,
      CLSCTX_ALL,
      nullptr,
      (void **) &audio_client);
    if (FAILED(status) || !audio_client) {
      BOOST_LOG(error) << "Failed to activate IAudioClient for mic write: [0x" << util::hex(status).to_string_view() << "]";

      // Get device info for debugging
      wstring_t device_id;
      device->GetId(&device_id);
      BOOST_LOG(error) << "Device ID: " << platf::to_utf8(device_id.get());

      cleanup();
      return -1;
    }

    // Try multiple audio formats, starting with the most compatible
    std::vector<WAVEFORMATEX> formats_to_try = {
      // 16-bit mono, 48kHz
      { WAVE_FORMAT_PCM, 1, 48000, 96000, 2, 16, 0 },
      // 16-bit mono, 44.1kHz
      { WAVE_FORMAT_PCM, 1, 44100, 88200, 2, 16, 0 },
      // 16-bit stereo, 48kHz
      { WAVE_FORMAT_PCM, 2, 48000, 192000, 4, 16, 0 },
      // 16-bit stereo, 44.1kHz
      { WAVE_FORMAT_PCM, 2, 44100, 176400, 4, 16, 0 },
    };

    HRESULT init_status = E_FAIL;
    WAVEFORMATEX *used_format = nullptr;

    for (const auto &format : formats_to_try) {
      BOOST_LOG(debug) << "Trying audio format: " << format.nChannels << " channels, "
                       << format.nSamplesPerSec << " Hz, " << format.wBitsPerSample << " bits";

      init_status = audio_client->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0,  // No special flags
        1000000,  // 100ms buffer (10000000 was 10 seconds)
        0,
        &format,
        nullptr);

      if (SUCCEEDED(init_status)) {
        used_format = const_cast<WAVEFORMATEX *>(&format);
        BOOST_LOG(info) << "Successfully initialized with format: " << format.nChannels << " channels, "
                        << format.nSamplesPerSec << " Hz, " << format.wBitsPerSample << " bits";
        break;
      }
      else {
        BOOST_LOG(debug) << "Format failed: [0x" << util::hex(init_status).to_string_view() << "]";
      }
    }

    if (FAILED(init_status)) {
      BOOST_LOG(error) << "Failed to initialize IAudioClient with any supported format: [0x" << util::hex(init_status).to_string_view() << "]";
      cleanup();
      return -1;
    }

    // Save the format that was used
    current_format = *used_format;

    // Start the audio client
    status = audio_client->Start();
    if (FAILED(status)) {
      BOOST_LOG(error) << "Failed to start IAudioClient for mic write: [0x" << util::hex(status).to_string_view() << "]";
      cleanup();
      return -1;
    }

    // Get IAudioRenderClient - used to write audio data
    status = audio_client->GetService(IID_IAudioRenderClient, (void **) &audio_render);
    if (FAILED(status) || !audio_render) {
      BOOST_LOG(error) << "Failed to get IAudioRenderClient for mic write: [0x" << util::hex(status).to_string_view() << "]";
      audio_client->Stop();
      cleanup();
      return -1;
    }

    // Set MMCSS priority
    {
      DWORD task_index = 0;
      mmcss_task_handle = AvSetMmThreadCharacteristics("Pro Audio", &task_index);
      if (!mmcss_task_handle) {
        BOOST_LOG(warning) << "Couldn't associate mic write thread with Pro Audio MMCSS task [0x" << util::hex(GetLastError()).to_string_view() << ']';
      }
    }

    BOOST_LOG(info) << "Successfully initialized mic write device with OPUS decoder";
    return 0;
  }

  int
  mic_write_wasapi_t::write_data(const char *data, size_t len, uint16_t seq) {
    if (!audio_client || !audio_render || !opus_decoder) {
      BOOST_LOG(error) << "Mic write device not initialized";
      return -1;
    }

    std::vector<int16_t> pcm_mono_buffer;
    ++total_packets;
    // FEC recovery: check for packet loss using sequence number
    if (seq != 0 && !first_packet) {
      uint16_t expected_seq = last_seq + 1;
      if (seq != expected_seq && seq > expected_seq) {
        // Packet loss detected, try to recover using FEC from current packet
        uint16_t lost_count = seq - expected_seq;
        packet_loss_count += lost_count;
        BOOST_LOG(verbose) << "Mic packet loss detected: expected " << expected_seq << ", got " << seq << " (lost " << lost_count << ")";
        
        // Use FEC to recover the previous lost packet from current packet's redundancy data
        // FEC can only recover one packet (the immediately preceding one)
        if (lost_count == 1) {
          int fec_frame_size = opus_decoder_get_nb_samples(opus_decoder, (const unsigned char *) data, len);
          if (fec_frame_size > 0) {
            std::vector<int16_t> fec_buffer(fec_frame_size);
            int fec_samples = opus_decode(
              opus_decoder,
              (const unsigned char *) data,
              len,
              fec_buffer.data(),
              fec_frame_size,
              1  // FEC recovery mode
            );
            if (fec_samples > 0) {
              BOOST_LOG(verbose) << "FEC recovered " << fec_samples << " samples for lost packet";
              ++fec_recovered_packets;
              // Write recovered audio (will be done together with current packet below)
              pcm_mono_buffer = std::move(fec_buffer);
            }
          }
        }
      }
    }

    // Update sequence tracking
    if (seq != 0) {
      last_seq = seq;
      first_packet = false;
    }

    // Decode the OPUS data
    int frame_size = opus_decoder_get_nb_samples(opus_decoder, (const unsigned char *) data, len);
    if (frame_size < 0) {
      BOOST_LOG(error) << "Failed to get OPUS frame size: " << opus_strerror(frame_size);
      return -1;
    }

    // If we recovered FEC data, append current frame; otherwise just decode current
    size_t fec_offset = pcm_mono_buffer.size();
    pcm_mono_buffer.resize(fec_offset + frame_size);

    int samples_decoded = opus_decode(
      opus_decoder,
      (const unsigned char *) data,
      len,
      pcm_mono_buffer.data() + fec_offset,
      frame_size,
      0  // Normal decode
    );

    if (samples_decoded < 0) {
      BOOST_LOG(error) << "Failed to decode OPUS data: " << opus_strerror(samples_decoded);
      return -1;
    }

    // Handle channel conversion if necessary
    std::vector<int16_t> pcm_output_buffer;
    UINT32 framesToWrite;

    if (current_format.nChannels == 1) {
      // Mono output, direct copy
      pcm_output_buffer = std::move(pcm_mono_buffer);
      framesToWrite = samples_decoded;
    }
    else if (current_format.nChannels == 2) {
      // Stereo output, duplicate mono samples
      pcm_output_buffer.resize(samples_decoded * 2);
      for (int i = 0; i < samples_decoded; ++i) {
        pcm_output_buffer[i * 2] = pcm_mono_buffer[i];  // Left channel
        pcm_output_buffer[i * 2 + 1] = pcm_mono_buffer[i];  // Right channel
      }
      framesToWrite = samples_decoded;  // Each original mono sample becomes one stereo frame
    }
    else {
      BOOST_LOG(error) << "Unsupported channel count for mic write: " << current_format.nChannels;
      return -1;
    }

    // Get the buffer size and the number of frames currently filled
    UINT32 bufferFrameCount = 0;
    UINT32 padding = 0;
    auto status = audio_client->GetBufferSize(&bufferFrameCount);
    if (FAILED(status)) {
      if (status == AUDCLNT_E_DEVICE_INVALIDATED) {
        BOOST_LOG(warning) << "Audio device invalidated during mic write (GetBufferSize)";
        return -2;  // Special return value indicating device invalidated
      }
      BOOST_LOG(error) << "Failed to get buffer size for mic write: [0x" << util::hex(status).to_string_view() << "]";
      return -1;
    }
    status = audio_client->GetCurrentPadding(&padding);
    if (FAILED(status)) {
      if (status == AUDCLNT_E_DEVICE_INVALIDATED) {
        BOOST_LOG(warning) << "Audio device invalidated during mic write (GetCurrentPadding)";
        return -2;  // Special return value indicating device invalidated
      }
      BOOST_LOG(error) << "Failed to get current padding for mic write: [0x" << util::hex(status).to_string_view() << "]";
      return -1;
    }

    // Ensure padding does not exceed the buffer size
    if (padding > bufferFrameCount) {
      BOOST_LOG(warning) << "Invalid padding value: " << padding << " > " << bufferFrameCount << ", using 0";
      padding = 0;
    }

    UINT32 availableFrames = bufferFrameCount - padding;

    // If there isn't enough buffer space, try waiting in several iterations
    if (framesToWrite > availableFrames) {
      BOOST_LOG(verbose) << "Buffer full, waiting for space. Need: " << framesToWrite << ", Available: " << availableFrames;

      // Up to 3 attempts, with increasing wait time
      const int max_retries = 3;
      for (int retry = 0; retry < max_retries && framesToWrite > availableFrames; ++retry) {
        // Compute the wait time from the frames required: frames / 48000 * 1000 (ms)
        // Conservative estimate: wait 80% of the required time
        DWORD wait_ms = static_cast<DWORD>((framesToWrite - availableFrames) * 1000 / 48000 * 0.8);
        wait_ms = std::max(wait_ms, 5ul);  // Wait at least 5ms
        wait_ms = std::min(wait_ms, 50ul); // Wait at most 50ms

        Sleep(wait_ms);

        // Re-check available space
        status = audio_client->GetCurrentPadding(&padding);
        if (FAILED(status)) {
          BOOST_LOG(error) << "Failed to get current padding after wait: [0x" << util::hex(status).to_string_view() << "]";
          return -1;
        }

        if (padding > bufferFrameCount) {
          padding = 0;
        }

        availableFrames = bufferFrameCount - padding;

        if (framesToWrite <= availableFrames) {
          BOOST_LOG(verbose) << "Buffer space available after " << (retry + 1) << " retries";
          break;
        }
      }

      // If there's still not enough space, downgrade to a warning and truncate
      if (framesToWrite > availableFrames) {
        BOOST_LOG(warning) << "Mic write buffer still full after retries: " << framesToWrite << " frames requested, " << availableFrames << " available. Truncating.";
        framesToWrite = availableFrames;
      }
    }

    if (framesToWrite == 0) {
      return 0;
    }

    // Acquire the render buffer
    BYTE *pData = nullptr;
    status = audio_render->GetBuffer(framesToWrite, &pData);
    if (FAILED(status)) {
      if (status == AUDCLNT_E_DEVICE_INVALIDATED) {
        BOOST_LOG(warning) << "Audio device invalidated during mic write (GetBuffer)";
        return -2;  // Special return value indicating device invalidated
      }
      BOOST_LOG(error) << "Failed to get render buffer for mic write: [0x" << util::hex(status).to_string_view() << "]";
      return -1;
    }

    // Copy decoded PCM data into the buffer
    memcpy(pData, pcm_output_buffer.data(), framesToWrite * current_format.nBlockAlign);

    // Release the buffer
    status = audio_render->ReleaseBuffer(framesToWrite, 0);
    if (FAILED(status)) {
      if (status == AUDCLNT_E_DEVICE_INVALIDATED) {
        BOOST_LOG(warning) << "Audio device invalidated during mic write (ReleaseBuffer)";
        return -2;  // Special return value indicating device invalidated
      }
      BOOST_LOG(error) << "Failed to release render buffer for mic write: [0x" << util::hex(status).to_string_view() << "]";
      return -1;
    }

    return framesToWrite * current_format.nBlockAlign;  // Return the actual number of bytes written
  }

  int
  mic_write_wasapi_t::test_write() {
    if (!audio_client || !audio_render || !opus_decoder) {
      BOOST_LOG(error) << "Mic write device not initialized for test";
      return -1;
    }

    // Build a simple test audio buffer (silence)
    const int test_frames = 480;  // 10ms at 48kHz
    const int test_bytes = test_frames * current_format.nBlockAlign;
    std::vector<BYTE> test_data(test_bytes, 0);  // All-zero data (silence)

    BOOST_LOG(info) << "Testing client mic redirection with " << test_frames << " frames, " << test_bytes << " bytes";

    return write_data(reinterpret_cast<const char *>(test_data.data()), test_bytes);
  }

  int
  mic_write_wasapi_t::create_virtual_audio_device() {
    BOOST_LOG(info) << "Attempting to create/use virtual audio device for client mic redirection";

    // Check for the VB-Cable virtual device
    auto vb_matched = find_device_id({ { match_field_e::adapter_friendly_name, L"VB-Audio Virtual Cable" } });
    if (vb_matched) {
      BOOST_LOG(info) << "Found existing VB-Audio Virtual Cable device";
      virtual_device_type = VirtualDeviceType::VB_CABLE;
      return 0;  // Device already exists
    }

    BOOST_LOG(debug) << "VB-Cable not found, attempting to download...";

    // Check whether the VB-Cable driver is already installed
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\VB\\VBAudioVAC", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
      RegCloseKey(hKey);
      BOOST_LOG(info) << "VB-Cable driver is already installed";
      return -1;  // Installed but device not found, likely disabled
    }

    // Check whether the installer was already downloaded and extracted (avoid duplicate downloads)
    std::wstring extract_path = std::filesystem::temp_directory_path().wstring() + L"\\VBCABLE_Install";
    std::wstring installer_path = extract_path + L"\\VBCABLE_Setup_x64.exe";
    if (std::filesystem::exists(installer_path)) {
      // Installer already exists; just prompt the user to install it
      BOOST_LOG(warning) << "VB-Cable already downloaded to: " << platf::to_utf8(extract_path) << " ; Please run 'VBCABLE_Setup_x64.exe' as administrator to install, then restart Sunshine";
      return -1;
    }

    // Download VB-Cable
    BOOST_LOG(debug) << "Downloading VB-Cable...";

    // Download the VB-Cable installer
    std::wstring download_url = L"https://download.vb-audio.com/Download_CABLE/VBCABLE_Driver_Pack43.zip";
    std::wstring temp_path = std::filesystem::temp_directory_path().wstring() + L"\\VBCABLE_Driver_Pack43.zip";

    HMODULE urlmon = LoadLibraryW(L"urlmon.dll");
    if (!urlmon) {
      BOOST_LOG(warning) << "VB-Cable is required for microphone streaming. Please download from: https://vb-audio.com/Cable/";
      return -1;
    }

    auto URLDownloadToFileW_ptr = (decltype(URLDownloadToFileW) *) GetProcAddress(urlmon, "URLDownloadToFileW");
    if (!URLDownloadToFileW_ptr || URLDownloadToFileW_ptr(nullptr, download_url.c_str(), temp_path.c_str(), 0, nullptr) != S_OK) {
      BOOST_LOG(warning) << "Failed to download VB-Cable. Please download manually from: https://vb-audio.com/Cable/";
      FreeLibrary(urlmon);
      return -1;
    }
    FreeLibrary(urlmon);

    // Extract the installer to a user-accessible location
    std::error_code ec;
    std::filesystem::create_directories(extract_path, ec);
    if (ec && ec != std::errc::file_exists) {
      BOOST_LOG(error) << "Failed to create extraction directory: " << ec.message();
      return -1;
    }

    // Extract VB-Cable
    BOOST_LOG(debug) << "Extracting VB-Cable...";
    std::wstring extract_cmd = L"powershell -command \"Expand-Archive -Path '" + temp_path + L"' -DestinationPath '" + extract_path + L"' -Force\"";

    if (_wsystem(extract_cmd.c_str()) != 0) {
      BOOST_LOG(error) << "Failed to extract VB-Cable installer";
      return -1;
    }

    // Guide the user through manual installation
    BOOST_LOG(warning) << "VB-Cable downloaded to: " << platf::to_utf8(extract_path) << " ; Please run 'VBCABLE_Setup_x64.exe' as administrator to install, then restart Sunshine";

    return -1;
  }

  std::optional<matched_field_t>
  mic_write_wasapi_t::find_device_id(const match_fields_list_t &match_list) {
    if (match_list.empty() || !device_enum) {
      return std::nullopt;
    }

    collection_t collection;
    auto status = device_enum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(status)) {
      BOOST_LOG(error) << "Couldn't enumerate render devices: [0x"sv << util::hex(status).to_string_view() << ']';
      return std::nullopt;
    }

    return find_device_in_collection(collection.get(), match_list);
  }

  std::optional<matched_field_t>
  mic_write_wasapi_t::find_capture_device_id(const match_fields_list_t &match_list) {
    if (match_list.empty() || !device_enum) {
      return std::nullopt;
    }

    collection_t collection;
    auto status = device_enum->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(status)) {
      BOOST_LOG(error) << "Couldn't enumerate capture devices: [0x"sv << util::hex(status).to_string_view() << ']';
      return std::nullopt;
    }

    return find_device_in_collection(collection.get(), match_list);
  }

  std::optional<matched_field_t>
  mic_write_wasapi_t::find_device_in_collection(void *collection_ptr, const match_fields_list_t &match_list) {
    auto collection = static_cast<IMMDeviceCollection *>(collection_ptr);
    UINT count = 0;
    collection->GetCount(&count);

    std::vector<std::wstring> matched(match_list.size());
    for (auto x = 0; x < count; ++x) {
      device_t device;
      collection->Item(x, &device);

      wstring_t wstring_id;
      device->GetId(&wstring_id);
      std::wstring device_id = wstring_id.get();

      prop_t prop;
      device->OpenPropertyStore(STGM_READ, &prop);

      prop_var_t adapter_friendly_name;
      prop_var_t device_friendly_name;
      prop_var_t device_desc;

      prop->GetValue(PKEY_Device_FriendlyName, &device_friendly_name.prop);
      prop->GetValue(PKEY_DeviceInterface_FriendlyName, &adapter_friendly_name.prop);
      prop->GetValue(PKEY_Device_DeviceDesc, &device_desc.prop);

      for (size_t i = 0; i < match_list.size(); i++) {
        if (matched[i].empty()) {
          const wchar_t *match_value = nullptr;
          switch (match_list[i].first) {
            case match_field_e::device_id:
              match_value = device_id.c_str();
              break;

            case match_field_e::device_friendly_name:
              match_value = device_friendly_name.prop.pwszVal;
              break;

            case match_field_e::adapter_friendly_name:
              match_value = adapter_friendly_name.prop.pwszVal;
              break;

            case match_field_e::device_description:
              match_value = device_desc.prop.pwszVal;
              break;
          }
          if (match_value && std::wcscmp(match_value, match_list[i].second.c_str()) == 0) {
            matched[i] = device_id;
          }
        }
      }
    }

    for (size_t i = 0; i < match_list.size(); i++) {
      if (!matched[i].empty()) {
        return matched_field_t(match_list[i].first, matched[i]);
      }
    }

    return std::nullopt;
  }

  HRESULT
  mic_write_wasapi_t::set_default_device_all_roles(const std::wstring &device_id) {
    IPolicyConfig *policy_raw = nullptr;
    HRESULT hr = CoCreateInstance(
      CLSID_CPolicyConfigClient,
      nullptr,
      CLSCTX_ALL,
      IID_IPolicyConfig,
      (void **) &policy_raw);

    if (FAILED(hr) || !policy_raw) {
      BOOST_LOG(error) << "Couldn't create PolicyConfig instance: [0x" << util::hex(hr).to_string_view() << "]";
      return hr;
    }

    policy_t policy(policy_raw);
    hr = policy->SetDefaultEndpoint(device_id.c_str(), eCommunications);
    if (FAILED(hr)) {
      BOOST_LOG(error) << "Failed to set device as default communications device: [0x" << util::hex(hr).to_string_view() << "]";
      return hr;
    }

    hr = policy->SetDefaultEndpoint(device_id.c_str(), eConsole);
    if (FAILED(hr)) {
      BOOST_LOG(error) << "Failed to set device as default console device: [0x" << util::hex(hr).to_string_view() << "]";
      return hr;
    }

    return S_OK;
  }

  int
  mic_write_wasapi_t::setup_virtual_mic_loopback() {
    if (virtual_device_type == VirtualDeviceType::NONE) {
      BOOST_LOG(warning) << "No virtual device available for loopback setup";
      return -1;
    }

    BOOST_LOG(info) << "Setting up virtual microphone loopback for client mic redirection";

    // Set up loopback based on the virtual device type
    switch (virtual_device_type) {
      case VirtualDeviceType::STEAM:
        return setup_steam_mic_loopback();
      case VirtualDeviceType::VB_CABLE:
        return setup_vb_cable_mic_loopback();
      default:
        BOOST_LOG(warning) << "Unknown virtual device type for loopback setup";
        return -1;
    }
  }

  int
  mic_write_wasapi_t::setup_steam_mic_loopback() {
    BOOST_LOG(info) << "Setting up Steam virtual microphone loopback";

    // Steam Streaming Speakers automatically loops back to the Steam Streaming Microphone.
    // We need to make sure the Steam Streaming Microphone is set as the default recording device.
    if (auto steam_mic = find_capture_device_id({ { match_field_e::adapter_friendly_name, L"Steam Streaming Microphone" } })) {
      HRESULT hr = set_default_device_all_roles(steam_mic->second);
      if (FAILED(hr)) {
        BOOST_LOG(error) << "Failed to set Steam Streaming Microphone as default device: [0x" << util::hex(hr).to_string_view() << "]";
        return -1;
      }
    }
    return 0;
  }

  int
  mic_write_wasapi_t::setup_vb_cable_mic_loopback() {
    BOOST_LOG(info) << "Setting up VB-Cable virtual microphone loopback";

    // 1. Check whether the VB-Cable input device exists
    auto vb_input = find_capture_device_id({ { match_field_e::adapter_friendly_name, L"VB-Audio Virtual Cable" } });
    if (!vb_input) {
      BOOST_LOG(warning) << "VB-Cable Input device not found";
      return -1;
    }

    // 2. Set VB-Cable as the default recording device
    HRESULT hr = set_default_device_all_roles(vb_input->second);
    if (FAILED(hr)) {
      BOOST_LOG(error) << "Failed to set VB-Cable as default device: [0x" << util::hex(hr).to_string_view() << "]";
      return -1;
    }
    restoration_state.input_device_changed = true;
    BOOST_LOG(info) << "Successfully set VB-Cable as default recording device";

    // 3. Check the VB-Cable output device
    auto vb_output = find_device_id({ { match_field_e::adapter_friendly_name, L"VB-Audio Virtual Cable" } });
    if (!vb_output) {
      BOOST_LOG(info) << "VB-Cable output device not found, skipping output device check";
      return 0;
    }

    // 4. Check whether VB-Cable is currently the default playback device
    device_t default_device;
    if (FAILED(device_enum->GetDefaultAudioEndpoint(eRender, eConsole, &default_device)) || !default_device) {
      BOOST_LOG(warning) << "Failed to get default playback device";
      return 0;
    }

    wstring_t default_id;
    if (FAILED(default_device->GetId(&default_id))) {
      BOOST_LOG(warning) << "Failed to get default playback device ID";
      return 0;
    }

    if (default_id.get() != vb_output->second) {
      BOOST_LOG(info) << "VB-Cable is not the default playback device, no need to switch";
      return 0;
    }

    // 5. Find an alternative playback device
    BOOST_LOG(info) << "VB-Cable is currently the default playback device, switching to alternative...";
    collection_t collection;
    if (FAILED(device_enum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection))) {
      BOOST_LOG(error) << "Failed to enumerate audio endpoints";
      return -1;
    }

    UINT count = 0;
    collection->GetCount(&count);

    for (UINT i = 0; i < count; ++i) {
      device_t device;
      if (FAILED(collection->Item(i, &device))) {
        continue;
      }

      wstring_t device_id;
      if (FAILED(device->GetId(&device_id))) {
        continue;
      }

      if (device_id.get() != vb_output->second) {
        if (SUCCEEDED(set_default_device_all_roles(device_id.get()))) {
          BOOST_LOG(info) << "Successfully changed default playback device to: " << platf::to_utf8(device_id.get());
          // restoration_state.output_device_changed = true;
          BOOST_LOG(info) << "VB-Cable virtual microphone loopback successfully configured";
          return 0;
        }
      }
    }

    BOOST_LOG(error) << "No alternative playback device available";
    return -1;
  }

  void
  mic_write_wasapi_t::store_original_audio_settings() {
    if (restoration_state.settings_stored) {
      return;
    }

    if (!device_enum) {
      BOOST_LOG(warning) << "Device enumerator not available, skipping audio settings storage";
      return;
    }

    // Fetch and store the current default input device ID
    device_t default_input;
    if (SUCCEEDED(device_enum->GetDefaultAudioEndpoint(eCapture, eConsole, &default_input)) && default_input) {
      wstring_t device_id;
      if (SUCCEEDED(default_input->GetId(&device_id))) {
        restoration_state.original_input_device_id = device_id.get();
        BOOST_LOG(debug) << "Stored original input device: " << platf::to_utf8(restoration_state.original_input_device_id);
      }
      else {
        BOOST_LOG(warning) << "Failed to obtain input device ID";
      }
    }
    else {
      BOOST_LOG(warning) << "Failed to obtain default input device";
    }

    restoration_state.settings_stored = true;
    BOOST_LOG(info) << "Original audio device settings have been stored";
  }

  int
  mic_write_wasapi_t::restore_audio_devices() {
    if (!restoration_state.settings_stored) {
      BOOST_LOG(debug) << "No audio device settings to restore";
      return 0;
    }

    BOOST_LOG(info) << "Restoring audio devices to original state";

    // Log FEC statistics
    if (total_packets > 0) {
      double loss_rate = (double)packet_loss_count / (total_packets + packet_loss_count) * 100.0;
      BOOST_LOG(info) << "Microphone Audio Stats, Total Audio Packets: " << total_packets
                      << ", Packet Loss: " << packet_loss_count << " (" << std::fixed << std::setprecision(1) << loss_rate << "%)"
                      << ", FEC Recovered: " << fec_recovered_packets;
    }

    int result = 0;

    // Restore the input device
    if (restoration_state.input_device_changed) {
      if (restore_original_input_device() != 0) {
        result = -1;
      }
    }

    // Reset the restoration state
    restoration_state.input_device_changed = false;
    restoration_state.settings_stored = false;

    BOOST_LOG(info) << "Audio device restoration " << (result == 0 ? "completed successfully" : "completed with errors");
    return result;
  }

  int
  mic_write_wasapi_t::restore_original_input_device() {
    if (restoration_state.original_input_device_id.empty()) {
      BOOST_LOG(warning) << "No original input device ID stored";
      return -1;
    }

    BOOST_LOG(info) << "Restoring original input device: " << platf::to_utf8(restoration_state.original_input_device_id);

    HRESULT hr = set_default_device_all_roles(restoration_state.original_input_device_id);
    if (FAILED(hr)) {
      BOOST_LOG(error) << "Failed to restore original input device: [0x" << util::hex(hr).to_string_view() << "]";
      return -1;
    }

    BOOST_LOG(info) << "Successfully restored original input device";
    return 0;
  }

}  // namespace platf::audio