@echo off
set SDK="./Video_Codec_SDK_13.0.19"
cl /EHsc /std:c++17 %~n1.cpp src\core\D3DContext.cpp src\capture\ScreenCaptureWGC.cpp src/encoder/NVEncoder.cpp src/buffer/RollingBufferManager.cpp src/utils/HotkeyListener.cpp src/audio/AudioDeviceManager.cpp src/utils/FFmpegMuxer.cpp src/utils/FFmpegAudioRecorder.cpp src/audio/WASAPILoopbackRecorder.cpp /I src /I %SDK%\Interface  user32.lib d3d11.lib dxgi.lib dxguid.lib windowsapp.lib /link /LIBPATH:%SDK%\Lib\x64 nvencodeapi.lib
pause
%~n1.exe