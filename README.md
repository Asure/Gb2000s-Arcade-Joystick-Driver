# mk_arcade_joystick_rpi #

Based on The Raspberry Pi GPIO Joystick Driver

Modified by Vanni Bruto

## Introduction ##

Looks like a quick & dirty GPIO banger for Pandora's Box 6 "Aus" 3H / 3A / Whatever it's an Orangi pi clone. :)
I didn't make this but it's here at github for safekeeping.

## The Software ##
The joystick driver is based on the gamecon_gpio_rpi driver by [marqs](https://github.com/marqs85)

It is written for 4 directions joysticks and 8 buttons per player. 

It can read one joystick + buttons wired on RPi GPIOs (two sticks and 13(?) buttons for the Pandora's box revision).

It uses internal pull-ups of the H3 GPIO ports, so all switches must be directly connected to its corresponding GPIO and to the ground.


### Installation ###

Tested to compile on 3.4.113 default Sunxi kernel.

### Installation on Retr0rangePi 4.2 lite (3.4.113 kernel) ###

Get a USB-lan adapter. ADMTek pegasus II confirmed to work. Plug into internal port and connect to your LAN.
Get a USB keyboard. Plug it into the 'outside left' port. Closest to the short side.
Boot up Retr0rangePi 4.2, press F4 to drop to shell. 
Use 'ifconfig' to get the ip of the device.

Download the installation script : 
```shell
sudo apt install linux-headers-$(uname-r)
sudo make
sudo insmod mk_arcade_joystick_rpi.ko
```
Check with dmesg to see the driver is loaded.

If this doesn't work, compile, or complains about unable to download headers, use dhclient eth2 or whatever the USB lan adapter is. It will set the default route for you :)
(Add the dhclient command to /etc/rc.local to avoid this in the future.)

### Auto load at startup ###

Copy the compiled kernel module to a safe place, and load it from rc.local for example:
```shell
insmod /home/pi/mk_arcade_joystick_rpi.ko
```
   
### Testing ###

Use the following command to test joysticks inputs :
```shell
jstest /dev/input/js0
```
(Might need "sudo apt install joystick" if the jstest command is missing.)

## More Joysticks case : MCP23017 ##

The MCP23017 is not supported by this version of the driver, and it's very unlikely i'll port it over. Sorry.

## Known Bugs ##
- Coin button not working at the moment.
- Doesn't compile on current mainline. (todo!)

Credits
-------------
-  [gamecon_gpio_rpi](https://github.com/petrockblog/RetroPie-Setup/wiki/gamecon_gpio_rpi) by [marqs](https://github.com/marqs85)
-  [RetroPie-Setup](https://github.com/petrockblog/RetroPie-Setup) by [petRockBlog](http://blog.petrockblock.com/)
-  [Low Level Programming of the Raspberry Pi in C](http://www.pieter-jan.com/node/15) by [Pieter-Jan](http://www.pieter-jan.com/)
- Vanni Bruto 
- [RedboX](https://github.com/sebastian404) for the initial Japb Jamma driver that never got a public release.
