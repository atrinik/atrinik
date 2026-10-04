# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
ARG PYTHON_IMAGE=python:3.13-slim-trixie@sha256:bb2988715db2cf7ace7b53f38f3cffbef7c7046a656bee66245eb0ed386e2e81
FROM ${PYTHON_IMAGE} AS corpus
RUN apt-get update && apt-get install -y --no-install-recommends git ca-certificates \
    && rm -rf /var/lib/apt/lists/*
COPY deploy/mcp/build_corpus.py /builder/build_corpus.py
COPY deploy/mcp/source-lock.json /builder/source-lock.json
RUN python3 /builder/build_corpus.py --lock /builder/source-lock.json --destination /corpus

FROM ${PYTHON_IMAGE}
ARG SOURCE_REVISION
LABEL org.opencontainers.image.source="https://github.com/atrinik/atrinik" \
      org.opencontainers.image.revision="${SOURCE_REVISION}" \
      org.opencontainers.image.title="Atrinik public source MCP"
RUN apt-get update && apt-get install -y --no-install-recommends git ripgrep ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --gid 10001 atrinik && useradd --uid 10001 --gid 10001 --no-create-home atrinik \
    && install -d -m 0700 -o 10001 -g 10001 /var/lib/atrinik-auth
WORKDIR /opt/atrinik
COPY atrinik_workspace/ ./atrinik_workspace/
COPY mcp/contract/ ./mcp/contract/
COPY LICENSE ./LICENSE
COPY deploy/mcp/source-lock.json ./source-lock.json
# Ownership is required by Git's safe-directory checks; production runs read-only.
COPY --from=corpus --chown=10001:10001 /corpus/ /opt/atrinik-corpus/
RUN chmod -R a-w /opt/atrinik /opt/atrinik-corpus
ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1
USER 10001:10001
EXPOSE 8765
ENTRYPOINT ["python3", "-B", "-m", "atrinik_workspace.mcp_container"]
CMD ["stdio"]
