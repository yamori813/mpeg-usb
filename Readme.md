This is NTSC caputure program use CONEXNT usb module on Mac OS X.

We need firmware in windows driver. This is extract.

% ./ezfwcut.rb p2usbwdm.sys

% ./cxfwcut.rb p2usbwdm.sys

This program use libusb.dylib. Build libusb and copy dylib in this dirctory.

% install_name_tool -id @executable_path/libusb.dylib libusb.dylib

Do EZ-USB firmware download by Ezloader

https://github.com/yamori813/Ezloader_OSX

for GV-MDVD3

% Ezload -v 0x04b4 -p 0x8613 -n -F -r -f ez173280.bin

for  PC-MDVD/U2

% Ezload -v 0x04b4 -p 0x8613 -n -F -r -f ez163280.bin

Caputure.

% ./mpegcapt cx83280.bin test.mpeg

stop to ^c
