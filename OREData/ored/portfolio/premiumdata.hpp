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

/*! \file ored/portfolio/premiumdata.hpp
    \brief premium data
    \ingroup tradedata
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>

#include <ql/time/date.hpp>

namespace ore {
namespace data {

//! Serializable object holding premium data
/*!
  \ingroup tradedata
*/
class PremiumData : public XMLSerializable {
public:
    struct PremiumDatum {
        PremiumDatum() {}
        PremiumDatum(double amount, const string& ccy, const QuantLib::Date& payDate)
            : amount(amount), ccy(ccy), payDate(payDate) {}
        double amount = QuantLib::Null<double>();
        string ccy;
        QuantLib::Date payDate;
    };

    PremiumData() {}
    PremiumData(double amount, const string& ccy, const QuantLib::Date& payDate)
        : premiumData_(1, PremiumDatum(amount, ccy, payDate)) {}
    explicit PremiumData(const std::vector<PremiumDatum>& premiumData) : premiumData_(premiumData) {}

    QuantLib::Date latestPremiumDate() const;

    //! \name Inspectors
    //@{
    const std::vector<PremiumDatum>& premiumData() const { return premiumData_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    std::vector<PremiumDatum> premiumData_;
};

} // namespace data
} // namespace ore
