#! /bin/sh

cd /home/folkert/Projects/esp32chesstest/app/src/android/DogChessEngine

#~/bin/android-ndk/ndk-build
/home/folkert/Android/Sdk/ndk/25.1.8937393/ndk-build

adb -s emulator-5554 push /home/folkert/Projects/esp32chesstest/app/src/android/DogChessEngine/libs/x86_64/Dog /data/local

for i in libs/* ; do mv $i/Dog $i/libDog.so ; done

(cd libs/ && tar cf - *) | (cd /home/folkert/AndroidStudioProjects/Dog/app/src/main/jniLibs && tar xvf -)
