/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file orea/simm/imschedulecalculator.hpp
    \brief Class for calculating SIMM
*/

#pragma once

#include <orea/simm/imscheduleresults.hpp>
#include <orea/simm/crifrecord.hpp>
#include <ored/marketdata/market.hpp>

namespace ore {
namespace analytics {

/*! A class to calculate Schedule IM given a set of aggregated CRIF results
    for one or more portfolios.
*/
class IMScheduleCalculator {

public:
    typedef CrifRecord::ProductClass ProductClass;
    typedef CrifRecord::RiskType RiskType;
    typedef SimmConfiguration::Regulation Regulation;
    typedef SimmConfiguration::SimmSide SimmSide;

    // Container to hold trade-level data (and results)
    struct IMScheduleTradeData {
        // IM Schedule trades are expected in the CRIF record as two rows, with RiskType::Notional and RiskType::PV
        IMScheduleTradeData(const std::string& tradeId, const ore::data::NettingSetDetails& nettingSetDetails, const RiskType& rt,
                            const CrifRecord::ProductClass& pc, const QuantLib::Real& amount,
                            const std::string& amountCcy, const QuantLib::Real& amountUsd,
                            const QuantLib::Date& endDate, const std::string& calculationCcy,
                            const std::string& collectRegulations, const std::string& postRegulations)
            : tradeId(tradeId), nettingSetDetails(nettingSetDetails), productClass(pc), endDate(endDate),
              calculationCcy(calculationCcy), collectRegulations(collectRegulations), postRegulations(postRegulations) {

            if (rt == RiskType::PV) {
                presentValue = amount;
                presentValueUsd = amountUsd;
                presentValueCcy = amountCcy;
            } else {
                notional = amount;
                notionalUsd = amountUsd;
                notionalCcy = amountCcy;
            }
        }

        bool missingPVData() {
            return presentValueCcy.empty()
                || presentValue == QuantLib::Null<QuantLib::Real>()
                || presentValueUsd == QuantLib::Null<QuantLib::Real>();
        }

        bool missingNotionalData() {
            return notionalCcy.empty()
                || notional == QuantLib::Null<QuantLib::Real>()
                || notionalUsd == QuantLib::Null<QuantLib::Real>();
        }

        bool incomplete() {
            return missingPVData() || missingNotionalData();
        }

        std::string tradeId;
        ore::data::NettingSetDetails nettingSetDetails;
        CrifRecord::ProductClass productClass;
        QuantLib::Real notional = QuantLib::Null<QuantLib::Real>();
        std::string notionalCcy;
        QuantLib::Real notionalUsd = QuantLib::Null<QuantLib::Real>();
        QuantLib::Real notionalCalc = QuantLib::Null<QuantLib::Real>();
        QuantLib::Real presentValue = QuantLib::Null<QuantLib::Real>();
        std::string presentValueCcy;
        QuantLib::Real presentValueUsd = QuantLib::Null<QuantLib::Real>();
        QuantLib::Real presentValueCalc = QuantLib::Null<QuantLib::Real>();
        QuantLib::Date endDate;
        QuantLib::Real maturity;

        IMScheduleLabel label;
        std::string labelString;
        QuantLib::Real multiplier;
        QuantLib::Real grossMarginUsd;
        QuantLib::Real grossMarginCalc;
        std::string calculationCcy;
        std::string collectRegulations;
        std::string postRegulations;

        IMScheduleTradeData()
            : tradeId(""), nettingSetDetails(ore::data::NettingSetDetails()), productClass(ProductClass::Empty),
              endDate(QuantLib::Date()), calculationCcy(""), collectRegulations(""), postRegulations("") {}
    };

    //! Construct the IMScheduleCalculator from a container of netted CRIF records.
    IMScheduleCalculator(const Crif& crif, const std::string& calculationCcy = "USD",
                         const QuantLib::ext::shared_ptr<ore::data::Market> market = nullptr,
                         const bool determineWinningRegulations = true, const bool enforceIMRegulations = false,
                         const bool quiet = false,
                         const std::map<SimmSide, std::set<NettingSetDetails>>& hasSEC =
                             std::map<SimmSide, std::set<NettingSetDetails>>(),
                         const std::map<SimmSide, std::set<NettingSetDetails>>& hasCFTC =
                             std::map<SimmSide, std::set<NettingSetDetails>>());

    //! Give back the set of portfolio IDs and trade IDs for which we have IM results
    //const std::set<std::string>& tradeIds() const { return tradeIds_; }
    const std::map<SimmSide, std::set<std::string>> finalTradeIds() const { return finalTradeIds_; }


    //! Return the winning regulation for each portfolioId
    const std::string& winningRegulations(const SimmSide& side,
                                          const ore::data::NettingSetDetails& nettingSetDetails) const;
    const std::map<ore::data::NettingSetDetails, string>& winningRegulations(const SimmSide& side) const;
    const std::map<SimmSide, std::map<ore::data::NettingSetDetails, string>>& winningRegulations() const;

    //! Give back the IM Schedule results container for the given portfolioId and IM side
    const std::map<std::string, IMScheduleResults>& imScheduleSummaryResults(const SimmSide& side, const ore::data::NettingSetDetails& nsd) const;
    const std::map<ore::data::NettingSetDetails, std::map<std::string, IMScheduleResults>>& imScheduleSummaryResults(const SimmSide& side) const;
    const std::map<SimmSide, std::map<ore::data::NettingSetDetails, std::map<std::string, IMScheduleResults>>>& imScheduleSummaryResults() const;

