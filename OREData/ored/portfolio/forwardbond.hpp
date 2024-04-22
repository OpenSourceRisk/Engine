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
    ForwardBond(Envelope env, const BondData& bondData, string fwdMaturityDate, string fwdSettlementDate,
                string settlement, string amount, string lockRate, string lockRateDayCounter, string settlementDirty,
                string compensationPayment, string compensationPaymentDate, string longInForward, string dv01 = string())
        : Trade("ForwardBond", env), originalBondData_(bondData), bondData_(bondData),
          fwdMaturityDate_(fwdMaturityDate), fwdSettlementDate_(fwdSettlementDate), settlement_(settlement),
          amount_(amount), lockRate_(lockRate), lockRateDayCounter_(lockRateDayCounter),
          settlementDirty_(settlementDirty), compensationPayment_(compensationPayment),
          compensationPaymentDate_(compensationPaymentDate), longInForward_(longInForward), dv01_(dv01) {}

    virtual void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;

    //! Add underlying Bond names
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    //! inspectors
    const BondData& bondData() const { return bondData_; }

    const string& fwdMaturityDate() const { return fwdMaturityDate_; }
    const string& fwdSettlementDate() const { return fwdSettlementDate_; }
    const string& settlement() const { return settlement_; }
    const string& amount() const { return amount_; }
    const string& lockRate() const { return lockRate_; }
    const string& lockRateDayCounter() const { return lockRateDayCounter_; }
    const string& settlementDirty() const { return settlementDirty_; }
    const string& compensationPayment() const { return compensationPayment_; }
    const string& compensationPaymentDate() const { return compensationPaymentDate_; }
    const string& longInForward() const { return longInForward_; }
    const string& dv01() const { return dv01_; }

protected:
    BondData originalBondData_, bondData_;
    string currency_;

    string fwdMaturityDate_;
    string fwdSettlementDate_;
    string settlement_;
    string amount_;
    string lockRate_;
    string lockRateDayCounter_;
    string settlementDirty_;
    string compensationPayment_;
    string compensationPaymentDate_;
    string longInForward_;
    string dv01_;
};
} // namespace data
} // namespace ore
