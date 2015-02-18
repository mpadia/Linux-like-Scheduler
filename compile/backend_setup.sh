
sudo sh -c "echo 127.0.0.1 `cs-status | head -1 | sed 's/://g'` >> /etc/hosts"

sudo /bin/serial_server &
