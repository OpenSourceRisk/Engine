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

#include <qle/termstructures/spreadedoptionletvolatility.hpp>
#include <qle/termstructures/spreadedsmilesection.hpp>

namespace QuantExt {

SpreadedOptionletVolatility::SpreadedOptionletVolatility(const Handle<OptionletVolatilityStructure>& baseVol,
                                                         const Handle<Quote>& spread)
    : QuantLib::SpreadedOptionletVolatility(baseVol, spread) {}

boost::shared_ptr<SmileSection> SpreadedOptionletVolatility::smileSectionImpl(const Date& d) const {
    boost::shared_ptr<QuantLib::SpreadedSmileSection> section =
        boost::dynamic_pointer_cast<QuantLib::SpreadedSmileSection>(
            QuantLib::SpreadedOptionletVolatility::smileSectionImpl(d));

    return boost::make_shared<SpreadedSmileSection>(section);
}

boost::shared_ptr<SmileSection> SpreadedOptionletVolatility::smileSectionImpl(Time optionTime) const {
    boost::shared_ptr<QuantLib::SpreadedSmileSection> section =
        boost::dynamic_pointer_cast<QuantLib::SpreadedSmileSection>(
            QuantLib::SpreadedOptionletVolatility::smileSectionImpl(optionTime));

    return boost::make_shared<SpreadedSmileSection>(section);
}

Volatility SpreadedOptionletVolatility::volatilityImpl(Time t, Rate s) const {
    Volatility spreadedVol = QuantLib::SpreadedOptionletVolatility::volatilityImpl(t, s);
    return std::max(spreadedVol, 0.0);
}
} // namespace QuantExt
