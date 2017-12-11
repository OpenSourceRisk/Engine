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

/*! \file analyticlgmcdsoptionengine.hpp
    \brief analytic lgm cds option engine
 \ingroup engines
*/

#ifndef quantext_lgm_cdsoptionengine_hpp
#define quantext_lgm_cdsoptionengine_hpp

#include <qle/instruments/cdsoption.hpp>
#include <qle/models/crossassetmodel.hpp>

namespace QuantExt {
//! analytic lgm cds option engine
//! \ingroup engines
/*! Reference: Modern Derivatives Pricing and Credit Exposure Analysis by Lichters, Stamm and Gallagher, 15.1 */
class AnalyticLgmCdsOptionEngine : public QuantExt::CdsOption::engine {
public:
    AnalyticLgmCdsOptionEngine(const boost::shared_ptr<CrossAssetModel>& model, const Size index, const Size ccy,
                               const Real recoveryRate,
                               const Handle<YieldTermStructure>& termStructure = Handle<YieldTermStructure>());
    void calculate() const;

private:
    Real Ei(const Real w, const Real strike, const Size i) const;
    Real lambdaStarHelper(const Real lambda) const;
    const boost::shared_ptr<CrossAssetModel> model_;
    const Size index_, ccy_;
    const Real recoveryRate_;
    const Handle<YieldTermStructure> termStructure_;
    mutable Array G_, t_;
    mutable Real tex_;
};

} // namespace QuantExt

#endif
