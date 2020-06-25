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
#include <qle/indexes/bmaindexwrapper.hpp>
#include <qle/indexes/bondindex.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/indexes/equityindex.hpp>
#include <qle/indexes/fxindex.hpp>

namespace ore {
namespace data {
using ore::data::Convention;
using QuantLib::Handle;
using QuantLib::IborIndex;
using QuantLib::Index;
using QuantLib::SwapIndex;
using QuantLib::YieldTermStructure;
using QuantLib::ZeroInflationIndex;
using QuantLib::ZeroInflationTermStructure;
using std::string;

//! Convert std::string to QuantExt::FxIndex
/*!
    \ingroup utilities
*/
boost::shared_ptr<QuantExt::FxIndex>
parseFxIndex(const string& s, const Handle<Quote>& fxSpot = Handle<Quote>(),
             const Handle<YieldTermStructure>& sourceYts = Handle<YieldTermStructure>(),
             const Handle<YieldTermStructure>& targetYts = Handle<YieldTermStructure>());

//! Convert std::string to QuantLib::IborIndex
/*!
    \ingroup utilities
*/
boost::shared_ptr<IborIndex> parseIborIndex(const string& s,
                                            const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>(),
                                            const boost::shared_ptr<Convention>& c = nullptr);

//! Convert std::string to QuantLib::IborIndex and return the tenor string component of the index
/*! In some cases, after parsing the IborIndex, we would like to know the exact tenor string that was part of
    the \p strIndex that was parsed. If we ask the resulting index for its tenor via the method
    `Period InterestRateIndex::tenor()`, it can be difficult to deduce the original tenor string. A simple example of
    this is `MXN-TIIE-28D` where if you call `tenor()` and then `to_string()`, you get `4W` which is different than
    the original `28D` that is passed in.

    \warning If the \p strIndex does not have a tenor component, as is the usual case for overnight indices,
             \p outTenor will be populated with the empty string.

    \ingroup utilities
*/
boost::shared_ptr<IborIndex> parseIborIndex(
    const std::string& strIndex, std::string& outTenor,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& h = QuantLib::Handle<QuantLib::YieldTermStructure>(),
    const boost::shared_ptr<Convention>& c = nullptr);

//! Try to convert std::string to QuantLib::IborIndex
/*!
    \ingroup utilities
*/
bool tryParseIborIndex(const string& s, boost::shared_ptr<IborIndex>& index,
                       const boost::shared_ptr<Convention>& c = nullptr);

//! Return true if the \p indexName is that of a generic index, otherwise false
/*!
    \ingroup utilities
*/
bool isGenericIndex(const string& indexName);

//! Return true if the \p indexName is that of an InflationIndex, otherwise false
/*!
    \ingroup utilities
*/
bool isInflationIndex(const std::string& indexName);

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
               boost::shared_ptr<Convention> convention = nullptr);

//! Convert std::string to QuantLib::ZeroInflationIndex
/*!
 \ingroup utilities
 */
boost::shared_ptr<ZeroInflationIndex>
parseZeroInflationIndex(const string& s, bool isInterpolated = false,
                        const Handle<ZeroInflationTermStructure>& h = Handle<ZeroInflationTermStructure>());

//! Convert std::string to QuantExt::BondIndex
/*!
 \ingroup utilities
 */
boost::shared_ptr<QuantExt::BondIndex> parseBondIndex(const string& s);

/*! Convert std::string to QuantExt::ComodityIndex

    This function can be used to parse commodity spot \e indices or commodity future \e indices:
    - for spot \e indices, the \p name is of the form <tt>COMM-EXCHANGE:COMMODITY</tt>
    - for future \e indices, the \p name is of the form <tt>COMM-EXCHANGE:CONTRACT:YYYY-MM</tt> or
      <tt>COMM-EXCHANGE:CONTRACT:YYYY-MM-DD</tt>

    \ingroup utilities
 */
boost::shared_ptr<QuantExt::CommodityIndex> parseCommodityIndex(
    const std::string& name, const QuantLib::Calendar& cal = QuantLib::NullCalendar(),
    const QuantLib::Handle<QuantExt::PriceTermStructure>& ts = QuantLib::Handle<QuantExt::PriceTermStructure>());

//! Convert std::string to QuantLib::Index
/*!
    \ingroup utilities
*/
boost::shared_ptr<Index> parseIndex(const string& s, const Conventions& conventions = Conventions());

//! Return true if the \p indexName is that of an overnight index, otherwise false
/*! \ingroup utilities
 */
bool isOvernightIndex(const std::string& indexName, const Conventions& conventions = Conventions());

/*! In some cases, we allow multiple external ibor index names to represent the same QuantLib index. This function
   returns the unique index name that we use internally to represent the QuantLib index.

    For example, we allow:
    - \c USD-FedFunds-1D and \c USD-FedFunds externally but we use \c USD-FedFunds internally
    - \c CAD-BA-tenor and \c CAD-CDOR-tenor externally but we use \c CAD-CDOR-tenor internally

    \ingroup utilities
*/
std::string internalIndexName(const std::string& indexName);

/*! Check if index is an fx index */
bool isFxIndex(const std::string& indexName);

/*! Invert an fx index */
std::string inverseFxIndex(const std::string& indexName);

} // namespace data
} // namespace ore
