/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <ored/marketdata/basecorrelationcurve.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/parsers.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/matrix.hpp>

#include <qle/utilities/interpolation.hpp>
#include <qle/utilities/time.hpp>
#include <qle/math/flatextrapolation2d.hpp>

using namespace QuantLib;
using namespace std;

namespace {

using std::string;

// Check that weight, prior Weight or recovery is in [0, 1]
void validateWeightRec(Real value, const string& name, const string& varName) {
    QL_REQUIRE(value <= 1.0, "The " << varName << " value (" << value << ") for name " << name <<
        " should not be greater than 1.0.");
    QL_REQUIRE(value >= 0.0, "The " << varName << " value (" << value << ") for name " << name <<
        " should not be less than 0.0.");
}

}

namespace ore {
namespace data {

BaseCorrelationCurve::BaseCorrelationCurve(
    Date asof,
    BaseCorrelationCurveSpec spec,
    const Loader& loader,
    const CurveConfigurations& curveConfigs,
    const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData)
    : spec_(spec), referenceData_(referenceData) {

    DLOG("BaseCorrelationCurve: start building base correlation structure with ID " << spec_.curveConfigID());
    
    try {

        const auto& config = *curveConfigs.baseCorrelationCurveConfig(spec_.curveConfigID());

        // The base correlation surface is of the form term x detachment point. We need at least two detachment points
        // and at least one term. The list of terms may be explicit or contain a single wildcard character '*'.
        // Similarly, the list of detachment points may be explicit or contain a single wildcard character '*'.

        // Terms
        const vector<string>& termStrs = config.terms();
        QL_REQUIRE(!termStrs.empty(), "BaseCorrelationCurve: need at least one term.");
        set<Period> terms;
        bool termWc = find(termStrs.begin(), termStrs.end(), "*") != termStrs.end();
        if (termWc) {
            QL_REQUIRE(termStrs.size() == 1, "BaseCorrelationCurve: only one wild card term can be specified.");
            DLOG("Have term wildcard pattern " << termStrs[0]);
        } else {
            for (const string& termStr : termStrs) {
                terms.insert(parsePeriod(termStr));
            }
            DLOG("Parsed " << terms.size() << " unique configured term(s).");
        }

        // Detachment points

        // Not advised to use set/map with floating point keys but should be ok here.
        auto cmp = [](Real dp_1, Real dp_2) { return !close_enough(dp_1, dp_2) && dp_1 < dp_2; };
        set<Real, decltype(cmp)> dps(cmp);

        const vector<string>& dpStrs = config.detachmentPoints();
        bool dpsWc = find(dpStrs.begin(), dpStrs.end(), "*") != dpStrs.end();
        if (dpsWc) {
            QL_REQUIRE(dpStrs.size() == 1, "BaseCorrelationCurve: only one wild card "
                                               << "detachment point can be specified.");
            DLOG("Have detachment point wildcard pattern " << dpStrs[0]);
        } else {
            for (const string& dpStr : dpStrs) {
                dps.insert(parseReal(dpStr));
            }
            DLOG("Parsed " << dps.size() << " unique configured detachment points.");
            QL_REQUIRE(dps.size() > 1, "BaseCorrelationCurve: need at least 2 unique detachment points.");
        }

        // Read in quotes relevant for the base correlation surface. The points that will be used are stored in data
        // where the key is the term, detachment point pair and value is the base correlation quote.
        auto mpCmp = [](pair<Period, Real> k_1, pair<Period, Real> k_2) {
            if (k_1.first != k_2.first)
                return k_1.first < k_2.first;
            else
                return !close_enough(k_1.second, k_2.second) && k_1.second < k_2.second;
        };
        map<pair<Period, Real>, Handle<Quote>, decltype(mpCmp)> data(mpCmp);

        std::ostringstream ss;
        ss << MarketDatum::InstrumentType::CDS_INDEX << "/" << MarketDatum::QuoteType::BASE_CORRELATION << "/*";
        Wildcard w(ss.str());
        for (const auto& md : loader.get(w, asof)) {

            QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");

            auto q = QuantLib::ext::dynamic_pointer_cast<BaseCorrelationQuote>(md);
            QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to BaseCorrelationQuote");

            // Go to next quote if index name in the quote does not match the cds vol configuration name.
            if (config.quoteName() != q->cdsIndexName())
                continue;

            TLOG("Processing quote " << q->name() << ": (" << q->term() << "," << fixed << setprecision(9)
                                     << q->detachmentPoint() << "," << q->quote()->value() << ")");

            // If we have been given a list of explicit terms, check that the quote matches one of them.
            // Move to the next quote if it does not.
            if (!termWc) {
                auto it = terms.find(q->term());
                if (it == terms.end())
                    continue;
            }

            // If we have been given a list of explicit detachment points, check that the quote matches one of them.
            // Move to the next quote if it does not.
            if (!dpsWc) {
                auto it = dps.find(q->detachmentPoint());
                if (it == dps.end())
                    continue;
            }

            // Skip if we have already added a quote for the given term and detachment point.
            auto key = make_pair(q->term(), q->detachmentPoint());
            if (data.find(key) != data.end()) {
                DLOG("Already added base correlation with term " << q->term() << " and detachment point " << fixed
                                                                 << setprecision(9) << q->detachmentPoint()
                                                                 << " so skipping quote " << q->name());
                continue;
            }

            // If we have wildcards, update the set so that dps and terms are populated.
            if (termWc)
                terms.insert(q->term());
            if (dpsWc)
                dps.insert(q->detachmentPoint());

            // Add to the data that we will use.
            data[key] = q->quote();

            TLOG("Added quote " << q->name() << ": (" << q->term() << "," << fixed << setprecision(9)
                                << q->detachmentPoint() << "," << q->quote()->value() << ")");
        }

        DLOG("After processing the quotes, we have " << terms.size() << " unique term(s), " << dps.size()
                                                     << " unique detachment points and " << data.size() << " quotes.");
        QL_REQUIRE(dps.size() > 1, "BaseCorrelationCurve: need at least 2 unique detachment points.");
        QL_REQUIRE(dps.size() * terms.size() == data.size(),
                   "BaseCorrelationCurve: number of quotes ("
                       << data.size() << ") should equal number of detachment points (" << dps.size()
                       << ") times the number of terms (" << terms.size() << ").");


        vector<vector<Handle<Quote>>> quotes;

        // Need a vector of terms and detachment points for ctor below
        vector<Period> tmpTerms(terms.begin(), terms.end());
        vector<Real> tmpDps(dps.begin(), dps.end());

        if (config.indexTerm() != 0 * Days) {
            vector<Time> termTimes;
            for (auto const& h : tmpTerms) {
                termTimes.push_back(QuantExt::periodToTime(h));
            }

            Real t = QuantExt::periodToTime(config.indexTerm());
            Size termIndex_m, termIndex_p;
            Real alpha;
            std::tie(termIndex_m, termIndex_p, alpha) = QuantExt::interpolationIndices(termTimes, t);

            for (const auto& dp : dps) {
                quotes.push_back({});
                for (const auto& term : terms) {
                    auto key_m = make_pair(tmpTerms[termIndex_m], dp);
                    auto it_m = data.find(key_m);
                    auto key_p = make_pair(tmpTerms[termIndex_p], dp);
                    auto it_p = data.find(key_p);
                    QL_REQUIRE(it_m != data.end() && it_p != data.end(),
                               "BaseCorrelationCurve: do not have a quote for term "
                                   << term << " and detachment point " << fixed << setprecision(9) << dp << ".");
                    quotes.back().push_back(Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(
                        alpha * it_m->second->value() + (1 - alpha) * it_p->second->value())));
                }
            }
        } else {

            // Need to now fill _completely_ the (number of dps) x (number of terms) quotes surface.

            for (const auto& dp : dps) {
                quotes.push_back({});
                for (const auto& term : terms) {
                    auto key = make_pair(term, dp);
                    auto it = data.find(key);
                    QL_REQUIRE(it != data.end(), "BaseCorrelationCurve: do not have a quote for term "
                                                     << term << " and detachment point " << fixed << setprecision(9)
                                                     << dp << ".");
                    quotes.back().push_back(it->second);
                }
            }
        }

        if (config.adjustForLosses()) {
            DLOG("Adjust for losses is true for base correlation " << spec_.curveConfigID());
            DLOG("Detachment points before: " << Array(tmpDps.begin(), tmpDps.end()));
            tmpDps = adjustForLosses(tmpDps);
            DLOG("Detachment points after: " << Array(tmpDps.begin(), tmpDps.end()));
        }

        // The QuantLib interpolator expects at least two terms, so add a column, copying the last
        QL_REQUIRE(!tmpTerms.empty(), "No terms found");
        tmpTerms.push_back(tmpTerms.back() + 1 * tmpTerms.back().units());
        for (Size i = 0; i < tmpDps.size(); ++i)
            quotes[i].push_back(quotes[i][tmpTerms.size() - 2]);

        baseCorrelation_ = QuantLib::ext::make_shared<QuantExt::InterpolatedBaseCorrelationTermStructure<QuantExt::BilinearFlat>>(
            config.settlementDays(), config.calendar(), config.businessDayConvention(), tmpTerms, tmpDps, quotes,
            config.dayCounter(), config.startDate(), config.rule());

        baseCorrelation_->enableExtrapolation(config.extrapolate());
        

    } catch (std::exception& e) {
        QL_FAIL("BaseCorrelationCurve: curve building failed :" << e.what());
    } catch (...) {
        QL_FAIL("BaseCorrelationCurve: curve building failed: unknown error");
    }

