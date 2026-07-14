#include "../uri_utils.hpp"

#include <gtest/gtest.h>

namespace just_audio_windows {
namespace test {

// ── EncodeUri ────────────────────────────────────────────────────────────────

// A plain URL with no spaces should pass through unchanged.
TEST(EncodeUri, NoSpaces_ReturnsUnchanged) {
  EXPECT_EQ(EncodeUri("https://example.com/audio.mp3"),
            "https://example.com/audio.mp3");
}

// An empty string should stay empty.
TEST(EncodeUri, EmptyString_ReturnsEmpty) {
  EXPECT_EQ(EncodeUri(""), "");
}

// Literal spaces in a local file path must be encoded as %20.
// This is the core regression for https://github.com/bdlukaa/just_audio_windows/issues/26.
TEST(EncodeUri, LiteralSpacesInFilePath_EncodedAsPercent20) {
  EXPECT_EQ(
      EncodeUri("file:///C:/Users/My Files/song.mp3"),
      "file:///C:/Users/My%20Files/song.mp3");
}

// Multiple consecutive spaces must each be individually encoded.
TEST(EncodeUri, MultipleSpaces_AllEncoded) {
  EXPECT_EQ(
      EncodeUri(
          "C:/Users/HP/Downloads/Ve Kamleya Rocky Aur Rani.mp3"),
      "C:/Users/HP/Downloads/Ve%20Kamleya%20Rocky%20Aur%20Rani.mp3");
}

// Already percent-encoded spaces (%20) must NOT be double-encoded.
// This ensures a properly-encoded file:// URI like those produced by
// Dart's Uri.file() is left intact.
TEST(EncodeUri, AlreadyEncodedSpaces_NotDoubleEncoded) {
  EXPECT_EQ(
      EncodeUri("file:///C:/Users/My%20Files/song.mp3"),
      "file:///C:/Users/My%20Files/song.mp3");
}

// Percent-encoded multi-byte UTF-8 sequences (e.g. U+2019 RIGHT SINGLE
// QUOTATION MARK) must pass through unchanged so that Windows::Foundation::Uri
// can decode them natively.
// This is the core regression for the original apostrophe bug.
TEST(EncodeUri, MultiBytePercentEncoded_Unchanged) {
  EXPECT_EQ(
      EncodeUri(
          "https://example.com/speech?text=I%E2%80%99d%20like%20a%20coffee"),
      "https://example.com/speech?text=I%E2%80%99d%20like%20a%20coffee");
}

// A URL with both unencoded spaces and percent-encoded multi-byte chars:
// spaces must be encoded while the percent-sequences stay intact.
TEST(EncodeUri, MixedLiteralSpacesAndPercentEncoded) {
  EXPECT_EQ(
      EncodeUri(
          "https://example.com/speech?text=I%E2%80%99d like a coffee"),
      "https://example.com/speech?text=I%E2%80%99d%20like%20a%20coffee");
}

// A URL with only query-string spaces should have them encoded.
TEST(EncodeUri, SpaceInQueryString) {
  EXPECT_EQ(
      EncodeUri("https://example.com/tts?text=hello world"),
      "https://example.com/tts?text=hello%20world");
}

// ── Non-ASCII characters ─────────────────────────────────────────────────────
// The bug this encoder fixes: any character outside the unreserved/reserved
// sets (○, ¤, CJK, emoji, ...) must be percent-encoded on its UTF-8 bytes,
// or Windows::Foundation::Uri rejects the URI and the song fails to play.
// https://github.com/bdlukaa/just_audio_windows special-character playback bug.

// U+25CB WHITE CIRCLE "○" is UTF-8 E2 97 8B.
TEST(EncodeUri, WhiteCircle_EncodedAsUtf8Bytes) {
  EXPECT_EQ(
      EncodeUri("file:///C:/Music/\xE2\x97\x8B song.mp3"),
      "file:///C:/Music/%E2%97%8B%20song.mp3");
}

// U+00A4 CURRENCY SIGN "¤" is UTF-8 C2 A4.
TEST(EncodeUri, CurrencySign_EncodedAsUtf8Bytes) {
  EXPECT_EQ(
      EncodeUri("file:///C:/Music/\xC2\xA4.mp3"),
      "file:///C:/Music/%C2%A4.mp3");
}

// A CJK character, U+4E2D "中" is UTF-8 E4 B8 AD.
TEST(EncodeUri, CjkCharacter_EncodedAsUtf8Bytes) {
  EXPECT_EQ(
      EncodeUri("file:///C:/Music/\xE4\xB8\xAD.mp3"),
      "file:///C:/Music/%E4%B8%AD.mp3");
}

// A 4-byte emoji, U+1F3B5 MUSICAL NOTE "🎵" is UTF-8 F0 9F 8E B5.
TEST(EncodeUri, Emoji_EncodedAsUtf8Bytes) {
  EXPECT_EQ(
      EncodeUri("file:///C:/Music/\xF0\x9F\x8E\xB5.mp3"),
      "file:///C:/Music/%F0%9F%8E%B5.mp3");
}

// The scheme, drive-letter colon, and path separators must stay literal.
TEST(EncodeUri, FileSchemeStructure_Preserved) {
  EXPECT_EQ(
      EncodeUri("file:///C:/Users/Me/song.mp3"),
      "file:///C:/Users/Me/song.mp3");
}

// ── PercentDecode ────────────────────────────────────────────────────────────

TEST(PercentDecode, NoEscapes_ReturnsUnchanged) {
  EXPECT_EQ(PercentDecode("C:/Music/song.mp3"), "C:/Music/song.mp3");
}

TEST(PercentDecode, DecodesSpaceAndAscii) {
  EXPECT_EQ(PercentDecode("My%20Files%5Bx%5D"), "My Files[x]");
}

// Multi-byte UTF-8 escapes decode back to their original bytes.
TEST(PercentDecode, DecodesUtf8Bytes) {
  // %E2%97%8B -> "○", %E2%A7%B8 -> "⧸"
  EXPECT_EQ(PercentDecode("%E2%97%8B%E2%A7%B8"), "\xE2\x97\x8B\xE2\xA7\xB8");
}

// A truncated or invalid escape is left literal rather than dropped.
TEST(PercentDecode, InvalidEscape_LeftLiteral) {
  EXPECT_EQ(PercentDecode("50%25 done"), "50% done");
  EXPECT_EQ(PercentDecode("trailing%"), "trailing%");
  EXPECT_EQ(PercentDecode("bad%zz"), "bad%zz");
}

// ── TryFileUriToWindowsPath ──────────────────────────────────────────────────

TEST(TryFileUriToWindowsPath, NonFileUri_ReturnsFalse) {
  std::string out = "untouched";
  EXPECT_FALSE(TryFileUriToWindowsPath("https://example.com/a.mp3", out));
  EXPECT_EQ(out, "untouched");
  EXPECT_FALSE(TryFileUriToWindowsPath("asset:///audio/a.mp3", out));
}

TEST(TryFileUriToWindowsPath, SimpleLocalFile) {
  std::string out;
  EXPECT_TRUE(TryFileUriToWindowsPath("file:///C:/Music/song.mp3", out));
  EXPECT_EQ(out, "C:\\Music\\song.mp3");
}

// The real regression: Dart's Uri.file() output for a name with spaces,
// brackets and non-ASCII characters must decode back to the exact path.
TEST(TryFileUriToWindowsPath, SpecialCharacters) {
  std::string out;
  EXPECT_TRUE(TryFileUriToWindowsPath(
      "file:///C:/Music/Pensees%20%E2%A7%B8%E2%A7%B8%20Wit%E2%97%8Bhout%20U%20%5B6Z-JJdBlxXs%5D2.mp3",
      out));
  EXPECT_EQ(out,
      "C:\\Music\\Pensees \xE2\xA7\xB8\xE2\xA7\xB8 Wit\xE2\x97\x8Bhout U [6Z-JJdBlxXs]2.mp3");
}

// "file://server/share/file" → UNC "\\server\share\file".
TEST(TryFileUriToWindowsPath, UncPath) {
  std::string out;
  EXPECT_TRUE(TryFileUriToWindowsPath("file://server/share/song.mp3", out));
  EXPECT_EQ(out, "\\\\server\\share\\song.mp3");
}

}  // namespace test
}  // namespace just_audio_windows
