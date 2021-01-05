/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file qle/indexes/bondindex.hpp
    \brief bond index class representing historical and forward bond prices
    \ingroup indexes
*/

#pragma once

#include <ql/handle.hpp>
#include <ql/index.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

namespace QuantExt {

using namespace QuantLib;

class DiscountingRiskyBondEngine;

//! Bond Index
/*! \ingroup indexes */
class BondIndex : public Index, public Observer {
public:
    /*! The values that this index return are of the form

        - 1.02 meaning 102% price clean or dirty (depending on the flag dirty in the ctor) i.e.
          the absolute bond clean or dirty NPV is divided by the current notional at the
          fixing date
        - 10020 meaning an absolute NPV in terms of the current notional of the underlying bond
          at the fixing date, again clean or dirty depending on the flag dirty in the ctor,
          here the notional would be 10000

        The first form is returned if the flag relative in the ctor is set to true, the second
        if this flag is set to false.

        The fixing projection (fixingDate > today) assumes that the given bond is vanilla,
        i.e. its present value can be calculated by discounting the cashflows retrieved
        with Bond::cashflows().

        If the bond has a pricing engine attached and today's fixing is projected, the
        pricing engine's result will be used. Otherwise today's fixing will be calcuated
        as projected fixings for dates > today, i.e. by simply discounting the bond's
        cashflows.

        If no bond is given, only historical fixings are returned by the index and only the
        clean price mode and relative price mode are supported respectively. Otherwise
        an exception is thrown whenever a fixing is requested from the index.

        To compute projected fixings for dates > today, a discountCurve is required. The
        other quotes and curves are optional and default as follows:
        - defaultCurve: defaults to zero hazard spread
        - recoveryRate: defaults to zero
        - securitySpread: defaults to zero
        - incomCurve: defaults to the curve build as discountCurve + securitySpread

        If conditionalOnSurvival is set to true, a projected fixing will be conditional
        on survival until the associated bond settlement date, otherwise it will include
        the default probability between today and the settlement date.

    */
    BondIndex(const std::string& securityName, const bool dirty = false, const bool relative = true,
              const Calendar& fixingCalendar = NullCalendar(), const boost::shared_ptr<QuantLib::Bond>& bond = nullptr,
              const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
              const Handle<DefaultProbabilityTermStructure>& defaultCurve = Handle<DefaultProbabilityTermStructure>(),
              const Handle<Quote>& recoveryRate = Handle<Quote>(),
              const Handle<Quote>& securitySpread = Handle<Quote>(),
              const Handle<YieldTermStructure>& incomeCurve = Handle<YieldTermStructure>(),
              const bool conditionalOnSurvival = true);

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

    //! \name Fixing calculations
    //@{
    virtual Rate forecastFixing(const Date& fixingDate) const;
    Rate pastFixing(const Date& fixingDate) const;
    //@}

    //! \name Inspectors
    //@{
    const std::string& securityName() const { return securityName_; }
    bool dirty() const { return dirty_; }
    bool relative() const { return relative_; }
    boost::shared_ptr<QuantLib::Bond> bond() const { return bond_; }
    Handle<YieldTermStructure> discountCurve() const { return discountCurve_; }
    Handle<DefaultProbabilityTermStructure> defaultCurve() const { return defaultCurve_; }
    Handle<Quote> recoveryRate() const { return recoveryRate_; }
    Handle<Quote> securitySpread() const { return securitySpread_; }
    Handle<YieldTermStructure> incomeCurve() const { return incomeCurve_; }
    bool conditionalOnSurvival() const { return conditionalOnSurvival_; }
    //@}


protected:
    std::string securityName_;
    bool dirty_, relative_;
    Calendar fixingCalendar_;
    boost::shared_ptr<QuantLib::Bond> bond_;
    Handle<YieldTermStructure> discountCurve_;
    Handle<DefaultProbabilityTermStructure> defaultCurve_;
    Handle<Quote> recoveryRate_;
    Handle<Quote> securitySpread_;
    Handle<YieldTermStructure> incomeCurve_;
    bool conditionalOnSurvival_;

    boost::shared_ptr<DiscountingRiskyBondEngine> vanillaBondEngine_;
};

//! Bond Futures Index
/*! \ingroup indexes */
class BondFuturesIndex : public BondIndex {
public:
    
    BondFuturesIndex(const QuantLib::Date& expiryDate, const std::string& securityName, const bool dirty = false, const bool relative = true,
              const Calendar& fixingCalendar = NullCalendar(), const boost::shared_ptr<QuantLib::Bond>& bond = nullptr,
              const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
              const Handle<DefaultProbabilityTermStructure>& defaultCurve = Handle<DefaultProbabilityTermStructure>(),
              const Handle<Quote>& recoveryRate = Handle<Quote>(),
              const Handle<Quote>& securitySpread = Handle<Quote>(),
              const Handle<YieldTermStructure>& incomeCurve = Handle<YieldTermStructure>(),
              const bool conditionalOnSurvival = true);

    //! \name Index interface
    //@{
    std::string name() const override;
    //@}

    //! \name Fixing calculations
    //@{
    Rate forecastFixing(const Date& fixingDate) const override;
    //@}

    //! \name Inspectors
    //@{
    const QuantLib::Date& expiryDate() const { return expiryDate_; }
    //@}

private:
    Date expiryDate_;
    mutable std::string name_;
};

} // namespace QuantExt
