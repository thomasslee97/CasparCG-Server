FROM buildpack-deps:trusty

ARG maintainer=mark.himsley@bbc.co.uk
LABEL maintainer=${maintainer}

ENV http_proxy=${http_proxy} \
    https_proxy=${https_proxy}

# The following best practices page says do not use 'apt-get -y upgrade'
# It also says to execute 'rm -rf /var/lib/apt/lists/*' at the end of the install
# https://docs.docker.com/engine/userguide/eng-image/dockerfile_best-practices/#apt-get
RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get -y install \
	cmake \
	time \
	lsb-release \
	libglew-dev \
	libfreeimage-dev \
	libtbb-dev \
	libsndfile1-dev \
	libopenal-dev \
	libxcursor-dev \
	libxinerama-dev \
	libxi-dev \
	libnih-dbus-dev \
	libsfml-dev \
	libxrandr-dev \
	libfdk-aac-dev \
	libjpeg-dev \
	libsndfile1-dev \
	libudev-dev \
	libglu1-mesa-dev \
	libharfbuzz0b \
	libpangoft2-1.0-0 \
	libcairo2 \
	libv4l-0 \
	libraw1394-11 \
	libavc1394-0 \
	libiec61883-0 \
	libxtst6 \
	libnss3 \
	libnspr4 \
	libgconf-2-4 \
	libasound2 \
	libopus-dev \
	libgsm1-dev \
	libmodplug-dev \
	libvpx-dev \
	libass-dev \
	libbluray-dev \
	libfribidi-dev \
	libgmp-dev \
	libmp3lame-dev \
	libopencore-amrnb-dev \
	libopencore-amrwb-dev \
	librtmp-dev \
	libtheora-dev \
	libx264-dev \
	libxvidcore-dev \
	libxss1 \
	libgtk2.0-0 \
	libssl1.0.0 \
 && rm -rf /var/lib/apt/lists/*

# only available in bionic
#	libglfw3-dev \
#	libx265-dev \
# Conflicts with gnutls-dev
# 	libgnutls28-dev \

WORKDIR /opt/casparcg

COPY --from=casparcg/ffmpeg:master-acdea9e-2 /opt/ffmpeg /opt/ffmpeg

COPY ./ /opt/casparcg/
