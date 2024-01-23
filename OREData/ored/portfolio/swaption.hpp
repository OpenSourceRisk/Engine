/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file portfolio/swaption.hpp
    \brief Swaption data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/trade.hpp>

#include <ql/instruments/nonstandardswap.hpp>

namespace ore {
namespace data {

//! Serializable Swaption
/*!
  \ingroup tradedata
*/
class Swaption : public Trade {
public:
    Swaption() : Trade("Swaption") {}
    Swaption(const Envelope& env, const OptionData& optionData, const vector<LegData>& legData)
        : Trade("Swaption", env), optionData_(optionData), legData_(legData) {}

    void build(const boost::shared_ptr<EngineFactory>&) override;

    //! \name Inspectors
    //@{
    const OptionData& optionData() { return optionData_; }
    const vector<LegData>& legData() { return legData_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}

    QuantLib::Real notional() const override;
    const std::map<std::string, boost::any>& additionalData() const override;
    bool hasCashflows() const override { return false; }

    bool isExercised() const;

private:
    OptionData optionData_;
    vector<LegData> legData_;

    //! build European Vanilla Swaption
    void buildEuropean(const boost::shared_ptr<EngineFactory>&);
    boost::shared_ptr<VanillaSwap> buildVanillaSwap(const boost::shared_ptr<EngineFactory>&);

    //! build all other types of Swaptions
    void buildBermudan(const boost::shared_ptr<EngineFactory>&);

    //! build underlying swaps for exposure simulation
    std::vector<boost::shared_ptr<Instrument>> buildUnderlyingSwaps(const boost::shared_ptr<PricingEngine>&,
                                                                    const std::vector<Date>&);

    boost::shared_ptr<ore::data::Swap> underlying_;
    boost::shared_ptr<ExerciseBuilder> exerciseBuilder_;
    Position::Type positionType_;
    Exercise::Type exerciseType_;
    Settlement::Type settlementType_;
    Settlement::Method settlementMethod_;
};
} // namespace data
} // namespace ore
