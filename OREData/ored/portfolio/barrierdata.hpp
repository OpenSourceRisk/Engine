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

/*! \file ored/portfolio/optiondata.hpp
    \brief trade option data model and serialization
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/schedule.hpp>
#include <ored/portfolio/tradebarrier.hpp>

namespace ore {
namespace data {

//! Serializable obejct holding barrier data
/*!
  \ingroup tradedata
*/
class BarrierData : public ore::data::XMLSerializable {
public:
    //! Default constructor
    BarrierData() : initialized_(false), rebate_(0.0) {}
    //! Constructor
    BarrierData(std::string barrierType, std::vector<double> levels, double rebate,
                std::vector<ore::data::TradeBarrier> tradeBarriers, std::string style = "")
        : initialized_(true), type_(barrierType), levels_(levels), rebate_(rebate), tradeBarriers_(tradeBarriers),
          style_(style) {}

    //! \name Inspectors
    //@{
    const std::string& type() const { return type_; }
    double rebate() const { return rebate_; }
    const std::string& rebateCurrency() const { return rebateCurrency_; }
    const std::string& rebatePayTime() const { return rebatePayTime_; }
    std::vector<ore::data::TradeBarrier> levels() const { return tradeBarriers_; }
    const std::string& style() const { return style_; }
    bool initialized() const { return initialized_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(ore::data::XMLNode* node) override;
    virtual ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

private:
    bool initialized_;
    std::string type_;
    std::vector<double> levels_;
    double rebate_;
    std::vector<ore::data::TradeBarrier> tradeBarriers_;
    std::string rebateCurrency_;
    std::string rebatePayTime_;
    std::string style_;
};
} // namespace data
} // namespace oreplus
