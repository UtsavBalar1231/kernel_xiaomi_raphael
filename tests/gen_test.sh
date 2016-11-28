#!/bin/bash

# We want to generate two device tree blob (.dtb) files by combining
# the "base" and "add" device tree source (.dts) files in two
# different ways.
#
# 1) /include/ the "add" at the end of the "base" file and
#   compile with dtc to make the "gold standard" .dtb
#
# 2) Compile them separately (modifying the "add" file to
#   be an overlay file) with dtc, then join them with the
#   ov_test program
#
# To do this, we need to generate a lot of files from the .base_dts
#    and .add_dts files:
# .base_inc_dts - Has the /include/ "${FILENAME}.add_dts" at the end.
# .base_inc_dtb - The dtc-generated "gold standard"
# .add_ov_dts - Has the /plugin/ at the start
# .base_dtb - Compiled version of just the base
# .add_ov_dtbo - Compiled version of the overlay
# .base_ov_dtb - overlay-test-joined version of the dtb
#
# Then, compare .base_inc_dtb and .base_ov_dtb with dtdiff
# (or maybe diff?) and return 0 iff they are identical.

# Include some functions from common.sh.
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
source ${SCRIPT_DIR}/common.sh

# Constants
IN_DATA_DIR="testdata"
OUT_DATA_DIR="test-out"

# Global variables
FILENAME=$1

on_exit() {
  rm -rf "${OUT_DATA_DIR}"
}

# Generate test cases to OUT_DATA_DIR
prepare_tests() {
  cp "${IN_DATA_DIR}/${FILENAME}.base_dts" "${OUT_DATA_DIR}/${FILENAME}.base_dts"
  cp "${IN_DATA_DIR}/${FILENAME}.add_dts" "${OUT_DATA_DIR}/${FILENAME}.add_dts"

  # Add the "include" to make .base_inc_dts
  cp "${IN_DATA_DIR}/${FILENAME}.base_dts" "${OUT_DATA_DIR}/${FILENAME}.base_inc_dts"
  echo "/include/ \"${FILENAME}.add_dts\"" >> "${OUT_DATA_DIR}/${FILENAME}.base_inc_dts"

  # Generate .base_inc_dtb
  dtc -@ -O dtb -s -b 0 -o "${OUT_DATA_DIR}/${FILENAME}.base_inc.dtb" \
    "${OUT_DATA_DIR}/${FILENAME}.base_inc_dts"

  # Add /dts-v1/ /plugin/; in front of .add_dts file. In order to trigger dtc's
  # fragment generation mechanism.
  if [ -a "${IN_DATA_DIR}/${FILENAME}.add_ov_dts" ]; then
    cp "${IN_DATA_DIR}/${FILENAME}.add_ov_dts" "${OUT_DATA_DIR}/${FILENAME}.add_ov_dts"
  else
    sed "1i/dts-v1/ /plugin/;" "${IN_DATA_DIR}/${FILENAME}.add_dts" > \
      "${OUT_DATA_DIR}/${FILENAME}.add_ov_dts"
  fi
}

# Compile test cases into dtb, and do the diff things.
compile_and_diff() {
  # Compile the base to make .base_dtb
  dtc -O dtb -b 0 -@ -o "${OUT_DATA_DIR}/${FILENAME}.base_dtb" \
    "${OUT_DATA_DIR}/${FILENAME}.base_dts"

  # Compile the overlay to make .add_ov_dtbo
  dtc -O dtb -b 0 -@ -o "${OUT_DATA_DIR}/${FILENAME}.add_ov_dtbo" \
    "${OUT_DATA_DIR}/${FILENAME}.add_ov_dts"

  # Run ov_test to combine .base_dtb and .add_ov_dtbo
  # into .base_ov_dtb
  ufdt_apply_overlay "${OUT_DATA_DIR}/${FILENAME}.base_dtb" \
    "${OUT_DATA_DIR}/${FILENAME}.add_ov_dtbo" \
    "${OUT_DATA_DIR}/${FILENAME}.base_ov.dtb"

  # Run the diff
  dt_diff ${OUT_DATA_DIR}/${FILENAME}.base_inc.dtb ${OUT_DATA_DIR}/${FILENAME}.base_ov.dtb
}

# The script will exit directly if any command fails.
set -e
trap on_exit EXIT

mkdir -p "${OUT_DATA_DIR}" >& /dev/null

prepare_tests
compile_and_diff
