// ffmpeg-libav-tutorial/mayhem/fuzz_hello.cpp
//
// libFuzzer harness for the "hello" target. It mirrors the decode path of the
// tutorial's 0_hello_world.c (demux -> find stream info -> open the first video
// decoder -> read packets -> decode frames) but drives it from the fuzzer's
// in-memory input via a custom AVIOContext instead of a file on disk.
//
// This replaces the previous file-input build of 0_hello_world.c, which was
// compiled without SanitizerCoverage (no -fsanitize=fuzzer-no-link) and could
// not be measured by Mayhem, and which also wrote .pgm files to CWD and could
// crash on a NULL fopen. This harness is instrumented (built with
// $LIB_FUZZING_ENGINE), performs no filesystem I/O, and is leak-clean per input.

#include <stdint.h>
#include <stddef.h>
#include <string.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/mem.h>
}

namespace {

struct BufferData {
    const uint8_t *ptr;
    size_t size;
    size_t pos;
};

int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    BufferData *bd = static_cast<BufferData *>(opaque);
    size_t left = bd->size - bd->pos;
    if (left == 0)
        return AVERROR_EOF;
    if (static_cast<size_t>(buf_size) > left)
        buf_size = static_cast<int>(left);
    memcpy(buf, bd->ptr + bd->pos, buf_size);
    bd->pos += buf_size;
    return buf_size;
}

int64_t seek_packet(void *opaque, int64_t offset, int whence)
{
    BufferData *bd = static_cast<BufferData *>(opaque);
    if (whence == AVSEEK_SIZE)
        return static_cast<int64_t>(bd->size);
    int64_t np;
    switch (whence & ~AVSEEK_FORCE) {
        case SEEK_SET: np = offset; break;
        case SEEK_CUR: np = static_cast<int64_t>(bd->pos) + offset; break;
        case SEEK_END: np = static_cast<int64_t>(bd->size) + offset; break;
        default: return -1;
    }
    if (np < 0 || static_cast<size_t>(np) > bd->size)
        return -1;
    bd->pos = static_cast<size_t>(np);
    return np;
}

void decode_streams(AVFormatContext *fmt)
{
    int video_stream_index = -1;
    const AVCodec *codec = NULL;

    for (unsigned i = 0; i < fmt->nb_streams; i++) {
        AVCodecParameters *par = fmt->streams[i]->codecpar;
        if (par->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_index < 0) {
            const AVCodec *c = avcodec_find_decoder(par->codec_id);
            if (c) {
                video_stream_index = static_cast<int>(i);
                codec = c;
            }
        }
    }
    if (video_stream_index < 0 || !codec)
        return;

    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    if (!ctx)
        return;

    if (avcodec_parameters_to_context(ctx, fmt->streams[video_stream_index]->codecpar) >= 0 &&
        avcodec_open2(ctx, codec, NULL) >= 0) {

        AVFrame *frame = av_frame_alloc();
        AVPacket *pkt = av_packet_alloc();
        if (frame && pkt) {
            int budget = 12; // bound work per input to keep throughput high
            while (budget-- > 0 && av_read_frame(fmt, pkt) >= 0) {
                if (pkt->stream_index == video_stream_index) {
                    if (avcodec_send_packet(ctx, pkt) >= 0) {
                        while (avcodec_receive_frame(ctx, frame) >= 0)
                            av_frame_unref(frame);
                    }
                }
                av_packet_unref(pkt);
            }
        }
        av_frame_free(&frame);
        av_packet_free(&pkt);
    }

    avcodec_free_context(&ctx);
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    // Reject large inputs fast (before any demux/decode work) so libFuzzer-grown
    // inputs cannot tank throughput; the seed corpus is a few KB, so a small cap
    // keeps per-exec cost bounded while still reaching the demux/decode path.
    if (size == 0 || size > (32u * 1024u))
        return 0;

    static bool inited = false;
    if (!inited) {
        av_log_set_level(AV_LOG_QUIET);
        inited = true;
    }

    BufferData bd = { data, size, 0 };

    const int avio_buf_size = 4096;
    unsigned char *avio_buf = static_cast<unsigned char *>(av_malloc(avio_buf_size));
    if (!avio_buf)
        return 0;

    AVIOContext *avio = avio_alloc_context(avio_buf, avio_buf_size, 0, &bd,
                                           read_packet, NULL, seek_packet);
    if (!avio) {
        av_free(avio_buf);
        return 0;
    }

    AVFormatContext *fmt = avformat_alloc_context();
    if (!fmt) {
        av_freep(&avio->buffer);
        avio_context_free(&avio);
        return 0;
    }
    fmt->pb = avio;
    fmt->flags |= AVFMT_FLAG_CUSTOM_IO; // we own avio; avformat must not free it

    if (avformat_open_input(&fmt, NULL, NULL, NULL) != 0) {
        // On failure avformat_open_input frees *fmt but leaves our custom pb.
        av_freep(&avio->buffer);
        avio_context_free(&avio);
        return 0;
    }

    if (avformat_find_stream_info(fmt, NULL) >= 0)
        decode_streams(fmt);

    avformat_close_input(&fmt); // CUSTOM_IO set -> does not touch avio
    av_freep(&avio->buffer);    // buffer may have been reallocated internally
    avio_context_free(&avio);
    return 0;
}
