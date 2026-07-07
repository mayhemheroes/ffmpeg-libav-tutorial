#!/usr/bin/env bash
#
# ffmpeg-libav-tutorial/mayhem/build.sh — build hello (libFuzzer) + logging (libFuzzer) targets.
#
# FFmpeg dev libraries and cmake are installed by mayhem/Dockerfile (offline at PATCH tier).
# FFMPEG_DEV_ROOT=/opt/ffmpeg is a small prefix layout (symlinked to Debian multiarch paths)
# so include/lib paths match the old ffmpeg-devel image layout.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

FFMPEG_ROOT="${FFMPEG_DEV_ROOT:-/opt/ffmpeg}"
FFMPEG_CFLAGS="-I${FFMPEG_ROOT}/include"
FFMPEG_LIBS="-L${FFMPEG_ROOT}/lib -lavcodec -lavformat -lavfilter -lavdevice -lswresample -lswscale -lavutil"

CLANG_RES="$(clang++ -print-resource-dir)"
FUZZER_INC="-I${CLANG_RES}/include"

# Apply additive FFmpeg-7 API compatibility patches (idempotent for offline re-runs).
git config --global --add safe.directory "$SRC" 2>/dev/null || true
for p in "$SRC"/mayhem/patches/*.patch; do
    [ -f "$p" ] || continue
    if git apply --reverse --check "$p" 2>/dev/null; then
        echo "patch already applied, skipping: $(basename "$p")"
    else
        echo "applying patch: $(basename "$p")"
        git apply "$p"
    fi
done

# ---------------------------------------------------------------------------
# 1) hello — libFuzzer harness driving the tutorial decode path over the
#    fuzzer's in-memory input (mayhem/fuzz_hello.cpp). Built with the fuzzing
#    engine so the binary carries SanitizerCoverage instrumentation.
# ---------------------------------------------------------------------------
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE $FUZZER_INC \
    "$SRC/mayhem/fuzz_hello.cpp" \
    $FFMPEG_CFLAGS $FFMPEG_LIBS -lstdc++ \
    -o /mayhem/hello

# ---------------------------------------------------------------------------
# 2) video_debug library (sanitized) — only the logging helpers the harness calls.
# ---------------------------------------------------------------------------
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/video_debugging.c" \
    $FFMPEG_CFLAGS -o /tmp/video_debug.o
rm -f /tmp/libvideo_debug.a
ar rcs /tmp/libvideo_debug.a /tmp/video_debug.o

# ---------------------------------------------------------------------------
# 3) logging libFuzzer harness + standalone reproducer.
# ---------------------------------------------------------------------------
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE $FUZZER_INC \
    "$SRC/mayhem/fuzz_logging.cpp" \
    /tmp/libvideo_debug.a $FFMPEG_CFLAGS $FFMPEG_LIBS -lstdc++ \
    -o /mayhem/fuzz_logging

$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o /tmp/standalone_main.o
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS $FUZZER_INC \
    /tmp/standalone_main.o "$SRC/mayhem/fuzz_logging.cpp" \
    /tmp/libvideo_debug.a $FFMPEG_CFLAGS $FFMPEG_LIBS -lstdc++ \
    -o /mayhem/fuzz_logging-standalone

echo "build.sh OK: $(ls -1 /mayhem/hello /mayhem/fuzz_logging /mayhem/fuzz_logging-standalone)"
