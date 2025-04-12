To be able to generate a strong NNUE network, Dog needs a large set of training data.
You can help generating that!

There are 2 ways:

* DOCKER

    git clone --recursive https://github.com/folkertvanheusden/Dog.git
    cd Dog
    ./dockerrun

That's it! Interrupt the program whenever you like.


* WITHOUT DOCKER

This is the recipe for any Debian/Ubuntu based system:

    sudo apt update
    sudo apt install clang cmake git build-essential libstdc++-12-dev screen python3-pip python3-venv
    git clone --recursive https://github.com/folkertvanheusden/Dog.git
    cd Dog/app/src/linux-windows/build/
    CXX=clang++-14 cmake ..
    make -j4
    cd ../..
    python3 -m venv venv
    . venv/bin/activate
    pip install python-chess
    ./gen-train-data.py -e linux-windows/build/Dog-native
