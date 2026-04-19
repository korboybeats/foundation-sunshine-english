<div align="center">

<img src="docs/poster.webp" width="800" alt="Foundation Sunshine">

<br>

[![English](https://img.shields.io/badge/English-blue?style=flat-square)](README.md)
[![Français](https://img.shields.io/badge/Français-green?style=flat-square)](README.fr.md)
[![Deutsch](https://img.shields.io/badge/Deutsch-yellow?style=flat-square)](README.de.md)
[![日本語](https://img.shields.io/badge/日本語-purple?style=flat-square)](README.ja.md)

An enhanced fork of [LizardByte/Sunshine](https://github.com/LizardByte/Sunshine), focused on the Windows game streaming experience

[Documentation](https://docs.qq.com/aio/DSGdQc3htbFJjSFdO?p=YTpMj5JNNdB5hEKJhhqlSB) · [LizardByte Docs](https://docs.lizardbyte.dev/projects/sunshine/latest/) · [QQ Group](https://qm.qq.com/cgi-bin/qm/qr?k=5qnkzSaLIrIaU4FvumftZH_6Hg7fUuLD&jump_from=webapi)

</div>

---

### ░▒▓ Core Features

- **Full HDR Pipeline** — Dual-format encoding (PQ + HLG) · Per-frame GPU luminance analysis · HDR10+ / HDR Vivid dynamic metadata · Full static metadata passthrough
- **Virtual Display** — Deep [ZakoVDD](https://github.com/qiin2333/zako-vdd) integration · 5 screen modes · Named Pipe IPC · Per-client GUID sessions
- **Audio Enhancements** — 7.1.4 surround (12ch) · Opus DRED packet loss recovery · Continuous audio stream · Remote microphone · Virtual speaker bit-depth matching
- **Encoding Optimization** — NVENC SDK 13.0 · AMF QVBR/HQVBR · Encoder result caching (260x) · Adaptive downscaling · Vulkan encoder
- **Control Panel** — Tauri 2 + Vue 3 + Vite · Dark mode · QR pairing · Live monitoring
- **Smart Pairing** — Per-client configuration · Automatic device capability matching · Virtual mouse driver (vmouse)

### ░▒▓ Technical Details

<details>
<summary><b>Full HDR Pipeline Architecture</b></summary>

#### Dual-format HDR encoding: HDR10 (PQ) + HLG in parallel

Traditional streaming solutions only support HDR10 (PQ) absolute luminance mapping. When the endpoint device's capabilities are insufficient or its luminance parameters do not match, you get crushed shadows, clipped highlights, and other artifacts.

To address this, HLG (Hybrid Log-Gamma, ITU-R BT.2100) support has been added at the encoding layer, using relative luminance mapping:
- **Scene-referred luminance adaptation**: HLG is based on a relative luminance curve. The display performs tone mapping based on its own peak brightness, so shadow detail on lower-brightness devices is preserved significantly better than with PQ.
- **Smooth highlight rolloff**: HLG's logarithmic-gamma hybrid transfer function provides progressive rolloff in the highlights, avoiding the hard clipping and tonal banding caused by PQ.
- **Native SDR backward compatibility**: HLG signals can be decoded by SDR displays directly as standard BT.709, with no extra tone mapping required.

**Per-frame luminance analysis and adaptive metadata generation**

A real-time luminance analysis module is integrated on the GPU side. Compute shaders run per frame to:
- **Compute MaxFALL / MaxCLL per frame**: Real-time statistics on per-frame maximum content light level (MaxCLL) and frame average light level (MaxFALL) are dynamically injected into HEVC/AV1 SEI/OBU metadata.
- **Robust outlier filtering**: A percentile-clipping strategy rejects extreme-luminance pixels (e.g. specular highlights), preventing isolated bright spots from skewing the global luminance reference and darkening the overall image.
- **Inter-frame exponential smoothing**: Per-frame luminance statistics are filtered with an EMA (exponential moving average) to eliminate brightness flicker caused by metadata jumps at scene cuts.

**Full HDR metadata passthrough**

HDR10 static metadata (Mastering Display Info + Content Light Level) is passed through end-to-end. Bitstreams produced by NVENC / AMF / QSV carry full color volume and luminance information conforming to the CTA-861 specification.

**HDR10+ / HDR Vivid dynamic metadata injection**

In the NVENC encoding pipeline, the per-frame luminance analysis results are used to automatically generate and inject the following dynamic metadata SEI:
- **HDR10+ (ST 2094-40)**: Carries scene-level MaxSCL / distribution percentiles / knee point references for tone mapping. Supports precise tone mapping on HDR10+-certified TVs from Samsung, Panasonic, etc.
- **HDR Vivid (CUVA T/UWA 005.3)**: An ITU-T T.35-registered standard from the China Ultra HD Video Industry Alliance (CUVA). Provides absolute-luminance tone mapping in PQ mode and scene-referred relative-luminance tone mapping in HLG mode, covering the domestic Chinese display ecosystem.

</details>

<details>
<summary><b>Virtual Display Integration</b> (requires Windows 10 22H2+)</summary>

Deep integration with the [ZakoVDD](https://github.com/qiin2333/zako-vdd) virtual display driver:
- Custom resolution and refresh rate support, with 10-bit HDR color depth
- **5 screen-combination modes**: virtual-only, physical-only, hybrid, mirror, extend
- Real-time Named Pipe IPC; the virtual display is automatically created/destroyed when streaming starts/stops
- Each client is bound to its own VDD session (GUID), supporting fast multi-client switching
- Live configuration changes with no restart required

</details>

<details>
<summary><b>Audio Enhancements</b></summary>

- **7.1.4 surround (12 channels)**: Full channel mapping for immersive audio layouts such as Dolby Atmos
- **Opus DRED deep redundancy**: Neural-network-based packet loss recovery; a 100 ms redundancy window smooths over network jitter
- **Continuous audio stream**: Uninterrupted audio stream that fills silence with zero data when no audio is playing, avoiding repeated audio device initialization
- **Virtual speaker auto-matching**: Automatically detects and matches the bit depth (16-bit / 24-bit, etc.) of virtual audio devices

</details>

<details>
<summary><b>Capture and Encoding Optimization</b></summary>

**Capture pipeline**
- **Gamma-aware shaders**: Automatically selects sRGB / linear gamma color conversion based on DXGI ColorSpace
- **High-quality downscaling**: Bicubic interpolation with three quality presets: fast / balanced / high_quality
- **Dynamic resolution detection**: Real-time awareness of display resolution and rotation changes; the encoder adapts automatically
- **GPU luminance analysis**: Two-stage compute-shader reduction, P95/P99 clipping, inter-frame EMA temporal smoothing

**NVENC**
- **SDK 13.0**: Fine-grained rate control and look-ahead
- **HDR metadata API**: Native Mastering Display / Content Light Level writing via NVENC SDK 12.2+
- **HDR10+ / HDR Vivid SEI**: Per-frame automatic generation of ST 2094-40 and CUVA T.35 dynamic metadata
- **SPS bitstream conformance**: Full H.264/HEVC SPS bitstream restrictions are written

**AMF (AMD)**
- **QVBR / HQVBR / HQCBR**: Advanced rate control with UI-adjustable quality levels
- **AV1 low-latency**: AV1 encoder optimization options with no impact on latency

**General**
- **Encoder result caching**: Probe results are persisted; subsequent connections drop from 26 s to <100 ms (260x speedup)
- **Adaptive downscaling**: Bilinear / bicubic / high-quality scaling presets to fit 4K-host → 1080p-stream scenarios
- **Vulkan encoder**: Experimental Vulkan video encoding support
- **Lock-free certificate chain**: `shared_mutex` replaces `mutex`, eliminating TLS queue overhead

</details>

<br>

---

### ░▒▓ Recommended Clients

Pair with the following optimized Moonlight clients for the best experience (unlocks the full feature set):

- **PC** — [Moonlight-PC](https://github.com/qiin2333/moonlight-qt) (Windows · macOS · Linux)
- **Android** — [Power-Up Edition](https://github.com/qiin2333/moonlight-vplus) · [Crown Edition](https://github.com/WACrown/moonlight-android)
- **iOS** — [VoidLink](https://github.com/The-Fried-Fish/VoidLink-previously-moonlight-zwm)
- **HarmonyOS** — [Moonlight V+](https://appgallery.huawei.com/app/detail?id=com.alkaidlab.sdream)

More resources: [awesome-sunshine](https://github.com/LizardByte/awesome-sunshine)

<br>

<details>
<summary><b>░▒▓ System Requirements</b></summary>

| Component | Minimum | Recommended (4K) |
|------|----------|---------|
| **GPU** | AMD VCE 1.0+ / Intel VAAPI / NVIDIA NVENC | AMD VCE 3.1+ / Intel HD 510+ / GTX 1080+ |
| **CPU** | Ryzen 3 / Core i3 | Ryzen 5 / Core i5 |
| **RAM** | 4 GB | 8 GB |
| **OS** | Windows 10 22H2+ | Windows 10 22H2+ |
| **Network** | 5 GHz 802.11ac | CAT5e Ethernet |

GPU compatibility: [NVENC](https://developer.nvidia.com/video-encode-and-decode-gpu-support-matrix-new) · [AMD VCE](https://github.com/obsproject/obs-amd-encoder/wiki/Hardware-Support) · [Intel VAAPI](https://www.intel.com/content/www/us/en/developer/articles/technical/linuxmedia-vaapi.html)

</details>

---

### ░▒▓ Documentation & Support

[![Docs](https://img.shields.io/badge/Documentation-ff69b4?style=flat-square)](https://docs.qq.com/aio/DSGdQc3htbFJjSFdO?p=YTpMj5JNNdB5hEKJhhqlSB) [![LizardByte](https://img.shields.io/badge/LizardByte_Docs-a78bfa?style=flat-square)](https://docs.lizardbyte.dev/projects/sunshine/latest/) [![QQ](https://img.shields.io/badge/QQ_Group-38bdf8?style=flat-square)](https://qm.qq.com/cgi-bin/qm/qr?k=5qnkzSaLIrIaU4FvumftZH_6Hg7fUuLD&jump_from=webapi)

Want to help? → [![Build](https://img.shields.io/badge/Building-34d399?style=flat-square)](docs/building.md) [![Config](https://img.shields.io/badge/Configuration-fbbf24?style=flat-square)](docs/configuration.md) [![WebUI](https://img.shields.io/badge/WebUI_Development-fb923c?style=flat-square)](docs/WEBUI_DEVELOPMENT.md)

<br>

<div align="center">

「 ░▒▓ 」

<a href="https://github.com/qiin2333/foundation-sunshine/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=qiin2333/foundation-sunshine&max=100" />
</a>

<br>

[![Join QQ Group](https://pub.idqqimg.com/wpa/images/group.png 'Join QQ Group')](https://qm.qq.com/cgi-bin/qm/qr?k=WC2PSZ3Q6Hk6j8U_DG9S7522GPtItk0m&jump_from=webapi&authKey=zVDLFrS83s/0Xg3hMbkMeAqI7xoHXaM3sxZIF/u9JW7qO/D8xd0npytVBC2lOS+z)

[![Star History Chart](https://api.star-history.com/svg?repos=qiin2333/Sunshine-Foundation&type=Date)](https://www.star-history.com/#qiin2333/Sunshine-Foundation&Date)

</div>
