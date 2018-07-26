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

/*! \file ored/utilities/indexparser.hpp
    \brief Map text representations to QuantLib/QuantExt types
    \ingroup utilities
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ql/index.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/indexes/inflationindex.hpp>
#include <ql/indexes/swapindex.hpp>
#include <qle/indexes/equityindex.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/indexes/bmaindexwrapper.hpp>

using std::string;
using QuantLib::IborIndex;
using QuantLib::SwapIndex;
using QuantLib::Index;
using QuantLib::ZeroInflationIndex;
using QuantLib::Handle;
using QuantLib::YieldTermStructure;
using QuantLib::ZeroInflationTermStructure;
using ore::data::Convention;

namespace ore {
namespace data {

//! Convert std::string to QuantExt::FxIndex
/*!
    \ingroup utilities
*/
boost::shared_ptr<QuantExt::FxIndex> parseFxIndex(const string& s);

//! Convert std::string to QuantLib::IborIndex
/*!
    \ingroup utilities
*/
boost::shared_ptr<IborIndex> parseIborIndex(const string& s,
                                            const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>());

//! Convert std::string (e.g SP5) to QuantExt::EquityIndex
/*!
\ingroup utilities
*/
boost::shared_ptr<QuantExt::EquityIndex> parseEquityIndex(const string& s);

//! Convert std::string to QuantLib::SwapIndex
/*!
    \ingroup utilities
*/
boost::shared_ptr<SwapIndex>
parseSwapIndex(const string& s, const Handle<YieldTermStructure>& forwarding = Handle<YieldTermStructure>(),
               const Handle<YieldTermStructure>& discounting = Handle<YieldTermStructure>(),
               boost::shared_ptr<data::IRSwapConvention> convention = boost::shared_ptr<data::IRSwapConvention>());

//! Convert std::string to QuantLib::ZeroInflationIndex
/*!
 \ingroup utilities
 */
boost::shared_ptr<ZeroInflationIndex>
parseZeroInflationIndex(const string& s, bool isInterpolated = false,
                        const Handle<ZeroInflationTermStructure>& h = Handle<ZeroInflationTermStructure>());

//! Convert std::string to QuantLib::Index
/*!
    \ingroup utilities
*/
boost::shared_ptr<Index> parseIndex(const string& s, const Conventions& conventions = Conventions());
} // namespace data
} // namespace ore
