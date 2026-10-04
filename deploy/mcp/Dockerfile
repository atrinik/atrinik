# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
ARG PYTHON_IMAGE=python:3.13-slim-trixie@sha256:bb2988715db2cf7ace7b53f38f3cffbef7c7046a656bee66245eb0ed386e2e81
FROM ${PYTHON_IMAGE}
RUN apt-get update && apt-get install -y --no-install-recommends git ripgrep ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --gid 10001 atrinik && useradd --uid 10001 --gid 10001 --no-create-home atrinik
WORKDIR /opt/atrinik
COPY atrinik_workspace/ ./atrinik_workspace/
COPY mcp/contract/ ./mcp/contract/
COPY LICENSE ./LICENSE
ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1
USER 10001:10001
EXPOSE 8765
HEALTHCHECK --interval=30s --timeout=6s --start-period=5s --retries=3 \
    CMD ["python3", "-B", "-m", "atrinik_workspace.mcp_http_health", "--ca-file", "/run/secrets/atrinik_mcp_ca"]
ENTRYPOINT ["python3", "-B", "-m", "atrinik_workspace.mcp_http"]
