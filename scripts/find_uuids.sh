#! /bin/bash

ROOT="$(cd `dirname $0`/.. && pwd)"

if expr match "$ROOT" " " >/dev/null; then
  echo "$0 doesn't work when there is a space in the path name \"$ROOT\"."
  exit 1
fi

# Only scan LL source files.
FOLDERS="$ROOT/indra/newview $(/bin/ls -d $ROOT/indra/ll*)"

# There should never be UUIDs declared in header files (because they aren't POD).
SOURCE_FILES="$(for d in $FOLDERS; do find "$d" ! -name 'tests' -type f -name '*.cpp' ! -wholename '*/tests/*'; done)"

# UUIDs that are not Asset Types.
# LL_UUID_ALL_AGENTS
AGENTIDS="44e87126-e794-4ded-05b3-7c42da3d5cdb"
# ALEXANDRIA_LINDEN_ID
AGENTIDS+="|ba2a564a-f0f1-4b82-9c61-b7520bfcd09f"
# GOVERNOR_LINDEN_ID / REALESTATE_LINDEN_ID
AGENTIDS+="|3d6181b0-6a4b-97ef-18d8-722652995cf1"
# "AscentReportClientUUID"
AGENTIDS+="|f25263b7-6167-4f34-a4ef-af65213b2e39"

# MAINTENANCE_GROUP_ID
GROUPIDS="dc7b21cd-3c89-fcaa-31c8-25f9ffd224cd"

# MAGIC_ID
INTERNALIDS="3c115e51-04f4-523c-9fa6-98aff1034730"
# CATEGORIZE_LOST_AND_FOUND_ID
INTERNALIDS+="|00000000-0000-0000-0000-000000000010"
# INTERACTIVE_SYSTEM_FROM
INTERNALIDS+="|F387446C-37C4-45f2-A438-D99CBDBB563B"
# sHomeID
INTERNALIDS+="|10000000-0000-0000-0000-000000000001"

LL_DEFAULT_LIGHT_UUID="00000000-0000-0000-0000-000000000000"

# "head"
BAKEDIDS="a4b9dc38-e13b-4df9-b284-751efb0566ff"
# "upper_body"
BAKEDIDS+="|5943ff64-d26c-4a90-a8c0-d61f56bd98d4"
# "lower_body"
BAKEDIDS+="|2944ee70-90a7-425d-a5fb-d749c782ed7d"
# "eyes"
BAKEDIDS+="|27b1bc0f-979f-4b13-95fe-b981c2ba9788"
# "skirt"
BAKEDIDS+="|03e7e8cb-1368-483b-b6f3-74850838ba63"
# "hair"
BAKEDIDS+="|a60e85a9-74e8-48d8-8a2d-8129f28d9b61"

SCRIPTLIMITSIDS="d574a375-0c6c-fe3d-5733-da669465afc7|8dbf2d41-69a0-4e5e-9787-0c9d297bc570|da05fb28-0d20-e593-2728-bddb42dd0160"

# PARCEL_MEDIA_LIST_ITEM_UUID
PARCELMEDIAIDS="CAB5920F-E484-4233-8621-384CF373A321"
# PARCEL_AUDIO_LIST_ITEM_UUID
PARCELMEDIAIDS+="|DF4B020D-8A24-4B95-AB5D-CA970D694822"

# ANIM_AGENT_PHYSICS_MOTION_ID
MOTIONIDS="7360e029-3cb8-ebc4-863e-212df440d987"

# Suspicious sound (probably only existing on SL)
# Radar sound
CUSTOMSOUNDIDS="76c78607-93f9-f55a-5238-e19b1a181389"
# AscentPowerfulWizard
CUSTOMSOUNDIDS+="|58a38e89-44c6-c52b-deb8-9f1ddc527319"

LL_TEXTURE_TRANSPARENT="8dcd4a48-2d37-4909-9f78-f7a9eb4ef903"
ANIM_AGENT_CUSTOMIZE="038fcec9-5ebd-8a8e-0e2e-6e71a0a1ac53"

# Terrain alpha textures
TERRAIN_ALPHA_IDS="e97cf410-8e61-7005-ec06-629eba4cd1fb|38b86f85-2575-52a9-a531-23108d8da837"

# Get a list of all UUIDs. All declared UUIDs will be between double quotes and not commented out.
rm -f sound.uuids.tmp anim.uuids.tmp texture.uuids.tmp
for s in $SOURCE_FILES; do
  LINES=$(grep -E '"[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}"' $s | sed -e 's/\/\/.*//' | \
      grep -v -E "$AGENTIDS|$GROUPIDS|$INTERNALIDS|$LL_DEFAULT_LIGHT_UUID|$BAKEDIDS|$SCRIPTLIMITSIDS|$PARCELMEDIAIDS|$MOTIONIDS|$CUSTOMSOUNDIDS")
  if test -n "$LINES"; then
    SOUND_LINES=$(echo "$LINES" | grep -E "SND|Sound|SOUND")
    echo "$SOUND_LINES" | grep -o -E '"[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}"' | sed 's/"//g' | tr "A-F" "a-f" >> sound.uuids.tmp
    ANIM_LINES=$(echo "$LINES" | grep -E "ANIM_")
    echo "$ANIM_LINES" | grep -o -E '"[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}"' | sed 's/"//g' | tr "A-F" "a-f" >> anim.uuids.tmp
    TEXTURE_LINES=$(echo "$LINES" | grep -E "IMG_|TEXTURE|LL_DEFAULT_|DEFAULT_WATER_NORMAL|TERRAIN_|grey_id|$TERRAIN_ALPHA_IDS")
    echo "$TEXTURE_LINES" | grep -o -E '"[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}"' | sed 's/"//g' | tr "A-F" "a-f" >> texture.uuids.tmp
    # Print unrecognized UUIDs.
    echo "$LINES" | grep -v -E "SND|Sound|SOUND|ANIM_|IMG_|TEXTURE|LL_DEFAULT_|DEFAULT_WATER_NORMAL|TERRAIN_|grey_id|$TERRAIN_ALPHA_IDS|$LL_TEXTURE_TRANSPARENT|$ANIM_AGENT_CUSTOMIZE"
  fi
