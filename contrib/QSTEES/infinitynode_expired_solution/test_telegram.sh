#!/bin/bash

. /home/$(whoami)/Telegram.sh
if [ $tENABLED = "1" ]; then
  curl -s -X POST $URL -d chat_id=$CHAT_ID -d text="Test message."
else
  echo "Telegram is not enabled. Please set tENABLED=1 in Telegram.sh"
fi
