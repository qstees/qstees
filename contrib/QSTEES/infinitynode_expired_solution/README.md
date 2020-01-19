# Infinity node expired solution
This guide will help you installing the script `infinitynode_expired_solution.sh`<br>
<br>
The script will automatically restart your infinity node in case it stops or the blockchain files get corrupted.<br>
Telegram and email notification options now included to get real time notifications.<br>

### Manual installation
The file will be downloaded and overwritten, if any.<br>
**Note:** All crontab entries will be deleted and recreated.
<br><br>
Establish the VPS connection with the new user you created during the infinity node initial setup.
<br><br>
Please carefully copy and paste the following commands into the terminal screen and press enter.

```
bash
crontab -r
rm /home/$(whoami)/.qstees/script.running
sudo rm infinitynode_expired_solution.sh
wget -O ~/infinitynode_expired_solution.sh https://raw.githubusercontent.com/qstees/QSTEES-core/master/contrib/QSTEES/infinitynode_expired_solution/Infinitynode_expired_solution.sh
wget -O ~/Telegram.sh https://raw.githubusercontent.com/qstees/QSTEES-core/master/contrib/QSTEES/infinitynode_expired_solution/Telegram.sh
wget -O ~/email.sh https://raw.githubusercontent.com/qstees/QSTEES-core/master/contrib/QSTEES/infinitynode_expired_solution/email.sh
chmod +x infinitynode_expired_solution.sh email.sh Telegram.sh
sudo apt-get update
sudo apt-get install sendemail libio-socket-ssl-perl libnet-ssleay-perl -y
(if asked about restart services, choose YES)
echo "*/5 * * * * /home/$(whoami)/infinitynode_expired_solution.sh" | crontab
```

## To configure email
```
sudo nano email.sh
```
replace `<FROM EMAIL>`, `<TO EMAIL>`, smtp if you don't want to use google, `<USERNAME>`, `<PASSWORD>` with your setting. Then change `eENABLED=1`

### Testing your email configuration
If you would like to test your email configuration use the following commands.
```
wget -O test_email.sh https://raw.githubusercontent.com/qstees/QSTEES-core/master/contrib/QSTEES/infinitynode_expired_solution/test_email.sh
chmod +x test_email.sh
./test_email.sh
```

## To configure telegram
```
sudo nano Telegram.sh
```
Please setup telegram bot with BotFather on telegram to use with script.
Replace `<TOKEN>` and `<chatid>` with telegram value for notification to work. Once configured, please change `tENABLED=1`

### Testing your telegram configuration
If you would like to test your telegram configuration use the following commands.
```
wget -O test_telegram.sh https://raw.githubusercontent.com/qstees/QSTEES-core/master/contrib/QSTEES/infinitynode_expired_solution/test_telegram.sh
chmod +x test_telegram.sh
./test_telegram.sh
```

## Log
You may want to follow the script log while updating (every 5 minutes) `Cltr and C` to exit:
```
tail -f ~/.qstees/qstees_control.log
```
or read the entire log<br>`PGDOWN`/`PGUP` keys to scroll, `Q` to exit:
```
less ~/.qstees/qstees_control.log
```
or use nano read entire log<br>`PGDOWN`/`PGUP` keys to scroll, `Cltr and X` to exit, `Cltr and _ then Ctrl and V` to go to bottom
``` 
nano ~/.qstees/qstees_control.log
```
to clear the log
```
> ~/.qstees/qstees_control.log
```
