/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file quotes/compositevectorquote.hpp
    \brief applies a function of a vector of quotes
    \ingroup quotes
*/

#pragma once

#include <ql/quote.hpp>

namespace QuantExt {

using namespace QuantLib;

template <typename Function> class CompositeVectorQuote : public Quote, public Observer {
public:
    CompositeVectorQuote(std::vector<Handle<Quote>> q, Function f) : q_(std::move(q)), f_(f) {
        for (auto const& q : q_)
            registerWith(q);
    }
    Real value() const override {
        std::vector<Real> v(q_.size());
        std::transform(q_.begin(), q_.end(), v.begin(), [](const Handle<Quote>& q) { return q->value(); });
        return f_(v);
    }
    bool isValid() const override {
        return std::all_of(q_.begin(), q_.end(), [](const Handle<Quote>& q) { return q->isValid(); });
    }
    void update() override { notifyObservers(); }

protected:
    std::vector<Handle<Quote>> q_;
    Function f_;
};

} // namespace QuantExt
