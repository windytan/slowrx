#!/bin/sh

# gengallery: Generate a SSTV image gallery then upload it to a remote server.
#
# Requirements:
# - jq
# - file
# - lame
# - rsync
#
# The HTML fragments for each image are generated, then the resulting fragments
# assembled into a single HTML file for upload.

echo "Handling event ${1}"
echo "Image file: ${2}"
echo "Log file: ${3}"
echo "Audio dump: ${4}"

set -x

IMAGE_DIR=$( dirname $( realpath ${0} ) )

parsets() {
  if [ -n "$1" ]; then
    TZ=UTC date --date @$(( $1 / 1000 )) -Isec
  fi
}

imgdims() {
  file "$( realpath "$1" )" | cut -f 2 -d, \
    | sed -e 's/^ \+/w=/; s/ x / h=/; s/[^0-9]\+$//g;'
  }

  showimg() {
    if [ -f "${3}" ]; then
      case "${3}" in
        *.ndjson)
          mode="$( head -n 1 "${3}" | jq -r .desc )"
          start_ts="$( parsets $( head -n 1 "${3}" | jq -r .timestamp ) )"
          end_ts="$( grep RECEIVE_END "${3}" | jq -r .timestamp )"
          fsk_id="$( grep FSK_RECEIVED "${3}" | jq -r .id )"
          ;;
        *.json)
          mode="$( jq -r .VIS_DETECT.desc "${3}" )"
          start_ts="$( parsets $( jq -r .VIS_DETECT.timestamp "${3}" ) )"
          end_ts="$( jq -r '.RECEIVE_END.timestamp // ""' "${3}" )"
          fsk_id="$( jq -r '.FSK_RECEIVED.id // ""' "${3}" | tr -d '\n\t' | tr -C '[:alnum:]_/-' '?' )"
          ;;
      esac
    else
      mode="???"
      start_ts="???"
      end_ts=""
      fsk_id=""
    fi

    eval $( imgdims "${2}" )
    audio="${2%.png}.mp3"

    if [ "${5}" = featured ] && [ -n "${w}" ] && [ -n "${h}" ]; then
      # Scale the image up
      h=$(( (${h} * 720) / ${w} ))
      w=720
    fi

    if [ -n "${w}" ]; then
      w="width=\"${w}\""
    fi

    if [ -n "${h}" ]; then
      h="height=\"${h}\""
    fi

    cat <<EOF
<div class="imgcard">
<img alt="${1}" src="$( basename "${2}" )" ${w} ${h} />
<ul>
  <li><strong>SSTV Mode:</strong> ${mode}</li>
  <li><strong>Started:</strong> ${start_ts}</li>
EOF
if [ -n "${end_ts}" ]; then
  cat <<EOF
  <li><strong>Finished:</strong> $( parsets ${end_ts} )</li>
EOF
fi

if [ -n "${fsk_id}" ]; then
  cat <<EOF
  <li><strong>Callsign:</strong> <code>${fsk_id}</code></li>
EOF
fi

if [ -n "${audio}" ] && [ -f "${audio}" ]; then
  cat <<EOF
  <li><strong><a href="$( basename ${audio} )" target="_blank">Audio</a></strong></li>
EOF
fi
cat <<EOF
</ul>
</div>
EOF
}

# Encode the latest received file as MP3
if [ "$1" = RECEIVE_END ] \
  && [ -n "$4" ] \
  && [ -f "$4" ] \
  && [ ! -L "$4" ] \
  && [ "${4##*.}" = "au" ]
then
  sstv_rec_au="$4"
  sstv_rec_mp3="${4%.au}.mp3"
  lame -S "${sstv_rec_au}" "${sstv_rec_mp3}" && rm -fv "${sstv_rec_au}"

  if [ -L "${IMAGE_DIR}/latest.au" ] \
    && [ "$( readlink "${IMAGE_DIR}/latest.au" )" \
    = "$( basename "${sstv_rec_au}" )" ]
    then
      rm "${IMAGE_DIR}/latest.mp3" \
        "${IMAGE_DIR}/latest.au"
              ln -s "$( basename "${sstv_rec_mp3}" )" \
                "${IMAGE_DIR}/latest.mp3"
  fi
fi

if [ "$3" = ${IMAGE_DIR}/inprogress.ndjson ]; then
  refresh_in=5
else
  refresh_in=60
fi

cat > ${IMAGE_DIR}/index.html <<EOF
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN"
  "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en"
  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
  xsi:schemaLocation="http://www.w3.org/1999/xhtml http://www.w3.org/MarkUp/SCHEMA/xhtml11.xsd"
>
  <head>
    <title>SSTV traffic -- N0CALL @ QTH</title>
    <link rel="stylesheet" href="style.css" />
    <meta http-equiv="Refresh" content="${refresh_in}" />
  </head>
  <body>
    <h1>N0CALL SSTV traffic</h1>
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

if [ "$3" = ${IMAGE_DIR}/inprogress.ndjson ]; then
  cat >> ${IMAGE_DIR}/index.html <<EOF
    <div id="inprogress">
      <h2>Currently in progress</h2>
      $( showimg "In progress SSTV image" \
        ${IMAGE_DIR}/inprogress.png \
        ${IMAGE_DIR}/inprogress.ndjson \
        - featured )
    </div>
    <hr />
EOF
fi

if [ -f "${IMAGE_DIR}/latest.ndjson" ]; then
  protected=$( basename $( readlink ${IMAGE_DIR}/latest.ndjson ) )
  for f in ${IMAGE_DIR}/*.ndjson; do
    if [ -L ${f} ]; then
      continue
    fi
    sed -e '1 s/^/[/ ; 2,$ s/^/,/; $ s/$/]/' \
      ${f} | jq '[ .[] | select(.type == "VIS_DETECT" or .type == "FSK_RECEIVED" or .type == "RECEIVE_END") | {"key":.type, "value":.} ] | from_entries' \
      > ${f%.ndjson}.json
          case "$( basename "$f" )" in
            ${protected}|inprogress.ndjson)
              ;;
            *)
              xz -9v ${f}
              ;;
          esac
        done

        cat >> ${IMAGE_DIR}/index.html <<EOF
    <div id="lastrx">
      <h2>Last received</h2>
      $( showimg "Last received SSTV image" \
        ${IMAGE_DIR}/latest.png \
        ${IMAGE_DIR}/latest.ndjson \
        ${IMAGE_DIR}/latest.mp3 \
        featured )
    </div>
    <hr />
EOF
fi

cat >> ${IMAGE_DIR}/index.html <<EOF
    <div id="previous">
      <h2>Previously received</h2>
EOF

latest_img="$( readlink ${IMAGE_DIR}/latest.png )"
for img in $( ls -1r ${IMAGE_DIR}/20*.png ); do
  imgbase="${img%.png}"
  if [ "${img}" != "${IMAGE_DIR}/${latest_img}" ]; then
    if [ ! -f "${imgbase}.htmlpart" ]; then
      showimg "Previous image ${img}" "${img}" \
        "${imgbase}.json" \
        "${imgbase}.mp3" \
        > "${imgbase}.htmlpart"
    fi
    cat "${imgbase}.htmlpart" >> ${IMAGE_DIR}/index.html
  fi
done
cat >> ${IMAGE_DIR}/index.html <<EOF
    </div>
    <hr />
    <div id="footer">
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
  --delete-before \
  ${IMAGE_DIR}/ \
  you@yourserver.example.com:/var/www/sites/sstv.example.com/htdocs/
