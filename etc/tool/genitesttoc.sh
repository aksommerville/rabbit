#!/bin/sh

if [ "$#" -lt 1 ] ; then
  echo "Usage: $0 [INPUTS...] OUTPUT"
  exit 1
fi

TOC=mid/itesttoc.tmp
echo -n "" >"$TOC"

while [ "$#" -gt 1 ] ; do
  SRCPATH="$1"
  shift 1
  SRCBASE="$(basename $SRCPATH)"
  nl -ba -s' ' "$SRCPATH" | sed -En 's/^ *([0-9]+) *(XXX_)?RB_ITEST *\( *([0-9a-zA-Z_]+) *(,([^)]*))?\).*$/'"$SRCBASE"' \1 _\2 \3 \5/p' >>$TOC
done

DSTPATH="$1"
cat - >"$DSTPATH" <<EOF
/* rb_itest_toc.h
 */
 
#ifndef RB_ITEST_TOC_H
#define RB_ITEST_TOC_H

EOF

while read F L I N T ; do
  echo "int $N();" >>"$DSTPATH"
done <$TOC

cat - >>"$DSTPATH" <<EOF

static const struct rb_itest {
  int (*fn)();
  const char *name;
  const char *file;
  int line;
  const char *tags;
  int ignore;
} rb_itestv[]={
EOF

while read F L I N T ; do
  if [ "$I" = "_XXX_" ] ; then
    IGNORE=1
  else
    IGNORE=0
  fi
  echo "  {$N,\"$N\",\"$F\",$L,\"$T\",$IGNORE}," >>"$DSTPATH"
done <$TOC

cat - >>"$DSTPATH" <<EOF
};

#endif
EOF

rm $TOC
