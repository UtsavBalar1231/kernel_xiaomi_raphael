#!/bin/bash
OUT_DIR=out/

export KBUILD_BUILD_HOST=CuntsSpace
export KERNEL_USE_CCACHE=1

make ARCH=arm64 \
	O=${OUT_DIR} \
	raphael_defconfig \
	-j4

scripts/config --file ${OUT_DIR}/.config \
	-d LTO \
	-d LTO_CLANG \
	-d TOOLS_SUPPORT_RELR \
	-d LD_LLD \
	-d FORTIFY_SOURCE

cd ${OUT_DIR}
make O=${OUT_DIR} \
	ARCH=arm64 \
	olddefconfig
cd ../

PATH=/home/utsavthecunt/arm64-gcc/bin:/home/utsavthecunt/arm32-gcc/bin:$PATH

make ARCH=arm64 \
	O=out \
	CROSS_COMPILE="aarch64-elf-" \
	CROSS_COMPILE_ARM32="arm-eabi-" \
	-j4

rm out/.version
