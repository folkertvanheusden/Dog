To be able to generate a strong NNUE network, Dog needs a large set of training data.
You can help generating that!
Feel free to run it whenever you like and how long you like.


There are 2 ways:

* DOCKER

```
    git clone --recursive https://github.com/folkertvanheusden/Dog.git
    cd Dog
    git checkout TRAINER
    ./dockerrun
```

That's it! Interrupt the program whenever you like.
To completely delete the docker you created for Dog, enter:

```
    docker rm -f dog
```


* WITHOUT DOCKER

This is the recipe for any Debian/Ubuntu based system:

```
    sudo apt update
    sudo apt install clang cmake git build-essential libstdc++-12-dev screen python3-pip python3-venv
    git clone --recursive https://github.com/folkertvanheusden/Dog.git
    cd Dog/app/src/linux-windows/build/
    git checkout TRAINER
    CXX=clang++-14 cmake ..
    make -j4
    cd ../..
    python3 -m venv venv
    . venv/bin/activate
    pip install python-chess
    ./gen-train-data.py -e linux-windows/build/Dog
```

*NOTE: for systems with large amounts of threads, do not forget to run ulimit -n 8192 ! (or bigger)

Interrupt the program whenever you want.
