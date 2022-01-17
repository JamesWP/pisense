# pisense
prometheus compatible bme230 i2c sensor reporter



# setup

$ python3.7 -m pip install --system -r requirements.txt
$ sudo python3.7 setup.py install
$ sudo cp pisense.service /etc/systemd/system/pisense.service
$ sudo cp pisense.timer /etc/systemd/system/pisense.timer
$ sudo systemctl daemon-reload 
$ sudo systemctl enable pisense.service 
$ sudo systemctl enable pisense.timer
