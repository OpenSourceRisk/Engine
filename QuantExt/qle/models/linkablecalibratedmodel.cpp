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

#include <ql/math/optimization/problem.hpp>
#include <ql/math/optimization/projectedconstraint.hpp>
#include <ql/math/optimization/projection.hpp>
#include <ql/utilities/null_deleter.hpp>
#include <qle/models/linkablecalibratedmodel.hpp>

using QuantLib::ext::shared_ptr;
using std::vector;

namespace QuantExt {

LinkableCalibratedModel::LinkableCalibratedModel()
    : constraint_(new PrivateConstraint(arguments_)), endCriteria_(EndCriteria::None) {}

class LinkableCalibratedModel::CalibrationFunction : public CostFunction {
public:
    CalibrationFunction(LinkableCalibratedModel* model, const vector<shared_ptr<CalibrationHelper> >& h,
                        const vector<Real>& weights, const Projection& projection)
        : model_(model, null_deleter()), instruments_(h), weights_(weights), projection_(projection) {}

    virtual ~CalibrationFunction() {}

    virtual Real value(const Array& params) const override {
        model_->setParams(projection_.include(params));
        Real value = 0.0;
        for (Size i = 0; i < instruments_.size(); i++) {
            Real diff = instruments_[i]->calibrationError();
            value += diff * diff * weights_[i];
        }
        return std::sqrt(value);
    }

    virtual Array values(const Array& params) const override {
        model_->setParams(projection_.include(params));
        Array values(instruments_.size());
        for (Size i = 0; i < instruments_.size(); i++) {
            values[i] = instruments_[i]->calibrationError() * std::sqrt(weights_[i]);
        }
        return values;
    }

    virtual Real finiteDifferenceEpsilon() const override { return 1e-6; }

private:
    shared_ptr<LinkableCalibratedModel> model_;
    const vector<shared_ptr<CalibrationHelper> >& instruments_;
    vector<Real> weights_;
    const Projection projection_;
};

void LinkableCalibratedModel::calibrate(const vector<ext::shared_ptr<BlackCalibrationHelper> >& instruments,
                                        OptimizationMethod& method, const EndCriteria& endCriteria,
                                        const Constraint& additionalConstraint, const vector<Real>& weights,
                                        const vector<bool>& fixParameters) {
    vector<QuantLib::ext::shared_ptr<CalibrationHelper> > tmp(instruments.size());
    for (Size i = 0; i < instruments.size(); ++i)
        tmp[i] = ext::static_pointer_cast<CalibrationHelper>(instruments[i]);
    calibrate(tmp, method, endCriteria, additionalConstraint, weights, fixParameters);
}

void LinkableCalibratedModel::calibrate(const vector<shared_ptr<CalibrationHelper> >& instruments,
                                        OptimizationMethod& method, const EndCriteria& endCriteria,
                                        const Constraint& additionalConstraint, const vector<Real>& weights,
                                        const vector<bool>& fixParameters) {

    QL_REQUIRE(weights.empty() || weights.size() == instruments.size(), "mismatch between number of instruments ("
                                                                            << instruments.size() << ") and weights("
                                                                            << weights.size() << ")");

    Constraint c;
    if (additionalConstraint.empty())
        c = *constraint_;
    else
        c = CompositeConstraint(*constraint_, additionalConstraint);
    vector<Real> w = weights.empty() ? vector<Real>(instruments.size(), 1.0) : weights;

    Array prms = params();
    vector<bool> all(prms.size(), false);
    Projection proj(prms, fixParameters.size() > 0 ? fixParameters : all);
    CalibrationFunction f(this, instruments, w, proj);
    ProjectedConstraint pc(c, proj);
    Problem prob(f, pc, proj.project(prms));
    endCriteria_ = method.minimize(prob, endCriteria);
    Array result(prob.currentValue());
    setParams(proj.include(result));
    problemValues_ = prob.values(result);

    notifyObservers();
}

Real LinkableCalibratedModel::value(const Array& params,
                                    const vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper> >& instruments) {
    vector<ext::shared_ptr<CalibrationHelper> > tmp(instruments.size());
    for (Size i = 0; i < instruments.size(); ++i)
        tmp[i] = ext::static_pointer_cast<CalibrationHelper>(instruments[i]);
    return value(params, tmp);
}

Real LinkableCalibratedModel::value(const Array& params, const vector<shared_ptr<CalibrationHelper> >& instruments) {
    vector<Real> w = vector<Real>(instruments.size(), 1.0);
    Projection p(params);
    CalibrationFunction f(this, instruments, w, p);
    return f.value(params);
}

Array LinkableCalibratedModel::params() const {
    Size size = 0, i;
    for (i = 0; i < arguments_.size(); i++)
        size += arguments_[i]->size();
    Array params(size);
    Size k = 0;
    for (i = 0; i < arguments_.size(); i++) {
        for (Size j = 0; j < arguments_[i]->size(); j++, k++) {
            params[k] = arguments_[i]->params()[j];
        }
    }
    return params;
}

void LinkableCalibratedModel::setParams(const Array& params) {
    Array::const_iterator p = params.begin();
    for (Size i = 0; i < arguments_.size(); ++i) {
        for (Size j = 0; j < arguments_[i]->size(); ++j, ++p) {
            QL_REQUIRE(p != params.end(), "parameter array too small");
            arguments_[i]->setParam(j, *p);
        }
    }
    QL_REQUIRE(p == params.end(), "parameter array too big!");
    generateArguments();
    notifyObservers();
}

void LinkableCalibratedModel::setParam(Size idx, const Real value) {
    int tmp = static_cast<int>(idx);
    for (Size i = 0; i < arguments_.size(); ++i) {
        for (Size j = 0; j < arguments_[i]->size(); ++j) {
            if (tmp-- == 0)
                arguments_[i]->setParam(j, value);
        }
    }
    generateArguments();
    notifyObservers();
}

} // namespace QuantExt
