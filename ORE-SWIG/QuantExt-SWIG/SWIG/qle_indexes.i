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

#ifndef qle_indexes_i
#define qle_indexes_i

%include instruments.i
%include termstructures.i
%include cashflows.i
%include timebasket.i
%include indexes.i
%include date.i
%include calendars.i
%include daycounters.i
%include currencies.i
%include inflation.i

%{
using QuantExt::BEHICP;
using QuantExt::BondIndex;
using QuantExt::BondFuturesIndex;
using QuantExt::ConstantMaturityBondIndex;
// QuantLib has an equity index since v1.30
using QuantExt::EquityIndex2;
using QuantExt::EquityAnnouncedDividendCurve;
using QuantExt::FxIndex;
using QuantExt::BMAIndexWrapper;
using QuantLib::BMAIndex;
using QuantExt::GenericIborIndex;
using QuantExt::CommodityIndex;
using QuantExt::CommoditySpotIndex;
using QuantExt::CommodityFuturesIndex;
using QuantLib::ZeroInflationIndex;
%}

%shared_ptr(BEHICP)
class BEHICP : public ZeroInflationIndex {
    public:
        BEHICP(const QuantLib::Handle<QuantLib::ZeroInflationTermStructure>& ts =
                                  QuantLib::Handle<QuantLib::ZeroInflationTermStructure>());
};

%shared_ptr(BondIndex)
class BondIndex : public Index {
    public:
        BondIndex(const std::string& securityName, const bool dirty = false, const bool relative = true,
              const Calendar& fixingCalendar = NullCalendar(), const ext::shared_ptr<QuantLib::Bond>& bond = nullptr,
              const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
              const Handle<DefaultProbabilityTermStructure>& defaultCurve = Handle<DefaultProbabilityTermStructure>(),
              const Handle<Quote>& recoveryRate = Handle<Quote>(),
              const Handle<Quote>& securitySpread = Handle<Quote>(),
              const Handle<YieldTermStructure>& incomeCurve = Handle<YieldTermStructure>(),
              const bool conditionalOnSurvival = true, const Date& issueDate = Date(),
	      const QuantExt::BondIndex::PriceQuoteMethod priceQuoteMethod = QuantExt::BondIndex::PriceQuoteMethod::PercentageOfPar,
	      const double priceQuoteBaseValue = 1.0,
	      const bool isInflationLinked = false,
              const double bidAskAdjustment = 0.0);
        const std::string& securityName() const;
        bool dirty() const;
        bool relative() const;
        ext::shared_ptr<QuantLib::Bond> bond() const;
        Handle<YieldTermStructure> discountCurve() const;
        Handle<DefaultProbabilityTermStructure> defaultCurve() const;
        Handle<Quote> recoveryRate() const;
        Handle<Quote> securitySpread() const;
        Handle<YieldTermStructure> incomeCurve() const;
        bool conditionalOnSurvival() const;
        Date issueDate() const;
        virtual Rate forecastFixing(const Date& fixingDate) const;
        Rate pastFixing(const Date& fixingDate) const;
};

%shared_ptr(BondFuturesIndex)
class BondFuturesIndex : public BondIndex {
    public:
        BondFuturesIndex(
            const QuantLib::Date& expiryDate, const std::string& securityName, const bool dirty = false,
            const bool relative = true, const Calendar& fixingCalendar = NullCalendar(),
            const ext::shared_ptr<QuantLib::Bond>& bond = nullptr,
            const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
            const Handle<DefaultProbabilityTermStructure>& defaultCurve = Handle<DefaultProbabilityTermStructure>(),
            const Handle<Quote>& recoveryRate = Handle<Quote>(), const Handle<Quote>& securitySpread = Handle<Quote>(),
            const Handle<YieldTermStructure>& incomeCurve = Handle<YieldTermStructure>(), 
	        const bool conditionalOnSurvival = true, const Date& issueDate = Date(),
            const QuantExt::BondIndex::PriceQuoteMethod priceQuoteMethod = QuantExt::BondIndex::PriceQuoteMethod::PercentageOfPar,
            const double priceQuoteBaseValue = 1.0);
        std::string name() const;
        Rate forecastFixing(const Date& fixingDate) const;
        const QuantLib::Date& expiryDate() const;
};

