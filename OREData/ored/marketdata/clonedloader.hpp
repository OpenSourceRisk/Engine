/*
  Copyright (C) 2020 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <ored/marketdata/inmemoryloader.hpp>

#pragma once

namespace oreplus {
namespace data {

class ClonedLoader : public ore::data::InMemoryLoader {

public:
    // Default constructor
    ClonedLoader(const QuantLib::Date& loaderDate, const boost::shared_ptr<Loader>& inLoader);

    //! Load market quotes
    const std::vector<boost::shared_ptr<ore::data::MarketDatum>>& loadQuotes(const QuantLib::Date& d) const override;

    //! Get a particular quote by its unique name
    const boost::shared_ptr<ore::data::MarketDatum>& get(const std::string& name, const QuantLib::Date& d) const override;

    //! Date getter
    const QuantLib::Date& getLoaderDate() const { return loaderDate_; };

    //! No fixings in ClonedLoader
    const std::vector<ore::data::Fixing>& loadFixings() const override {
        static const std::vector<ore::data::Fixing> empty;
        return empty;
    }

protected:
    QuantLib::Date loaderDate_;
    std::map<std::string, boost::shared_ptr<ore::data::MarketDatum>> data_;
    std::vector<boost::shared_ptr<ore::data::MarketDatum>> datums_;
};

} // namespace data
} // namespace oreplus
