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

#include <ql/index.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/indexes/swapindex.hpp>
#include <ored/configuration/conventions.hpp>
#include <qle/indexes/fxindex.hpp>
using std::string;
using QuantLib::IborIndex;
using QuantLib::SwapIndex;
using QuantLib::Index;
using QuantLib::Handle;
using QuantLib::YieldTermStructure;
using ore::data::Convention;

namespace ore {
namespace data {

//! Convert std::string to QuantLib::IborIndex
/*!
    \ingroup utilities
*/
boost::shared_ptr<QuantExt::FxIndex> parseFxIndex(const string& s);
boost::shared_ptr<IborIndex> parseIborIndex(const string& s,
                                            const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>());

//! Convert std::string to QuantLib::SwapIndex
/*!
    \ingroup utilities
*/
boost::shared_ptr<SwapIndex>
parseSwapIndex(const string& s, const Handle<YieldTermStructure>& forwarding,
               const Handle<YieldTermStructure>& discounting,
               boost::shared_ptr<data::IRSwapConvention> convention = boost::shared_ptr<data::IRSwapConvention>());

//! Convert std::string to QuantLib::Index
/*!
    \ingroup utilities
*/
boost::shared_ptr<Index> parseIndex(const string& s,
                                    const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>());
}
}
