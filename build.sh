#!/bin/bash
OUT_DIR=out/

make ARCH=arm64 \
	O=${OUT_DIR} \
	raphael_defconfig \
	-j$(nproc --all)

scripts/config --file ${OUT_DIR}/.config \
	-d LTO \
	-d LTO_CLANG \
	-d TOOLS_SUPPORT_RELR \

cd ${OUT_DIR}
make O=${OUT_DIR} \
	ARCH=arm64 \
	olddefconfig
cd ../

PATH=/home/utsavthecunt/benzoclang/bin/:/home/utsavthecunt/arm64-gcc/bin:/home/utsavthecunt/arm32-gcc/bin:$PATH

make ARCH=arm64 \
	O=out \
	CC="ccache clang" \
	CLANG_TRIPLE="aarch64-linux-gnu-" \
	CROSS_COMPILE="aarch64-linux-android-" \
	CROSS_COMPILE_ARM32="arm-linux-androideabi-" \
	-j$(nproc --all)

rm out/.version
