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

#include <ql/handle.hpp>
#include <ql/quote.hpp>
#include <ql/types.hpp>
#include <vector>

namespace ore {
namespace data {
using QuantLib::Handle;
using QuantLib::Quote;

//! Intelligent FX price repository
/*! FX Triangulation is an intelligent price repository that will attempt to calculate FX spot values
 *
 *  As quotes for currency pairs are added to the repository they are stored in an internal map
 *  If the repository is asked for the FX spot price for a given pair it will attempt the following:
 *  1) Look in the map for the pair
 *  2) Look for the reverse quote (EURUSD -> USDEUR), if found it will return an inverse quote.
 *  3) Look through the map and attempt to find a bridging pair (e.g EURUSD and EURJPY for USDJPY)
 *     and return the required composite quote.
 *
 *  In cases (2) and (3) the constructed quote is then stored in the map so subsequent calls will hit (1).
 *
 *  The constructed quotes all reference the original quotes which are added by the addQuote() method
 *  and so if these original quotes change in the future, the constructed quotes will reflect the new
 *  value
 *
 *  Warning: The result of getQuote() can depend on previous calls to getQuote(), since newly added
 *  pairs in 3) are candidates for bridging pairs in future calls and the bridging only uses one
 *  intermediate currency. This state-dependent behaviour will be removed in a future release.
 *
 *  \ingroup marketdata
 */
class FXTriangulation {
public:
    //! Default ctor, once built the repo is empty
    FXTriangulation() {}

    //! Add a quote to the repo
    void addQuote(const std::string& pair, const Handle<Quote>& spot);

    //! Get a quote from the repo, this will follow the algorithm described above
    Handle<Quote> getQuote(const std::string&) const;

    //! Get all quotes currently stored in the triangulation
    const std::vector<std::pair<std::string, Handle<Quote>>>& quotes() const { return map_; }

private:
    mutable std::vector<std::pair<std::string, Handle<Quote>>> map_;
};
} // namespace data
} // namespace ore
