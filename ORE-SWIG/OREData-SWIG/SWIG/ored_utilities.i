/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#ifndef ored_utilitles_i
#define ored_utilitles_i

%include types.i
%include ored_iborfallbackconfig.i

%shared_ptr(ore::data::DateGrid)

namespace ore {
namespace data {

class DateGrid {
public:
    DateGrid();
    DateGrid(const std::string& grid, const QuantLib::Calendar& gridCalendar = QuantLib::TARGET(),
             const QuantLib::DayCounter& dayCounter = QuantLib::ActualActual(QuantLib::ActualActual::ISDA));
    DateGrid(const std::vector<QuantLib::Period>& tenors, const QuantLib::Calendar& gridCalendar = QuantLib::TARGET(),
             const QuantLib::DayCounter& dayCounter = QuantLib::ActualActual(QuantLib::ActualActual::ISDA));
    DateGrid(const std::vector<QuantLib::Date>& dates, const QuantLib::Calendar& gridCalendar = QuantLib::TARGET(),
             const QuantLib::DayCounter& dayCounter = QuantLib::ActualActual(QuantLib::ActualActual::ISDA));
    QuantLib::Size size() const;
    void addCloseOutDates(const QuantLib::Period& p = QuantLib::Period(2, QuantLib::Weeks));
    const std::vector<QuantLib::Period>& tenors() const;
    const std::vector<QuantLib::Date>& dates() const;
    const std::vector<bool>& isValuationDate() const;
    const std::vector<bool>& isCloseOutDate() const;
    std::vector<QuantLib::Date> valuationDates() const;
    std::vector<QuantLib::Date> closeOutDates() const;
    const QuantLib::Calendar& calendar() const;
    const QuantLib::DayCounter& dayCounter() const;
    const std::vector<QuantLib::Time>& times() const;
    const QuantLib::TimeGrid& timeGrid() const;
    QuantLib::Date closeOutDateFromValuationDate(const QuantLib::Date& d) const;
};

void addMarketObjectDependencies(std::map<std::string, std::map<MarketObject, std::set<std::string>>>* objects,
                                 const ext::shared_ptr<CurveConfigurations>& curveConfigs, const std::string& baseCcy,
                                 const std::string& baseCcyDiscountCurve,
                                 ext::shared_ptr<IborFallbackConfig> iborFallbackConfig =
                                     ext::make_shared<IborFallbackConfig>(IborFallbackConfig::defaultConfig()));

std::string marketObjectToCurveSpec(const MarketObject& mo, const std::string& name, const std::string& baseCcy,
                                    const ext::shared_ptr<CurveConfigurations>& curveConfigs);

std::string currencyToDiscountCurve(const std::string& ccy, const std::string& baseCcy,
                                    const std::string& baseCcyDiscountCurve,
                                    const ext::shared_ptr<CurveConfigurations>& curveConfigs);

std::string swapIndexDiscountCurve(const std::string& ccy, const std::string& baseCcy = std::string(),
                                   const std::string& swapIndexConvId = std::string());

void buildCollateralCurveConfig(const std::string& curveId, const std::string& baseCcy,
                                const std::string& baseCcyDiscountCurve,
                                const ext::shared_ptr<CurveConfigurations>& curveConfigs);

std::set<std::string> getCollateralisedDiscountCcy(const std::string& ccy,
                                                   const QuantLib::ext::shared_ptr<CurveConfigurations>& curveConfigs);

const bool isCollateralCurve(const std::string& id, std::vector<std::string>& tokens);

} // namespace data
} // namespace ore

#endif
