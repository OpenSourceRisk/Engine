/*
 Copyright (C) 2026 AcadiaSoft, Inc.
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file orea/cube/cube_io_utils.hpp
    \brief Fast I/O helpers shared by cube_io.cpp and cubewriter/reader translation units.
    \ingroup cube
*/

#pragma once

#include <ql/types.hpp>

#include <boost/iostreams/filtering_stream.hpp>

#include <charconv>
#include <cstring>
#include <system_error>
#include <vector>

// ---------------------------------------------------------------------------
// Apple / libc++ availability guard for floating-point charconv.
//
// std::to_chars / std::from_chars for float/double are gated on different
// macOS dylib versions.  The macro below is defined by the SDK when neither
// direction is available; on every other platform fp charconv is fine.
// ---------------------------------------------------------------------------
#if defined(__APPLE__) && defined(_LIBCPP_VERSION)
#  if !defined(_LIBCPP_AVAILABILITY_HAS_FROM_CHARS_FLOATING_POINT) || \
      !_LIBCPP_AVAILABILITY_HAS_FROM_CHARS_FLOATING_POINT
#    define ORE_CHARCONV_FLOAT_UNAVAILABLE
#  endif
#endif

namespace ore {
namespace analytics {
namespace cube_io_utils {

using QuantLib::Size;

//! Parse an unsigned integer from [p, end), advance p past the token and the following comma.
inline bool fast_parse_size(const char*& p, const char* end, Size& out) {
    auto [ptr, ec] = std::from_chars(p, end, out);
    if (ec != std::errc{})
        return false;
    p = ptr;
    if (p != end && *p == ',')
        ++p;
    return true;
}

//! Parse a double from [p, end), advance p past the token and the following comma.
inline bool fast_parse_double(const char*& p, const char* end, double& out) {
#ifdef ORE_CHARCONV_FLOAT_UNAVAILABLE
    char* endptr = nullptr;
    out = std::strtod(p, &endptr);
    if (endptr == p)
        return false;
    p = endptr;
#else
    auto [ptr, ec] = std::from_chars(p, end, out);
    if (ec != std::errc{})
        return false;
    p = ptr;
#endif
    if (p != end && *p == ',')
        ++p;
    return true;
}

//! Write a double into [buf, end) and return a pointer past the last character written.
inline char* write_double(char* buf, char* end, double v) {
#ifdef ORE_CHARCONV_FLOAT_UNAVAILABLE
    int n = std::snprintf(buf, static_cast<std::size_t>(end - buf), "%.17g", v);
    return buf + (n > 0 ? n : 0);
#else
    return std::to_chars(buf, end, v).ptr;
#endif
}

//! Batches small writes into a 1 MB buffer before flushing to the underlying stream.
struct BufWriter {
    static constexpr std::size_t CAPACITY = 1u << 20; // 1 MB

    explicit BufWriter(boost::iostreams::filtering_stream<boost::iostreams::output>& s)
        : sink_(s), buf_(CAPACITY) {}

    ~BufWriter() { flush(); }

    void flush() {
        if (pos_ > 0) {
            sink_.write(buf_.data(), static_cast<std::streamsize>(pos_));
            pos_ = 0;
        }
    }

    // Caller must ensure n < CAPACITY.
    void write(const char* data, std::size_t n) {
        if (pos_ + n > CAPACITY - 512)
            flush();
        std::memcpy(buf_.data() + pos_, data, n);
        pos_ += n;
    }

    void put(char c) {
        if (pos_ + 1 > CAPACITY - 512)
            flush();
        buf_[pos_++] = c;
    }

private:
    boost::iostreams::filtering_stream<boost::iostreams::output>& sink_;
    std::vector<char> buf_;
    std::size_t pos_ = 0;
};

} // namespace cube_io_utils
} // namespace analytics
} // namespace ore
