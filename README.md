# fad3r

v0.2 / 2025-05-14

Improved preset storage in EEPROM. Faders can now have indiviual MIDI channels.  

Load preset:  
``` F0 7D 40 <preset> F7```

Store current CC/Channel values to preset:  
```F0 7D 41 <preset> F7```

Set CCs and Channels for preset (remember dec to hex!):  
```F0 7D 42 <preset> <cc1> <ch1> <cc2> <ch2> <cc3> <ch3> F7```

Dump preset for debugging:  
``` F0 7D 43 <preset> F7```

-> Sends: ```F0 7D 43 <preset> <cc1> <cc2> <cc3> <ch1> <ch2> <ch3> F7```

=================
v0.1 / 2020-02-12

Firmware for Teensy LC (legacy) controlling three faders via USB Midi  

https://www.pjrc.com/store/teensylc.html  
https://www.pjrc.com/teensy/td_midi.html  

``` F0 7D <cc1> <cc2> <cc3> F7```
remember to convert from dec to hex!