    const std::pair<std::string, IMScheduleResults>& finalImScheduleSummaryResults(const SimmSide& side, const ore::data::NettingSetDetails& nsd) const;
    const std::map<ore::data::NettingSetDetails, std::pair<std::string, IMScheduleResults>>& finalImScheduleSummaryResults(const SimmSide& side) const;
    const std::map<SimmSide, std::map<ore::data::NettingSetDetails, std::pair<std::string, IMScheduleResults>>>&
    finalImScheduleSummaryResults() const {
        return finalImScheduleResults_;
    };

    //! Give back the IM Schedule results container for the given tradeId and IM side
    const std::vector<IMScheduleTradeData>& imScheduleTradeResults(const std::string& tradeId) const;
    const std::map<std::string, std::vector<IMScheduleTradeData>>& imScheduleTradeResults() const;

    const IMScheduleTradeData& finalImScheduleTradeResults(const std::string& tradeId) const;
    const std::map<std::string, IMScheduleTradeData>& finalImScheduleTradeResults() const;

    //! Return the calculator's calculation currency
    const std::string& calculationCurrency() const { return calculationCcy_; }

    static const IMScheduleLabel label(const ProductClass& productClass, const QuantLib::Real& maturity);
    static const std::string labelString(const IMScheduleLabel& label);

    // Populate the finalImScheduleResults_ container using the provided map of winning call/post regulations
    void populateFinalResults(const std::map<SimmSide, std::map<ore::data::NettingSetDetails, std::string>>& winningRegulations);

private:
    //! The net sensitivities used in the calculation
    ore::analytics::Crif crif_;

    //! The SIMM calculation currency i.e. the currency of the SIMM results
    std::string calculationCcy_;

    //! Market data for FX rates to use for converting amounts to USD
    QuantLib::ext::shared_ptr<ore::data::Market> market_;

    //! If true, no logging is written out
    bool quiet_;

    std::map<SimmSide, std::set<NettingSetDetails>> hasSEC_, hasCFTC_;

    //! For each netting set, whether all CRIF records' collect regulations are empty
    std::map<ore::data::NettingSetDetails, bool> collectRegsIsEmpty_;

    //! For each netting set, whether all CRIF records' post regulations are empty
    std::map<ore::data::NettingSetDetails, bool> postRegsIsEmpty_;

    //! Containers, one for call and post, with an IMScheduleResults object for each regulation under each portfolio ID
    //       side,              netting set details,                   regulation
    std::map<SimmSide, std::map<ore::data::NettingSetDetails, std::map<std::string, IMScheduleResults>>> imScheduleResults_;

    //! Containers, one for call and post, with an IMScheduleResults object for each portfolio ID
    //       side,              netting set details,                   regulation
    std::map<SimmSide, std::map<ore::data::NettingSetDetails, std::pair<std::string, IMScheduleResults>>> finalImScheduleResults_;

    //! Container for keeping track of what trade IDs belong to each regulation
    //       side,              netting set details,                   regulation
    std::map<SimmSide, std::map<ore::data::NettingSetDetails, std::map<std::string, set<string>>>> tradeIds_;

    std::map<SimmSide, set<string>> finalTradeIds_;

    //! Regulation with highest IM for each given netting set
    //       side,              netting set details,          regulation
    std::map<SimmSide, std::map<ore::data::NettingSetDetails, std::string>> winningRegulations_;

    //! Container for trade data collected from CRIF report
    //!      trade ID     trade data
    std::map<std::string, std::vector<IMScheduleTradeData>> finalTradeData_;

    //! Container for trade data, taking into account regulations applicable to each netting set
    //       side,              netting set details,                   regulation,           trade ID
    std::map<SimmSide, std::map<ore::data::NettingSetDetails, std::map<std::string, std::map<std::string, IMScheduleTradeData>>>> nettingSetRegTradeData_;

    std::map<IMScheduleLabel, QuantLib::Real> multiplierMap_ = std::map<IMScheduleLabel, QuantLib::Real>({
        // clang-format off
        {IMScheduleLabel::Credit2,      0.02},
        {IMScheduleLabel::Credit5,      0.05},
        {IMScheduleLabel::Credit100,    0.10},
        {IMScheduleLabel::Commodity,    0.15},
        {IMScheduleLabel::Equity,       0.15},
        {IMScheduleLabel::FX,           0.06},
        {IMScheduleLabel::Rates2,       0.01},
        {IMScheduleLabel::Rates5,       0.02},
        {IMScheduleLabel::Rates100,     0.04},
        {IMScheduleLabel::Other,        0.15},
        // clang-format on
    });

    QuantLib::Real multiplier(const IMScheduleLabel& label) { return multiplierMap_[label]; }

    //! Collect trade data as defined by the CRIF records
    void collectTradeData(const CrifRecord& cr, const bool enforceIMRegulations);

    /*! Populate the results structure with the higher level results after the IMs have been
        calculated at the (product class, maturity) level for each portfolio
    */
    void populateResults(const ore::data::NettingSetDetails& nsd, const string& regulation, const SimmSide& side);

    /*! Populate final (i.e. winning regulators') using own list of winning regulators, which were determined
        solely by the IMSchedule results (i.e. not including any external SIMM results)
    */
    void populateFinalResults();

    //! Add a margin result to either call or post results container depending on the SimmSide parameter
    void add(const SimmSide& side, const ore::data::NettingSetDetails& nsd, const std::string& regulation,
             const CrifRecord::ProductClass& pc, const std::string& ccy, const QuantLib::Real& grossIM,
             const QuantLib::Real& grossRC = QuantLib::Null<QuantLib::Real>(),
             const QuantLib::Real& netRC = QuantLib::Null<QuantLib::Real>(),
             const QuantLib::Real& ngr = QuantLib::Null<QuantLib::Real>(),
             const QuantLib::Real& scheduleIM = QuantLib::Null<QuantLib::Real>());

};

} // namespace analytics
} // namespace ore
