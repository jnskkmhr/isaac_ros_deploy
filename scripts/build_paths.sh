#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Sourced by build/launch/test scripts; separate architectures and backends.
case "${DEPLOY_PROFILE:-cpu}" in
    cpu) suffix="-cpu-$(uname -m)" ;;
    x86-gpu|jp72-gpu|jp71-gpu|jp6-gpu|jp5-gpu) suffix="-${DEPLOY_PROFILE}" ;;
    *) echo "Unknown DEPLOY_PROFILE: ${DEPLOY_PROFILE}" >&2; return 1 ;;
esac
export DEPLOY_BUILD_DIR="build${suffix}"
export DEPLOY_INSTALL_DIR="install${suffix}"
export DEPLOY_LOG_DIR="log${suffix}"
