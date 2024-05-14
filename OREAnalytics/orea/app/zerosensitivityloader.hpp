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

/*! \file orea/app/zerosensitivityloader.hpp
    \brief Class for structured analytics warnings
    \ingroup app
*/

#pragma once

#include <map>
#include <string>
#include <vector>

namespace ore {
namespace analytics {
class ZeroSensitivityLoader {
public:
    struct ZeroSensitivity {
        std::string riskFactor;
        double delta;
        std::string currency;
        double baseNpv;
        double shiftSize;
    };

    ZeroSensitivityLoader(const std::string& filename, const std::string& idColum = "TradeId",
                          const std::string& riskFactorColumn = "Factor_1", const std::string& deltaColumn = "Delta",
                          const std::string& currencyColumn = "Currency",
                          const std::string& baseNpvColumn = "Base NPV",
                          const std::string& shiftSizeColumn = "ShiftSize_1");

    std::map<std::string, std::vector<ZeroSensitivity>> sensitivities() { return sensitivities_; }

private:
    std::map<std::string, std::vector<ZeroSensitivity>> sensitivities_;
};
} // namespace analytics
} // namespace ore