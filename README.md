# ACRModem

The 88-ACR Audio Cassette Interface provided a means for Altair 8800 users
to save and load data on audio cassettes.

The 88-ACR consisted of two parts: A serial interface (identical to 88-SIO)
and a modulator/demodulator to convert the serial bit stream to an audio
signal (and vice versa).

This project represents the modulator/demodulator part, to be used with the
[Altair 8800 Simulator](https://www.hackster.io/david-hansel/arduino-altair-8800-simulator-3594a6). The function is fairly simple:
1) Connect the serial input of the modulator/demodulator board
to a serial interface of the Altair Simulator.
2) Connect the audio input of the board to the headphone output of an audio 
cassette recorder and the audio output of the board to the microphone
input of the cassette recorder.
3) Configure the serial interface on the Simulator for 300 baud.
4) In the Simulator configuration menu, map the 88-ACR card to the
serial interface to which the modulator/demodulator is connected.

Any data sent to the 88-ACR card by software running in the Simulator
should now appear on the microphone input of the cassette recorder and
data played back on the recorder should be visible on the 88-ACR card.

You will likely need to adjust recording level and volume on the cassette
recorder to find levels that work well.

## Hardware

The modulator/demodulator hardware is based on an ATMega 328p MCU
and a LM358 OpAmp. The ACRModem.fzz Fritzing file contains the schematic
as well as a working stripboard layout.

## Software

The ACRModem.ino file can be uploaded to the ATMega using the Arduino
development GUI. There are plenty of descriptions on how to upload
Arduino sketches to a stand-alone ATMega available on the internet.
Search for "Breadboard Arduino" or use [this link](http://www.buildcircuit.com/make-arduino-on-breadboard-using-ftdi-breakout-board).

## Acknowledgements

This project is loosely based on Mike Douglas' implementation
of an [Audio Cassette Interface](http://altairclone.com/cassette.htm) 
for the [Altair 8800 clone](http://altairclone.com/).

Thanks to Mike for tracking down the original 88-ACR documentation,
collecting supporting files and making the plans for his interface [available to the public](http://altairclone.com/downloads/cassette_interface/Hardware).

Although the implementation is different, the function of the two should
be almost identical. Instead of building this project here, purchasing
one of Mike's interfaces and hooking it up to the Altair Simulator
should work just the same.

For much more information on how to use the cassette interface on the
Altair 8800, see Mike's [documentation](http://altairclone.com/downloads/manuals/Cassette%20Interface%20Manual.pdf) 
for his cassette interface.
