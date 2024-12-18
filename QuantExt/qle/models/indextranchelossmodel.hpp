/*
 Copyright (C) 2024 AcadiaSoft Inc.
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

#pragma once

#include <functional>
#include <numeric>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/randomnumbers/inversecumulativerng.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>
#include <qle/models/defaultlossmodel.hpp>
#include <ored/utilities/log.hpp>

/* Intended to replace LossDistribution in
    ql/experimental/credit/lossdistribution, not sure its covering all the
    functionality (see mthod below)
*/

namespace QuantExt {
using namespace QuantLib;

/*! Model Class for 1 factor gaussian copula loss model
using monte carlo simulation
*/
class IndexTrancheLossModel : public LazyObject {
public:
        IndexTrancheLossModel(const std::vector<std::string>& names, 
                              const std::map<std::string, Handle<DefaultTermStructure> defaultCurves,
                              const std::map<std::string, double> notionals,
                              const std::map<std::string, double> recoveryRates,
                              Handle<Quote> baseCorrelation,
                              const double attachmentPoint,
                              const double detachmentPoint);
        
        //! To implement for each model
        Real expectedTrancheLoss(const Date& d, const double attachmentPoint, const double detachmentPoint,
                                Real recoveryRate = Null<Real>()) const = 0;

protected:

    Handle<Quote> baseCorrelation_;
    std::vector<std::vector<double>> recoveryRates_;
    std::vector<double> recoveryProbabilities_;
    std::vector<double> cumRecoveryProbabilities_;
    mutable std::map<QuantLib::Date, double> expectedTrancheLoss_;
    mutable std::map<QuantLib::Date, double> expectedTrancheLossZeroRecovery_;
    size_t nSamples_;
};

} // namespace QuantExt
