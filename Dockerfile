FROM python:3.11

RUN apt update && apt install -y build-essential cmake clang-14 libstdc++-12-dev

COPY app /app

RUN pip3 install python-chess

RUN git checkout TRAINER

RUN cd /app/src/linux-windows/build &&  \
    CXX=clang++-14 CC=clang-14 cmake .. && \
    make -j4

WORKDIR /app/src

CMD ./gen-train-data.py -e ./linux-windows/build/Dog
