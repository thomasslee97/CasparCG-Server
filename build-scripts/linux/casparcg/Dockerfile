ARG casparcg_from="casparcg_base"
FROM ${casparcg_from}

ARG BUILD_ARCHIVE_NAME="CasparCG Server"
ARG BUILD_PARALLEL_THREADS=1
ARG GIT_HASH
ENV BUILD_ARCHIVE_NAME="${BUILD_ARCHIVE_NAME}" \
    BUILD_PARALLEL_THREADS="${BUILD_PARALLEL_THREADS}" \
    GIT_HASH="${GIT_HASH}"

RUN cd build-scripts && ./build-linux.sh