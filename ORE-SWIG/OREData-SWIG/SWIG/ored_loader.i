/*
 Copyright (C) 2019, 2020 Quaternion Risk Management Ltd
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

#ifndef ored_loader_i
#define ored_loader_i

%include ored_marketdatum.i
%include std_set.i

%shared_ptr(ore::data::Loader)
%shared_ptr(ore::data::CSVLoader)
%shared_ptr(ore::data::InMemoryLoader)

namespace ore {
namespace data {

class Wildcard {
  public:
    Wildcard(const std::string& pattern, const bool usePrefixes = true,
             const bool aggressivePrefixes = false);

    bool hasWildcard() const;
    std::size_t wildcardPos() const;
    bool isPrefix() const;
    bool matches(const std::string& s) const;

    const std::string& pattern() const;
    const std::string& regex() const;
    const std::string& prefix() const;

    bool usePrefixes() const;
    bool aggressivePrefixes() const;
};

class Loader {
  private:
    Loader();
  public:
    std::vector<ext::shared_ptr<ore::data::MarketDatum>> loadQuotes(const QuantLib::Date&) const;
    ext::shared_ptr<ore::data::MarketDatum> get(const std::string& name, const QuantLib::Date&) const;
    ext::shared_ptr<ore::data::MarketDatum> get(const std::pair<std::string, bool>& name, const QuantLib::Date& d) const;
    std::set<ext::shared_ptr<ore::data::MarketDatum>> get(const std::set<std::string>& names, const QuantLib::Date& asof) const;
    std::set<ext::shared_ptr<ore::data::MarketDatum>> get(const ore::data::Wildcard& wildcard, const QuantLib::Date& asof) const;
    bool has(const std::string& name, const QuantLib::Date& d) const;
    bool hasQuotes(const QuantLib::Date& d) const;
    std::set<ore::data::Fixing> loadFixings() const;

    %extend {
      std::vector<ext::shared_ptr<ore::data::MarketDatum>> getByPattern(
        const std::string& pattern, const QuantLib::Date& asof,
        const bool usePrefixes = true,
        const bool aggressivePrefixes = false) const {
        const ore::data::Wildcard wildcard(pattern, usePrefixes,
                         aggressivePrefixes);
        const auto quotes = self->get(wildcard, asof);
        return std::vector<ext::shared_ptr<ore::data::MarketDatum>>(
          quotes.begin(), quotes.end());
      }
    }
};
} // namespace data
} // namespace ore

%template(StringBoolPair) std::pair<std::string, bool>;
%template(MarketDatumVector) std::vector<ext::shared_ptr<ore::data::MarketDatum>>;
%template(MarketDatumSet) std::set<ext::shared_ptr<ore::data::MarketDatum>>;

// Fixing class has no default ctor, excluding some features of std::vector
%ignore std::vector<ore::data::Fixing>::pop;
%ignore std::vector<ore::data::Fixing>::resize;
%ignore std::vector<ore::data::Fixing>::vector(size_type);
%template(FixingVector) std::vector<ore::data::Fixing>;
%template(FixingSet) std::set<ore::data::Fixing>;

namespace ore {
namespace data {
class CSVLoader : public ore::data::Loader {
  public:
    CSVLoader(const std::string& marketFilename, const std::string& fixingFilename,
              bool implyTodaysFixings = false);
    CSVLoader(const std::vector<std::string>& marketFiles, const std::vector<std::string>& fixingFiles,
              bool implyTodaysFixings = false);
};

class InMemoryLoader : public ore::data::Loader {
  public:
    InMemoryLoader();
    void add(QuantLib::Date date, const std::string& name, QuantLib::Real value);
    void addFixing(QuantLib::Date date, const std::string& name, QuantLib::Real value);
};

struct Fixing {
    QuantLib::Date date;
    std::string name;
    QuantLib::Real fixing;
    Fixing(const QuantLib::Date& d, const std::string& s, const QuantLib::Real f);
};

} // namespace data
} // namespace ore

#endif
