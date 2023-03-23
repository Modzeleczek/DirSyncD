#!/bin/bash

check_error ()
{
  if [ $? -ne 0 ]
  then
    echo An error occured. Stopping.
    exit 1
  fi
}

BUILD=./build
TARGET=DirSyncD
INCLUDE=-Iinclude
SOURCE=$(find source/ -type f -iregex ".*\.c")
check_error

if [ -d $BUILD ]
then
  rm -r $BUILD
fi
mkdir -p $BUILD

OBJECTS=""
for path in $SOURCE
do
  path_wo_ext=$(echo $path | sed 's|.c\>||')
  # Equivalent to 'dirname' command.
  dir_name=$(echo $path_wo_ext | sed 's|/[^/]*$||')

  mkdir -p $BUILD/$dir_name
  check_error

  source=$path_wo_ext.c
  object=$BUILD/$path_wo_ext.o
  gcc $source $INCLUDE -c -o $object
  check_error

  OBJECTS="$OBJECTS $object"
done

gcc $OBJECTS -o $BUILD/$TARGET
