# OpenIPC-Adaptive-Link
Greg's Adaptive-Link - Files for OpenIPC camera and Radxa Zero 3w/e ground station

ALink42c


udp listener and video-link profile changer/manager for OpenIPC


copy to `/usr/bin` on OpenIPC camera and make it executable

`ALink42c --help` for command line options

Copy `txprofiles.conf` to `/etc` (don't have power set too high for your card in here)
(future) copy ALink.conf to `/etc` for general settings / custom mode-changing execution strings
I'm running /usr/bin/ALink42c & from /etc/rc.local startup script.  You also need to run wfb_rx on the camera,

eg


`wfb_rx -c 127.0.0.1 -u 5000 -K /etc/drone.key -p 1 -i 7669207 wlan0 &`


--- adaptive_link_greg3.py ---

Run this on ground station - Radxa, in  my case

https://github.com/sickgreg/steam-groundstations/blob/master/adaptive-link/adaptive_link_greg3.py

`python3 adaptive_link_greg3.py`

You also need to be running wfb_tx on the ground station
`#/bin/bash

if [ -e /etc/default/wifibroadcast ]; then
  . /etc/default/wifibroadcast
fi

sudo wfb_tx -p 1 -u 9999 -K /etc/gs.key -R 456000 -B20 -M 0 -S 1 -L 1 -G long -k 1 -n 2 -i 7669207 -f data $WFB_NICS
`

--- spreadfwd.py ----
Runs on ground station

Listens on a single port and forwards messages to the same number of ports as there are adapters found in /etc/default/wifibroadcast in a round-robin fashion (taking it in turns to send a message)

Example

Run

`python3 spreadfwd.py 9998 9999`

 9998 is the UDP port you set in adaptive_link_greg3.py script
 
  If in `/etc/default/wifibroadcast` file we have `WFB_NICS="rtl1 rtl2 rtl3"`, the program will send to port 9999,10000,10001, so set --first_port_out to the same port you start wfb_tx on (eg 9999)
  

--- txprofiles.conf ---

Lives on camera: `/etc/txprofiles.conf`
Stores video / netowrk settings for all the different modes used in adaptive link

NOTE: Be careful with PWR settings.  These examples are for pushing the BL-8812EU2 (square-blue) fairly hard.

Example contents of `/etc/txprofiles.conf`


```999 - 999 long 0 12 15 3332 5.0 61 0,0,0,0
1000 - 1150 long 0 12 15 3333 5.0 60 0,0,0,0
1151 - 1300 long 1 12 15 6667 5.0 59 0,0,0,0
1301 - 1700 long 2 12 15 10000 5.0 58 0,0,0,0
1701 - 1950 long 3 12 15 12500 5.0 56 0,0,0,0
1951 - 2001 short 3 12 15 14000 5.0 56 0,0,0,0```

The values are: rangestart - rangeend guard_interval FECN FECK Bitrate GOP PWR ROI-QP-definition 
