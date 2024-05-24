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

/*! \file ored/portfolio/pairwisevarianceswap.hpp
    \brief pairwise variance swap representation
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/builders/pairwisevarianceswap.hpp>

#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/underlying.hpp>

namespace ore {
namespace data {
using std::string;

class PairwiseVarSwap : public Trade {
public:
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    const string& longShort() { return longShort_; }
    const vector<QuantLib::ext::shared_ptr<Underlying>>& underlyings() const { return underlyings_; }
    const string& name(int idx) const { return underlyings_[idx]->name(); }
    const string& currency() { return currency_; }
    double basketStrike() { return basketStrike_; }
    double basketNotional() { return basketNotional_; }
    using Trade::notional;
    double notional(int idx) { return underlyingNotionals_[idx]; };
    double strike(int idx) { return underlyingStrikes_[idx]; };
    AssetClass& assetClassUnderlyings() { return assetClassUnderlyings_; }

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;

protected:
    PairwiseVarSwap(AssetClass assetClassUnderlyings)
        : Trade("PairwiseVarianceSwap"), assetClassUnderlyings_(assetClassUnderlyings) {}
    PairwiseVarSwap(Envelope& env, string longShort, const vector<QuantLib::ext::shared_ptr<Underlying>>& underlyings,
                    vector<double> underlyingStrikes, vector<double> underlyingNotionals, double basketNotional,
                    double basketStrike, ScheduleData valuationSchedule, string currency, string settlementDate,
                    AssetClass assetClassUnderlyings, ScheduleData laggedValuationSchedule, double payoffLimit = 0.0,
                    double cap = 0.0, double floor = 0.0, int accrualLag = 1)
        : Trade("PairwiseVarianceSwap", env), assetClassUnderlyings_(assetClassUnderlyings), underlyings_(underlyings),
          longShort_(longShort), underlyingStrikes_(underlyingStrikes), underlyingNotionals_(underlyingNotionals),
          basketNotional_(basketNotional), basketStrike_(basketStrike), payoffLimit_(payoffLimit), cap_(cap),
          floor_(floor), accrualLag_(accrualLag), settlementDate_(settlementDate), currency_(currency) {

        for (const auto& u : underlyings_) {
            indexNames_.push_back(assetClassUnderlyings_ == AssetClass::FX ? "FX-" + u->name() : u->name());
        }
    }

    AssetClass assetClassUnderlyings_;

protected:
    vector<QuantLib::ext::shared_ptr<Underlying>> underlyings_;

private:
    string longShort_;
    vector<Real> underlyingStrikes_, underlyingNotionals_;
    Real basketNotional_, basketStrike_, payoffLimit_, cap_, floor_;
    int accrualLag_;
    ScheduleData valuationSchedule_, laggedValuationSchedule_;
    string settlementDate_, currency_;

    /*! The index name. Not sure why the index was not just used in the trade XML.
        This is set to "FX-" + name for FX and left as name for the others for the moment.
    */
    vector<string> indexNames_;
};

class EqPairwiseVarSwap : public PairwiseVarSwap {
public:
    EqPairwiseVarSwap() : PairwiseVarSwap(AssetClass::EQ) { tradeType_ = "EquityPairwiseVarianceSwap"; }
    EqPairwiseVarSwap(ore::data::Envelope& env, string longShort,
                      const vector<QuantLib::ext::shared_ptr<Underlying>>& underlyings, vector<double> underlyingStrikes,
                      vector<double> underlyingNotionals, double basketNotional, double basketStrike,
                      ScheduleData valuationSchedule, string currency, string settlementDate,
                      ScheduleData laggedValuationSchedule, double payoffLimit = 0.0, double cap = 0.0,
                      double floor = 0.0, int accrualLag = 1)
        : PairwiseVarSwap(env, longShort, underlyings, underlyingStrikes, underlyingNotionals, basketNotional,
                          basketStrike, valuationSchedule, currency, settlementDate, AssetClass::EQ,
                          laggedValuationSchedule, payoffLimit, cap, floor, accrualLag) {
        Trade::tradeType_ = "EquityPairwiseVarianceSwap";
    }

    //! Add underlying Equity names
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;
};

class FxPairwiseVarSwap : public PairwiseVarSwap {
public:
    FxPairwiseVarSwap() : PairwiseVarSwap(AssetClass::FX) { tradeType_ = "FxPairwiseVarianceSwap"; }
    FxPairwiseVarSwap(Envelope& env, string longShort, const vector<QuantLib::ext::shared_ptr<Underlying>>& underlyings,
                      vector<double> underlyingStrikes, vector<double> underlyingNotionals, double basketNotional,
                      double basketStrike, ScheduleData valuationSchedule, string currency, string settlementDate,
                      ScheduleData laggedValuationSchedule, double payoffLimit = 0.0, double cap = 0.0,
                      double floor = 0.0, int accrualLag = 1)
        : PairwiseVarSwap(env, longShort, underlyings, underlyingStrikes, underlyingNotionals, basketNotional,
                          basketStrike, valuationSchedule, currency, settlementDate, AssetClass::FX,
                          laggedValuationSchedule, payoffLimit, cap, floor, accrualLag) {
        Trade::tradeType_ = "FxPairwiseVarianceSwap";
    }
};

} // namespace data
} // namespace ore
