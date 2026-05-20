#pragma once
// Header-only base64 encode / decode (RFC 4648, no line breaks).
// Intended for binary IQ snapshots embedded in JSON messages.
// encode: arbitrary bytes  → ASCII string (ceil(n/3)*4 chars)
// decode: ASCII string     → raw bytes   (any padding / whitespace tolerated)
#include <cstring>
#include <string>
#include <vector>
#include <cstdint>

namespace sdr::base64 {

namespace detail {
inline constexpr char kEnc[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

inline int decval(char c) noexcept {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;  // padding ('=') or whitespace
}
} // namespace detail

// Encode [data, data+len) to base64 string.
inline std::string encode(const void* data, size_t len)
{
    const auto* src = static_cast<const uint8_t*>(data);
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t b = static_cast<uint32_t>(src[i]) << 16;
        if (i + 1 < len) b |= static_cast<uint32_t>(src[i + 1]) << 8;
        if (i + 2 < len) b |= static_cast<uint32_t>(src[i + 2]);
        out += detail::kEnc[(b >> 18) & 0x3F];
        out += detail::kEnc[(b >> 12) & 0x3F];
        out += (i + 1 < len) ? detail::kEnc[(b >> 6) & 0x3F] : '=';
        out += (i + 2 < len) ? detail::kEnc[b & 0x3F]        : '=';
    }
    return out;
}

// Decode base64 string to raw bytes.
inline std::vector<uint8_t> decode(const std::string& b64)
{
    std::vector<uint8_t> out;
    out.reserve(b64.size() * 3 / 4);
    uint32_t buf = 0;
    int      bits = 0;
    for (char c : b64) {
        int v = detail::decval(c);
        if (v < 0) continue;  // skip padding and whitespace
        buf  = (buf << 6) | static_cast<uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<uint8_t>((buf >> bits) & 0xFF));
        }
    }
    return out;
}

// Decode a base64 string into a vector<float>.
// The bytes are interpreted as little-endian IEEE 754 float32
// (i.e. the same format produced by encode(floats.data(), floats.size()*4)).
inline std::vector<float> decodeFloats(const std::string& b64)
{
    auto bytes = decode(b64);
    std::vector<float> out(bytes.size() / sizeof(float));
    if (!out.empty())
        std::memcpy(out.data(), bytes.data(), out.size() * sizeof(float));
    return out;
}

} // namespace sdr::base64
