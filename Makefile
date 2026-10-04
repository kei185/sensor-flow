FORMAT_FILE := $(shell find src -name "*.cpp" -or -name "*.hpp" -type f)

KERNEL := $(shell uname)
ifeq ($(KERNEL), Darwin)
	PORT:=$(shell ls /dev/cu.usb* )
endif
ifeq ($(KERNEL), Linux)
	PORT:=$(shell ls /dev/ttyACM* )
endif

.PHONY:init debug build release run clean

init:
	cmake -S . -B build
	ln -s  build/compile_commands.json compile_commands.json

build:
	cmake -S . -B build
	cmake --build build --verbose

debug:
	cmake -DCMAKE_BUILD_TYPE=Debug -S . -B build
	cmake --build build --verbose

release:
	cmake \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=./install \
		-S . -B release
	cmake --build release --verbose
	cmake --install release --verbose

run:
	build/main ${PORT}


format:
	clang-format -i ${FORMAT_FILE}

clean:
	rm -rf build release install debug
