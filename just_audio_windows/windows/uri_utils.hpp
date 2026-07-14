#pragma once

#include <cctype>
#include <cstdio>
#include <string>

// Percent-encodes a UTF-8 URI string so that Windows::Foundation::Uri accepts
// it. Windows::Foundation::Uri is a strict RFC 3986 parser: every character
// outside the unreserved/reserved sets must be percent-encoded, otherwise the
// constructor throws and MediaSource::CreateFromUri fails. This affects local
// file paths that contain literal spaces or non-ASCII characters such as
// "○", "¤", CJK, or emoji (e.g. "file:///C:/Music/○ song.mp3").
//
// Encoding is performed on the UTF-8 *bytes*, which is exactly what a
// percent-encoded IRI path expects: a multi-byte character like "○"
// (UTF-8: E2 97 8B) becomes "%E2%97%8B".
//
// URI structural characters (scheme, path separators, drive-letter colon,
// query/fragment delimiters) are left untouched so the "file:///C:/..." form
// is preserved. Existing "%XX" escapes are also passed through unchanged so a
// URI that is already correctly encoded (e.g. from Dart's Uri.file()) is never
// double-encoded.

// RFC 3986 unreserved characters: never need encoding.
inline bool IsUnreserved(unsigned char c) {
  return std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~';
}

// Characters that make up the URI's structure. Keeping these literal preserves
// the scheme, path separators, drive-letter colon, and query/fragment.
inline bool IsUriSafeDelimiter(unsigned char c) {
  static const std::string kSafe = ":/?#[]@!$&'()*+,;=";
  return kSafe.find(static_cast<char>(c)) != std::string::npos;
}

inline bool IsHexDigit(unsigned char c) {
  return std::isxdigit(c) != 0;
}

inline std::string EncodeUri(const std::string& uri) {
  std::string encoded;
  // Reserve worst-case capacity (every byte percent-encoded → 3 chars each).
  encoded.reserve(uri.length() * 3);

  for (size_t i = 0; i < uri.length(); ++i) {
    unsigned char c = static_cast<unsigned char>(uri[i]);

    // Leave existing percent-escapes (%20, %E2%80%99, ...) untouched.
    if (c == '%' && i + 2 < uri.length() &&
        IsHexDigit(static_cast<unsigned char>(uri[i + 1])) &&
        IsHexDigit(static_cast<unsigned char>(uri[i + 2]))) {
      encoded += uri.substr(i, 3);
      i += 2;
      continue;
    }

    if (IsUnreserved(c) || IsUriSafeDelimiter(c)) {
      encoded += static_cast<char>(c);
    } else {
      char buf[4];
      std::snprintf(buf, sizeof(buf), "%%%02X", c);
      encoded += buf;
    }
  }

  return encoded;
}
