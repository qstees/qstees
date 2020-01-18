. /home/$(whoami)/email.sh
if [ $eENABLED = "1" ]; then
  sendemail -f $FROM -t $TO -u "VPS email" -m "Your email is working" -s $SMTP -xu $USERNAME -xp $PASSWORD -o tls=yes
else
  echo "Can't send email as it is not enabled. Please set eENABLED=1 in email.sh"
fi
