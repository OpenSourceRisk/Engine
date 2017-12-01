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
    \brief
*/

#ifndef quantext_inflation_index_observer_hpp
#define quantext_inflation_index_observer_hpp

class InflationIndexObserver : public Observer, public Observable {
public:
    InflationIndexObserver(const boost::shared_ptr<InflationIndex>& index, const Handle<Quote>& quote, 
                           const Date& baseDate, const Period& observationLag)
        : index_(index), quote_(quote), baseDate_(baseDate), observationLag_(observationLag) {
        registerWith(quote);
    }

    void update() { // called when the quote changes
        setFixing();
    }

private:
    void setFixing() {
        // something like this
        Date today = Settings::instance().evaluationDate();
        Date fixingDate = today - observationLag_;
        // overwrite the current fixing in the QuantLib::FixingManager
        Real baseCPI = index_->fixing(baseDate_);
        index_->addFixing(fixingDate, baseCPI * quote_->value(), true);
    }

    boost::shared_ptr<InflationIndex> index_;
    Handle<Quote> quote_;
    Date baseDate_;
    Period observationLag_;
};

#endif