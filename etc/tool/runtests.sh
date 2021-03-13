#!/bin/bash

PASSC=0
FAILC=0
SKIPC=0

logskip()   { echo -e "\x1b[90mSKIP\x1b[0m $1" ; }
logpass()   { echo -e "\x1b[32mPASS\x1b[0m $1" ; }
logfail()   { echo -e "\x1b[31mFAIL\x1b[0m $1" ; }
logdetail() { echo -e "$1" ; }
logother()  { echo -e "$1" ; }

while [ "$#" -gt 0 ] ; do
  EXE="$1"
  shift 1
  while IFS= read INPUT ; do
    read INTRODUCER KEYWORD ARGS <<<"$INPUT"
    if [ "$INTRODUCER" = RB_TEST ] ; then
      case "$KEYWORD" in
        SKIP) SKIPC=$((SKIPC+1)) ; logskip "$ARGS" ;;
        PASS) PASSC=$((PASSC+1)) ; logpass "$ARGS" ;;
        FAIL) FAILC=$((FAILC+1)) ; logfail "$ARGS" ;;
        DETAIL) logdetail "$ARGS" ;;
        *) logother "$INPUT" ;;
      esac
    else
      logother "$INPUT"
    fi
  done < <( $EXE $RB_TEST_FILTER 2>&1 || echo "RB_TEST FAIL $EXE" )
done

if [ "$FAILC" -gt 0 ] ; then
  BANNER="\x1b[41m    \x1b[0m"
elif [ "$PASSC" -gt 0 ] ; then
  BANNER="\x1b[42m    \x1b[0m"
else
  BANNER="\x1b[40m    \x1b[0m"
fi
echo -e "$BANNER $FAILC fail, $PASSC pass, $SKIPC skip"
