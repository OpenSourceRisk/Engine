/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <orea/engine/pnlexplainreport.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <ored/report/inmemoryreport.hpp>

namespace ore {
namespace analytics {

void populateResults(QuantLib::ext::shared_ptr<MarketRiskGroup> mrg, PnlExplainReport::PnlExplainResults& result,
                     QuantLib::Real deltaPnl, QuantLib::Real gammaPnl, QuantLib::Real pnl, std::string riskFactor = "") {
        
    if (riskFactor != "")
        result.riskFactor = riskFactor; 
    
     if (mrg->riskClass() == MarketRiskConfiguration::RiskClass::All) {
        if (mrg->riskType() == MarketRiskConfiguration::RiskType::DeltaGamma) {
            result.delta = deltaPnl;
            result.gamma = gammaPnl;
        } else if (mrg->riskType() == MarketRiskConfiguration::RiskType::Vega)
            result.vega = pnl;
        else if (mrg->riskType() == MarketRiskConfiguration::RiskType::All)
            result.pnl = pnl;
    }
    if (mrg->riskClass() == MarketRiskConfiguration::RiskClass::InterestRate) {
        if (mrg->riskType() == MarketRiskConfiguration::RiskType::DeltaGamma) {
            result.irDelta = deltaPnl;
            result.irGamma = gammaPnl;
        } else if (mrg->riskType() == MarketRiskConfiguration::RiskType::Vega)
            result.irVega = pnl;
    }
    if (mrg->riskClass() == MarketRiskConfiguration::RiskClass::Equity) {
        if (mrg->riskType() == MarketRiskConfiguration::RiskType::DeltaGamma) {
            result.eqDelta = deltaPnl;
            result.eqGamma = gammaPnl;
        } else if (mrg->riskType() == MarketRiskConfiguration::RiskType::Vega)
            result.eqVega = pnl;
    }
    if (mrg->riskClass() == MarketRiskConfiguration::RiskClass::FX) {
        if (mrg->riskType() == MarketRiskConfiguration::RiskType::DeltaGamma) {
            result.fxDelta = deltaPnl;
            result.fxGamma = gammaPnl;
        } else if (mrg->riskType() == MarketRiskConfiguration::RiskType::Vega)
            result.fxVega = pnl;
    }
    if (mrg->riskClass() == MarketRiskConfiguration::RiskClass::Inflation) {
        if (mrg->riskType() == MarketRiskConfiguration::RiskType::DeltaGamma) {
            result.infDelta = deltaPnl;
            result.infGamma = gammaPnl;
        } else if (mrg->riskType() == MarketRiskConfiguration::RiskType::Vega)
            result.infVega = pnl;
    }
    if (mrg->riskClass() == MarketRiskConfiguration::RiskClass::Credit) {
        if (mrg->riskType() == MarketRiskConfiguration::RiskType::DeltaGamma) {
            result.creditDelta = deltaPnl;
            result.creditGamma = gammaPnl;
        } else if (mrg->riskType() == MarketRiskConfiguration::RiskType::Vega)
            result.creditVega = pnl;
    }
    if (mrg->riskClass() == MarketRiskConfiguration::RiskClass::Commodity) {
        if (mrg->riskType() == MarketRiskConfiguration::RiskType::DeltaGamma) {
            result.comDelta = deltaPnl;
            result.comGamma = gammaPnl;
        } else if (mrg->riskType() == MarketRiskConfiguration::RiskType::Vega)
            result.comVega = pnl;
    }
}

void PnlExplainReport::createReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) {
    QL_REQUIRE(reports->reports().size() == 3, "We in total three reports to create PNL Explain report");

    QuantLib::ext::shared_ptr<InMemoryReport> pnlReport =
        QuantLib::ext::dynamic_pointer_cast<ore::data::InMemoryReport>(reports->reports().at(0));  
    QL_REQUIRE(pnlReport, "PNL report must be an InMemoryReport");

    QuantLib::ext::shared_ptr<InMemoryReport> pnlExplainReport =
        QuantLib::ext::dynamic_pointer_cast<ore::data::InMemoryReport>(reports->reports().at(2)); 
    QL_REQUIRE(pnlExplainReport, "PNL report must be an InMemoryReport");

    pnlReportColumnSize_ = pnlReport->columns();
    //copy pnl report colums to pnl explain report
    for (QuantLib::Size i = 0; i < pnlReportColumnSize_; i++) {
        //columns with double has 6 decimal digit
        if (pnlReport->columnType(i).which() == 1)
            pnlExplainReport->addColumn(pnlReport->header(i), pnlReport->columnType(i), 6);
        else
            pnlExplainReport->addColumn(pnlReport->header(i), pnlReport->columnType(i));
    }

    if (runRiskFactorDetail(reports))
        pnlExplainReport->addColumn("RiskFactor", std::string());  
        
    pnlExplainReport->addColumn("ScenarioPnl", double(), 6)
            .addColumn("TotalDelta", double(), 6)
            .addColumn("TotalGamma", double(), 6)
            .addColumn("TotalVega", double(), 6)
            .addColumn("IrDelta", double(), 6)
            .addColumn("IrGamma", double(), 6)
            .addColumn("IrVega", double(), 6)
            .addColumn("EqDelta", double(), 6)
            .addColumn("EqGamma", double(), 6)
            .addColumn("EqVega", double(), 6)
            .addColumn("FxDelta", double(), 6)
            .addColumn("FxGamma", double(), 6)
            .addColumn("FxVega", double(), 6)
            .addColumn("InfDelta", double(), 6)
            .addColumn("InfGamma", double(), 6)
            .addColumn("InfVega", double(), 6)
            .addColumn("CreditDelta", double(), 6)
            .addColumn("CreditGamma", double(), 6)
            .addColumn("CreditVega", double(), 6)
            .addColumn("CommDelta", double(), 6)
            .addColumn("CommGamma", double(), 6)
            .addColumn("CommVega", double(), 6);      
}

void PnlExplainReport::handleSensiResults(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& report,
    const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
    const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup) {
}

void PnlExplainReport::addPnlCalculators(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) {
    pnlCalculators_.push_back(QuantLib::ext::make_shared<PNLCalculator>(period_.get()));
}

void PnlExplainReport::writeReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports,
    const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
    const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup) {  

    std::vector<QuantLib::Real> sensiPnls = pnlCalculators_[0]->pnls();
    std::vector<QuantLib::Real> foSensiPnls = pnlCalculators_[0]->foPnls();
    TradePnLStore tradeSensiPnls = pnlCalculators_[0]->tradePnls();
    TradePnLStore foTradeSensiPnls = pnlCalculators_[0]->foTradePnls();

    QL_REQUIRE(sensiPnls.size() == 1, "PNLExplainReport::writeReports - should have exactly 1 sensi pnl");
    QL_REQUIRE(foSensiPnls.size() == 1, "PNLExplainReport::writeReports - should have exactly 1 fo sensi pnl");
    Real pnl = sensiPnls[0];
    Real deltaPnl = foSensiPnls[0];
    Real gammaPnl = pnl - deltaPnl;

    auto pId = portfolioId(tradeGroup);
    auto it = results_.find(pId);
    if (it == results_.end())
        results_[pId] = PnlExplainResults();

    auto mrg = ext::dynamic_pointer_cast<MarketRiskGroup>(riskGroup);
    QL_REQUIRE(mrg, "Require a group of type MarketRiskGroup");

    populateResults(mrg, results_[pId], deltaPnl, gammaPnl, pnl);

    if (runTradeDetail(reports)) {
        QL_REQUIRE(tradeSensiPnls.size() == 1,
                   "PNLExplainReport::writeReports - should have exactly 1 sensi pnl for each trade");
        QL_REQUIRE(foTradeSensiPnls.size() == 1,
                   "PNLExplainReport::writeReports - should have exactly 1 fo sensi pnl for each trade");
        QL_REQUIRE(tradeSensiPnls[0].size() == tradeIds_.size(),
                   "PNLExplainReport::writeReports - tradeSensiPNLs do not match portfolio size");
        QL_REQUIRE(foTradeSensiPnls[0].size() == tradeIds_.size(),
                   "PNLExplainReport::writeReports - foTradeSensiPNLs do not match portfolio size");

        for (QuantLib::Size i = 0; i < tradeIds_.size(); i++) {  
            auto tId = tradeIds_.at(i);
            Real tpnl = tradeSensiPnls[0].at(i);
            Real tdeltaPnl = foTradeSensiPnls[0].at(i);
            Real tgammaPnl = tpnl - tdeltaPnl;
            auto itt = results_.find(tId);
            if (itt == results_.end())
                results_[tId] = PnlExplainResults();
            
            populateResults(mrg, results_[tId], tdeltaPnl, tgammaPnl, tpnl);
        }
    }

    if (runRiskFactorDetail(reports)) {        
        PNLCalculator::RiskFactorTradePnLStore riskFactorTradeSensiPnls = pnlCalculators_[0]->riskFactorTradePnls();
        PNLCalculator::RiskFactorTradePnLStore riskFactorFoTradeSensiPnls = pnlCalculators_[0]->riskFactorFoTradePnls();
        QL_REQUIRE(riskFactorTradeSensiPnls.size() == 1,
                   "PNLExplainReport::writeReports - should have exactly 1 sensi pnl for each trade");
        QL_REQUIRE(riskFactorFoTradeSensiPnls.size() == 1,
                   "PNLExplainReport::writeReports - should have exactly 1 fo sensi pnl for each trade");

        for (QuantLib::Size i = 0; i < tradeIds_.size(); i++) {  
            std::map<std::string, std::vector<QuantLib::Real>> mapRiskFacforTradePnl = riskFactorTradeSensiPnls[0];
            std::map<std::string, std::vector<QuantLib::Real>> mapRiskFacforFoTradePnl = riskFactorFoTradeSensiPnls[0];
            auto tId = tradeIds_.at(i);
            for (auto const& x : mapRiskFacforTradePnl){
                Real rfTpnl = mapRiskFacforTradePnl[x.first][i];
                Real rfTdeltaPnl = mapRiskFacforFoTradePnl[x.first][i];//riskFactorFoTradeSensiPnls[x.first][i];
                Real rfTgammaPnl = rfTpnl - rfTdeltaPnl;
                std::string tID_rfID = tId + "_" + x.first;
                auto itt = results_.find(tID_rfID);
                if (itt == results_.end())
                    results_[tID_rfID] = PnlExplainResults();

                populateResults(mrg, results_[tID_rfID], rfTdeltaPnl, rfTgammaPnl, rfTpnl, x.first);
            }
        }
    }
}

