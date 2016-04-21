ALL=$(curl -iLs https://ddns.za3k.com)
HEADER=$(echo "$ALL" | head -1)
echo "${ALL}" | grep -q "404 Not Found" >/dev/null || exit 1
echo "${ALL}" | grep -q "no domain" >/dev/null || exit 1
