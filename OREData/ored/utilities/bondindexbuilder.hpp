/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file ored/utilities/bondindexbuilder.hpp
    \brief Interface for building a bond index
    \ingroup utilities
*/

#pragma once

#include <ored/portfolio/bond.hpp>

namespace ore {
namespace data {

class BondIndexBuilder {
public:
    BondIndexBuilder(BondData bondData, const bool dirty, const bool relative,
                     const Calendar& fixingCalendar, const bool conditionalOnSurvival, 
                     const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                     QuantLib::Real bidAskAdjustment = 0.0, const bool bondIssueDateFallback = false);

    BondIndexBuilder(const Bond& bond, const bool dirty, const bool relative,
                     const Calendar& fixingCalendar, const bool conditionalOnSurvival, 
                     const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                     QuantLib::Real bidAskAdjustment = 0.0, const bool bondIssueDateFallback = false);

    BondIndexBuilder(const std::string& securityId, const bool dirty, const bool relative, 
                     const Calendar& fixingCalendar, const bool conditionalOnSurvival, 
                     const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, QuantLib::Real bidAskAdjustment = 0.0,
                     const bool bondIssueDateFallback = false);

    QuantLib::ext::shared_ptr<QuantExt::BondIndex> bondIndex() const;
    void addRequiredFixings(RequiredFixings& requiredFixings, Leg leg = {});
    const Bond& bond() const { return bond_; };
    QuantLib::Real priceAdjustment(QuantLib::Real price);

private:
    Bond bond_;
    QuantLib::ext::shared_ptr<QuantExt::BondIndex> bondIndex_;
    RequiredFixings fixings_;
    const bool dirty_;

    void buildIndex(const bool relative, const Calendar& fixingCalendar, const bool conditionalOnSurvival, 
        const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, 
        QuantLib::Real bidAskAdjustment, const bool bondIssueDateFallback);
};

} // namespace data
} // namespace ore
