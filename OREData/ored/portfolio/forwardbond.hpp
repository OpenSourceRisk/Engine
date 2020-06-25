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

/*! \file portfolio/bond.hpp
 \brief Bond trade data model and serialization
 \ingroup tradedata
 */
#pragma once

#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

class ForwardBond : public Trade {
public:
    //! Default constructor
    ForwardBond() : Trade("ForwardBond") {}

    //! Constructor taking an envelope and bond data
    ForwardBond(Envelope env, const BondData& bondData, string fwdMaturityDate, string payOff, string longInBond,
                string settlementDirty, string compensationPayment, string compensationPaymentDate)
        : Trade("ForwardBond", env), bondData_(bondData), fwdMaturityDate_(fwdMaturityDate), payOff_(payOff),
          longInBond_(longInBond), settlementDirty_(settlementDirty), compensationPayment_(compensationPayment),
          compensationPaymentDate_(compensationPaymentDate) {}

    virtual void build(const boost::shared_ptr<EngineFactory>&) override;

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;

    //! inspectors
    const BondData bondData() const { return bondData_; }
    // only available after build() was called
    const string& currency() const { return currency_; }
    // FIXE remove this, replace by bondData()->creditCurveId()
    const string& creditCurveId() const { return bondData_.creditCurveId(); }

    const string& fwdMaturityDate() const { return fwdMaturityDate_; }
    const string& payOff() const { return payOff_; }
    const string& longInBond() const { return longInBond_; }
    const string& settlementDirty() const { return settlementDirty_; }
    const string& compensationPayment() const { return compensationPayment_; }
    const string& compensationPaymentDate() const { return compensationPaymentDate_; }

protected:
    BondData bondData_;
    string currency_;

    string fwdMaturityDate_;
    string payOff_;
    string longInBond_;
    string settlementDirty_;
    string compensationPayment_;
    string compensationPaymentDate_;
};
} // namespace data
} // namespace ore
