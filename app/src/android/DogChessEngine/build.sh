#! /bin/sh

cd /home/folkert/Projects/esp32chesstest/app/src/android/DogChessEngine

#~/bin/android-ndk/ndk-build
/home/folkert/Android/Sdk/ndk/25.1.8937393/ndk-build

(cd libs/ && tar cf - *) | (cd /home/folkert/AndroidStudioProjects/Dog/app/src/main/jniLibs && tar xvf -)

adb -s emulator-5554 push /home/folkert/Projects/esp32chesstest/app/src/android/DogChessEngine/libs/x86_64/Dog /data/local
