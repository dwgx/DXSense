#pragma once

// ============================================================================
// JsonLite — zero-dependency JSON token scanner for Python bridge output.
//
// DXSense panels receive JSON strings from exec_and_collect() and need to
// extract values without pulling in a full JSON library (which would bloat
// the injected DLL). This header provides the shared primitives that
// VulnLabPanel, InteractionFatherPanel, and any future bridge consumer need.
//
// All functions operate on string_view and never allocate unless they return
// a std::string (decode_string, for example).
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace dxs::json {

// --- Whitespace -------------------------------------------------------------

inline bool is_ws(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

inline void skip_ws(std::string_view s, std::size_t& pos) {
    while (pos < s.size() && is_ws(s[pos])) ++pos;
}

// --- Scanners ---------------------------------------------------------------
// Each scanner advances `pos` past the scanned token and returns true on
// success.  On failure `pos` is left at an indeterminate position — callers
// should abort parsing.

inline bool scan_string(std::string_view s, std::size_t& pos) {
    if (pos >= s.size() || s[pos] != '"') return false;
    bool esc = false;
    for (++pos; pos < s.size();) {
        const char c = s[pos++];
        if (esc)  { esc = false; continue; }
        if (c == '\\') { esc = true; continue; }
        if (c == '"')  return true;
    }
    return false;
}

inline bool scan_compound(std::string_view s, std::size_t& pos,
                          char open_ch, char close_ch) {
    if (pos >= s.size() || s[pos] != open_ch) return false;
    int depth = 0;
    while (pos < s.size()) {
        const char c = s[pos];
        if (c == '"') { if (!scan_string(s, pos)) return false; continue; }
        ++pos;
        if (c == open_ch)  ++depth;
        if (c == close_ch && --depth == 0) return true;
    }
    return false;
}

inline bool scan_value(std::string_view s, std::size_t& pos) {
    skip_ws(s, pos);
    if (pos >= s.size()) return false;
    if (s[pos] == '"') return scan_string(s, pos);
    if (s[pos] == '{') return scan_compound(s, pos, '{', '}');
    if (s[pos] == '[') return scan_compound(s, pos, '[', ']');
    // Bare token (number, bool, null).
    while (pos < s.size() && s[pos] != ',' && s[pos] != '}'
           && s[pos] != ']' && !is_ws(s[pos]))
        ++pos;
    return true;
}

// --- Lookup -----------------------------------------------------------------

// Find the value associated with `key` inside a JSON object fragment `obj`.
// Returns the raw token (including quotes for strings). Returns empty on miss.
inline std::string_view find_value(std::string_view obj, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\":";
    const std::size_t at = obj.find(needle);
    if (at == std::string_view::npos) return {};
    std::size_t beg = at + needle.size();
    skip_ws(obj, beg);
    std::size_t end = beg;
    if (!scan_value(obj, end) || end <= beg) return {};
    return obj.substr(beg, end - beg);
}

// Find the first top-level `{...}` in a raw Python output string.
inline std::string_view extract_object(std::string_view out) {
    const std::size_t beg = out.find('{');
    const std::size_t end = out.rfind('}');
    if (beg == std::string_view::npos || end == std::string_view::npos || end < beg)
        return {};
    return out.substr(beg, end - beg + 1);
}

// --- Decoders ---------------------------------------------------------------

inline std::string decode_string(std::string_view tok) {
    if (tok.size() < 2 || tok.front() != '"' || tok.back() != '"') return {};
    std::string out;
    out.reserve(tok.size() - 2);
    for (std::size_t i = 1; i + 1 < tok.size(); ++i) {
        const char c = tok[i];
        if (c != '\\') { out.push_back(c); continue; }
        if (i + 1 >= tok.size() - 1) break;
        switch (const char e = tok[++i]) {
            case '"':  out.push_back('"');  break;
            case '\\': out.push_back('\\'); break;
            case '/':  out.push_back('/');  break;
            case 'b':  out.push_back('\b'); break;
            case 'f':  out.push_back('\f'); break;
            case 'n':  out.push_back('\n'); break;
            case 'r':  out.push_back('\r'); break;
            case 't':  out.push_back('\t'); break;
            case 'u':  if (i + 4 < tok.size() - 1) i += 4; break;
            default:   out.push_back(e); break;
        }
    }
    return out;
}

inline double parse_double(std::string_view tok) {
    if (tok.empty()) return 0.0;
    std::string tmp(tok);
    return std::strtod(tmp.c_str(), nullptr);
}

inline int parse_int(std::string_view tok) {
    if (tok.empty()) return 0;
    std::string tmp(tok);
    return static_cast<int>(std::strtol(tmp.c_str(), nullptr, 10));
}

inline std::uint64_t parse_u64(std::string_view tok) {
    if (tok.empty()) return 0;
    std::string tmp(tok);
    return static_cast<std::uint64_t>(std::strtoull(tmp.c_str(), nullptr, 10));
}

inline bool parse_bool(std::string_view tok) {
    return tok == "true";
}

// --- Iteration --------------------------------------------------------------

// Call `fn(std::string_view element_token)` for each element in a JSON array.
template <typename Fn>
void for_each_array(std::string_view arr, Fn&& fn) {
    if (arr.size() < 2 || arr.front() != '[' || arr.back() != ']') return;
    for (std::size_t pos = 1; pos + 1 < arr.size();) {
        skip_ws(arr, pos);
        if (pos >= arr.size() || arr[pos] == ']') break;
        if (arr[pos] == ',') { ++pos; continue; }
        const std::size_t beg = pos;
        if (!scan_value(arr, pos) || pos <= beg) break;
        fn(arr.substr(beg, pos - beg));
    }
}

}  // namespace dxs::json
