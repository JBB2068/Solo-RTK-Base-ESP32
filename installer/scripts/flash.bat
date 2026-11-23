@echo off
setlocal
cd /d "%~dp0"
title Solo RTK Base installer
echo.
echo  Solo RTK Base installer
echo  -----------------------
echo  1. Plug the ESP32-C3 into USB.
echo  2. Press any key to start.
echo.
pause >nul

set MODE=%1
if /I "%MODE%"=="update" goto update

echo Installing (erases settings)...
esptool.exe --chip esp32c3 --baud 460800 erase_flash || goto fail
:update
esptool.exe --chip esp32c3 --baud 460800 write_flash 0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin || goto fail

echo.
echo  Done.
echo  Join Wi-Fi "SoloRTK-XXXX" (password solortk1) and open http://192.168.4.1
echo.
pause
exit /b 0

:fail
echo.
echo  FAILED. If no port was found: unplug, hold the BOOT button, plug back in, release, and run this again.
echo.
pause
exit /b 1
