#!/bin/sh
# po/update.sh: extract the texts of the GonWare Commander into
# po/cmdr.pot and merge them into every po/<language>.po
# (run from anywhere; needs xgettext and msgmerge)
set -e
cd "$(dirname "$0")/.."
xgettext --from-code=UTF-8 -k_ -kN_ -kP_:1,2 -kNC_:1c,2 -ktrc:1c,2 \
	--package-name="GonWare Commander" --package-version=4.0 \
	--msgid-bugs-address="https://gitea.disconnected-by-peer.at/GonSoft/deco/issues" \
	-o po/cmdr.pot *.c
for po in po/*.po; do
	msgmerge -q --update --backup=none "$po" po/cmdr.pot
	msgfmt -c --statistics -o /dev/null "$po"
done
