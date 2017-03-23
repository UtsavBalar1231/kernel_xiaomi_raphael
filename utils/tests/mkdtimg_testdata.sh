#!/bin/bash

SRCDIR="data"
DTS_LIST="
  board1v1.dts
  board1v1_1.dts
  board2v1.dts
"
CONFIG="${SRCDIR}/mkdtimg.cfg"

ALIGN=4

OUTDIR="out"
OUTDTB="${OUTDIR}/dump.dtb"

mkdir -p "$OUTDIR"
for dts in ${DTS_LIST}; do
  echo "Building $dts..."
  src_dts="${SRCDIR}/${dts}"
  out_dtb="${OUTDIR}/${dts}.dtb"
  dtc -O dtb -@ -qq -a "$ALIGN" -o "$out_dtb" "$src_dts"
done

IMG="${OUTDIR}/cfg_create.img"
mkdtimg cfg_create "$IMG" "${CONFIG}" --dtb-dir="$OUTDIR"
mkdtimg dump "$IMG" -b "$OUTDTB" | tee "${OUTDIR}/cfg_create.dump"

IMG="${OUTDIR}/create.img"
mkdtimg create "$IMG" \
  --page_size=4096 --id=/:board_id --rev=/:board_rev --custom0=0xabc \
  "${OUTDIR}/board1v1.dts.dtb" \
  "${OUTDIR}/board1v1_1.dts.dtb" --id=/:another_board_id \
  "${OUTDIR}/board2v1.dts.dtb" --rev=0x201
mkdtimg dump "$IMG" | tee "${OUTDIR}/create.dump"

diff "${OUTDIR}/cfg_create.dump" "${OUTDIR}/create.dump"
