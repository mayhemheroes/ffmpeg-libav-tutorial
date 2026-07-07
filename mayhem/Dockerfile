# syntax=docker/dockerfile:1.7
#
# ffmpeg-libav-tutorial commit image — builds on ghcr.io/mayhemheroes/base with FFmpeg dev libs.

FROM ghcr.io/mayhemheroes/base:20260621

ARG COMMIT
ARG REPO_URL
ARG SOURCE_DATE_EPOCH

ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

# FFmpeg dev packages are installed here (online layer) so mayhem/build.sh stays air-gapped.
USER root
RUN ([ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true) ; \
    apt-get update && apt-get install -yq --no-install-recommends \
        libavcodec-dev libavformat-dev libavfilter-dev libavdevice-dev \
        libswresample-dev libswscale-dev libavutil-dev \
 && rm -rf /var/lib/apt/lists/* ; \
    mkdir -p /opt/ffmpeg && \
    ln -sfn /usr/include /opt/ffmpeg/include && \
    ln -sfn /usr/lib/x86_64-linux-gnu /opt/ffmpeg/lib
USER mayhem

WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN bash mayhem/build.sh

# Seed corpus for the hello (demux/decode) target — a small streamable MPEG-TS
# prefix. Mayhem auto-discovers /testsuite as the run seed corpus.
USER root
RUN mkdir -p /testsuite && cp /mayhem/mayhem/corpus_hello/* /testsuite/ 2>/dev/null || true
USER mayhem

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh — repo ships no functional tests"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
