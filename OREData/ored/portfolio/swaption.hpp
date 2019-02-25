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

#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/optiondata.hpp>
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
    //! Default constructor
    Swaption() : Trade("Swaption") {}
    //! Constructor
    Swaption(Envelope& env, OptionData& option, vector<LegData>& swap)
        : Trade("Swaption", env), option_(option), swap_(swap) {}

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const boost::shared_ptr<EngineFactory>&) override;

    //! Return the fixings that will be requested to price the Swaption given the \p settlementDate.
    std::map<std::string, std::set<QuantLib::Date>> fixings(
        const QuantLib::Date& settlementDate = QuantLib::Date()) const override;

    //! \name Inspectors
    //@{
    const OptionData& option() { return option_; }
    const vector<LegData>& swap() { return swap_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}
private:
    OptionData option_;
    vector<LegData> swap_;

    boost::shared_ptr<VanillaSwap> buildVanillaSwap(const boost::shared_ptr<EngineFactory>&,
                                                    const Date& firstExerciseDate = Null<Date>());
    boost::shared_ptr<NonstandardSwap> buildNonStandardSwap(const boost::shared_ptr<EngineFactory>&);
    void buildEuropean(const boost::shared_ptr<EngineFactory>&);
    void buildBermudan(const boost::shared_ptr<EngineFactory>&);
    std::vector<boost::shared_ptr<Instrument>> buildUnderlyingSwaps(const boost::shared_ptr<PricingEngine>&,
                                                                    const boost::shared_ptr<Swap>&,
                                                                    const std::vector<Date>&);

    //! Underlying Exercise object
    boost::shared_ptr<QuantLib::Exercise> exercise_;

    //! Store the name of the underlying swap's floating leg index
    std::string underlyingIndex_;

    //! Store the underlying swap's floating leg
    QuantLib::Leg underlyingLeg_;
};
} // namespace data
} // namespace ore
