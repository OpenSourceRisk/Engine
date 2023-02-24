/*
  Copyright (C) 2023 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file orepsimm/ored/portfolio/equityindexdecomposition.cpp
    \brief Helper function used for the index decompositon
 */

#include <ored/configuration/curveconfigurations.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ql/time/date.hpp>

#pragma once

namespace ore {
namespace data {



class CurrencyHedgedEquityIndexDecomposition {
public:
    CurrencyHedgedEquityIndexDecomposition(
        const std::string indexName,
        const boost::shared_ptr<ore::data::CurrencyHedgedEquityIndexReferenceDatum>& indexRefData,
        const boost::shared_ptr<ore::data::EquityIndexReferenceDatum> underlyingRefData,
        const std::map<std::string, std::pair<double, std::string>>& currencyWeightsAndFxIndexNames)
        : name_(indexName), indexRefData_(indexRefData), underlyingRefData_(underlyingRefData),
          currencyWeightsAndFxIndexNames_(currencyWeightsAndFxIndexNames) {
        QL_REQUIRE(indexRefData_, "CurrencyHedgedDecomposition requires a valid indexRefData");
        QL_REQUIRE(underlyingRefData_, "CurrencyHedgedDecomposition requires a valid underlyingRefData");
        QL_REQUIRE(currencyWeightsAndFxIndexNames_.size() == 1,
                   "CurrencyHedgedDecomposition supports only single currency hedged equity indexes");
    }

    const std::string& indexName() const { return name_; }

    const std::string& underlyingIndexName() const { return underlyingRefData_->id(); }

    bool isValid() const { return indexRefData_ && underlyingRefData_ && !currencyWeightsAndFxIndexNames_.empty(); }

    QuantLib::Date referenceDate(const QuantLib::Date& asof) const;

    QuantLib::Date rebalancingDate(const QuantLib::Date& asof) const;

    const std::map<std::string, std::pair<double, std::string>>& currencyWeightsAndFxIndexNames() const {
        return currencyWeightsAndFxIndexNames_;
    }

    std::map<std::string, double>
    fxSpotRiskFromForwards(const double quantity, const QuantLib::Date& asof,
                           const boost::shared_ptr<ore::data::Market>& todaysMarket) const;

    double unhedgedDelta(double hedgedDelta, const double quantity, const QuantLib::Date& asof,
                         const boost::shared_ptr<ore::data::Market>& todaysMarket) const;

    boost::shared_ptr<ore::data::EquityIndexReferenceDatum> underlyingRefData() const { return underlyingRefData_; }

    boost::shared_ptr<ore::data::CurrencyHedgedEquityIndexReferenceDatum> indexRefData() const { return indexRefData_; }

    void
    addAdditionalFixingsForEquityIndexDecomposition(const QuantLib::Date& asof,
                                                    std::map<std::string, std::set<QuantLib::Date>>& fixings) const;

private:
    std::string name_;
    boost::shared_ptr<ore::data::CurrencyHedgedEquityIndexReferenceDatum> indexRefData_;
    boost::shared_ptr<ore::data::EquityIndexReferenceDatum> underlyingRefData_;
    std::map<std::string, std::pair<double, std::string>> currencyWeightsAndFxIndexNames_;
};

boost::shared_ptr<CurrencyHedgedEquityIndexDecomposition>
loadCurrencyHedgedIndexDecomposition(const std::string& name,
                                     const boost::shared_ptr<ore::data::ReferenceDataManager>& refDataMgr,
                                     const boost::shared_ptr<ore::data::CurveConfigurations>& curveConfigs);

} // namespace data
} // namespace ore
