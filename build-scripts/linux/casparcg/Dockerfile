ARG casparcg_from="casparcg_base"
FROM ${casparcg_from}

WORKDIR /opt/src
# COPY ./ /opt/src/

ARG BUILD_ARCHIVE_NAME="CasparCG Server"
ARG BUILD_PARALLEL_THREADS=4
ARG GIT_HASH
ENV BUILD_ARCHIVE_NAME="${BUILD_ARCHIVE_NAME}" \
    BUILD_PARALLEL_THREADS="${BUILD_PARALLEL_THREADS}" \
    GIT_HASH="${GIT_HASH}"


# RUN cd build-scripts && ./build-linux.sh