#!/bin/bash

echo "Start blinking LED..."
gpioset -c gpiochip0 --toggle 1s 26=1