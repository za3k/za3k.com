ALL=$(curl -iLs https://ddns.za3k.com)
HEADER=$(echo "$ALL" | head -1)
echo "${ALL}" | grep -q "404 Not Found" >/dev/null || exit 1
echo "${ALL}" | grep -q "no domain" >/dev/null || exit 1
echo "ddns.za3k.com is up"

IP="$(curl -q icanhazip.com 2>/dev/null)"
host deadtree.moreorcs.com ns.za3k.com | grep "has" | grep "${IP}" || exit 1
echo "deadtree.moreorcs.com == this ip"
