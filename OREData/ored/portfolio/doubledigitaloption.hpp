/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/doubledigitaloption.hpp
    \brief double digital option wrapper for scripted trade
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/scriptedtrade.hpp>

#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

using namespace ore::data;

class DoubleDigitalOption : public ScriptedTrade {
public:
    DoubleDigitalOption() : ScriptedTrade("DoubleDigitalOption") {}
    DoubleDigitalOption(const Envelope& env, const string& expiry, const string& settlement, const string& binaryPayout,
                        const string& binaryLevel1, const string& binaryLevel2, const string& type1,
                        const string& type2, const string& position,
                        const QuantLib::ext::shared_ptr<Underlying>& underlying1,
                        const QuantLib::ext::shared_ptr<Underlying>& underlying2,
                        const QuantLib::ext::shared_ptr<Underlying>& underlying3,
                        const QuantLib::ext::shared_ptr<Underlying>& underlying4, const string& payCcy,
                        const QuantLib::ext::shared_ptr<Conventions>& conventions = nullptr,
                        const std::string& binaryLevelUpper1 = std::string(),
                        const std::string& binaryLevelUpper2 = std::string())
        : ScriptedTrade("DoubleDigitalOption", env), expiry_(expiry), settlement_(settlement),
          binaryPayout_(binaryPayout), binaryLevel1_(binaryLevel1), binaryLevel2_(binaryLevel2), type1_(type1),
          type2_(type2), position_(position), payCcy_(payCcy), binaryLevelUpper1_(binaryLevelUpper1),
          binaryLevelUpper2_(binaryLevelUpper2), underlying1_(underlying1),
          underlying2_(underlying2), underlying3_(underlying3), underlying4_(underlying4) {
        initIndices();
    }
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

private:
    void initIndices();
    string expiry_, settlement_, binaryPayout_, binaryLevel1_, binaryLevel2_, type1_, type2_, position_, payCcy_,
        binaryLevelUpper1_, binaryLevelUpper2_;
    QuantLib::ext::shared_ptr<Underlying> underlying1_, underlying2_, underlying3_, underlying4_;
};

} // namespace data
} // namespace ore
