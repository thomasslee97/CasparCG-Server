SHELL := /bin/bash
DATE := $(shell date +'%Y%m%d%H%M')
GIT_HASH := $(shell git rev-parse --verify --short HEAD)
CASPARCG_VERSION := $(shell git branch | sed -n -e 's/^\* \(.*\)/\1/p')

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

build: build_base build_casparcg

build-base:
	pushd ../ >/dev/null && docker build \
	--build-arg http_proxy="${http_proxy}" \
	--build-arg https_proxy="${https_proxy}" \
	--build-arg GIT_HASH="${GIT_HASH}" \
	-f build-scripts/linux/base/Dockerfile \
	-t "casparcg_base:${CASPARCG_VERSION}" \
	$${PWD}

build-casparcg:
	pushd linux/casparcg/ >/dev/null && docker build \
	--build-arg casparcg_from="casparcg_base:${CASPARCG_VERSION}" \
	--build-arg GIT_HASH="${GIT_HASH}" \
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
