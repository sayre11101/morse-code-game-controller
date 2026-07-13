FROM public.ecr.aws/docker/library/gcc:13-bookworm@sha256:930f2ebe239275fa67226654cb79273ea34eee672ae61c8a39f689c37fb7ac5c

WORKDIR /app

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        python3=3.11.2-1+b1 \
        python3-pip=23.0.1+dfsg-1 \
        tmux \
        asciinema \
    && rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir pytest==8.4.1 pytest-json-ctrf==0.3.5 --break-system-packages

# ---------------------------------------------------------
# 6. Setup Agent Workspace
# ---------------------------------------------------------
RUN mkdir -p /app

# Copy the core application files
COPY main.c /app/
COPY main.c /app/main.c.orig
COPY morse-code-sheet.pdf /app/

# Copy the mock environment and headers
COPY mock_sensor.c /app/mock_sensor.c
COPY mock_sensor.c /app/mock_sensor.c.orig
COPY mock_sensor.h /app/
