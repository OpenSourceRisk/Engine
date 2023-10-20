/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2006, 2007 Ferdinando Ametrano
 Copyright (C) 2001, 2002, 2003 Nicolas Di C�sar�
 Copyright (C) 2007 Fran�ois du Vignaud

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

/*! \file method_mt.hpp
    \brief Abstract multithreaded optimization method class
*/

#ifndef quantext_optimization_method_mt_h
#define quantext_optimization_method_mt_h

#include <ql/math/optimization/endcriteria.hpp>

#include <qle/math/problem_mt.hpp>

namespace QuantExt {
using namespace QuantLib;

    class Problem_MT;

    //! Abstract class for constrained optimization method

    // the mt variants of method and problem are separated from
    // the standard variants currently, the inheritance here at
    // least allows to have a common base class for both variants
    // of methods, but the minimize call is still not compatible
    // between mt and standard variants.

    class OptimizationMethod_MT : public OptimizationMethod {
    public:
        virtual ~OptimizationMethod_MT() {}

        //! minimize the optimization problem P
        virtual EndCriteria::Type minimize(Problem_MT& P,
                                           const EndCriteria& endCriteria) = 0;

        EndCriteria::Type minimize(Problem&, const EndCriteria&) override {
            QL_FAIL("OptimizationMethod_MT requires Problem_MT, got Problem");
        }

    };

}

#endif
