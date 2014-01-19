/* C++ code produced by gperf version 3.0.3 */
/* Command-line: gperf -n -m 10 common.textures.gperf  */
/* Computed positions: -k'1,5' */


#include "llviewerprecompiledheaders.h"
#include "llassettype.h"
#include "lluuid.h"
#include "aicommonuuid.h"
/* maximum key range = 68, duplicates = 0 */

class CommonUUIDHash
{
private:
  static inline unsigned int uuid_hash (const char *str, unsigned int len);
public:
  static const struct AICommonUUID *is_common (const char *str, unsigned int len);
};

inline /*ARGSUSED*/
unsigned int
CommonUUIDHash::uuid_hash (register const char *str, register unsigned int len)
{
  static const unsigned char asso_values[] =
    {
      51, 51, 68, 68, 68, 68, 68, 68, 33, 13,
      68, 68, 16, 27, 68, 68, 32, 39, 68, 31,
      68, 68, 30,  7, 30, 68, 68, 68, 68, 68,
      68, 35, 68, 42, 68, 68, 24, 68, 28, 68,
      43, 68, 27, 27, 12,  9, 68, 26, 28, 26,
      25, 68, 68, 68, 68, 68, 25, 41, 24, 68,
      23, 24, 28, 22, 68, 37, 68, 18, 20, 68,
      68, 20, 68, 68, 68, 19, 15, 68, 68, 68,
      68, 68, 68, 23, 30, 68, 68,  1, 18, 68,
      68, 26, 68, 68, 17, 68, 68, 68, 68, 68,
      17, 30, 68, 16, 68, 16, 68, 68, 15,  2,
      68, 68,  2, 68, 14, 68, 68, 68, 68, 14,
      13, 12, 14, 68, 20, 68, 26, 68, 68, 68,
      11, 68, 68, 68, 13, 68, 25, 17, 10, 10,
      68,  9,  8,  8, 68, 68,  8, 68, 68, 68,
       7, 68,  6, 25, 10, 68, 68, 23, 68, 68,
       6, 68, 68, 68, 68,  5, 14, 68,  5, 68,
      68, 17, 14,  4, 68, 68, 68, 68, 68, 68,
       4, 68, 12, 27,  4, 68, 68,  4, 68, 68,
      23, 68, 68,  5, 19, 11, 68, 68, 68, 68,
       6, 68, 19,  4, 68,  3, 68, 68,  8,  3,
      68, 68, 68, 68, 30,  5, 68,  7, 68, 68,
       0, 68, 15, 31,  1, 68, 68, 68, 68, 68,
      68, 68, 29, 68, 10, 68, 68,  1, 68, 68,
      68, 68, 68, 68,  6, 68,  0, 68, 68, 68,
      68, 22, 68, 68, 68, 68
    };
  return asso_values[(unsigned char)str[4]] + asso_values[(unsigned char)str[0]];
}

