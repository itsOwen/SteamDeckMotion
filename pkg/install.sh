#!/bin/sh

cd "$(dirname "$(readlink -f "$0")")"

echo "Stopping the service if it's running..."
systemctl --user -q stop sdmotion.service >/dev/null 2>&1
systemctl --user -q disable sdmotion.service >/dev/null 2>&1 
echo "Copying binary..."
if mkdir -p $HOME/sdmotion >/dev/null; then
	:
else
	echo -e "\e[1mFailed to create a directory.\e[0m"
	exit 24
fi
rm $HOME/sdmotion/sdmotion >/dev/null 2>&1
if cp sdmotion $HOME/sdmotion/ >/dev/null; then
	:
else
	echo -e "\e[1mFailed to copy binary file.\e[0m"
	exit 25
fi
if chmod +x $HOME/sdmotion/sdmotion >/dev/null; then
	echo "Binary copied."
else
	echo -e "\e[1mFailed to set binary as executable.\e[0m"
	exit 26
fi

rm $HOME/sdmotion/update.sh >/dev/null 2>&1
cp update.sh $HOME/sdmotion/ >/dev/null
if chmod +x $HOME/sdmotion/update.sh >/dev/null; then
	echo "Update script copied."
fi

rm $HOME/sdmotion/uninstall.sh >/dev/null 2>&1
cp uninstall.sh $HOME/sdmotion/ >/dev/null
if chmod +x $HOME/sdmotion/uninstall.sh >/dev/null; then
	echo "Uninstall script copied."
fi

rm $HOME/sdmotion/logcurrentrun.sh >/dev/null 2>&1
cp logcurrentrun.sh $HOME/sdmotion/ >/dev/null
if chmod +x $HOME/sdmotion/logcurrentrun.sh >/dev/null; then
	echo "Log script copied."
fi

echo "Installing service..."
rm $HOME/.config/systemd/user/sdmotion.service >/dev/null 2>&1 
if cp sdmotion.service $HOME/.config/systemd/user/; then
	:
else
	echo -e "\e[1mFailed to copy service file into user systemd location.\e[0m"
	exit 27
fi

if systemctl --user -q enable --now sdmotion.service >/dev/null; then
	echo "Installation done."
	echo ""
	echo "Motion service is now running and broadcasting JSON motion data on UDP port 27760"
	echo "Service will auto-start on boot."
	echo ""
	echo "To check status: systemctl --user status sdmotion"
	echo "To view logs: journalctl --user -f -u sdmotion"
	echo "To uninstall: run uninstall-sdmotion.desktop from Desktop"
else
	echo -e "\e[1mFailed enabling the service.\e[0m"
	exit 28
fi

echo "Setting up desktop shortcuts..."
cp uninstall-sdmotion.desktop $HOME/Desktop/ >/dev/null
if chmod +x $HOME/Desktop/uninstall-sdmotion.desktop >/dev/null; then
	echo "Uninstall shortcut copied."
fi