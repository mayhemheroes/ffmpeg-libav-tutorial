FROM leandromoreira/ffmpeg-devel:4.4 as builder

COPY . /files
RUN gcc -L/opt/ffmpeg/lib -I/opt/ffmpeg/include/ /files/0_hello_world.c \
    -lavcodec -lavformat -lavfilter -lavdevice -lswresample -lswscale -lavutil \
    -o /files/build/hello

RUN apt remove --purge --auto-remove cmake
RUN apt update && \
    DEBIAN_FRONTEND=noninteractive apt install -y software-properties-common lsb-release && \
    apt clean all
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y wget
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
RUN apt-add-repository "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main"
RUN apt update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y cmake

WORKDIR /files/build
RUN cmake .. -DFFMPEG_DEV_ROOT=/opt/ffmpeg/
RUN make

RUN DEBIAN_FRONTEND=noninteractive apt-get install -y clang
RUN clang++ /files/mayhem/fuzz_logging.cpp -fsanitize=fuzzer,address -L /files/build/lib/ -lvideo_debug -o /fuzz

FROM ubuntu:20.04 as package
RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y libavcodec-dev libavformat-dev libavutil-dev
COPY --from=builder /files/build/hello /
COPY --from=builder /fuzz /fuzz_logging
