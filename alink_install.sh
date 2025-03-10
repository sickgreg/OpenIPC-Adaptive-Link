#!/bin/sh

# Color-coded echo functions
echo_red()   { printf "\033[1;31m$*\033[m\n"; }
echo_green() { printf "\033[1;32m$*\033[m\n"; }
echo_blue()  { printf "\033[1;34m$*\033[m\n"; }

# Default configurations
UDP_IP=10.5.0.10
UDP_PORT=9999
LOG_INTERVAL=100
WFBGS_CFG=/etc/wifibroadcast.cfg
WFBGS_CFG2=/home/radxa/gs/wfb.sh
WFBGS_CFG3=/gs/wfb.sh


# Variables for repository
REPO_OWNER="OpenIPC"
REPO_NAME="adaptive-link"

# Function to fetch the latest release asset URL
github_asset_url() {
    FILE_NAME=$1
    curl -s "https://api.github.com/repos/$REPO_OWNER/$REPO_NAME/releases" | \
        grep -o '"browser_download_url": "[^"]*' | \
        grep "$FILE_NAME" | head -n 1 | sed 's/"browser_download_url": "//'
}


# Ensure the script is run as root
if [ $(id -u) -ne 0 ]; then
	echo_red "Permission denied. Use sudo to run this script."
	echo "Usage: sudo $0 <system> <action>"
	exit 1
fi

# Detect system type
gs_check() {
	# Check for absence of Buildroot to identify a Ground Station
	grep -o "NAME=Buildroot" /etc/os-release > /dev/null 2>&1 || return 0
	return 1
}


dr_check() {
	# Ensure Buildroot is present, indicating a Drone setup
	gs_check || return 0
	return 1
}


# Function to update wifibroadcast configuration
update_log_interval() {
	if grep -q "log_interval" ${WFBGS_CFG}; then
		sed -i 's/log_interval.*/log_interval = '${LOG_INTERVAL}'/' $WFBGS_CFG
	else
		sed -i '/\[common\]/a log_interval = '${LOG_INTERVAL}'' $WFBGS_CFG
	fi
	
	if [ -f "$WFBGS_CFG2" ]; then
		# If the file exists, update the log_interval here too (CC's GS)
		if grep -q "log_interval" "$WFBGS_CFG2"; then
			sed -i 's/log_interval.*/log_interval = '${LOG_INTERVAL}'/' "$WFBGS_CFG2"
		else
			sed -i '/\[common\]/a log_interval = '${LOG_INTERVAL}'' "$WFBGS_CFG2"
		fi
	fi
	
	if [ -f "$WFBGS_CFG3" ]; then
		# If the file exists, update the log_interval here too (CC's GS)
		if grep -q "log_interval" "$WFBGS_CFG3"; then
			sed -i 's/log_interval.*/log_interval = '${LOG_INTERVAL}'/' "$WFBGS_CFG3"
		else
			sed -i '/\[common\]/a log_interval = '${LOG_INTERVAL}'' "$WFBGS_CFG3"
		fi
	fi
	
}

# Ground Station setup
gs_setup() {
	FILE_NAME=alink_gs
	FILE=/usr/local/bin/$FILE_NAME
# Check if /config directory exists
if [ -d "/config" ]; then
  # If /config exists, use it
  FILE_CONF="/config/$FILE_NAME.conf"
# Check if /home/radxa directory exists
elif [ -d "/home/radxa" ]; then
  # If /home/radxa exists, use it
  FILE_CONF="/home/radxa/$FILE_NAME.conf"
else
  # If neither /config nor /home/radxa exist, use /etc
  FILE_CONF="/etc/$FILE_NAME.conf"
fi
	PATH_SERVICE=/etc/systemd/system/$FILE_NAME.service

	case "$1" in
		install)
			echo_blue "Installing Adaptive Link for Ground Station..."
			if [ -f $FILE ]; then
				echo_red "$FILE_NAME already installed. Use 'remove' first."
				exit 1
			fi
			URL_ALINK_GS=$(github_asset_url "alink_gs")
			wget -O $FILE "$URL_ALINK_GS" && chmod +x $FILE
			
			# Create service file
			cat <<EOF | tee $PATH_SERVICE
[Unit]
Description=OpenIPC Adaptive_Link

[Service]
ExecStart=${FILE} --config ${FILE_CONF}
Type=idle
RemainAfterExit=true

[Install]
WantedBy=multi-user.target
EOF

			# Start service for testing
			echo "Starting Adaptive Link temporarily for testing..."
			update_log_interval
			
			$FILE --config $FILE_CONF &
			sleep 5 && kill $!

			# Configure
			if [ ! -f $FILE_CONF ]; then
				echo_red "Error: Missing configuration file ${FILE_CONF}."
				exit 1
			fi
			sed -i 's/udp_ip.*/udp_ip = '$UDP_IP'/' $FILE_CONF
			sed -i 's/udp_port.*/udp_port = '$UDP_PORT'/' $FILE_CONF
			update_log_interval

			# Restart services
			 systemctl restart wifibroadcast.service
			 systemctl enable $FILE_NAME.service
			 systemctl start $FILE_NAME.service
			echo_green "Adaptive Link installed successfully."
			;;
		remove)
	
