#!/bin/sh
#
# Generate .config from configs/sedna-riscv64_defconfig.
#
# Usage:
#   ./config-sedna.sh              # via the pinned docker image (default)
#   ./config-sedna.sh --native     # run make on the host instead
#
set -e

cd "$(dirname "$0")"

DEFCONFIG=sedna-riscv64_defconfig
IMAGE="${BUILDROOT_DOCKER_IMAGE:-buildroot/base:20200814.2228}"

if [ "$1" = "--native" ]; then
	exec make "$DEFCONFIG"
fi

if ! command -v docker >/dev/null 2>&1; then
	echo "docker not found; falling back to a native make (see --native)" >&2
	exec make "$DEFCONFIG"
fi

mkdir -p output/.docker-home
exec docker run --rm \
	-u "$(id -u):$(id -g)" \
	-v "$PWD:/build" \
	-w /build \
	-e HOME=/build/output/.docker-home \
	"$IMAGE" \
	make "$DEFCONFIG"