    DLOG("BaseCorrelationCurve: finished building base correlation structure with ID " << spec_.curveConfigID());
}

vector<Real> BaseCorrelationCurve::adjustForLosses(const vector<Real>& detachPoints) const {

    const auto& cId = spec_.curveConfigID();

    const auto& [qualifier, period] = splitCurveIdWithTenor(cId);

    DLOG("BaseCorrelationCurve::adjustForLosses: start adjusting for losses for base correlation " << qualifier);

    if (!referenceData_) {
        DLOG("Reference data manager is null so cannot adjust for losses.");
        return detachPoints;
    }

    if (!referenceData_->hasData(CreditIndexReferenceDatum::TYPE, qualifier)) {
        DLOG("Reference data manager does not have index credit data for " << qualifier
                                                                           << " so cannot adjust for losses.");
        return detachPoints;
    }

    auto crd = QuantLib::ext::dynamic_pointer_cast<CreditIndexReferenceDatum>(
        referenceData_->getData(CreditIndexReferenceDatum::TYPE, qualifier));
    if (!crd) {
        DLOG("Index credit data for " << qualifier << " is not of correct type so cannot adjust for losses.");
        return detachPoints;
    }

    // Process the credit index reference data
    Real totalRemainingWeight = 0.0;
    Real totalPriorWeight = 0.0;
    Real lost = 0.0;
    Real recovered = 0.0;

    for (const auto& c : crd->constituents()) {

        const auto& name = c.name();
        auto weight = c.weight();
        validateWeightRec(weight, name, "weight");

        if (!close(0.0, weight)) {
            totalRemainingWeight += weight;
        } else {
            auto priorWeight = c.priorWeight();
            QL_REQUIRE(priorWeight != Null<Real>(), "Expecting a valid prior weight for name " << name << ".");
            validateWeightRec(priorWeight, name, "prior weight");
            auto recovery = c.recovery();
            QL_REQUIRE(recovery != Null<Real>(), "Expecting a valid recovery for name " << name << ".");
            validateWeightRec(recovery, name, "recovery");
            lost += (1.0 - recovery) * priorWeight;
            recovered += recovery * priorWeight;
            totalPriorWeight += priorWeight;
        }
    }

    Real totalWeight = totalRemainingWeight + totalPriorWeight;
    TLOG("Total remaining weight = " << totalRemainingWeight);
    TLOG("Total prior weight = " << totalPriorWeight);
    TLOG("Total weight = " << totalWeight);

    if (!close(totalRemainingWeight, 1.0) && totalRemainingWeight > 1.0) {
        ALOG("Total remaining weight is greater than 1, possible error in CreditIndexReferenceDatum for " << qualifier);
    }

    if (!close(totalWeight, 1.0)) {
        ALOG("Expected the total weight (" << totalWeight << " = " << totalRemainingWeight << " + " << totalPriorWeight
                                           << ") to equal 1, possible error in CreditIndexReferenceDatum for "
                                           << qualifier);
    }

    if (close(totalRemainingWeight, 0.0)) {
        ALOG("The total remaining weight is 0 so cannot adjust for losses.");
        return detachPoints;
    }

    if (close(totalRemainingWeight, 1.0)) {
        DLOG("Index factor for " << qualifier << " is 1 so adjustment for losses not required.");
        return detachPoints;
    }

    // Index factor is less than 1 so need to adjust each of the detachment points.
    vector<Real> result;
    for (Size i = 0; i < detachPoints.size(); ++i) {

        // Amounts below, for tranche and above for original (quoted) attachment and detachment points.
        Real below = i == 0 ? 0.0 : detachPoints[i-1];
        Real tranche = detachPoints[i] - below;
        Real above = 1.0 - detachPoints[i];
        Real newBelow = max(below - lost, 0.0);
        Real newTranche = tranche - max(min(recovered - above, tranche), 0.0) - max(min(lost - below, tranche), 0.0);
        Real newDetach = (newBelow + newTranche) / totalRemainingWeight;

        TLOG("Quoted detachment point " << detachPoints[i] << " adjusted to " << newDetach << ".");

        if (i > 0 && (newDetach < result.back() || close(newDetach, result.back()))) {
            ALOG("The " << io::ordinal(i + 1) << " adjusted detachment point is not greater than the previous " <<
                "adjusted detachment point so cannot adjust for losses.");
            return detachPoints;
        }

        result.push_back(newDetach);
    }

    DLOG("BaseCorrelationCurve::adjustForLosses: finished adjusting for losses for base correlation " << qualifier);

    return result;
}

} // namespace data
} // namespace ore
