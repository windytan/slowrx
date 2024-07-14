#!/bin/bash

# gengallery: Generate a SSTV image gallery then upload it to a remote server.
#
# Requirements:
# - jq
# - file
# - lame
# - rsync
# - sox
# - imagemagick
# - netpbm
# - xz
#
# The HTML fragments for each image are generated, then the resulting fragments
# assembled into a single HTML file for upload.

IMAGE_DIR=$( dirname $( realpath ${0} ) )

# Capture arguments provided by slowrxd.
SSTV_EVENT=${1}                   # SSTV event triggering this script
SSTV_IMAGE="$( realpath "${2}" )" # SSTV image file
SSTV_LOG="$( realpath "${3}" )"   # SSTV log file (NDJSON or JSON)
SSTV_AUDIO="$( realpath "${4}" )" # SSTV audio file (Sun audio or MP3)

# Make a note of the base name
SSTV_BASE="${SSTV_IMAGE%.png}"

# Derive generated file names
SSTV_FREQ="${SSTV_BASE}.freq"
SSTV_SPEC="${SSTV_BASE}-spec.png"
SSTV_ORIG="${SSTV_BASE}-orig.png"

. ${IMAGE_DIR}/lib.sh

set -x

# Dump a HTML fragment displaying the image
# Arguments:
# 1: image alt text
# 2: image file name
# 3: optional style
#
# styles supported:
# - featured: use FEATURED_WIDTH for the image width instead of STANDARD_WIDTH
#
# Emits raw HTML
showimg() {
  alt_text="${1}"
  img_src="$( basename "${2}" )"
  img_file="$( realpath "${2}" )"
  style="${3}"

  base="${img_file%.png}"
  log_file="$( firstexistsof "${base}.json" "${base}.ndjson" )"
  orig_file="${base}-orig.png"
  spec_file="${base}-spec.png"
  freq_file="${base}.freq"
  audio_file="${base}.mp3"

  if [ ! -f "${orig_file}" ]; then
    # Image may in fact be in-progress
    orig_file="${img_file}"
  fi

  if [ -f "${log_file}" ]; then
    mode="$( logfetch "${log_file}" "mode" )"
    start_ts="$( logfetch "${log_file}" "start_ts" )"
    end_ts="$( logfetch "${log_file}" "end_ts" )"
    fsk_id="$( logfetch "${log_file}" "fsk_id" )"
  else
    mode="Unknown Mode"
    start_ts=""
    end_ts=""
    fsk_id=""
  fi

  if [ -f "${freq_file}" ]; then
    freq="$( cat "${freq_file}" )"
  else
    freq=""
  fi

  eval $( imgdims "${img_file}" )
  w=${orig_w}
  h=${orig_h}

  if [ -n "${w}" ] && [ -n "${h}" ]; then
    if [ "${style}" = featured ]; then
      # Scale the image up
      out_w=${FEATURED_WIDTH}
    else
      # Shrink it down
      out_w=${STANDARD_WIDTH}
    fi
    h=$(( (${orig_h} * ${out_w}) / ${orig_w} ))
    w=${out_w}
  fi

  if [ -n "${w}" ]; then
    w_attr="width=\"${w}\""
  fi

  map=""
  usemap=""
  if [ -n "${h}" ]; then
    if [ -f "${spec_file}" ]; then
      spec_h=$(( ${h} - ((${SPECTROGRAM_HEIGHT} * ${out_w}) / ${orig_w}) ))

      map="<map name=\"${name}-map\">"
      map="${map}<area shape=\"rect\""
      map="${map} coords=\"0, 0, ${w}, ${spec_h}\""
      map="${map} href=\"$( basename ${orig_file} )\" />"
      map="${map}<area shape=\"rect\""
      map="${map} coords=\"0, ${spec_h}, ${w}, ${h}\""
      map="${map} href=\"$( basename ${spec_file} )\" />"
      map="${map}</map>"
      usemap="usemap=\"#${name}-map\""
    fi
    h_attr="height=\"${h}\""
  fi

  cat <<EOF
<div class="imgcard">
  ${map}
  <img alt="${alt_text}" src="${img_src}"
    ${usemap} ${w_attr} ${h_attr} />
  <ul>
    <li><strong>SSTV Mode:</strong> ${mode}</li>
EOF
  if [ -n "${freq}" ]; then
    cat <<EOF
    <li><strong>Frequency:</strong> $( formatfreq ${freq} )</li>
EOF
  fi
  cat <<EOF
    <li><strong>Started:</strong> ${start_ts}</li>
EOF
  if [ -n "${end_ts}" ]; then
    cat <<EOF
    <li><strong>Finished:</strong> ${end_ts}</li>
EOF
  fi

  if [ -n "${fsk_id}" ]; then
    cat <<EOF
    <li><strong>Callsign:</strong> <code>${fsk_id}</code></li>
EOF
  fi

  if [ -n "${audio_file}" ] && [ -f "${audio_file}" ]; then
    cat <<EOF
    <li><strong><a href="$( basename ${audio_file} )">Transmission Audio</a></strong></li>
EOF
  fi
  cat <<EOF
  </ul>
</div>
EOF
}

