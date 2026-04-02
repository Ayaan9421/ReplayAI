@echo off
set SDK="C:\Program Files (x86)\Video_Codec_SDK_13.0.19"
cl /EHsc /std:c++17 %~n1.cpp /I %SDK%\Interface /link /LIBPATH:%SDK%\Lib\x64 nvencodeapi.lib d3d11.lib dxgi.lib dxguid.lib user32.lib
pause
%~n1.exe