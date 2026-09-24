#!/bin/sh

# Compose Steam library artwork from IW4x branding SVGs.
#
# Steam asks for four images per library entry and shows each in a different
# place, so they are composed:
#
#   cover   600x900   the portrait capsule in the library grid
#   wide    920x430   the horizontal capsule and the library header
#   hero   1920x620   the banner behind the entry's page
#   logo   1280x720   drawn over the hero, and so transparent
#
# Plus the small icon Steam shows in lists, which is the rounded mark.
#
# The two clients get their own set, differing in their accent colour and in
# the tag under the wordmark: IW4x has none, IW4x (mm) is tagged MM. They sit
# side by side in the library and a player has to be able to tell at a glance
# which is which.
#
# The results are committed under launcher/shortcut/artwork/ and embedded
# into the launcher, so this is only run when the branding changes. From the
# root of the repository:
#
#   sh etc/branding/compose-artwork.sh <branding-dir> \
#      launcher/launcher/shortcut/artwork
#
# where <branding-dir> is a checkout of the IW4x branding repository. Only
# three of its files are used: iw4x-logo.svg, iw4x-banner.svg and
# iw4x-logo-padding-round.svg.
#
# Requires ImageMagick 7.
#

set -e

B=${1:?usage: $0 <branding-dir> [<output-dir>]}
O=${2:-out}

BG='#222226'   # The brand background; the shipped icon already uses it.

accent ()
{
  case "$1" in
    x86) echo '#86BC25' ;;
    x64) echo '#F5CE64' ;;
  esac
}

tag ()
{
  case "$1" in
    x64) echo 'MM' ;;
  esac
}

GREENS='#89BD24 #86BC25'

FONT="$B/BankGothic Bold.ttf"

mkdir -p "$O"

recolour () # <in> <out> <accent>
{
  cp "$1" "$2"
  for g in $GREENS; do
    sed -i "s/$g/$3/Ig" "$2"
  done
}

for a in x86 x64; do
  L=$(tag "$a")
  FG=$(accent "$a")

  # Without a tag, render an empty label so the layout stays the same.
  #
  test -n "$L" || L=' '

  recolour "$B/iw4x-logo.svg"               mark.svg  "$FG"
  recolour "$B/iw4x-banner.svg"             word.svg  "$FG"
  recolour "$B/iw4x-logo-padding-round.svg" round.svg "$FG"

  magick -background none -density 200 mark.svg -trim +repage mark.png
  magick -background none -density 400 word.svg -trim +repage word.png

  magick -background none -density 200 round.svg -resize 256x256 \
    -strip "$O/$a-icon.png"

  magick -size 600x900 "xc:$BG" \
    \( mark.png -resize 330x330 \) -gravity north -geometry +0+210 -composite \
    \( -background none -fill "$FG" -font "$FONT" -pointsize 84 \
       -kerning 14 label:"$L" \) -gravity north -geometry +0+610 -composite \
    -strip "$O/$a-cover.png"

  magick -size 920x430 "xc:$BG" \
    \( word.png -resize 540x \) -gravity center -geometry +0-45 -composite \
    \( -background none -fill "$FG" -font "$FONT" -pointsize 54 \
       -kerning 11 label:"$L" \) -gravity center -geometry +0+115 -composite \
    -strip "$O/$a-wide.png"

  magick -size 1920x620 "xc:$BG" \
    \( mark.png -resize x1120 -alpha set \
       -channel A -evaluate multiply 0.13 +channel \) \
    -gravity east -geometry -130-30 -composite \
    -strip "$O/$a-hero.png"

  magick -size 1280x720 xc:none \
    \( word.png -resize 940x \) -gravity center -geometry +0-70 -composite \
    \( -background none -fill "$FG" -font "$FONT" -pointsize 92 \
       -kerning 20 label:"$L" \) -gravity center -geometry +0+140 -composite \
    -strip "$O/$a-logo.png"

  rm -f mark.svg word.svg round.svg mark.png word.png
done

if test -f "$O/../shortcut-artwork.cxx"; then
  touch "$O/../shortcut-artwork.cxx"
fi
