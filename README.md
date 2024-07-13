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

### Common requirements

* Linux
* Alsa (`libasound2-dev`)
* FFTW 3 (`libfftw3-dev`)

And, obviously:

* shortwave radio with SSB
* computer with sound card capable of at least 6kHz sample rate (>24kHz
  strongly recommended)
* means of getting sound from radio to sound card

### For the GUI:

* gtk+ 3.4 (`libgtk-3-dev`)

### For the daemon:

* libgd

Compiling
---------

`make`

### GUI only

```
make slowrx
```

### Daemon only

The daemon is experimental.

```
make slowrxd
```

Running
-------

### GUI

`./slowrx`

### Daemon

`./slowrxd [arguments]`

The daemon runs in the foreground, the intention is to run it beneath a daemon
service like `supervisord` or `systemd`.  An example unit file for `systemd`
might be (see the
[`systemd-unit` manpage](https://www.freedesktop.org/software/systemd/man/latest/systemd.unit.html)
for correct syntax):

```
[Unit]
Description=slowrxd

[Service]
User=slowrxuser
ExecStart=/path/to/slowrxd [arguments]

[Install]
WantedBy=multi-user.target
```

#### Daemon arguments

These are listed with the `-h` argument:

```
$ ./slowrxd -h
Usage: ./slowrxd [-h] [-F] [-S] [-A inprogress.au]
        [-I inprogress.png] [-L inprogress.ndjson] [-a latest.au]
        [-c channel] [-d directory] [-i latest.png]
        [-l latest.ndjson] [-p pcmdevice] [-r samplerate]
        [-x script]

where:
  -F : disable FSK ID detection
  -S : disable slant correction
  -h : display this help and exit
  -d : set the directory where images are kept
  -A : set the in-progress audio dump path (- to disable)
  -I : set the in-progress image path (- to disable)
  -L : set the in-progress receive log path (- to disable)
  -a : set the latest audio dump path (- to disable)
  -c : set the audio channel to use, left, right or mono
  -i : set the latest image path (- to disable)
  -l : set the latest receive log path (- to disable)
  -r : set the ALSA PCM sample rate
  -p : set the ALSA PCM capture device
  -x : specify a script to run on receive events
```

##### Paths

`-d directory` sets the destination for files to `directory`, the path must
already exist.  Default is the current working directory.  All output files are
assumed to reside in the same directory (and **must** reside on the same
filesystem volume).  As the image is received, it is periodically written to
the _in progress_ image path (see `-I`) as a PNG image.

Alongside this file are two other files (unless disabled):

 * `inprogress.ndjson`: a [NDJSON](https://github.com/ndjson/ndjson-spec) log
   file storing the events as the file was received.
 * `inprogress.au`: a [Sun Audio](https://en.wikipedia.org/wiki/Au_file_format)
   audio file containing the recording of the transmission.

`-A`, `-I` and `-L` set the path to the audio dump, image and receive log of
the transmission currently being received.  Relative to the output directory
(see `-d`) unless they start with a `/`.  `-A` and `-L` may be disabled by
specifying `-` as the path.  `-I` may not be disabled.

`-a`, `-i` and `-l`: same as the upper-case counterparts, but these are for the
symbolic link to the _latest_ received image.  Again, these are relative to the
output directory unless they start with a `/`.  Pass `-` as the argument to
disable the corresponding symbolic link being generated.

(**NOTE** the code that figures out what the _target_ of the symlink is, is
especially dumb and does **NOT** handle the symlink residing in a different
place to the output file.)

When a SSTV image is fully received (after the FSK ID), the file names of the
three (or less) files currently named `inprogress` will be renamed to one of
the following forms:

 * `YYYY-mm-ddTHH-MMZ-MODE-FSKID` if the FSK ID was decoded (non-alphanumeric
   characters in the FSK ID are replaced with `-`).
 * `YYYY-mm-ddTHH-MMZ-MODE` if no FSK ID was detected (or it was disabled).

The "latest" image symbolic links (`-a`, `-i` and `-l`) will then be re-created
to point to these new files.

##### PCM input options

`slowrxd` uses ALSA device names (see `arecord -L`).  You can specify a
different PCM capture device with `-p name`.  e.g. Pipewire users may want
`-p pipewire` or `-p pulse`.

The sample rate can be adjusted with `-r`.  The default is 44100 Hz for
historical reasons, however some users may want to set this to 48000 if they
have hardware that requires it (some modern sound cards do not natively support
44.1kHz) or they are using a userspace subsystem like PulseAudio, PipeWire or
JACK running at this (or any other) sample rate.

Bare minimum sample rate required is 6kHz, and will work but deliver poor
results ([see this Mastodon thread](https://mastodon.longlandclan.id.au/@stuartl/112771433903628425)
for an example of how poor).

`-c` selects which channel is used.  By default, the left channel is used, but
if your interface requires it, you may choose the right channel, or have both
summed together (mono).  Only the first letter (`l`, `m` or `r`) is used, and
case does not matter.

##### SSTV decoder features

By default, FSK ID detection and slant correction are enabled, you can disable
these with `-F` and `-S` respectively.

##### Running scripts

The daemon can run a script on events.  The path to the script must include the
relative or full path to the script if it is not in `${PATH}` and must be
executable by the process running `slowrxd`.

It will be called with 4 arguments:

 * the event being triggered (see log file format)
 * the path to the image file (either the in-progress or final image)
 * the path to the log file for the image
 * the path to the audio file for the image

This script **MUST** do its thing then return back to the process as soon as
possible, because it will block the SSTV receiver otherwise!

A recommended solution to this is to write a simple shell script that launches
the real workhorse as a forked process in the background using
[`nohup`](https://en.wikipedia.org/wiki/Nohup):

```bash
#!/bin/sh

nohup $( dirname $( realpath $0 ) )/real-upload.sh "$@" > upload.log 2>&1 &
```

This script can do whatever you want:

 * upload the file to a web server for a SSTV cam
 * notify another program
 * post the image to social media

…etc…

#### The log file format

NDJSON was used since JSON itself is relatively easy to generate from C code,
and this allows for separate JSON "packets" to be logged one per line, to the
file and still maintain valid JSON at all times.

Each line of the NDJSON file is a JSON object storing a log record.

 * `timestamp` (required): The timestamp of the event, in milliseconds since
   the 1st January 1970 (UTC).
 * `type` (required): The type of log record being emitted.
 * `msg` (optional): Message text string, if applicable.

Each record type may have its own special parameters.

##### `RECEIVE_START` records

This indicates the start of a transmission.  No special log records here.

##### `VIS_DETECT` records

This indicates the VIS header has been detected and decoded.

 * `code`: Raw VIS code as an integer
 * `mode`: Human+Machine-readable "short" descriptor of the mode.  See the
   `ShortName` fields in `modespec.c`.
 * `desc`: Human-readable description of the mode.  See the `Name` field in
   `modespec.c`.

##### `SIG_STRENGTH` records

This is the FFT power calculation.  Zero-valued FFT buckets before and after
the signal are omitted.

 * `win`: The Hann window index being used
 * `num`: the number of FFT buckets computed (usually 1024)
 * `first`: the FFT bucket number of the first bucket storing non-zero data,
   this will correspond to the first element of `fft`.
 * `last`: the FFT bucket number of the last bucket storing non-zero data, this
   will correspond to the last element of `fft`.
 * `fft`: the FFT buckets between `first` and `last` (inclusive).

All omitted buckets may be assumed to contain zeroes (`0.000000`).

##### `IMAGE_REFRESHED` records

These do not actually appear in the log, but instead are used exclusively with
the receive script.  This event tells the receive script that the image has
been re-drawn with new image data.

##### `FSK_DETECT` records

Indication that the actual SSTV image has been received in full (and no slant
correction yet applied).

##### `STATUS` records

These indicate the status bar text that would be seen in the GTK+ UI.

##### `FSK_RECEIVED` records

These are emitted if a FSK ID is successfully decoded.

 * `id` is the FSK ID decoded.

##### `RECEIVE_END` records

Indicate that reception of this particular image has finished.
