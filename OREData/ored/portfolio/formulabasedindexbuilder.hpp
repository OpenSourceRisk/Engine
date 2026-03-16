/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/formulabasedindexbuilder.hpp
    \brief formula based index builder
    \ingroup portfolio
*/

#pragma once

#include <qle/indexes/formulabasedindex.hpp>

#include <ored/configuration/conventions.hpp>
#include <ored/marketdata/market.hpp>

namespace ore {
namespace data {
using namespace QuantLib;

/*! builds a formula based index using the ibor and swap indices in the given market, the fixing
  calendar is the joint holiday calendar of all constituents of the resulting index */
QuantLib::ext::shared_ptr<QuantExt::FormulaBasedIndex>
makeFormulaBasedIndex(const std::string& formula, const QuantLib::ext::shared_ptr<ore::data::Market> market,
                      const std::string& configuration,
                      std::map<std::string, QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>>& indexMaps,
                      const Calendar& fixingCalendar = Calendar());

} // namespace data
} // namespace ore