# Check and stop the new Ground Station service if its .service file exists
if [ -f "$PATH_SERVICE" ]; then
    sudo systemctl stop $FILE_NAME.service
    sudo systemctl disable $FILE_NAME.service
    sudo rm -f $FILE $FILE_CONF $PATH_SERVICE
    echo_green "Adaptive Link ($FILE_NAME) removed."
else
    echo_blue "Service file $PATH_SERVICE not found, skipping..."
fi

# Check and stop the old Adaptive Link service if its .service file exists
if [ -f "/etc/systemd/system/adaptive_link.service" ]; then
    sudo systemctl stop adaptive_link.service
    sudo systemctl disable adaptive_link.service
    sudo rm -f /usr/bin/adaptive_link /etc/adaptive_link.conf /etc/systemd/system/adaptive_link.service
    echo_green "Old Adaptive Link (adaptive_link) removed."
else
    echo_blue "Service file /etc/systemd/system/adaptive_link.service not found, skipping..."
fi
;;
		update)
			echo_blue "Updating Adaptive Link for Ground Station..."
			URL_ALINK_GS=$(github_asset_url "alink_gs")
			wget -O $FILE "$URL_ALINK_GS" && chmod +x $FILE
			update_log_interval
			 systemctl restart wifibroadcast.service
			 systemctl restart $FILE_NAME.service
			echo_green "Adaptive Link updated successfully."
			;;
		*)
			echo_red "Invalid action for Ground Station. Use install, remove, or update."
			;;
	esac
}

# Drone setup
dr_setup() {
	FILE_NAME=alink_drone
	FILE=/usr/bin/$FILE_NAME
	TXPROFILE=/etc/txprofiles.conf
	ALINK=/etc/alink.conf
	
	case "$1" in
		install)
			echo_blue "Installing Adaptive Link for Drone..."
			if [ -f $FILE ]; then
				echo_red "$FILE_NAME already installed. Use 'remove' first."
				exit 1
			fi
			URL_ALINK_DRONE=$(github_asset_url "alink_drone")
			URL_TXPROFILE_CONF=$(github_asset_url "txprofiles.conf")
			URL_ALINK_CONF=$(github_asset_url "alink.conf")

			curl -L -o $FILE "$URL_ALINK_DRONE"
			curl -L -o $TXPROFILE "$URL_TXPROFILE_CONF"
			curl -L -o $ALINK "$URL_ALINK_CONF"
			chmod +x $FILE

			# Set qpDelta
			cli -s .video0.qpDelta -12

   			# Set fpv noiseLevel
			cli -s .fpv.enabled true
   			cli -s .fpv.noiseLevel 0

			# Add to rc.local
			sed -i -e '$i \'$FILE' --ip '$UDP_IP' --port '$UDP_PORT' &' /etc/rc.local
			sed -i 's/tunnel=.*/tunnel=true/' /etc/datalink.conf
			echo_green "Adaptive Link installed successfully. Restart the drone."
			;;
		remove)
			echo_blue "Removing Adaptive Link for Drone..."
			killall $FILE_NAME 2>/dev/null
			sed -i '/.*'$FILE_NAME'.*/d' /etc/rc.local
			rm -f $FILE $TXPROFILE $ALINK
   		 	
       			cli -d .video0.qpDelta
	  
			echo_green "Adaptive Link removed."
			;;
		update)
			echo_blue "Updating Adaptive Link for Drone..."
			URL_ALINK_DRONE=$(github_asset_url "alink_drone")
			URL_TXPROFILE_CONF=$(github_asset_url "txprofiles.conf")
			URL_ALINK_CONF=$(github_asset_url "alink.conf")

			curl -L -o $FILE "$URL_ALINK_DRONE"
			curl -L -o $TXPROFILE "$URL_TXPROFILE_CONF"
			curl -L -o $ALINK "$URL_ALINK_CONF"
			# Set qpDelta
			cli -s .video0.qpDelta -12

			echo_green "Adaptive Link updated successfully. Restart the drone."
			;;
		*)
			echo_red "Invalid action for Drone. Use install, remove, or update."
			;;
	esac
}

# Main script execution
case "$1" in
	gs)
		gs_check || { echo_red "Error: Not a valid Ground Station environment."; exit 1; }
		gs_setup "$2";;
	drone)
		dr_check || { echo_red "Error: Not a valid Drone environment."; exit 1; }
		dr_setup "$2";;
	*)
		echo_red "Usage: $0 gs|drone install|remove|update"
		;;
esac

exit 0
