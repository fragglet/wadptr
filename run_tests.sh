#!/bin/bash

set -eu

wd=$(mktemp -dt wadptr-test.XXXXXX)
trap "rm -rf '$wd'" INT EXIT

file_size() {
    ls -l "$1" | while read _ _ _ _ len rest; do
        echo "$len"
    done
}

test_wad_file() {
    local fn=$1
    local orig_size=$(file_size "$fn")
    if ! ./wadptr -c $fn >$wd/output.txt 2>&1; then
        cat $wd/output.txt
    fi

    local new_size=$(file_size "$fn")
    if ! [ $new_size -lt $orig_size ]; then
        echo "$fn compressed size: $new_size" \
            "not smaller than original size: $orig_size"
        exit 1
    fi

    if ! ./wadptr -u $fn >$wd/output.txt 2>&1; then
        cat $wd/output.txt
    fi
    local decompr_size=$(file_size "$fn")
    # TODO: Do something to check the uncompressed file vs. original
    #echo "$fn $orig_size $new_size $decompr_size"
}

all_success=true

for wad in test/*.wad; do
    filename=$(basename $wad)
    cp $wad $wd
    if test_wad_file "$wd/$filename"; then
        echo "PASS $filename"
    else
        echo "FAIL $filename"
        all_success=false
    fi
done

if ! $all_success; then
    exit 1
fi


