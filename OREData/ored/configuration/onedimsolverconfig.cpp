/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <ored/configuration/onedimsolverconfig.hpp>
#include <ored/utilities/parsers.hpp>

using QuantExt::Solver1DOptions;
using namespace QuantLib;
using std::make_pair;
using std::pair;

namespace ore {
namespace data {

OneDimSolverConfig::OneDimSolverConfig()
    : maxEvaluations_(Null<Size>()),
      initialGuess_(Null<Real>()),
      accuracy_(Null<Real>()),
      minMax_(make_pair(Null<Real>(), Null<Real>())),
      step_(Null<Real>()),
      lowerBound_(Null<Real>()),
      upperBound_(Null<Real>()),
      empty_(true) {}

OneDimSolverConfig::OneDimSolverConfig(Size maxEvaluations,
    Real initialGuess,
    Real accuracy,
    const pair<Real, Real>& minMax,
    Real lowerBound,
    Real upperBound)
    : maxEvaluations_(maxEvaluations),
      initialGuess_(initialGuess),
      accuracy_(accuracy),
      minMax_(minMax),
      step_(Null<Real>()),
      lowerBound_(lowerBound),
      upperBound_(upperBound),
      empty_(false) {
    check();
}

OneDimSolverConfig::OneDimSolverConfig(Size maxEvaluations,
    Real initialGuess,
    Real accuracy,
    Real step,
    Real lowerBound,
    Real upperBound)
    : maxEvaluations_(maxEvaluations),
      initialGuess_(initialGuess),
      accuracy_(accuracy),
      minMax_(make_pair(Null<Real>(), Null<Real>())),
      step_(step),
      lowerBound_(lowerBound),
      upperBound_(upperBound),
      empty_(false) {
    check();
}

void OneDimSolverConfig::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "OneDimSolverConfig");

    maxEvaluations_ = XMLUtils::getChildValueAsInt(node, "MaxEvaluations", true);
    initialGuess_ = XMLUtils::getChildValueAsDouble(node, "InitialGuess", true);
    accuracy_ = XMLUtils::getChildValueAsDouble(node, "Accuracy", true);

    // Choice between (min, max) pair or step
    if (XMLNode* minMaxNode = XMLUtils::getChildNode(node, "MinMax")) {
        Real min = XMLUtils::getChildValueAsDouble(minMaxNode, "Min", true);
        Real max = XMLUtils::getChildValueAsDouble(minMaxNode, "Max", true);
        minMax_ = make_pair(min, max);
    } else if (XMLNode* stepNode = XMLUtils::getChildNode(node, "Step")) {
        step_ = parseReal(XMLUtils::getNodeValue(stepNode));
    } else {
        QL_FAIL("OneDimSolverConfig: expected a MinMax or Step node.");
    }

    lowerBound_ = Null<Real>();
    if (XMLNode* n = XMLUtils::getChildNode(node, "LowerBound")) {
        lowerBound_ = parseReal(XMLUtils::getNodeValue(n));
    }

    upperBound_ = Null<Real>();
    if (XMLNode* n = XMLUtils::getChildNode(node, "UpperBound")) {
        upperBound_ = parseReal(XMLUtils::getNodeValue(n));
    }

    check();

    empty_ = false;
}

XMLNode* OneDimSolverConfig::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("OneDimSolverConfig");

    XMLUtils::addChild(doc, node, "MaxEvaluations", static_cast<int>(maxEvaluations_));
    XMLUtils::addChild(doc, node, "InitialGuess", initialGuess_);
    XMLUtils::addChild(doc, node, "Accuracy", accuracy_);

    if (step_ != Null<Real>()) {
        XMLUtils::addChild(doc, node, "Step", accuracy_);
    } else {
        XMLNode* minMaxNode = doc.allocNode("MinMax");
        XMLUtils::addChild(doc, minMaxNode, "Min", minMax_.first);
        XMLUtils::addChild(doc, minMaxNode, "Max", minMax_.second);
        XMLUtils::appendNode(node, minMaxNode);
    }

    if (lowerBound_ != Null<Real>())
        XMLUtils::addChild(doc, node, "LowerBound", lowerBound_);

    if (upperBound_ != Null<Real>())
        XMLUtils::addChild(doc, node, "UpperBound", upperBound_);

    return node;
}

// Leaving QuantExt:: in the method signature satisfies VS intellisense.
// Without it, it complains about a missing definition.
OneDimSolverConfig::operator QuantExt::Solver1DOptions() const {

    Solver1DOptions solverOptions;

    if (!empty_) {
        solverOptions.maxEvaluations = maxEvaluations_;
        solverOptions.accuracy = accuracy_;
        solverOptions.initialGuess = initialGuess_;
        solverOptions.minMax = minMax_;
        solverOptions.step = step_;
        solverOptions.lowerBound = lowerBound_;
        solverOptions.upperBound = upperBound_;
    }

    return solverOptions;
}

void OneDimSolverConfig::check() const {

    QL_REQUIRE(maxEvaluations_ > 0, "MaxEvaluations (" << maxEvaluations_ << ") should be positive.");
    QL_REQUIRE(accuracy_ > 0, "Accuracy (" << accuracy_ << ") should be positive.");

    if (step_ != Null<Real>()) {
        QL_REQUIRE(step_ > 0, "Step (" << step_ << ") should be positive when given.");
    } else {
        const Real& min = minMax_.first;
        const Real& max = minMax_.second;
        QL_REQUIRE(min != Null<Real>() && max != Null<Real>(), "When Step is not given" <<
            " Min and Max should be provided.");
        QL_REQUIRE(min < max, "When given, Min (" << min << ") should be less than Max (" << max <<").");
    }

    if (lowerBound_ != Null<Real>() && upperBound_ != Null<Real>()) {
        QL_REQUIRE(lowerBound_ < upperBound_, "When given, LowerBound (" << lowerBound_ <<
            ") should be less than UpperBound (" << upperBound_ << ").");
    }
}

}
}
