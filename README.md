# A blank Mongoose OS app
# Enable aws shadow
mos config-set aws.shadow.enable=true

# Set wifi configuration
mos config-set wifi.sta.enable=true wifi.ap.enable=false \
               wifi.sta.ssid=xxxxx wifi.sta.pass=xxxxxxxxx

# Setup device to connect to AWS IoT
mos aws-iot-setup --aws-iot-policy=mos-default

# Attach console
mos console


## Overview

This is an empty app, serves as a skeleton for building Mongoose OS
apps from scratch.

## How to install this app

- Install and start [mos tool](https://mongoose-os.com/software.html)
- Switch to the Project page, find and import this app, build and flash it:

<p align="center">
  <img src="https://mongoose-os.com/images/app1.gif" width="75%">
</p>