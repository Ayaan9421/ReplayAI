# ValoAI - High-Performance Instant Replay Recorder

ValoAI is a lightweight, high-performance screen recording tool for Windows designed for "Instant Replay" functionality. It captures the last 60 seconds of gameplay (or desktop activity) and saves it to a high-quality MP4 file upon a hotkey press, with minimal CPU overhead.

## 🚀 Key Features

- **Low-Latency Capture:** Uses **Windows Graphics Capture (WGC)** API for efficient, low-impact screen grabbing.
- **Hardware Acceleration:** Leverages **NVIDIA NVENC (Video Codec SDK 13.0)** for blazing-fast H.264 video encoding directly on the GPU.
- **System Audio:** High-fidelity system audio capture via **WASAPI Loopback**.
- **Instant Replay:** Maintains a **Rolling Buffer** in memory/temporary storage for the last 60 seconds of activity.
- **Automatic Muxing:** Seamlessly combines video and audio into a shareable `.mp4` format using FFmpeg.
- **D3D11 Integration:** Direct integration with DirectX 11 for zero-copy frame handling between capture and encoder.

## 🛠️ Project Structure

- `src/capture/`: Windows Graphics Capture (WGC) implementation.
- `src/encoder/`: NVIDIA NVENC hardware encoder wrapper.
- `src/audio/`: WASAPI Loopback recorder for system audio.
- `src/buffer/`: Circular buffer management for the rolling replay window.
- `src/utils/`: FFmpeg muxer, hotkey listener, and other helpers.
- `src/core/`: Direct3D 11 device and context management.

## 📋 Prerequisites

- **OS:** Windows 10 (version 1903 or later) or Windows 11.
- **GPU:** NVIDIA GPU with NVENC support (GeForce 600 series or newer).
- **Software:**
  - [FFmpeg](https://ffmpeg.org/download.html) installed and added to your system `PATH`.
  - NVIDIA Video Codec SDK (included in the `Video_Codec_SDK_13.0.19` folder).
- **Compiler:** MSVC (Visual Studio 2019/2022) with C++17 support.

## 🔨 Build Instructions

The project includes a simple batch script for compilation:

1. Open a **Developer Command Prompt for VS**.
2. Navigate to the `project` directory.
3. Run the build script:
   ```cmd
   build_nvenc.bat main.cpp
   ```
   This will compile `main.cpp` along with all source modules, link the required libraries (`d3d11.lib`, `dxgi.lib`, `nvencodeapi.lib`, etc.), and launch the application.

## 🎮 Usage

1. Run the compiled `main.exe`.
2. The application will start capturing in the background.
3. **Hotkey Trigger:** Press **Ctrl + Shift + F9** to save the last 60 seconds.
4. The output will be saved as `clip.mp4` in the project directory.

## ⚙️ Technical Details

- **Video Format:** H.264 (via NVENC).
- **Audio Format:** AAC (encoded during muxing from WAV).
- **Container:** MP4 (via FFmpeg).
- **Memory Management:** Video fragments are stored in a rolling buffer to prevent excessive RAM usage while maintaining a high bitrate.

## 📝 Roadmap/To-Do

- [ ] Audio/Video synchronization improvements (currently noted in `main.cpp`).
- [ ] Configurable replay duration and bitrate.
- [ ] Support for specific window capture instead of full monitor.
- [ ] GUI for settings and clip management.

---