%shared_ptr(ConstantMaturityBondIndex)
class ConstantMaturityBondIndex : public InterestRateIndex {
    public:
        ConstantMaturityBondIndex(
			const std::string& familyName,
			const Period& tenor,
			Natural settlementDays = 0,
			Currency currency = Currency(),
			Calendar fixingCalendar = NullCalendar(),
			DayCounter dayCounter = SimpleDayCounter(),
            BusinessDayConvention convention = Following,
			bool endOfMonth = false,
            ext::shared_ptr<Bond> bond = nullptr,
            Compounding compounding = Compounded,
			Frequency frequency = Annual,
			Real accuracy = 1.0e-8,
			Size maxEvaluations = 100,
			Real guess = 0.05,
			QuantLib::Bond::Price::Type priceType = QuantLib::Bond::Price::Clean);
        Date maturityDate(const Date& valueDate) const;
        Rate forecastFixing(const Date& fixingDate) const;
        BusinessDayConvention convention() const;
        bool endOfMonth() const;
        const ext::shared_ptr<Bond>& bond() const;
};

// QuantLib has an equity index since v1.30, so we have renamed the QuantExt version to EquityIndex2 for now.
// EquityIndex to be consolidated with QuantLib and removed from QuantExt.

%shared_ptr(EquityIndex2)
class EquityIndex2 : public Index {
    public:
        EquityIndex2(const std::string& familyName, const Calendar& fixingCalendar, const Currency& currency,
                const Handle<Quote> spotQuote = Handle<Quote>(),
                const Handle<YieldTermStructure>& rate = Handle<YieldTermStructure>(),
                const Handle<YieldTermStructure>& dividend = Handle<YieldTermStructure>(),
                const Handle<EquityAnnouncedDividendCurve>& announcedDividend = Handle<EquityAnnouncedDividendCurve>());
        std::string name();
        Currency currency() const;
        Calendar fixingCalendar() const override;
        bool isValidFixingDate(const Date& fixingDate) const override;
        Real fixing(const Date& fixingDate, bool forecastTodaysFixing = false) const override;
        Real fixing(const Date& fixingDate, bool forecastTodaysFixing, bool incDividend) const;
        std::string familyName() const;
        const Handle<Quote>& equitySpot() const;
        const Handle<YieldTermStructure>& equityForecastCurve() const;
        const Handle<YieldTermStructure>& equityDividendCurve() const;
        const Handle<EquityAnnouncedDividendCurve>& announcedDividendCurve() const;
        Real forecastFixing(const Date& fixingDate) const;
        Real forecastFixing(const Time& fixingTime) const override;
        Real forecastFixing(const Date& fixingDate, bool incDividend) const;
        Real forecastFixing(const Time& fixingTime, bool incDividend) const;
        Real pastFixing(const Date& fixingDate) const override;
        ext::shared_ptr<EquityIndex2> clone(const Handle<Quote> spotQuote, const Handle<YieldTermStructure>& rate,
                                            const Handle<YieldTermStructure>& dividend, 
                                            const Handle<EquityAnnouncedDividendCurve>& announcedDividend) const;
    
};


%extend Handle<EquityIndex2> {
    // Handle<EquityIndex2>::addFixings() and __str__ inherited from Index cause compilation issues. We override them here
    void addFixings() {}
    std::string __str__() {
       return self->operator->()->name();
    }
};

%template(EquityIndex2Handle) Handle<EquityIndex2>;
%template(RelinkableEquityIndex2Handle) RelinkableHandle<EquityIndex2>;

