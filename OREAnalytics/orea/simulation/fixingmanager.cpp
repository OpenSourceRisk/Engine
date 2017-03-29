#include <orea/simulation/fixingmanager.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/flowanalysis.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>

using namespace std;
using namespace QuantLib;
using namespace QuantExt;
using FlowList = std::vector<std::vector<std::string>>;

namespace ore {
namespace analytics {

//! Initialise the manager
void FixingManager::initialise(const boost::shared_ptr<Portfolio>& portfolio) {

    // Get the flow list and required indices from the portfolio.
    vector<FlowList> flowList;
    set<boost::shared_ptr<Index>> setIndices;

    for (auto trade : portfolio->trades()) {
        for (auto leg : trade->legs()) {
            flowList.push_back(data::flowAnalysis(leg));
            for (auto cf : leg) {

                //
                boost::shared_ptr<IborCoupon> ic = boost::dynamic_pointer_cast<IborCoupon>(cf);
                if (ic)
                    setIndices.insert(ic->iborIndex());

                boost::shared_ptr<FloatingRateFXLinkedNotionalCoupon> fc =
                    boost::dynamic_pointer_cast<FloatingRateFXLinkedNotionalCoupon>(cf);
                if (fc) {
                    setIndices.insert(fc->index());
                    setIndices.insert(fc->fxLinkedCashFlow().index());
                }
                boost::shared_ptr<FXLinkedCashFlow> flcf = boost::dynamic_pointer_cast<FXLinkedCashFlow>(cf);
                if (flcf)
                    setIndices.insert(flcf->index());
            }
        }
    }
    // convert to a vector
    indices_ = vector<boost::shared_ptr<Index>> (setIndices.begin(), setIndices.end());

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
    QL_REQUIRE(d > fixingsEnd_, "Can't go back in time, fixings must be reset");
    applyFixings(fixingsEnd_, d);
    fixingsEnd_ = d;
}

//! Reset fixings to t0 (today)
void FixingManager::reset() {
    for (auto& kv : fixingCache_)
        IndexManager::instance().setHistory(kv.first, kv.second);

    fixingsEnd_ = today_;
}

void FixingManager::applyFixings(Date start, Date end) {
    // Loop over all indices
    for (const boost::shared_ptr<Index>& index : indices_) {
        string qlIndexName = index->name();
        // Only need to apply fixings if the index is in fixingMap
        if (fixingMap_.find(qlIndexName) != fixingMap_.end()) {

            // Add we have a coupon between start and asof.
            bool needFixings = false;
            vector<Date>& fixingDates = fixingMap_[qlIndexName];
            for (Size i = 0; i < fixingDates.size(); i++) {
                if (fixingDates[i] >= start && fixingDates[i] < end) {
                    needFixings = true;
                    break;
                }
            }

            if (needFixings) {
                Date currentFixingDate = index->fixingCalendar().adjust(end, Following);
                Rate currentFixing = index->fixing(currentFixingDate);
                TimeSeries<Real> history;
                vector<Date>& fixingDates = fixingMap_[qlIndexName];
                for (Size i = 0; i < fixingDates.size(); i++) {
                    if (fixingDates[i] >= start && fixingDates[i] < end)
                        history[fixingDates[i]] = currentFixing;
                    if (fixingDates[i] >= end)
                        break;
                }
                index->addFixings(history, true);
            }
        }
    }
}


}
}
