FROM leandromoreira/ffmpeg-devel:4.4 as builder

COPY . /files
RUN gcc -L/opt/ffmpeg/lib -I/opt/ffmpeg/include/ /files/0_hello_world.c \
    -lavcodec -lavformat -lavfilter -lavdevice -lswresample -lswscale -lavutil \
    -o /files/build/hello

FROM ubuntu:20.04 as package
RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y libavcodec-dev libavformat-dev libavutil-dev
COPY --from=builder /files/build/hello /
