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
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/matrix.hpp>

using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

BaseCorrelationCurve::BaseCorrelationCurve(
    Date asof,
    BaseCorrelationCurveSpec spec,
    const Loader& loader,
    const CurveConfigurations& curveConfigs) {

    DLOG("BaseCorrelationCurve: start building base correlation structure with ID " << spec.curveConfigID());
    
    try {

        const auto& config = *curveConfigs.baseCorrelationCurveConfig(spec.curveConfigID());

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
            QL_REQUIRE(dpStrs.size() == 1, "BaseCorrelationCurve: only one wild card " <<
                "detachment point can be specified.");
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
            return k_1.first < k_2.first && (!close_enough(k_1.second, k_2.second) && k_1.second < k_2.second);
        };
        map<pair<Period, Real>, Handle<Quote>> data;

        for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {

            // Go to next quote if the market data point's date does not equal to asof.
            if (md->asofDate() != asof)
                continue;

            // Go to next quote if not a base correlation quote.
            auto q = boost::dynamic_pointer_cast<BaseCorrelationQuote>(md);
            if (!q)
                continue;

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
                DLOG("Already added base correlation with term " << q->term() << " and detachment point " <<
                    fixed << setprecision(9) << q->detachmentPoint() << " so skipping quote " << q->name());
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

        DLOG("After processing the quotes, we have " << terms.size() << " unique term(s), " <<
            dps.size() << " unique detachment points and " << data.size() << " quotes.");
        QL_REQUIRE(dps.size() > 1, "BaseCorrelationCurve: need at least 2 unique detachment points.");
        QL_REQUIRE(dps.size() * terms.size() == data.size(), "BaseCorrelationCurve: number of quotes (" <<
            data.size() << ") should equal number of detachment points (" << dps.size() <<
            ") times the number of terms (" << terms.size() << ").");

        // Need to now fill _completely_ the (number of dps) x (number of terms) quotes surface.
        vector<vector<Handle<Quote>>> quotes;
        for (const auto& dp : dps) {
            quotes.push_back({});
            for (const auto& term : terms) {
                auto key = make_pair(term, dp);
                auto it = data.find(key);
                QL_REQUIRE(it != data.end(), "BaseCorrelationCurve: do not have a quote for term " << term <<
                    " and detachment point " << fixed << setprecision(9) << dp << ".");
                quotes.back().push_back(it->second);
            }
        }

        // Need a vector of terms and detachment points for ctor below
        vector<Period> tmpTerms(terms.begin(), terms.end());
        vector<Real> tmpDps(dps.begin(), dps.end());

        // The QuantLib interpolator expects at least two terms, so add a second column, copying the first
        if (tmpTerms.size() == 1) {
            tmpTerms.push_back(tmpTerms[0] + 1 * tmpTerms[0].units());
            for (Size i = 0; i < tmpDps.size(); ++i)
                quotes[i].push_back(quotes[i][0]);
        }

        baseCorrelation_ = boost::make_shared<BilinearBaseCorrelationTermStructure>(
            config.settlementDays(), config.calendar(), config.businessDayConvention(), tmpTerms,
            tmpDps, quotes, config.dayCounter());

        baseCorrelation_->enableExtrapolation(config.extrapolate());

    } catch (std::exception& e) {
        QL_FAIL("BaseCorrelationCurve: curve building failed :" << e.what());
    } catch (...) {
        QL_FAIL("BaseCorrelationCurve: curve building failed: unknown error");
    }

    DLOG("BaseCorrelationCurve: finished building base correlation structure with ID " << spec.curveConfigID());
}

} // namespace data
} // namespace ore
