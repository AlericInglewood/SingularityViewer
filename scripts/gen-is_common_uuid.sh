#! /bin/bash

# This script takes as input two files that must be passed through the command line.

ROOT="$(cd `dirname $0`/.. && pwd)"

# Generate the gperf input file.

cat > common.textures.gperf << EOF
%{
#include "llviewerprecompiledheaders.h"
#include "llassettype.h"
#include "lluuid.h"
#include "aicommonuuid.h"
%}
%struct-type
%language=C++
%define slot-name digest
%define hash-function-name uuid_hash
%define lookup-function-name is_common
%define class-name CommonUUIDHash
%compare-lengths
%readonly-tables
%enum
%null-strings
%define word-array-name common_uuids
%omit-struct-type
struct AICommonUUID { char const* digest; int asset_type; };
%%
EOF

sed -e 's/-//g;s/../\\x&/g;s/^/"/;s/$/",LLAssetType::AT_TEXTURE/' "$ROOT/etc/common.texture.uuids" >> common.textures.gperf
sed -e 's/-//g;s/../\\x&/g;s/^/"/;s/$/",LLAssetType::AT_SOUND/' "$ROOT/etc/common.sound.uuids" >> common.textures.gperf

cat >> common.textures.gperf << EOF
%%
namespace AIMultiGrid {

AICommonUUID const* is_common_uuid(LLUUID const& id)
{
  return CommonUUIDHash::is_common((char const*)id.mData, 16);
}

} // namespace AIMultiGrid
EOF

# Generate the source code.
gperf -n -m 10 common.textures.gperf | grep -v '^#line 1 ' > $ROOT/indra/newview/aicommonuuid.cpp

echo "Generated source file indra/newview/aicommonuuid.cpp"

