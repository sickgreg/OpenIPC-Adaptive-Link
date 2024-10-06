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
echo Restarting wifibroadcast
sudo systemctl start wifibroadcast
echo Finished