%shared_ptr(FxIndex)
class FxIndex : public Index {
  public:
    FxIndex(const std::string& familyName,
            QuantLib::Natural fixingDays,
            const QuantLib::Currency& source,
            const QuantLib::Currency& target,
            const QuantLib::Calendar& fixingCalendar,
            const QuantLib::Handle<QuantLib::YieldTermStructure>& sourceYts
                = QuantLib::Handle<QuantLib::YieldTermStructure>(),
            const QuantLib::Handle<QuantLib::YieldTermStructure>& targetYts
                = QuantLib::Handle<QuantLib::YieldTermStructure>());
    FxIndex(const std::string& familyName,
            QuantLib::Natural fixingDays,
            const QuantLib::Currency& source,
            const QuantLib::Currency& target,
            const QuantLib::Calendar& fixingCalendar,
            const QuantLib::Handle<QuantLib::Quote> fxQuote,
            const QuantLib::Handle<QuantLib::YieldTermStructure>& sourceYts
                = QuantLib::Handle<QuantLib::YieldTermStructure>(),
            const QuantLib::Handle<QuantLib::YieldTermStructure>& targetYts
                = QuantLib::Handle<QuantLib::YieldTermStructure>());
    std::string familyName() const;
    QuantLib::Natural fixingDays() const;
    QuantLib::Date fixingDate(const QuantLib::Date& valueDate) const;
    const QuantLib::Currency& sourceCurrency() const;
    const QuantLib::Currency& targetCurrency() const;
    virtual QuantLib::Date valueDate(const QuantLib::Date& fixingDate) const;
    QuantLib::Real forecastFixing(const QuantLib::Date& fixingDate) const;
    QuantLib::Real pastFixing(const QuantLib::Date& fixingDate) const;
    QuantLib::Real fixing(const Date& fixingDate, bool forecastTodaysFixing = false) const override;
};

// QuantExt Commodity Index
%shared_ptr(CommodityIndex)
class CommodityIndex : public Index {
    private:
      CommodityIndex();
    public:
        CommodityIndex(const std::string& underlyingName, const QuantLib::Date& expiryDate, const Calendar& fixingCalendar,
                       const Handle<QuantExt::PriceTermStructure>& priceCurve = Handle<QuantExt::PriceTermStructure>());
        CommodityIndex(const std::string& underlyingName, const QuantLib::Date& expiryDate, const Calendar& fixingCalendar,
            bool keepDays, const Handle<QuantExt::PriceTermStructure>& priceCurve = Handle<QuantExt::PriceTermStructure>());
        std::string underlyingName() const;
        const Handle<QuantExt::PriceTermStructure>& priceCurve() const;
        QuantLib::Real fixing(const Date& fixingDate, bool forecastTodaysFixing = false) const;
        //virtual Real forecastFixing(const QuantLib::Date& fixingDate) const;
        //virtual Real pastFixing(const QuantLib::Date& fixingDate) const;
        ext::shared_ptr<CommodityIndex> clone(const QuantLib::Date& expireDate = QuantLib::Date(),
                                              const QuantLib::Date& optionExpiryDate = QuantLib::Date(),
                                              const boost::optional<QuantLib::Handle<PriceTermStructure>>& ts = boost::mpme) const = 0;
};

// QuantExt Commodity Spot Index
%shared_ptr(CommoditySpotIndex)
class CommoditySpotIndex : public CommodityIndex {
  public:
    CommoditySpotIndex(const std::string& underlyingName, const Calendar& fixingCalendar,
                   const Handle<QuantExt::PriceTermStructure>& priceCurve = Handle<QuantExt::PriceTermStructure>());
    ext::shared_ptr<CommodityIndex> clone(const QuantLib::Date& expiryDate = QuantLib::Date(),
                                          const QuantLib::Date& optionExpiryDate = QuantLib::Date(),
                                          const boost::optional<QuantLib::Handle<PriceTermStructure>>& ts = boost::none) const;
};

// QuantExt Commodity Futures Index
%shared_ptr(CommodityFuturesIndex)
class CommodityFuturesIndex : public CommodityIndex {
public:
    CommodityFuturesIndex(
        const std::string& underlyingName, const Date& expiryDate, const Calendar& fixingCalendar,
        const Handle<QuantExt::PriceTermStructure>& priceCurve = Handle<QuantExt::PriceTermStructure>());

