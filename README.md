slowrx
======

Slowrx is an SSTV decoder for Linux.

Created by Oona Räisänen / OH2EIQ (windyoona at gmail).

http://windytan.github.com/slowrx/

Features
--------

* Support for a multitude of modes (see `modespec.c` for a list)
* Detect frequency-shifted signals – no need to fine-tune the radio
* Automatic slant correction, also manual adjustments are simple
* Adaptive noise reduction
* Decode digital FSK ID
* Save received pictures as PNG
* Written in C99

Requirements
------------

* Linux
* Alsa
* Gtk+ 3.4
* FFTW 3

And, obviously:

* shortwave radio with SSB
* computer with sound card
* means of getting sound from one to the other

Compiling
---------

`make`

Running
-------

`./slowrx`
