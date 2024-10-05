# OpenIPC-Adaptive-Link
Greg's Adaptive-Link - Files for OpenIPC camera and Radxa Zero 3w/e ground station


--- spreadfwd.py ----
Runs on ground station
Listens on a single port and forwards messages to all adapters found in /etc/default/wifibroadcast in a round-robin fashion (taking it in turns to send a message)
