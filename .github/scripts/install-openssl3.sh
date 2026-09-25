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

# RPM distributions split core Perl modules out of the perl package.
# OpenSSL documents perl-core for this: Configure needs IPC::Cmd, and the
# generated Makefile also uses Time::Piece. Text::Template ships inside
# the OpenSSL tarball. Test::More is only required to run the test suite,
# which this script does not.
if ! perl -MIPC::Cmd -MTime::Piece -e 'exit 0' 2>/dev/null; then
    echo "Installing perl-core"
    if command -v dnf >/dev/null 2>&1; then
        dnf install -y perl-core
    elif command -v yum >/dev/null 2>&1; then
        yum install -y perl-core
    else
        echo "perl-core is required to build OpenSSL ${VERSION}" >&2
        exit 1
    fi
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