    CommodityFuturesIndex(
        const std::string& underlyingName, const Date& expiryDate, const Calendar& fixingCalendar, bool keepDays,
        const Handle<QuantExt::PriceTermStructure>& priceCurve = Handle<QuantExt::PriceTermStructure>());

    ext::shared_ptr<CommodityIndex> clone(const QuantLib::Date& expiryDate = QuantLib::Date(),
        const QuantLib::Date& optionExpiryDate = QuantLib::Date(),
        const boost::optional<QuantLib::Handle<PriceTermStructure>>& ts = boost::none) const;
};

// QuantLib BMA Index (not yet wrapped in QL v1.14)
%shared_ptr(BMAIndex)
class BMAIndex : public InterestRateIndex {
  public:
    BMAIndex(const QuantLib::Handle<QuantLib::YieldTermStructure>& h =
        QuantLib::Handle<QuantLib::YieldTermStructure>());
    std::string name() const;
    bool isValidFixingDate(const Date& fixingDate) const;
    QuantLib::Handle<QuantLib::YieldTermStructure> forwardingTermStructure() const;
    QuantLib::Date maturityDate(const Date& valueDate) const;
    QuantLib::Schedule fixingSchedule(const QuantLib::Date& start, const QuantLib::Date& end);
};

%shared_ptr(BMAIndexWrapper)
class BMAIndexWrapper : public IborIndex {
  public:
    BMAIndexWrapper(const ext::shared_ptr<BMAIndex> bma);
    std::string name() const;
    bool isValidFixingDate(const Date& fixingDate) const;
    QuantLib::Handle<QuantLib::YieldTermStructure> forwardingTermStructure() const;
    QuantLib::Date maturityDate(const Date& valueDate) const;
    QuantLib::Schedule fixingSchedule(const QuantLib::Date& start, const QuantLib::Date& end);
    ext::shared_ptr<BMAIndex> bma() const;
};


%define qle_export_xibor_instance(Name)
%{
using QuantExt::Name;
%}
%shared_ptr(Name)
class Name : public IborIndex {
  public:
    Name(const Period& tenor,
         const Handle<YieldTermStructure>& h =
                                Handle<YieldTermStructure>());
};
%enddef

%define qle_export_overnight_instance(Name)
%{
using QuantExt::Name;
%}
%shared_ptr(Name)
class Name : public OvernightIndex {
  public:
    Name(const Handle<YieldTermStructure>& h =
                                Handle<YieldTermStructure>());
};
%enddef

qle_export_xibor_instance(CZKPribor);
qle_export_xibor_instance(DEMLibor);
qle_export_xibor_instance(DKKCibor);
qle_export_xibor_instance(HKDHibor);
qle_export_xibor_instance(HUFBubor);
qle_export_xibor_instance(IDRIdrfix);
qle_export_xibor_instance(INRMifor);
qle_export_xibor_instance(KRWKoribor);
qle_export_xibor_instance(MXNTiie);
qle_export_xibor_instance(MYRKlibor);
qle_export_xibor_instance(NOKNibor);
qle_export_xibor_instance(NZDBKBM);
qle_export_xibor_instance(PHPPhiref);
qle_export_xibor_instance(SEKStibor);
qle_export_xibor_instance(SGDSibor);
qle_export_xibor_instance(SGDSor);
qle_export_xibor_instance(SKKBribor);
qle_export_xibor_instance(THBBibor);
qle_export_xibor_instance(TWDTaibor);

qle_export_overnight_instance(BRLCdi);
qle_export_overnight_instance(CHFTois);
qle_export_overnight_instance(CLPCamara);
qle_export_overnight_instance(COPIbr);
qle_export_overnight_instance(CORRA);
qle_export_overnight_instance(DKKOis);
qle_export_overnight_instance(SEKSior);
qle_export_overnight_instance(Tonar);

#endif
