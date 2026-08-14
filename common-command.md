set gpio
```
gpioset -c gpiochip0 26=0
```

reload systemd unit/service
```
sudo systemctl daemon-reload
```