# Custom AWF container with PowerShell support
# Base image from catthehacker with pwsh pre-installed
FROM ghcr.io/catthehacker/ubuntu:pwsh-latest

# Switch to root for package installation
USER root

# Install additional packages needed by AWF
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    iptables \
    gosu \
    libcap2-bin && \
    rm -rf /var/lib/apt/lists/*

# Verify pwsh is available
RUN pwsh --version

# Create non-root user
ARG USER_UID=1000
ARG USER_GID=1000
RUN groupadd -g ${USER_GID} awfuser || groupadd awfuser && \
    useradd -u ${USER_UID} -g awfuser -m -s /bin/bash awfuser || useradd -g awfuser -m -s /bin/bash awfuser && \
    mkdir -p /home/awfuser/.copilot/logs && \
    chown -R awfuser:awfuser /home/awfuser

# Copy AWF scripts
COPY setup-iptables.sh /usr/local/bin/setup-iptables.sh
COPY entrypoint.sh /usr/local/bin/entrypoint.sh
COPY pid-logger.sh /usr/local/bin/pid-logger.sh
COPY docker-stub.sh /usr/bin/docker
RUN chmod +x /usr/local/bin/setup-iptables.sh /usr/local/bin/entrypoint.sh /usr/local/bin/pid-logger.sh /usr/bin/docker

WORKDIR /workspace

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
