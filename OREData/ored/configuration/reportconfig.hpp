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

/*! \file ored/configuration/reportconfig.hpp
    \brief md report and arbitrage check configuration
    \ingroup configuration
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>

#include <boost/optional.hpp>

namespace ore {
namespace data {

class ReportConfig : public XMLSerializable {
public:
    ReportConfig() {}
    ReportConfig(const boost::optional<bool>& reportOnDeltaGrid, const boost::optional<bool>& reportOnMoneynessGrid,
                 const boost::optional<std::vector<std::string>>& deltas,
                 const boost::optional<std::vector<Real>>& moneyness,
                 const boost::optional<std::vector<Period>>& expiries)
        : reportOnDeltaGrid_(reportOnDeltaGrid), reportOnMoneynessGrid_(reportOnMoneynessGrid), deltas_(deltas),
          moneyness_(moneyness), expiries_(expiries) {}

    const boost::optional<bool> reportOnDeltaGrid() const { return reportOnDeltaGrid_; }
    const boost::optional<bool> reportOnMoneynessGrid() const { return reportOnMoneynessGrid_; }
    const boost::optional<std::vector<std::string>>& deltas() const { return deltas_; }
    const boost::optional<std::vector<Real>>& moneyness() const { return moneyness_; }
    const boost::optional<std::vector<Period>>& expiries() const { return expiries_; }

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;

private:
    boost::optional<bool> reportOnDeltaGrid_;
    boost::optional<bool> reportOnMoneynessGrid_;

    boost::optional<std::vector<std::string>> deltas_;
    boost::optional<std::vector<Real>> moneyness_;
    boost::optional<std::vector<Period>> expiries_;
};

ReportConfig effectiveReportConfig(const ReportConfig& globalConfig, const ReportConfig& localConfig);

} // namespace data
} // namespace ore
