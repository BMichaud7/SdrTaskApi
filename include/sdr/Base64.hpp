/*
========================================================================
Project: OpenRFStack
Author:  Brendan Michaud
Year:    2026
Part of OpenRFStack (https://github.com/OpenRFStack)

Licensed under the Personal Use License.
Do not use for commercial, organizational, or military purposes.
Contact author for permission: https://github.com/OpenRFStack
========================================================================
*/
#pragma once
/**
 * @file Base64.hpp
 * @brief Header-only RFC 4648 base64 encode/decode (no line breaks).
 *
 * Intended for embedding binary IQ snapshots inside JSON AMQP messages.
 *
 * **Encoding**: arbitrary bytes → ASCII string (⌈n/3⌉ × 4 characters).
 * **Decoding**: ASCII string → raw bytes (padding and whitespace tolerated).
 *
 * Typical usage in AcquisitionApp (encode):
 * @code
 * #include <sdr/Base64.hpp>
 * std::string b64 = sdr::base64::encode(snapshot.data(),
 *                                       snapshot.size() * sizeof(float));
 * @endcode
 *
 * Typical usage in AnalysisApp (decode):
 * @code
 * std::vector<float> iq = sdr::base64::decodeFloats(msg["iq_snapshot_b64"]);
 * @endcode
 */
#include <cstring>
#include <string>
#include <vector>
#include <cstdint>

/// @namespace sdr::base64
/// @brief RFC 4648 base64 encode/decode utilities.
namespace sdr::base64 {

/// @cond INTERNAL
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
/// @endcond

/**
 * @brief Encode a binary buffer to a base64 string.
 * @param data Pointer to the first byte to encode.
 * @param len  Number of bytes to encode.
 * @return Base64-encoded ASCII string (no newlines, standard alphabet).
 */
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

/**
 * @brief Decode a base64 string to raw bytes.
 * @param b64 Base64-encoded string (padding and whitespace tolerated).
 * @return Decoded bytes.
 */
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

/**
 * @brief Decode a base64 string into a vector of float32 values.
 *
 * The bytes are interpreted as little-endian IEEE 754 float32 —
 * the same format produced by `encode(floats.data(), floats.size() * 4)`.
 * Used to recover CF32 IQ snapshots from RF_DETECTION messages.
 *
 * @param b64 Base64-encoded string of raw float32 bytes.
 * @return Decoded float32 values (interleaved I,Q,I,Q,… for CF32 IQ data).
 */
inline std::vector<float> decodeFloats(const std::string& b64)
{
    auto bytes = decode(b64);
    std::vector<float> out(bytes.size() / sizeof(float));
    if (!out.empty())
        std::memcpy(out.data(), bytes.data(), out.size() * sizeof(float));
    return out;
}

} // namespace sdr::base64
