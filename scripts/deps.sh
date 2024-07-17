#!/bin/bash
set -e

CROSS_COMPILE="${CROSS_COMPILE:-mipsel-linux-}"
TOP=$(pwd)/build


deps() {
	rm -rf ${TOP}/3rdparty
	mkdir -p ${TOP}/3rdparty/install/lib

	echo "import libimp"
	cd ${TOP}/3rdparty
	rm -rf ingenic-lib
	if [[ ! -d ingenic-lib ]]; then
	git clone --depth 1 https://github.com/gtxaspec/ingenic-lib

	case "$1" in
		T10)
			echo "use T10 libs"
			cp ingenic-lib/T10/lib/3.12.0/uclibc/4.7.2/* $TOP/3rdparty/install/lib
			;;
		T20)
			echo "use T20 libs"
			cp ingenic-lib/T20/lib/3.12.0/uclibc/4.7.2/* $TOP/3rdparty/install/lib
			;;
		T21)
			echo "use $1 libs"
			cp ingenic-lib/$1/lib/1.0.33/uclibc/5.4.0/* $TOP/3rdparty/install/lib
			;;
		T23)
			echo "use $1 libs"
			cp ingenic-lib/$1/lib/1.1.0/uclibc/5.4.0/* $TOP/3rdparty/install/lib
			;;
		T30)
			echo "use $1 libs"
			cp ingenic-lib/$1/lib/1.0.5/uclibc/5.4.0/* $TOP/3rdparty/install/lib
			;;
		T31)
			echo "use $1 libs"
			cp ingenic-lib/$1/lib/1.1.6/uclibc/5.4.0/* $TOP/3rdparty/install/lib
			;;
		*)
			echo "Unsupported or unspecified SoC model."
			;;
	esac
	fi

	cd ../

	echo "import libmuslshim"
	cd 3rdparty
	rm -rf ingenic-musl
	if [[ ! -d ingenic-musl ]]; then
	git clone --depth 1 https://github.com/gtxaspec/ingenic-musl
	cd ingenic-musl
	if [[ "$2" == "-static" ]]; then
		make CC="${CROSS_COMPILE}gcc" static
	else
		make CC="${CROSS_COMPILE}gcc"
	fi
	cp libmuslshim.* ../install/lib/
	fi
	cd ..
}

if [ $# -eq 0 ]; then
	echo "Usage: ./build.sh deps <platform> <-static>"
	echo "Platform: T20/T21/T23/T30/T31"
	exit 1
elif [[ "$1" == "deps" ]]; then
	echo "SOC: $2"
	deps $2 $3
fi
