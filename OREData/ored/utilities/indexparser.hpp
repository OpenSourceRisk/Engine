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
#include <ql/indexes/inflationindex.hpp>
#include <ql/indexes/swapindex.hpp>
#include <qle/indexes/bmaindexwrapper.hpp>
#include <qle/indexes/bondindex.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/indexes/equityindex.hpp>
#include <qle/indexes/fxindex.hpp>

namespace ore {
namespace data {
using QuantLib::Handle;
using QuantLib::IborIndex;
using QuantLib::Index;
using QuantLib::Quote;
using QuantLib::SwapIndex;
using QuantLib::YieldTermStructure;
using QuantLib::ZeroInflationIndex;
using QuantLib::ZeroInflationTermStructure;
using std::string;

//! Convert std::string to QuantExt::FxIndex
/*!
    \ingroup utilities
*/
QuantLib::ext::shared_ptr<QuantExt::FxIndex>
parseFxIndex(const string& s, const Handle<Quote>& fxSpot = Handle<Quote>(),
             const Handle<YieldTermStructure>& sourceYts = Handle<YieldTermStructure>(),
             const Handle<YieldTermStructure>& targetYts = Handle<YieldTermStructure>(),
             const bool useConventions = false);

//! Convert std::string to QuantLib::IborIndex
/*!
    \ingroup utilities
*/
QuantLib::ext::shared_ptr<IborIndex> parseIborIndex(const string& s,
                                            const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>());

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
QuantLib::ext::shared_ptr<IborIndex> parseIborIndex(
    const std::string& strIndex, std::string& outTenor,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& h = QuantLib::Handle<QuantLib::YieldTermStructure>());

//! Try to convert std::string to QuantLib::IborIndex
/*!
    \ingroup utilities
*/
bool tryParseIborIndex(const string& s, QuantLib::ext::shared_ptr<IborIndex>& index);

//! Return true if the \p indexName is that of a generic ibor index, otherwise false
/*!
    \ingroup utilities
*/
bool isGenericIborIndex(const string& indexName);

/*! Returns true as the first element in the pair if the \p indexName is that of an InflationIndex. The second element 
    in the pair is an instance of the inflation index. If \p indexName is not an inflation index, the first element in 
    the pair is \c false and the second element is a \c nullptr.

    If inflation indices have been set up via ZeroInflationIndex entries in the Conventions, the \p conventions
    should be passed here. If not, the default \c nullptr parameter will be sufficient.

    \ingroup utilities
*/
std::pair<bool, QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>> isInflationIndex(const std::string& indexName);

//! Return true if the \p indexName is that of an EquityIndex, otherwise false
/*!
    \ingroup utilities
*/
bool isEquityIndex(const std::string& indexName);

//! Return true if the \p indexName is that of an CommodityIndex, otherwise false
/*!
    \ingroup utilities
*/
bool isCommodityIndex(const std::string& indexName);

/*! Return true if the \p indexName is that of an GenericIndex, otherwise false
    \ingroup utilities
*/
bool isGenericIndex(const std::string& indexName);

//! Convert std::string (e.g SP5) to QuantExt::EquityIndex
/*!
\ingroup utilities
*/
QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> parseEquityIndex(const string& s);

//! Convert std::string to QuantLib::SwapIndex
/*!
    \ingroup utilities
*/
QuantLib::ext::shared_ptr<SwapIndex>
parseSwapIndex(const string& s, const Handle<YieldTermStructure>& forwarding = Handle<YieldTermStructure>(),
               const Handle<YieldTermStructure>& discounting = Handle<YieldTermStructure>());

//! Convert std::string to QuantLib::ZeroInflationIndex
/*!
 \ingroup utilities
 */
QuantLib::ext::shared_ptr<ZeroInflationIndex>
parseZeroInflationIndex(const string& s, const Handle<ZeroInflationTermStructure>& h = Handle<ZeroInflationTermStructure>());

QL_DEPRECATED
QuantLib::ext::shared_ptr<ZeroInflationIndex>
parseZeroInflationIndex(const string& s, bool isInterpolated,
                        const Handle<ZeroInflationTermStructure>& h = Handle<ZeroInflationTermStructure>());

//! Convert std::string to QuantExt::BondIndex
/*!
 \ingroup utilities
 */
QuantLib::ext::shared_ptr<QuantExt::BondIndex> parseBondIndex(const string& s);

//! Convert std::string to QuantExt::ConstantMaturityBondIndex
/*!
 \ingroup utilities
 */
QuantLib::ext::shared_ptr<QuantExt::ConstantMaturityBondIndex> parseConstantMaturityBondIndex(const string& s);

/*! Convert std::string to QuantExt::ComodityIndex

    This function can be used to parse commodity spot \e indices or commodity future \e indices:
    - for spot \e indices, the \p name is of the form <tt>COMM-EXCHANGE:COMMODITY</tt>
    - for future \e indices, the \p name is of the form <tt>COMM-EXCHANGE:CONTRACT:YYYY-MM</tt> or
      <tt>COMM-EXCHANGE:CONTRACT:YYYY-MM-DD</tt>

    \ingroup utilities
 */
QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> parseCommodityIndex(
    const std::string& name, bool hasPrefix = true,
    const QuantLib::Handle<QuantExt::PriceTermStructure>& ts = QuantLib::Handle<QuantExt::PriceTermStructure>(),
    const QuantLib::Calendar& cal = QuantLib::NullCalendar(), const bool enforceFutureIndex = true);

//! Convert std::string (GENERIC-...) to QuantExt::Index
/*!
    \ingroup utilities
*/
QuantLib::ext::shared_ptr<QuantLib::Index> parseGenericIndex(const string& s);

//! Convert std::string to QuantLib::Index
/*!
    \ingroup utilities
*/
QuantLib::ext::shared_ptr<Index> parseIndex(const string& s);

//! Return true if the \p indexName is that of an overnight index, otherwise false
/*! \ingroup utilities
 */
bool isOvernightIndex(const std::string& indexName);

//! Return true if the \p indexName is that of an bma/sifma index, otherwise false
/*! \ingroup utilities
 */
bool isBmaIndex(const std::string& indexName);

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