done

# Sort the files and make them unique.
sort -u sound.uuids.tmp > sound.uuids
sort -u anim.uuids.tmp > anim.uuids
sort -u texture.uuids.tmp > texture.uuids

rm sound.uuids.tmp anim.uuids.tmp texture.uuids.tmp

# Sanity check.
DUPLICATES="$(cat sound.uuids anim.uuids texture.uuids | sort | uniq -d)"
if test -n "$DUPLICATES"; then
  echo "Something went wrong with harvesting UUIDs from the source code. The asset type of the following UUIDs is not clear:"
  echo "$DUPLICATES"
  exit 1
fi

# Get more UUIDs from the web.
wget -q 'http://lslwiki.net/lslwiki/wakka.php?wakka=ClientAssetKeys' -O ClientAssetKeys.tmp

grep -A 1000 '<h3>Textures' ClientAssetKeys.tmp | grep -B 1000 '<h4>General UI' | grep -o -E '[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}' | tr "A-F" "a-f" | sort -u > wiki.texture.uuids
grep -A 1000 '<h3>Animations' ClientAssetKeys.tmp | grep -B 1000 '<h3>Sounds' | grep -o -E '[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}' | tr "A-F" "a-f" | sort -u > wiki.anim.uuids
grep -A 1000 '<h3>Sounds' ClientAssetKeys.tmp | grep -o -E '[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}' | tr "A-F" "a-f" | sort -u > wiki.sound.uuids

rm ClientAssetKeys.tmp

# Get UUIDS defined by opensim.
wget -q 'http://opensimulator.org/viewgit/?a=blob&p=opensim&h=a29ac38bfbab3528bac322363c52332f90c65980&n=TexturesAssetSet.xml' -O TexturesAssetSet.xml
wget -q 'http://opensimulator.org/viewgit/?a=blob&p=opensim&h=b93a76660ef2e4fde2c8341c2a0adedb3a582f11&n=SoundsAssetSet.xml' -O SoundsAssetSet.xml
wget -q 'http://opensimulator.org/viewgit/?a=blob&p=opensim&h=b570c554bece5a16a62d02bdb781bb6545b93ac8&n=CollisionSoundsAssetSet.xml' -O CollisionSoundsAssetSet.xml
wget -q 'http://opensimulator.org/viewgit/?a=blob&p=opensim&h=a26b95fd3f9ad9c0e7d28f2b45067c30df0fdcc8&n=index.xml' -O AnimationsAssetSet.xml

cat TexturesAssetSet.xml | grep -o -E '[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}' | tr "A-F" "a-f" | sort -u > opensim.texture.uuids
cat SoundsAssetSet.xml CollisionSoundsAssetSet.xml | grep -o -E '[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}' | tr "A-F" "a-f" | sort -u > opensim.sound.uuids
cat AnimationsAssetSet.xml | grep -o -E '[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}' | tr "A-F" "a-f" | sort -u > opensim.anim.uuids

rm TexturesAssetSet.xml SoundsAssetSet.xml CollisionSoundsAssetSet.xml AnimationsAssetSet.xml

# Find common UUIDs.
cat texture.uuids wiki.texture.uuids | sort -u | cat - opensim.texture.uuids | sort | uniq -d > $ROOT/etc/common.texture.uuids
cat sound.uuids wiki.sound.uuids | sort -u | cat - opensim.sound.uuids | sort | uniq -d > $ROOT/etc/common.sound.uuids
cat anim.uuids wiki.anim.uuids | sort -u | cat - opensim.anim.uuids | sort | uniq -d > common.anim.uuids

# Get a count.
nr_common_textures=$(cat $ROOT/etc/common.texture.uuids | wc --lines)
nr_common_sounds=$(cat $ROOT/etc/common.sound.uuids | wc --lines)
nr_common_anims=$(cat common.anim.uuids | wc --lines)

# Sanity check (animations UUIDs are not shared).
if test $nr_common_anims -ne 0; then
  echo "Unexpectedly found common animation UUIDs!"
  exit 1
fi

rm common.anim.uuids

# However, all animations refered in the source code are common (they are hardcoded in the viewer after all).
cp anim.uuids $ROOT/etc/common.anim.uuids
nr_common_anims=$(cat $ROOT/etc/common.anim.uuids | wc --lines)

echo "Found $nr_common_textures common texture UUIDs (see common.texture.uuids), $nr_common_sounds common sound UUIDs (see common.sound.uuids) and $nr_common_anims common animation UUIDs (see common.anim.uuids)."
ls -l $ROOT/etc/common.texture.uuids $ROOT/etc/common.sound.uuids $ROOT/etc/common.anim.uuids

echo "Run scripts/gen-is_common_uuid.sh to generate indra/newview/aicommonuuid.cpp from this."

