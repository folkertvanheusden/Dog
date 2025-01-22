To be able to generate a strong NNUE network, Dog needs a large set of training data.
You can help generating that!
If you have docker installed on your computer, you can do the following:

	git clone --recursive https://github.com/folkertvanheusden/Dog.git
	cd Dog
	./dockerrun

That's it! Interrupt the program whenever you like.


regards

Folkert van Heusden

p.s. it's also possible without docker:

    sudo apt update
    sudo apt install git build-essential python3-pip clang-14 cmake python3.11-venv
    git clone --recursive https://github.com/folkertvanheusden/Dog.git
    cd Dog/app/src/linux-windows/build/
    CXX=clang++-14 CC=clang-14 cmake ..
    make -j4
    cd ../..                  #  <-- go to Dog/app/src
    python3 -m venv venv
    . venv/bin/activate
    pip3 install python-chess
    ./gen-train-data.py -e ./linux-windows/build/Dog -f backup.dat -t 4

...where '-t 4' selects the number of threads to use.
