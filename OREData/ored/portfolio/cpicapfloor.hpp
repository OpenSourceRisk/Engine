/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file portfolio/cpicapfloor.hpp
    \brief CPI cap, floor or collar trade data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

//! Serializable CPI cap or floor as wrapper of QuantLib::CPICapFloor
/*! \ingroup tradedata
 */
class CPICapFloor : public Trade {
public:
    CPICapFloor() : Trade("CPICapFloor") {}
    CPICapFloor(const Envelope& env, const string& longShort, const string& capFloor, const std::string& currency,
                double nominal, const std::string& startDate, double baseCPI, const std::string& maturityDate,
                const std::string& fixCalendar, const std::string& fixConvention, const std::string& payCalendar,
                const std::string& payConvention, double strike, const std::string& index,
                const std::string observationLag)
        : Trade("CPICapFloor", env), longShort_(longShort), capFloor_(capFloor), currency_(currency), nominal_(nominal),
          startDate_(startDate), baseCPI_(baseCPI), maturityDate_(maturityDate), fixCalendar_(fixCalendar),
          fixConvention_(fixConvention), payCalendar_(payCalendar), payConvention_(payConvention), strike_(strike),
          index_(index), observationLag_(observationLag) {}

    virtual void build(const boost::shared_ptr<EngineFactory>&);

    //! Inspectors
    //@{
    const std::string& longShort() const { return longShort_; }
    const std::string& capFloor() const { return capFloor_; }
    const std::string& currency() const { return currency_; }
    double nominal() const { return nominal_; }
    const std::string& startDate() const { return startDate_; }
    double baseCPI() const { return baseCPI_; }
    const std::string& maturityDate() const { return maturityDate_; }
    const std::string& fixCalendar() const { return fixCalendar_; }
    const std::string& fixConvention() const { return fixConvention_; }
    const std::string& payCalendar() const { return payCalendar_; }
    const std::string& payConvention() const { return payConvention_; }
    double strike() const { return strike_; }
    const std::string& index() const { return index_; }
    const std::string& observationLag() const { return observationLag_; }
    //@}

    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);

private:
    std::string longShort_;
    std::string capFloor_;
    std::string currency_;
    double nominal_;
    std::string startDate_;
    double baseCPI_;
    std::string maturityDate_;
    std::string fixCalendar_;
    std::string fixConvention_;
    std::string payCalendar_;
    std::string payConvention_;
    double strike_;
    std::string index_;
    std::string observationLag_;
};
} // namespace data
} // namespace ore
