#!/bin/bash

set -e

configs_dir="${PWD}/configs"
linux_config="${configs_dir}/linux.config"
busybox_config="${configs_dir}/busybox.config"
init_script="${configs_dir}/init"
build_dir="${PWD}/build"
qemu_params="-enable-kvm -cpu host -nographic -m 2G -smp 4 -s"
linux_src="${1}"
busybox_src="${2}"

function print_help() {
  printf "command: %s <LINUX KERNEL SRC> <BUSYBOX SRC>\n" "${0}"
}

if [[ -z $linux_src ]]; then
  print_help >&2
  printf "error: Linux kernel source not defined.\n" >&2
  exit
fi

if [[ -z $busybox_src ]]; then
  print_help >&2
  printf "error: BusyBox source not defined.\n" >&2
  exit
fi



linux_build="${build_dir}/linux"
printf "Building Linux kernel...\n"
mkdir -p $linux_build
make -C $linux_src O=$linux_build defconfig
cp $linux_config "${linux_build}/.config"
make -C $linux_src O=$linux_build -j8

busybox_build="${build_dir}/busybox"
printf "Building BusyBox...\n"
mkdir -p $busybox_build 
make -C $busybox_src O=$busybox_build defconfig
cp $busybox_config "${busybox_build}/.config"
make -C $busybox_src O=$busybox_build -j8
make -C $busybox_src O=$busybox_build install

make LINUX_SRC=${linux_build}
(cd userland && ninja)

busybox_install="${busybox_build}/_install"
initrd_build="${build_dir}/initrd"
initrd_image="${build_dir}/initrd.cpio.gz"
printf "Building initrd...\n"
rm -rf $initrd_build || true
mkdir -p $initrd_build
cp -a $busybox_install/* $initrd_build
cp -f $init_script "${initrd_build}/init"
install -m 644 -c *.ko $initrd_build
install -m 644 -c userland/*.out $initrd_build
(cd $initrd_build && find . -print0 | cpio --null -ov --format=newc | gzip -9 > $initrd_image)

linux_image="${linux_build}/arch/x86_64/boot/bzImage"
printf "Booting QEMU...\n"
qemu-system-x86_64 $qemu_params -kernel $linux_image -initrd $initrd_image -append "console=ttyS0 root=/dev/ram0 rootfstype=tmpfs rw"
