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

inline int HexValue(unsigned char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Percent-decodes a string at the byte level (the inverse of EncodeUri).
// Invalid or truncated "%XX" sequences are left literal.
inline std::string PercentDecode(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '%' && i + 2 < s.size()) {
      int hi = HexValue(static_cast<unsigned char>(s[i + 1]));
      int lo = HexValue(static_cast<unsigned char>(s[i + 2]));
      if (hi >= 0 && lo >= 0) {
        out += static_cast<char>((hi << 4) | lo);
        i += 2;
        continue;
      }
    }
    out += s[i];
  }
  return out;
}

// Converts a "file://" URI (such as the one produced by Dart's Uri.file()) into
// a native Windows filesystem path, percent-decoding the UTF-8 bytes. Returns
// false for any non-file URI (http/https/asset/...), leaving `outPath` untouched.
//
// Local files must be opened via StorageFile::GetFileFromPathAsync rather than
// MediaSource::CreateFromUri: WinRT's Uri canonicalizes the path (decoding
// bracket and non-ASCII escapes while keeping "%20"), and MediaFoundation's
// file:// resolver then fails to locate files whose names contain characters
// like "○", "⧸", "[" or "]" ("The system cannot find the file specified").
inline bool TryFileUriToWindowsPath(const std::string& uri, std::string& outPath) {
  const std::string scheme = "file:";
  if (uri.size() < scheme.size()) return false;
  for (size_t i = 0; i < scheme.size(); ++i) {
    if (static_cast<char>(std::tolower(static_cast<unsigned char>(uri[i]))) != scheme[i]) {
      return false;
    }
  }

  size_t pos = scheme.size();  // just past "file:"
  std::string authority;
  std::string path;
  if (uri.compare(pos, 2, "//") == 0) {
    pos += 2;
    size_t slash = uri.find('/', pos);
    if (slash == std::string::npos) {
      authority = uri.substr(pos);
    } else {
      authority = uri.substr(pos, slash - pos);
      path = uri.substr(slash);  // keeps the leading '/'
    }
  } else {
    // "file:C:/..." (no authority) — rare, but handle it.
    path = uri.substr(pos);
  }

  std::string decoded = PercentDecode(path);
  std::string result;
  if (!authority.empty()) {
    // "file://server/share/..." → UNC "\\server\share\...".
    result = "\\\\" + PercentDecode(authority) + decoded;
  } else {
    // "file:///C:/..." → strip the leading '/' before the drive letter.
    if (!decoded.empty() && decoded[0] == '/') {
      decoded.erase(0, 1);
    }
    result = decoded;
  }

  for (char& c : result) {
    if (c == '/') c = '\\';
  }
  outPath = result;
  return true;
}
