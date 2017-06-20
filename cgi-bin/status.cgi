#!/bin/bash
echo "Content-type: text/html"
echo

STATUS="$(basename "$PATH_TRANSLATED")"
FOLDER="/var/www/za3k/cgi-bin/${STATUS}.d"
SECONDS=5
if [[ "${QUERY_STRING}" =~ ^t=[0-9]+$ ]]
then
    SECONDS="${QUERY_STRING#t=}"
fi
if [ "$SECONDS" -gt 30 ]; SECONDS=5; fi
export SECONDS

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
find "${FOLDER}" -type f -print0 | sort -z | SHELL=/bin/sh parallel -0 -j 0 --keep-order -n1 -- ./status-simple "${SECONDS}"
echo "</table>"
echo "<a href=\"https://github.com/za3k/za3k.com/tree/master/cgi-bin/service.status.d\">[Source]</a>"
echo "</body>"
echo "</html>"
