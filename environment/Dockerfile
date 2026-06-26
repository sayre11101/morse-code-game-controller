FROM public.ecr.aws/docker/library/ubuntu:24.04@sha256:0d39fcc8335d6d74d5502f6df2d30119ff4790ebbb60b364818d5112d9e3e932

ENV DEBIAN_FRONTEND=noninteractive
ENV PIP_BREAK_SYSTEM_PACKAGES=1

ENV ZEPHYR_BASE=/zephyrproject/zephyr
ENV ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk
ENV ZEPHYR_SDK_VERSION=0.16.5

# ---------------------------------------------------------
# 1. OS-Level Dependencies & C Compilers
# ---------------------------------------------------------
# OPTIMIZATIONS: --no-install-recommends, removed ccache, apt-get clean
RUN apt-get update && apt-get install -y --no-install-recommends \
    git cmake ninja-build gperf dfu-util device-tree-compiler wget \
    python3 python3-dev python3-pip python3-setuptools python3-wheel python-is-python3 \
    xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1 \
    tmux asciinema curl ca-certificates \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* /var/cache/apt/archives/* /tmp/* /var/tmp/*

# ---------------------------------------------------------
# 2. install pytest
# ---------------------------------------------------------
RUN pip3 install --no-cache-dir pytest==8.4.1 pytest-json-ctrf==0.3.5

# ---------------------------------------------------------
# 4. Download and Install the Zephyr SDK (MINIMAL VERSION)
# ---------------------------------------------------------
# OPTIMIZATIONS: Minimal x86_64 toolchain only, clear /tmp/ after install
RUN wget -q "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz" && \
    tar -xf zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz -C /opt && \
    mv /opt/zephyr-sdk-${ZEPHYR_SDK_VERSION} /opt/zephyr-sdk && \
    rm zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz && \
    cd /opt/zephyr-sdk && \
    yes | ./setup.sh -t x86_64-zephyr-elf -h -c && \
    rm -rf /tmp/* /var/tmp/*

# ---------------------------------------------------------
# 5. Initialize Zephyr Workspace & Install ITS Requirements
# ---------------------------------------------------------
# OPTIMIZATIONS: manifest.group-filter "-hal" blocks vendor hardware code. Clear pip caches.
RUN git config --global user.email "agent@example.com" && \
    git config --global user.name "AI Agent" && \
    pip3 install --no-cache-dir west==1.2.0 && \
    west init -m https://github.com/zephyrproject-rtos/zephyr --mr v3.7.0 /zephyrproject && \
    cd /zephyrproject && \
    west config manifest.group-filter "-hal" && \
    west update --narrow --fetch-opt=--depth=1 && \
    pip3 install --no-cache-dir -r zephyr/scripts/requirements.txt && \
    rm -rf /root/.cache/pip /tmp/* /var/tmp/*

# ---------------------------------------------------------
# 6. Setup Agent Workspace
# ---------------------------------------------------------
RUN mkdir -p /app
WORKDIR /app

# Copy the core application files
COPY main.c /app/
COPY main.c /app/main.c.orig
COPY adxl345.pdf /app/
COPY morse-code-sheet.pdf /app/

# Copy the mock environment and headers
COPY mock_sensor.c /app/mock_sensor.c
COPY mock_sensor.c /app/mock_sensor.c.orig
COPY adxl_regs.h /app/

# Copy the Zephyr build configuration
COPY CMakeLists.txt /app/
COPY prj.conf /app/
COPY app.overlay /app/