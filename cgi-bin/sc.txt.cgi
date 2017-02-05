#!/bin/bash
echo "Content-type: text/plain"
echo

QUERY_FILE="${PATH_TRANSLATED}"
echo "${PATH_TRANSLATED}" >/dev/stderr
[[ "${QUERY_FILE}" == *.sc.txt ]] || echo "Invalid .sc.txt file" >/dev/stderr && exit 1
QUERY_SC_FILE="$(echo "${QUERY_FILE} | sed -e 's/.txt$//')"
DB_NAME=$(head -n1 "${QUERY_FILE}" | sed -e 's/--\s*//')

echo "Source: https://za3k.com/${QUERY_SC_FILE}"
echo
sc -W % "${QUERY_SC_FILE}"
