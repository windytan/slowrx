#!/bin/bash

# Widths of standard and featured images
FEATURED_WIDTH=720
STANDARD_WIDTH=320

# Station identity
STATION_IDENT="N0CALL SSTV @ QTH"

# Information text font -- use any TTF font you like, but the full path
# must be known here.  See generateoutimg for how this is used, you may
# need to tweak things there too if you don't use the font I used here.
INFO_IMG_FONT=/usr/share/fonts/truetype/terminus/TerminusTTF-4.46.0.ttf

# Spectrogram graph will be a fixed height, and scaled width-wise to match the
# width of the output image.
SPECTROGRAM_GRAPH_HEIGHT=80

# SoX seems to add this size to the dimension you give it for legends,
# axis labels, etc.
SPECTROGRAM_WIDTH_PAD=144
SPECTROGRAM_HEIGHT_PAD=78

# Given this information, we can work out exactly how high a spectrogram will
# be including all padding.
SPECTROGRAM_HEIGHT=$(( \
  ${SPECTROGRAM_GRAPH_HEIGHT} + ${SPECTROGRAM_HEIGHT_PAD} \
))

# Capture the frequency from rigctl.  Tweak this for your set-up, or if you
# only ever receive on one frequency, replace this with an `echo ${FREQ}`.
# The frequency is given in Hz.
getfreq() {
  rigctl -m 2 -r localhost:4532 --vfo f VFOA
}

# Parse a time-stamp into ISO-8601 format
# Input is a timestamp in milliseconds since 1st Jan 1970.
# Returns empty string if the input is empty.
parsets() {
  if [ -n "$1" ]; then
    TZ=UTC date --date @$(( $1 / 1000 )) -Isec
  fi
}

# HTML-escape a string
htmlescape() {
  sed -e 's/&/\&amp;/; s/</\&lt;/; s/>/\&gt;/;'
}

# Format a frequency in Hz as kHz.
# Arguments:
# 1: frequency in Hz
# 2: text to use if $1 is empty
formatfreq() {
  if [ -n "${1}" ]; then
    freq_khz=$(( ${1} / 1000 ))
    freq_hz=$(( ${1} % 1000 ))
    printf "%6d.%03d kHz" ${freq_khz} ${freq_hz}
  else
    echo "${2}"
  fi
}

# Retrieve the dimensions of an image.  Use with eval,
# defines orig_w and orig_h.
imgdims() {
  file "$( realpath "$1" )" | cut -f 2 -d, \
    | sed -e 's/^ \+/orig_w=/; s/ x / orig_h=/; s/[^0-9]\+$//g;'
}

# Fetch a data field from a log, either a NDJSON full log, or a summarised
# JSON log.
# Arguments:
# 1: log file (NDJSON or JSON file)
# 2: field requested
#
# Fields supported:
#
# - mode: the mode description
# - start_ts: ISO-8601 start of transmission
# - end_ts: ISO-8601 end of transmission, empty string if in progress
# - fsk_id: FSK ID from station, empty if not detected
logfetch() {
  case "${1}" in
  *.ndjson)
    case "${2}" in
    mode)
      head -n 1 "${1}" | jq -r .desc
      ;;
    start_ts)
      parsets $( head -n 1 "${1}" | jq -r .timestamp )
      ;;
    end_ts)
      parsets $( grep RECEIVE_END "${1}" | jq -r .timestamp )
      ;;
    fsk_id)
      grep FSK_RECEIVED "${1}" | jq -r .id
      ;;
    esac
    ;;
  *.json)
    case "${2}" in
    mode)
      jq -r .VIS_DETECT.desc "${1}"
      ;;
    start_ts)
      parsets $( jq -r .VIS_DETECT.timestamp "${1}" )
      ;;
    end_ts)
      parsets $( jq -r '.RECEIVE_END.timestamp // ""' "${1}" )
      ;;
    fsk_id)
      jq -r '.FSK_RECEIVED.id // ""' "${1}" \
        | tr -d '\n\t' | tr -C '[:alnum:]_/-' '?'
      ;;
    esac
    ;;
  esac
}

