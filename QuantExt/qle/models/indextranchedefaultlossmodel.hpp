/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <map>
#include <ql/handle.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>
#include <vector>
/* Intended to replace LossDistribution in
    ql/experimental/credit/lossdistribution, not sure its covering all the
    functionality (see mthod below)
*/

namespace QuantExt {
using namespace QuantLib;

/*! Default loss model, get rid of the basket and pool, attach it to the model
 */
class CreditIndexConstituentCalibration : public LazyObject {
public:
    CreditIndexConstituentCalibration(const std::vector<ext::shared_ptr<DefaultProbabilityTermStructure>>& dpts)
        : uncalibratedCurves_(dpts) {
        for (const auto& creditCurve : uncalibratedCurves_) {
            registerWithObservables(creditCurve);
        }
    };

    const std::vector<ext::shared_ptr<DefaultProbabilityTermStructure>>& creditCurves() const {
        calculate();
        return calibratedCurves_;
    }

    std::vector<double> defaultProbabilites(const Date& d){
        calculate();
        return pds[d];
    }

    

protected:
    std::vector<ext::shared_ptr<DefaultProbabilityTermStructure>> uncalibratedCurves_;
    mutable std::vector<ext::shared_ptr<DefaultProbabilityTermStructure>> calibratedCurves_;
    mutable std::map<QuantLib::Date, std::vector<double>> pds;
};

class NoCreditIndexConstituentCalibration : public CreditIndexConstituentCalibration {
public:
    NoCreditIndexConstituentCalibration(const std::vector<ext::shared_ptr<DefaultProbabilityTermStructure>>& dpts)
        : CreditIndexConstituentCalibration(dpts){};

protected:
    void performCalculations() const override { calibratedCurves_ = uncalibratedCurves_; }
};

class IndexTrancheDefaultLossModel : public LazyObject {

public:
    IndexTrancheDefaultLossModel(ext::shared_ptr<CreditIndexConstituentCalibration>& creditCurves,
                                 RelinkableHandle<Quote> baseCorrelation, std::vector<double>& notionals,
                                 double adjustedAttachPoint, double adjustedDetachPoint)
        : creditCurves_(creditCurves), baseCorrelation_(baseCorrelation), notionals_(notionals),
          adjustedAttachPoint_(adjustedAttachPoint), adjustedDetachPoint_(adjustedDetachPoint) {

        inceptionTrancheNotional_ = 0;
        for (const auto& n : notionals_) {
            inceptionTrancheNotional_ += n * (adjustedDetachPoint_ - adjustedAttachPoint_);
        }
        registerWithObservables(creditCurves_);
        registerWith(baseCorrelation_);
    }

    double inceptionTrancheNotional() const { return inceptionTrancheNotional_; };

    virtual Real expectedTrancheLoss(const Date& d, Real recoveryRate = Null<Real>()) const {
        calculate();
        return expectedTrancheLoss_[d];
    };

protected:
    ext::shared_ptr<CreditIndexConstituentCalibration> creditCurves_;
    RelinkableHandle<Quote> baseCorrelation_;
    std::vector<double> notionals_;
    double adjustedAttachPoint_;
    double adjustedDetachPoint_;

    double inceptionTrancheNotional_;
    mutable std::map<QuantLib::Date, double> expectedTrancheLoss_;
};

} // namespace QuantExt
