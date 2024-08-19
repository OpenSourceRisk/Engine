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

/*! \file ored/portfolio/bond.hpp
 \brief Bond trade data model and serialization
 \ingroup tradedata
 */
#pragma once

#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/referencedata.hpp>

#include <qle/indexes/bondindex.hpp>

namespace ore {
namespace data {

class BondTRS : public Trade {
public:
    //! Default Constructor
    BondTRS() : Trade("BondTRS") {}

    //! Constructor for coupon bonds
    BondTRS(Envelope env, const BondData& bondData)
        : Trade("BondTRS", env), originalBondData_(bondData), bondData_(bondData) {}

    virtual void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;

    //! Add underlying Bond names
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    const BondData& bondData() const { return bondData_; }

    const ScheduleData& scheduleData() const { return scheduleData_; }
    const LegData& fundingLegData() const { return fundingLegData_; }
    const bool payTotalReturnLeg() const { return payTotalReturnLeg_; }
    const Real initialPrice() const { return initialPrice_; }
    const bool useDirtyPrices() const { return useDirtyPrices_; }
    const std::string& observationLag() const { return observationLag_; }
    const std::string& observationConvention() const { return observationConvention_; }
    const std::string& observationCalendar() const { return observationCalendar_; }
    const std::string& paymentLag() const { return paymentLag_; }
    const std::string& paymentConvention() const { return paymentConvention_; }
    const std::string& paymentCalendar() const { return paymentCalendar_; }
    const std::vector<std::string>& paymentDates() { return paymentDates_; }

private:
    // underlying bond data
    BondData originalBondData_, bondData_;

    // total return data
    ScheduleData scheduleData_;
    LegData fundingLegData_;
    bool payTotalReturnLeg_ = false;
    Real initialPrice_ = Null<Real>();
    bool useDirtyPrices_ = true;
    bool payBondCashFlowsImmediately_ = false;
    std::string observationLag_;
    std::string observationConvention_;
    std::string observationCalendar_;
    std::string paymentLag_;
    std::string paymentConvention_;
    std::string paymentCalendar_;
    std::vector<std::string> paymentDates_;

    // optional fx terms for composite bond trs
    string fxIndex_ = "";
};
} // namespace data
} // namespace ore
