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

/*! \file immfraratehelper.hpp
    \brief IMM FRA rate helper
*/

#ifndef quantext_immfraratehelper_hpp
#define quantext_immfraratehelper_hpp

#include <ql/termstructures/yield/ratehelpers.hpp>

using namespace QuantLib;

namespace QuantExt {

typedef RelativeDateBootstrapHelper<YieldTermStructure> RelativeDateRateHelper;

//! Rate helper for bootstrapping over %FRA rates
class ImmFraRateHelper : public RelativeDateRateHelper {
public:
    ImmFraRateHelper(const Handle<Quote>& rate,
        unsigned int& imm1, unsigned int& imm2,
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
    Date getImmDate(Date asof, int i);
    Date fixingDate_;
    unsigned int imm1_, imm2_;
    Pillar::Choice pillarChoice_;
    boost::shared_ptr<IborIndex> iborIndex_;
    RelinkableHandle<YieldTermStructure> termStructureHandle_;
};

};

#endif