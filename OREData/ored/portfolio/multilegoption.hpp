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

/*! \file multilegoption.hpp
    \brief Multileg Option data model
*/

#pragma once

#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

class MultiLegOption : public Trade {
public:
    // default ctor
    MultiLegOption() : Trade("MultiLegOption"), hasOption_(false) {}

    // trade consists only of the underlying
    MultiLegOption(const Envelope& env, const vector<LegData>& underlyingData)
        : Trade("MultiLegOption", env), hasOption_(false), underlyingData_(underlyingData) {}

    // trade is an option to exercise into an underlying
    MultiLegOption(const Envelope& env, const OptionData& optionData, const vector<LegData>& underlyingData)
        : Trade("MultiLegOption", env), optionData_(optionData), hasOption_(true), underlyingData_(underlyingData) {}

    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    const OptionData& option() { return optionData_; }
    const bool& hasOption() { return hasOption_; }
    const vector<LegData>& underlying() { return underlyingData_; }

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

private:
    OptionData optionData_;
    bool hasOption_;
    vector<LegData> underlyingData_;
};

} // namespace data
} // namespace ore
