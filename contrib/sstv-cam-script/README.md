Example SSTV CAM page script
============================

Requirements
------------

- `jq`: for decoding the NDJSON files to extract the FSK ID and mode
- `file` for figuring out the dimensions of the PNG image
- `lame` for MP3-encoding the SSTV signals
- `sox` for generating spectrograms of the SSTV signals
- `imagemagick` for generating the timestamp/mode/FSK ID data embedded in the
  image (vertical text running up the left/right sides of the image).
- `netpbm` for assembling the final uploaded image
- `xz` for archiving the NDJSON files after summarisation.
- `rsync` for uploading the result

Usage
-----

Drop these files into your image directory and adjust to taste.  Point
`slowrxd -x` at the `upload.sh` script.
