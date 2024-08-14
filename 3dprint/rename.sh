#!/bin/sh

rename() {
    dir=`pwd`
    pn=`basename $dir`
    for i in {1..100}; do
        name=${pn}_${i}.jpg
        [ -e $name ] || {
            mv -i $1 $name
            return
        }
    done
}

for x in PXL_*.jpg; do
  rename "$x"
done
