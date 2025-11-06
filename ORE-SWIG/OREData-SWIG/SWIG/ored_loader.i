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

%{
using ore::data::Loader;
using ore::data::Fixing;
using ore::data::Wildcard;
%}

%shared_ptr(Loader)
class Loader {
  private:
    Loader();
  public:
    std::vector<ext::shared_ptr<MarketDatum>> loadQuotes(const QuantLib::Date&) const;
    ext::shared_ptr<MarketDatum> get(const std::string& name, const QuantLib::Date&) const;
    ext::shared_ptr<MarketDatum> get(const std::pair<std::string, bool>& name, const QuantLib::Date& d) const;
    std::set<ext::shared_ptr<MarketDatum>> get(const std::set<std::string>& names, const QuantLib::Date& asof) const;
    std::set<ext::shared_ptr<MarketDatum>> get(const Wildcard& wildcard, const QuantLib::Date& asof) const;
    bool has(const std::string& name, const QuantLib::Date& d) const;
    bool hasQuotes(const QuantLib::Date& d) const;
    std::set<Fixing> loadFixings() const;
};
%template(StringBoolPair) std::pair<std::string, bool>;
%template(MarketDatumVector) std::vector<ext::shared_ptr<MarketDatum>>;

// Fixing class has no default ctor, excluding some features of std::vector
%ignore std::vector<Fixing>::pop;
%ignore std::vector<Fixing>::resize;
%ignore std::vector<Fixing>::vector(size_type);
%template(FixingVector) std::vector<Fixing>;
%template(FixingSet) std::set<Fixing>;

%{
using ore::data::CSVLoader;
%}

%shared_ptr(CSVLoader)
class CSVLoader : public Loader {
  public:
    CSVLoader(const std::string& marketFilename, const std::string& fixingFilename,
              bool implyTodaysFixings = false);
    CSVLoader(const std::vector<std::string>& marketFiles, const std::vector<std::string>& fixingFiles,
              bool implyTodaysFixings = false);
};

%{
using ore::data::InMemoryLoader;
%}

%shared_ptr(InMemoryLoader)
class InMemoryLoader : public Loader {
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

#endif
