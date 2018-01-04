#!/bin/sh

export ARCH=arm 
#export CROSS_COMPILE=arm-linux-gnueabihf-

if [ ! -d ../out_v_up ]; then
  mkdir ../out_v_up;
fi

export RELEASE_DIR=../out_v_up
CPU_JOB_NUM=$(grep processor /proc/cpuinfo | awk '{field=$NF};END{print field+1}')



if [[ $1 = 'remake' && $2 = 'car' ]]; then
echo "Set Car type compile flags"
make distclean
make kodari_car_defconfig
echo "Done. Set Car type comile flags"
exit 0
fi

if [[ $1 = 'remake' && $2 = 'mobile' ]]; then
echo "Set Mobile type compile flags"
make distclean
make kodari_mobile_defconfig
echo "Done. Set Mobile type comile flags"
exit 0
fi
echo make -j$CPU_JOB_NUM
make -j$CPU_JOB_NUM 

echo "Make modules"

make modules_install INSTALL_MOD_PATH=$RELEASE_DIR/mod

echo
echo "Copying..."$RELEASE_DIR
cp -f ./arch/arm/boot/zImage $RELEASE_DIR/zImage
echo
echo "Copy done"

