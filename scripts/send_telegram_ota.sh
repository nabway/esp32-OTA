#!/usr/bin/env bash
set -euo pipefail
# Send an OTA trigger message via Telegram using values extracted from
# include/config/secrets.hpp so you don't have to paste tokens in the shell.

SECRETS_FILE="include/config/secrets.hpp"
if [ ! -f "$SECRETS_FILE" ]; then
  echo "Secrets file not found: $SECRETS_FILE" >&2
  exit 1
fi

TELEGRAM_TOKEN=$(grep "TELEGRAM_TOKEN\[\]" "$SECRETS_FILE" | sed 's/.*"\([^"]*\)".*/\1/')
TELEGRAM_CHAT_ID=$(grep "TELEGRAM_CHAT_ID\[\]" "$SECRETS_FILE" | sed 's/.*"\([^"]*\)".*/\1/')

if [ -z "$TELEGRAM_TOKEN" ] || [ -z "$TELEGRAM_CHAT_ID" ]; then
  echo "Could not extract TELEGRAM_TOKEN or TELEGRAM_CHAT_ID from $SECRETS_FILE" >&2
  exit 1
fi

URL="${1:-http://192.168.178.87:8000/firmware.bin}"

echo "Sending OTA trigger to chat $TELEGRAM_CHAT_ID with URL: $URL"
curl -s -X POST "https://api.telegram.org/bot${TELEGRAM_TOKEN}/sendMessage" \
  -d chat_id="$TELEGRAM_CHAT_ID" \
  -d text="/ota $URL"

echo "Done."
