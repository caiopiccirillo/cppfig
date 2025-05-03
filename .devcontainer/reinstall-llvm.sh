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
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
./llvm.sh ${LLVM_VERSION} all

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
