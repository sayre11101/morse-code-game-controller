# Upgrade to 24.04 to get Python 3.12 natively
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
# Required in 24.04 to allow global pip installs in a container
ENV PIP_BREAK_SYSTEM_PACKAGES=1

ENV ZEPHYR_BASE=/zephyrproject/zephyr
ENV ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk
ENV ZEPHYR_SDK_VERSION=0.16.5

# ---------------------------------------------------------
# 1. OS-Level Dependencies & C Compilers
# ---------------------------------------------------------
RUN apt-get update && apt-get install -y \
    git cmake ninja-build gperf ccache dfu-util device-tree-compiler wget \
    python3-dev python3-pip python3-setuptools python3-wheel xz-utils file \
    make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1 \
    poppler-utils \
    tesseract-ocr \
    ghostscript \
    tmux \
    asciinema \
    && rm -rf /var/lib/apt/lists/*

# ---------------------------------------------------------
# 2. Install curl (required to download uv)
# ---------------------------------------------------------
RUN apt-get update && apt-get install -y curl ca-certificates && rm -rf /var/lib/apt/lists/*

# ---------------------------------------------------------
# 3. Install uv package manager
# ---------------------------------------------------------
RUN curl -LsSf https://astral.sh/uv/0.9.5/install.sh | sh
ENV PATH="/root/.local/bin:$PATH"

# ---------------------------------------------------------
# 4. Pre-warm the offline cache
# ---------------------------------------------------------
# By running 'pytest --version' with your exact flags, uv will download 
# Python 3.12, pytest 8.4.1, and pytest-json-ctrf 0.3.5 and save them permanently.
RUN uvx -p 3.12 -w pytest==8.4.1 -w pytest-json-ctrf==0.3.5 pytest --version

# ---------------------------------------------------------
# 5. Download and Install the Zephyr SDK
# ---------------------------------------------------------
RUN wget -q "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64.tar.xz" && \
    tar -xf zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64.tar.xz -C /opt && \
    mv /opt/zephyr-sdk-${ZEPHYR_SDK_VERSION} /opt/zephyr-sdk && \
    rm zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64.tar.xz && \
    cd /opt/zephyr-sdk && \
    yes | ./setup.sh -h -c

# ---------------------------------------------------------
# 6. Initialize Zephyr Workspace & Install ITS Requirements
# ---------------------------------------------------------
RUN git config --global user.email "agent@example.com" && \
    git config --global user.name "AI Agent" && \
    pip3 install --no-cache-dir west==1.2.0 && \
    west init -m https://github.com/zephyrproject-rtos/zephyr --mr v3.7.0 /zephyrproject && \
    cd /zephyrproject && \
    west update --narrow --fetch-opt=--depth=1 && \
    pip3 install --no-cache-dir -r zephyr/scripts/requirements.txt
    
# ---------------------------------------------------------
# 7. Install the Agent's Custom PDF Tools Last
# ---------------------------------------------------------
RUN pip3 install --no-cache-dir PyMuPDF==1.24.2 pdfplumber==0.11.9 pypdf==5.0.1

RUN mkdir -p /app
WORKDIR /app
# Copy from the local environment/ context directly into the image's /app root
COPY CMakeLists.txt /app/CMakeLists.txt
COPY mock_sensor.c /app/mock_sensor.c
COPY app.overlay /app/app.overlay
COPY prj.conf /app/prj.conf
# Copy the instructions and target PDF directly into the agent's workspace
COPY instruction.md /app/instruction.md
COPY adxl345.pdf /app/
