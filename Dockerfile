FROM python:trixie

RUN apt update && apt install -y build-essential cmake

COPY app /app

RUN pip3 install python-chess

RUN cd /app/src/linux-windows/build &&  \
    cmake .. && \
    make -j4 Dog

WORKDIR /app/src

CMD ./gen-train-data.py -e ./linux-windows/build/Dog
