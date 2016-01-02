slowrx
======

Slowrx is an SSTV decoder for Unix-like systems.

Created by Oona Räisänen (OH2EIQ [at] sral.fi).

http://windytan.github.io/slowrx/

Features
--------

* Support for a multitude of modes (see `src/modespec.cc` for a list)
* Detect frequency-shifted signals – no need to fine-tune the radio
* Automatic slant correction, also manual adjustments are simple
* Adaptive noise reduction
* Decode MMSSTV-compatible FSKID
* Decode from audio device, file, or stdin
* Save received pictures as PNG
* Written in C++/C

Requirements
------------

* Linux, OSX, ...
* PortAudio
* libsndfile
* gtkmm 3 (`libgtkmm-3.0-dev`)
* FFTW 3 (`libfftw3-dev`)

And, obviously:

* shortwave radio with SSB
* computer with sound card
* means of getting sound from radio to sound card

Compiling
---------

    ./configure
    make

Running
-------

`./src/slowrx`
