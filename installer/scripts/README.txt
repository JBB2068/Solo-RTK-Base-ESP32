Solo RTK Base - installer package

Windows:    double-click flash.bat              (fresh install, erases settings)
            double-click update-keep-settings.bat (update, keeps settings)
Mac/Linux:  ./flash.sh          or   ./flash.sh update
            (needs python3; installs esptool on first run)

Easiest of all: open the web installer in Chrome or Edge (link on the GitHub
release page) and click Install. Nothing to download.

After installing: join Wi-Fi "SoloRTK-XXXX" (password solortk1), open
http://192.168.4.1, enter your SoloLink SSID and password, Save & reboot.

If the port is not found: unplug, hold BOOT, plug in, release BOOT, retry.
