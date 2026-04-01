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
using ore::data::TradeMonetary;
using ore::data::PremiumData;
using ore::data::OptionExerciseData;
using ore::data::OptionPaymentData;
using ore::data::OptionData;
using ore::data::TradeStrike;
using ore::data::Underlying;
using ore::data::EquityUnderlying;
using ore::data::TradeBarrier;
using ore::data::BarrierData;
%}

%shared_ptr(ore::data::TradeMonetary)
%shared_ptr(ore::data::PremiumData)
%shared_ptr(ore::data::OptionExerciseData)
%shared_ptr(ore::data::OptionPaymentData)
%shared_ptr(ore::data::OptionData)
%shared_ptr(ore::data::TradeStrike)
%shared_ptr(ore::data::Underlying)
%shared_ptr(ore::data::EquityUnderlying)
%shared_ptr(ore::data::TradeBarrier)
%shared_ptr(ore::data::BarrierData)

namespace ore {
namespace data {
class TradeMonetary {
public:
    TradeMonetary();
    TradeMonetary(const QuantLib::Real& value, std::string currency = std::string());
    TradeMonetary(const std::string& valueString);
};

class PremiumData : public XMLSerializable {
public:
    PremiumData();
    PremiumData(double amount, const std::string& ccy, const QuantLib::Date& payDate);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

class OptionExerciseData : public XMLSerializable {
public:
    OptionExerciseData();
    OptionExerciseData(const std::string& date, const std::string& price);
    const QuantLib::Date& date() const;
    QuantLib::Real price() const;
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

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

class TradeStrike {
public:
    enum class Type {
        Price,
        Yield
    };
    TradeStrike(Type type, const QuantLib::Real& value);
    TradeStrike(const QuantLib::Real& value, const std::string& currency);
};

class Underlying : public XMLSerializable {
public:
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

class EquityUnderlying : public Underlying {
public:
    explicit EquityUnderlying(const std::string& equityName);
    EquityUnderlying(const std::string& name, const std::string& identifierType, const std::string& currency,
                     const std::string& exchange, QuantLib::Real weight);
};

class TradeBarrier : public TradeMonetary {
public:
    TradeBarrier(QuantLib::Real value, std::string currency);
};

class BarrierData : public XMLSerializable {
public:
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

} // namespace data
} // namespace ore

%template(UnderlyingVector) std::vector<ext::shared_ptr<ore::data::Underlying>>;
SWIG_SHARED_PTR_VECTOR_TYPEMAP(ore::data::Underlying, UnderlyingVector)

%template(TradeBarrierDataVector) std::vector<ore::data::TradeBarrier>;
%template(TradeBarrierVector) std::vector<ext::shared_ptr<ore::data::TradeBarrier>>;
SWIG_SHARED_PTR_VECTOR_TYPEMAP(ore::data::TradeBarrier, TradeBarrierVector)

%template(BarrierDataVector) std::vector<ext::shared_ptr<ore::data::BarrierData>>;
SWIG_SHARED_PTR_VECTOR_TYPEMAP(ore::data::BarrierData, BarrierDataVector)

%extend ore::data::BarrierData {
    BarrierData(const std::string& barrierType, const std::vector<double>& levels, const double rebate,
                const std::vector<ore::data::TradeBarrier>& tradeBarriers,
                const std::string& style = std::string(),
                const std::optional<std::string>& strictComparison = std::nullopt,
                const std::optional<bool>& overrideTriggered = std::nullopt) {
        return new ore::data::BarrierData(barrierType, levels, rebate, tradeBarriers,
            style, strictComparison, overrideTriggered);
    }

    BarrierData(const std::string& barrierType, const std::vector<double>& levels, const double rebate,
                const std::vector<ext::shared_ptr<ore::data::TradeBarrier>>& tradeBarriers,
                const std::string& style = std::string(),
                const std::optional<std::string>& strictComparison = std::nullopt,
                const std::optional<bool>& overrideTriggered = std::nullopt) {
        return new ore::data::BarrierData(barrierType, levels, rebate, VECTOR_SWIG_TO_ORE(tradeBarriers),
            style, strictComparison, overrideTriggered);
    }
}

%pythoncode %{
def _barrier_data_init(self, barrierType, levels, rebate, tradeBarriers, *args):
    ore_module = globals().get("_ORE", globals().get("_OREP"))
    if isinstance(tradeBarriers, (list, tuple)):
        vector = TradeBarrierDataVector()
        for tradeBarrier in tradeBarriers:
            vector.append(tradeBarrier)
        ore_module.BarrierData_swiginit(self, ore_module.new_BarrierData(barrierType, levels, rebate, vector, *args))
        return
    ore_module.BarrierData_swiginit(self, ore_module.new_BarrierData(barrierType, levels, rebate, tradeBarriers, *args))


BarrierData.__init__ = _barrier_data_init
%}
#endif
