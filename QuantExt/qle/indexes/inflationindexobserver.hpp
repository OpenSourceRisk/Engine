/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file qle/indexes/inflationindexobserver.hpp
    \brief inflation index observer class
    \ingroup indexes
*/

#ifndef quantext_inflation_index_observer_hpp
#define quantext_inflation_index_observer_hpp

#include <ql/indexes/inflationindex.hpp>
#include <qle/utilities/inflation.hpp>

namespace QuantExt {

//! Inflation Index observer
/*! \ingroup indexes */
class InflationIndexObserver : public TermStructure {
public:
    InflationIndexObserver(const QuantLib::Handle<ZeroInflationIndex>& index, const Handle<Quote>& quote,
                           const Size& simulationLag, const DayCounter& dayCounter = DayCounter())
        : TermStructure(dayCounter), index_(index), quote_(quote), simulationLag_(simulationLag) {
        registerWith(quote_);
        QL_REQUIRE(!index_.empty(), "index is null");
        QL_REQUIRE(!index_->zeroInflationTermStructure().empty(), "index does not have an associated zero inflation term structure");
    }

    void update() override { // called when the quote changes
        setFixing();
    }

    Date maxDate() const override {
        Date today = Settings::instance().evaluationDate();
        return today;
    }

private:
    void setFixing() {
        // something like this
        Date today = Settings::instance().evaluationDate();
        Date fixingDate = today - simulationLag_;
        auto zits = index_->zeroInflationTermStructure();
        // In the CAM we simulate the cpi without seasonality adjustment, so we need to apply the seasonality adjustment
        // here to get the correct fixing for the index. If this risk factor will be used in future for other purposes
        // than exposure simulation in CAM, we might want to make the application of seasonality adjustment optional via
        // a flag in the constructor.
        auto cpi = seasonalizeCPI(index_->zeroInflationTermStructure()->baseDate(), fixingDate, quote_->value(), zits.currentLink());
        // overwrite the current fixing in the QuantLib::FixingManager
        index_->addFixing(fixingDate, cpi, true);
    }

    QuantLib::Handle<ZeroInflationIndex> index_;
    Handle<Quote> quote_;
    Size simulationLag_;
};
} // namespace QuantExt

#endif