# Summarise a log file.  This picks out the critical fields needed for
# rendering a gallery out of a NDJSON image and generates a JSON file from it.
# The input NDJSON is then compressed with XZ for archival purposes.
#
# If the file already is a plain JSON, nothing is done.
#
# Symbolic links are not modified.
#
# Arguments:
# - input log file
#
# Returns
# - output log file
summarisendjsonlog() {
  input="${1}"
  ext="${input##*.}"

  if [ -f "${input}" ] && [ ! -L "${input}" ] \
      && [ "${ext}" = ndjson ]
  then
    output="${input%${ext}}json"
    sed -e '1 s/^/[/ ; 2,$ s/^/,/; $ s/$/]/' "${input}" \
      | jq '[ .[] | select(.type == "VIS_DETECT" or .type == "FSK_RECEIVED" or .type == "RECEIVE_END") | {"key":.type, "value":.} ] | from_entries' \
      > "${output}"
    xz -9 "${input}"

    # Update the latest symlink
    if [ -L "${IMAGE_DIR}/latest.ndjson" ] \
      && [ "$( readlink "${IMAGE_DIR}/latest.ndjson" )" \
        = "$( basename "${input}" )" ]
    then
      rm "${IMAGE_DIR}/latest.json" \
        "${IMAGE_DIR}/latest.ndjson"
      ln -s "$( basename "${output}" )" \
        "${IMAGE_DIR}/latest.json"
    fi

    echo "${output}"
  else
    echo "${input}"
  fi
}

# Generate a spectrogram of the audio recording
# 1: image file from the transmission
# 2: audio file of the transmission
# 3: spectrogram file name
genspectrogram() {
  orig_img="${1}"
  audio_dump="${2}"
  out_img="${3}"

  eval $( imgdims "${orig_img}" )

  # Maintain the aspect ratio of the original image.
  # output image will be 158px higher (96 + 62px for
  # headers/labels)
  out_h=$(( ${orig_h} + ${SPECTROGRAM_HEIGHT} ))
  out_w=$(( (${orig_w} * ${out_h}) / ${orig_h} ))

  # Generate the spectrogram
  spec_w=$(( ${out_w} - ${SPECTROGRAM_WIDTH_PAD} ))
  sox "${audio_dump}" -n rate 6k spectrogram \
    -x ${spec_w} -y 80 -z 48 -Z -16 -o "${out_img}"
}

# Return the first file given that exists.
firstexistsof() {
  for f in "$@"; do
    if [ -f "${f}" ]; then
      echo "${f}"
      break
    fi
  done
}

