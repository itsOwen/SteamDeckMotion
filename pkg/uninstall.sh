#!/bin/sh

echo "Uninstalling the Steam Deck Motion Service"
systemctl --user -q stop sdmotion.service >/dev/null 2>&1
systemctl --user -q disable sdmotion.service >/dev/null 2>&1
rm $HOME/.config/systemd/user/sdmotion.service >/dev/null 2>&1

echo "Removing files"
rm $HOME/sdmotion/sdmotion >/dev/null 2>&1
rm $HOME/sdmotion/update.sh >/dev/null 2>&1
rm $HOME/sdmotion/uninstall.sh >/dev/null 2>&1
rm $HOME/sdmotion/logcurrentrun.sh >/dev/null 2>&1
rm $HOME/Desktop/uninstall-sdmotion.desktop >/dev/null 2>&1
rm -d $HOME/sdmotion >/dev/null 2>&1

echo "Uninstalling complete."

read -n 1 -s -r -p "Finished. Press any key to exit."
echo " "