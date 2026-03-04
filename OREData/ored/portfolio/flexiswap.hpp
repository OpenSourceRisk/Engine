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

/*! \file ored/portfolio/flexiswap.hpp
    \brief Flexi-Swap data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

//! Serializable Flexi-Swap
/*!
  \ingroup tradedata
*/
class FlexiSwap : public ore::data::Trade {
public:
    FlexiSwap() : Trade("FlexiSwap") {}

    /*! the optionality is described by lower notional bounds */
    FlexiSwap(
        const ore::data::Envelope& env, const std::vector<ore::data::LegData>& underlyingData,
        const std::map<std::string, std::pair<std::vector<double>, std::vector<std::string>>>& lowerNotionalBounds,
        const std::string& optionLongShort)
        : Trade("FlexiSwap", env), underlyingData_(underlyingData), lowerNotionalBounds_(lowerNotionalBounds),
          optionLongShort_(optionLongShort) {}

    /*! the optionality is described by exercise dates / types and values */
    // FlexiSwap(const ore::data::Envelope& env, const std::vector<ore::data::LegData>& swap,
    //           const std::string& noticePeriod, const std::string& noticeCalendar, const std::string& noticeConvention,
    //           const std::vector<std::string>& exerciseDates, const std::vector<std::string>& exerciseTypes,
    //           const std::vector<double>& exerciseValues, const std::string& optionLongShort)
    //     : Trade("FlexiSwap", env), swap_(swap), noticePeriod_(noticePeriod), noticeCalendar_(noticeCalendar),
    //       noticeConvention_(noticeConvention), exerciseDates_(exerciseDates), exerciseTypes_(exerciseTypes),
    //       exerciseValues_(exerciseValues), optionLongShort_(optionLongShort) {
    //     QL_REQUIRE(exerciseDates_.size() == exerciseTypes.size(), "exercise dates (" << exerciseDates_.size()
    //                                                                                  << ") must match exercise types ("
    //                                                                                  << exerciseTypes.size() << ")");
    //     QL_REQUIRE(exerciseDates_.size() == exerciseValues.size(),
    //                "exercise dates (" << exerciseDates_.size() << ") must match exercise values ("
    //                                   << exerciseTypes.size() << ")");
    // }

    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;

    //! \name Inspectors
    //@{
    // const std::vector<ore::data::LegData>& swap() const { return swap_; }
    // optionality described by lower notional bounds
    // const std::vector<double>& lowerNotionalBounds() const { return lowerNotionalBounds_; }
    // const std::vector<std::string>& lowerNotionalBoundsDates() const { return lowerNotionalBoundsDates_; }
    // // optionality described by exercise dates, types, values
    // const std::string& noticePeriod() const { return noticePeriod_; }
    // const std::string& noticeCalendar() const { return noticeCalendar_; }
    // const std::string& noticeConvention() const { return noticeConvention_; }
    // const std::vector<std::string>& exerciseDates() const { return exerciseDates_; }
    // const std::vector<std::string>& exerciseTypes() const { return exerciseTypes_; }
    // const std::vector<double>& exerciseValues() const { return exerciseValues_; }
    // // option long / short flag
    // const std::string& optionLongShort() const { return optionLongShort_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(ore::data::XMLNode* node) override;
    virtual ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}
private:
    std::vector<ore::data::LegData> underlyingData_;
    // optionality given by lower notional boudns
    std::map<std::string, std::pair<std::vector<double>, std::vector<std::string>>> lowerNotionalBounds_;
    // optionality given by exercise dates, types and values
    // std::string noticePeriod_, noticeCalendar_, noticeConvention_;
    // std::vector<std::string> exerciseDates_, exerciseTypes_;
    // std::vector<double> exerciseValues_;
    // long or short option
    std::string optionLongShort_;
};

} // namespace data
} // namespace ore
