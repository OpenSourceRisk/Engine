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

/*! \file fraratehelper.hpp
    \brief FRA rate helper
*/

#ifndef quantext_fraratehelper_hpp
#define quantext_fraratehelper_hpp

#include <ql/termstructures/yield/ratehelpers.hpp>

using namespace QuantLib;

namespace QuantExt {

typedef RelativeDateBootstrapHelper<YieldTermStructure> RelativeDateRateHelper;

//! Rate helper for bootstrapping over %FRA rates
class FraRateHelper : public RelativeDateRateHelper {
public:
    FraRateHelper(const Handle<Quote>& rate,
        Period periodToStart,
        Period term,
        const boost::shared_ptr<IborIndex>& iborIndex,
        Pillar::Choice pillar = Pillar::LastRelevantDate,
        Date customPillarDate = Date());
    FraRateHelper(const Handle<Quote>& rate,
        Period periodToStart,
        const boost::shared_ptr<IborIndex>& iborIndex,
        Pillar::Choice pillar = Pillar::LastRelevantDate,
        Date customPillarDate = Date());

    //! \name RateHelper interface
    //@{
    Real impliedQuote() const;
    void setTermStructure(YieldTermStructure*);
    //@}
    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&);
    //@}

private:
    void initializeDates();
    Date fixingDate_;
    Period periodToStart_;
    Period term_;
    bool termFromIndex_ = true;
    Pillar::Choice pillarChoice_;
    boost::shared_ptr<IborIndex> iborIndex_;
    RelinkableHandle<YieldTermStructure> termStructureHandle_;
};

};

#endif