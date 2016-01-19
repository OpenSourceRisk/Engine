/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file linkablecalibratedmodel.hpp
    \brief calibrated model class with linkable parameters
*/

#ifndef quantext_calibrated_model_hpp
#define quantext_calibrated_model_hpp

#include <ql/patterns/observable.hpp>
#include <ql/option.hpp>
#include <ql/math/optimization/method.hpp>
#include <ql/models/parameter.hpp>
#include <ql/models/calibrationhelper.hpp>
#include <ql/math/optimization/endcriteria.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! Calibrated model class with linkable parameters
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
        virtual void calibrate(
                const std::vector<boost::shared_ptr<CalibrationHelper> >&,
                OptimizationMethod& method,
                const EndCriteria& endCriteria,
                const Constraint& constraint = Constraint(),
                const std::vector<Real>& weights = std::vector<Real>(),
                const std::vector<bool>& fixParameters = std::vector<bool>());

        Real value(const Array& params,
                   const std::vector<boost::shared_ptr<CalibrationHelper> >&);

        const boost::shared_ptr<Constraint>& constraint() const;

        //! Returns end criteria result
        EndCriteria::Type endCriteria() const { return shortRateEndCriteria_; }

        //! Returns array of arguments on which calibration is done
        Disposable<Array> params() const;

        virtual void setParams(const Array& params);

      protected:
        virtual void generateArguments() {}
        std::vector<boost::shared_ptr<Parameter> > arguments_;
        boost::shared_ptr<Constraint> constraint_;
        EndCriteria::Type shortRateEndCriteria_;

      private:
        //! Constraint imposed on arguments
        class PrivateConstraint;
        //! Calibration cost function class
        class CalibrationFunction;
        friend class CalibrationFunction;
    };


    class LinkableCalibratedModel::PrivateConstraint : public Constraint {
      private:
        class Impl :  public Constraint::Impl {
          public:
            Impl(const std::vector<boost::shared_ptr<Parameter> >& arguments)
            : arguments_(arguments) {}

            bool test(const Array& params) const {
                Size k=0;
                for (Size i=0; i<arguments_.size(); i++) {
                    Size size = arguments_[i]->size();
                    Array testParams(size);
                    for (Size j=0; j<size; j++, k++)
                        testParams[j] = params[k];
                    if (!arguments_[i]->testParams(testParams))
                        return false;
                }
                return true;
            }

            Array upperBound(const Array &params) const {
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
                    Array tmpBound =
                        arguments_[i]->constraint().upperBound(partialParams);
                    for (Size j = 0; j < size; j++, k2++)
                        result[k2] = tmpBound[j];
                }
                return result;
            }

            Array lowerBound(const Array &params) const {
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
                    Array tmpBound =
                        arguments_[i]->constraint().lowerBound(partialParams);
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
        : Constraint(boost::shared_ptr<Constraint::Impl>(
                                   new PrivateConstraint::Impl(arguments))) {}
    };

} // namespace QuantExt

#endif
