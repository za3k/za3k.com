#!/bin/bash
echo "Content-type: text/html"
echo

QUERY_FILE="${PATH_TRANSLATED}"
DB_NAME=$(head -n1 "${QUERY_FILE}" | sed -e 's/--\s*//')
DB="/var/www/za3k/cgi-bin/db/${DB_NAME}"

echo "<html><head><title>Query on #{DB_NAME}</title><link rel="stylesheet" type="text/css" href="db.css"></head><body><table id=\"${DB_NAME}\">"
sqlite3 "$DB" -html -header <"${QUERY_FILE}"
echo "</table></body></html>"
