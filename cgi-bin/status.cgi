#!/bin/bash
echo "Content-type: text/html"
echo

STATUS="$(basename "$PATH_TRANSLATED")"
FOLDER="/home/za3k/cgi-bin/${STATUS}.d"

echo "<html>"
echo "<head>"
echo "  <title>${PATH_TRANSLATED}</title>"
echo '  <link rel="stylesheet" type="text/css" href="/service.css"'
echo "</head>"
echo "<body>"
echo "<table>"
echo "  <thead>"
echo "    <th>Service</td>"
echo "    <th>Status</td>"
echo "    <th>Details</td>"
echo "  </thead>"
find "${FOLDER}" -type f -print0 | sort -z | parallel -0 --keep-order -n1 -- ./status-simple
echo "</table>"
echo "</body>"
echo "</html>"