# Generate the output image that embeds the original transmitted image
# along with the spectrogram and some data of when the file was received,
# the frequency, and the output mode.
#
# Arguments:
# 1: base name of the image being generated without extension.
#    - input image will be ${1}-orig.png
#    - spectrogram will be ${1}-spec.png
#    - log will be ${1}.json or ${1}.ndjson
#    - frequency will be kept in ${1}.freq (plain text with frequency in Hz)
#    - output image will be ${1}.png
generateoutimg() {
  input_img="${1}-orig.png"
  spec_img="${1}-spec.png"
  freq_file="${1}.freq"
  output_img="${1}.png"
  left_img="${1}-left.png"
  right_img="${1}-right.png"
  log_file="$( firstexistsof "${1}.json" "${1}.ndjson" )"

  # If we have an output file, but no original, then assume
  # that *is* the original and rename it so we don't clobber it!
  if [ -f "${output_img}" ] && [ ! -f "${input_img}" ]; then
    mv -v "${output_img}" "${input_img}"
  fi

  # Pick up the dimensions of the original image
  eval $( imgdims "${input_img}" )

  # Generate left-side info image
  infotxt=""
  if [ -f "${freq_file}" ]; then
    freq=$( cat "${freq_file}" )
    infotxt="${infotxt}Received on $( \
      formatfreq ${freq} "unknown frequency" )\n"
  else
    freq=""
    infotxt="${infotxt}Received "
  fi

  # e.g. 2024-07-14T04-22Z-S1-VK3EME.png
  timestamp=$( basename $( realpath "${input_img}" ) \
    | sed -e 's/^\(20..-..-..\)T\(..\)-\(..\)Z.*$/\1T\2:\3Z/' )
  infotxt="${infotxt}at ${timestamp}\n"
  infotxt="${infotxt}by ${STATION_IDENT}"

  # Generate a timestamp image go on the left side of the image.
  # The positions of the three lines may need tweaking if you use a
  # different font.
  convert -size ${orig_h}x56 canvas:none \
    -font ${INFO_IMG_FONT} \
    -pointsize 16 -fill white \
    -draw "text 0,16 '$( echo -e "${infotxt}" | tail -n +1 | head -n 1 )'" \
    -draw "text 0,32 '$( echo -e "${infotxt}" | tail -n +2 | head -n 1 )'" \
    -draw "text 0,48 '$( echo -e "${infotxt}" | tail -n +3 | head -n 1 )'" \
    -rotate -90 \
    "${left_img}"

  # Generate right-side info image
  mode=$( logfetch "${log_file}" "mode" )
  fsk_id=$( logfetch "${log_file}" "fsk_id" )

  infotxt="SSTV Mode: ${mode}"
  if [ -n "${fsk_id}" ]; then
    infotxt="${infotxt}\nFSK ID: ${fsk_id}"
  else
    infotxt="${infotxt}\nFSK ID not known"
  fi

  # Generate a timestamp image go on the left side of the image.
  convert -size ${orig_h}x56 canvas:none \
    -font ${INFO_IMG_FONT} \
    -pointsize 16 -fill white \
    -draw "text 0,32 '$( echo -e "${infotxt}" | tail -n +1 | head -n 1 )'" \
    -draw "text 0,48 '$( echo -e "${infotxt}" | tail -n +2 | head -n 1 )'" \
    -rotate -90 \
    "${right_img}"

  # Convert to netpbm PAM format
  for f in "${input_img}" "${spec_img}"; do
    pngtopam ${f} > ${f}.pam
  done
  # Convert info images with alpha
  for f in "${left_img}" "${right_img}"; do
    pngtopam -alpha ${f} > ${f}.pam
  done

  # Concatenate and generate final output PNG
  pamcat -topbottom -black -jcenter \
      "${input_img}.pam" "${spec_img}.pam" \
    | pamcomp -align left -valign top "${left_img}.pam" - \
    | pamcomp -align right -valign top "${right_img}.pam" - \
    | pamtopng -interlace > "${output_img}"

  # Remove temporary PAM files and images
  rm -f *.pam ${left_img} ${right_img}
}

# Convert an AU format audio file to MP3 if required.  If `latest.au` points
# to this file, this is replaced by a `latest.mp3` pointing at the new file.
# Arguments:
# 1: audio file to convert, if it does not have an .mp3 extension, it will be
#    encoded as MP3 and the original removed.
#
# Returns the name of the MP3
audiotomp3() {
  input="${1}"
  ext="${input##*.}"
  if [ "${ext}" != "mp3" ]; then
    output="${input%${ext}}mp3"
    lame -S "${input}" "${output}" && rm -f "${input}"

    # Update the latest symlink
    if [ -L "${IMAGE_DIR}/latest.au" ] \
      && [ "$( readlink "${IMAGE_DIR}/latest.au" )" \
        = "$( basename "${input}" )" ]
    then
      rm "${IMAGE_DIR}/latest.mp3" \
        "${IMAGE_DIR}/latest.au"
      ln -s "$( basename "${output}" )" \
        "${IMAGE_DIR}/latest.mp3"
    fi

    # Report the name of the new audio file
    echo "${output}"
  else
    echo "${input}"
  fi
}
