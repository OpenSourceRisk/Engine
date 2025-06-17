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
#include <orea/engine/correlationreport.hpp>
#include <ored/utilities/to_string.hpp>
#include <qle/math/deltagammavar.hpp>

namespace ore {
namespace analytics {

std::vector<Real> extractLowerTriangle(const QuantLib::Matrix& corrMatrix) {
    Size n = corrMatrix.rows();
    std::vector<Real> result;
    for (Size col = 0; col < n; col++) {
        for (Size row = col+1; row<n;row++){
            result.push_back(corrMatrix[row][col]);
        }
    }
    return result;
}

void CorrelationReport::writeHeader(const ext::shared_ptr<Report>& report) {

    report->addColumn("RiskFactor1", string())
        .addColumn("Index1", string())
        .addColumn("RiskFactor2", string())
        .addColumn("Index2", string())
        .addColumn("Correlation", Real(), 6);
}

void CorrelationReport::createReports(const ext::shared_ptr<MarketRiskReport::Reports>& reports) {
    int s = reports->reports().size();
    QL_REQUIRE(s >= 1 && s <= 2, "We should only report for CORRELATION report");
    QuantLib::ext::shared_ptr<Report> report = reports->reports().at(0);
    // prepare report
    writeHeader(report);
}

void CorrelationReport::writeReports(const ext::shared_ptr<MarketRiskReport::Reports>& reports,
                             const ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
                             const ext::shared_ptr<TradeGroupBase>& tradeGroup) {

    vector<Real> corrFormatted = extractLowerTriangle(correlationMatrix_);
    int s = reports->reports().size();
    QL_REQUIRE(s >= 1 && s <= 2, "We should only report for CORRELATION report");
    QuantLib::ext::shared_ptr<Report> report = reports->reports().at(0);

    for (int i = 0; i < corrFormatted.size(); i++) {
        report->next().add(std::get<0>(correlationPairs_[i]))
            .add(std::get<1>(correlationPairs_[i]))
            .add(std::get<2>(correlationPairs_[i]))
            .add(std::get<3>(correlationPairs_[i]))
            .add(corrFormatted[i]);
    }
}

} // namespace analytics
} // namespace ore
