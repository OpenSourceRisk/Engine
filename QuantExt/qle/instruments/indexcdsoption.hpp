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

/*
 Copyright (C) 2008 Roland Stamm
 Copyright (C) 2009 Jose Aparicio

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file cdsoption.hpp
    \brief CDS option, removed requirements (rec must knock out,
    no upfront amount), that should be taken care of in pricing engines
*/

#ifndef quantext_index_cds_option_hpp
#define quantext_index_cds_option_hpp

#include <qle/instruments/indexcreditdefaultswap.hpp>
#include <qle/instruments/cdsoption.hpp>

#include <ql/option.hpp>

using namespace QuantLib;

namespace QuantLib {
class Quote;
class YieldTermStructure;
}

namespace QuantExt {

//! CDS option
/*! The side of the swaption is set by choosing the side of the CDS.
    A receiver CDS option is a right to buy an underlying CDS
    selling protection and receiving a coupon. A payer CDS option
    is a right to buy an underlying CDS buying protection and
    paying coupon.
*/
class IndexCdsOption : public Option {
public:
    class arguments;
    class results;
    class engine;
    IndexCdsOption(const boost::shared_ptr<IndexCreditDefaultSwap>& swap, const boost::shared_ptr<Exercise>& exercise,
                   bool knocksOut = true);

    //! \name Instrument interface
    //@{
    bool isExpired() const;
    void setupArguments(PricingEngine::arguments*) const;
    //@}
    //! \name Inspectors
    //@{
    const boost::shared_ptr<IndexCreditDefaultSwap>& underlyingSwap() const { return swap_; }
    //@}
    //! \name Calculations
    //@{
    Rate atmRate() const;
    Real riskyAnnuity() const;
    Volatility impliedVolatility(Real price, const Handle<QuantLib::YieldTermStructure>& termStructure,
                                 const Handle<DefaultProbabilityTermStructure>&, Real recoveryRate,
                                 Real accuracy = 1.e-4, Size maxEvaluations = 100, Volatility minVol = 1.0e-7,
                                 Volatility maxVol = 4.0) const;
    // TODO add impliedVolatility with probability term structures and recovery rates on underlying level
    // ...
    //@}

private:
    boost::shared_ptr<IndexCreditDefaultSwap> swap_;
    bool knocksOut_;

    mutable Real riskyAnnuity_;
    void setupExpired() const;
    void fetchResults(const PricingEngine::results*) const;
};

//! %Arguments for CDS-option calculation
class IndexCdsOption::arguments : public IndexCreditDefaultSwap::arguments, public Option::arguments {
public:
    arguments() {}

    boost::shared_ptr<IndexCreditDefaultSwap> swap;
    bool knocksOut;
    void validate() const;
};

//! %Results from CDS-option calculation
class IndexCdsOption::results : public CdsOption::results {
public:
    Real riskyAnnuity;
    void reset();
};

//! base class for swaption engines
class IndexCdsOption::engine : public GenericEngine<IndexCdsOption::arguments, IndexCdsOption::results> {};
}

#endif
