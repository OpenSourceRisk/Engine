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

/*! \file linkablecalibratedmodel.hpp
    \brief calibrated model class with linkable parameters
    \ingroup models
*/

#ifndef quantext_calibrated_model_hpp
#define quantext_calibrated_model_hpp

#include <ql/math/optimization/endcriteria.hpp>
#include <ql/math/optimization/method.hpp>
#include <ql/models/calibrationhelper.hpp>
#include <ql/models/parameter.hpp>
#include <ql/option.hpp>
#include <ql/patterns/observable.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Calibrated model class with linkable parameters
/*! \ingroup models
 */
class LinkableCalibratedModel : public virtual Observer, public virtual Observable {
public:
    LinkableCalibratedModel();

    void update() {
        generateArguments();
        notifyObservers();
    }

    //! Calibrate to a set of market instruments (usually caps/swaptions)
    /*! An additional constraint can be passed which must be
        satisfied in addition to the constraints of the model.
    */
    virtual void calibrate(const std::vector<boost::shared_ptr<CalibrationHelper> >&, OptimizationMethod& method,
                           const EndCriteria& endCriteria, const Constraint& constraint = Constraint(),
                           const std::vector<Real>& weights = std::vector<Real>(),
                           const std::vector<bool>& fixParameters = std::vector<bool>());

    Real value(const Array& params, const std::vector<boost::shared_ptr<CalibrationHelper> >&);

    const boost::shared_ptr<Constraint>& constraint() const;

    //! Returns end criteria result
    EndCriteria::Type endCriteria() const { return endCriteria_; }

    //! Returns the problem values
    const Array& problemValues() const { return problemValues_; }

    //! Returns array of arguments on which calibration is done
    Disposable<Array> params() const;

    virtual void setParams(const Array& params);

protected:
    virtual void generateArguments() {}
    std::vector<boost::shared_ptr<Parameter> > arguments_;
    boost::shared_ptr<Constraint> constraint_;
    EndCriteria::Type endCriteria_;
    Array problemValues_;

private:
    //! Constraint imposed on arguments
    class PrivateConstraint;
    //! Calibration cost function class
    class CalibrationFunction;
    friend class CalibrationFunction;
};

//! Linkable Calibrated Model
/*! \ingroup models
 */
class LinkableCalibratedModel::PrivateConstraint : public Constraint {
private:
    class Impl : public Constraint::Impl {
    public:
        Impl(const std::vector<boost::shared_ptr<Parameter> >& arguments) : arguments_(arguments) {}

        bool test(const Array& params) const {
            Size k = 0;
            for (Size i = 0; i < arguments_.size(); i++) {
                Size size = arguments_[i]->size();
                Array testParams(size);
                for (Size j = 0; j < size; j++, k++)
                    testParams[j] = params[k];
                if (!arguments_[i]->testParams(testParams))
                    return false;
            }
            return true;
        }

        Array upperBound(const Array& params) const {
            Size k = 0, k2 = 0;
            Size totalSize = 0;
            for (Size i = 0; i < arguments_.size(); i++) {
                totalSize += arguments_[i]->size();
            }
            Array result(totalSize);
            for (Size i = 0; i < arguments_.size(); i++) {
                Size size = arguments_[i]->size();
                Array partialParams(size);
                for (Size j = 0; j < size; j++, k++)
                    partialParams[j] = params[k];
                Array tmpBound = arguments_[i]->constraint().upperBound(partialParams);
                for (Size j = 0; j < size; j++, k2++)
                    result[k2] = tmpBound[j];
            }
            return result;
        }

        Array lowerBound(const Array& params) const {
            Size k = 0, k2 = 0;
            Size totalSize = 0;
            for (Size i = 0; i < arguments_.size(); i++) {
                totalSize += arguments_[i]->size();
            }
            Array result(totalSize);
            for (Size i = 0; i < arguments_.size(); i++) {
                Size size = arguments_[i]->size();
                Array partialParams(size);
                for (Size j = 0; j < size; j++, k++)
                    partialParams[j] = params[k];
                Array tmpBound = arguments_[i]->constraint().lowerBound(partialParams);
                for (Size j = 0; j < size; j++, k2++)
                    result[k2] = tmpBound[j];
            }
            return result;
        }

    private:
        const std::vector<boost::shared_ptr<Parameter> >& arguments_;
    };

public:
    PrivateConstraint(const std::vector<boost::shared_ptr<Parameter> >& arguments)
        : Constraint(boost::shared_ptr<Constraint::Impl>(new PrivateConstraint::Impl(arguments))) {}
};

} // namespace QuantExt

#endif
