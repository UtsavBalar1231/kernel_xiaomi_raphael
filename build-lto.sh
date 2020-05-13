#!/bin/bash
OUT_DIR=out/

make ARCH=arm64 \
	O=${OUT_DIR} \
	raphael_defconfig \
	-j$(nproc --all)

scripts/config --file ${OUT_DIR}/.config \
	-e LTO \
	-e LTO_CLANG \
	-e TOOLS_SUPPORT_RELR \

cd ${OUT_DIR}
make O=${OUT_DIR} \
	ARCH=arm64 \
	olddefconfig
cd ../

PATH=/home/utsavthecunt/proton-clang/bin/:$PATH

make ARCH=arm64 \
	O=out \
	CC="ccache clang" \
	LD="ld.lld" \
	AR="llvm-ar" \
	NM="llvm-nm" \
	OBJCOPY="llvm-objcopy" \
	OBJDUMP="llvm-objdump" \
	STRIP="llvm-strip" \
	CLANG_TRIPLE="aarch64-linux-gnu-" \
	CROSS_COMPILE="aarch64-linux-gnu-" \
	CROSS_COMPILE_ARM32="arm-linux-gnueabi-" \
	-j$(nproc --all)

rm out/.version
