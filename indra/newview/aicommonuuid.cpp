/* C++ code produced by gperf version 3.0.3 */
/* Command-line: gperf -n -m 10 common.textures.gperf  */
/* Computed positions: -k'1,7' */


#include "llviewerprecompiledheaders.h"
#include "llassettype.h"
#include "lluuid.h"
#include "aicommonuuid.h"
/* maximum key range = 219, duplicates = 0 */

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
      109,  70, 219,  75, 137,  28, 108,  99,  58, 119,
      107,  12,  50, 107,  62,  74,  60, 106, 144, 219,
      205, 174, 160,  34,  16,  67,  65, 219, 157,  34,
      104, 219, 199,  11,  49,  29,  39,   6,  70, 219,
       28, 142,  10,  12,  11,  18, 219,  60,  35,  66,
      219,  72, 163,  25,  50, 187, 219,  86, 158, 141,
       17,  21, 103, 219, 157,  10,  26,  96,  95, 178,
      173, 114,   9,   5,  70, 110, 139, 219,   3, 118,
      219,  49, 219, 174,  11, 173,  69,   2, 146, 100,
       95,  82,  83,  90,  49, 219, 219, 166,   1, 152,
       48, 174, 172,  16,  14,  21, 219,  66,  50,  13,
       14,  47,   4, 219,  70, 103, 219,  28,  12, 159,
      219,  13,  10, 165,   9,  33,  12,  95, 119, 148,
        4,  14, 102,  90, 113, 219,  44,  95,  10,  73,
      219, 156, 112,  38, 219, 106,  54,  86, 219,  38,
        0, 219, 219,   8,   7,  27,  87,  84,  80,   1,
       31, 144,   6, 145,  70,  32, 138,  95,  76,  12,
        9, 133, 219, 105,  18, 119,  24,  25, 219, 111,
      219, 105,  24,  70,   8,  52,  88,  27, 115,  49,
       74,  35,   9,  14,  12, 124,  69,  71, 219, 219,
        6, 219,  93, 219, 219,  23, 111, 219, 219, 111,
      107, 219,  66,  53,   0,  25,  35,  70,  15,   5,
       95, 219,  71,  28,  48, 103, 219, 219,  80, 219,
       53, 219,  53, 219,  52,  11,   8,  36,   2,  15,
       70,  41,  48,  22,  39,  52,   7,   3, 219,  69,
      219,  81,  66,  40,  27,  53
    };
  return asso_values[(unsigned char)str[6]] + asso_values[(unsigned char)str[0]];
}

