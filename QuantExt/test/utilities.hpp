/*
 Copyright (C) 2003, 2004, 2008 StatPro Italia srl
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

#ifndef quantext_test_utilities_hpp
#define quantext_test_utilities_hpp

// This makes it easier to use array literals
#define LENGTH(a) (sizeof(a)/sizeof(a[0]))


#define QUANTLIB_TEST_CASE(f) BOOST_TEST_CASE(QuantLib::detail::quantlib_test_case(f))

namespace QuantLib {

    namespace detail {

        class quantlib_test_case {
            boost::function0<void> test_;
          public:
            template <class F>
            quantlib_test_case(F test) : test_(test) {}
            void operator()() const {
                Date before = Settings::instance().evaluationDate();
                BOOST_CHECK(true);
                test_();
                Date after = Settings::instance().evaluationDate();
                if (before != after)
                    BOOST_ERROR("Evaluation date not reset"
                                << "\n  before: " << before
                                << "\n  after:  " << after);
            }
        };

    }

}


#endif

