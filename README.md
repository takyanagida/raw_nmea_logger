Raw NMEA Logger for Arduino
================================

Reads NMEA data from GPS module over serial, and save them to SD card.

Hardware
----------------------
- [Adafruit Ultimate GPS FeatherWing](https://www.adafruit.com/product/3133)
- [Adafruit Feather M0 Adalogger](https://www.adafruit.com/product/2796)

Usage
----------------------

Just turn power on, and then soon start saving raw NMEA GPS log into micro SD in FAT format.
To stop, just cut the power. SD card is flushed every second so that data loss from power cut is minimum.

LED works as follows.

- Green LED near SD slot: blinks when flushed.
- Red LED near USB connector: blinks every second when battery is low (at 3.55 volts).
- Red LED near GPS receiver: factory default (blinks with 1 sec. interval while searching GPS, 15 sec. interval when position fixed.

Development Notes
----------------------

- SD card flush is called not for every line of log to save battery consumption, but for every second to minimize data loss against empty battery.
- Battery alert threshold is determined as decribed in [my Adafruit forum post](https://forums.adafruit.com/viewtopic.php?f=57&t=125276).


License
----------------------

Copyright (c) 2017 by Tak Yanagida (OnTarget Inc.)

This software is released under the MIT License.
