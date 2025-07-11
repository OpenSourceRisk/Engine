/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file portfolio/bondfuture.hpp
 \brief Bond trade data model and serialization
 \ingroup tradedata
 */
#pragma once

#include <ored/portfolio/referencedata.hpp>
#include <ored/portfolio/trade.hpp>
#include <ql/instruments/bond.hpp>

namespace ore {
namespace data {

class BondFuture : public Trade {
public:
    //! Default constructor
    BondFuture() : Trade("BondFuture") {}

    //! Constructor to set up a bondfuture from reference data
    BondFuture(const string& contractName, Real contractNotional, const std::string longShort = "Long",
               Envelope env = Envelope())
        : Trade("BondFuture", env), contractName_(contractName), contractNotional_(contractNotional),
          longShort_(longShort) {}

    virtual void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;

protected:
    void checkData();

private:
    std::string contractName_;
    double contractNotional_;
    std::string longShort_;
};

} // namespace data
} // namespace ore
