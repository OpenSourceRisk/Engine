/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file logquote.cpp
    \brief implementation file for logquote
*/

#include <qle/quotes/logquote.hpp>

namespace QuantExt {

    Real LogQuote::quote() const {
        return q_->value();
    }

    Real LogQuote::value() const {
        return logValue_;
    }

    bool LogQuote::isValid() const {
        return q_->isValid();
    }

    void LogQuote::update() {
        Real v = q_->value();
        QL_REQUIRE(v>0.0, "Invalid quote, cannot take log of non-postive number");
        logValue_ = std::log(v);
    }
}
