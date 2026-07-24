/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

/*! \file qle/quotes/derivedquote.hpp
    \brief Add a helper to create a pointer to a derived quote
    \ingroup quotes
*/
#pragma once

#include <ql/quotes/derivedquote.hpp>

namespace QuantExt {

//! A helper to create a pointer to a derived quote.
template <class UnaryFunction>
QuantLib::ext::shared_ptr<QuantLib::DerivedQuote<UnaryFunction>> makeDerivedQuotePtr(
    QuantLib::Handle<QuantLib::Quote> element, UnaryFunction f, bool pureFunction = false)
{
    return QuantLib::ext::make_shared<QuantLib::DerivedQuote<UnaryFunction>>(
        std::move(element), std::move(f), pureFunction);
}

}
