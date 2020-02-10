ARG casparcg_from="casparcg_base"
FROM ${casparcg_from}

WORKDIR /opt/src
COPY ./ /opt/src/

ARG BUILD_ARCHIVE_NAME="CasparCG Server"
ARG BUILD_PARALLEL_THREADS=4
ARG GIT_HASH
ENV BUILD_ARCHIVE_NAME="${BUILD_ARCHIVE_NAME}" \
    BUILD_PARALLEL_THREADS="${BUILD_PARALLEL_THREADS}" \
    GIT_HASH="${GIT_HASH}"


RUN cd build-scripts && ./build-linux.sh

# Find a better way to copy deps
RUN cd /opt/src/build/shell && /opt/src/shell/copy_deps.sh casparcg ./
RUN cd /opt/src/build/shell && rm -R CMakeFiles casparcg_pch
RUN cp /opt/src/shell/run_docker.sh ./

FROM nvidia/opengl:1.0-glvnd-devel-ubuntu18.04
	COPY --from=build-casparcg /opt/src/build/shell /opt/casparcg

RUN set -ex; \
    apt-get update; \
    apt-get install -y --no-install-recommends \
        libc++1 \
        libnss3 \
        fontconfig \
    ; \
    rm -rf /var/lib/apt/lists/*

WORKDIR /opt/casparcg

CMD ["./run_docker.sh"]
