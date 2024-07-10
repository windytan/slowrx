slowrx
======

Slowrx is an SSTV decoder for Linux.

Created by Oona Räisänen (OH2EIQ [at] sral.fi).

http://windytan.github.io/slowrx/

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
* Alsa (`libasound2-dev`)
* FFTW 3 (`libfftw3-dev`)

For the GUI:
* Gtk+ 3.4 (`libgtk-3-dev`)

For the daemon:
* libgd

And, obviously:

* shortwave radio with SSB
* computer with sound card
* means of getting sound from radio to sound card

Compiling
---------

`make`

Running
-------

`./slowrx`
