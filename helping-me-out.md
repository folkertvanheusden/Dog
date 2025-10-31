Help me out
===========

To be able to generate a strong NNUE network, Dog needs a large set of training data.
You can help generating that!
Feel free to run it whenever you like and how long you like.

Please always make a clean clone of the github-repository: delete a previous clone.
That way we're certain that e.g. libchess is always the latest version!


There are 2 ways:

DOCKER
------

```shell
git clone --recursive https://github.com/folkertvanheusden/Dog.git
cd Dog
git checkout TRAINER
./dockerrun
```

That's it! Interrupt the program whenever you like.

To completely stop and delete the docker you created for Dog, enter:

```shell
docker rm -f dog
```


WITHOUT DOCKER
--------------

This is the recipe for Linux systems. It has 2 steps that depend on the distribution you use.

### step 1

Debian/Ubuntu (DEB based systems):

```shell
sudo apt update
sudo apt install clang cmake git build-essential python3-pip python3-venv
```

Fedora (RPM based systems):

```shell
sudo dnf install cmake git g++ python3-pip
```

### step 2

```shell
git clone --recursive https://github.com/folkertvanheusden/Dog.git
cd Dog/app/src/linux-windows/build/
git checkout TRAINER
cmake ..
make -j4 Dog
cd ../..
python3 -m venv venv
. venv/bin/activate
pip install python-chess
./gen-train-data.py -e linux-windows/build/Dog
```

*NOTE: for systems with large amounts of threads, do not forget to run ulimit -n 8192 ! (or bigger)

Interrupt the program whenever you want.
