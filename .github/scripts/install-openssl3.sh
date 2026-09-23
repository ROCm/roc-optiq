#!/bin/bash
# Install a side-by-side OpenSSL 3 for images whose system OpenSSL is 1.1
# (manylinux_2_28 / Rocky 8). The system library stays in place; CMake is
# pointed at this prefix with OPENSSL_ROOT_DIR.
set -euo pipefail

PREFIX="/opt/openssl-3"
VERSION="3.5.8"
TARBALL="openssl-${VERSION}.tar.gz"
URL="https://github.com/openssl/openssl/releases/download/openssl-${VERSION}/${TARBALL}"
SHA256="a8f84a39918ec6415ce765d9b429d313ba97b8143169c172e734b9514464f5b2"

if [ -x "${PREFIX}/bin/openssl" ] && "${PREFIX}/bin/openssl" version | grep -q "OpenSSL ${VERSION}"; then
    echo "OpenSSL ${VERSION} already installed at ${PREFIX}"
    "${PREFIX}/bin/openssl" version
    exit 0
fi

workdir="$(mktemp -d)"
trap 'rm -rf "${workdir}"' EXIT
cd "${workdir}"

curl -fsSL -o "${TARBALL}" "${URL}"
echo "${SHA256}  ${TARBALL}" | sha256sum -c -

tar -xzf "${TARBALL}"
cd "openssl-${VERSION}"
./config --prefix="${PREFIX}" --libdir=lib --openssldir="${PREFIX}/ssl" shared -fPIC
make -j"$(nproc)"
make install_sw

"${PREFIX}/bin/openssl" version
ls -l "${PREFIX}/lib"/libssl.so*
