# OpenIPC-Adaptive-Link
**Greg's Adaptive-Link - For OpenIPC Camera and Radxa Zero 3w/e or Android PixelPilot Ground Station**

Warning: Mindfully set power levels appropriately in /etc/txprofiles.conf.  Default is an attempt at MarioAIO-safe, max 30 (Which is quite low for a lot of cards). 

About the files above. There are some older versions eg 42q etc still there for reference. Current code for latest release is in alink_drone.c (and it's conf files alink.conf and txprofiles.conf) and alink_gs. 


**Installation**


1. A recent OpenIPC firmware for Sigmastar including wfb tunnel is required. It is recommended to upgrade camera to latest OpenIPC (Warning: All files will be overwritten)

`sysupgrade -k -r -n`

2. For manual file download; check out releases --------->

OR Auto-install on drone --> run this (fetches and installs latest pre-release, for now)

```
cd /etc
curl -L -o alink_install.sh https://raw.githubusercontent.com/sickgreg/OpenIPC-Adaptive-Link/refs/heads/main/alink_install.sh
chmod +x alink_install.sh
./alink_install.sh drone install
reboot
```
Config files are `/etc/txprofiles.conf` and `/etc/alink.conf`

3. PixelPilot Android... Still testing early pre-release.  Stay tuned.

4. Auto-install on ground station

```
curl -L -o alink_install.sh https://raw.githubusercontent.com/sickgreg/OpenIPC-Adaptive-Link/refs/heads/main/alink_install.sh
sudo chmod +x alink_install.sh
sudo ./alink_install.sh gs install
```

Service will be added to systemd. Use stop | start | disable | enable | status with `sudo systemctl stop alink_gs`

`alink_gs.conf` config file will be created in `/config/` if it exists or `/home/radxa` if it exists or finally `/etc` if neither of those exist

5.  Set power level on your ground station wifi card(s)

alink_drone listens for an informative_heartbeat from the ground station to decide on link speed and range and therefor video quality.  If it doesn't hear anything for a preset period of time, it will fall back to profile 999, which is set to MCS 0 for the furthest possible range.  If you're not getting out of this mode, the drone isn't hearing the ground station.

Whenever an informative_heartbeat is heard, information about the link imbedded in the hearbeat is used by alink_drone to set link speed appropriatly.  We need to set ground station power levels high enough to be heard by the drone.

I have found 20 to 40 for AU cards to be appropriate


Here's how i have been setting the cards power. Works for 8812AU cards

```
#/bin/bash

# usage ./setpower.sh <pwrlvl>
echo Stopping wifibroadcast
sudo systemctl stop wifibroadcast
echo Unloading 88XXau_wfb  module
sudo modprobe -r 88XXau_wfb &&
echo Loading 88XXau_wfb module with power level $1
sudo modprobe 88XXau_wfb rtw_tx_pwr_idx_override=$1
echo Restarting wifibroadcast
sudo systemctl start wifibroadcast
echo Finished
```

Get this gs script by running this:

```
curl -L -o setpower.sh https://raw.githubusercontent.com/sickgreg/OpenIPC-Adaptive-Link/refs/heads/main/more/setpower.sh
chmod +x setpower.sh
```

Then run, eg, `sudo ./setpower.sh 30`

**--- Changing the rate at which wfb-ng talks to the gs script ---**

Note: Installer sets this to latest release's default automatically.  If it finds /home/radxa/gs/wfb.sh then it will update it there (taking effect after restart) otherwise it will directly update `/etc/wifibroadcast.cfg`

Default is only 1Hz (1000ms).  200ms gives the script 5 rssi/snr/etc/etc updates per second
```
[common]
log_interval = 200
```

**Some of the information below MAY BE OUTDATED -- it is there for reference**

**--- Config files on drone/camera ---**
**--- txprofiles.conf ---**

Lives on camera: `/etc/txprofiles.conf`
Stores video / mcs settings for all the different modes used in adaptive link

NOTE: Be careful with PWR settings.  These examples are for pushing the BL-8812EU2 (square-blue) fairly hard.  They may blow up other adapters. power settings are the 56 to 61 values towards the end of each line in this example

Example contents of `/etc/txprofiles.conf` for use with BL-8812EU2 (sqauare blue)


```
999 - 999 long 0 12 15 3332 5.0 61 0,0,0,0
1000 - 1150 long 0 12 15 3333 5.0 60 0,0,0,0
1151 - 1300 long 1 12 15 6667 5.0 59 12,6,6,12
1301 - 1700 long 2 12 15 10000 5.0 58 12,6,6,12
1701 - 1950 long 3 12 15 12500 5.0 56 8,4,4,8
1951 - 2001 short 3 12 15 14000 5.0 56 4,0,0,4
```

The values are: `rangestart - rangeend guard_interval FECN FECK Bitrate GOP PWR ROI-QP-definition`


**--- alink.conf ---**

Lives on camera: `/etc/alink.conf`

```

rssi_weight=0.3
snr_weight=0.7
fallback_ms=1000
hold_fallback_mode_s=3
min_between_changes_ms=150
hold_modes_down_s=4
idr_every_change=false
roi_focus_mode=false
request_keyframe_interval_ms=100
hysteresis_percent=15


# Command templates - Don't change these unless you know what you are doing
powerCommand="iw dev wlan0 set txpower fixed %d"
mcsCommand="wfb_tx_cmd 8000 set_radio -B 20 -G %s -S 1 -L 1 -M %d"
bitrateCommand="curl -s 'http://localhost/api/v1/set?video0.bitrate=%d'"
gopCommand="curl -s 'http://localhost/api/v1/set?video0.gopSize=%f'"
fecCommand="wfb_tx_cmd 8000 set_fec -k %d -n %d"
roiCommand="curl -s 'http://localhost/api/v1/set?fpv.roiQp=%s'"
idrCommand="echo 'IDR 0' | nc localhost 4000"
#idrCommand="curl localhost/request/idr"
msposdCommand="echo '%ld s %d M:%d %s F:%d/%d P:%d G:%.1f&L30&F28 CPU:&C &Tc' >/tmp/MSPOSD.msg"
#timeElapsed, profile->setBitrate, profile->setMCS, profile->setGI, profile->setFecK, profile->setFecN, profile->wfbPower, profile->setGop
```


**--- How to WinSCP to your drone via gs over tunnel ---**

```
# On drone set up a route to your LAN (sub 192xxx with yours). This should persist after reboot
ip route add 192.168.8.0/24 via 10.5.0.1

# On GS set up IP forwarding temporarily:
sudo sysctl -w net.ipv4.ip_forward=1
# Make Permenant:
sudo nano /etc/sysctl.conf #uncomment or add this line: 
net.ipv4.ip_forward = 1
sudo sysctl -p
# On GS (Sub 192.x.x.x with your LAN). This seems to not persist after reboot
sudo iptables -t nat -A POSTROUTING -o gs-wfb -s 192.168.8.0/24 -j MASQUERADE

# On Windows as Administrator (Sub 192xxx with your GS IP) - persists after reboot
cmd
route add 10.5.0.0 mask 255.255.255.0 192.168.8.116 -p
```


**More details - Old readme. Still may be useful information**

**--- ALink42p ---**

udp listener and video-link mode changer for OpenIPC


copy to `/usr/bin` on OpenIPC camera and make it executable

`ALink42p --help` for command line options

Copy `txprofiles.conf` to `/etc` (Warning: contains tx power level settings.  Don't set your specific device' power too high)

Copy `alink.conf` to `/etc` for general settings / custom mode-changing execution strings.

Note: It won't start up without those files

I'm running `/usr/bin/ALink42p &` from `/etc/rc.local` startup script. You also need wfb-ng tunnel or run wfb_rx on the camera,

eg

how to MANUALLY START TUNNEL (not necessary now:  enable tunnel in /etc/datalink.conf)
```
# Tunnel pair
wfb_rx -c 127.0.0.1 -u 5800 -K /etc/drone.key -p 160 -i 7669206 wlan0 > /dev/null &
wfb_tx -p 32 -u 5801 -K /etc/drone.key -R 2097152 -B20 -M 0 -S 1 -L 1 -G long -k 1 -n 2 -i 7669206 -f data wlan0 > /dev/null &
wfb_tun -T 0 &
# Although this may work out of the box already if you sysupgrade (?)
# Then
sleep 2
/usr/bin/ALink42
```

Old Way without tunnel, just a simple rx on drone, no tunnel overhead, original way, superceded

`wfb_rx -c 127.0.0.1 -u 5000 -K /etc/drone.key -p 1 -i 7669207 wlan0 &`


**--- adaptive_link_greg3.py ---**

Run this on ground station - Radxa, in  my case

https://github.com/sickgreg/steam-groundstations/blob/master/adaptive-link/adaptive_link_greg3.py (keep an eye on it for newer versions)

`python3 adaptive_link_greg3.py --udp_ip 10.5.0.2 --udp_port 9999`

for no tunnel
`python3 adaptive_link_greg3.py`

You also need wfb-tunnel on ground station... default worked for me

Or the old way (non-tunnel)
```
#/bin/bash

if [ -e /etc/default/wifibroadcast ]; then
  . /etc/default/wifibroadcast
fi

sudo wfb_tx -p 1 -u 9998 -K /etc/gs.key -R 456000 -B20 -M 0 -S 1 -L 1 -G long -k 1 -n 2 -i 7669207 -f data $WFB_NICS
```

**--- setpower.sh ---**

Don't forget to set output power for your card(s) so the drone can hear messages

In my case, i'm using 3 rtl8812au's and have been using this script (which also restarts everything, maybe do this first)

```
#/bin/bash

# usage ./setpower.sh <pwrlvl>
echo Stopping wifibroadcast
sudo systemctl stop wifibroadcast
echo Unloading 88XXau_wfb  module
sudo modprobe -r 88XXau_wfb &&
echo Loading 88XXau_wfb module with power level $1
sudo modprobe 88XXau_wfb rtw_tx_pwr_idx_override=$1
echo Restarting openipc.service
sudo systemctl restart openipc.service
echo Starting wifibroadcast
sudo systemctl start wifibroadcast
```

Usage
`./setpower.sh 40`

**--- spreadfwd.py ----**
Runs on ground station

Listens on a single port and forwards messages to the same number of ports as there are adapters found in /etc/default/wifibroadcast in a round-robin fashion (taking it in turns to send a message)

Example

Run

`python3 spreadfwd.py 9998 9999`

 9998 is the UDP port you set in adaptive_link_greg3.py script
 
  If in `/etc/default/wifibroadcast` file we have `WFB_NICS="rtl1 rtl2 rtl3"`, the program will send to port 9999,10000,10001, so set --first_port_out to the same port you start wfb_tx on (eg 9999)
  