bool PnlExplainReport::includeDeltaMargin(
    const QuantLib::ext::shared_ptr<ore::analytics::MarketRiskGroupBase>& riskGroup) const {
    return true;
}
bool PnlExplainReport::includeGammaMargin(
    const QuantLib::ext::shared_ptr<ore::analytics::MarketRiskGroupBase>& riskGroup) const {
    return true;
}

void PnlExplainReport::closeReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) {
    QuantLib::ext::shared_ptr<InMemoryReport> pnlReport =
        QuantLib::ext::dynamic_pointer_cast<ore::data::InMemoryReport>(reports->reports().at(0));  
    QuantLib::ext::shared_ptr<InMemoryReport> sensireport =
        QuantLib::ext::dynamic_pointer_cast<ore::data::InMemoryReport>(reports->reports().at(1)); 
    QuantLib::ext::shared_ptr<InMemoryReport> pnlExplainReport =
        QuantLib::ext::dynamic_pointer_cast<ore::data::InMemoryReport>(reports->reports().at(2)); 

    for (Size j = 0; j <  pnlReport->rows(); j++) {
        string tradeId = boost::get<std::string>(pnlReport->data(0, j));
        PnlExplainResults res;
        const auto& r = results_.find(tradeId);
        if (r == results_.end())
            res = PnlExplainResults();
        else
            res = r->second;            
        
        pnlExplainReport->next();
        for (QuantLib::Size i = 0; i < pnlReportColumnSize_; i++) {
            pnlExplainReport->add(pnlReport->data(i, j));
        }
        pnlExplainReport->jumpToColumn(pnlReportColumnSize_);
        if(requireRiskFactorPnl_){
            // close trade level results
            pnlExplainReport->add(res.riskFactor)
                .add(res.pnl)
                .add(res.delta)
                .add(res.gamma)
                .add(res.vega)
                .add(res.irDelta)
                .add(res.irGamma)
                .add(res.irVega)
                .add(res.eqDelta)
                .add(res.eqGamma)
                .add(res.eqVega)
                .add(res.fxDelta)
                .add(res.fxGamma)
                .add(res.fxVega)
                .add(res.infDelta)
                .add(res.infGamma)
                .add(res.infVega)
                .add(res.creditDelta)
                .add(res.creditGamma)
                .add(res.creditVega)
                .add(res.comDelta)
                .add(res.comGamma)
                .add(res.comVega);
            // close rf level results
            for (Size k = 0; k <  sensireport->rows(); k++) {
                string tradeIdSensi = boost::get<std::string>(sensireport->data(0, k));
                if(tradeId == tradeIdSensi){
                    string rfId = boost::get<std::string>(sensireport->data(2, k));
                    string tradeId_rfId = tradeId + "_" + rfId;
                    PnlExplainResults res;
                    const auto& r = results_.find(tradeId_rfId);
                    if (r == results_.end())
                        res = PnlExplainResults();
                    else
                        res = r->second;
                    pnlExplainReport->next();
                    for (QuantLib::Size i = 0; i < pnlReportColumnSize_; i++) {
                        pnlExplainReport->add(pnlReport->data(i, j));
                    }
                    pnlExplainReport->jumpToColumn(pnlReportColumnSize_);
                    pnlExplainReport->add(res.riskFactor)
                        .add(res.pnl)
                        .add(res.delta)
                        .add(res.gamma)
                        .add(res.vega)
                        .add(res.irDelta)
                        .add(res.irGamma)
                        .add(res.irVega)
                        .add(res.eqDelta)
                        .add(res.eqGamma)
                        .add(res.eqVega)
                        .add(res.fxDelta)
                        .add(res.fxGamma)
                        .add(res.fxVega)
                        .add(res.infDelta)
                        .add(res.infGamma)
                        .add(res.infVega)
                        .add(res.creditDelta)
                        .add(res.creditGamma)
                        .add(res.creditVega)
                        .add(res.comDelta)
                        .add(res.comGamma)
                        .add(res.comVega);
                }
            }
        }
        else {
            // close trade level results
            pnlExplainReport->add(res.pnl)
                .add(res.delta)
                .add(res.gamma)
                .add(res.vega)
                .add(res.irDelta)
                .add(res.irGamma)
                .add(res.irVega)
                .add(res.eqDelta)
                .add(res.eqGamma)
                .add(res.eqVega)
                .add(res.fxDelta)
                .add(res.fxGamma)
                .add(res.fxVega)
                .add(res.infDelta)
                .add(res.infGamma)
                .add(res.infVega)
                .add(res.creditDelta)
                .add(res.creditGamma)
                .add(res.creditVega)
                .add(res.comDelta)
                .add(res.comGamma)
                .add(res.comVega);
        }
    }

    MarketRiskReport::closeReports(reports);
}

} // namespace analytics
} // namespace ore