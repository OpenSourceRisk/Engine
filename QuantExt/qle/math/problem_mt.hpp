/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2007 Ferdinando Ametrano
 Copyright (C) 2007 Fran�ois du Vignaud
 Copyright (C) 2001, 2002, 2003 Nicolas Di C�sar�

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

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
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

/*! \file problem_mt.hpp
    \brief Abstract optimization problem class
    (for multithreaded optimization methods)
*/

#ifndef quantext_optimization_problem_mt_h
#define quantext_optimization_problem_mt_h

#include <ql/math/optimization/method.hpp>
#include <ql/math/optimization/costfunction.hpp>

namespace QuantLib {
    class Constraint;
}

namespace QuantExt {
using namespace QuantLib;

    //! Constrained optimization problem
    /*! \warning The passed Constraint instances are
                 stored by reference.  The user of this class must
                 make sure that they are not destroyed before the
                 Problem instance.
    */
    class Problem_MT {
      public:
        /*! The requirement is that the cost functions can be evaluated
          from multiple threads without data races, which can e.g. be done
          by complete separation of the underlying data. Note that some
          methods in this class are *not* thread safe, namely
          reset, setCurrentValue, setFunctionValue, setGradientNormValue,
          i.e. those can not be used from several threads in a mt optimizer */
        Problem_MT(const std::vector<QuantLib::ext::shared_ptr<CostFunction>>& costFunctions,
                Constraint& constraint,
                const Array& initialValue = Array())
        : costFunctions_(costFunctions), constraint_(constraint),
          currentValue_(initialValue), functionEvaluation_(costFunctions.size(),0),
          gradientEvaluation_(costFunctions.size(),0) {}

        /*! \warning it does not reset the current minumum to any initial value
        */
        void reset();

        //! call cost function i computation and increment evaluation counter
        Real value(const Size i, const Array& x);

        //! call cost values i computation and increment evaluation counter
        Array values(const Size i, const Array& x);

        //! call cost function i gradient computation and increment
        //  evaluation counter
        void gradient(const Size i, Array& grad_f,
                      const Array& x);

        //! call cost function i computation and it gradient
        Real valueAndGradient(const Size i, Array& grad_f,
                              const Array& x);

        //! Constraint
        Constraint& constraint() const { return constraint_; }

        //! number of available independent cost functions
        Integer availableCostFunctions() const { return costFunctions_.size(); }

        //! Cost function
        QuantLib::ext::shared_ptr<CostFunction> costFunction(const Size i) const { return costFunctions_.at(i); }

        //! Cost funcionts
        const std::vector<QuantLib::ext::shared_ptr<CostFunction>>& costFunctions() const { return costFunctions_; }

        void setCurrentValue(const Array& currentValue) {
            currentValue_=currentValue;
        }

        //! current value of the local minimum
        const Array& currentValue() const { return currentValue_; }

        void setFunctionValue(Real functionValue) {
            functionValue_=functionValue;
        }

        //! value of cost function
        Real functionValue() const { return functionValue_; }

        void setGradientNormValue(Real squaredNorm) {
            squaredNorm_=squaredNorm;
        }
        //! value of cost function gradient norm
        Real gradientNormValue() const { return squaredNorm_; }

        //! number of evaluation of cost function
        Integer functionEvaluation() const;

        //! number of evaluation of cost function gradient
        Integer gradientEvaluation() const;

      protected:
        //! Unconstrained cost function
        std::vector<QuantLib::ext::shared_ptr<CostFunction>> costFunctions_;
        //! Constraint
        Constraint& constraint_;
        //! current value of the local minimum
        Array currentValue_;
        //! function and gradient norm values at the curentValue_ (i.e. the last step)
        Real functionValue_, squaredNorm_;
        //! number of evaluation of cost function and its gradient
        std::vector<Integer> functionEvaluation_, gradientEvaluation_;
    };

    // inline definitions
    inline Real Problem_MT::value(const Size i, const Array& x) {
        ++functionEvaluation_.at(i);
        return costFunctions_.at(i)->value(x);
    }

    inline Array Problem_MT::values(const Size i, const Array& x) {
        ++functionEvaluation_.at(i);
        return costFunctions_.at(i)->values(x);
    }

    inline void Problem_MT::gradient(const Size i, Array& grad_f, const Array& x) {
        ++gradientEvaluation_.at(i);
        costFunctions_[i]->gradient(grad_f, x);
    }

    inline Real Problem_MT::valueAndGradient(const Size i, Array& grad_f,
                                             const Array& x) {
        ++functionEvaluation_.at(i);
        ++gradientEvaluation_.at(i);
        return costFunctions_[i]->valueAndGradient(grad_f, x);
    }

    inline void Problem_MT::reset() {
        for(Size i=0;i<costFunctions_.size();++i) {
            functionEvaluation_.at(i) = gradientEvaluation_.at(i) = 0;
        }
        functionValue_ = squaredNorm_ = Null<Real>();
    }

    inline Integer Problem_MT::functionEvaluation() const {
        Integer tmp = 0;
        for (Size i = 0; i < costFunctions_.size(); ++i)
            tmp += functionEvaluation_.at(i);
        return tmp;
    }

    inline Integer Problem_MT::gradientEvaluation() const {
        Integer tmp = 0;
        for (Size i = 0; i < costFunctions_.size(); ++i)
            tmp += gradientEvaluation_.at(i);
        return tmp;
    }

} 

#endif
