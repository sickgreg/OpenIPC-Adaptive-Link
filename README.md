# OpenIPC-Adaptive-Link
Greg's Adaptive-Link - Files for OpenIPC camera and Radxa Zero 3w/e ground station


--- spreadfwd.py ----
Runs on ground station

Listens on a single port and forwards messages to the same number of ports as there are adapters found in /etc/default/wifibroadcast in a round-robin fashion (taking it in turns to send a message)

Example

Run

python3 spreadfwd.py 9000 9999

 in port 9000 is the UDP port set up in adaptive_link.py script
 
  If in /etc/default/wifibroadcast file we have WFB_NICS="greg1 greg2 greg3", the program will send to port 9999,10000,10001, so set --first_port_out to the same port you start wfb_tx on (9999)
  

--- txprofiles.conf ---

Lives on camera: /etc/txprofiles.conf
Stores video / netowrk settings for all the different modes used in adaptive link

NOTE: Be careful with PWR settings.  These examples are for pushing the BL-8812EU2 (square-blue) fairly hard.

Example contents of /etc/txprofiles.conf


999 - 999 long 0 12 15 3332 5.0 61 0,0,0,0

1000 - 1150 long 0 12 15 3333 5.0 60 0,0,0,0

1151 - 1300 long 1 12 15 6667 5.0 59 0,0,0,0

1301 - 1700 long 2 12 15 10000 5.0 58 0,0,0,0

1701 - 1950 long 3 12 15 12500 5.0 56 0,0,0,0

1951 - 2001 short 3 12 15 14000 5.0 56 0,0,0,0

The values are: rangestart - rangeend guard_interval FECN FECK Bitrate GOP PWR ROI-QP-definition 
