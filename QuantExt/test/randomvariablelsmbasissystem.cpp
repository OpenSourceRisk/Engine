/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <qle/math/randomvariablelsmbasissystem.hpp>

#include <ql/math/randomnumbers/haltonrsg.hpp>
#include <ql/time/date.hpp>

#include <boost/test/unit_test.hpp>

#include <set>

using namespace QuantExt;

BOOST_AUTO_TEST_SUITE(QuantExtTestSuite)

BOOST_AUTO_TEST_SUITE(RandomVariableLsmBasisSystemTest)

BOOST_AUTO_TEST_CASE(testBasisSystem) {

    BOOST_TEST_MESSAGE("Testing lsm basis system for random variables...");

    std::set<Size> dims = {1, 2, 5};
    std::set<Size> orders = {0, 1, 2, 3, 4, 5, 10};
    std::set<QuantLib::LsmBasisSystem::PolynomialType> polynomialTypes = {
        QuantLib::LsmBasisSystem::PolynomialType::Monomial,    QuantLib::LsmBasisSystem::PolynomialType::Laguerre,
        QuantLib::LsmBasisSystem::PolynomialType::Hermite,     QuantLib::LsmBasisSystem::PolynomialType::Hyperbolic,
        QuantLib::LsmBasisSystem::PolynomialType::Legendre,    QuantLib::LsmBasisSystem::PolynomialType::Chebyshev,
        QuantLib::LsmBasisSystem::PolynomialType::Chebyshev2nd};
    Size nSamplePoints = 20;

    for (auto const& dim : dims) {
        for (auto const& order : orders) {
            for (auto const& polynomialType : polynomialTypes) {

                BOOST_TEST_MESSAGE("Testing dim " << dim << ", order " << order << ", polynomial type "
                                                  << static_cast<int>(polynomialType));

                auto bsrv = RandomVariableLsmBasisSystem::multiPathBasisSystem(dim, order, polynomialType);
                auto bsref = LsmBasisSystem::multiPathBasisSystem(dim, order, polynomialType);

                BOOST_REQUIRE(bsrv.size() == bsref.size());

                Array samplePoint;
                std::vector<RandomVariable> samplePointRv(dim);
                std::vector<const RandomVariable*> samplePointRvPtr(dim);

                HaltonRsg rsg(dim, 42);
                for (Size i = 0; i < nSamplePoints; ++i) {

                    auto tmp = rsg.nextSequence().value;
                    samplePoint = Array(tmp.begin(), tmp.end());
                    for (Size d = 0; d < dim; ++d) {
                        samplePointRv[d] = RandomVariable(1, samplePoint[d]);
                        samplePointRvPtr[d] = &samplePointRv[d];
                    }

                    std::set<Real> valuesRv;
                    std::set<Real> valuesRef;

                    for (auto const& f : bsrv) {
                        valuesRv.insert(f(samplePointRvPtr).at(0));
                    }

                    for (auto const& f : bsref) {
                        valuesRef.insert(f(samplePoint));
                    }

                    BOOST_REQUIRE(valuesRv.size() == valuesRef.size());
                    for (auto itRv = valuesRv.begin(), itRef = valuesRef.begin(); itRv != valuesRv.end();
                         ++itRv, ++itRef) {
                        BOOST_CHECK(QuantLib::close_enough(*itRv, *itRef));
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testVarGroups) {

    BOOST_TEST_MESSAGE("Testing lsm basis system for random variables with var groups...");

    auto bs = multiPathBasisSystem(5, 5, QuantLib::LsmBasisSystem::PolynomialType::Monomial, {{0, 1}, {2, 3, 4}});

    auto bs1 = multiPathBasisSystem(2, 5, QuantLib::LsmBasisSystem::PolynomialType::Monomial);
    auto bs2 = multiPathBasisSystem(3, 5, QuantLib::LsmBasisSystem::PolynomialType::Monomial);
    bs2.erase(bs2.begin()); // constant term is in bs1 and bs2, so we remove the one in bs2

    BOOST_REQUIRE(bs.size() == bs1.size() + bs2.size());

    Size nSamplePoints = 20;

    Array samplePoint;
    std::vector<RandomVariable> samplePointRv(5);
    std::vector<RandomVariable> samplePointRv1(2);
    std::vector<RandomVariable> samplePointRv2(3);
    std::vector<const RandomVariable*> samplePointRvPtr(5);
    std::vector<const RandomVariable*> samplePointRvPtr1(2);
    std::vector<const RandomVariable*> samplePointRvPtr2(3);

    HaltonRsg rsg(5, 42);
    for (Size i = 0; i < nSamplePoints; ++i) {
        auto tmp = rsg.nextSequence().value;
        samplePoint = Array(tmp.begin(), tmp.end());

        for (Size d = 0; d < 5; ++d) {
            samplePointRv[d] = RandomVariable(1, samplePoint[d]);
            samplePointRvPtr[d] = &samplePointRv[d];
        }

        for (Size d = 0; d < 2; ++d) {
            samplePointRv1[d] = RandomVariable(1, samplePoint[d]);
            samplePointRvPtr1[d] = &samplePointRv[d];
        }

        for (Size d = 0; d < 3; ++d) {
            samplePointRv2[d] = RandomVariable(1, samplePoint[d + 2]);
            samplePointRvPtr2[d] = &samplePointRv2[d];
        }

        std::set<Real> valuesRv;
        std::set<Real> valuesRv12;

        for (auto const& f : bs) {
            valuesRv.insert(f(samplePointRvPtr).at(0));
        }

        for (auto const& f : bs1) {
            valuesRv12.insert(f(samplePointRvPtr1).at(0));
        }
        for (auto const& f : bs2) {
            valuesRv12.insert(f(samplePointRvPtr2).at(0));
        }

        BOOST_REQUIRE(valuesRv.size() == valuesRv12.size());
        for (auto itRv = valuesRv.begin(), itRv12 = valuesRv12.begin(); itRv != valuesRv.end(); ++itRv, ++itRv12) {
            BOOST_CHECK(QuantLib::close_enough(*itRv, *itRv12));
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
