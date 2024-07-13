Example SSTV CAM page script
============================

Requirements
------------

- `jq`: for decoding the NDJSON files to extract the FSK ID and mode
- `file` for figuring out the dimensions of the PNG image
- `lame` for MP3-encoding the SSTV signals
- `rsync` for uploading the result

Usage
-----

Drop these files into your image directory and adjust to taste.  Point
`slowrxd -x` at the `upload.sh` script.
