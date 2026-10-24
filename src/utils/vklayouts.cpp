#include "vklayouts.h"

#include <cstdint>
#include <vector>

namespace freewb
{

struct VkLayoutEntry
{
    VkKey key;
    const char *normal;
    const char *shift;
};

static const VkLayoutEntry kGreek[] = {
    {VK_KEY_A, u8"κ", u8"Κ"},
    {VK_KEY_B, u8"χ", u8"Χ"},
    {VK_KEY_C, u8"υ", u8"Υ"},
    {VK_KEY_D, u8"μ", u8"Μ"},
    {VK_KEY_E, u8"γ", u8"Γ"},
    {VK_KEY_F, u8"ν", u8"Ν"},
    {VK_KEY_G, u8"ξ", u8"Ξ"},
    {VK_KEY_H, u8"ο", u8"Ο"},
    {VK_KEY_I, u8"θ", u8"Θ"},
    {VK_KEY_J, u8"π", u8"Π"},
    {VK_KEY_K, u8"ρ", u8"Ρ"},
    {VK_KEY_M, u8"ω", u8"Ω"},
    {VK_KEY_N, u8"ψ", u8"Ψ"},
    {VK_KEY_O, u8"ι", u8"Ι"},
    {VK_KEY_Q, u8"α", u8"Α"},
    {VK_KEY_R, u8"δ", u8"Δ"},
    {VK_KEY_S, u8"λ", u8"Λ"},
    {VK_KEY_T, u8"ε", u8"Ε"},
    {VK_KEY_U, u8"η", u8"Η"},
    {VK_KEY_V, u8"φ", u8"Φ"},
    {VK_KEY_W, u8"β", u8"Β"},
    {VK_KEY_X, u8"τ", u8"Τ"},
    {VK_KEY_Y, u8"ζ", u8"Ζ"},
    {VK_KEY_Z, u8"σ", u8"Σ"},
};

static const VkLayoutEntry kRussian[] = {
    {VK_KEY_A, u8"л", u8"Л"},
    {VK_KEY_B, u8"ъ", u8"Ъ"},
    {VK_KEY_C, u8"ш", u8"Ш"},
    {VK_KEY_D, u8"н", u8"Н"},
    {VK_KEY_E, u8"в", u8"В"},
    {VK_KEY_F, u8"о", u8"О"},
    {VK_KEY_G, u8"п", u8"П"},
    {VK_KEY_H, u8"р", u8"Р"},
    {VK_KEY_I, u8"ж", u8"Ж"},
    {VK_KEY_J, u8"с", u8"С"},
    {VK_KEY_K, u8"т", u8"Т"},
    {VK_KEY_L, u8"у", u8"У"},
    {VK_KEY_M, u8"ь", u8"Ь"},
    {VK_KEY_N, u8"ы", u8"Ы"},
    {VK_KEY_O, u8"з", u8"З"},
    {VK_KEY_P, u8"и", u8"И"},
    {VK_KEY_Q, u8"а", u8"А"},
    {VK_KEY_R, u8"г", u8"Г"},
    {VK_KEY_S, u8"м", u8"М"},
    {VK_KEY_T, u8"д", u8"Д"},
    {VK_KEY_U, u8"ё", u8"Ё"},
    {VK_KEY_V, u8"щ", u8"Щ"},
    {VK_KEY_W, u8"б", u8"Б"},
    {VK_KEY_X, u8"ч", u8"Ч"},
    {VK_KEY_Y, u8"е", u8"Е"},
    {VK_KEY_Z, u8"ц", u8"Ц"},
    {VK_KEY_LEFT_BRACKET, u8"й", u8"Й"},
    {VK_KEY_RIGHT_BRACKET, u8"к", u8"К"},
    {VK_KEY_SEMICOLON, u8"ф", u8"Ф"},
    {VK_KEY_QUOTE, u8"х", u8"Х"},
    {VK_KEY_COMMA, u8"э", u8"Э"},
    {VK_KEY_PERIOD, u8"ю", u8"Ю"},
    {VK_KEY_SLASH, u8"я", u8"Я"},
};

static const VkLayoutEntry kPhonetic[] = {
    {VK_KEY_DIGIT0, u8"ㄦ", u8""},
    {VK_KEY_DIGIT1, u8"ㄉ", u8""},
    {VK_KEY_DIGIT4, u8"ㄓ", u8""},
    {VK_KEY_DIGIT7, u8"ㄚ", u8""},
    {VK_KEY_DIGIT8, u8"ㄞ", u8""},
    {VK_KEY_DIGIT9, u8"ㄢ", u8""},
    {VK_KEY_A, u8"ㄇ", u8""},
    {VK_KEY_B, u8"ㄖ", u8""},
    {VK_KEY_C, u8"ㄏ", u8""},
    {VK_KEY_D, u8"ㄎ", u8""},
    {VK_KEY_E, u8"ㄍ", u8""},
    {VK_KEY_F, u8"ㄑ", u8""},
    {VK_KEY_G, u8"ㄕ", u8""},
    {VK_KEY_H, u8"ㄘ", u8""},
    {VK_KEY_I, u8"ㄛ", u8""},
    {VK_KEY_J, u8"ㄨ", u8""},
    {VK_KEY_K, u8"ㄜ", u8""},
    {VK_KEY_L, u8"ㄠ", u8""},
    {VK_KEY_M, u8"ㄩ", u8""},
    {VK_KEY_N, u8"ㄙ", u8""},
    {VK_KEY_O, u8"ㄟ", u8""},
    {VK_KEY_P, u8"ㄣ", u8""},
    {VK_KEY_Q, u8"ㄆ", u8""},
    {VK_KEY_R, u8"ㄐ", u8""},
    {VK_KEY_S, u8"ㄋ", u8""},
    {VK_KEY_T, u8"ㄔ", u8""},
    {VK_KEY_U, u8"ㄧ", u8""},
    {VK_KEY_V, u8"ㄒ", u8""},
    {VK_KEY_W, u8"ㄊ", u8""},
    {VK_KEY_Y, u8"ㄗ", u8""},
    {VK_KEY_Z, u8"ㄈ", u8""},
    {VK_KEY_BACKQUOTE, u8"ㄅ", u8""},
    {VK_KEY_SEMICOLON, u8"ㄤ", u8""},
    {VK_KEY_COMMA, u8"ㄝ", u8""},
    {VK_KEY_PERIOD, u8"ㄡ", u8""},
    {VK_KEY_SLASH, u8"ㄥ", u8""},
};

static const VkLayoutEntry kPinyin[] = {
    {VK_KEY_A, u8"ē", u8""},
    {VK_KEY_C, u8"ǔ", u8""},
    {VK_KEY_D, u8"ě", u8""},
    {VK_KEY_E, u8"ǎ", u8""},
    {VK_KEY_F, u8"è", u8""},
    {VK_KEY_H, u8"ī", u8""},
    {VK_KEY_I, u8"ǒ", u8""},
    {VK_KEY_J, u8"í", u8""},
    {VK_KEY_K, u8"ǐ", u8""},
    {VK_KEY_L, u8"ì", u8""},
    {VK_KEY_M, u8"ǘ", u8""},
    {VK_KEY_N, u8"ǖ", u8""},
    {VK_KEY_O, u8"ò", u8""},
    {VK_KEY_Q, u8"ā", u8""},
    {VK_KEY_R, u8"à", u8""},
    {VK_KEY_S, u8"é", u8""},
    {VK_KEY_U, u8"ó", u8""},
    {VK_KEY_V, u8"ù", u8""},
    {VK_KEY_W, u8"á", u8""},
    {VK_KEY_X, u8"ú", u8""},
    {VK_KEY_Y, u8"ō", u8""},
    {VK_KEY_Z, u8"ū", u8""},
    {VK_KEY_LEFT_BRACKET, u8"ê", u8""},
    {VK_KEY_COMMA, u8"ǚ", u8""},
    {VK_KEY_PERIOD, u8"ǜ", u8""},
    {VK_KEY_SLASH, u8"ü", u8""},
};

static const VkLayoutEntry kJapanFlat[] = {
    {VK_KEY_DIGIT1, u8"ぃ", u8"い"},
    {VK_KEY_DIGIT2, u8"ぅ", u8"う"},
    {VK_KEY_DIGIT3, u8"ぇ", u8"え"},
    {VK_KEY_DIGIT4, u8"ぉ", u8"お"},
    {VK_KEY_DIGIT5, u8"か", u8"が"},
    {VK_KEY_DIGIT6, u8"き", u8"ぎ"},
    {VK_KEY_DIGIT7, u8"く", u8"ぐ"},
    {VK_KEY_DIGIT8, u8"け", u8"げ"},
    {VK_KEY_DIGIT9, u8"こ", u8"ご"},
    {VK_KEY_A, u8"な", u8"ぱ"},
    {VK_KEY_B, u8"も", u8"ろ"},
    {VK_KEY_C, u8"む", u8"る"},
    {VK_KEY_D, u8"ぬ", u8"ぷ"},
    {VK_KEY_E, u8"す", u8"ず"},
    {VK_KEY_F, u8"ね", u8"ぺ"},
    {VK_KEY_G, u8"の", u8"ぽ"},
    {VK_KEY_H, u8"は", u8"ぼ"},
    {VK_KEY_I, u8"つ", u8"づ"},
    {VK_KEY_J, u8"ひ", u8"び"},
    {VK_KEY_K, u8"ふ", u8"ぶ"},
    {VK_KEY_L, u8"へ", u8"べ"},
    {VK_KEY_M, u8"ゅ", u8"ゆ"},
    {VK_KEY_N, u8"ゃ", u8"や"},
    {VK_KEY_O, u8"っ", u8""},
    {VK_KEY_P, u8"て", u8"で"},
    {VK_KEY_Q, u8"さ", u8"ざ"},
    {VK_KEY_R, u8"せ", u8"ぜ"},
    {VK_KEY_S, u8"に", u8"ぴ"},
    {VK_KEY_T, u8"そ", u8"ぞ"},
    {VK_KEY_U, u8"ち", u8"ぢ"},
    {VK_KEY_V, u8"め", u8"れ"},
    {VK_KEY_W, u8"し", u8"じ"},
    {VK_KEY_X, u8"み", u8"り"},
    {VK_KEY_Y, u8"た", u8"だ"},
    {VK_KEY_Z, u8"ま", u8"ら"},
    {VK_KEY_BACKQUOTE, u8"ぁ", u8"あ"},
    {VK_KEY_SUB, u8"ん", u8""},
    {VK_KEY_LEFT_BRACKET, u8"と", u8"ど"},
    {VK_KEY_RIGHT_BRACKET, u8"ゐ", u8""},
    {VK_KEY_SEMICOLON, u8"ほ", u8"ぼ"},
    {VK_KEY_QUOTE, u8"ゑ", u8""},
    {VK_KEY_COMMA, u8"ょ", u8"よ"},
    {VK_KEY_PERIOD, u8"ゎ", u8"わ"},
    {VK_KEY_SLASH, u8"を", u8""},
};

static const VkLayoutEntry kJapanPiece[] = {
    {VK_KEY_DIGIT0, u8"ケ", u8"ゲ"},
    {VK_KEY_DIGIT1, u8"ィ", u8"イ"},
    {VK_KEY_DIGIT2, u8"ゥ", u8"ウ"},
    {VK_KEY_DIGIT3, u8"ヴ", u8""},
    {VK_KEY_DIGIT4, u8"ェ", u8"エ"},
    {VK_KEY_DIGIT5, u8"ォ", u8"オ"},
    {VK_KEY_DIGIT6, u8"カ", u8"ガ"},
    {VK_KEY_DIGIT7, u8"ヵ", u8""},
    {VK_KEY_DIGIT8, u8"キ", u8"ギ"},
    {VK_KEY_DIGIT9, u8"ク", u8"グ"},
    {VK_KEY_A, u8"ナ", u8"パ"},
    {VK_KEY_B, u8"モ", u8"ロ"},
    {VK_KEY_C, u8"ム", u8"ル"},
    {VK_KEY_D, u8"ヌ", u8"プ"},
    {VK_KEY_E, u8"ス", u8"ズ"},
    {VK_KEY_F, u8"ネ", u8"ペ"},
    {VK_KEY_G, u8"ノ", u8"ポ"},
    {VK_KEY_H, u8"ハ", u8"バ"},
    {VK_KEY_I, u8"ツ", u8"ヅ"},
    {VK_KEY_J, u8"ヒ", u8"ビ"},
    {VK_KEY_K, u8"フ", u8"ブ"},
    {VK_KEY_L, u8"ヘ", u8"ベ"},
    {VK_KEY_M, u8"ュ", u8"ユ"},
    {VK_KEY_N, u8"ャ", u8"ヤ"},
    {VK_KEY_O, u8"ッ", u8""},
    {VK_KEY_P, u8"テ", u8"デ"},
    {VK_KEY_Q, u8"サ", u8"ザ"},
    {VK_KEY_R, u8"セ", u8"ゼ"},
    {VK_KEY_S, u8"ニ", u8"ピ"},
    {VK_KEY_T, u8"ソ", u8"ゾ"},
    {VK_KEY_U, u8"チ", u8"ヂ"},
    {VK_KEY_V, u8"メ", u8"レ"},
    {VK_KEY_W, u8"シ", u8"ジ"},
    {VK_KEY_X, u8"ミ", u8"リ"},
    {VK_KEY_Y, u8"タ", u8"ダ"},
    {VK_KEY_Z, u8"マ", u8"ラ"},
    {VK_KEY_BACKQUOTE, u8"ァ", u8"ア"},
    {VK_KEY_SUB, u8"ヶ", u8""},
    {VK_KEY_EQUAL, u8"コ", u8"ゴ"},
    {VK_KEY_LEFT_BRACKET, u8"ト", u8"ド"},
    {VK_KEY_RIGHT_BRACKET, u8"ヰ", u8""},
    {VK_KEY_BACKSLASH, u8"ン", u8""},
    {VK_KEY_SEMICOLON, u8"ホ", u8"ボ"},
    {VK_KEY_QUOTE, u8"ヱ", u8""},
    {VK_KEY_COMMA, u8"ョ", u8"ヨ"},
    {VK_KEY_PERIOD, u8"ヮ", u8"ワ"},
    {VK_KEY_SLASH, u8"ヲ", u8""},
};

static const VkLayoutEntry kPunctuation[] = {
    {VK_KEY_DIGIT0, u8"ˉ", u8""},
    {VK_KEY_DIGIT1, u8"，", u8""},
    {VK_KEY_DIGIT2, u8"、", u8""},
    {VK_KEY_DIGIT3, u8";", u8""},
    {VK_KEY_DIGIT4, u8"：", u8""},
    {VK_KEY_DIGIT5, u8"？", u8""},
    {VK_KEY_DIGIT6, u8"！", u8""},
    {VK_KEY_DIGIT7, u8"…", u8""},
    {VK_KEY_DIGIT8, u8"—", u8""},
    {VK_KEY_DIGIT9, u8"·", u8""},
    {VK_KEY_A, u8"〔", u8""},
    {VK_KEY_B, u8"（", u8""},
    {VK_KEY_C, u8"【", u8""},
    {VK_KEY_D, u8"〈", u8""},
    {VK_KEY_E, u8"“", u8""},
    {VK_KEY_F, u8"〉", u8""},
    {VK_KEY_G, u8"《", u8""},
    {VK_KEY_H, u8"》", u8""},
    {VK_KEY_I, u8"∶", u8""},
    {VK_KEY_J, u8"「", u8""},
    {VK_KEY_K, u8"」", u8""},
    {VK_KEY_L, u8"『", u8""},
    {VK_KEY_M, u8"［", u8""},
    {VK_KEY_N, u8"）", u8""},
    {VK_KEY_O, u8"＂", u8""},
    {VK_KEY_P, u8"＇", u8""},
    {VK_KEY_Q, u8"‘", u8""},
    {VK_KEY_R, u8"”", u8""},
    {VK_KEY_S, u8"〕", u8""},
    {VK_KEY_T, u8"々", u8""},
    {VK_KEY_U, u8"‖", u8""},
    {VK_KEY_V, u8"】", u8""},
    {VK_KEY_W, u8"’", u8""},
    {VK_KEY_X, u8"〗", u8""},
    {VK_KEY_Y, u8"～", u8""},
    {VK_KEY_Z, u8"〖", u8""},
    {VK_KEY_BACKQUOTE, u8"。", u8""},
    {VK_KEY_SUB, u8"ˇ", u8""},
    {VK_KEY_EQUAL, u8"¨", u8""},
    {VK_KEY_LEFT_BRACKET, u8"｀", u8""},
    {VK_KEY_RIGHT_BRACKET, u8"｜", u8""},
    {VK_KEY_BACKSLASH, u8"〃", u8""},
    {VK_KEY_SEMICOLON, u8"』", u8""},
    {VK_KEY_QUOTE, u8"．", u8""},
    {VK_KEY_COMMA, u8"］", u8""},
    {VK_KEY_PERIOD, u8"｛", u8""},
    {VK_KEY_SLASH, u8"｝", u8""},
};

static const VkLayoutEntry kDigitalOrder[] = {
    {VK_KEY_DIGIT0, u8"Ⅺ", u8""},
    {VK_KEY_DIGIT1, u8"Ⅱ", u8""},
    {VK_KEY_DIGIT2, u8"Ⅲ", u8""},
    {VK_KEY_DIGIT3, u8"Ⅳ", u8""},
    {VK_KEY_DIGIT4, u8"Ⅴ", u8""},
    {VK_KEY_DIGIT5, u8"Ⅵ", u8""},
    {VK_KEY_DIGIT6, u8"Ⅶ", u8""},
    {VK_KEY_DIGIT7, u8"Ⅷ", u8""},
    {VK_KEY_DIGIT8, u8"Ⅸ", u8""},
    {VK_KEY_DIGIT9, u8"Ⅹ", u8""},
    {VK_KEY_A, u8"㈠", u8"①"},
    {VK_KEY_B, u8"⑸", u8"⒂"},
    {VK_KEY_C, u8"⑶", u8"⒀"},
    {VK_KEY_D, u8"㈢", u8"③"},
    {VK_KEY_E, u8"⒊", u8"⒔"},
    {VK_KEY_F, u8"㈣", u8"④"},
    {VK_KEY_G, u8"㈤", u8"⑤"},
    {VK_KEY_H, u8"㈥", u8"⑥"},
    {VK_KEY_I, u8"⒏", u8"⒙"},
    {VK_KEY_J, u8"㈦", u8"⑦"},
    {VK_KEY_K, u8"㈧", u8"⑧"},
    {VK_KEY_L, u8"㈨", u8"⑨"},
    {VK_KEY_M, u8"⑺", u8"⒄"},
    {VK_KEY_N, u8"⑹", u8"⒃"},
    {VK_KEY_O, u8"⒐", u8"⒚"},
    {VK_KEY_P, u8"⒑", u8"⒛"},
    {VK_KEY_Q, u8"⒈", u8"⒒"},
    {VK_KEY_R, u8"⒋", u8"⒕"},
    {VK_KEY_S, u8"㈡", u8"②"},
    {VK_KEY_T, u8"⒌", u8"⒖"},
    {VK_KEY_U, u8"⒎", u8"⒘"},
    {VK_KEY_V, u8"⑷", u8"⒁"},
    {VK_KEY_W, u8"⒉", u8"⒓"},
    {VK_KEY_X, u8"⑵", u8"⑿"},
    {VK_KEY_Y, u8"⒍", u8"⒗"},
    {VK_KEY_Z, u8"⑴", u8"⑾"},
    {VK_KEY_BACKQUOTE, u8"Ⅰ", u8""},
    {VK_KEY_SUB, u8"Ⅻ", u8""},
    {VK_KEY_SEMICOLON, u8"㈩", u8"⑩"},
    {VK_KEY_COMMA, u8"⑻", u8"⒅"},
    {VK_KEY_PERIOD, u8"⑼", u8"⒆"},
    {VK_KEY_SLASH, u8"⑽", u8"⒇"},
};

static const VkLayoutEntry kMath[] = {
    {VK_KEY_DIGIT1, u8"≡", u8""},
    {VK_KEY_DIGIT2, u8"≠", u8""},
    {VK_KEY_DIGIT3, u8"＝", u8""},
    {VK_KEY_DIGIT4, u8"≤", u8""},
    {VK_KEY_DIGIT5, u8"≥", u8""},
    {VK_KEY_DIGIT6, u8"＜", u8""},
    {VK_KEY_DIGIT7, u8"＞", u8""},
    {VK_KEY_DIGIT8, u8"≮", u8""},
    {VK_KEY_DIGIT9, u8"≯", u8""},
    {VK_KEY_A, u8"∧", u8""},
    {VK_KEY_B, u8"⊙", u8""},
    {VK_KEY_C, u8"∠", u8""},
    {VK_KEY_D, u8"∑", u8""},
    {VK_KEY_E, u8"－", u8""},
    {VK_KEY_F, u8"∏", u8""},
    {VK_KEY_G, u8"∪", u8""},
    {VK_KEY_H, u8"∩", u8""},
    {VK_KEY_I, u8"∫", u8""},
    {VK_KEY_J, u8"∈", u8""},
    {VK_KEY_L, u8"∵", u8""},
    {VK_KEY_M, u8"∽", u8""},
    {VK_KEY_N, u8"≌", u8""},
    {VK_KEY_O, u8"∮", u8""},
    {VK_KEY_P, u8"∝", u8""},
    {VK_KEY_Q, u8"±", u8""},
    {VK_KEY_R, u8"×", u8""},
    {VK_KEY_S, u8"∨", u8""},
    {VK_KEY_T, u8"÷", u8""},
    {VK_KEY_V, u8"⌒", u8""},
    {VK_KEY_W, u8"＋", u8""},
    {VK_KEY_X, u8"∥", u8""},
    {VK_KEY_Y, u8"／", u8""},
    {VK_KEY_Z, u8"⊥", u8""},
    {VK_KEY_BACKQUOTE, u8"≈", u8""},
    {VK_KEY_SUB, u8"∷", u8""},
    {VK_KEY_LEFT_BRACKET, u8"∞", u8""},
    {VK_KEY_SEMICOLON, u8"∴", u8""},
    {VK_KEY_PERIOD, u8"√", u8""},
};

static const VkLayoutEntry kUnit[] = {
    {VK_KEY_DIGIT0, u8"¤", u8""},
    {VK_KEY_DIGIT1, u8"°", u8""},
    {VK_KEY_DIGIT2, u8"′", u8""},
    {VK_KEY_DIGIT3, u8"″", u8""},
    {VK_KEY_DIGIT4, u8"＄", u8""},
    {VK_KEY_DIGIT5, u8"￡", u8""},
    {VK_KEY_DIGIT6, u8"￥", u8""},
    {VK_KEY_DIGIT7, u8"‰", u8""},
    {VK_KEY_DIGIT8, u8"％", u8""},
    {VK_KEY_DIGIT9, u8"℃", u8""},
    {VK_KEY_A, u8"百", u8"佰"},
    {VK_KEY_C, u8"毫", u8""},
    {VK_KEY_D, u8"万", u8""},
    {VK_KEY_E, u8"二", u8"贰"},
    {VK_KEY_F, u8"亿", u8""},
    {VK_KEY_G, u8"兆", u8""},
    {VK_KEY_H, u8"吉", u8""},
    {VK_KEY_I, u8"七", u8"柒"},
    {VK_KEY_J, u8"太", u8""},
    {VK_KEY_K, u8"拍", u8""},
    {VK_KEY_L, u8"艾", u8""},
    {VK_KEY_O, u8"八", u8"捌"},
    {VK_KEY_P, u8"九", u8"玖"},
    {VK_KEY_Q, u8"○", u8"零"},
    {VK_KEY_R, u8"三", u8"叁"},
    {VK_KEY_S, u8"千", u8"仟"},
    {VK_KEY_T, u8"四", u8"肆"},
    {VK_KEY_U, u8"六", u8"陆"},
    {VK_KEY_V, u8"微", u8""},
    {VK_KEY_W, u8"一", u8"壹"},
    {VK_KEY_X, u8"厘", u8""},
    {VK_KEY_Y, u8"五", u8"伍"},
    {VK_KEY_Z, u8"分", u8""},
    {VK_KEY_SUB, u8"￠", u8""},
    {VK_KEY_LEFT_BRACKET, u8"十", u8"拾"},
};

static const VkLayoutEntry kTabs[] = {
    {VK_KEY_DIGIT0, u8"┄", u8"┅"},
    {VK_KEY_DIGIT1, u8"┍", u8"┕"},
    {VK_KEY_DIGIT2, u8"┎", u8"┖"},
    {VK_KEY_DIGIT3, u8"┏", u8"┗"},
    {VK_KEY_DIGIT4, u8"┐", u8"┘"},
    {VK_KEY_DIGIT5, u8"┑", u8"┙"},
    {VK_KEY_DIGIT6, u8"┒", u8"┚"},
    {VK_KEY_DIGIT7, u8"┓", u8"┛"},
    {VK_KEY_DIGIT9, u8"─", u8"━"},
    {VK_KEY_A, u8"┬", u8"┴"},
    {VK_KEY_B, u8"╀", u8"╈"},
    {VK_KEY_C, u8"┾", u8"╆"},
    {VK_KEY_D, u8"┮", u8"┶"},
    {VK_KEY_E, u8"┞", u8"┦"},
    {VK_KEY_F, u8"┯", u8"┷"},
    {VK_KEY_G, u8"┰", u8"┸"},
    {VK_KEY_H, u8"┱", u8"┹"},
    {VK_KEY_I, u8"┣", u8"┫"},
    {VK_KEY_J, u8"┲", u8"┺"},
    {VK_KEY_K, u8"┳", u8"┻"},
    {VK_KEY_M, u8"╂", u8"╊"},
    {VK_KEY_N, u8"╁", u8"╉"},
    {VK_KEY_P, u8"│", u8"┃"},
    {VK_KEY_Q, u8"├", u8"┤"},
    {VK_KEY_R, u8"┟", u8"┧"},
    {VK_KEY_S, u8"┭", u8"┵"},
    {VK_KEY_T, u8"┠", u8"┨"},
    {VK_KEY_U, u8"┢", u8"┪"},
    {VK_KEY_V, u8"┿", u8"╇"},
    {VK_KEY_W, u8"┝", u8"┥"},
    {VK_KEY_X, u8"┽", u8"╅"},
    {VK_KEY_Y, u8"┡", u8"┩"},
    {VK_KEY_Z, u8"┼", u8"╄"},
    {VK_KEY_BACKQUOTE, u8"┌", u8"└"},
    {VK_KEY_SUB, u8"┈", u8"┉"},
    {VK_KEY_LEFT_BRACKET, u8"┆", u8"┇"},
    {VK_KEY_RIGHT_BRACKET, u8"┊", u8"┋"},
    {VK_KEY_QUOTE, u8"╃", u8"╋"},
};

static const VkLayoutEntry kSpecial[] = {
    {VK_KEY_A, u8"■", u8""},
    {VK_KEY_B, u8"＾", u8""},
    {VK_KEY_C, u8"＠", u8""},
    {VK_KEY_D, u8"▲", u8""},
    {VK_KEY_E, u8"☆", u8""},
    {VK_KEY_F, u8"※", u8""},
    {VK_KEY_G, u8"→", u8""},
    {VK_KEY_H, u8"←", u8""},
    {VK_KEY_I, u8"◇", u8""},
    {VK_KEY_J, u8"↑", u8""},
    {VK_KEY_K, u8"↓", u8""},
    {VK_KEY_L, u8"〓", u8""},
    {VK_KEY_M, u8"￣", u8""},
    {VK_KEY_N, u8"＿", u8""},
    {VK_KEY_O, u8"◆", u8""},
    {VK_KEY_P, u8"□", u8""},
    {VK_KEY_Q, u8"§", u8""},
    {VK_KEY_R, u8"★", u8""},
    {VK_KEY_S, u8"△", u8""},
    {VK_KEY_T, u8"○", u8""},
    {VK_KEY_U, u8"◎", u8""},
    {VK_KEY_V, u8"＼", u8""},
    {VK_KEY_W, u8"№", u8""},
    {VK_KEY_X, u8"＆", u8""},
    {VK_KEY_Y, u8"●", u8""},
    {VK_KEY_Z, u8"＃", u8""},
};

struct VkModeLayout
{
    VirtualKeyboardMode mode;
    const VkLayoutEntry *entries;
    size_t count;
};

const VkModeLayout kVkModeLayouts[] = {
    {VK_MODE_GREEK, kGreek, sizeof(kGreek) / sizeof(kGreek[0])},
    {VK_MODE_RUSSIAN, kRussian, sizeof(kRussian) / sizeof(kRussian[0])},
    {VK_MODE_PHONETIC, kPhonetic, sizeof(kPhonetic) / sizeof(kPhonetic[0])},
    {VK_MODE_PINYIN, kPinyin, sizeof(kPinyin) / sizeof(kPinyin[0])},
    {VK_MODE_JAPAN_FLAT, kJapanFlat, sizeof(kJapanFlat) / sizeof(kJapanFlat[0])},
    {VK_MODE_JAPAN_PIECE, kJapanPiece, sizeof(kJapanPiece) / sizeof(kJapanPiece[0])},
    {VK_MODE_PUNCTUATION, kPunctuation, sizeof(kPunctuation) / sizeof(kPunctuation[0])},
    {VK_MODE_DIGITAL_ORDER, kDigitalOrder, sizeof(kDigitalOrder) / sizeof(kDigitalOrder[0])},
    {VK_MODE_MATH, kMath, sizeof(kMath) / sizeof(kMath[0])},
    {VK_MODE_UNIT, kUnit, sizeof(kUnit) / sizeof(kUnit[0])},
    {VK_MODE_TABS, kTabs, sizeof(kTabs) / sizeof(kTabs[0])},
    {VK_MODE_SPECIAL, kSpecial, sizeof(kSpecial) / sizeof(kSpecial[0])},
};

struct VkKeyMeta
{
    char asciiNormal;
    char asciiShift;
};

constexpr VkKeyMeta kVkKeyMeta[kVkSymbolKeyCount] = {
    {'0', ')'}, {'1', '!'}, {'2', '@'}, {'3', '#'}, {'4', '$'},
    {'5', '%'}, {'6', '^'}, {'7', '&'}, {'8', '*'}, {'9', '('},
    {'a', 'A'}, {'b', 'B'}, {'c', 'C'}, {'d', 'D'}, {'e', 'E'},
    {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'i', 'I'}, {'j', 'J'},
    {'k', 'K'}, {'l', 'L'}, {'m', 'M'}, {'n', 'N'}, {'o', 'O'},
    {'p', 'P'}, {'q', 'Q'}, {'r', 'R'}, {'s', 'S'}, {'t', 'T'},
    {'u', 'U'}, {'v', 'V'}, {'w', 'W'}, {'x', 'X'}, {'y', 'Y'},
    {'z', 'Z'},
    {'`', '~'}, {'-', '_'}, {'=', '+'}, {'[', '{'}, {']', '}'},
    {'\\', '|'}, {';', ':'}, {'\'', '"'}, {',', '<'}, {'.', '>'},
    {'/', '?'},
};

VkLayouts::VkLayouts()
{
    initLayouts();
}

void VkLayouts::initLayouts()
{
    for (int i = 0; i < kVkSymbolKeyCount; ++i)
    {
        const VkKeyMeta &meta = kVkKeyMeta[i];
        table_[VK_MODE_PC][i] = {std::string(1, meta.asciiNormal), std::string(1, meta.asciiShift)};
        ascii_[static_cast<unsigned char>(meta.asciiNormal)] = {static_cast<int8_t>(i), false};
        ascii_[static_cast<unsigned char>(meta.asciiShift)] = {static_cast<int8_t>(i), true};
    }

    for (const VkModeLayout &src : kVkModeLayouts)
    {
        VkKeyPair *dst = table_[src.mode];
        for (size_t i = 0; i < src.count; ++i)
        {
            const int keyIdx = src.entries[i].key;
            if (keyIdx >= 0 && keyIdx < kVkSymbolKeyCount)
            {
                dst[keyIdx] = {src.entries[i].normal, src.entries[i].shift};
            }
        }
    }
}

const VkLayouts &VkLayouts::instance()
{
    static const VkLayouts tables;
    return tables;
}

bool VkLayouts::locateAscii(unsigned char ascii, VkKey &key, bool &shift) const
{
    if (ascii >= 128)
    {
        return false;
    }
    const AsciiIndex &idx = ascii_[ascii];
    if (idx.key < 0)
    {
        return false;
    }
    key = static_cast<VkKey>(idx.key);
    shift = idx.shift;
    return true;
}

const VkKeyPair &VkLayouts::getKeyPair(VirtualKeyboardMode mode, VkKey key)
{
    const int modeIdx = mode;
    const int keyIdx = key;
    if (modeIdx < VK_MODE_PC || modeIdx >= VK_MODE_USER_CHAR || keyIdx < 0 || keyIdx >= kVkSymbolKeyCount)
    {
        static const VkKeyPair empty;
        return empty;
    }
    return instance().table_[static_cast<size_t>(modeIdx)][static_cast<size_t>(keyIdx)];
}

std::string VkLayouts::convertToVkText(VirtualKeyboardMode mode, unsigned char ascii)
{
    VkKey key = VK_KEY_DIGIT0;
    bool shift = false;
    if (!instance().locateAscii(ascii, key, shift))
    {
        return {};
    }
    const VkKeyPair &pair = getKeyPair(mode, key);
    return shift ? pair.shift : pair.normal;
}

std::string VkLayouts::convertToCustomSymbol(const VkLayout &layout, unsigned char ascii)
{
    VkKey key = VK_KEY_DIGIT0;
    bool shift = false;
    if (!instance().locateAscii(ascii, key, shift))
    {
        return {};
    }
    const VkKeyPair &binding = layout[static_cast<size_t>(key)];
    return shift ? binding.shift : binding.normal;
}

VkLayout VkLayouts::parse(const std::string &text)
{
    VkLayout layout{};
    std::vector<std::string> tokens;
    std::string token;
    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == ' ')
        {
            tokens.push_back(token);
            token.clear();
        }
        else
        {
            token.push_back(text[i]);
        }
    }
    tokens.push_back(token);

    for (int i = 0; i < kVkSymbolKeyCount; ++i)
    {
        const size_t base = static_cast<size_t>(i) * 2;
        if (base + 1 < tokens.size())
        {
            layout[static_cast<size_t>(i)].normal = tokens[base];
            layout[static_cast<size_t>(i)].shift = tokens[base + 1];
        }
    }
    return layout;
}

} // namespace freewb
