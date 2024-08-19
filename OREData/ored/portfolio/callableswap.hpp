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

/*! \file ored/portfolio/callableswap.hpp
    \brief Callable Swap data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>

namespace ore {
namespace data {

//! Serializable Swaption
/*!
  \ingroup tradedata
*/
class CallableSwap : public ore::data::Trade {
public:
    //! Default constructor
    CallableSwap() : ore::data::Trade("CallableSwap") {}
    //! Constructor
    CallableSwap(const ore::data::Envelope& env, const ore::data::Swap& swap, const ore::data::Swaption& swaption)
        : Trade("CallableSwap", env), swap_(swap), swaption_(swaption) {}

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;

    //! \name Inspectors
    //@{
    const ore::data::Swap& swap() const { return swap_; }
    const ore::data::Swaption& swaption() const { return swaption_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(ore::data::XMLNode* node) override;
    virtual ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

    const std::map<std::string,boost::any>& additionalData() const override;

private:
    ore::data::Swap swap_;
    ore::data::Swaption swaption_;
};
} // namespace data
} // namespace ore
