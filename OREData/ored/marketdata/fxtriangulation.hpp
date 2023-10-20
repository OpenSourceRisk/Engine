/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file marketdata/fxtriangulation.hpp
    \brief Intelligent FX price repository
    \ingroup marketdata
*/

#pragma once

#include <ored/marketdata/market.hpp>

#include <qle/indexes/fxindex.hpp>

#include <ql/handle.hpp>
#include <ql/quote.hpp>
#include <ql/types.hpp>

#include <vector>

namespace ore {
namespace data {

class FXTriangulation {
public:
    /*! Set up empty repository */
    FXTriangulation() {}

    /*! Set up fx quote repository with available market quotes ccypair => quote */
    explicit FXTriangulation(std::map<std::string, QuantLib::Handle<QuantLib::Quote>> quotes);

    /*! Get quote, possibly via triangulation
        If you need an exact handling of spot lag differences, use getIndex() instead.
    */
    QuantLib::Handle<QuantLib::Quote> getQuote(const std::string& pair) const;

    /*! Get fx index, possibly via triangulation. The index name can be of the form FX-TAG-CCY1-CCY2 or also
        be just a currency pair CCY1CCY2. In the latter case, the fixing source is set to TAG = GENERIC.
        The fx index requires discount curves from a market. The assumption is that the market provides discount
        curves consistent with cross-currency discounting under its default configuration. If the triangulation
        is not possible or required curves are not available an exception is thrown.
    */
    QuantLib::Handle<QuantExt::FxIndex> getIndex(const std::string& indexOrPair, const Market* market) const;

private:
    /* get path for conversion forCcy => domCcy, throws if such a path does not exist     */
    std::vector<std::string> getPath(const std::string& forCcy, const std::string& domCcy) const;

    /* return quote or inverse quote to convert forCcy => domCcy, there must be an input quote for the pair in either
       order, otherwise throws */
    Handle<Quote> getQuote(const std::string& forCcy, const std::string& domCcy) const;

    /* return a string enumerating all quotes as a comma separated list (for error messages) */
    std::string getAllQuotes() const;

    // the input quotes
    std::map<std::string, QuantLib::Handle<QuantLib::Quote>> quotes_;

    // caches to improve perfomance
    mutable std::map<std::string, QuantLib::Handle<QuantLib::Quote>> quoteCache_;
    mutable std::map<std::string, QuantLib::Handle<QuantExt::FxIndex>> indexCache_;

    // internal data structure to represent the undirected graph of currencies
    std::vector<std::string> nodeToCcy_;
    std::map<std::string, std::size_t> ccyToNode_;
    std::vector<std::set<std::size_t>> neighbours_;
};

} // namespace data
} // namespace ore
