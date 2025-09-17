FROM ran-base:latest
ARG USERNAME=root
ARG USER_UID=1000
ARG USER_GID=1000

# More developer tools can be added here
RUN apt update && apt install -y vim sudo wget meson ninja-build libnuma-dev ccache

# Create the user
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

## Download and build DPDK
RUN cd /oai-ran && \
    wget http://fast.dpdk.org/rel/dpdk-20.11.9.tar.xz && \
    tar -xvf dpdk-20.11.9.tar.xz && \
    cd dpdk-stable-20.11.9 && \
    meson build && \
    ninja -C build && \
    ninja install -C build

## Build Fronthaul library
COPY cmake_targets/tools/oran_fhi_integration_patches/F/oaioran_F.patch /oaioran_F.patch
RUN git clone https://gerrit.o-ran-sc.org/r/o-du/phy.git /opt/phy && \
    cd /opt/phy && \
    git checkout oran_f_release_v1.0 &&\
    git apply /oaioran_F.patch && \
    cd /opt/phy/fhi_lib/lib && \
    WIRELESS_SDK_TOOLCHAIN=gcc RTE_SDK=/oai-ran/dpdk-stable-20.11.9/ XRAN_DIR=/opt/phy/fhi_lib make XRAN_LIB_SO=1
ENV xran_LOCATION=/opt/phy/fhi_lib/lib/

# This is a temporary solution to issue with apt: one of the installed packages creates a group that may
# not be present in the host: remove the group entry to allow apt usage in container
RUN dpkg-statoverride --remove /usr/bin/uml_net

# Override ccache directory to avoid permission issues
ENV CCACHE_DIR=/home/$USERNAME/.cache/ccache-oai-dev-env

USER $USERNAME
