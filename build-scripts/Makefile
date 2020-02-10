SHELL := /bin/bash
DATE := $(shell date +'%Y%m%d%H%M')
GIT_HASH := $(shell git rev-parse --verify --short HEAD)
CASPARCG_VERSION := $(shell git branch | sed -n -e 's/^\* \(.*\)/\1/p' | sed -r 's/[^a-z0-9]+/-/gi')

define HELP
targets:
	build
		runs build_base and build_casparcg below

	build-base
		makes an image containing all of the required source code and libraries

	build-casparcg
		builds CasparCG in a Docker container from the source code above

	clean-exited
		removes ALL containers which are not running

	clean-dangling
		removes ALL images which are not referenced
endef
export HELP
default:
	@echo "$${HELP}"

build: build-base build-casparcg

build-base:
	echo ${CASPARCG_VERSION}
	pushd ../ >/dev/null && docker build \
	-f build-scripts/linux/base/Dockerfile \
	-t "casparcg_base:${CASPARCG_VERSION}" \
	build-scripts/linux/base

build-casparcg:
	pushd ../ >/dev/null && docker build \
	--build-arg casparcg_from="casparcg_base:${CASPARCG_VERSION}" \
	--build-arg GIT_HASH="${GIT_HASH}" \
	-f build-scripts/linux/casparcg/Dockerfile \
	-t "casparcg_build:${CASPARCG_VERSION}" \
	$${PWD}

clean: clean-exited clean-dangling

clean-exited:
	set -e ;\
	CONTAINERS=$$(docker ps -aq --filter "status=exited") ;\
	if [ "Z$$CONTAINERS" != "Z" ]; then \
		echo $$CONTAINERS | xargs docker rm -v ;\
	fi

clean-dangling:
	set -e ;\
	IMAGES=$$(docker images -q --filter "dangling=true") ;\
	if [ "Z$$IMAGES" != "Z" ]; then \
		echo $$IMAGES | xargs docker rmi ;\
	fi
