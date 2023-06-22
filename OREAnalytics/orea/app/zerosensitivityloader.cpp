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

#include <orea/app/zerosensitivityloader.hpp>
#include <ored/utilities/csvfilereader.hpp>
#include <ored/utilities/parsers.hpp>

namespace ore {
namespace analytics {

ZeroSensitivityLoader::ZeroSensitivityLoader(const std::string& filename, const std::string& idColumn,
                                             const std::string& riskFactorColumn, const std::string& deltaColumn,
                                             const std::string& currencyColumn, const std::string& baseNpvColumn,
                                             const std::string& shiftSizeColumn) {
    ore::data::CSVFileReader reader(filename, true);
    // TODO: Check if all columns in file
    while (reader.next()) {
        QuantLib::Real shiftSize = QuantLib::Null<QuantLib::Real>();
        QuantLib::Real baseNpv = QuantLib::Null<QuantLib::Real>();
        QuantLib::Real delta = QuantLib::Null<QuantLib::Real>();
        std::string id = reader.get(idColumn);
        std::string key = reader.get(riskFactorColumn);
        std::string currency = reader.get(currencyColumn);
        bool validNpv = ore::data::tryParseReal(reader.get(baseNpvColumn), baseNpv);
        bool validDelta = ore::data::tryParseReal(reader.get(deltaColumn), delta);
        bool validShiftSize = ore::data::tryParseReal(reader.get(shiftSizeColumn), shiftSize);
        if (validDelta && validNpv && validShiftSize && !QuantLib::close_enough(delta, 0.0)) {
            sensitivities_[id].push_back({key, delta, currency, baseNpv, shiftSize});
        }
    }
}

} // namespace analytics
} // namespace ore