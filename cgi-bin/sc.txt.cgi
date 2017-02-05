#!/bin/bash
echo "Content-type: text/plain"
echo

QUERY_FILE="${PATH_TRANSLATED}"
if [[ "${QUERY_FILE}" != *.sc.txt ]]
then
	echo "Invalid .sc.txt file" >/dev/stderr
	exit 1
fi
QUERY_SC_FILE="$(echo "${QUERY_FILE}" | sed -e 's/.txt$//')"
if [ \! -f "${QUERY_SC_FILE}" ]
then
	echo "Invalid .sc file" >/dev/stderr
	exit 1
fi

echo "Source: $(basename ${QUERY_SC_FILE})"
echo
sc -W % "${QUERY_SC_FILE}"

