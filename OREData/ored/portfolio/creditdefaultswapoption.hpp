/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file portfolio/creditdefaultswapoption.hpp
 \brief credit default swap option trade data model and serialization
 \ingroup tradedata
 */

#pragma once

#include <ored/portfolio/creditdefaultswap.hpp>
#include <ored/portfolio/optiondata.hpp>

//! Serializable Credit Default Swap Option
/*!
 \ingroup tradedata
 */
namespace ore {
namespace data {

class CreditDefaultSwapOption : public Trade {
public:
    //! Default constructor
    CreditDefaultSwapOption() : Trade("CreditDefaultSwapOption") {}
    //! Constructor
    CreditDefaultSwapOption(const Envelope& env, const OptionData& option,
             const CreditDefaultSwapData& swap, const string& term)
        : Trade("CreditDefaultSwapOption", env),
          option_(option), swap_(swap), term_(term) {}

    void build(const boost::shared_ptr<EngineFactory>&) override;

    //! \name Inspectors
    //@{
    const OptionData& option() { return option_; }
    const CreditDefaultSwapData& swap() { return swap_; }
    const string& term() { return term_; }
    //@}

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;

private:
    OptionData option_;
    CreditDefaultSwapData swap_;
    string term_;
};

} // namespace data
} // namespace ore
