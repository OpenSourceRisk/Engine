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

#include <qle/termstructures/spreadedsmilesection.hpp>
#include <qle/termstructures/spreadedswaptionvolatility.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

SpreadedSwaptionVolatility::SpreadedSwaptionVolatility(const Handle<SwaptionVolatilityStructure>& baseVol,
                                                       const Handle<Quote>& spread)
    : QuantLib::SpreadedSwaptionVolatility(baseVol, spread) {}

boost::shared_ptr<SmileSection> SpreadedSwaptionVolatility::smileSectionImpl(const Date& d, const Period& p) const {
    boost::shared_ptr<QuantLib::SpreadedSmileSection> section =
        boost::dynamic_pointer_cast<QuantLib::SpreadedSmileSection>(
            QuantLib::SpreadedSwaptionVolatility::smileSectionImpl(d, p));

    return boost::make_shared<SpreadedSmileSection>(section);
}

boost::shared_ptr<SmileSection> SpreadedSwaptionVolatility::smileSectionImpl(Time t, Time l) const {
    boost::shared_ptr<QuantLib::SpreadedSmileSection> section =
        boost::dynamic_pointer_cast<QuantLib::SpreadedSmileSection>(
            QuantLib::SpreadedSwaptionVolatility::smileSectionImpl(t, l));

    return boost::make_shared<SpreadedSmileSection>(section);
}

Volatility SpreadedSwaptionVolatility::volatilityImpl(const Date& d, const Period& p, Rate strike) const {
    Volatility spreadedVol = QuantLib::SpreadedSwaptionVolatility::volatilityImpl(d, p, strike);
    return std::max(spreadedVol, 0.0);
}

Volatility SpreadedSwaptionVolatility::volatilityImpl(Time t, Time l, Rate strike) const {
    Volatility spreadedVol = QuantLib::SpreadedSwaptionVolatility::volatilityImpl(t, l, strike);
    return std::max(spreadedVol, 0.0);
}
} // namespace QuantExt
