FROM mcr.microsoft.com/devcontainers/cpp:0-ubuntu-22.04

ARG REINSTALL_CMAKE_VERSION_FROM_SOURCE="3.31.7"
ARG REINSTALL_LLVM_VERSION="20"

# Optionally install the cmake for vcpkg
COPY ./reinstall-cmake.sh /tmp/

RUN if [ "${REINSTALL_CMAKE_VERSION_FROM_SOURCE}" != "none" ]; then \
        chmod +x /tmp/reinstall-cmake.sh && /tmp/reinstall-cmake.sh ${REINSTALL_CMAKE_VERSION_FROM_SOURCE}; \
    fi \
    && rm -f /tmp/reinstall-cmake.sh

# [Optional] Uncomment this section to install additional packages.
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends lsb-release wget software-properties-common gnupg

COPY ./reinstall-llvm.sh /tmp/

RUN if [ "${REINSTALL_LLVM_VERSION}" != "none" ]; then \
        chmod +x /tmp/reinstall-llvm.sh && /tmp/reinstall-llvm.sh ${REINSTALL_LLVM_VERSION}; \
    fi \
    && rm -f /tmp/reinstall-llvm.sh
