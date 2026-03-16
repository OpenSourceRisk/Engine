/*
  Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/varianceswap.hpp
    \brief variance swap representation
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/builders/varianceswap.hpp>

#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/underlying.hpp>

namespace ore {
namespace data {
using std::string;

class VarSwap : public ore::data::Trade {
public:
    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;

    const string& longShort() { return longShort_; }
    const std::string& name() const { return underlying_->name(); }
    const QuantLib::ext::shared_ptr<ore::data::Underlying>& underlying() const { return underlying_; }
    const string& currency() { return currency_; }
    double strike() { return strike_; }
    //double notional() const override { return notional_; }
    double notional() const override;
    const string& startDate() { return startDate_; }
    const string& endDate() { return endDate_; }
    const string& calendar() { return calendar_; }
    AssetClass assetClassUnderlying() { return assetClassUnderlying_; }
    const string& momentType() { return momentType_; }
    const bool addPastDividends() { return addPastDividends_; }

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(ore::data::XMLDocument& doc) const override;

protected:
    VarSwap(AssetClass assetClassUnderlying)
        : Trade("VarSwap"), assetClassUnderlying_(assetClassUnderlying), oldXml_(false) {}
    VarSwap(ore::data::Envelope& env, string longShort, const QuantLib::ext::shared_ptr<ore::data::Underlying>& underlying,
            string currency, double strike, double notional, string startDate, string endDate,
            AssetClass assetClassUnderlying, string momentType, bool addPastDividends)
        : Trade("VarSwap", env), assetClassUnderlying_(assetClassUnderlying), underlying_(underlying),
          longShort_(longShort), currency_(currency), strike_(strike), notional_(notional), startDate_(startDate),
          endDate_(endDate), momentType_(momentType), addPastDividends_(addPastDividends), oldXml_(false) {
        initIndexName();
    }

    AssetClass assetClassUnderlying_;

protected:
    QuantLib::ext::shared_ptr<ore::data::Underlying> underlying_;

private:
    void initIndexName();
    string longShort_;
    string currency_;
    double strike_;
    double notional_;
    string startDate_;
    string endDate_;
    string calendar_;
    string momentType_;
    bool addPastDividends_;

    // Store some parsed variables needed in the fixings method
    QuantLib::Date start_;
    QuantLib::Calendar cal_;

    /*! The index name. Not sure why the index was not just used in the trade XML.
        This is set to "FX-" + name for FX and left as name for the others for the moment.
    */
    std::string indexName_;

    // for backward compatability
    bool oldXml_;
};

class EqVarSwap : public VarSwap {
public:
    EqVarSwap() : VarSwap(AssetClass::EQ) { tradeType_ = "EquityVarianceSwap"; }
    EqVarSwap(ore::data::Envelope& env, string longShort, const QuantLib::ext::shared_ptr<ore::data::Underlying>& underlying,
              string currency, double strike, double notional, string startDate, string endDate, string momentType,
              bool addPastDividends)
        : VarSwap(env, longShort, underlying, currency, strike, notional, startDate, endDate, AssetClass::EQ,
                  momentType, addPastDividends) {
        Trade::tradeType_ = "EquityVarianceSwap";
    }

    //! Add underlying Equity names
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;
};

class FxVarSwap : public VarSwap {
public:
    FxVarSwap() : VarSwap(AssetClass::FX) { tradeType_ = "FxVarianceSwap"; }
    FxVarSwap(ore::data::Envelope& env, string longShort, const QuantLib::ext::shared_ptr<ore::data::Underlying>& underlying,
              string currency, double strike, double notional, string startDate, string endDate, string momentType,
              bool addPastDividends)
        : VarSwap(env, longShort, underlying, currency, strike, notional, startDate, endDate, AssetClass::FX,
                  momentType, addPastDividends) {
        Trade::tradeType_ = "FxVarianceSwap";
    }
};

class ComVarSwap : public VarSwap {
public:
    ComVarSwap() : VarSwap(AssetClass::COM) { tradeType_ = "CommodityVarianceSwap"; }
    ComVarSwap(ore::data::Envelope& env, string longShort, const QuantLib::ext::shared_ptr<ore::data::Underlying>& underlying,
              string currency, double strike, double notional, string startDate, string endDate, string momentType,
              bool addPastDividends)
        : VarSwap(env, longShort, underlying, currency, strike, notional, startDate, endDate, AssetClass::EQ,
                  momentType, addPastDividends) {
        Trade::tradeType_ = "CommodityVarianceSwap";
    }

    //! Add underlying Equity names
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;
};

} // namespace data
} // namespace ore
