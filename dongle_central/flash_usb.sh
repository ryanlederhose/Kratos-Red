#!/bin/bash

west build -p auto

nrfutil pkg generate --hw-version 52 --sd-req=0x00 \
        --application build/zephyr/zephyr.hex \
        --application-version 1 bsu.zip


nrfutil dfu usb-serial -pkg bsu.zip -p /dev/ttyACM0

rm -rf bsu.zip
