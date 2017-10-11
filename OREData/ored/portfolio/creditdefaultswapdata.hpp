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

#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

class CreditDefaultSwapData {
public:
    //! Default constructor
    CreditDefaultSwapData() {}

    //! Constructor
    CreditDefaultSwapData(const string& issuerId, const string& creditCurveId,
                          const LegData& leg, const bool settlesAccrual = true, const bool paysAtDefaultTime = true,
                          const Date& protectionStart = Date(), const Date& upfrontDate = Date(),
                          const Real upfrontFee = Null<Real>())
        : issuerId_(issuerId), creditCurveId_(creditCurveId), leg_(leg),
          settlesAccrual_(settlesAccrual), paysAtDefaultTime_(paysAtDefaultTime), protectionStart_(protectionStart),
          upfrontDate_(upfrontDate), upfrontFee_(upfrontFee) {}

    void fromXML(XMLNode* node);
    XMLNode* toXML(XMLDocument& doc);

    const string& issuerId() const { return issuerId_; }
    const string& creditCurveId() const { return creditCurveId_; }
    const LegData& leg() const { return leg_; }
    bool settlesAccrual() const { return settlesAccrual_; }
    bool paysAtDefaultTime() const { return paysAtDefaultTime_; }
    const Date& protectionStart() const { return protectionStart_; }
    const Date& upfrontDate() const { return upfrontDate_; }
    Real upfrontFee() const { return upfrontFee_; }

private:
    string issuerId_;
    string creditCurveId_;
    LegData leg_;
    bool settlesAccrual_, paysAtDefaultTime_;
    Date protectionStart_, upfrontDate_;
    Real upfrontFee_;
};

} // namespace data
} // namespace ore
