slowrx
======

Slowrx is an SSTV decoder for Unix-like systems.

Created by Oona Räisänen (OH2EIQ [at] sral.fi).

http://windytan.github.io/slowrx/

Features
--------

* Decode from audio device, file, or stdin
* Detect frequency-shifted signals
* Automatic slant correction, also manual adjustments are simple
* Adaptive noise reduction
* Support for a large number of SSTV modes
* Decode MMSSTV-compatible FSKID
* Save received pictures as PNG

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
