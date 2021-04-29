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

/*! \file ored/utilities/iborreplacementconfig.hpp
    \brief ibor replacement configuration
    \ingroup utilities
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>

namespace ore {
namespace data {

class IborReplacementConfig : public XMLSerializable {
public:
    struct ReplacementData {
        string rfrIndex;
        QuantLib::Real spread;
        QuantLib::Date switchDate;
    };

    IborReplacementConfig();

    bool useRfrCurveInTodaysMarket() const;
    bool useRfrCurveInSimulationMarket() const;
    bool enableIborReplacements() const;

    void addIndexReplacementRule(const string& iborIndex, const ReplacementData& replacementData);

    bool isIndexReplaced(const string& iborIndex, const QuantLib::Date& asof) const;
    const ReplacementData& replacementData(const string& iborIndex) const;

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;

private:
    bool useRfrCurveInTodaysMarket_ = true;
    bool useRfrCurveInSimulationMarket_ = true;
    bool enableIborReplacements_ = true;
    std::map<std::string, ReplacementData> replacements_;
};

} // namespace data
} // namespace ore
