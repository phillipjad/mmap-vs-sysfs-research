#!/usr/bin/env bash

# Set e to instantly fail when errors occurs on commands
# Set u to make unset variables an error
set -eu

### Global Variables
ROOT_DIR=$(dirname "$(readlink -f "$0")")
CMAKE_ARGS=("-DCMAKE_TOOLCHAIN_FILE=${ROOT_DIR}/cmake/arch.cmake")
ARCH="x86_64"
BUILD_TYPE="Release"
CLEAR_DIR=0
PWM_BACKEND="MMAP"

### Functions

help() {
	echo "Usage: $(basename "$0") [options]"
	echo ""
	echo "Options:"
	echo "  -h, --help     Show this help message and exit with status code 0"
	echo "  -a, --arch     Set the architecture to build (x86_64, arm, aarch64, ppc)"
	echo "  -d, --debug    Enable debug build"
	echo "  -c, --clear    Remove the cmake build directory"
	echo "  -m, --mmap     Use mmap-based PWM control (default)"
	echo "  -s, --sysfs    Use sysfs-based PWM control"
	echo ""
	echo "Examples:"
	echo "  ./build.sh"
	echo "  ./build.sh -a aarch64 -d -c -m"
	echo "  ./build.sh -a x86_64 -s"
	exit 0
}

# Parse command-line arguments
parse_arguments() {
	while [[ $# -gt 0 ]]; do
		case "$1" in
		-a | --arch)
			ARCH="$2"
			shift 2
			;;
		-d | --debug)
			BUILD_TYPE="Debug"
			shift 1
			;;
		-h | --help)
			help
			;;
		-c | --clear)
			CLEAR_DIR=1
			shift 1
			;;
		-m | --mmap)
			if [ "$PWM_BACKEND" = "SYSFS" ]; then
				echo "Error: Cannot specify both -m and -s" >&2
				exit 1
			fi
			PWM_BACKEND="MMAP"
			shift 1
			;;
		-s | --sysfs)
			if [ "$PWM_BACKEND" = "MMAP" ] && [ "$1" = "--sysfs" ]; then
				# Only error if -m was explicitly set
				echo "Error: Cannot specify both -m and -s" >&2
				exit 1
			fi
			PWM_BACKEND="SYSFS"
			shift 1
			;;
		*)
			echo "Error: Unknown argument '$1'" >&2
			exit 1
			;;
		esac
	done
}

### Main script logic

parse_arguments "$@"

if [ $CLEAR_DIR -eq 1 ]; then
	printf "Clearing build directory: %s\n" "$ROOT_DIR/build"
	rm -rf "$ROOT_DIR/build"
fi

case $ARCH in
"x86_64")
	CMAKE_ARGS+=("-DCMAKE_SYSTEM_PROCESSOR=$ARCH")
	;;
"arm")
	CMAKE_ARGS+=("-DCMAKE_SYSTEM_PROCESSOR=$ARCH")
	;;
"aarch64")
	CMAKE_ARGS+=("-DCMAKE_SYSTEM_PROCESSOR=$ARCH")
	;;
"ppc" | "powerpc")
	CMAKE_ARGS+=("-DCMAKE_SYSTEM_PROCESSOR=$ARCH")
	;;
*)
	echo "Error: Unknown compilation architecture '$ARCH' provided" >&2
	exit 1
	;;
esac

echo "Build Type: $BUILD_TYPE"
CMAKE_ARGS+=("-DCMAKE_BUILD_TYPE=$BUILD_TYPE")

# Set PWM backend
if [ "$PWM_BACKEND" = "SYSFS" ]; then
	CMAKE_ARGS+=("-DUSE_SYSFS=1")
	echo "PWM Backend: SYSFS"
else
	CMAKE_ARGS+=("-DUSE_MMAP=1")
	echo "PWM Backend: MMAP (default)"
fi

# Run cmake configure with the build directory
cmake -B build "${CMAKE_ARGS[@]}"

# Build the project
cmake --build build
