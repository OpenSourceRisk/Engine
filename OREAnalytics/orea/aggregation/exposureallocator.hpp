/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file orea/aggregation/exposureallocator.hpp
    \brief Exposure allocator
    \ingroup analytics
*/

#pragma once

#include <orea/cube/npvcube.hpp>
#include <ored/portfolio/portfolio.hpp>

namespace ore {
namespace analytics {
using namespace QuantLib;
using namespace QuantExt;
using namespace data;
using namespace std;

//! Exposure allocator base class
/*!
  Derived classes implement a constructor with the relevant additional input data
  and a build function that performs the XVA calculations for all netting sets and
  along all paths.
*/
class ExposureAllocator {
public:

    enum class AllocationMethod {
        None,
        Marginal, // Pykhtin & Rosen, 2010
        RelativeFairValueGross,
        RelativeFairValueNet,
        RelativeXVA
    };

    ExposureAllocator(
        const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
        const QuantLib::ext::shared_ptr<NPVCube>& tradeExposureCube,
        const QuantLib::ext::shared_ptr<NPVCube>& nettedExposureCube,
        const Size allocatedTradeEpeIndex = 2, const Size allocatedTradeEneIndex = 3,
        const Size tradeEpeIndex = 0, const Size tradeEneIndex = 1,
        const Size nettingSetEpeIndex = 1, const Size nettingSetEneIndex = 2);

    virtual ~ExposureAllocator() {}
    const QuantLib::ext::shared_ptr<NPVCube>& exposureCube() { return tradeExposureCube_; }
    //! Compute exposures along all paths and fill result structures
    virtual void build();


protected:
    virtual Real calculateAllocatedEpe(const string& tid, const string& nid, const Date& date, const Size sample) = 0;
    virtual Real calculateAllocatedEne(const string& tid, const string& nid, const Date& date, const Size sample) = 0;
    QuantLib::ext::shared_ptr<Portfolio> portfolio_;
    QuantLib::ext::shared_ptr<NPVCube> tradeExposureCube_;
    QuantLib::ext::shared_ptr<NPVCube> nettedExposureCube_;
    Size tradeEpeIndex_;
    Size tradeEneIndex_;
    Size allocatedTradeEpeIndex_;
    Size allocatedTradeEneIndex_;
    Size nettingSetEpeIndex_;
    Size nettingSetEneIndex_;
    map<string, Real> nettingSetValueToday_, nettingSetPositiveValueToday_, nettingSetNegativeValueToday_;
};

class RelativeFairValueNetExposureAllocator : public ExposureAllocator {
public:
    RelativeFairValueNetExposureAllocator(
        const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
        const QuantLib::ext::shared_ptr<NPVCube>& tradeExposureCube,
        const QuantLib::ext::shared_ptr<NPVCube>& nettedExposureCube,
        const QuantLib::ext::shared_ptr<NPVCube>& npvCube,
        const Size allocatedTradeEpeIndex = 2, const Size allocatedTradeEneIndex = 3,
        const Size tradeEpeIndex = 0, const Size tradeEneIndex = 1,
        const Size nettingSetEpeIndex = 1, const Size nettingSetEneIndex = 2);

protected:
    virtual Real calculateAllocatedEpe(const string& tid, const string& nid, const Date& date, const Size sample) override;
    virtual Real calculateAllocatedEne(const string& tid, const string& nid, const Date& date, const Size sample) override;
    map<string, Real> tradeValueToday_;
    map<string, Real> nettingSetPositiveValueToday_;
    map<string, Real> nettingSetNegativeValueToday_;
};

class RelativeFairValueGrossExposureAllocator : public ExposureAllocator {
public:
    RelativeFairValueGrossExposureAllocator(
        const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
        const QuantLib::ext::shared_ptr<NPVCube>& tradeExposureCube,
        const QuantLib::ext::shared_ptr<NPVCube>& nettedExposureCube,
        const QuantLib::ext::shared_ptr<NPVCube>& npvCube,
        const Size allocatedTradeEpeIndex = 2, const Size allocatedTradeEneIndex = 3,
        const Size tradeEpeIndex = 0, const Size tradeEneIndex = 1,
        const Size nettingSetEpeIndex = 1, const Size nettingSetEneIndex = 2);

protected:
    virtual Real calculateAllocatedEpe(const string& tid, const string& nid, const Date& date, const Size sample) override;
    virtual Real calculateAllocatedEne(const string& tid, const string& nid, const Date& date, const Size sample) override;
    map<string, Real> tradeValueToday_;
    map<string, Real> nettingSetValueToday_;
};

class RelativeXvaExposureAllocator : public ExposureAllocator {
public:
    RelativeXvaExposureAllocator(
        const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
        const QuantLib::ext::shared_ptr<NPVCube>& tradeExposureCube,
        const QuantLib::ext::shared_ptr<NPVCube>& nettedExposureCube,
        const QuantLib::ext::shared_ptr<NPVCube>& npvCube,
        const map<string, Real>& tradeCva,
        const map<string, Real>& tradeDva,
        const map<string, Real>& nettingSetSumCva,
        const map<string, Real>& nettingSetSumDva,
        const Size allocatedTradeEpeIndex = 2, const Size allocatedTradeEneIndex = 3,
        const Size tradeEpeIndex = 0, const Size tradeEneIndex = 1,
        const Size nettingSetEpeIndex = 0, const Size nettingSetEneIndex = 1);

protected:
    virtual Real calculateAllocatedEpe(const string& tid, const string& nid, const Date& date, const Size sample) override;
    virtual Real calculateAllocatedEne(const string& tid, const string& nid, const Date& date, const Size sample) override;
    map<string, Real> tradeCva_;
    map<string, Real> tradeDva_;
    map<string, Real> nettingSetSumCva_;
    map<string, Real> nettingSetSumDva_;
    map<string, Real> tradeValueToday_;
};

class NoneExposureAllocator : public ExposureAllocator {
public:
    NoneExposureAllocator(
        const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
        const QuantLib::ext::shared_ptr<NPVCube>& tradeExposureCube,
        const QuantLib::ext::shared_ptr<NPVCube>& nettedExposureCube);

protected:
    virtual Real calculateAllocatedEpe(const string& tid, const string& nid, const Date& date, const Size sample) override;
    virtual Real calculateAllocatedEne(const string& tid, const string& nid, const Date& date, const Size sample) override;
};

//! Convert text representation to ExposureAllocator::AllocationMethod
ExposureAllocator::AllocationMethod parseAllocationMethod(const string& s);
//! Convert ExposureAllocator::AllocationMethod to text representation
std::ostream& operator<<(std::ostream& out, ExposureAllocator::AllocationMethod m);
} // namespace analytics
} // namespace ore
