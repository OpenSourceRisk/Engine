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

#include <ored/scripting/models/modelimpl.hpp>
#include <ored/scripting/utilities.hpp>

#include <ored/utilities/indexparser.hpp>

#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>

#include <ql/math/comparison.hpp>
#include <ql/quotes/simplequote.hpp>

namespace ore {
namespace data {

using namespace QuantLib;

ModelImpl::ModelImpl(const DayCounter& dayCounter, const Size size, const std::vector<std::string>& currencies,
                     const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>>& irIndices,
                     const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>& infIndices,
                     const std::vector<std::string>& indices, const std::vector<std::string>& indexCurrencies,
                     const std::set<Date>& simulationDates, const IborFallbackConfig& iborFallbackConfig)
    : Model(size), dayCounter_(dayCounter), currencies_(currencies), indexCurrencies_(indexCurrencies),
      simulationDates_(simulationDates), iborFallbackConfig_(iborFallbackConfig) {

    // populate index vectors

    for (auto const& str : indices) {
        indices_.push_back(IndexInfo(str));
    }

    for (auto const& p : irIndices) {
        irIndices_.push_back(std::make_pair(IndexInfo(p.first), p.second));
    }

    for (auto const& p : infIndices) {
        infIndices_.push_back(std::make_pair(IndexInfo(p.first), p.second));
    }

    // check consistency of inputs

    QL_REQUIRE(indexCurrencies_.size() == indices_.size(), "mismatch of indexCurrencies (" << indexCurrencies_.size()
                                                                                           << ") and indices ("
                                                                                           << indices_.size() << ")");

    for (auto const& c : currencies_)
        QL_REQUIRE(!c.empty(), "empty currency string");

    // look for fx indices, check consistency with currencies and index currencies vectors

    for (Size i = 0; i < indices_.size(); ++i) {
        if (indices_[i].isFx()) {
            QL_REQUIRE(indices_[i].fx()->targetCurrency().code() == currencies_.front(),
                       "fx index domestic currency (" << indices_[i].fx()->targetCurrency().code()
                                                      << ") does not match base currency (" << currencies_.front()
                                                      << ")");
            QL_REQUIRE(indices_[i].fx()->sourceCurrency().code() == indexCurrencies_[i],
                       "fx index foreign currency (" << indices_[i].fx()->sourceCurrency().code()
                                                     << ") does not match index currency (" << indexCurrencies_[i]);
            auto ccy = std::find(currencies_.begin(), currencies_.end(), indexCurrencies_[i]);
            QL_REQUIRE(ccy != currencies_.end(),
                       "fx index foreign currency (" << indexCurrencies_[i] << ") not found in model currencies");
        }
    }

    // register with observables

    for (auto const& i : irIndices_)
        registerWith(i.second);

    for (auto const& i : infIndices_)
        registerWith(i.second);

    for (auto const& i : indices_) {
        if (i.isComm()) {
            for (auto const& d : simulationDates_)
                registerWith(i.index(d));
        } else
            registerWith(i.index());
    }

} // ModelImpl ctor

RandomVariable ModelImpl::pay(const RandomVariable& amount, const Date& obsdate, const Date& paydate,
                              const std::string& currency) const {
    calculate();

    // result is as of max(obsdate, refDate) by definition of pay()

    Date effectiveDate = std::max(obsdate, referenceDate());
    auto c = std::find(currencies_.begin(), currencies_.end(), currency);
    QL_REQUIRE(c != currencies_.end(), "currency " << currency << " not handled");
    Size cidx = std::distance(currencies_.begin(), c);

    // do we have a dynamic fx underlying to convert to base at the effective date?

    RandomVariable fxSpot;
    for (Size i = 0; i < indexCurrencies_.size(); ++i) {
        if (indices_.at(i).isFx() && currency == indexCurrencies_[i]) {
            fxSpot = getIndexValue(i, effectiveDate);
            break;
        }
    }

    // if no we use the zero vol fx spot at the effective date

    if (!fxSpot.initialised()) {
        if (cidx > 0)
            fxSpot = RandomVariable(size(), getFxSpot(cidx - 1)) * getDiscount(cidx, referenceDate(), effectiveDate) /
                     getDiscount(0, referenceDate(), effectiveDate);
        else
            fxSpot = RandomVariable(size(), 1.0);
    }

    // discount from pay to obs date on ccy curve, convert to base ccy and divide by the numeraire

    return amount * getDiscount(cidx, effectiveDate, paydate) / getNumeraire(effectiveDate) * fxSpot;
}

RandomVariable ModelImpl::discount(const Date& obsdate, const Date& paydate, const std::string& currency) const {
    calculate();
    auto c = std::find(currencies_.begin(), currencies_.end(), currency);
    QL_REQUIRE(c != currencies_.end(), "currency " << currency << " not handled");
    Size cidx = std::distance(currencies_.begin(), c);
    return getDiscount(cidx, obsdate, paydate);
}

namespace {
struct comp {
    comp(const std::string& indexInput) : indexInput_(indexInput) {}
    template <typename T> bool operator()(const std::pair<IndexInfo, QuantLib::ext::shared_ptr<T>>& p) const {
        return p.first.name() == indexInput_;
    }
    const std::string indexInput_;
};
} // namespace

RandomVariable ModelImpl::getInflationIndexFixing(const bool returnMissingFixingAsNull, const std::string& indexInput,
                                                  const QuantLib::ext::shared_ptr<ZeroInflationIndex>& infIndex,
                                                  const Size indexNo, const Date& limDate, const Date& obsdate,
                                                  const Date& fwddate, const Date& baseDate) const {
    RandomVariable res(size());
    Real f = infIndex->timeSeries()[limDate];
    // we exclude historical fixings
    // - which are "impossible" to know (limDate > refDate)
    // - which have to be projected because a fwd date is given and they are later than the obsdate
    if (f != Null<Real>() && !(limDate > referenceDate()) && (fwddate == Null<Date>() || limDate <= obsdate)) {
        res = RandomVariable(size(), f);
    } else {
        Date effectiveObsDate = std::min(obsdate, limDate);
        if (effectiveObsDate >= baseDate) {
            res = getInfIndexValue(indexNo, effectiveObsDate, limDate);
        } else if (returnMissingFixingAsNull) {
            return RandomVariable();
        } else {
            QL_FAIL("missing " << indexInput << " fixing for " << QuantLib::io::iso_date(limDate) << " (obsdate="
                               << QuantLib::io::iso_date(obsdate) << ", fwddate=" << QuantLib::io::iso_date(fwddate)
                               << ", basedate=" << QuantLib::io::iso_date(baseDate) << ")");
        }
    }
    return res;
}

RandomVariable ModelImpl::eval(const std::string& indexInput, const Date& obsdate, const Date& fwddate,
                               const bool returnMissingFixingAsNull, const bool ignoreTodaysFixing) const {
    calculate();

    std::string index = indexInput;
    IndexInfo indexInfo(index);

    // 1 handle inflation indices
    QL_DEPRECATED_DISABLE_WARNING
    // Remove in later version, scripts should take care of interpolation
    if (indexInfo.isInf()) {
        auto inf = std::find_if(infIndices_.begin(), infIndices_.end(), comp(indexInput));
        QL_REQUIRE(inf != infIndices_.end(),
                   "ModelImpl::eval(): did not find inflation index '" << indexInput << "' in model index list.");
        Date baseDate = inf->second->zeroInflationTermStructure()->baseDate();
        Date effectiveFixingDate = fwddate != Null<Date>() ? fwddate : obsdate;
        std::pair<Date, Date> lim = inflationPeriod(effectiveFixingDate, inf->second->frequency());
        RandomVariable indexStart =
            getInflationIndexFixing(returnMissingFixingAsNull, indexInput, inf->second,
                                    std::distance(infIndices_.begin(), inf), lim.first, obsdate, fwddate, baseDate);
        // if the index is not interpolated we are done
        if (!indexInfo.inf()->interpolated()) {
            return indexStart;
        }
        ALOG("Interpolated Inflation Indices are deprecated, adjust your script to handle interpolation there");
        // otherwise we need to get a second value and interpolate as in ZeroInflationIndex
        RandomVariable indexEnd = getInflationIndexFixing(returnMissingFixingAsNull, indexInput, inf->second,
                                                          std::distance(infIndices_.begin(), inf), lim.second + 1,
                                                          obsdate, fwddate, baseDate);
        // this is not entirely correct, since we should use the days in the lagged period, but we don't know the lag
        return indexStart +
               (indexEnd - indexStart) * RandomVariable(size(), static_cast<Real>(effectiveFixingDate - lim.first) /
                                                                    static_cast<Real>(lim.second + 1 - lim.first));
    }
    QL_DEPRECATED_ENABLE_WARNING
    // 2 handle non-inflation indices

    // 2a handle historical fixings and today's fixings (if given as a historical fixing)
    // for fx indices try to get the fixing both from the straight and the inverse index

    if (fwddate == Null<Date>()) {
        if (obsdate < referenceDate() || (obsdate == referenceDate() && !ignoreTodaysFixing)) {
            if (indexInfo.irIborFallback(iborFallbackConfig_, referenceDate())) {
                // handle ibor fallback indices, they don't fit into the below treatment...
                auto ir = std::find_if(irIndices_.begin(), irIndices_.end(), comp(indexInput));
                if (ir != irIndices_.end()) {
                    return RandomVariable(size(),
                                          ir->second->fixing(ir->second->fixingCalendar().adjust(obsdate, Preceding)));
                } else {
                    QL_FAIL("ir (fallback ibor) index '" << indexInput
                                                         << "' not found in ir indices list, internal error.");
                }
            } else {
                // handle all other cases than ibor fallback indices
                QuantLib::ext::shared_ptr<Index> idx = indexInfo.index(obsdate);
                Real fixing = Null<Real>();
                try {
                    fixing = idx->fixing(idx->fixingCalendar().adjust(obsdate, Preceding));
                } catch (...) {
                }
                if (fixing != Null<Real>())
                    return RandomVariable(size(), fixing);
                else {
                    // for dates < refDate we are stuck now
                    if (obsdate != referenceDate()) {
                        if (returnMissingFixingAsNull)
                            return RandomVariable();
                        else {
                            QL_FAIL("missing "
                                    << idx->name() << " fixing for " << QuantLib::io::iso_date(obsdate)
                                    << " (adjusted fixing date = "
                                    << QuantLib::io::iso_date(idx->fixingCalendar().adjust(obsdate, Preceding)) << ")");
                        }
                    }
                }
            }
        }
    } else {
        // if fwd date is given, ensure we can project
        QL_REQUIRE(obsdate >= referenceDate(), "if fwd date is given ("
                                                   << QuantLib::io::iso_date(fwddate) << "), obsdate ("
                                                   << QuantLib::io::iso_date(obsdate) << ") must be >= refdate ("
                                                   << QuantLib::io::iso_date(referenceDate()) << ")");
    }

    // 2b handle fixings >= today (and fwd fixings, in which case we know fwddate > obsdate >= refdate)

    // 2b1 handle IR indices

    if (indexInfo.isIr()) {
        auto ir = std::find_if(irIndices_.begin(), irIndices_.end(), comp(indexInput));
        if (ir != irIndices_.end()) {
            RandomVariable res = getIrIndexValue(std::distance(irIndices_.begin(), ir), obsdate, fwddate);
            QL_REQUIRE(res.initialised(), "internal error: could not project "
                                              << ir->second->name() << " fixing for (obsdate/fwddate) = ("
                                              << QuantLib::io::iso_date(obsdate) << ","
                                              << QuantLib::io::iso_date(fwddate) << ")");
            return res;
        }
    }

    // 2b2 handle FX, EQ, COMM indices

    // if we have an FX index, we "normalise" the tag by GENERIC (since it does not matter for projections)

    if (indexInfo.isFx())
        indexInfo = IndexInfo("FX-GENERIC-" + indexInfo.fx()->sourceCurrency().code() + "-" +
                              indexInfo.fx()->targetCurrency().code());

    RandomVariable res;
    auto i = std::find(indices_.begin(), indices_.end(), indexInfo);
    if (i != indices_.end()) {
        // we have the index directly as an underlying
        res = getIndexValue(std::distance(indices_.begin(), i), obsdate, fwddate);
    } else {
        // if not, we can only try something else for FX indices
        QL_REQUIRE(indexInfo.isFx(), "ModelImpl::eval(): index " << index << " not handled");
        // is it a trivial fx index (CCY-CCY) or can we triangulate it?
        RandomVariable fx1(size(), 1.0), fx2(size(), 1.0);
        if (indexInfo.fx()->sourceCurrency() == indexInfo.fx()->targetCurrency()) {
            res = fx1;
            // no fwd correction required, if ccy1=ccy2 we have spot = fwd = 1
        } else {
            Size ind1 = Null<Size>(), ind2 = Null<Size>();
            for (Size i = 0; i < indexCurrencies_.size(); ++i) {
                if (indices_[i].isFx()) {
                    if (indexInfo.fx()->sourceCurrency().code() == indexCurrencies_[i])
                        ind1 = i;
                    if (indexInfo.fx()->targetCurrency().code() == indexCurrencies_[i])
                        ind2 = i;
                }
            }
            if (ind1 != Null<Size>()) {
                fx1 = getIndexValue(ind1, obsdate);
            }
            if (ind2 != Null<Size>()) {
                fx2 = getIndexValue(ind2, obsdate);
            }
            res = fx1 / fx2;
            if (fwddate != Null<Date>()) {
                auto ind1 = std::find(currencies_.begin(), currencies_.end(), indexInfo.fx()->sourceCurrency().code());
                auto ind2 = std::find(currencies_.begin(), currencies_.end(), indexInfo.fx()->targetCurrency().code());
                QL_REQUIRE(ind1 != currencies_.end(), "currency " << indexInfo.fx()->sourceCurrency().code()
                                                                  << " in index " << index << " not handled");
                QL_REQUIRE(ind2 != currencies_.end(), "currency " << indexInfo.fx()->targetCurrency().code()
                                                                  << " in index " << index << " not handled");
                res *= getDiscount(std::distance(currencies_.begin(), ind1), obsdate, fwddate) /
                       getDiscount(std::distance(currencies_.begin(), ind2), obsdate, fwddate);
            }
        }
    }

    return res;
}

Real ModelImpl::fxSpotT0(const std::string& forCcy, const std::string& domCcy) const {
    auto c1 = std::find(currencies_.begin(), currencies_.end(), forCcy);
    auto c2 = std::find(currencies_.begin(), currencies_.end(), domCcy);
    QL_REQUIRE(c1 != currencies_.end(), "currency " << forCcy << " not handled");
    QL_REQUIRE(c2 != currencies_.end(), "currency " << domCcy << " not handled");
    Size cidx1 = std::distance(currencies_.begin(), c1);
    Size cidx2 = std::distance(currencies_.begin(), c2);
    Real fx = 1.0;
    if (cidx1 > 0)
        fx *= getFxSpot(cidx1 - 1);
    if (cidx2 > 0)
        fx /= getFxSpot(cidx2 - 1);
    return fx;
}

RandomVariable ModelImpl::barrierProbability(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                             const RandomVariable& barrier, const bool above) const {

    calculate();

    // determine the fixing calendar (assume that for commodity this is the same for different futures)

    IndexInfo ind(index);
    auto qlIndex = ind.index(obsdate1);

    // handle all dates < reference date here

    Filter barrierHit(barrier.size(), false);
    Date d = obsdate1;
    while (d < std::min(referenceDate(), obsdate2)) {
        if (qlIndex->fixingCalendar().isBusinessDay(d)) {
            RandomVariable f = eval(index, d, Null<Date>(), true);
            if (f.initialised()) {
                if (above) {
                    barrierHit = barrierHit || f >= barrier;
                } else {
                    barrierHit = barrierHit || f <= barrier;
                }
            } else {
                // lax check of historical fixings, since e.g. for equity underlyings
                // we can't expect to get the actual fixing calendar from index info
                // TODO can we improve this by using the correct calendars?
                TLOG("ignore missing fixing for " << qlIndex->name() << " on " << QuantLib::io::iso_date(d)
                                                  << " in ModelImpl::barrierProbability()");
            }
        }
        ++d;
    }

    RandomVariable barrierHitAsNumber = RandomVariable(barrierHit, 1.0, 0.0);
    if (obsdate2 < referenceDate()) {
        return barrierHitAsNumber;
    }

    // handle future part (call into derived classes, this is model dependent)

    RandomVariable res =
        getFutureBarrierProb(index, std::max<Date>(obsdate1, referenceDate()), obsdate2, barrier, above);

    // combine historical and future part and return result

    return applyInverseFilter(res, barrierHit) + barrierHitAsNumber;
}

Real ModelImpl::extractT0Result(const RandomVariable& value) const { return expectation(value).at(0); }

} // namespace data
} // namespace ore
