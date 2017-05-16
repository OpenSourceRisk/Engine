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

#include <ql/math/comparison.hpp>
#include <ql/math/matrix.hpp>

using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

BaseCorrelationCurve::BaseCorrelationCurve(Date asof, BaseCorrelationCurveSpec spec, const Loader& loader,
                                           const CurveConfigurations& curveConfigs) {
    DLOG("base correlation curve called for " << spec.curveConfigID());
    try {

        const boost::shared_ptr<BaseCorrelationCurveConfig>& config =
            curveConfigs.baseCorrelationCurveConfig(spec.curveConfigID());

        // Read in quotes matrix
        vector<Period> terms = config->terms();
        vector<double> detachmentPoints = config->detachmentPoints();
        QL_REQUIRE(!terms.empty(), "At least one term > 0*Days expected");
        QL_REQUIRE(!detachmentPoints.empty(), "At least one detachment point expected");
        vector<vector<Handle<Quote>>> data(detachmentPoints.size(), vector<Handle<Quote>>(terms.size()));
        vector<vector<bool>> found(detachmentPoints.size(), vector<bool>(terms.size()));
        Size remainingQuotes = terms.size() * detachmentPoints.size();
        Size quotesRead = 0;

        for (auto& md : loader.loadQuotes(asof)) {
            if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::CDS_INDEX &&
                md->quoteType() == MarketDatum::QuoteType::BASE_CORRELATION) {
                boost::shared_ptr<BaseCorrelationQuote> q = boost::dynamic_pointer_cast<BaseCorrelationQuote>(md);
                if (q != NULL && q->cdsIndexName() == spec.curveConfigID()) {
                    quotesRead++;
                    Size i = std::find_if(detachmentPoints.begin(), detachmentPoints.end(), [&q](const double& s) {
                                 return close_enough(s, q->detachmentPoint());
                             }) - detachmentPoints.begin();
                    Size j = std::find(terms.begin(), terms.end(), q->term()) - terms.begin();
                    if (i < detachmentPoints.size() && j < terms.size()) {
                        data[i][j] = q->quote();
                        if (!found[i][j]) {
                            found[i][j] = true;
                            --remainingQuotes;
                        }
                    }
                }
            }
        }

        LOG("BaseCorrelation curve: read " << quotesRead << " quotes");

        // Check that we have all the data we need
        if (remainingQuotes != 0) {
            ostringstream m;
            m << "Not all quotes found for spec " << spec << endl;
            m << "Found base correlation data (*) and missing data (.):" << std::endl;
            for (Size i = 0; i < detachmentPoints.size(); ++i) {
                for (Size j = 0; j < terms.size(); ++j) {
                    m << (found[i][j] ? "*" : ".");
                }
                if (i < detachmentPoints.size() - 1)
                    m << endl;
            }
            LOG(m.str());
            QL_FAIL("could not build base correlation curve");
        }

	// FIXME: The QuantLib interpolator expects at least two terms, so add a second column, copying the first 
        if (terms.size() == 1) {
	  terms.push_back(terms[0] + 1 * Days); // arbitrary, but larger than the first term
            for (Size i = 0; i < detachmentPoints.size(); ++i)
                data[i].push_back(data[i][0]);
        }

        baseCorrelation_ = boost::make_shared<BilinearBaseCorrelationTermStructure>(
            config->settlementDays(), config->calendar(), config->businessDayConvention(), terms, detachmentPoints,
            data, config->dayCounter());

        baseCorrelation_->enableExtrapolation(config->extrapolate());

    } catch (std::exception& e) {
        QL_FAIL("base correlation curve building failed :" << e.what());
    } catch (...) {
        QL_FAIL("base correlation curve building failed: unknown error");
    }
    DLOG("base correlation curve built for " << spec.curveConfigID());
}
}
}
