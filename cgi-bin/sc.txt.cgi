#!/bin/bash
echo "Content-type: text/plain"
echo

QUERY_FILE="${PATH_TRANSLATED}"
[[ "${QUERY_FILE}" == *.sc.txt ]] || echo "Invalid .sc.txt file" >/dev/stderr && exit 1
QUERY_SC_FILE="$(echo "${QUERY_FILE} | sed -e 's/.txt$//')"
[ -f "${QUERY_SC_FILE}" ] || echo "Invalid .sc file" >/dev/stderr && exit 1

echo "Source: https://za3k.com/${QUERY_SC_FILE}"
echo
sc -W % "${QUERY_SC_FILE}"
