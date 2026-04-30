/*
 Copyright (C) 2018, 2020 Quaternion Risk Management Ltd
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

#ifndef qle_instruments_i
#define qle_instruments_i

%include common.i
%include cashflows.i
%include instruments.i
%include termstructures.i
%include timebasket.i
%include indexes.i
%include currencies.i
%include qle_ccyswap.i
%include qle_indexes.i
%include qle_termstructures.i

%rename(QLECallableBond) QuantExt::CallableBond;
%rename(QLEBalanceGuaranteedSwap) QuantExt::BalanceGuaranteedSwap;

%shared_ptr(QuantExt::CrossCcyBasisSwap)
namespace QuantExt {
class CrossCcyBasisSwap : public QuantExt::CrossCcySwap {
  public:
    CrossCcyBasisSwap(QuantLib::Real payNominal,
                      const QuantLib::Currency& payCurrency,
                      const QuantLib::Schedule& paySchedule,
                      const ext::shared_ptr<IborIndex>& payIndex,
                      QuantLib::Spread paySpread,
		      QuantLib::Real payGearing,
                      QuantLib::Real recNominal,
                      const QuantLib::Currency& recCurrency,
                      const QuantLib::Schedule& recSchedule,
                      const ext::shared_ptr<IborIndex>& recIndex,
                      QuantLib::Spread recSpread,
		      QuantLib::Real recGearing);
    QuantLib::Real payNominal() const;
    const QuantLib::Currency& payCurrency() const;
    const QuantLib::Schedule& paySchedule() const;
    QuantLib::Spread paySpread() const;
    QuantLib::Real payGearing() const;
    QuantLib::Real recNominal() const;
    const QuantLib::Currency& recCurrency() const;
    const QuantLib::Schedule& recSchedule() const;
    QuantLib::Spread recSpread() const;
    QuantLib::Real recGearing() const;
    QuantLib::Spread fairPaySpread() const;
    QuantLib::Spread fairRecSpread() const;
};
}


%shared_ptr(QuantExt::CrossCcyBasisMtMResetSwap)
namespace QuantExt {
class CrossCcyBasisMtMResetSwap : public QuantExt::CrossCcySwap {
  public:
    CrossCcyBasisMtMResetSwap(Real foreignNominal,
                              const Currency& foreignCurrency,
                              const Schedule& foreignSchedule,
                              const ext::shared_ptr<IborIndex>& foreignIndex,
                              Spread foreignSpread,
                              const Currency& domesticCurrency,
                              const Schedule& domesticSchedule,
                              const ext::shared_ptr<IborIndex>& domesticIndex,
                              Spread domesticSpread,
                              const ext::shared_ptr<FxIndex>& fxIdx,
                              bool receiveDomestic = true);
    Spread fairForeignSpread() const;
    Spread fairDomesticSpread() const;
};
}


%shared_ptr(QuantExt::Deposit)
namespace QuantExt {
class Deposit : public Instrument {
  public:
    Deposit(const QuantLib::Real nominal,
            const QuantLib::Rate rate,
            const QuantLib::Period& tenor,
            const QuantLib::Natural fixingDays,
            const QuantLib::Calendar& calendar,
            const QuantLib::BusinessDayConvention convention,
            const bool endOfMonth,
            const QuantLib::DayCounter& dayCounter,
            const QuantLib::Date& tradeDate,
            const bool isLong = true,
            const QuantLib::Period forwardStart = QuantLib::Period(0, QuantLib::Days));
    QuantLib::Date fixingDate() const;
    QuantLib::Date startDate() const;
    QuantLib::Date maturityDate() const;
    QuantLib::Real fairRate() const;
    const QuantLib::Leg& leg() const;
};
}

%shared_ptr(QuantExt::DepositEngine)
namespace QuantExt {
class DepositEngine : public PricingEngine {
  public:
    DepositEngine(const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve = QuantLib::Handle<QuantLib::YieldTermStructure>(),
                  QuantLib::ext::optional<bool> includeSettlementDateFlows = QuantLib::ext::nullopt,
                  QuantLib::Date settlementDate = QuantLib::Date(),
                  QuantLib::Date npvDate = QuantLib::Date());
};
}

%shared_ptr(QuantExt::Payment)
namespace QuantExt {
class Payment : public Instrument {
  public:
    Payment(const QuantLib::Real amount,
            const QuantLib::Currency& currency,
            const QuantLib::Date& date);
    const ext::shared_ptr<SimpleCashFlow>& cashFlow() const;
    QuantLib::Currency currency() const;
};
}


%shared_ptr(QuantExt::PaymentDiscountingEngine)
namespace QuantExt {
class PaymentDiscountingEngine : public PricingEngine {
  public:
    PaymentDiscountingEngine(const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
                             const QuantLib::Handle<QuantLib::Quote>& spotFX = QuantLib::Handle<QuantLib::Quote>(),
                             QuantLib::ext::optional<bool> includeSettlementDateFlows = QuantLib::ext::nullopt,
                             const QuantLib::Date& settlementDate = QuantLib::Date(),
                             const QuantLib::Date& npvDate = QuantLib::Date());
    const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve();
    const QuantLib::Handle<QuantLib::Quote>& spotFX();
};
}


%shared_ptr(QuantExt::FxForward)
namespace QuantExt {
class FxForward : public Instrument {
  public:
    FxForward(const QuantLib::Real& nominal1,
              const QuantLib::Currency& currency1,
              const QuantLib::Real& nominal2,
              const QuantLib::Currency& currency2,
              const QuantLib::Date& maturityDate,
              const bool& payCurrency1);
    const QuantLib::ExchangeRate& fairForwardRate();
    QuantLib::Real currency1Nominal() const;
    QuantLib::Real currency2Nominal() const;
    QuantLib::Currency currency1() const;
    QuantLib::Currency currency2() const;
    QuantLib::Date maturityDate() const;
    bool payCurrency1() const;
};
}


%shared_ptr(QuantExt::DiscountingFxForwardEngine)
namespace QuantExt {
class DiscountingFxForwardEngine : public PricingEngine {
  public:
    DiscountingFxForwardEngine(const QuantLib::Currency& ccy1,
                               const QuantLib::Handle<QuantLib::YieldTermStructure>& currency1Discountcurve,
                               const QuantLib::Currency& ccy2,
                               const QuantLib::Handle<QuantLib::YieldTermStructure>& currency2Discountcurve,
                               const QuantLib::Handle<QuantLib::Quote>& spotFX,
                               const QuantLib::Date& settlementDate = QuantLib::Date(),
                               const QuantLib::Date& npvDate = QuantLib::Date());
};
                }


                %shared_ptr(QuantExt::CommodityForward)
                namespace QuantExt {
                class CommodityForward : public Instrument {
public:
                  CommodityForward(const ext::shared_ptr<QuantExt::CommodityIndex>& index,
                     const QuantLib::Currency& currency,
                     QuantLib::Position::Type position,
                     QuantLib::Real quantity,
                     const QuantLib::Date& maturityDate,
                     QuantLib::Real strike);
                  const ext::shared_ptr<QuantExt::CommodityIndex>& index() const;
    const QuantLib::Currency& currency() const;
    QuantLib::Position::Type position() const;
    QuantLib::Real quantity() const;
    const QuantLib::Date& maturityDate() const;
    QuantLib::Real strike() const;
};
                }

                %shared_ptr(QuantExt::DiscountingCommodityForwardEngine)
                namespace QuantExt {
                class DiscountingCommodityForwardEngine : public PricingEngine {
public:
    DiscountingCommodityForwardEngine(const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
                                      QuantLib::ext::optional<bool> includeSettlementDateFlows = QuantLib::ext::nullopt,
                                      const QuantLib::Date& npvDate = QuantLib::Date());
};
                }

                %shared_ptr(QuantExt::VarianceSwap2)
                namespace QuantExt {
                class VarianceSwap2 : public Instrument {
public:
    VarianceSwap2(Position::Type position, Real strike, Real notional,
                  const Date& startDate, const Date& maturityDate,
                  const Calendar& calendar, bool addPastDividends);
};
                }

                %shared_ptr(QuantExt::GeneralisedReplicatingVarianceSwapEngine)
                namespace QuantExt {
                class GeneralisedReplicatingVarianceSwapEngine : public PricingEngine {
public:
    class VarSwapSettings {
    public:
        enum class Scheme { GaussLobatto, Segment };
        enum class Bounds { Fixed, PriceThreshold };
        VarSwapSettings() {}
        Scheme scheme = Scheme::GaussLobatto;
        Bounds bounds = Bounds::PriceThreshold;
        Real accuracy = 1E-5;
        Size maxIterations = 1000;
        Size steps = 100;
        Real priceThreshold = 1E-10;
        Size maxPriceThresholdSteps = 100;
        Real priceThresholdStep = 0.1;
        Real fixedMinStdDevs = -5.0;
        Real fixedMaxStdDevs = 5.0;
    };

    GeneralisedReplicatingVarianceSwapEngine(const ext::shared_ptr<QuantLib::Index>& index,
                                             const ext::shared_ptr<GeneralizedBlackScholesProcess>& process,
                                             const Handle<YieldTermStructure>& discountingTS,
                                             const VarSwapSettings settings = VarSwapSettings(),
                                             const bool staticTodaysSpot = true);
 };
}

%shared_ptr(QuantExt::MultiLegOption)
namespace QuantExt {
class MultiLegOption : public Instrument {
  public:
    MultiLegOption(const std::vector<Leg>& legs,
                   const std::vector<bool>& payer,
                   const std::vector<Currency>& currency,
                   const QuantLib::ext::shared_ptr<Exercise>& exercise = QuantLib::ext::shared_ptr<Exercise>(),
                   const Settlement::Type settlementType = Settlement::Physical,
                   Settlement::Method settlementMethod = Settlement::PhysicalOTC,
                   const std::vector<Date>& settlementDates = std::vector<Date>(),
                   const bool midCouponExericse = false,
                   const Period& noticePeriod = 0 * Days,
                   const Calendar& noticeCalendar = NullCalendar(),
                   const BusinessDayConvention noticeConvention = Following);
    const std::vector<Leg>& legs() const;
    const std::vector<bool>& payer() const;
    const std::vector<Currency>& currency() const;
    const QuantLib::ext::shared_ptr<Exercise> exercise() const;
    Real underlyingNpv() const;
};
}

%shared_ptr(QuantExt::BondTRS)
namespace QuantExt {
class BondTRS : public Instrument {
  public:
    BondTRS(const QuantLib::ext::shared_ptr<QuantExt::BondIndex>& bondIndex,
            const Real bondNotional,
            const Real initialPrice,
            const std::vector<Leg>& fundingLeg,
            const bool payTotalReturnLeg,
            const std::vector<Date>& valuationDates,
            const std::vector<Date>& paymentDates,
            const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fxIndex = nullptr,
            bool payBondCashFlowsImmediately = false,
            const Currency& fundingCurrency = Currency(),
            const Currency& bondCurrency = Currency(),
            const bool applyFXIndexFixingDays = false,
            const Period& payLagPeriod = Period(),
            const Calendar& paymentCalendar = WeekendsOnly());

    Real bondNotional() const;
    Real initialPrice() const;
    bool payTotalReturnLeg() const;
    const std::vector<Date>& valuationDates() const;
    const std::vector<Date>& paymentDates() const;
};
}

%shared_ptr(QuantExt::GenericSwaption)
namespace QuantExt {
class GenericSwaption : public Option {
  public:
    GenericSwaption(const ext::shared_ptr<QuantLib::Swap>& swap,
                    const ext::shared_ptr<Exercise>& exercise,
                    QuantLib::Settlement::Type delivery = QuantLib::Settlement::Physical,
                    QuantLib::Settlement::Method settlementMethod = QuantLib::Settlement::PhysicalOTC);

    Settlement::Type settlementType() const;
    Settlement::Method settlementMethod() const;
    const ext::shared_ptr<QuantLib::Swap>& underlyingSwap() const;
    Real underlyingValue() const;
};
}

%shared_ptr(QuantExt::RiskParticipationAgreement)
namespace QuantExt {
class RiskParticipationAgreement : public Instrument {
  public:
    RiskParticipationAgreement(const std::vector<Leg>& underlying,
                               const std::vector<bool>& underlyingPayer,
                               const std::vector<std::string>& underlyingCcys,
                               const std::vector<Leg>& protectionFee,
                               const bool protectionFeePayer,
                               const std::vector<std::string>& protectionFeeCcys,
                               const Real participationRate,
                               const Date& protectionStart,
                               const Date& protectionEnd,
                               const bool settlesAccrual,
                               const Real fixedRecoveryRate = Null<Real>(),
                               const QuantLib::ext::shared_ptr<QuantLib::Exercise>& exercise = nullptr,
                               const bool exerciseIsLong = false,
                               const std::vector<QuantLib::ext::shared_ptr<CashFlow>>& premium = std::vector<QuantLib::ext::shared_ptr<CashFlow>>(),
                               const bool nakedOption = false);
};
}

%shared_ptr(QuantExt::ConvertibleBond2)
namespace QuantExt {
class ConvertibleBond2 : public Bond {
};
}

%shared_ptr(QuantExt::CallableBond)
namespace QuantExt {
class CallableBond : public Bond {
};
}

%shared_ptr(QuantExt::BalanceGuaranteedSwap)
namespace QuantExt {
class BalanceGuaranteedSwap : public Swap {
};
}

#endif
