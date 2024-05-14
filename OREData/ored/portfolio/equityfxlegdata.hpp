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

/*! \file ored/portfolio/equityfxlegdata.hpp
    \brief leg data for equityfx leg types
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/legdata.hpp>

namespace ore {
namespace data {

using std::string;
using std::vector;

//! Serializable Equity Margin Leg Data
/*!
\ingroup tradedata
*/
class EquityMarginLegData : public ore::data::LegAdditionalData {
public:
    //! Default constructor
    EquityMarginLegData() : ore::data::LegAdditionalData("EquityMargin") {}
    //! Constructor
    EquityMarginLegData(QuantLib::ext::shared_ptr<ore::data::EquityLegData>& equityLegData, const vector<double>& rates, 
        const vector<string>& rateDates = vector<string>(), const double& initialMarginFactor = QuantExt::Null<double>(),
        const double& multiplier = QuantExt::Null<double>())
        : ore::data::LegAdditionalData("EquityMargin"), equityLegData_(equityLegData), rates_(rates), rateDates_(rateDates), 
            initialMarginFactor_(initialMarginFactor), multiplier_(multiplier) {}

    //! \name Inspectors
    //@{
    const QuantLib::ext::shared_ptr<ore::data::EquityLegData> equityLegData() { return equityLegData_; } 
    const vector<double>& rates() const { return rates_; }
    const vector<string>& rateDates() const { return rateDates_; }
    const double& initialMarginFactor() const { return initialMarginFactor_; }
    const double& multiplier() const { return multiplier_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(ore::data::XMLNode* node) override;
    virtual ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}
private:
   
    QuantLib::ext::shared_ptr<ore::data::EquityLegData> equityLegData_;
    vector<double> rates_;
    vector<string> rateDates_;
    double initialMarginFactor_;
    double multiplier_;

};

//! \name Utilities for building QuantLib Legs
//@{
QuantExt::Leg makeEquityMarginLeg(const ore::data::LegData& data,
                                  const QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>& equityCurve,
                                  const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fxIndex = nullptr,
                                  const QuantLib::Date& openEndDateReplacement = QuantLib::Null<QuantLib::Date>());
//@}

} // namespace data
} // namespace ore
