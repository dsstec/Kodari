#!/bin/sh
echo
echo KODARI Bootloader building start
#export CROSS_COMPILE=arm-linux-gnueabihf-
export ARCH=arm

if [ ! -d ../out_v_up ]; then
  mkdir ../out_v_up;
fi

CPU_JOB_NUM=$(grep processor /proc/cpuinfo | awk '{field=$NF};END{print field+1}')
#CPU_JOB_NUM=1
make distclean
echo make distclean done!!!
make smdk4412_config
echo make smdk4412_config  board configuration done!

export RELEASE_DIR=../out_v_up

echo buiding !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
echo make -j$CPU_JOB_NUM
make -j$CPU_JOB_NUM


echo
echo "Copying..."$RELEASE_DIR
cp -f u-boot.bin $RELEASE_DIR
cp -f bl2.bin $RELEASE_DIR
echo
