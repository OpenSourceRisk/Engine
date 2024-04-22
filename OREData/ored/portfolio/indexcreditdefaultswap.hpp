/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#pragma once

#include <ored/portfolio/referencedata.hpp>
#include <ored/portfolio/builders/indexcreditdefaultswap.hpp>
#include <ored/portfolio/indexcreditdefaultswapdata.hpp>

#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

class IndexCreditDefaultSwap : public Trade {
public:
    //! Default constructor
    IndexCreditDefaultSwap() : Trade("IndexCreditDefaultSwap") {}

    //! Constructor
    IndexCreditDefaultSwap(const Envelope& env, const IndexCreditDefaultSwapData& swap, const BasketData& basket)
        : Trade("IndexCreditDefaultSwap", env), swap_(swap), basket_(basket) {}

    virtual void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    QuantLib::Real notional() const override;

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    const IndexCreditDefaultSwapData& swap() const { return swap_; }

    CreditPortfolioSensitivityDecomposition sensitivityDecomposition() const { return sensitivityDecomposition_; }
    
    const std::map<std::string, QuantLib::Real>& constituents() const { return constituents_; };

    const std::map<std::string,boost::any>& additionalData() const override;

private:
    IndexCreditDefaultSwapData swap_;
    BasketData basket_;
    //! map of all the constituents to notionals
    std::map<std::string, QuantLib::Real> constituents_;
    CreditPortfolioSensitivityDecomposition sensitivityDecomposition_;
};

} // namespace data
} // namespace ore
