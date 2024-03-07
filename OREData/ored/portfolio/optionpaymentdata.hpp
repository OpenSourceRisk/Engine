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

/*! \file ored/portfolio/optionpaymentdata.hpp
    \brief option payment data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/date.hpp>

namespace ore {
namespace data {

/*! Serializable object holding option payment data for cash settled options.
    \ingroup tradedata
*/
class OptionPaymentData : public XMLSerializable {
public:
    //! When we have payment rules, specifies what date the payment is relative to.
    enum class RelativeTo { Expiry, Exercise };

    //! Default constructor
    OptionPaymentData();

    //! Constructor taking an explicit set of payment dates.
    OptionPaymentData(const std::vector<std::string>& dates);

    //! Constructor taking a set of payment rules.
    OptionPaymentData(const std::string& lag, const std::string& calendar, const std::string& convention,
                      const std::string& relativeTo = "Expiry");

    /*! Returns \c true if the OptionPaymentData was constructed using rules and \c false if it was constructed
        using explicit payment dates.
    */
    bool rulesBased() const { return rulesBased_; }

    //! \name Inspectors
    //@{
    const std::vector<QuantLib::Date>& dates() const { return dates_; }
    QuantLib::Natural lag() const { return lag_; }
    const QuantLib::Calendar& calendar() const { return calendar_; }
    QuantLib::BusinessDayConvention convention() const { return convention_; }
    RelativeTo relativeTo() const { return relativeTo_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    std::vector<std::string> strDates_;
    std::string strLag_;
    std::string strCalendar_;
    std::string strConvention_;
    std::string strRelativeTo_;

    bool rulesBased_;
    std::vector<QuantLib::Date> dates_;
    QuantLib::Natural lag_;
    QuantLib::Calendar calendar_;
    QuantLib::BusinessDayConvention convention_;
    RelativeTo relativeTo_;

    //! Initialisation
    void init();

    //! Populate the value of relativeTo_ member from string.
    void populateRelativeTo();
};

//! Print RelativeTo enum values.
std::ostream& operator<<(std::ostream& out, const OptionPaymentData::RelativeTo& relativeTo);

} // namespace data
} // namespace ore
