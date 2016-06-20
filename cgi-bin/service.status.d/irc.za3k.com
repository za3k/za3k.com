if  nmap irc.za3k.com -p 6667 2>/dev/null | grep -q open
then
  echo "Port 6667 open"
else
  echo "Port 6667 not open"
  exit 1
fi
