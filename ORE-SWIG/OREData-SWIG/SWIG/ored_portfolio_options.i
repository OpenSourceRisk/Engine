/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#ifndef ored_portfolio_options_i
#define ored_portfolio_options_i

%{
using ore::data::PremiumData;
using ore::data::OptionData;
using ore::data::OptionExerciseData;
using ore::data::OptionPaymentData;
using ore::data::TradeStrike;
using ore::data::Underlying;
using ore::data::EquityUnderlying;
using ore::data::TradeMonetary;
using ore::data::TradeBarrier;
using ore::data::BarrierData;
using ore::data::XMLSerializable;
using namespace std;
%}

%shared_ptr(PremiumData)
class PremiumData : public XMLSerializable {
public:
    PremiumData();
    PremiumData(double amount, const string& ccy, const QuantLib::Date& payDate);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(OptionData)
class OptionData : public XMLSerializable {
public:
    OptionData(string longShort, string callPut, string style, bool payoffAtExpiry, vector<string> exerciseDates,
               string settlement = "Cash", string settlementMethod = std::string(), const PremiumData& premiumData = {},
               vector<double> exerciseFees = vector<Real>(), vector<double> exercisePrices = vector<Real>(),
               string noticePeriod = std::string(), string noticeCalendar = std::string(), string noticeConvention = std::string(),
               const vector<string>& exerciseFeeDates = vector<string>(),
               const vector<string>& exerciseFeeTypes = vector<string>(), string exerciseFeeSettlementPeriod = std::string(),
               string exerciseFeeSettlementCalendar = std::string(), string exerciseFeeSettlementConvention = std::string(),
               string payoffType = std::string(), string payoffType2 = std::string(),
               const QuantLib::ext::optional<bool>& automaticExercise = QuantLib::ext::nullopt,
               const QuantLib::ext::optional<OptionExerciseData>& exerciseData = QuantLib::ext::nullopt,
               const QuantLib::ext::optional<OptionPaymentData>& paymentData = QuantLib::ext::nullopt,
               const bool midCouponExercise = false, const std::string& cashSettlementCurrency = std::string(),
               const std::string& cashSettlementFxIndex = std::string(),
               const std::string& cashSettlementFixingDate = std::string());
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(TradeStrike)
class TradeStrike {
public:
    enum class Type {
        Price,
        Yield
    };
    TradeStrike(Type type, const QuantLib::Real& value);
    TradeStrike(const QuantLib::Real& value, const std::string& currency);
};

%shared_ptr(Underlying)
class Underlying : public XMLSerializable {
public:
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(EquityUnderlying)
class EquityUnderlying : public Underlying {
public:
    explicit EquityUnderlying(const std::string& equityName);
};

%shared_ptr(TradeBarrier)
class TradeBarrier : public TradeMonetary {
public:
    TradeBarrier(QuantLib::Real value, std::string currency);
};
%template(TradeBarrierVector) vector<ext::shared_ptr<TradeBarrier>>;

%shared_ptr(BarrierData)
class BarrierData : public XMLSerializable {
public:
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
%extend BarrierData {
    BarrierData(const std::string& barrierType, const std::vector<double>& levels, const double rebate,
                const std::vector<ext::shared_ptr<TradeBarrier>>& tradeBarriers, const std::string& style = std::string(),
                const std::optional<string>& strictComparison = std::nullopt, const std::optional<bool>& overrideTriggered = std::nullopt) {
        return new BarrierData(barrierType, levels, rebate, VECTOR_SWIG_TO_ORE(tradeBarriers),
            style, strictComparison, overrideTriggered);
    }
}

#endif
