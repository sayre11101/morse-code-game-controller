# Upgrade to 24.04 to get Python 3.12 natively
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
    python3-dev python3-pip python3-setuptools python3-wheel xz-utils file \
    make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1 \
    poppler-utils \
    tesseract-ocr \
    ghostscript \
    tmux \
    asciinema \
    curl ca-certificates \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* /var/cache/apt/archives/* /tmp/* /var/tmp/*

# ---------------------------------------------------------
# 2. Install uv package manager
# ---------------------------------------------------------
RUN curl -LsSf https://astral.sh/uv/0.9.5/install.sh | sh
ENV PATH="/root/.local/bin:$PATH"

# ---------------------------------------------------------
# 3. Pre-warm the offline cache
# ---------------------------------------------------------
RUN uvx -p 3.12 -w pytest==8.4.1 -w pytest-json-ctrf==0.3.5 pytest --version

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
# 6. Install the Agent's Custom PDF Tools Last
# ---------------------------------------------------------
# OPTIMIZATION: Clear pip caches after install
RUN pip3 install --no-cache-dir PyMuPDF==1.24.2 pdfplumber==0.11.9 pypdf==5.0.1 && \
    rm -rf /root/.cache/pip /tmp/* /var/tmp/*

# ---------------------------------------------------------
# 7. Setup Agent Workspace
# ---------------------------------------------------------
RUN mkdir -p /app
WORKDIR /app
COPY adxl345.pdf /app/
COPY main.c /app/main.c
COPY prj.conf /app/
COPY CMakeLists.txt /app/
COPY mock_sensor.c /app/