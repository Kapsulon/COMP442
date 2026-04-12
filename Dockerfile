FROM --platform=linux/amd64 ubuntu:24.04

# Avoid prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install only essentials
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        curl \
        git \
    && rm -rf /var/lib/apt/lists/*

# Install xmake (official script)
RUN curl -fsSL https://xmake.io/shget.text -o /tmp/xmake-install.sh && \
    bash /tmp/xmake-install.sh && \
    rm /tmp/xmake-install.sh

# Make xmake available
ENV PATH="/root/.local/bin:/root/.xmake/bin:${PATH}"

ENV XMAKE_ROOT=y

WORKDIR /project

COPY xmake.lua .
COPY src/ src/

RUN xmake f -m release -y && xmake -y
