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

#include <orea/simulation/fixingmanager.hpp>
#include <ored/utilities/flowanalysis.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/cashflows/cpicoupon.hpp>
#include <ql/cashflows/yoyinflationcoupon.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>

using namespace std;
using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;
using FlowList = std::vector<std::vector<std::string>>;

namespace ore {
namespace analytics {

//! Initialise the manager-		[4]	{px=0x000002b074c35030 {yoyIndex_={px=0x000002b074c324a0 {ratio_=true yoyInflation_={link_={px=0x000002b07526f190 {...} ...} } } ...} ...} ...}	boost::shared_ptr<QuantLib::CashFlow>

void FixingManager::initialise(const boost::shared_ptr<Portfolio>& portfolio) {

    // Get the flow list and required indices from the portfolio.
    vector<FlowList> flowList;
    set<boost::shared_ptr<Index>> setIndices;

    for (auto trade : portfolio->trades()) {
        for (auto leg : trade->legs()) {
            flowList.push_back(data::flowAnalysis(leg));
            for (auto cf : leg) {
                boost::shared_ptr<FloatingRateCoupon> frc = boost::dynamic_pointer_cast<FloatingRateCoupon>(cf);
                if (frc)
                    setIndices.insert(frc->index());

                boost::shared_ptr<FloatingRateFXLinkedNotionalCoupon> fc =
                    boost::dynamic_pointer_cast<FloatingRateFXLinkedNotionalCoupon>(cf);
                if (fc) {
                    setIndices.insert(fc->index());
                    setIndices.insert(fc->fxLinkedCashFlow().index());
                }

                boost::shared_ptr<FXLinkedCashFlow> flcf = boost::dynamic_pointer_cast<FXLinkedCashFlow>(cf);
                if (flcf)
                    setIndices.insert(flcf->index());

                boost::shared_ptr<CPICoupon> cpc = boost::dynamic_pointer_cast<CPICoupon>(cf);
                if (cpc)
                    setIndices.insert(cpc->index());

                boost::shared_ptr<InflationCoupon> ic = boost::dynamic_pointer_cast<InflationCoupon>(cf);
                if (ic)
                    setIndices.insert(ic->index());

                boost::shared_ptr<CPICashFlow> cpcf = boost::dynamic_pointer_cast<CPICashFlow>(cf);
                if (cpcf)
                    setIndices.insert(cpcf->index());

                // add more coupon types here ...
            }
        }
    }
    // convert to a vector
    indices_ = vector<boost::shared_ptr<Index>>(setIndices.begin(), setIndices.end());

    // first pass: init date vectors
    for (Size j = 0; j < flowList.size(); j++) {
        FlowList flow = flowList[j];
        QL_REQUIRE(flow.size() > 0, "flowList " << j << " is empty");
        for (Size i = 1; i < flow.size(); i++) { // skip header
            std::string indexName = flow[i][4];
            if (indexName != "#N/A" && fixingMap_.find(indexName) == fixingMap_.end()) {
                LOG("add index name for fixing history: " << indexName);
                fixingMap_[indexName] = vector<Date>();
            }
        }
    }

    // second pass: Add to date vectors
    for (Size j = 0; j < flowList.size(); j++) {
        FlowList flow = flowList[j];
        QL_REQUIRE(flow.size() > 0, "flowList " << j << " is empty");
        for (Size i = 1; i < flow.size(); i++) { // skip header
            std::string indexName = flow[i][4];
            if (indexName != "#N/A") {
                Date fixingDate = ore::data::parseDate(flow[i][3]);
                fixingMap_[indexName].push_back(fixingDate);
            }
        }
    }

    // third pass: sort date vectors and remove duplicates
    for (auto it = fixingMap_.begin(); it != fixingMap_.end(); ++it) {
        std::sort(it->second.begin(), it->second.end());
        // using default comparison:
        vector<Date>::iterator it2 = std::unique(it->second.begin(), it->second.end());
        it->second.resize(it2 - it->second.begin());
    }

    // Now cache the original fixings so we can re-write on reset()
    for (auto index : indices_) {
        string qlIndexName = index->name();
        if (fixingMap_.find(qlIndexName) != fixingMap_.end()) {
            fixingCache_[qlIndexName] = IndexManager::instance().getHistory(qlIndexName);
        }
    }
}

//! Update fixings to date d
void FixingManager::update(Date d) {
    if (!fixingMap_.empty()) {
        QL_REQUIRE(d >= fixingsEnd_, "Can't go back in time, fixings must be reset."
                                     " Update date "
                                         << d << " but current fixings go to " << fixingsEnd_);
        applyFixings(fixingsEnd_, d);
    }
    fixingsEnd_ = d;
}

//! Reset fixings to t0 (today)
void FixingManager::reset() {
    if (modifiedFixingHistory_) {
        for (auto& kv : fixingCache_)
            IndexManager::instance().setHistory(kv.first, kv.second);
        modifiedFixingHistory_ = false;
    }
    fixingsEnd_ = today_;
}

void FixingManager::applyFixings(Date start, Date end) {
    // Loop over all indices
    for (const boost::shared_ptr<Index>& index : indices_) {
        string qlIndexName = index->name();
        // Only need to apply fixings if the index is in fixingMap
        if (fixingMap_.find(qlIndexName) != fixingMap_.end()) {

            boost::shared_ptr<ZeroInflationIndex> zii = boost::dynamic_pointer_cast<ZeroInflationIndex>(index);
            boost::shared_ptr<YoYInflationIndex> yii = boost::dynamic_pointer_cast<YoYInflationIndex>(index);
            vector<Date> fixingDates = fixingMap_[qlIndexName];
            Date currentFixingDate;
            Date fixStart = start;
            Date fixEnd = end;
            if (zii) {// for inflation indices we just only add a fixing for the first date in the month
                fixStart = inflationPeriod(fixStart - zii->zeroInflationTermStructure()->observationLag(), zii->frequency()).first;
                fixEnd = inflationPeriod(fixEnd - zii->zeroInflationTermStructure()->observationLag(), zii->frequency()).first + 1;
                currentFixingDate = fixEnd;
                for (Size i = 0; i < fixingDates.size(); i++) {
                    fixingDates[i] = inflationPeriod(fixingDates[i], zii->frequency()).first;
                }
            }
            else if (yii) {
                fixStart = inflationPeriod(fixStart - yii->yoyInflationTermStructure()->observationLag(), yii->frequency()).first;
                fixEnd = inflationPeriod(fixEnd - yii->yoyInflationTermStructure()->observationLag(), yii->frequency()).first + 1;
                currentFixingDate = fixEnd;
                for (Size i = 0; i<fixingDates.size(); i++)
                    fixingDates[i] = inflationPeriod(fixingDates[i], yii->frequency()).first;
            }
            else
                currentFixingDate = index->fixingCalendar().adjust(fixEnd, Following);

            // Add we have a coupon between start and asof.
            bool needFixings = false;
            for (Size i = 0; i < fixingDates.size(); i++) {
                if (fixingDates[i] >= fixStart && fixingDates[i] < fixEnd) {
                    needFixings = true;
                    break;
                }
            }

            if (needFixings) {
                Rate currentFixing = index->fixing(currentFixingDate);
                TimeSeries<Real> history;
                for (Size i = 0; i < fixingDates.size(); i++) {
                    if (fixingDates[i] >= fixStart && fixingDates[i] < fixEnd) {
                        history[fixingDates[i]] = currentFixing;
                        modifiedFixingHistory_ = true;
                    }
                    if (fixingDates[i] >= fixEnd)
                        break;
                }
                index->addFixings(history, true);
            }
        }
    }
}
} // namespace analytics
} // namespace ore
