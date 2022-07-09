for x in *.txt; do
  [ -f "$x" ] && mv "$x" "${x%.txt}.md"
done
[ -f LexiconOne.md ] && mv -i LexiconOne.md index.md

for x in *.md; do
  # Replace [[The Dog]] with [The Dog](The Dog)
  sed --in-place -Ee "s/\[\[([^][]+?)\]\]/[\1](\1)/g" "$x"
  # Replace [The Loping Doge](https://lexicon.za3k.com/index.php/Loping%20Doge,%20The) with [The Loping Doge](Loping Doge, The)
done
