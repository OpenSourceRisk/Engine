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
using ore::data::EquitySwap;
using ore::data::InflationSwap;
using ore::data::TradeBarrier;
using ore::data::BarrierData;
using ore::data::XMLSerializable;
%}

%shared_ptr(TradeMonetary)
class TradeMonetary {
public:
    TradeMonetary();
    TradeMonetary(const QuantLib::Real& value, std::string currency = std::string());
    TradeMonetary(const std::string& valueString);
};

%shared_ptr(PremiumData)
class PremiumData : public XMLSerializable {
public:
    PremiumData();
    PremiumData(double amount, const std::string& ccy, const QuantLib::Date& payDate);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(OptionExerciseData)
class OptionExerciseData : public XMLSerializable {
public:
    OptionExerciseData();
    OptionExerciseData(const std::string& date, const std::string& price);
    const QuantLib::Date& date() const;
    QuantLib::Real price() const;
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(OptionPaymentData)
class OptionPaymentData : public XMLSerializable {
public:
    enum class RelativeTo { Expiry, Exercise };

    OptionPaymentData();
    OptionPaymentData(const std::vector<std::string>& dates);
    OptionPaymentData(const std::string& lag, const std::string& calendar,
                      const std::string& convention,
                      const std::string& relativeTo = "Expiry");
    bool rulesBased() const;
    const std::vector<QuantLib::Date>& dates() const;
    QuantLib::Natural lag() const;
    const QuantLib::Calendar& calendar() const;
    QuantLib::BusinessDayConvention convention() const;
    RelativeTo relativeTo() const;
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(OptionData)
class OptionData : public XMLSerializable {
public:
    OptionData(std::string longShort, std::string callPut, std::string style, bool payoffAtExpiry, std::vector<std::string> exerciseDates,
               std::string settlement = "Cash", std::string settlementMethod = std::string(), const PremiumData& premiumData = {},
               std::vector<double> exerciseFees = std::vector<Real>(), std::vector<double> exercisePrices = std::vector<Real>(),
               std::string noticePeriod = std::string(), std::string noticeCalendar = std::string(), std::string noticeConvention = std::string(),
               const std::vector<std::string>& exerciseFeeDates = std::vector<std::string>(),
               const std::vector<std::string>& exerciseFeeTypes = std::vector<std::string>(), std::string exerciseFeeSettlementPeriod = std::string(),
               std::string exerciseFeeSettlementCalendar = std::string(), std::string exerciseFeeSettlementConvention = std::string(),
               std::string payoffType = std::string(), std::string payoffType2 = std::string(),
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
%template(UnderlyingVector) std::vector<ext::shared_ptr<Underlying>>;
SWIG_SHARED_PTR_VECTOR_TYPEMAP(Underlying, UnderlyingVector)

%shared_ptr(EquityUnderlying)
class EquityUnderlying : public Underlying {
public:
    explicit EquityUnderlying(const std::string& equityName);
    EquityUnderlying(const std::string& name, const std::string& identifierType, const std::string& currency,
                     const std::string& exchange, QuantLib::Real weight);
};

%shared_ptr(TradeBarrier)
class TradeBarrier : public TradeMonetary {
public:
    TradeBarrier(QuantLib::Real value, std::string currency);
};
%template(TradeBarrierVector) std::vector<ext::shared_ptr<TradeBarrier>>;

%shared_ptr(BarrierData)
class BarrierData : public XMLSerializable {
public:
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
%template(BarrierDataVector) std::vector<ext::shared_ptr<BarrierData>>;
SWIG_SHARED_PTR_VECTOR_TYPEMAP(BarrierData, BarrierDataVector)
%extend BarrierData {
    BarrierData(const std::string& barrierType, const std::vector<double>& levels, const double rebate,
                const std::vector<ext::shared_ptr<TradeBarrier>>& tradeBarriers, const std::string& style = std::string(),
                const std::optional<std::string>& strictComparison = std::nullopt,
                const std::optional<bool>& overrideTriggered = std::nullopt) {
        return new BarrierData(barrierType, levels, rebate, VECTOR_SWIG_TO_ORE(tradeBarriers),
            style, strictComparison, overrideTriggered);
    }
}

%shared_ptr(EquitySwap)
class EquitySwap : public ORESwap {
public:
    EquitySwap();
    EquitySwap(const Envelope& env, const LegData& leg0, const LegData& leg1);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend EquitySwap {
    EquitySwap(const Envelope& env, const vector<ext::shared_ptr<LegData>>& legData) {
        return new EquitySwap(env, VECTOR_SWIG_TO_ORE(legData));
    }
}

%shared_ptr(InflationSwap)
class InflationSwap : public ORESwap {
public:
    InflationSwap();
    InflationSwap(const Envelope& env, const LegData& leg0, const LegData& leg1);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend InflationSwap {
    InflationSwap(const Envelope& env, const vector<ext::shared_ptr<LegData>>& legData) {
        return new InflationSwap(env, VECTOR_SWIG_TO_ORE(legData));
    }
}

#endif