const struct AICommonUUID *
CommonUUIDHash::is_common (register const char *str, register unsigned int len)
{
  enum
    {
      TOTAL_KEYWORDS = 218,
      MIN_WORD_LENGTH = 16,
      MAX_WORD_LENGTH = 16,
      MIN_HASH_VALUE = 0,
      MAX_HASH_VALUE = 218
    };

  static const unsigned char lengthtable[] =
    {
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16,  0, 16
    };
  static const struct AICommonUUID common_uuids[] =
    {
#line 214 "common.textures.gperf"
      {"\326;\301\371\374\201\226%\240\306\000qv\330.\267",LLAssetType::AT_ANIMATION},
#line 151 "common.textures.gperf"
      {"bW\010B\011P\226\3704\034\200\236e\021\010#",LLAssetType::AT_ANIMATION},
#line 60 "common.textures.gperf"
      {"\326\221\240\034\023\267W\215W\300\\\256\360\264\347\341",LLAssetType::AT_TEXTURE},
#line 152 "common.textures.gperf"
      {"b\305\336X\3133WC=\007\236L\3245(d",LLAssetType::AT_ANIMATION},
#line 213 "common.textures.gperf"
      {"\326\014A\322|$pt\323\372a\001\316\242*Q",LLAssetType::AT_ANIMATION},
#line 184 "common.textures.gperf"
      {"\237Ik\322X\232p\237\026\314i\277}\361\323l",LLAssetType::AT_ANIMATION},
#line 218 "common.textures.gperf"
      {"\333\225V\037\361\260\237\232r$\261/q\257\022n",LLAssetType::AT_ANIMATION},
#line 217 "common.textures.gperf"
      {"\333\204\202\233F,\356\203\036'\233\276\346k\326$",LLAssetType::AT_ANIMATION},
#line 225 "common.textures.gperf"
      {"\356\374y\276\332\256\2429\214\004\211\017]#eJ",LLAssetType::AT_ANIMATION},
#line 235 "common.textures.gperf"
      {"\367l\332\224A\324\242)(r\340)nX\257\341",LLAssetType::AT_ANIMATION},
#line 88 "common.textures.gperf"
      {"\366\272\230\026\334\257\367U{gQ\263\033b3\345",LLAssetType::AT_SOUND},
#line 43 "common.textures.gperf"
      {"\202-\355I\232l\366\034\313\211m\365OB\315\364",LLAssetType::AT_TEXTURE},
#line 168 "common.textures.gperf"
      {"\202\233\310[\002\374\354A\276.t\314m\327!]",LLAssetType::AT_ANIMATION},
#line 159 "common.textures.gperf"
      {"p\236\242\216\025s\300#\213\370R\014\213\3067\372",LLAssetType::AT_ANIMATION},
#line 201 "common.textures.gperf"
      {"\300\304\003\017\300+I\336$\272#1\364?\344\034",LLAssetType::AT_ANIMATION},
#line 139 "common.textures.gperf"
      {"I\256\244;Z\303\212D\265\225\226\020\012\360\276\332",LLAssetType::AT_ANIMATION},
#line 80 "common.textures.gperf"
      {"\310\002`\272A\375\212Fv\212k\36266\016:",LLAssetType::AT_SOUND},
#line 224 "common.textures.gperf"
      {"\354\225,\312a\357\252;'\211M\023D\360\026\336",LLAssetType::AT_ANIMATION},
#line 81 "common.textures.gperf"
      {"\310%\337\274\230'~\002e\0077\023\321\211\026\301",LLAssetType::AT_SOUND},
#line 188 "common.textures.gperf"
      {"\252\023D\004}\254z\312,\272C_\235\270u\312",LLAssetType::AT_ANIMATION},
#line 164 "common.textures.gperf"
      {"z\027\260Y\022\262A\261W\012\030ch\266\252o",LLAssetType::AT_ANIMATION},
#line 228 "common.textures.gperf"
      {"\357\334\027'\213\212\310\000@w\227_\302~\342\362",LLAssetType::AT_ANIMATION},
#line 133 "common.textures.gperf"
      {"ABh6t7~\211\002]\012\244\321\017\035i",LLAssetType::AT_ANIMATION},
#line 51 "common.textures.gperf"
      {"\231\275`\2422P\357\311.9/\274\255\357\276\314",LLAssetType::AT_TEXTURE},
#line 226 "common.textures.gperf"
      {"\357b\323U\310\025H\026$t\261\254\302\020\224\246",LLAssetType::AT_ANIMATION},
#line 37 "common.textures.gperf"
      {"g\223\0231\014\002Hv\022U(w\010\226\306\242",LLAssetType::AT_TEXTURE},
#line 28 "common.textures.gperf"
      {"-xDv\320\333\231y\014\377\224\010tZ|\363",LLAssetType::AT_TEXTURE},
#line 205 "common.textures.gperf"
      {"\310\344-2s\020i\006\311\003\312\265\324\243FV",LLAssetType::AT_ANIMATION},
#line 74 "common.textures.gperf"
      {"g\314(D\000\363+<\271\221d\030\320\036\033\267",LLAssetType::AT_SOUND},
#line 160 "common.textures.gperf"
      {"p\352qO:\227\327B\033\001Y\012\217\315\035\265",LLAssetType::AT_ANIMATION},
#line 70 "common.textures.gperf"
      {"=\011\365\2028Q\300\340\365\272'z\305\307?\264",LLAssetType::AT_SOUND},
#line 25 "common.textures.gperf"
      {"*H\200\266\267\243i\012 I\277\2768\352\373\237",LLAssetType::AT_TEXTURE},
#line 132 "common.textures.gperf"
      {"=\241\327S\002\212TF$\363\234\233\205m\224\"",LLAssetType::AT_ANIMATION},
#line 26 "common.textures.gperf"
      {"+\3758\204~'i\271\272:>g?h\000\004",LLAssetType::AT_TEXTURE},
#line 121 "common.textures.gperf"
      {"-m\252Q1\222g\224\216.\241_\2038\3540",LLAssetType::AT_ANIMATION},
#line 83 "common.textures.gperf"
      {"\327\251\245e\240\023*iy}S2\272\241\251G",LLAssetType::AT_SOUND},
#line 192 "common.textures.gperf"
      {"\261p\234\215\354\323T\241O(\325Z\300\204\007\202",LLAssetType::AT_ANIMATION},
#line 193 "common.textures.gperf"
      {"\261\355y\202\306\216\251\202uaR\250\212R\230\300",LLAssetType::AT_ANIMATION},
#line 136 "common.textures.gperf"
      {"B\354\320\013\231G\251|@\012\273\311\027Lz\353",LLAssetType::AT_ANIMATION},
#line 134 "common.textures.gperf"
      {"B\264b\024KDy\256\336\270\015\366\024$\377K",LLAssetType::AT_ANIMATION},
#line 180 "common.textures.gperf"
      {"\233\014\034N\212\307yi\024\224(\310t\304\366h",LLAssetType::AT_ANIMATION},
#line 103 "common.textures.gperf"
      {"\027\300$\314\356\362\366\2405'\230i\207mwR",LLAssetType::AT_ANIMATION},
#line 198 "common.textures.gperf"
      {"\270\310\262\243\220\010\027q;\374\220\222IU\253-",LLAssetType::AT_ANIMATION},
#line 55 "common.textures.gperf"
      {"\270\323\226Z\255x\277Ci\233\277\370\354\246\311u",LLAssetType::AT_TEXTURE},
#line 178 "common.textures.gperf"
      {"\225\024i\364\307\262\310\030\235\356\255~\352\2140\267",LLAssetType::AT_ANIMATION},
#line 179 "common.textures.gperf"
      {"\232\250\260\246\014o\225\030\307\303OA\362\300\001\255",LLAssetType::AT_ANIMATION},
#line 66 "common.textures.gperf"
      {"$*\370+C\302\232;\341\010;\014~8I\201",LLAssetType::AT_SOUND},
#line 215 "common.textures.gperf"
      {"\330?\240\345\227\355~\262\347\230{\320\006!\\\264",LLAssetType::AT_ANIMATION},
#line 208 "common.textures.gperf"
      {"\315vh\246p\021\327\342\352\330\374i\357\361\241\004",LLAssetType::AT_ANIMATION},
#line 202 "common.textures.gperf"
      {"\301\274\1776;\240\330D\371<\223\276\224]dO",LLAssetType::AT_ANIMATION},
#line 207 "common.textures.gperf"
      {"\315(\266\233\234\225\273x?\224\215`_\361\273\022",LLAssetType::AT_ANIMATION},
#line 78 "common.textures.gperf"
      {"\273\344\307\374pD\260^{\2116\222JgY<",LLAssetType::AT_SOUND},
#line 79 "common.textures.gperf"
      {"\301\3634\373\245\276\217\347\"\263)c\034!\317\013",LLAssetType::AT_SOUND},
#line 116 "common.textures.gperf"
      {"#\005\275u\034\251\260;\037\252\261v\270\250\304\236",LLAssetType::AT_ANIMATION},
#line 131 "common.textures.gperf"
      {"=\224\272\320\305[}\314\207c\003<Y@]3",LLAssetType::AT_ANIMATION},
#line 166 "common.textures.gperf"
      {"}\260\014\315\363\200\363\356C\235a\226\216\306\234\212",LLAssetType::AT_ANIMATION},
#line 232 "common.textures.gperf"
      {"\3630\012\3314b\035\007 D\017\357\200\006-\240",LLAssetType::AT_ANIMATION},
#line 165 "common.textures.gperf"
      {"zN\207\376\3369o\313b#\002K\000\2112D",LLAssetType::AT_ANIMATION},
#line 27 "common.textures.gperf"
      {",\257\021yxao\363K}F\341w\200\275\372",LLAssetType::AT_TEXTURE},
#line 230 "common.textures.gperf"
      {"\362/\355\213\245\355,\223d\325\275\330\271<\210\237",LLAssetType::AT_ANIMATION},
#line 222 "common.textures.gperf"
      {"\352\350\220['\032\231\342L\0161\020j\375\020\014",LLAssetType::AT_ANIMATION},
#line 23 "common.textures.gperf"
      {"\027\234\332\2759\212\233k\023\221M\3033\2722\037",LLAssetType::AT_TEXTURE},
#line 171 "common.textures.gperf"
      {"\203\377Y\376#F\3626\220\011N6\010\257d\301",LLAssetType::AT_ANIMATION},
#line 170 "common.textures.gperf"
      {"\203Ye\306\177/\275\242]\353$xs\177\221\277",LLAssetType::AT_ANIMATION},
#line 163 "common.textures.gperf"
      {"v\223\362h\006\307\352q\372!+0\326S?\217",LLAssetType::AT_ANIMATION},
#line 104 "common.textures.gperf"
      {"\030\263\244\265\264c\275H\344\266q\352\254v\305\025",LLAssetType::AT_ANIMATION},
#line 177 "common.textures.gperf"
      {"\222\214\256\030\343\035v\375\234\311/U\026\017\370\030",LLAssetType::AT_ANIMATION},
#line 49 "common.textures.gperf"
      {"\217E\205I\027;#\377\324\377\277\252^\2427\033",LLAssetType::AT_TEXTURE},
#line 221 "common.textures.gperf"
      {"\352c4\023\200\006\030\012\303\272\226\335\035ug ",LLAssetType::AT_ANIMATION},
#line 97 "common.textures.gperf"
      {"\016\267\002\342\314Z\232\210V\245f\032U\300gj",LLAssetType::AT_ANIMATION},
#line 44 "common.textures.gperf"
      {"\210r\362\2701\333B\330X\012\263\344\251\022b\336",LLAssetType::AT_TEXTURE},
#line 63 "common.textures.gperf"
      {"\014\267\260\012L\020iH\204\336\251<\011\257+\251",LLAssetType::AT_SOUND},
#line 50 "common.textures.gperf"
      {"\226\264\3361\364\3723}\354xE\0366\011v\236",LLAssetType::AT_TEXTURE},
#line 117 "common.textures.gperf"
      {"$\010\376\236\337\035\035}\364\377\023\204\372{5\017",LLAssetType::AT_ANIMATION},
#line 118 "common.textures.gperf"
      {"$_<T\361\300\277.\201\037F\330\356\263\206\347",LLAssetType::AT_ANIMATION},
#line 108 "common.textures.gperf"
      {"\032_\350\254\250\004\212]|\275V\275\203\030Eh",LLAssetType::AT_ANIMATION},
#line 109 "common.textures.gperf"
      {"\032\261\2626\315\010!\346\014\274\015\222?\306\354\242",LLAssetType::AT_ANIMATION},
#line 65 "common.textures.gperf"
      {"!\234]\223l\0111\305\373?\305\376t\225\301\025",LLAssetType::AT_SOUND},
#line 125 "common.textures.gperf"
      {"1\\:A\245\363\013\244'\332\370\223\367i\346\233",LLAssetType::AT_ANIMATION},
#line 189 "common.textures.gperf"
      {"\252-\370M\317\217r\030R{BJR\336vn",LLAssetType::AT_ANIMATION},
#line 91 "common.textures.gperf"
      {"\0106\266\177\177{\363{\300\012F\015\301R\037Z",LLAssetType::AT_ANIMATION},
#line 71 "common.textures.gperf"
      {"At\370Y\015=\305\027\304$r\222=\302\037e",LLAssetType::AT_SOUND},
#line 90 "common.textures.gperf"
      {"\005\335\277\370\252\251\222\241+t\217\347z)\264E",LLAssetType::AT_ANIMATION},
#line 76 "common.textures.gperf"
      {"z\377\"e\320[\213rc\307\333\371m\302\362\037",LLAssetType::AT_SOUND},
#line 94 "common.textures.gperf"
      {"\013\214\202\021\327\2143\350\372(\305\032\225\224\344$",LLAssetType::AT_ANIMATION},
#line 89 "common.textures.gperf"
      {"\003\217\316\311^\275\212\216\016.nq\240\241\254S",LLAssetType::AT_ANIMATION},
#line 64 "common.textures.gperf"
      {"\020It\343\337\332B\213\231\356\260\324\347H\323\243",LLAssetType::AT_SOUND},
#line 237 "common.textures.gperf"
      {"\376\343\337H\372=\020\025\036&\242\005\201\0160\001",LLAssetType::AT_ANIMATION},
#line 236 "common.textures.gperf"
      {"\375\003q4\205\324\362Ar\306OB\026O\355\356",LLAssetType::AT_ANIMATION},
#line 106 "common.textures.gperf"
      {"\032\003\265u\2264\266*Wg:g\236\201\364\336",LLAssetType::AT_ANIMATION},
#line 196 "common.textures.gperf"
      {"\266\212=|\336\236\374\207\356\310T=x~[\015",LLAssetType::AT_ANIMATION},
#line 46 "common.textures.gperf"
      {"\212QX\211\352\311\373U\216\272\322\334\011\3532\310",LLAssetType::AT_TEXTURE},
#line 59 "common.textures.gperf"
      {"\315\331\251\374m\013\371\015\204\026\307+`\031\274\250",LLAssetType::AT_TEXTURE},
#line 233 "common.textures.gperf"
      {"\364\360\015n\271\376\222\222\364\313\012\340n\245\215W",LLAssetType::AT_ANIMATION},
#line 95 "common.textures.gperf"
      {"\014]\322\242QM\210\223\324M\005\276\377\255 \213",LLAssetType::AT_ANIMATION},
#line 176 "common.textures.gperf"
      {"\222bM>\020h\361\252\245\354\202DXQ\223\355",LLAssetType::AT_ANIMATION},
#line 181 "common.textures.gperf"
      {"\233)\315a\304[V\211\336\322\221uk\215v\251",LLAssetType::AT_ANIMATION},
#line 162 "common.textures.gperf"
      {"up\307\265\037\"V\335V\357\251\026\202A\273\266",LLAssetType::AT_ANIMATION},
#line 229 "common.textures.gperf"
      {"\360ar=\012\030uOf\356)\244G\225\243/",LLAssetType::AT_ANIMATION},
#line 204 "common.textures.gperf"
      {"\305A\304\177\340\300\005\213\255\032\326\256:E\204\331",LLAssetType::AT_ANIMATION},
#line 122 "common.textures.gperf"
      {"0\004wx\020\352\032\367h\201M\267\243\245\241\024",LLAssetType::AT_ANIMATION},
#line 211 "common.textures.gperf"
      {"\324Ao\361\011\3230\017A\203\033h\241\233\237\301",LLAssetType::AT_ANIMATION},
#line 86 "common.textures.gperf"
      {"\355\022Gdp]\324\227\026z\030,\331\372.l",LLAssetType::AT_SOUND},
#line 206 "common.textures.gperf"
      {"\312[?\0241\224z+\310\224\252i\233q\215\037",LLAssetType::AT_ANIMATION},
#line 58 "common.textures.gperf"
      {"\312N\214'G<\353\034/]P\356?\007\330\\",LLAssetType::AT_TEXTURE},
#line 234 "common.textures.gperf"
      {"\365\374t3\004=\350\031\202\230\365\031\241\031\266\210",LLAssetType::AT_ANIMATION},
#line 220 "common.textures.gperf"
      {"\346\350\321\335\346C\377\367\2628\306\264\260V\246\215",LLAssetType::AT_ANIMATION},
#line 182 "common.textures.gperf"
      {"\233\241\311B\010\276\344:\373)\026\255D\016\374P",LLAssetType::AT_ANIMATION},
#line 85 "common.textures.gperf"
      {"\340W\302DWh\020V\303~\0257EN\353b",LLAssetType::AT_SOUND},
#line 61 "common.textures.gperf"
      {"\373*\342\004?\321\3373YO\311\370\202\203\016f",LLAssetType::AT_TEXTURE},
#line 87 "common.textures.gperf"
      {"\364\240f\017TF\336\242\200\267d\202\240\202\200<",LLAssetType::AT_SOUND},
#line 24 "common.textures.gperf"
      {"\030\373\210\213\350\361\334\347}\2472\035e\036\246\260",LLAssetType::AT_TEXTURE},
#line 40 "common.textures.gperf"
      {"yPK\365\303\354\007cec\330C\336f\320\241",LLAssetType::AT_TEXTURE},
#line 154 "common.textures.gperf"
      {"h\002\325SI\332\007x\237\205\025\231\242&e&",LLAssetType::AT_ANIMATION},
#line 155 "common.textures.gperf"
      {"h\203\246\032\262{Y\024\246\036\335\241\030\251\356,",LLAssetType::AT_ANIMATION},
#line 216 "common.textures.gperf"
      {"\332\002\005%M\224Y\326#\327\201\375\353\3631H",LLAssetType::AT_ANIMATION},
#line 96 "common.textures.gperf"
      {"\016H\226\313\373\244\222l\363U\207 \030\235[U",LLAssetType::AT_ANIMATION},
#line 84 "common.textures.gperf"
      {"\331\367<\370\027\264oz\025eyQ\"l0]",LLAssetType::AT_SOUND},
#line 124 "common.textures.gperf"
      {"1G\330\025c8\2712\360\021\026\265m\232\301\213",LLAssetType::AT_ANIMATION},
#line 199 "common.textures.gperf"
      {"\271\006\304\272p;\031@2\243\014\177}y\025\020",LLAssetType::AT_ANIMATION},
#line 105 "common.textures.gperf"
      {"\031\231\224\006::\325\214\242\254\327.U]\317Q",LLAssetType::AT_ANIMATION},
#line 68 "common.textures.gperf"
      {",\250I\272(\205K\303\220\357\324\230z[\230:",LLAssetType::AT_SOUND},
#line 57 "common.textures.gperf"
      {"\302(\321\317K]K\250\204\364\211\232\007\226\252\227",LLAssetType::AT_TEXTURE},
#line 82 "common.textures.gperf"
      {"\321f\003\233\264\365\302\354I\021\310\\r{\001l",LLAssetType::AT_SOUND},
#line 158 "common.textures.gperf"
      {"n\322K\330\221\252K\022\314\307\311|\205z\264\340",LLAssetType::AT_ANIMATION},
#line 209 "common.textures.gperf"
      {"\316\230c%\013\247nn\314$\261|KyUx",LLAssetType::AT_ANIMATION},
#line 145 "common.textures.gperf"
      {"WG\244\216\007>\3031\366\363|!Ia=>",LLAssetType::AT_ANIMATION},
#line 56 "common.textures.gperf"
      {"\276\261i\307\021\352\377\362\357\345\017$\334\210\035\362",LLAssetType::AT_TEXTURE},
#line 161 "common.textures.gperf"
      {"r\206F\331\314y\010\2622\326\223\177\012\203\\$",LLAssetType::AT_ANIMATION},
#line 174 "common.textures.gperf"
      {"\206\236\315\255\244Kg\0362fV\256\362\343\254.",LLAssetType::AT_ANIMATION},
#line 69 "common.textures.gperf"
      {"<\217\307&\037\326\206-\372\001\026\305\262V\215\266",LLAssetType::AT_SOUND},
#line 31 "common.textures.gperf"
      {"<Y\367\376\235\310G\371\212\257\251\335\037\274;\357",LLAssetType::AT_TEXTURE},
#line 138 "common.textures.gperf"
      {"G\365\366\373\"\345\256D\370qs\252\257J`\"",LLAssetType::AT_ANIMATION},
#line 190 "common.textures.gperf"
      {"\256\304a\014u\177\274N\300\222\306\351\312\361\215\257",LLAssetType::AT_ANIMATION},
#line 231 "common.textures.gperf"
      {"\362\276\325\371\235D9\257\260\315%{*\027\376@",LLAssetType::AT_ANIMATION},
#line 150 "common.textures.gperf"
      {"^\243\231\037\302\2239.h`\221\337\240\022x\243",LLAssetType::AT_ANIMATION},
#line 73 "common.textures.gperf"
      {"^\031\034{\211\226\234\355\241w\262\2542\277\352\006",LLAssetType::AT_SOUND},
#line 183 "common.textures.gperf"
      {"\234\005\345\307o\007l\244\355Z\26209\0149P",LLAssetType::AT_ANIMATION},
#line 38 "common.textures.gperf"
      {"lG'\270\254y\272D;\201\371\252\210{G\353",LLAssetType::AT_TEXTURE},
#line 197 "common.textures.gperf"
      {"\267\307\3103\343\323\304\343\237\300\023\0227Dc\022",LLAssetType::AT_ANIMATION},
#line 195 "common.textures.gperf"
      {"\265\264\246}\012\3560\322r\315w\2633\3512\357",LLAssetType::AT_ANIMATION},
#line 141 "common.textures.gperf"
      {"K\326\232\035\021\024\240\264b_\204\340\245#qU",LLAssetType::AT_ANIMATION},
#line 194 "common.textures.gperf"
      {"\263\022\261\016e\253\240\244\213<\023&\352\216>\331",LLAssetType::AT_ANIMATION},
#line 191 "common.textures.gperf"
      {"\260\334A|\037\021\2576.\200~t\211\372|\334",LLAssetType::AT_ANIMATION},
#line 67 "common.textures.gperf"
      {",4n\332\266\014\2533\021\031\270\224\031\026\244\231",LLAssetType::AT_SOUND},
#line 140 "common.textures.gperf"
      {"J\350\001k1\271\003\273\304\001\261\352\224\035\264\035",LLAssetType::AT_ANIMATION},
#line 53 "common.textures.gperf"
      {"\250Z\306t\313uJ\366\224\231\337|Z\257z(",LLAssetType::AT_TEXTURE},
#line 200 "common.textures.gperf"
      {"\271.\301\245\347\316\247k+\005\274\333\223\021A~",LLAssetType::AT_ANIMATION},
#line 175 "common.textures.gperf"
      {"\213\020&\027\274\272\003{\206\301\267b\031\371\014\210",LLAssetType::AT_ANIMATION},
#line 52 "common.textures.gperf"
      {"\246\026!3rKT\337\241/Q\315\007\012\326\363",LLAssetType::AT_TEXTURE},
#line 167 "common.textures.gperf"
      {"\200p\0041t\354\240\010\024\370wW^si?",LLAssetType::AT_ANIMATION},
#line 186 "common.textures.gperf"
      {"\245M\216\342(\273\200\251\177\014z\373\276$\245\326",LLAssetType::AT_ANIMATION},
#line 149 "common.textures.gperf"
      {"\\x\016\250\034\321\304c\241(H\300#\366\373\352",LLAssetType::AT_ANIMATION},
#line 148 "common.textures.gperf"
      {"\\h*\225m\244\244c\013\366\017[{\341)\321",LLAssetType::AT_ANIMATION},
#line 185 "common.textures.gperf"
      {"\243Q\261\274\314\224\252\302{\352\247\346\353\255\025\357",LLAssetType::AT_ANIMATION},
#line 223 "common.textures.gperf"
      {"\353n\277\262\244\263\241\234\323\210M\325\3008#\367",LLAssetType::AT_ANIMATION},
#line 187 "common.textures.gperf"
      {"\250\336\345o.\256\236z\005\242o\271+\227\342\036",LLAssetType::AT_ANIMATION},
#line 47 "common.textures.gperf"
      {"\213_\354e\215\215\235\305\315\250\217\337'\026\343a",LLAssetType::AT_TEXTURE},
#line 126 "common.textures.gperf"
      {"33\221v}\334\223\227\224\244\2774\003\313\310\365",LLAssetType::AT_ANIMATION},
#line 35 "common.textures.gperf"
      {"d6{\321i~\263\346\013e?\206*Wsf",LLAssetType::AT_TEXTURE},
#line 77 "common.textures.gperf"
      {"\216\256\326\037\222\377d\205\336\203M\314\223\212G\216",LLAssetType::AT_SOUND},
#line 48 "common.textures.gperf"
      {"\215\315JH-7I\011\237x\367\251\353N\371\003",LLAssetType::AT_TEXTURE},
#line 22 "common.textures.gperf"
      {"\020\322\240\032\010\030\204\271K\226\302\353c%e\031",LLAssetType::AT_TEXTURE},
#line 227 "common.textures.gperf"
      {"\357\317g\014-\030\201(\227:\003N\274\200kg",LLAssetType::AT_ANIMATION},
#line 144 "common.textures.gperf"
      {"V\340\272\015J\237\177'a\0272\362\353\277a5",LLAssetType::AT_ANIMATION},
#line 110 "common.textures.gperf"
      {"\034v\000\326f\037\270{\357\342\327B\036\271<\206",LLAssetType::AT_ANIMATION},
#line 42 "common.textures.gperf"
      {"|\014\370\233D\261\034\342\335t\007\020*\230\254*",LLAssetType::AT_TEXTURE},
#line 146 "common.textures.gperf"
      {"W\253\252\346\035\027{\033_\230m\021\246A\022v",LLAssetType::AT_ANIMATION},
#line 75 "common.textures.gperf"
      {"w\240\030\257\011\216\3007Q\246\027\217\005\207|o",LLAssetType::AT_SOUND},
#line 123 "common.textures.gperf"
      {"1;\230\201C\002s\300\307\320\016z6\266\302$",LLAssetType::AT_ANIMATION},
#line 120 "common.textures.gperf"
      {"+Z8\262^\000:\227\244\225L\202k\304C\346",LLAssetType::AT_ANIMATION},
#line 30 "common.textures.gperf"
      {":6}\034\276\361mCu\225\350\214\036:\255\263",LLAssetType::AT_TEXTURE},
#line 107 "common.textures.gperf"
      {"\032+\325\216\207\377\015\370\013LS\340G\260\273n",LLAssetType::AT_ANIMATION},
#line 156 "common.textures.gperf"
      {"ka\310\350GG\015u\022\327\344\237\362\007\244\312",LLAssetType::AT_ANIMATION},
#line 153 "common.textures.gperf"
      {"fc\007\331\250`W-o\324\303\253\210e\300\224",LLAssetType::AT_ANIMATION},
#line 32 "common.textures.gperf"
      {"WH\336\314\366)F\034\2326\243Z\"\037\342\037",LLAssetType::AT_TEXTURE},
#line 137 "common.textures.gperf"
      {"F\273CY\3368N\330j\"\361\365/\350\365\006",LLAssetType::AT_ANIMATION},
#line 36 "common.textures.gperf"
      {"e\"\347M\026`N\177\266\001oH\301e\232w",LLAssetType::AT_TEXTURE},
#line 135 "common.textures.gperf"
      {"B\335\225\325\013\306c\222\366Pws\004\224l\017",LLAssetType::AT_ANIMATION},
#line 39 "common.textures.gperf"
      {"m\343~Np)a\365T\270\365\346?\230?X",LLAssetType::AT_TEXTURE},
#line 173 "common.textures.gperf"
      {"\205\231P&\352\336]x\323d\224\246E\022\313f",LLAssetType::AT_ANIMATION},
#line 34 "common.textures.gperf"
      {"[\301\034\326/@\007\036\250\332\011\0039B\004\371",LLAssetType::AT_TEXTURE},
#line 33 "common.textures.gperf"
      {"X\224\342\347\253\215\355\372\346\034\030\317\026\205K\243",LLAssetType::AT_TEXTURE},
#line 119 "common.textures.gperf"
      {"*\216\272\035\247\370U\226\324J\264\227{\370\310\273",LLAssetType::AT_ANIMATION},
#line 41 "common.textures.gperf"
      {"z+:JS\302S\254W\026\252\307\327C\300 ",LLAssetType::AT_TEXTURE},
#line 219 "common.textures.gperf"
      {"\340ME\015\375\265\0042\375h\201\212\257Y5\370",LLAssetType::AT_ANIMATION},
#line 143 "common.textures.gperf"
      {"QJ\364\210\220Q\004J\263\374\324\333\367cw\306",LLAssetType::AT_ANIMATION},
#line 203 "common.textures.gperf"
      {"\304\312a\210\221'O1\001X#\304\342\3713\004",LLAssetType::AT_ANIMATION},
#line 72 "common.textures.gperf"
      {"L\214<w\336\215\275\342\271\2702c^\017\324\246",LLAssetType::AT_SOUND},
#line 115 "common.textures.gperf"
      {"!J\246\301\272jEx\362|\316v\210\366\035\015",LLAssetType::AT_ANIMATION},
#line 147 "common.textures.gperf"
      {"Z\227~\331\177rD\351LLn\221=\370\256t",LLAssetType::AT_ANIMATION},
#line 45 "common.textures.gperf"
      {"\211UgG$\313C\355\222\013G\312\355\025F_",LLAssetType::AT_TEXTURE},
#line 29 "common.textures.gperf"
      {"0\004|\354&\235@\216\0140\262`;\210rh",LLAssetType::AT_TEXTURE},
#line 172 "common.textures.gperf"
      {"\205B\206\200k\371>d\264\211o\201\010|$\275",LLAssetType::AT_ANIMATION},
#line 212 "common.textures.gperf"
      {"\3255G\033\205\277;M\245B\223\276\244\365\2353",LLAssetType::AT_ANIMATION},
#line 130 "common.textures.gperf"
      {"7\017: l\246\231q\204\214\232\001\274B\256<",LLAssetType::AT_ANIMATION},
#line 129 "common.textures.gperf"
      {"6\370\032\222\360vX\223\334K|7\225\344\207\317",LLAssetType::AT_ANIMATION},
#line 128 "common.textures.gperf"
      {"5\333O~(\302fy\316\251>\341\010\367\374\177",LLAssetType::AT_ANIMATION},
#line 127 "common.textures.gperf"
      {"4\2328\001T\371\277,;\320\032\310\227r\257\001",LLAssetType::AT_ANIMATION},
#line 142 "common.textures.gperf"
      {"LZ\020>\2700/\034\026\274\"J\240\255[\310",LLAssetType::AT_ANIMATION},
#line 92 "common.textures.gperf"
      {"\010FOx:\216)D\313\245\014\224\257\363\257)",LLAssetType::AT_ANIMATION},
#line 100 "common.textures.gperf"
      {"\025F\216\0004\000\273f\316\314dm|\024E\216",LLAssetType::AT_ANIMATION},
#line 101 "common.textures.gperf"
      {"\025\335\221\035\276\202(V&\333'e\233\024(u",LLAssetType::AT_ANIMATION},
#line 54 "common.textures.gperf"
      {"\253\267\203\346>\223&\300$\212$vf\205]\243",LLAssetType::AT_TEXTURE},
#line 113 "common.textures.gperf"
      {" \037?\337\313\037\333\354 \037s3\343(\256|",LLAssetType::AT_ANIMATION},
#line 114 "common.textures.gperf"
      {" \360c\352\203\006%b\013\007\\\205;7\263\036",LLAssetType::AT_ANIMATION},
#line 111 "common.textures.gperf"
      {"\034\265b\260\272!\"\002\357\2630\370,\337\225\225",LLAssetType::AT_ANIMATION},
#line 112 "common.textures.gperf"
      {"\036\215\220\314\250N\3415\210L|\202\310\260:\024",LLAssetType::AT_ANIMATION},
#line 102 "common.textures.gperf"
      {"\026\200:\237Q@\340BM{\322\213\242G\303%",LLAssetType::AT_ANIMATION},
#line 169 "common.textures.gperf"
      {"\202\351\2220\311\006\024\003M\2348\211\335\230\332\272",LLAssetType::AT_ANIMATION},
#line 157 "common.textures.gperf"
      {"k\320\030`N\275\022z\273=\321B~\216\014B",LLAssetType::AT_ANIMATION},
#line 99 "common.textures.gperf"
      {"\021\000\006\224?A\255\302`k\356\341\326o7$",LLAssetType::AT_ANIMATION},
#line 98 "common.textures.gperf"
      {"\017\206\343U\3351\246\034\375\260:\226\271\252\320_",LLAssetType::AT_ANIMATION},
#line 93 "common.textures.gperf"
      {"\012\237\271p\213D\221\024\323\251\277i\317\350\004\326",LLAssetType::AT_ANIMATION},
#line 62 "common.textures.gperf"
      {"\011\262\030N\206\001D\342\257\273\3167CK\213\241",LLAssetType::AT_SOUND},
#line 210 "common.textures.gperf"
      {"\322\362\356X\212\321\006\311\330\3238'\2721Vz",LLAssetType::AT_ANIMATION},
#line 21 "common.textures.gperf"
      {"\001\207\272\277l\015X\221\353\355N\312\261Bf\203",LLAssetType::AT_TEXTURE},
      {(char*)0},
#line 238 "common.textures.gperf"
      {"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000",LLAssetType::AT_NONE}
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
#line 239 "common.textures.gperf"

namespace AIMultiGrid {

AICommonUUID const* is_common_uuid(LLUUID const& id)
{
  return CommonUUIDHash::is_common((char const*)id.mData, 16);
}

} // namespace AIMultiGrid
