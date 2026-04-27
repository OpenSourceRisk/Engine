/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#ifndef qle_termstructures_ext_i
#define qle_termstructures_ext_i

%include common.i
%include date.i
%include termstructures.i
%include volatilities.i

%rename(QLESpreadedSwaptionVolatility) QuantExt::SpreadedSwaptionVolatility;
%shared_ptr(QuantExt::SpreadedSwaptionVolatility)
namespace QuantExt {
class SpreadedSwaptionVolatility : public SwaptionVolatilityDiscrete {
public:
    SpreadedSwaptionVolatility(const Handle<SwaptionVolatilityStructure>& base,
                               const std::vector<Period>& optionTenors,
                               const std::vector<Period>& swapTenors,
                               const std::vector<Real>& strikeSpreads,
                               const std::vector<std::vector<Handle<Quote>>>& volSpreads,
                               const QuantLib::ext::shared_ptr<SwapIndex>& baseSwapIndexBase = nullptr,
                               const QuantLib::ext::shared_ptr<SwapIndex>& baseShortSwapIndexBase = nullptr,
                               const QuantLib::ext::shared_ptr<SwapIndex>& simulatedSwapIndexBase = nullptr,
                               const QuantLib::ext::shared_ptr<SwapIndex>& simulatedShortSwapIndexBase = nullptr,
                               const bool stickyAbsMoney = false);
    const Handle<SwaptionVolatilityStructure>& baseVol();
};
}

%shared_ptr(QuantExt::DynamicSwaptionVolatilityMatrix)
%nodefaultctor QuantExt::DynamicSwaptionVolatilityMatrix;
namespace QuantExt {
class DynamicSwaptionVolatilityMatrix : public SwaptionVolatilityStructure {
};
}

%shared_ptr(QuantExt::BlackVolatilitySurfaceBFRR)
%nodefaultctor QuantExt::BlackVolatilitySurfaceBFRR;
namespace QuantExt {
class BlackVolatilitySurfaceBFRR : public BlackVolTermStructure {
public:
    enum class SmileInterpolation { Linear = 1, Cubic = 2 };
    enum class TimeInterpolation { V, V2T };

    const std::vector<QuantLib::Date>& dates() const;
    const std::vector<Real>& deltas() const;
    const std::vector<Real>& currentDeltas() const;
    const std::vector<std::vector<Real>>& bfQuotes() const;
    const std::vector<std::vector<Real>>& rrQuotes() const;
    const std::vector<Real>& atmQuotes() const;
};
}

%shared_ptr(QuantExt::CorrelationTermStructure)
%nodefaultctor QuantExt::CorrelationTermStructure;
namespace QuantExt {
class CorrelationTermStructure : public TermStructure {
public:
    Real correlation(Time t, Real strike = Null<Real>(), bool extrapolate = false) const;
    Real correlation(const Date& d, Real strike = Null<Real>(), bool extrapolate = false) const;
    virtual Time minTime() const;
};
}

%template(CorrelationTermStructureHandle) Handle<QuantExt::CorrelationTermStructure>;
%template(RelinkableCorrelationTermStructureHandle) RelinkableHandle<QuantExt::CorrelationTermStructure>;

%shared_ptr(QuantExt::NegativeCorrelationTermStructure)
namespace QuantExt {
class NegativeCorrelationTermStructure : public QuantExt::CorrelationTermStructure {
public:
    NegativeCorrelationTermStructure(const Handle<QuantExt::CorrelationTermStructure>& c);
};
}

%shared_ptr(QuantExt::CorrelationValue)
namespace QuantExt {
class CorrelationValue : public Quote {
public:
    CorrelationValue(const Handle<QuantExt::CorrelationTermStructure>& correlation,
                     const Time t,
                     const Real strike = Null<Real>());
    Real value() const;
    bool isValid() const;
    void update() override;
};
}

#endif
