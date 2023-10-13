#!/bin/bash

# We extract everything except levels, since SIDEDEFS and BLOCKMAP lumps
# will not be exactly the same after decompression.
DEUTEX_OPTS="
    -v0 -overwrite -png -rgb 0 255 255
    -flats -graphics -lumps -musics -patches -sounds -sprites -textures
"

set -eu

wd=$(mktemp -dt wadptr-test.XXXXXX)
trap "rm -rf '$wd'" INT EXIT

file_size() {
    ls -l "$1" | while read _ _ _ _ len rest; do
        echo "$len"
    done
}

deutex_extract() {
    local fn=$1
    local dir=$2
    rm -rf "$dir"
    mkdir -p "$dir"
    # We set up the bootstrap wad as a fake IWAD to make deutex happy.
    cp bootstrap.wad "$dir/doom2.wad"
    export -n DOOMWADDIR DOOMWADPATH
    pushd "$dir"
    if ! deutex $DEUTEX_OPTS -xtract "$fn"; then
        echo "deutex exited with status $? when extracting $fn"
    fi
    popd
}

test_wad_file() {
    local fn=$1
    local orig_size=$(file_size "$fn")
    deutex_extract $fn $wd/deutex-orig
    if ! ./wadptr -c $fn; then
        return 1
    fi

    # TODO: Fix the -o option.
    cp $fn $wd/compr2.wad
    if ! ./wadptr -c $wd/compr2.wad || ! diff $fn $wd/compr2.wad; then
        echo "WAD differs after compression a second time"
        return 1
    fi

    deutex_extract $fn $wd/deutex-compressed
    local new_size=$(file_size "$fn")
    if ! [ $new_size -lt $orig_size ]; then
        echo "$fn compressed size: $new_size" \
            "not smaller than original size: $orig_size"
        return 1
    fi

    if ! ./wadptr -u $fn; then
        return 1
    fi

    cp $fn $wd/uncompr2.wad
    if ! ./wadptr -u $wd/uncompr2.wad || ! diff $fn $wd/uncompr2.wad; then
        echo "WAD differs after decompression a second time"
        return 1
    fi

    # Decompressed WAD contents (apart from levels) should exactly match
    # the original WAD.
    deutex_extract $fn $wd/deutex-decompressed
    if ! diff -q -ur $wd/deutex-orig $wd/deutex-decompressed; then
        echo "Decompressed WAD contents do not match original."
        return 1
    fi

    local decompr_size=$(file_size "$fn")
    # TODO: Do something to check the uncompressed file vs. original
    #echo "$fn $orig_size $new_size $decompr_size"
}

all_success=true

for wad in test/*.wad; do
    filename=$(basename $wad)
    cp $wad $wd

    if test_wad_file "$wd/$filename" >$wd/log 2>&1; then
        echo "PASS $filename"
    else
        echo "FAIL $filename"
        sed "s/^/| /" < $wd/log
        all_success=false
    fi
done

if ! $all_success; then
    exit 1
fi


