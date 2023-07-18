/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <ored/portfolio/commodityspreadoption.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ql/time/schedule.hpp>

namespace ore::data {

class CommoditySpreadOptionStrip : public ore::data::Trade {
public:
    CommoditySpreadOptionStrip()
        : ore::data::Trade("CommoditySpreadOptionStrip"), tenor_(1 * QuantLib::Days), bdc_(QuantLib::Unadjusted),
          termBdc_(QuantLib::Unadjusted), rule_(QuantLib::DateGeneration::Backward), cal_(QuantLib::NullCalendar()) {}

    //! Implement the build method
    void build(const boost::shared_ptr<ore::data::EngineFactory>& engineFactory) override;

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;
    //@}

    //! \name Inspectors
    //@{

    //@}

private:
    CommoditySpreadOptionData csoData_;
    QuantLib::Period tenor_;
    QuantLib::BusinessDayConvention bdc_;
    QuantLib::BusinessDayConvention termBdc_;
    QuantLib::DateGeneration::Rule rule_;
    Calendar cal_;
    ScheduleData scheduleData_;
    PremiumData premiumData_;
};
}
