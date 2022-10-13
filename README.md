Dog was written by Folkert van Heusden.
It is (strongly) based on the chess program Micah (altought slightly different evaluation and different threading (lazy limited smp)).
Licensed under the MIT license.


Create a virtual env:

	virtualenv --python="/usr/bin/python3.7" test

When you want to change options (first create the virtual env):
	cd app

	. test/bin/activate

	PATH=~/.platformio/packages/toolchain-xtensa-esp32/bin:$PATH ~/.platformio/packages/framework-espidf/tools/idf.py menuconfig

	cp sdkconfig sdkconfig.esp32

To juist build the program and upload it to a wemos32 mini:

	cd app
	pio run -t upload

To build it for Linux:

	cd app/src/linux-windows
	mkdir build
	cd build
	cmake ..
	make

To build it for windows:

	cd app/src/linux-windows
	mkdir buildwindows
	cd buildwindows
	cmake -DCMAKE_TOOLCHAIN_FILE=../mingw64.cmake ..
	make
