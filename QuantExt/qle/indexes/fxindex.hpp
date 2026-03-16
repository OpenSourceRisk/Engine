/*
 Copyright (C) 2015 Peter Caspers
 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/
 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program.
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.
 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file fxindex.hpp
    \brief FX index class
        \ingroup indexes
*/

#ifndef quantext_fxindex_hpp
#define quantext_fxindex_hpp

#include <ql/currency.hpp>
#include <ql/exchangerate.hpp>
#include <ql/handle.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendar.hpp>
#include <qle/indexes/eqfxindexbase.hpp>

namespace QuantExt {
using namespace QuantLib;

class FxRateQuote : public Quote, public Observer {
public:
    //! if sourceYts, targetYts are not given, the non-discounted spot quote will be returned as a fallback
    FxRateQuote(Handle<Quote> spotQuote, const Handle<YieldTermStructure>& sourceYts,
                const Handle<YieldTermStructure>& targetYts, Natural fixingDays, const Calendar& fixingCalendar);
    //! \name Quote interface
    //@{
    Real value() const override;
    bool isValid() const override;
    //@}
    //! \name Observer interface
    //@{
    void update() override;
    //@}
private:
    const Handle<Quote> spotQuote_;
    const Handle<YieldTermStructure> sourceYts_, targetYts_;
    Natural fixingDays_;
    Calendar fixingCalendar_;
};

class FxSpotQuote : public Quote, public Observer {
public:
    FxSpotQuote(Handle<Quote> todaysQuote, const Handle<YieldTermStructure>& sourceYts,
                const Handle<YieldTermStructure>& targetYts, Natural fixingDays, const Calendar& fixingCalendar);
    //! \name Quote interface
    //@{
    Real value() const override;
    bool isValid() const override;
    //@}
    //! \name Observer interface
    //@{
    void update() override;
    //@}
private:
    const Handle<Quote> todaysQuote_;
    const Handle<YieldTermStructure> sourceYts_, targetYts_;
    Natural fixingDays_;
    Calendar fixingCalendar_;
};

//! FX Index
/*! \ingroup indexes */
class FxIndex : public EqFxIndexBase {
public:
    /*! familyName may be e.g. ECB
        fixingDays determine the spot date of the currency pair
        source is the asset or foreign currency
        target is the numeraire or domestic currency
        fixingCalendar is the calendar defining good days for the pair
        this class uses the exchange rate manager to retrieve spot values
        fxSpot is the fx rate settled at today + fixingDays */
    FxIndex(const std::string& familyName, Natural fixingDays, const Currency& source, const Currency& target,
            const Calendar& fixingCalendar, const Handle<YieldTermStructure>& sourceYts = Handle<YieldTermStructure>(),
            const Handle<YieldTermStructure>& targetYts = Handle<YieldTermStructure>(),
            bool fixingTriangulation = false);
    FxIndex(const std::string& familyName, Natural fixingDays, const Currency& source, const Currency& target,
            const Calendar& fixingCalendar, const Handle<Quote> fxSpot,
            const Handle<YieldTermStructure>& sourceYts = Handle<YieldTermStructure>(),
            const Handle<YieldTermStructure>& targetYts = Handle<YieldTermStructure>(),
            bool fixingTriangulation = true);
    //! \name Index interface
    //@{
    std::string name() const override;
    Calendar fixingCalendar() const override;
    bool isValidFixingDate(const Date& fixingDate) const override;
    Real fixing(const Date& fixingDate, bool forecastTodaysFixing = false) const override;
    //@}
    //! \name Observer interface
    //@{
    void update() override;
    //@}
    //! \name Inspectors
    //@{
    std::string familyName() const { return familyName_; }
    std::string oreName() const { return oreName_; }
    Natural fixingDays() const { return fixingDays_; }
    Date fixingDate(const Date& valueDate) const;
    const Currency& sourceCurrency() const { return sourceCurrency_; }
    const Currency& targetCurrency() const { return targetCurrency_; }
    const Handle<YieldTermStructure>& sourceCurve() const { return sourceYts_; }
    const Handle<YieldTermStructure>& targetCurve() const { return targetYts_; }

    //! fxQuote returns instantaneous Quote by default, otherwise settlement after fixingDays
    const Handle<Quote> fxQuote(bool withSettlementLag = false) const;
    const bool useQuote() const { return useQuote_; }
    //@}
    /*! \name Date calculations */
    virtual Date valueDate(const Date& fixingDate) const;
    //! \name Fixing calculations
    //@{
    //! It can be overridden to implement particular conventions
    virtual Real forecastFixing(const Time& fixingTime) const override;
    virtual Real forecastFixing(const Date& fixingDate) const;
    Real pastFixing(const Date& fixingDate) const override;
    // @}
    
    //! clone the index, the clone will be linked to the provided handles
    QuantLib::ext::shared_ptr<FxIndex> clone(const Handle<Quote> fxQuote = Handle<Quote>(),
                                     const Handle<YieldTermStructure>& sourceYts = Handle<YieldTermStructure>(),
                                     const Handle<YieldTermStructure>& targetYts = Handle<YieldTermStructure>(),
                                     const std::string& familyName = std::string());

protected:
    std::string familyName_, oreName_;
    Natural fixingDays_;
    Currency sourceCurrency_, targetCurrency_;
    const Handle<YieldTermStructure> sourceYts_, targetYts_;
    std::string name_;
    // Spot as quoted in market
    const Handle<Quote> fxSpot_;
    // instantaneous fx rate
    mutable Handle<Quote> fxRate_;
    bool useQuote_;

private:
    Calendar fixingCalendar_;
    bool fixingTriangulation_;

    void initialise();
};

} // namespace QuantExt

#endif
