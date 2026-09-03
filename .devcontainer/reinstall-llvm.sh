#!/usr/bin/env bash
set -e

LLVM_VERSION=${1:-"none"}

if [ "${LLVM_VERSION}" = "none" ]; then
    echo "No LLVM version specified, skipping LLVM reinstallation"
    exit 0
fi

echo "Installing LLVM..."
apt-get -y purge --auto-remove "llvm*"
mkdir -p /opt/llvm
cd /opt/llvm

# apt.llvm.org publishes AAAA records, but the Docker build network has no IPv6
# route. wget picks A or AAAA depending on how the name resolves that time, so
# the build fails at random with either
#
#   Connecting to apt.llvm.org|2a04:4e42:94::561|:443... failed:
#   Network is unreachable.                                        (exit 4)
#
# or, when it is llvm.sh's own HEAD check that draws the AAAA record, the
# misleading
#
#   [error] Distribution 'ubuntu' in version '22.04.2 LTS (Jammy Jellyfish)'
#           is not supported by this script.                        (exit 2)
#
# even though the repository is perfectly reachable over IPv4. Pin wget to IPv4
# globally so llvm.sh inherits it too, and pin apt the same way.
echo 'inet4_only = on' >> /etc/wgetrc
echo 'Acquire::ForceIPv4 "true";' > /etc/apt/apt.conf.d/99force-ipv4

# Retry anyway: the download is still a network call, and one bad response
# should not cost a whole image build.
attempts=5
for attempt in $(seq 1 ${attempts}); do
    rm -f llvm.sh

    if wget --tries=3 --timeout=30 https://apt.llvm.org/llvm.sh \
        && chmod +x llvm.sh \
        && ./llvm.sh "${LLVM_VERSION}" all; then
        break
    fi

    if [ "${attempt}" -eq "${attempts}" ]; then
        echo "Failed to install LLVM ${LLVM_VERSION} after ${attempts} attempts" >&2
        exit 1
    fi

    delay=$((attempt * 15))
    echo "LLVM install attempt ${attempt}/${attempts} failed, retrying in ${delay}s..." >&2
    sleep "${delay}"
done

llvm_dir="/usr/lib/llvm-${LLVM_VERSION}/bin"
usr_local_dir="/usr/local/bin"

# Loop through each file in the input directory
for file in "$llvm_dir"/*
do
    # Check if the file is a binary
    if [[ -x "$file" && -f "$file" ]]; then
        # Get the file name and create the symbolic link in the output directory
        file_name=$(basename "$file")
        ln -s "$file" "$usr_local_dir/$file_name"
    fi
done
