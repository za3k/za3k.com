for x in *.txt; do
  mv "$x" "${x%.txt}.md"
done
mv LexiconOne.md index.md

# Replace [[The Dog]] with [The Dog](The Dog)

# Replace [The Loping Doge](https://lexicon.za3k.com/index.php/Loping%20Doge,%20The) with [The Loping Doge](Loping Doge, The)