const struct AICommonUUID *
CommonUUIDHash::is_common (register const char *str, register unsigned int len)
{
  enum
    {
      TOTAL_KEYWORDS = 68,
      MIN_WORD_LENGTH = 16,
      MAX_WORD_LENGTH = 16,
      MIN_HASH_VALUE = 0,
      MAX_HASH_VALUE = 67
    };

  static const unsigned char lengthtable[] =
    {
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16
    };
  static const struct AICommonUUID common_uuids[] =
    {
#line 88 "common.textures.gperf"
      {"\366\272\230\026\334\257\367U{gQ\263\033b3\345",LLAssetType::AT_SOUND},
#line 32 "common.textures.gperf"
      {"WH\336\314\366)F\034\2326\243Z\"\037\342\037",LLAssetType::AT_TEXTURE},
#line 85 "common.textures.gperf"
      {"\340W\302DWh\020V\303~\0257EN\353b",LLAssetType::AT_SOUND},
#line 86 "common.textures.gperf"
      {"\355\022Gdp]\324\227\026z\030,\331\372.l",LLAssetType::AT_SOUND},
#line 39 "common.textures.gperf"
      {"m\343~Np)a\365T\270\365\346?\230?X",LLAssetType::AT_TEXTURE},
#line 59 "common.textures.gperf"
      {"\315\331\251\374m\013\371\015\204\026\307+`\031\274\250",LLAssetType::AT_TEXTURE},
#line 78 "common.textures.gperf"
      {"\273\344\307\374pD\260^{\2116\222JgY<",LLAssetType::AT_SOUND},
#line 82 "common.textures.gperf"
      {"\321f\003\233\264\365\302\354I\021\310\\r{\001l",LLAssetType::AT_SOUND},
#line 55 "common.textures.gperf"
      {"\270\323\226Z\255x\277Ci\233\277\370\354\246\311u",LLAssetType::AT_TEXTURE},
#line 53 "common.textures.gperf"
      {"\250Z\306t\313uJ\366\224\231\337|Z\257z(",LLAssetType::AT_TEXTURE},
#line 79 "common.textures.gperf"
      {"\301\3634\373\245\276\217\347\"\263)c\034!\317\013",LLAssetType::AT_SOUND},
#line 83 "common.textures.gperf"
      {"\327\251\245e\240\023*iy}S2\272\241\251G",LLAssetType::AT_SOUND},
#line 81 "common.textures.gperf"
      {"\310%\337\274\230'~\002e\0077\023\321\211\026\301",LLAssetType::AT_SOUND},
#line 50 "common.textures.gperf"
      {"\226\264\3361\364\3723}\354xE\0366\011v\236",LLAssetType::AT_TEXTURE},
#line 84 "common.textures.gperf"
      {"\331\367<\370\027\264oz\025eyQ\"l0]",LLAssetType::AT_SOUND},
#line 49 "common.textures.gperf"
      {"\217E\205I\027;#\377\324\377\277\252^\2427\033",LLAssetType::AT_TEXTURE},
#line 77 "common.textures.gperf"
      {"\216\256\326\037\222\377d\205\336\203M\314\223\212G\216",LLAssetType::AT_SOUND},
#line 28 "common.textures.gperf"
      {"-xDv\320\333\231y\014\377\224\010tZ|\363",LLAssetType::AT_TEXTURE},
#line 48 "common.textures.gperf"
      {"\215\315JH-7I\011\237x\367\251\353N\371\003",LLAssetType::AT_TEXTURE},
#line 47 "common.textures.gperf"
      {"\213_\354e\215\215\235\305\315\250\217\337'\026\343a",LLAssetType::AT_TEXTURE},
#line 46 "common.textures.gperf"
      {"\212QX\211\352\311\373U\216\272\322\334\011\3532\310",LLAssetType::AT_TEXTURE},
#line 43 "common.textures.gperf"
      {"\202-\355I\232l\366\034\313\211m\365OB\315\364",LLAssetType::AT_TEXTURE},
#line 76 "common.textures.gperf"
      {"z\377\"e\320[\213rc\307\333\371m\302\362\037",LLAssetType::AT_SOUND},
#line 40 "common.textures.gperf"
      {"yPK\365\303\354\007cec\330C\336f\320\241",LLAssetType::AT_TEXTURE},
#line 67 "common.textures.gperf"
      {",4n\332\266\014\2533\021\031\270\224\031\026\244\231",LLAssetType::AT_SOUND},
#line 27 "common.textures.gperf"
      {",\257\021yxao\363K}F\341w\200\275\372",LLAssetType::AT_TEXTURE},
#line 62 "common.textures.gperf"
      {"\011\262\030N\206\001D\342\257\273\3167CK\213\241",LLAssetType::AT_SOUND},
#line 75 "common.textures.gperf"
      {"w\240\030\257\011\216\3007Q\246\027\217\005\207|o",LLAssetType::AT_SOUND},
#line 52 "common.textures.gperf"
      {"\246\026!3rKT\337\241/Q\315\007\012\326\363",LLAssetType::AT_TEXTURE},
#line 38 "common.textures.gperf"
      {"lG'\270\254y\272D;\201\371\252\210{G\353",LLAssetType::AT_TEXTURE},
#line 72 "common.textures.gperf"
      {"L\214<w\336\215\275\342\271\2702c^\017\324\246",LLAssetType::AT_SOUND},
#line 63 "common.textures.gperf"
      {"\014\267\260\012L\020iH\204\336\251<\011\257+\251",LLAssetType::AT_SOUND},
#line 37 "common.textures.gperf"
      {"g\223\0231\014\002Hv\022U(w\010\226\306\242",LLAssetType::AT_TEXTURE},
#line 35 "common.textures.gperf"
      {"d6{\321i~\263\346\013e?\206*Wsf",LLAssetType::AT_TEXTURE},
#line 73 "common.textures.gperf"
      {"^\031\034{\211\226\234\355\241w\262\2542\277\352\006",LLAssetType::AT_SOUND},
#line 33 "common.textures.gperf"
      {"X\224\342\347\253\215\355\372\346\034\030\317\026\205K\243",LLAssetType::AT_TEXTURE},
#line 87 "common.textures.gperf"
      {"\364\240f\017TF\336\242\200\267d\202\240\202\200<",LLAssetType::AT_SOUND},
#line 41 "common.textures.gperf"
      {"z+:JS\302S\254W\026\252\307\327C\300 ",LLAssetType::AT_TEXTURE},
#line 57 "common.textures.gperf"
      {"\302(\321\317K]K\250\204\364\211\232\007\226\252\227",LLAssetType::AT_TEXTURE},
#line 58 "common.textures.gperf"
      {"\312N\214'G<\353\034/]P\356?\007\330\\",LLAssetType::AT_TEXTURE},
#line 42 "common.textures.gperf"
      {"|\014\370\233D\261\034\342\335t\007\020*\230\254*",LLAssetType::AT_TEXTURE},
#line 45 "common.textures.gperf"
      {"\211UgG$\313C\355\222\013G\312\355\025F_",LLAssetType::AT_TEXTURE},
#line 66 "common.textures.gperf"
      {"$*\370+C\302\232;\341\010;\014~8I\201",LLAssetType::AT_SOUND},
#line 80 "common.textures.gperf"
      {"\310\002`\272A\375\212Fv\212k\36266\016:",LLAssetType::AT_SOUND},
#line 61 "common.textures.gperf"
      {"\373*\342\004?\321\3373YO\311\370\202\203\016f",LLAssetType::AT_TEXTURE},
#line 54 "common.textures.gperf"
      {"\253\267\203\346>\223&\300$\212$vf\205]\243",LLAssetType::AT_TEXTURE},
#line 31 "common.textures.gperf"
      {"<Y\367\376\235\310G\371\212\257\251\335\037\274;\357",LLAssetType::AT_TEXTURE},
#line 30 "common.textures.gperf"
      {":6}\034\276\361mCu\225\350\214\036:\255\263",LLAssetType::AT_TEXTURE},
#line 23 "common.textures.gperf"
      {"\027\234\332\2759\212\233k\023\221M\3033\2722\037",LLAssetType::AT_TEXTURE},
#line 70 "common.textures.gperf"
      {"=\011\365\2028Q\300\340\365\272'z\305\307?\264",LLAssetType::AT_SOUND},
#line 51 "common.textures.gperf"
      {"\231\275`\2422P\357\311.9/\274\255\357\276\314",LLAssetType::AT_TEXTURE},
#line 44 "common.textures.gperf"
      {"\210r\362\2701\333B\330X\012\263\344\251\022b\336",LLAssetType::AT_TEXTURE},
#line 34 "common.textures.gperf"
      {"[\301\034\326/@\007\036\250\332\011\0039B\004\371",LLAssetType::AT_TEXTURE},
#line 26 "common.textures.gperf"
      {"+\3758\204~'i\271\272:>g?h\000\004",LLAssetType::AT_TEXTURE},
#line 25 "common.textures.gperf"
      {"*H\200\266\267\243i\012 I\277\2768\352\373\237",LLAssetType::AT_TEXTURE},
#line 68 "common.textures.gperf"
      {",\250I\272(\205K\303\220\357\324\230z[\230:",LLAssetType::AT_SOUND},
#line 29 "common.textures.gperf"
      {"0\004|\354&\235@\216\0140\262`;\210rh",LLAssetType::AT_TEXTURE},
#line 65 "common.textures.gperf"
      {"!\234]\223l\0111\305\373?\305\376t\225\301\025",LLAssetType::AT_SOUND},
#line 69 "common.textures.gperf"
      {"<\217\307&\037\326\206-\372\001\026\305\262V\215\266",LLAssetType::AT_SOUND},
#line 24 "common.textures.gperf"
      {"\030\373\210\213\350\361\334\347}\2472\035e\036\246\260",LLAssetType::AT_TEXTURE},
#line 36 "common.textures.gperf"
      {"e\"\347M\026`N\177\266\001oH\301e\232w",LLAssetType::AT_TEXTURE},
#line 60 "common.textures.gperf"
      {"\326\221\240\034\023\267W\215W\300\\\256\360\264\347\341",LLAssetType::AT_TEXTURE},
#line 56 "common.textures.gperf"
      {"\276\261i\307\021\352\377\362\357\345\017$\334\210\035\362",LLAssetType::AT_TEXTURE},
#line 64 "common.textures.gperf"
      {"\020It\343\337\332B\213\231\356\260\324\347H\323\243",LLAssetType::AT_SOUND},
#line 71 "common.textures.gperf"
      {"At\370Y\015=\305\027\304$r\222=\302\037e",LLAssetType::AT_SOUND},
#line 22 "common.textures.gperf"
      {"\020\322\240\032\010\030\204\271K\226\302\353c%e\031",LLAssetType::AT_TEXTURE},
#line 21 "common.textures.gperf"
      {"\001\207\272\277l\015X\221\353\355N\312\261Bf\203",LLAssetType::AT_TEXTURE},
#line 74 "common.textures.gperf"
      {"g\314(D\000\363+<\271\221d\030\320\036\033\267",LLAssetType::AT_SOUND}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = uuid_hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        if (len == lengthtable[key])
          {
            register const char *s = common_uuids[key].digest;

            if (s && *str == *s && !memcmp (str + 1, s + 1, len - 1))
              return &common_uuids[key];
          }
    }
  return 0;
}
#line 89 "common.textures.gperf"

namespace AIMultiGrid {

AICommonUUID const* is_common_uuid(LLUUID const& id)
{
  return CommonUUIDHash::is_common((char const*)id.mData, 16);
}

} // namespace AIMultiGrid
