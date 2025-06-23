#!/bin/sh

journalctl --user -u sdmotion -b | grep "\[$(systemctl --user status sdmotion | grep 'Main PID' | sed 's/.*PID: \([0-9]\+\).*/\1/')\]" --color=never