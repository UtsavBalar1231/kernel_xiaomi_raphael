#!/bin/bash

alert() {
  echo "$*" >&2
}

die() {
  echo "ERROR: $@"
  exit 1
}

command_exists () {
  type "$1" &> /dev/null;
}

dtb_to_dts () {
  dtc -O dts -s $1
  if [ $? -ne 0 ]; then
    die "dtb_to_dts $1 failed!"
  fi
}

dts_to_dtb () {
  dtc -O dtb -s -@ $1
  if [ $? -ne 0 ]; then
    die "dts_to_dtb $1 failed!"
  fi
}

remove_local_fixups() {
  sed '/__local_fixups__/ {s/^\s*__local_fixups__\s*//; :again;N; s/{[^{}]*};//; /^$/ !b again; d}' $1
}

remove_overlay_stuff() {
  # remove __symbols__, phandle, "linux,phandle" and __local_fixups__
  sed "/__symbols__/,/[}];/d" $1 | sed "/\(^[ \t]*phandle\)/d" | sed "/\(^[ \t]*linux,phandle\)/d" | sed '/^\s*$/d' | remove_local_fixups
}

dt_diff () {
  diff -u <(dtb_to_dts "$1" | remove_overlay_stuff) <(dtb_to_dts "$2" | remove_overlay_stuff)
}
