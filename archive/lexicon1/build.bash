for x in *.txt; do
  [ -f "$x" ] && mv "$x" "${x%.txt}.md"
done
[ -f LexiconOne.md ] && mv -i LexiconOne.md index.md


for x in *.md; do
  # Replace [[The Dog]] with [The Dog](The Dog)
  sed --in-place -Ee "s/\[\[([^][]+?)\]\]/[\1](\1)/g" "$x"
  # Replace [The Loping Doge](https://lexicon.za3k.com/index.php/Loping%20Doge,%20The) with [The Loping Doge](Loping Doge, The)
  sed --in-place -Ee "s^https://lexicon.za3k.com/index.php/^^g" "$x"
  sed --in-place -Ee "s^%20^ ^g" "$x"
  # Add a header
  if [ "$x" = index.md ]; then
    HEADER="[za3k](/) > [archive](/archive) > [lexicon1](/archive/lexicon1) > LexiconOne"
  else
    HEADER="[za3k](/) > [archive](/archive) > [lexicon1](/archive/lexicon1) > ${x%.md}"
  fi
  grep -qxF "$HEADER" "$x" || sed -i "1s@^@${HEADER}\n\n@" "$x"
done
