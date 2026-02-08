# Custom AWF container with PowerShell support
# Based on official AWF Dockerfile but adapted for runner base image
ARG BASE_IMAGE=ghcr.io/catthehacker/ubuntu:runner-22.04
FROM ${BASE_IMAGE}

# Switch to root for package installation
USER root

# Install required packages (most already exist in runner images)
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
RUN if ! getent group awfuser >/dev/null 2>&1; then \
      if ! getent group ${USER_GID} >/dev/null 2>&1; then \
        groupadd -g ${USER_GID} awfuser; \
      else \
        groupadd awfuser; \
      fi; \
    fi && \
    if ! id -u awfuser >/dev/null 2>&1; then \
      if ! getent passwd ${USER_UID} >/dev/null 2>&1; then \
        useradd -u ${USER_UID} -g awfuser -m -s /bin/bash awfuser; \
      else \
        useradd -g awfuser -m -s /bin/bash awfuser; \
      fi; \
    fi && \
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
