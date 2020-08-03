#!/bin/bash
OUT_DIR=out/

export KBUILD_BUILD_HOST=CuntsSpace

make ARCH=arm64 \
	O=${OUT_DIR} \
	raphael_defconfig \
	-j8

scripts/config --file ${OUT_DIR}/.config \
	-d LTO \
	-d LTO_CLANG \
	-e SHADOW_CALL_STACK \
	-e TOOLS_SUPPORT_RELR \
	-e LD_LLD

cd ${OUT_DIR}
make O=${OUT_DIR} \
	ARCH=arm64 \
	olddefconfig
cd ../

PATH=/home/utsavthecunt/proton-clang/bin/:$PATH

make ARCH=arm64 \
	O=${OUT_DIR} \
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
	-j8

rm ${OUT_DIR}/.version
