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

/*! \file orea/simm/portfoliomodifier.hpp
    \brief Modify a portfolio before a SIMM calculation
*/

#pragma once

#include <set>
#include <orea/simm/crifrecord.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/envelope.hpp>
#include <ored/portfolio/portfolio.hpp>

namespace ore {
namespace analytics {

/*! Modify the \p portfolio by applying the various SIMM exemptions outlined in the
    document <em>SIMM Cross-Currency Swap Treatment, February 27, 2017<\em>
    Returns a pair of sets (s1,s2), where s1 contains the trade ids that were removed
    and s2 conatins the trade ids that were modified.
*/
std::pair<std::set<std::string>, std::set<std::string>>
applySimmExemptions(ore::data::Portfolio& portfolio,
                    const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory,
                    const std::set<ore::analytics::CrifRecord::Regulation>& simmExemptionOverrides = {});

} // namespace analytics
} // namespace ore
