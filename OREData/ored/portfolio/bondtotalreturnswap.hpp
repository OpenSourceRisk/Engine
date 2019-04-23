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

#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

class BondTRS : public Trade {
public:
    //! Default constructor
    BondTRS() : Trade("BondTRS"), zeroBond_(false) {}

    //! Constructor for coupon bonds
    BondTRS(Envelope env, string creditCurveId, string securityId, string referenceCurveId, string settlementDays,
            string calendar, string issueDate, LegData& coupons)
        : Trade("BondTRS", env), creditCurveId_(creditCurveId), securityId_(securityId),
          referenceCurveId_(referenceCurveId), settlementDays_(settlementDays), calendar_(calendar),
          issueDate_(issueDate), coupons_(std::vector<LegData>{coupons}), faceAmount_(0), maturityDate_(), currency_(),
          zeroBond_(false) {}

    //! Constructor for coupon bonds with multiple phases (represented as legs)
    BondTRS(Envelope env, string creditCurveId, string securityId, string referenceCurveId, string settlementDays,
            string calendar, string issueDate, const std::vector<LegData>& coupons)
        : Trade("BondTRS", env), creditCurveId_(creditCurveId), securityId_(securityId),
          referenceCurveId_(referenceCurveId), settlementDays_(settlementDays), calendar_(calendar),
          issueDate_(issueDate), coupons_(coupons), faceAmount_(0), maturityDate_(), currency_(), zeroBond_(false) {}

    //! Constructor for zero bonds
    BondTRS(Envelope env, string creditCurveId, string securityId, string referenceCurveId, string settlementDays,
            string calendar, Real faceAmount, string maturityDate, string currency, string issueDate)
        : Trade("BondTRS", env), creditCurveId_(creditCurveId), securityId_(securityId),
          referenceCurveId_(referenceCurveId), settlementDays_(settlementDays), calendar_(calendar),
          issueDate_(issueDate), coupons_(), faceAmount_(faceAmount), maturityDate_(maturityDate), currency_(currency),
          zeroBond_(true) {}

    // Build QuantLib/QuantExt instrument, link pricing engine
    virtual void build(const boost::shared_ptr<EngineFactory>&) override;
    //! Return the fixings that will be requested to price the Bond given the \p settlementDate.
    std::map<std::string, std::set<QuantLib::Date>>
    fixings(const QuantLib::Date& settlementDate = QuantLib::Date()) const override;

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;

    const string& creditCurveId() const { return creditCurveId_; }
    const string& securityId() const { return securityId_; }
    const string& referenceCurveId() const { return referenceCurveId_; }
    const string& settlementDays() const { return settlementDays_; }
    const string& calendar() const { return calendar_; }
    const string& issueDate() const { return issueDate_; }
    const string& derivativeCurveId() const { return derivativeCurveId_; }
    const std::vector<LegData>& coupons() const { return coupons_; }
    const Real& faceAmount() const { return faceAmount_; }
    const string& maturityDate() const { return maturityDate_; }
    const string& currency() const { return currency_; }
    const string& longInBond() const { return longInBond_; }
    const ScheduleData& scheduleData() const { return scheduleData_; }

private:
    string creditCurveId_;
    string securityId_;
    string referenceCurveId_;
    string settlementDays_;
    string calendar_;
    string issueDate_;
    string derivativeCurveId_;
    string adjustmentSpread_;
    string longInBond_;
    std::vector<LegData> coupons_;
    Real faceAmount_;
    string maturityDate_;
    LegData fundingLegData_;
    string currency_;
    bool zeroBond_;
    ScheduleData scheduleData_;

    /*! A bond may consist of multiple legs joined together to create a single leg. This member stores the
       separate legs so that fixings can be retrieved later for legs that have fixings.
   */
    std::vector<QuantLib::Leg> separateLegs_;

    /*! Set of pairs where first element of pair is the ORE index name and the second element of the pair is
        the index of the leg, in separateLegs_, that contains that ORE index.
    */
    std::set<std::pair<std::string, QuantLib::Size>> nameIndexPairs_;
};
} // namespace data
} // namespace ore
