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

#include <orea/aggregation/exposureallocator.hpp>
#include <orea/cube/inmemorycube.hpp>

#include <ored/portfolio/trade.hpp>

using namespace std;
using namespace QuantLib;

namespace ore {
namespace analytics {

ExposureAllocator::ExposureAllocator(
        const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
        const QuantLib::ext::shared_ptr<NPVCube>& tradeExposureCube,
        const QuantLib::ext::shared_ptr<NPVCube>& nettedExposureCube,
        const Size allocatedTradeEpeIndex, const Size allocatedTradeEneIndex,
        const Size tradeEpeIndex, const Size tradeEneIndex,
        const Size nettingSetEpeIndex, const Size nettingSetEneIndex)
    : portfolio_(portfolio), tradeExposureCube_(tradeExposureCube),
      nettedExposureCube_(nettedExposureCube),
      tradeEpeIndex_(tradeEpeIndex), tradeEneIndex_(tradeEneIndex),
      allocatedTradeEpeIndex_(allocatedTradeEpeIndex), allocatedTradeEneIndex_(allocatedTradeEneIndex),
      nettingSetEpeIndex_(nettingSetEpeIndex), nettingSetEneIndex_(nettingSetEneIndex) {}

void ExposureAllocator::build() {
    LOG("Compute allocated trade exposures");

    for (const auto& [nettingSetId, idx] : nettedExposureCube_->idsAndIndexes()) {

        for (const auto & [tid, trade] : portfolio_->trades()) {
            string nid = trade->envelope().nettingSetId();
            if (nid != nettingSetId)
                continue;
            

            for (Date date : tradeExposureCube_->dates()) {
                for (Size k = 0; k < tradeExposureCube_->samples(); ++k) {
                    tradeExposureCube_->set(calculateAllocatedEpe(tid, nid, date, k),
                                            tid, date, k, allocatedTradeEpeIndex_);
                    tradeExposureCube_->set(calculateAllocatedEne(tid, nid, date, k),
                                            tid, date, k, allocatedTradeEneIndex_);
                }
            }
        }
    }
    LOG("Completed calculating allocated trade exposures");
}

RelativeFairValueNetExposureAllocator::RelativeFairValueNetExposureAllocator(
    const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
    const QuantLib::ext::shared_ptr<NPVCube>& tradeExposureCube,
    const QuantLib::ext::shared_ptr<NPVCube>& nettedExposureCube,
    const QuantLib::ext::shared_ptr<NPVCube>& npvCube,
    const Size allocatedTradeEpeIndex, const Size allocatedTradeEneIndex,
    const Size tradeEpeIndex, const Size tradeEneIndex,
    const Size nettingSetEpeIndex, const Size nettingSetEneIndex)
    : ExposureAllocator(portfolio, tradeExposureCube, nettedExposureCube,
                        allocatedTradeEpeIndex, allocatedTradeEneIndex,
                        tradeEpeIndex, tradeEneIndex,
                        nettingSetEpeIndex, nettingSetEneIndex) {
    size_t i = 0;
    for (auto tradeIt = portfolio_->trades().begin(); tradeIt != portfolio_->trades().end(); ++tradeIt, ++i) {
        auto trade = tradeIt->second;
        string tradeId = tradeIt->first;
        string nettingSetId = trade->envelope().nettingSetId();
        if (nettingSetPositiveValueToday_.find(nettingSetId) == nettingSetPositiveValueToday_.end()) {
            nettingSetPositiveValueToday_[nettingSetId] = 0.0;
            nettingSetNegativeValueToday_[nettingSetId] = 0.0;
        }
        Real npv = npvCube->getT0(i);
        tradeValueToday_[tradeId] = npv;
        if (npv > 0)
            nettingSetPositiveValueToday_[nettingSetId] += npv;
        else
            nettingSetNegativeValueToday_[nettingSetId] += npv;
    }
}

Real RelativeFairValueNetExposureAllocator::calculateAllocatedEpe(const string& tid, const string& nid,
                                                                    const Date& date, const Size sample) {
    // FIXME: What to do when either the pos. or neg. netting set value is zero?
    QL_REQUIRE(nettingSetPositiveValueToday_[nid] > 0.0, "non-zero positive NPV expected");
    Real netEPE = nettedExposureCube_->get(nid, date, sample, nettingSetEpeIndex_);
    return netEPE * std::max(tradeValueToday_[tid], 0.0) / nettingSetPositiveValueToday_[nid];
}

Real RelativeFairValueNetExposureAllocator::calculateAllocatedEne(const string& tid, const string& nid,
                                                                    const Date& date, const Size sample) {
    // FIXME: What to do when either the pos. or neg. netting set value is zero?
    QL_REQUIRE(nettingSetNegativeValueToday_[nid] > 0.0, "non-zero negative NPV expected");
    Real netENE = nettedExposureCube_->get(nid, date, sample, nettingSetEneIndex_);
    return netENE * -std::max(-tradeValueToday_[tid], 0.0) / nettingSetPositiveValueToday_[nid];
}

RelativeFairValueGrossExposureAllocator::RelativeFairValueGrossExposureAllocator(
    const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
    const QuantLib::ext::shared_ptr<NPVCube>& tradeExposureCube,
    const QuantLib::ext::shared_ptr<NPVCube>& nettedExposureCube,
    const QuantLib::ext::shared_ptr<NPVCube>& npvCube,
    const Size allocatedTradeEpeIndex, const Size allocatedTradeEneIndex,
    const Size tradeEpeIndex, const Size tradeEneIndex,
    const Size nettingSetEpeIndex, const Size nettingSetEneIndex)
    : ExposureAllocator(portfolio, tradeExposureCube, nettedExposureCube,
                        allocatedTradeEpeIndex, allocatedTradeEneIndex,
                        tradeEpeIndex, tradeEneIndex,
                        nettingSetEpeIndex, nettingSetEneIndex) {
    size_t i = 0;
    for (auto tradeIt = portfolio_->trades().begin(); tradeIt != portfolio_->trades().end(); ++tradeIt, ++i) {
        auto trade = tradeIt->second;
        string tradeId = tradeIt->first;
        string nettingSetId = trade->envelope().nettingSetId();
        if (nettingSetValueToday_.find(nettingSetId) == nettingSetValueToday_.end())
            nettingSetValueToday_[nettingSetId] = 0.0;
        Real npv = npvCube->getT0(i);
        tradeValueToday_[tradeId] = npv;
        nettingSetValueToday_[nettingSetId] += npv;
    }
}

Real RelativeFairValueGrossExposureAllocator::calculateAllocatedEpe(const string& tid, const string& nid,
                                                                    const Date& date, const Size sample) {
    // FIXME: What to do when the netting set value is zero?
    QL_REQUIRE(nettingSetValueToday_[nid] != 0.0, "non-zero netting set value expected");
    Real netEPE = nettedExposureCube_->get(nid, date, sample, nettingSetEpeIndex_);
    return netEPE * tradeValueToday_[tid] / nettingSetValueToday_[nid];
}

Real RelativeFairValueGrossExposureAllocator::calculateAllocatedEne(const string& tid, const string& nid,
                                                                    const Date& date, const Size sample) {
    // FIXME: What to do when the netting set value is zero?
    QL_REQUIRE(nettingSetValueToday_[nid] != 0.0, "non-zero netting set value expected");
    Real netENE = nettedExposureCube_->get(nid, date, sample, nettingSetEneIndex_);
    return netENE * tradeValueToday_[tid] / nettingSetValueToday_[nid];
}

RelativeXvaExposureAllocator::RelativeXvaExposureAllocator(
    const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
    const QuantLib::ext::shared_ptr<NPVCube>& tradeExposureCube,
    const QuantLib::ext::shared_ptr<NPVCube>& nettedExposureCube,
    const QuantLib::ext::shared_ptr<NPVCube>& npvCube,
    const map<string, Real>& tradeCva,
    const map<string, Real>& tradeDva,
    const map<string, Real>& nettingSetSumCva,
    const map<string, Real>& nettingSetSumDva,
    const Size allocatedTradeEpeIndex, const Size allocatedTradeEneIndex,
    const Size tradeEpeIndex, const Size tradeEneIndex,
    const Size nettingSetEpeIndex, const Size nettingSetEneIndex)
    : ExposureAllocator(portfolio, tradeExposureCube, nettedExposureCube,
                        allocatedTradeEpeIndex, allocatedTradeEneIndex,
                        tradeEpeIndex, tradeEneIndex,
                        nettingSetEpeIndex, nettingSetEneIndex),
      tradeCva_(tradeCva), tradeDva_(tradeDva),
      nettingSetSumCva_(nettingSetSumCva), nettingSetSumDva_(nettingSetSumDva) {}

Real RelativeXvaExposureAllocator::calculateAllocatedEpe(const string& tid, const string& nid,
                                                         const Date& date, const Size sample) {
    Real netEPE = nettedExposureCube_->get(nid, date, sample, nettingSetEpeIndex_);
    return netEPE * tradeCva_[tid] / nettingSetSumCva_[nid];
}
Real RelativeXvaExposureAllocator::calculateAllocatedEne(const string& tid, const string& nid,
                                                         const Date& date, const Size sample) {
    Real netENE = nettedExposureCube_->get(nid, date, sample, nettingSetEneIndex_);
    return netENE * tradeDva_[tid] / nettingSetSumDva_[nid];
}

NoneExposureAllocator::NoneExposureAllocator(
    const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
    const QuantLib::ext::shared_ptr<NPVCube>& tradeExposureCube,
    const QuantLib::ext::shared_ptr<NPVCube>& nettedExposureCube)
    : ExposureAllocator(portfolio, tradeExposureCube, nettedExposureCube) {}

Real NoneExposureAllocator::calculateAllocatedEpe(const string& tid, const string& nid,
                                                  const Date& date, const Size sample) {
    return 0;
}
Real NoneExposureAllocator::calculateAllocatedEne(const string& tid, const string& nid,
                                                  const Date& date, const Size sample) {
    return 0;
}

ExposureAllocator::AllocationMethod parseAllocationMethod(const string& s) {
    static map<string, ExposureAllocator::AllocationMethod> m = {
        {"None", ExposureAllocator::AllocationMethod::None},
        {"Marginal", ExposureAllocator::AllocationMethod::Marginal},
        {"RelativeFairValueGross", ExposureAllocator::AllocationMethod::RelativeFairValueGross},
        {"RelativeFairValueNet", ExposureAllocator::AllocationMethod::RelativeFairValueNet},
        {"RelativeXVA", ExposureAllocator::AllocationMethod::RelativeXVA},
    };

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("AllocationMethod \"" << s << "\" not recognized");
    }
}

std::ostream& operator<<(std::ostream& out, ExposureAllocator::AllocationMethod m) {
    if (m == ExposureAllocator::AllocationMethod::None)
        out << "None";
    else if (m == ExposureAllocator::AllocationMethod::Marginal)
        out << "Marginal";
    else if (m == ExposureAllocator::AllocationMethod::RelativeFairValueGross)
        out << "RelativeFairValueGross";
    else if (m == ExposureAllocator::AllocationMethod::RelativeFairValueNet)
        out << "RelativeFairValueNet";
    else if (m == ExposureAllocator::AllocationMethod::RelativeXVA)
        out << "RelativeXVA";
    else
        QL_FAIL("Allocation method not covered");
    return out;
}

} // namespace analytics
} // namespace ore
