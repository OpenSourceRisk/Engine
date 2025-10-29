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

/*! \file portfolio/creditdefaultswap.hpp
 \brief Ibor cap, floor or collar trade data model and serialization
 \ingroup tradedata
 */

#pragma once

#include <ored/portfolio/creditdefaultswapdata.hpp>
#include <ored/portfolio/trade.hpp>

//! Serializable Credit Default Swap
/*!
 \ingroup tradedata
 */
namespace ore {
namespace data {

class CreditDefaultSwap : public Trade {
public:
    //! Default constructor
    CreditDefaultSwap() : Trade("CreditDefaultSwap") {}

    //xg! Constructor
    CreditDefaultSwap(const Envelope& env, const CreditDefaultSwapData& swap)
        : Trade("CreditDefaultSwap", env), swap_(swap) {}

    virtual void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    QuantLib::Real notional() const override;

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;

    const CreditDefaultSwapData& swap() const { return swap_; }

    const std::map<std::string,boost::any>& additionalData() const override;

private:
    CreditDefaultSwapData swap_;
};

} // namespace data
} // namespace ore
