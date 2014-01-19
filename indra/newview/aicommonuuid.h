// See scripts/gen-is_common_uuid.sh, this must be the same as defined there.
struct AICommonUUID { char const* digest; int asset_type; };

namespace AIMultiGrid {

// Returns non-NULL if the id is a common UUID.
AICommonUUID const* is_common_uuid(LLUUID const& id);

} // namespace AIMultiGrid
