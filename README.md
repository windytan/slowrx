slowrx
======

Slowrx is an SSTV decoder for Linux.

Created by Oona Räisänen (OH2EIQ [at] sral.fi).

http://windytan.github.io/slowrx/

Features
--------

* Support for a multitude of modes (see `modespec.cc` for a list)
* Detect frequency-shifted signals – no need to fine-tune the radio
* Automatic slant correction, also manual adjustments are simple
* Adaptive noise reduction
* Decode digital FSK ID
* Save received pictures as PNG
* Written in C++11

Requirements
------------

* Linux or OSX
* gtkmm 3 (`libgtkmm-3.0-dev`)
* FFTW 3 (`libfftw3-dev`)

And, obviously:

* shortwave radio with SSB
* computer with sound card
* means of getting sound from radio to sound card

Compiling
---------

    autoreconf --install
    ./configure
    make

Running
-------

`./src/slowrx`
