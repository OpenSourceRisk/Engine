/*
  Copyright (C) 2023 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file ored/utilities/currencyhedgedequityindexdecomposition.hpp
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
        const QuantLib::ext::shared_ptr<ore::data::CurrencyHedgedEquityIndexReferenceDatum>& indexRefData,
        const QuantLib::ext::shared_ptr<ore::data::EquityIndexReferenceDatum> underlyingRefData,
        const std::string& indexCurrency, const std::string& underlyingIndexCurrency, const std::string& fxIndexName,
        const std::map<std::string, std::pair<double, std::string>>& currencyWeightsAndFxIndexNames)
        : name_(indexName), indexRefData_(indexRefData), underlyingRefData_(underlyingRefData),
          indexCurrency_(indexCurrency), underlyingIndexCurrency_(underlyingIndexCurrency), fxIndexName_(fxIndexName),
          currencyWeightsAndFxIndexNames_(currencyWeightsAndFxIndexNames) {
        QL_REQUIRE(indexRefData_, "CurrencyHedgedDecomposition requires a valid indexRefData");
        QL_REQUIRE(underlyingRefData_, "CurrencyHedgedDecomposition requires a valid underlyingRefData");
        QL_REQUIRE(!indexCurrency_.empty(), "CurrencyHedgedDecomposition requires the currency of the index");
        QL_REQUIRE(!underlyingIndexCurrency_.empty(), "CurrencyHedgedDecomposition requires the currency of the underlying index");
        QL_REQUIRE(!fxIndexName_.empty(),
                   "CurrencyHedgedDecomposition requires the FXIndex name to convert underlyingIndexCurrency to IndexCurrency");
    }

    const std::string& indexName() const { return name_; }

    const std::string& underlyingIndexName() const { return underlyingRefData_->id(); }

    const std::string& indexCurrency() const { return indexCurrency_; }

    const std::string& underlyingIndexCurrency() const { return underlyingIndexCurrency_; }

    const std::string& fxIndexName() const { return fxIndexName_; }

    bool isValid() const { return indexRefData_ && underlyingRefData_ && !currencyWeightsAndFxIndexNames_.empty(); }

    QuantLib::Date referenceDate(const QuantLib::Date& asof) const;

    QuantLib::Date rebalancingDate(const QuantLib::Date& asof) const;

    static QuantLib::Date referenceDate(const QuantLib::ext::shared_ptr<CurrencyHedgedEquityIndexReferenceDatum>& refData,
                                        const QuantLib::Date& asof);

    static QuantLib::Date rebalancingDate(const QuantLib::ext::shared_ptr<CurrencyHedgedEquityIndexReferenceDatum>& refData,
                                          const QuantLib::Date& asof);

    const std::map<std::string, std::pair<double, std::string>>& currencyWeightsAndFxIndexNames() const {
        return currencyWeightsAndFxIndexNames_;
    }

    std::map<std::string, double> fxSpotRiskFromForwards(
        const double quantity, const QuantLib::Date& asof,
        const QuantLib::ext::shared_ptr<ore::data::Market>& todaysMarket, const double shiftsize) const;

    double unhedgedSpotExposure(double hedgedExposure, const double quantity, const QuantLib::Date& asof,
                         const QuantLib::ext::shared_ptr<ore::data::Market>& todaysMarket) const;

    QuantLib::ext::shared_ptr<ore::data::EquityIndexReferenceDatum> underlyingRefData() const { return underlyingRefData_; }

    QuantLib::ext::shared_ptr<ore::data::CurrencyHedgedEquityIndexReferenceDatum> indexRefData() const { return indexRefData_; }

    void
    addAdditionalFixingsForEquityIndexDecomposition(const QuantLib::Date& asof,
                                                    std::map<std::string, RequiredFixings::FixingDates>& fixings) const;

private:
    std::string name_;
    QuantLib::ext::shared_ptr<ore::data::CurrencyHedgedEquityIndexReferenceDatum> indexRefData_;
    QuantLib::ext::shared_ptr<ore::data::EquityIndexReferenceDatum> underlyingRefData_;
    std::string indexCurrency_;
    std::string underlyingIndexCurrency_;
    std::string fxIndexName_;
    std::map<std::string, std::pair<double, std::string>> currencyWeightsAndFxIndexNames_;
};

QuantLib::ext::shared_ptr<CurrencyHedgedEquityIndexDecomposition>
loadCurrencyHedgedIndexDecomposition(const std::string& name,
                                     const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& refDataMgr,
                                     const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs);

} // namespace data
} // namespace ore