# Are we at the end of a transmission?
if [ "${SSTV_EVENT}" = RECEIVE_END ]; then
  # Capture the frequency if not yet done, this can disrupt
  # reception due to quirks in rigctl, so we can't do this during
  # reception.
  if [ ! -f "${SSTV_FREQ}" ]; then
    getfreq > "${SSTV_FREQ}"
  fi

  # Generate a spectrogram of the audio if we haven't done it already.
  if [ ! -f "${SSTV_SPEC}" ]; then
    genspectrogram \
      "${SSTV_IMAGE}" \
      "${SSTV_AUDIO}" \
      "${SSTV_SPEC}"
  fi

  # Encode the audio as MP3 if not already done
  SSTV_AUDIO="$( audiotomp3 "${SSTV_AUDIO}" )"

  # Summarise the NDJSON log
  SSTV_LOG="$( summarisendjsonlog "${SSTV_LOG}" )"

  # Generate the composite output image
  generateoutimg "${SSTV_BASE}"
fi

if [ "${SSTV_LOG}" = "${IMAGE_DIR}/inprogress.ndjson" ]; then
  # We're receiving an image, check back in 5 seconds
  refresh_in=5
else
  # We're receiving an image, check back in a minute
  refresh_in=60
fi

# Emit the start of the web page.
# Adjust this to taste, e.g. you may or may not want to share your station
# details, that is up to you.

cat > ${IMAGE_DIR}/index.html <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN"
  "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en"
  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
  xsi:schemaLocation="http://www.w3.org/1999/xhtml http://www.w3.org/MarkUp/SCHEMA/xhtml11.xsd"
>
  <head>
    <title>SSTV traffic -- ${STATION_IDENT}</title>
    <link rel="stylesheet" href="style.css" />
    <meta http-equiv="Refresh" content="${refresh_in}" />
  </head>
  <body>
    <h1>${STATION_IDENT}</h1>
    <p>
      This is an experimental SSTV receiver station using <code>slowrxd</code>.
      <strong>Please note I do not control what people transmit on SSTV
      frequencies, and this service runs unsupervised.  If unsavory images
      are posted, the responsibility rests with the station transmitting them.</strong>
    </p>
    <ul>
      <li><strong>Location:</strong> Your Location <code>XY12ab</code></li>
      <li><strong>Antenna:</strong> Your antenna type</li>
      <li><strong>Radio:</strong> Your radio
      <li><strong>Radio Interface:</strong> Your radio interface
      <li><strong>Computer:</strong> Your computer</li>
      <li><strong>OS:</strong> Your OS</li>
      <li><strong>SSTV Decoder:</strong> <a href="https://github.com/windytan/slowrx/pull/12" target="_blank"><code>slowrx</code> + daemon addition</a></li>
    </ul>

    <p>This page will refresh in ${refresh_in} seconds.</p>
    <hr />
EOF

if [ "${SSTV_LOG}" = ${IMAGE_DIR}/inprogress.ndjson ]; then
  cat >> ${IMAGE_DIR}/index.html <<EOF
    <div id="inprogress">
      <h2>Currently in progress</h2>
      $( showimg "In progress SSTV image" \
        ${IMAGE_DIR}/inprogress.png \
        featured )
    </div>
    <hr />
EOF
fi

if [ -f "${IMAGE_DIR}/latest.png" ]; then
  cat >> ${IMAGE_DIR}/index.html <<EOF
    <div id="lastrx">
      <h2>Last received</h2>
      $( showimg "Last received SSTV image" \
        ${IMAGE_DIR}/latest.png \
        featured )
    </div>
    <hr />
EOF
fi

cat >> ${IMAGE_DIR}/index.html <<EOF
    <div id="previous">
      <h2>Previously received</h2>
EOF

latest_img="$( realpath ${IMAGE_DIR}/latest.png )"
for img in $( ls -1r ${IMAGE_DIR}/20*-orig.png ); do
  imgbase="${img%-orig.png}"
  if [ "${imgbase}.png" != "${latest_img}" ]; then
    if [ ! -f "${imgbase}.htmlpart" ]; then
      showimg "Previous image $( basename ${img} )" \
        "${imgbase}.png" \
        > "${imgbase}.htmlpart"
    fi
    cat "${imgbase}.htmlpart" >> ${IMAGE_DIR}/index.html
  fi
done
cat >> ${IMAGE_DIR}/index.html <<EOF
    </div>
    <hr />
    <div id="footer">
      <p>
        <a href="https://validator.w3.org/markup/check?uri=referer"><img
          src="valid-xhtml11.png" alt="Valid XHTML 1.1" height="31" width="88" /></a>
        <a href="https://jigsaw.w3.org/css-validator/check/referer">
          <img style="border:0;width:88px;height:31px"
            src="valid-css3.png"
          alt="Valid CSS!" />
        </a>
      </p>
      <p>Generated $( TZ=UTC date -Imin )</p>
    </div>
  </body>
</html>
EOF

# Upload the resulting files
rsync -zaHS --partial \
  --exclude "$( basename "$0" )" \
  --exclude .\*.swp \
  --exclude \*.sh \
  --exclude \*.au \
  --exclude \*.ndjson \
  --exclude \*.log \
  --exclude \*.freq \
  --exclude \*.htmlpart \
  --delete-before \
  ${IMAGE_DIR}/ \
  you@yourserver.example.com:/var/www/sites/sstv.example.com/htdocs/