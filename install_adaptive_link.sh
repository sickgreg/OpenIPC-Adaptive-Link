#!/bin/sh

UDP_IP=10.5.0.10
UDP_PORT=9999

# drone or gs
isSystem=$(grep -o "NAME=Buildroot" /etc/os-release)


if [ "$1" = "gs" ]; then
	if [ ! -z $isSystem ]; then
		echo "Error: It doesn't look like it's an SBC"
		exit 1
	fi

	FILE_NAME=adaptive_link
	FILE=/usr/bin/$FILE_NAME
	FILE_CONF=/etc/$FILE_NAME.conf
	PATH_SERVICE=/etc/systemd/system/$FILE_NAME.service

	if [ "$2" = "install" ]; then
		echo "Installing Adaptive Link"
		
		if [ -f $FILE ];then
			echo "$FILE_NAME is already installed"
			exit 1
		fi

		
        sudo apt install -y dos2unix
        sudo wget https://raw.githubusercontent.com/sickgreg/steam-groundstations/refs/heads/master/adaptive-link/adaptive_link_greg3.py -O $FILE
        sudo chmod +x $FILE
		sudo dos2unix $FILE
		
        cat <<EOF | sudo tee $PATH_SERVICE
[Unit]
Description=OpenIPC_AdaptiveLink

[Service]
ExecStart=${FILE} --config ${FILE_CONF}
Type=idle
RemainAfterExit=true

[Install]
WantedBy=multi-user.target
EOF
		echo ""
		echo "Run $FILE --config $FILE_CONF &"
		sudo $FILE --config $FILE_CONF & sleep 5
		echo "kill pid $!"
		sudo kill $!
		
		if [ ! -f $FILE_CONF ]; then	
			echo "Error: File ${FILE_CONF} not found" 
			exit 0
		fi
		
		sudo sed -i 's/udp_ip.*/udp_ip = '$UDP_IP'/' $FILE_CONF
		sudo sed -i 's/udp_port.*/udp_port = '$UDP_PORT'/' $FILE_CONF

		sudo systemctl enable $FILE_NAME.service
		sudo systemctl start $FILE_NAME.service
		sudo systemctl status $FILE_NAME.service
		
		echo "Configuration file ${FILE_CONF}"
		
	elif [ "$2" = "remove" ]; then
		echo "Removing  Adaptive Link"
		
		sudo systemctl stop $FILE_NAME.service && echo "Wait..." && sleep 3
		sudo systemctl status $FILE_NAME.service
		sudo systemctl disable $FILE_NAME.service
		
		sudo rm -f $FILE_CONF $FILE $PATH_SERVICE
	
	elif [ "$2" = "update" ]; then	
		echo "Updating Adaptive Link"
		
		sudo systemctl stop $FILE_NAME.service && echo "Wait..." && sleep 3
		sudo systemctl status $FILE_NAME.service
		
		sudo wget https://raw.githubusercontent.com/sickgreg/steam-groundstations/refs/heads/master/adaptive-link/adaptive_link_greg3.py -O $FILE
		sudo dos2unix $FILE
		
		sudo systemctl start $FILE_NAME.service 
		sudo systemctl status $FILE_NAME.service
		
		echo "The update is complete"
	
	fi

elif [ "$1" = "drone" ]; then
	if [ -z $isSystem ]; then
		echo "Error: It doesn't look like it's an Drone"
		exit 1
	fi

	
	TXPROFILE=/etc/txprofiles.conf
	ALINK=/etc/alink.conf
	FILE_NAME=ALink42p
	FILE=/usr/bin/$FILE_NAME
	
	if [ "$2" = "install" ]; then
		echo "Installing Adaptive Link"
		
		if [ -f $FILE ];then
			echo "$FILE_NAME is already installed"
			exit 1
		fi
		
		curl -L -o $FILE https://github.com/sickgreg/OpenIPC-Adaptive-Link/raw/refs/heads/main/ALink42p
		curl -L -o $TXPROFILE https://raw.githubusercontent.com/sickgreg/OpenIPC-Adaptive-Link/refs/heads/main/txprofiles.conf
		curl -L -o $ALINK https://github.com/sickgreg/OpenIPC-Adaptive-Link/raw/refs/heads/main/alink.conf

		chmod +x $FILE
		
		isFile=$(grep -o "$FILE" /etc/rc.local)
		if [ -z $isFile ]; then
			sed -i -e '$i \'$FILE' --ip '$UDP_IP' --port '$UDP_PORT' &' /etc/rc.local
		fi
		
		# Outputs garbage to the console
		#echo "Starting $FILE"
		#$FILE --ip $UDP_IP --port $UDP_PORT < /dev/null &
		
		echo "Installation completed. Restart the system"
			
	elif [ "$2" = "remove" ]; then
		echo "Removing Adaptive Link"
		
		echo "killall $FILE_NAME"
		killall $FILE_NAME && echo "Wait..." && sleep 1
		
		echo "Remove from /etc/rc.local"
		sed -i '/.*'$FILE_NAME'.*/d' /etc/rc.local
		
		
		echo "Remove $FILE $TXPROFILE $ALINK"
		rm -f $FILE $TXPROFILE $ALINK
	
	elif [ "$2" = "update" ]; then	
		echo "Updating Adaptive Link"
		
		echo "killall $FILE_NAME"
		killall $FILE_NAME && echo "Wait..." && sleep 1
		
		curl -L -o $FILE https://github.com/sickgreg/OpenIPC-Adaptive-Link/raw/refs/heads/main/ALink42p
		curl -L -o $TXPROFILE https://raw.githubusercontent.com/sickgreg/OpenIPC-Adaptive-Link/refs/heads/main/txprofiles.conf
		curl -L -o $ALINK https://github.com/sickgreg/OpenIPC-Adaptive-Link/raw/refs/heads/main/alink.conf
		
		echo "The update is complete. Restart the system"
	fi
	
	exit 0
fi


cat <<EOF
Usage: $0 gs install|remove|update
       $0 drone install|remove|update
EOF


exit 0
