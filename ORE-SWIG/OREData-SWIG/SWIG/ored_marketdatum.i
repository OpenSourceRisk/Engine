/*
 Copyright (C) 2019, 2020 Quaternion Risk Management Ltd
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

#ifndef ored_marketdatum_i
#define ored_marketdatum_i

%{
using ore::data::BaseStrike;
using ore::data::Expiry;
using ore::data::MarketDatum;
using ore::data::parseMarketDatum;
using ore::data::MoneyMarketQuote;
using ore::data::FRAQuote;
using ore::data::ImmFraQuote;
using ore::data::SwapQuote;
using ore::data::ZeroQuote;
using ore::data::DiscountQuote;
using ore::data::MMFutureQuote;
using ore::data::OIFutureQuote;
using ore::data::BasisSwapQuote;
using ore::data::BMASwapQuote;
using ore::data::CrossCcyBasisSwapQuote;
using ore::data::CrossCcyFixFloatSwapQuote;
using ore::data::CdsQuote;
using ore::data::HazardRateQuote;
using ore::data::RecoveryRateQuote;
using ore::data::SwaptionQuote;
using ore::data::SwaptionShiftQuote;
using ore::data::BondOptionQuote;
using ore::data::BondOptionShiftQuote;
using ore::data::CapFloorQuote;
using ore::data::CapFloorShiftQuote;
using ore::data::FXSpotQuote;
using ore::data::FXForwardQuote;
using ore::data::FXOptionQuote;
using ore::data::ZcInflationSwapQuote;
using ore::data::InflationCapFloorQuote;
using ore::data::ZcInflationCapFloorQuote;
using ore::data::YoYInflationSwapQuote;
using ore::data::YyInflationCapFloorQuote;
using ore::data::SeasonalityQuote;
using ore::data::EquitySpotQuote;
using ore::data::EquityForwardQuote;
using ore::data::EquityDividendYieldQuote;
using ore::data::EquityOptionQuote;
using ore::data::SecuritySpreadQuote;
using ore::data::BaseCorrelationQuote;
using ore::data::IndexCDSOptionQuote;
using ore::data::CommoditySpotQuote;
using ore::data::CommodityForwardQuote;
using ore::data::CommodityOptionQuote;
using ore::data::CorrelationQuote;
using ore::data::CPRQuote;
using ore::data::BondPriceQuote;
%}

%shared_ptr(ore::data::BaseStrike)
%shared_ptr(ore::data::Expiry)
%shared_ptr(ore::data::MarketDatum)
%shared_ptr(ore::data::MoneyMarketQuote)
%shared_ptr(ore::data::FRAQuote)
%shared_ptr(ore::data::ImmFraQuote)
%shared_ptr(ore::data::SwapQuote)
%shared_ptr(ore::data::ZeroQuote)
%shared_ptr(ore::data::DiscountQuote)
%shared_ptr(ore::data::MMFutureQuote)
%shared_ptr(ore::data::OIFutureQuote)
%shared_ptr(ore::data::BasisSwapQuote)
%shared_ptr(ore::data::BMASwapQuote)
%shared_ptr(ore::data::CrossCcyBasisSwapQuote)
%shared_ptr(ore::data::CrossCcyFixFloatSwapQuote)
%shared_ptr(ore::data::CdsQuote)
%shared_ptr(ore::data::HazardRateQuote)
%shared_ptr(ore::data::RecoveryRateQuote)
%shared_ptr(ore::data::SwaptionQuote)
%shared_ptr(ore::data::SwaptionShiftQuote)
%shared_ptr(ore::data::BondOptionQuote)
%shared_ptr(ore::data::BondOptionShiftQuote)
%shared_ptr(ore::data::CapFloorQuote)
%shared_ptr(ore::data::CapFloorShiftQuote)
%shared_ptr(ore::data::FXSpotQuote)
%shared_ptr(ore::data::FXForwardQuote)
%shared_ptr(ore::data::FXOptionQuote)
%shared_ptr(ore::data::ZcInflationSwapQuote)
%shared_ptr(ore::data::InflationCapFloorQuote)
%shared_ptr(ore::data::ZcInflationCapFloorQuote)
%shared_ptr(ore::data::YoYInflationSwapQuote)
%shared_ptr(ore::data::YyInflationCapFloorQuote)
%shared_ptr(ore::data::SeasonalityQuote)
%shared_ptr(ore::data::EquitySpotQuote)
%shared_ptr(ore::data::EquityForwardQuote)
%shared_ptr(ore::data::EquityDividendYieldQuote)
%shared_ptr(ore::data::EquityOptionQuote)
%shared_ptr(ore::data::SecuritySpreadQuote)
%shared_ptr(ore::data::BaseCorrelationQuote)
%shared_ptr(ore::data::IndexCDSOptionQuote)
%shared_ptr(ore::data::CommoditySpotQuote)
%shared_ptr(ore::data::CommodityForwardQuote)
%shared_ptr(ore::data::CommodityOptionQuote)
%shared_ptr(ore::data::CorrelationQuote)
%shared_ptr(ore::data::CPRQuote)
%shared_ptr(ore::data::BondPriceQuote)

namespace ore {
namespace data {

class BaseStrike;
class Expiry;

ext::shared_ptr<ore::data::MarketDatum> parseMarketDatum(const Date&, const std::string&, const Real&);

class MarketDatum {
public:
    enum class InstrumentType {
        ZERO,
        DISCOUNT,
        MM,
        MM_FUTURE,
        OI_FUTURE,
        FRA,
        IMM_FRA,
        IR_SWAP,
        BASIS_SWAP,
        BMA_SWAP,
        CC_BASIS_SWAP,
        CC_FIX_FLOAT_SWAP,
        CDS,
        CDS_INDEX,
        FX_SPOT,
        FX_FWD,
        HAZARD_RATE,
        RECOVERY_RATE,
        SWAPTION,
        CAPFLOOR,
        FX_OPTION,
        ZC_INFLATIONSWAP,
        ZC_INFLATIONCAPFLOOR,
        YY_INFLATIONSWAP,
        YY_INFLATIONCAPFLOOR,
        SEASONALITY,
        EQUITY_SPOT,
        EQUITY_FWD,
        EQUITY_DIVIDEND,
        EQUITY_OPTION,
        BOND,
        BOND_OPTION,
        INDEX_CDS_OPTION,
        COMMODITY_SPOT,
        COMMODITY_FWD,
        CORRELATION,
        COMMODITY_OPTION,
        CPR
    };

    enum class QuoteType {
        BASIS_SPREAD,
        CREDIT_SPREAD,
        CONV_CREDIT_SPREAD,
        YIELD_SPREAD,
        HAZARD_RATE,
        RATE,
        RATIO,
        PRICE,
        RATE_LNVOL,
        RATE_NVOL,
        RATE_SLNVOL,
        BASE_CORRELATION,
        SHIFT,
        NONE
    };

    MarketDatum(Real value, Date asofDate, const std::string& name, QuoteType quoteType,
                InstrumentType instrumentType);
    const std::string& name() const;
    const Handle<Quote>& quote() const;
    Date asofDate() const;
    MarketDatum::InstrumentType instrumentType();
    MarketDatum::QuoteType quoteType();
};

class MoneyMarketQuote : public MarketDatum {
public:
    MoneyMarketQuote(Real value, Date asofDate, const std::string& name,
                     MarketDatum::QuoteType quoteType, std::string ccy,
                     Period fwdStart, Period term, const std::string& indexName = "");
    const std::string& ccy() const;
    const Period& fwdStart() const;
    const Period& term() const;
    %extend {
        static const ext::shared_ptr<MoneyMarketQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<MoneyMarketQuote>(baseInput);
        }
    }
};

class FRAQuote : public MarketDatum {
public:
    FRAQuote(Real value, Date asofDate, const std::string& name,
             MarketDatum::QuoteType quoteType, std::string ccy,
             Period fwdStart, Period term);
    const std::string& ccy() const;
    const Period& fwdStart() const;
    const Period& term() const;
    %extend {
        static const ext::shared_ptr<FRAQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<FRAQuote>(baseInput);
        }
    }
};

class ImmFraQuote : public MarketDatum {
public:
    ImmFraQuote(Real value, Date asofDate, const std::string& name,
                MarketDatum::QuoteType quoteType, std::string ccy, Size imm1, Size imm2);
    const std::string& ccy() const;
    const Size& imm1() const;
    const Size& imm2() const;
    %extend {
        static const ext::shared_ptr<ImmFraQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<ImmFraQuote>(baseInput);
        }
    }
};

class SwapQuote : public MarketDatum {
public:
    SwapQuote(Real value, Date asofDate, const std::string& name,
              MarketDatum::QuoteType quoteType, std::string ccy,
              Period fwdStart, Period term, Period tenor, const std::string& indexName = "");
    const std::string& ccy() const;
    const Period& fwdStart() const;
    const Period& term() const;
    const Period& tenor() const;
    %extend {
        static const ext::shared_ptr<SwapQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<SwapQuote>(baseInput);
        }
    }
};

class ZeroQuote : public MarketDatum {
public:
    ZeroQuote(Real value, Date asofDate, const std::string& name,
              MarketDatum::QuoteType quoteType, std::string ccy, Date date,
              DayCounter dayCounter, Period tenor = Period());
    const std::string& ccy() const;
    Date date() const;
    DayCounter dayCounter() const;
    const Period& tenor() const;
    bool tenorBased() const;
    %extend {
        static const ext::shared_ptr<ZeroQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<ZeroQuote>(baseInput);
        }
    }
};

class DiscountQuote : public MarketDatum {
public:
    DiscountQuote(Real value, Date asofDate, const std::string& name,
                  MarketDatum::QuoteType quoteType, std::string ccy, Date date, Period tenor);
    const std::string& ccy() const;
    Date date() const;
    %extend {
        static const ext::shared_ptr<DiscountQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<DiscountQuote>(baseInput);
        }
    }
};

class MMFutureQuote : public MarketDatum {
public:
    MMFutureQuote(Real value, Date asofDate, const std::string& name,
                  MarketDatum::QuoteType quoteType, std::string ccy, std::string expiry,
                  std::string contract = "", Period tenor = 3 * Months);
    const std::string& ccy() const;
    const std::string expiry() const;
    Natural expiryYear() const;
    Month expiryMonth() const;
    const std::string& contract() const;
    const Period& tenor() const;
    %extend {
        static const ext::shared_ptr<MMFutureQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<MMFutureQuote>(baseInput);
        }
    }
};

class OIFutureQuote : public MarketDatum {
public:
    OIFutureQuote(Real value, Date asofDate, const std::string& name,
                  MarketDatum::QuoteType quoteType, std::string ccy, std::string expiry,
                  std::string contract = "", Period tenor = 3 * Months);
    const std::string& ccy() const;
    const std::string expiry() const;
    Natural expiryYear() const;
    Month expiryMonth() const;
    const std::string& contract() const;
    const Period& tenor() const;
    %extend {
        static const ext::shared_ptr<OIFutureQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<OIFutureQuote>(baseInput);
        }
    }
};

class BasisSwapQuote : public MarketDatum {
public:
    BasisSwapQuote(Real value, Date asofDate, const std::string& name,
                   MarketDatum::QuoteType quoteType, Period flatTerm, Period term,
                   std::string ccy = "USD", Period maturity = 3 * Months);
    const Period& flatTerm() const;
    const Period& term() const;
    const std::string& ccy() const;
    const Period& maturity() const;
    %extend {
        static const ext::shared_ptr<BasisSwapQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<BasisSwapQuote>(baseInput);
        }
    }
};

class BMASwapQuote : public MarketDatum {
public:
    BMASwapQuote(Real value, Date asofDate, const std::string& name,
                 MarketDatum::QuoteType quoteType, Period term,
                 std::string ccy = "USD", Period maturity = 3 * Months);
    const Period& term() const;
    const std::string& ccy() const;
    const Period& maturity() const;
    %extend {
        static const ext::shared_ptr<BMASwapQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<BMASwapQuote>(baseInput);
        }
    }
};

class CrossCcyBasisSwapQuote : public MarketDatum {
public:
    CrossCcyBasisSwapQuote(Real value, Date asofDate, const std::string& name,
                           MarketDatum::QuoteType quoteType, std::string flatCcy, Period flatTerm,
                           std::string ccy, Period term, Period maturity = 3 * Months);
    const std::string& flatCcy() const;
    const Period& flatTerm() const;
    const std::string& ccy() const;
    const Period& term() const;
    const Period& maturity() const;
    %extend {
        static const ext::shared_ptr<CrossCcyBasisSwapQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<CrossCcyBasisSwapQuote>(baseInput);
        }
    }
};

class CrossCcyFixFloatSwapQuote : public MarketDatum {
public:
    CrossCcyFixFloatSwapQuote(Real value, Date asofDate, const std::string& name,
                              MarketDatum::QuoteType quoteType,
                              const std::string& floatCurrency, const Period& floatTenor,
                              const std::string& fixedCurrency, const Period& fixedTenor,
                              const Period& maturity);
    const std::string& floatCurrency() const;
    const QuantLib::Period& floatTenor() const;
    const std::string& fixedCurrency() const;
    const QuantLib::Period& fixedTenor() const;
    const QuantLib::Period& maturity() const;
    %extend {
        static const ext::shared_ptr<CrossCcyFixFloatSwapQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<CrossCcyFixFloatSwapQuote>(baseInput);
        }
    }
};

class CdsQuote : public MarketDatum {
public:
    CdsQuote(Real value, Date asofDate, const std::string& name, MarketDatum::QuoteType quoteType,
             const std::string& underlyingName, const std::string& seniority,
             const std::string& ccy, Period term, const std::string& docClause = "",
             Real runningSpread = Null<Real>());
    const Period& term() const;
    const std::string& seniority() const;
    const std::string& ccy() const;
    const std::string& underlyingName() const;
    const std::string& docClause() const;
    Real runningSpread() const;
    %extend {
        static const ext::shared_ptr<CdsQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<CdsQuote>(baseInput);
        }
    }
};

class HazardRateQuote : public MarketDatum {
public:
    HazardRateQuote(Real value, Date asofDate, const std::string& name,
                    const std::string& underlyingName, const std::string& seniority,
                    const std::string& ccy, Period term, const string& docClause = "");
    const Period& term() const;
    const std::string& seniority() const;
    const std::string& ccy() const;
    const std::string& underlyingName() const;
    const std::string& docClause() const;
    %extend {
        static const ext::shared_ptr<HazardRateQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<HazardRateQuote>(baseInput);
        }
    }
};

class RecoveryRateQuote : public MarketDatum {
public:
    RecoveryRateQuote(Real value, Date asofDate, const std::string& name,
                      const std::string& underlyingName, const std::string& seniority,
                      const std::string& ccy, const string& docClause = "");
    const std::string& seniority() const;
    const std::string& ccy() const;
    const std::string& underlyingName() const;
    const std::string& docClause() const;
    %extend {
        static const ext::shared_ptr<RecoveryRateQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<RecoveryRateQuote>(baseInput);
        }
    }
};

class SwaptionQuote : public MarketDatum {
public:
    SwaptionQuote(Real value, Date asofDate, const std::string& name,
                  MarketDatum::QuoteType quoteType, std::string ccy, Period expiry,
                  Period term, std::string dimension, Real strike = 0.0,
                  const std::string& quoteTag = std::string());
    const std::string& ccy() const;
    const Period& expiry() const;
    const Period& term() const;
    const std::string& dimension() const;
    Real strike();
    const std::string& quoteTag() const;
    %extend {
        static const ext::shared_ptr<SwaptionQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<SwaptionQuote>(baseInput);
        }
    }
};

class SwaptionShiftQuote : public MarketDatum {
public:
    SwaptionShiftQuote(Real value, Date asofDate, const std::string& name,
                       MarketDatum::QuoteType quoteType, std::string ccy, Period term,
                       const std::string& quoteTag = std::string());
    const std::string& ccy() const;
    const Period& term() const;
    const std::string& quoteTag() const;
    %extend {
        static const ext::shared_ptr<SwaptionShiftQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<SwaptionShiftQuote>(baseInput);
        }
    }
};

class BondOptionQuote : public MarketDatum {
public:
    BondOptionQuote(Real value, Date asofDate, const std::string& name,
                    MarketDatum::QuoteType quoteType, std::string qualifier,
                    Period expiry, Period term);
    const std::string& qualifier() const;
    const Period& expiry() const;
    const Period& term() const;
};

class BondOptionShiftQuote : public MarketDatum {
public:
    BondOptionShiftQuote(Real value, Date asofDate, const std::string& name,
                         MarketDatum::QuoteType quoteType, std::string& qualifier,
                         Period term);
    const std::string& qualifier() const;
    const Period& term() const;
};

class CapFloorQuote : public MarketDatum {
public:
    CapFloorQuote(Real value, Date asofDate, const std::string& name,
                  MarketDatum::QuoteType quoteType, std::string ccy, Period term,
                  Period underlying, bool atm, bool relative, Real strike = 0.0,
                  const std::string& indexName = std::string());
    const std::string& ccy() const;
    const Period& term() const;
    const Period& underlying() const;
    bool atm() const;
    bool relative() const;
    Real strike();
    const std::string& indexName() const;
    %extend {
        static const ext::shared_ptr<CapFloorQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<CapFloorQuote>(baseInput);
        }
    }
};

class CapFloorShiftQuote : public MarketDatum {
public:
    CapFloorShiftQuote(Real value, Date asofDate, const std::string& name,
                       MarketDatum::QuoteType quoteType, std::string ccy,
                       const Period& indexTenor, const std::string& indexName = std::string());
    const std::string& ccy() const;
    const Period& indexTenor() const;
    const std::string& indexName() const;
    %extend {
        static const ext::shared_ptr<CapFloorShiftQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<CapFloorShiftQuote>(baseInput);
        }
    }
};

class FXSpotQuote : public MarketDatum {
public:
    FXSpotQuote(Real value, Date asofDate, const std::string& name,
                MarketDatum::QuoteType quoteType,
                std::string unitCcy, std::string ccy);
    const std::string& unitCcy() const;
    const std::string& ccy() const;
    %extend {
        static const ext::shared_ptr<FXSpotQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<FXSpotQuote>(baseInput);
        }
    }
};

class FXForwardQuote : public MarketDatum {
public:
    FXForwardQuote(Real value, Date asofDate, const std::string& name,
                   MarketDatum::QuoteType quoteType,
                   std::string unitCcy, std::string ccy,
                   const Period& term, Real conversionFactor = 1.0);
    const std::string& unitCcy() const;
    const std::string& ccy() const;
    const Period& term() const;
    Real conversionFactor() const;
    %extend {
        static const ext::shared_ptr<FXForwardQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<FXForwardQuote>(baseInput);
        }
    }
};

class FXOptionQuote : public MarketDatum {
public:
    FXOptionQuote(Real value, Date asofDate, const std::string& name,
                  MarketDatum::QuoteType quoteType,
                  std::string unitCcy, std::string ccy,
                  Period expiry, std::string strike);
    const std::string& unitCcy() const;
    const std::string& ccy() const;
    const Period& expiry() const;
    const std::string& strike() const;
    %extend {
        static const ext::shared_ptr<FXOptionQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<FXOptionQuote>(baseInput);
        }
    }
};

class ZcInflationSwapQuote : public MarketDatum {
public:
    ZcInflationSwapQuote(Real value, Date asofDate, const std::string& name,
                         const std::string& index, Period term);
    std::string index();
    Period term();
    %extend {
        static const ext::shared_ptr<ZcInflationSwapQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<ZcInflationSwapQuote>(baseInput);
        }
    }
};

class InflationCapFloorQuote : public MarketDatum {
public:
    InflationCapFloorQuote(Real value, Date asofDate, const std::string& name,
                           MarketDatum::QuoteType quoteType,
                           const std::string& index, Period term,
                           bool isCap, const std::string& strike,
                           MarketDatum::InstrumentType instrumentType);
    std::string index();
    Period term();
    bool isCap();
    std::string strike();
    %extend {
        static const ext::shared_ptr<InflationCapFloorQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<InflationCapFloorQuote>(baseInput);
        }
    }
};

class ZcInflationCapFloorQuote : public MarketDatum {
public:
    ZcInflationCapFloorQuote(Real value, Date asofDate, const std::string& name,
                             MarketDatum::QuoteType quoteType,
                             const std::string& index, Period term,
                             bool isCap, const std::string& strike);
    std::string index();
    Period term();
    bool isCap();
    std::string strike();
    %extend {
        static const ext::shared_ptr<ZcInflationCapFloorQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<ZcInflationCapFloorQuote>(baseInput);
        }
    }
};

class YoYInflationSwapQuote : public MarketDatum {
public:
    YoYInflationSwapQuote(Real value, Date asofDate, const std::string& name,
                          const std::string& index, Period term);
    std::string index();
    Period term();
    %extend {
        static const ext::shared_ptr<YoYInflationSwapQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<YoYInflationSwapQuote>(baseInput);
        }
    }
};

class YyInflationCapFloorQuote : public MarketDatum {
public:
    YyInflationCapFloorQuote(Real value, Date asofDate, const std::string& name,
                             MarketDatum::QuoteType quoteType,
                             const std::string& index, Period term,
                             bool isCap, const std::string& strike);
    std::string index();
    Period term();
    bool isCap();
    std::string strike();
    %extend {
        static const ext::shared_ptr<YyInflationCapFloorQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<YyInflationCapFloorQuote>(baseInput);
        }
    }
};

class SeasonalityQuote : public MarketDatum {
public:
    SeasonalityQuote(Real value, Date asofDate, const std::string& name,
                     const std::string& index, const std::string& type,
                     const std::string& month);
    std::string index();
    std::string type();
    std::string month();
    QuantLib::Size applyMonth() const;
    %extend {
        static const ext::shared_ptr<SeasonalityQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<SeasonalityQuote>(baseInput);
        }
    }
};

class EquitySpotQuote : public MarketDatum {
public:
    EquitySpotQuote(Real value, Date asofDate, const std::string& name,
                    MarketDatum::QuoteType quoteType,
                    std::string equityName, std::string ccy);
    const std::string& eqName() const;
    const std::string& ccy() const;
    %extend {
        static const ext::shared_ptr<EquitySpotQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<EquitySpotQuote>(baseInput);
        }
    }
};

class EquityForwardQuote : public MarketDatum {
public:
    EquityForwardQuote(Real value, Date asofDate, const std::string& name,
                       MarketDatum::QuoteType quoteType,
                       std::string equityName, std::string ccy,
                       const Date& expiryDate);
    const std::string& eqName() const;
    const std::string& ccy() const;
    const Date& expiryDate() const;
    %extend {
        static const ext::shared_ptr<EquityForwardQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<EquityForwardQuote>(baseInput);
        }
    }
};

class EquityDividendYieldQuote : public MarketDatum {
public:
    EquityDividendYieldQuote(Real value, Date asofDate, const std::string& name,
                             MarketDatum::QuoteType quoteType,
                             std::string equityName, std::string ccy,
                             const Date& tenorDate);
    const std::string& eqName() const;
    const std::string& ccy() const;
    const Date& tenorDate() const;
    %extend {
        static const ext::shared_ptr<EquityDividendYieldQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<EquityDividendYieldQuote>(baseInput);
        }
    }
};

class EquityOptionQuote : public MarketDatum {
public:
    EquityOptionQuote(Real value, Date asofDate, const std::string& name,
                      MarketDatum::QuoteType quoteType,
                      std::string equityName, std::string ccy,
                      std::string expiry, const ext::shared_ptr<BaseStrike>& strike, bool isCall = true);
    const std::string& eqName() const;
    const std::string& ccy() const;
    const std::string& expiry() const;
    const std::string& strike() const;
    %extend {
        static const ext::shared_ptr<EquityOptionQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<EquityOptionQuote>(baseInput);
        }
    }
};

class SecuritySpreadQuote : public MarketDatum {
public:
    SecuritySpreadQuote(Real value, Date asofDate, const std::string& name,
                        const std::string securityID);
    const std::string& securityID() const;
    %extend {
        static const ext::shared_ptr<SecuritySpreadQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<SecuritySpreadQuote>(baseInput);
        }
    }
};

class BaseCorrelationQuote : public MarketDatum {
public:
    BaseCorrelationQuote(Real value, Date asofDate, const std::string& name,
                         MarketDatum::QuoteType quoteType,
                         const std::string cdsIndexName,
                         Period term, Real attachmentPoint, Real detachmentPoint);
    const std::string& cdsIndexName() const;
    Real detachmentPoint() const;
    Period term() const;
    %extend {
        static const ext::shared_ptr<BaseCorrelationQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<BaseCorrelationQuote>(baseInput);
        }
    }
};

class IndexCDSOptionQuote : public MarketDatum {
public:
    IndexCDSOptionQuote(Real value, Date asofDate, const std::string& name,
                        const std::string& indexName, const ext::shared_ptr<Expiry>& expiry,
                        const std::string& indexTerm = "",
                        const ext::shared_ptr<BaseStrike>& strike = nullptr);
    const std::string& indexName() const;
    const ext::shared_ptr<Expiry>& expiry() const;
    const std::string& indexTerm() const;
    const ext::shared_ptr<BaseStrike>& strike() const;
    %extend {
        static const ext::shared_ptr<IndexCDSOptionQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<IndexCDSOptionQuote>(baseInput);
        }
    }
};

class CommoditySpotQuote : public MarketDatum {
public:
    CommoditySpotQuote(Real value, Date asofDate, const std::string& name,
                       MarketDatum::QuoteType quoteType,
                       const std::string commodityName,
                       const std::string quoteCurrency);
    const std::string& commodityName() const;
    const std::string& quoteCurrency() const;
    %extend {
        static const ext::shared_ptr<CommoditySpotQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<CommoditySpotQuote>(baseInput);
        }
    }
};

class CommodityForwardQuote : public MarketDatum {
public:
    CommodityForwardQuote(Real value, Date asofDate, const std::string& name,
                          MarketDatum::QuoteType quoteType,
                          const std::string commodityName,
                          const std::string quoteCurrency,
                          const QuantLib::Date& expiryDate);
    CommodityForwardQuote(Real value, const Date& asofDate, const std::string& name,
                          MarketDatum::QuoteType quoteType, const std::string& commodityName,
                          const std::string& quoteCurrency, const Period& tenor,
                          boost::optional<QuantLib::Period> startTenor = boost::none);

    const std::string& commodityName() const;
    const std::string& quoteCurrency() const;
    const Date& expiryDate() const;
    const Period& tenor() const;
    const boost::optional<QuantLib::Period>& startTenor() const;
    bool tenorBased() const;
    %extend {
        static const ext::shared_ptr<CommodityForwardQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<CommodityForwardQuote>(baseInput);
        }
    }
};

class CommodityOptionQuote : public MarketDatum {
public:
    CommodityOptionQuote(Real value, Date asofDate, const std::string& name,
                         MarketDatum::QuoteType quoteType,
                         const std::string commodityName,
                         const std::string quoteCurrency,
                         const ext::shared_ptr<Expiry>& expiry,
                         const ext::shared_ptr<BaseStrike>& strike,
                         QuantLib::Option::Type optionType = QuantLib::Option::Call);
    const std::string& commodityName() const;
    const std::string& quoteCurrency() const;
    const ext::shared_ptr<Expiry>& expiry() const;
    const ext::shared_ptr<BaseStrike>& strike() const;
    QuantLib::Option::Type optionType() const;
    %extend {
        static const ext::shared_ptr<CommodityOptionQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<CommodityOptionQuote>(baseInput);
        }
    }
};

class CorrelationQuote : public MarketDatum {
public:
    CorrelationQuote(Real value, Date asofDate, const std::string& name,
                     MarketDatum::QuoteType quoteType,
                     const std::string index1,
                     const std::string index2,
                     const std::string expiry,
                     const std::string strike);
    const std::string& index1() const;
    const std::string& index2() const;
    const std::string& expiry() const;
    const std::string& strike() const;
    %extend {
        static const ext::shared_ptr<CorrelationQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<CorrelationQuote>(baseInput);
        }
    }
};

class CPRQuote : public MarketDatum {
public:
    CPRQuote(Real value, Date asofDate, const std::string& name,
             const std::string securityID);
    const std::string& securityID() const;
    %extend {
        static const ext::shared_ptr<CPRQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<CPRQuote>(baseInput);
        }
    }
};

class BondPriceQuote : public MarketDatum {
public:
    BondPriceQuote(Real value, Date asofDate, const std::string& name, const std::string& securityId);
    const std::string& securityID() const;

    %extend {
        static const ext::shared_ptr<BondPriceQuote> getFullView(ext::shared_ptr<MarketDatum> baseInput) {
            return ext::dynamic_pointer_cast<BondPriceQuote>(baseInput);
        }
    }
};

} // namespace data
} // namespace ore

#endif
