#!/bin/bash
ALL=$(curl -ILs "http://transmission.moreorcs.com:9091")
HEADER=$(echo "$ALL" | head -1)
echo "${HEADER}"
echo "${ALL}" | grep -q "401 Unauthorized"
