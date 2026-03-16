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
#include <ored/marketdata/expiry.hpp>
#include <ql/time/date.hpp>

#include <ql/optional.hpp>

namespace ore {
namespace data {

class ReportConfig : public XMLSerializable {
public:
    ReportConfig() {}
    ReportConfig(const QuantLib::ext::optional<bool>& reportOnDeltaGrid, const QuantLib::ext::optional<bool>& reportOnMoneynessGrid,
                 const QuantLib::ext::optional<bool>& reportOnStrikeGrid, const QuantLib::ext::optional<bool>& reportOnStrikeSpreadGrid,
                 const QuantLib::ext::optional<std::vector<std::string>>& deltas,
                 const QuantLib::ext::optional<std::vector<Real>>& moneyness, const QuantLib::ext::optional<std::vector<Real>>& strikes,
                 const QuantLib::ext::optional<std::vector<Real>>& strikeSpreads,
                 const QuantLib::ext::optional<std::vector<Period>>& expiries,
                 const QuantLib::ext::optional<std::vector<Date>>& pillarDates,
                 const QuantLib::ext::optional<std::vector<Period>>& underlyingTenors,
                 const QuantLib::ext::optional<std::vector<QuantLib::ext::shared_ptr<Expiry>>>& continuationExpiries)
        : reportOnDeltaGrid_(reportOnDeltaGrid), reportOnMoneynessGrid_(reportOnMoneynessGrid),
          reportOnStrikeGrid_(reportOnStrikeGrid), reportOnStrikeSpreadGrid_(reportOnStrikeSpreadGrid), deltas_(deltas),
          moneyness_(moneyness), strikes_(strikes), strikeSpreads_(strikeSpreads), expiries_(expiries),
          pillarDates_(pillarDates), underlyingTenors_(underlyingTenors), continuationExpiries_(continuationExpiries) {}

    const QuantLib::ext::optional<bool> reportOnDeltaGrid() const { return reportOnDeltaGrid_; }
    const QuantLib::ext::optional<bool> reportOnMoneynessGrid() const { return reportOnMoneynessGrid_; }
    const QuantLib::ext::optional<bool> reportOnStrikeGrid() const { return reportOnStrikeGrid_; }
    const QuantLib::ext::optional<bool> reportOnStrikeSpreadGrid() const { return reportOnStrikeSpreadGrid_; }
    const QuantLib::ext::optional<std::vector<std::string>>& deltas() const { return deltas_; }
    const QuantLib::ext::optional<std::vector<Real>>& moneyness() const { return moneyness_; }
    const QuantLib::ext::optional<std::vector<Real>>& strikes() const { return strikes_; }
    const QuantLib::ext::optional<std::vector<Real>>& strikeSpreads() const { return strikeSpreads_; }
    const QuantLib::ext::optional<std::vector<Period>>& expiries() const { return expiries_; }
    const QuantLib::ext::optional<std::vector<Date>>& pillarDates() const { return pillarDates_; }
    const QuantLib::ext::optional<std::vector<Period>>& underlyingTenors() const { return underlyingTenors_; }
    const QuantLib::ext::optional<std::vector<QuantLib::ext::shared_ptr<Expiry>>>& continuationExpiries() const { return continuationExpiries_; }
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

private:
    QuantLib::ext::optional<bool> reportOnDeltaGrid_;
    QuantLib::ext::optional<bool> reportOnMoneynessGrid_;
    QuantLib::ext::optional<bool> reportOnStrikeGrid_;
    QuantLib::ext::optional<bool> reportOnStrikeSpreadGrid_;

    QuantLib::ext::optional<std::vector<std::string>> deltas_;
    QuantLib::ext::optional<std::vector<Real>> moneyness_;
    QuantLib::ext::optional<std::vector<Real>> strikes_;
    QuantLib::ext::optional<std::vector<Real>> strikeSpreads_;
    QuantLib::ext::optional<std::vector<Period>> expiries_;
    QuantLib::ext::optional<std::vector<Date>> pillarDates_;
    QuantLib::ext::optional<std::vector<Period>> underlyingTenors_;
    QuantLib::ext::optional<std::vector<QuantLib::ext::shared_ptr<Expiry>>> continuationExpiries_;
};

ReportConfig effectiveReportConfig(const ReportConfig& globalConfig, const ReportConfig& localConfig);

} // namespace data
} // namespace ore
