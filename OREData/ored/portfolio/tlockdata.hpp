/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/tlockdata.hpp
    \brief A class to hold Treasury-Lock data
    \ingroup tradedata
 */

#pragma once

#include <boost/optional.hpp>
#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/bondutils.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

/*! Serializable credit default swap data
    \ingroup tradedata
*/
class TreasuryLockData : public XMLSerializable {
public:
    //! Default constructor
    TreasuryLockData() : empty_(true) {}

    //! Constructor that takes an explicit \p creditCurveId
    TreasuryLockData(bool payer, const BondData& bondData, Real referenceRate, string dayCounter,
                     string terminationDate, int paymentGap, string paymentCalendar)
        : empty_(false), payer_(payer), bondData_(bondData), referenceRate_(referenceRate), dayCounter_(dayCounter),
          terminationDate_(terminationDate), paymentGap_(paymentGap), paymentCalendar_(paymentCalendar) {}

    bool empty() const { return empty_; }
    bool payer() const { return payer_; }
    BondData& bondData() { return bondData_; }
    const BondData& bondData() const { return bondData_; }
    const BondData& originalBondData() const { return originalBondData_; }
    Real referenceRate() const { return referenceRate_; }
    const string& dayCounter() const { return dayCounter_; }
    const string& terminationDate() const { return terminationDate_; }
    int paymentGap() const { return paymentGap_; }
    const string& paymentCalendar() const { return paymentCalendar_; }

    //! XMLSerializable interface
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;

private:
    bool empty_;
    bool payer_;
    BondData originalBondData_, bondData_;
    Real referenceRate_;
    string dayCounter_;
    string terminationDate_;
    int paymentGap_;
    string paymentCalendar_;
};

} // namespace data
} // namespace ore
