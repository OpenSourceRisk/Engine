/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file orea/engine/smrc.hpp
    \brief class for SMRC trade data
*/

#pragma once

#include <map>
#include <utility> // pair
#include <vector>

#include <ql/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

#include <ql/time/period.hpp>

#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/nettingsetmanager.hpp>
#include <ored/report/report.hpp>
#include <ored/portfolio/counterpartymanager.hpp>
#include <ored/portfolio/collateralbalance.hpp>

namespace ore {
    namespace analytics {
        using namespace ore::data;
        using namespace QuantLib;
        using namespace std;

        //! Compute standardize market risk capital charge 
        class SMRC {
        public:

            struct TradeData {
                string id;
                string type;
                string nettingSet;
                string asset;
                string id2;
                Real npv;
                Real signedNotional;
                Date maturityDate;
                Real riskWeight;

                TradeData()
                    : id(""), type(""), nettingSet(""), asset(""), id2(""), npv(0), signedNotional(0),
                      maturityDate(Date::minDate()), riskWeight(0){};

                //! Full ctor to allow braced initialisation
                TradeData(const std::string& id, const std::string& type, const std::string& nettingSet,
                          const std::string& asset, const std::string& id2, QuantLib::Real npv, QuantLib::Real signedNotional)
                    : id(id), type(type), nettingSet(nettingSet), asset(asset), id2(id2), npv(npv),
                      signedNotional(signedNotional) {}
            };

            SMRC(const QuantLib::ext::shared_ptr<Portfolio>& portfolio, const QuantLib::ext::shared_ptr<Market>& market,
                 const std::string& baseCcyCode, QuantLib::ext::shared_ptr<ore::data::Report> smrcReportDetail,
                 QuantLib::ext::shared_ptr<ore::data::Report> smrcReportAggr);

            // getters
            const QuantLib::ext::shared_ptr<Portfolio>& portfolio() const { return portfolio_; };
            std::string baseCcyCode() const { return baseCcyCode_; };
            const QuantLib::ext::shared_ptr<Market>& market() const { return market_; };
            vector<TradeData>& tradeData() { return tradeData_; }

            const vector<string> supportedTypes_ = {"FxForward",
                                                    "FxOption",
                                                    "CommodityForward",
                                                    "CommoditySwap",
                                                    "CommodityOption",
                                                    "EquityOption",
                                                    "EquityPosition",
                                                    "EquityOptionPosition",
                                                    "TotalReturnSwap",
                                                    "ContractForDifference",
                                                    "Swap",
                                                    "Bond",
                                                    "ForwardBond",
                                                    "ConvertibleBond",
                                                    "BondOption",
                                                    "ForwardRateAgreement",
                                                    "CapFloor",
                                                    "Swaption",
                                                    "Failed"};

        private:
            QuantLib::ext::shared_ptr<Portfolio> portfolio_;
            QuantLib::ext::shared_ptr<Market> market_;
            std::string baseCcyCode_;
			const vector<string> majorCcys_ = { "USD", "CAD", "EUR", "GBP", "JPY", "CHF" };

            //! Report that results are written to
            QuantLib::ext::shared_ptr<ore::data::Report> smrcReportDetail_;
            QuantLib::ext::shared_ptr<ore::data::Report> smrcReportAggr_;

			//! trade data from which smrcDetail report is written
            vector<TradeData> tradeData_;

			//! trade data from which smrcAggr report is written
			std::map<std::string, QuantLib::Real> FxForwardCcyBuckets_;
			std::map<std::set<string>, QuantLib::Real> FxOptionCcyPairs_;
            std::map<std::string, QuantLib::Real> EquityBuckets_;
            std::map<std::vector<string>, QuantLib::Real> BondBuckets_;
            std::map<std::string, QuantLib::Real> CommodityBuckets_;
            std::map<std::vector<string>, QuantLib::Real> SwapIndexMaturity_;

            // fill TradeData vector with trade-level information
            void tradeDetails();
            Real getLegAverageNotional(const QuantLib::ext::shared_ptr<Trade>& trade, const Size legIdx) const;
            Real getRiskWeight(const QuantLib::ext::shared_ptr<Trade> &trade) const;
            Real getFxRate(const string& ccy) const;
            Real getFxOptionSign(const string& ccyA, const string& ccyB, const string& putCall, const string& longShort) const;
            Real getOptionSign(const string& putCall, const string& longShort) const;
            Real getSwaptionSign(const string& longShort, const bool FloatingPayer) const;
            Real getBondWeight(const string& underlyingISIN, const Date& maturity) const;
            Real getSwapWeight(const Date& maturityDate) const;

            //! write report
            void writeReports();

        };

    } // namespace analytics

} // namespace ore
