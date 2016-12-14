#!/bin/bash

# Include some functions from common.sh.
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
source ${SCRIPT_DIR}/common.sh

# Usage: run_test_case <filename> <description>
# Args:
#   filename: The file name for ./gen_test.sh to generate and run the
#     test case. Several files under ./testdata subfolder are required:
#       - ./testdata/${filename}.base_dts
#       - ./testdata/${filename}.add_dts
#       - ./testdata/${filename}.add_ov_dts (optional)
#     For more details, check ./gen_test.sh.
#   description: a description message to be displayed in the terminal
run_test_case() {
  local filename="$1"
  local description="$2"

  alert "${description}"
  ./gen_test.sh "${filename}" >&2 ||
    die "Test case: ${filename} failed!!"
}

main() {
  alert "========== Running Tests of libufdt =========="

  if [ -z "${ANDROID_BUILD_TOP}" ]; then
    die "Run envsetup.sh / lunch yet?"
  fi

  if ! command_exists dtc ||
     ! command_exists ufdt_apply_overlay; then
    die "Run mmma $(dirname ${SCRIPT_DIR}) yet?"
  fi

  (

  # cd to ${SCRIPT_DIR} in a subshell because gen_test.sh uses relative
  # paths for dependent files.
  cd "${SCRIPT_DIR}"

  run_test_case \
    "no_local_fixup" \
    "Run test about fdt_apply_fragment with no local fixup"
  run_test_case \
    "apply_fragment" \
    "Run test about fdt_apply_fragment with phandle update"
  run_test_case \
    "libufdt_local_fixup" \
    "Run test about fdt_overlay_do_local_fixups"
  run_test_case \
    "dtc_local_fixup" \
    "Run test about local fixup format consistent with current dtc"
  run_test_case \
    "local_fixup_with_offset" \
    "Run test about dealing with local fixup with offset > 0"
  run_test_case \
    "overlay_2_layers" \
    "Run test about dealing with overlay deep tree"
  )

  if [ $? -ne 0 ]; then
    die "Some test cases failed, please check error message..."
  fi
}

main "$@"
