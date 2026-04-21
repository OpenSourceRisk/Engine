/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#ifndef ored_portfolio_core_i
#define ored_portfolio_core_i

%include cashflows.i
%include ored_xmlutils.i

%{
typedef ore::data::Portfolio Portfolio;
typedef ore::data::Envelope Envelope;
typedef ore::data::MarketContext MarketContext;
typedef ore::data::EngineData EngineData;
typedef ore::data::ReferenceDataManager ReferenceDataManager;
typedef ore::data::LegBuilder LegBuilder;
typedef ore::data::EngineBuilder EngineBuilder;
typedef ore::data::EngineFactory EngineFactory;
typedef ore::data::Market Market;
typedef ore::data::IborFallbackConfig IborFallbackConfig;
typedef ore::data::Trade Trade;
typedef ore::data::TradeFactory TradeFactory;
typedef ore::data::InstrumentWrapper InstrumentWrapper;
typedef ore::data::NettingSetDetails NettingSetDetails;
%}

%include std_string.i
// Apply std::string typemaps to the QuantLib 'string' typedef so SWIG uses
// SWIG_AsPtr_std_string (str↔string) rather than SWIG_ConvertPtr (pointer route).
%apply std::string { string };
%apply std::string & { string & };
%apply const std::string & { const string & };

%template(TradeVector) std::vector<ext::shared_ptr<ore::data::Trade>>;
%template(StringStringMap) std::map<std::string, std::string>;
%template(StringTradeMap) std::map<std::string, ext::shared_ptr<ore::data::Trade>>;
%shared_ptr(ore::data::EngineData)
%shared_ptr(ore::data::LegBuilder)
%shared_ptr(ore::data::EngineBuilder)
%shared_ptr(ore::data::EngineFactory)
%shared_ptr(ore::data::TradeFactory)
%shared_ptr(ore::data::Envelope)
%shared_ptr(ore::data::InstrumentWrapper)
%shared_ptr(ore::data::Trade)
%shared_ptr(ore::data::Portfolio)

%{
template<typename T>
std::vector<T> VECTOR_SWIG_TO_ORE(const std::vector<ext::shared_ptr<T>>& v) {
    std::vector<T> ret;
    for (const ext::shared_ptr<T>& t : v)
        ret.push_back(*t);
    return ret;
}
%}

// Macro: add a typemap so Python lists/sequences are accepted wherever SWIG
// expects vector<ext::shared_ptr<CppType>>.  The macro must be invoked after
// %shared_ptr(CppType) and the %template(...) declaration for that type.
// We also provide a freearg typemap that suppresses SWIG's default cleanup
// (`if (SWIG_IsNewObj(resN)) delete argN;`) which references variables our
// custom `in` typemap does not declare.
%define SWIG_SHARED_PTR_VECTOR_TYPEMAP(CppType, SwigTypeName)
%typemap(in) std::vector<ext::shared_ptr<CppType> >
    (std::vector<ext::shared_ptr<CppType> > tmp) {
  void* vptr = 0;
  int vres = SWIG_ConvertPtr(
    $input, &vptr, $descriptor(std::vector<ext::shared_ptr<CppType> > *), 0
  );
  if (SWIG_IsOK(vres) && vptr) {
    $1 = *reinterpret_cast<std::vector<ext::shared_ptr<CppType> >*>(vptr);
  } else {
    if (!PySequence_Check($input)) {
        PyErr_SetString(PyExc_TypeError, "Expected a Python sequence for " #CppType " vector");
        SWIG_fail;
    }
    Py_ssize_t sz = PySequence_Size($input);
    for (Py_ssize_t i = 0; i < sz; ++i) {
        PyObject* item = PySequence_GetItem($input, i);
        void* eptr = 0;
        int eres = SWIG_ConvertPtr(item, &eptr, $descriptor(ext::shared_ptr<CppType> *), 0);
        Py_DECREF(item);
        if (!SWIG_IsOK(eres) || !eptr) {
            PyErr_Format(PyExc_TypeError, "Element %zd of sequence is not a " #CppType " instance", i);
            SWIG_fail;
        }
        tmp.push_back(*reinterpret_cast<ext::shared_ptr<CppType>*>(eptr));
    }
    $1 = tmp;
      }
}
%typemap(freearg) std::vector<ext::shared_ptr<CppType> > ""
%typemap(in) const std::vector<ext::shared_ptr<CppType> >&
    (std::vector<ext::shared_ptr<CppType> > tmp) {
      void* vptr = 0;
      int vres = SWIG_ConvertPtr(
        $input, &vptr, $descriptor(std::vector<ext::shared_ptr<CppType> > *), 0
      );
      if (SWIG_IsOK(vres) && vptr) {
        $1 = reinterpret_cast<std::vector<ext::shared_ptr<CppType> >*>(vptr);
      } else {
    if (!PySequence_Check($input)) {
        PyErr_SetString(PyExc_TypeError, "Expected a Python sequence for " #CppType " vector");
        SWIG_fail;
    }
    Py_ssize_t sz = PySequence_Size($input);
    for (Py_ssize_t i = 0; i < sz; ++i) {
        PyObject* item = PySequence_GetItem($input, i);
        void* eptr = 0;
        int eres = SWIG_ConvertPtr(item, &eptr, $descriptor(ext::shared_ptr<CppType> *), 0);
        Py_DECREF(item);
        if (!SWIG_IsOK(eres) || !eptr) {
            PyErr_Format(PyExc_TypeError, "Element %zd of sequence is not a " #CppType " instance", i);
            SWIG_fail;
        }
        tmp.push_back(*reinterpret_cast<ext::shared_ptr<CppType>*>(eptr));
    }
    $1 = &tmp;
      }
}
%typemap(freearg) const std::vector<ext::shared_ptr<CppType> >& ""
%typemap(in) vector<ext::shared_ptr<CppType> > = std::vector<ext::shared_ptr<CppType> >;
%typemap(in) const vector<ext::shared_ptr<CppType> >& = const std::vector<ext::shared_ptr<CppType> >&;
%enddef

%template(InstrumentWrapperVector) std::vector<ext::shared_ptr<ore::data::InstrumentWrapper>>;
SWIG_SHARED_PTR_VECTOR_TYPEMAP(ore::data::InstrumentWrapper, InstrumentWrapperVector)

namespace ore {
namespace data {

enum class MarketContext { irCalibration, fxCalibration, eqCalibration, pricing };

class EngineData : public XMLSerializable {
public:
    EngineData();
    bool hasProduct(const std::string& productName);
    const std::string& model(const std::string& productName) const;
    const std::map<std::string, std::string>& modelParameters(const std::string& productName) const;
    const std::string& engine(const std::string& productName) const;
    const std::map<std::string, std::string>& engineParameters(const std:: string& productName) const;
    const std::map<std::string, std::string>& globalParameters() const;
    std::vector<std::string> products() const;
    void setModel(const std::string& productName, const std::string& model) { model_[productName] = model; }
    void setModelParameters(const std::string& productName, const std::map<std::string, std::string>& params) { modelParams_[productName] = params; }
    void setEngine(const std::string& productName, const std::string& engine) { engine_[productName] = engine; }
    void setEngineParameters(const std::string& productName, const std::map<std::string, std::string>& params) { engineParams_[productName] = params; }
    void setGlobalParameter(const std::string& name, const std::string& param) { globalParams_[name] = param; }
    void clear();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class LegBuilder {
  private:
    LegBuilder(const std::string& legType);
};

class EngineBuilder {
  public:
    EngineBuilder(const std::string& model, const std::string& engine,
                  const std::set<std::string>& tradeTypes);
};

class EngineFactory {
  public:
   EngineFactory(const ext::shared_ptr<ore::data::EngineData>& data,
          const ext::shared_ptr<Market>& market,
            const std::map<ore::data::MarketContext, std::string>& configurations = std::map<ore::data::MarketContext, std::string>(),
            const ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData = nullptr,
            const ext::shared_ptr<ore::data::IborFallbackConfig>& iborFallbackConfig =
              ext::make_shared<ore::data::IborFallbackConfig>(ore::data::IborFallbackConfig::defaultConfig()),
            const std::vector<ext::shared_ptr<ore::data::EngineBuilder>> extraEngineBuilders = {});
};

class TradeFactory {
public:
};

class Envelope : public XMLSerializable {
public:
    Envelope();
    Envelope(const std::string& counterparty, const std::string& nettingSetId, const std::set<std::string>& portfolioIds = std::set<std::string>());
  Envelope(const std::string& counterparty, const ore::data::NettingSetDetails& nettingSetDetails = ore::data::NettingSetDetails(),
             const std::set<std::string>& portfolioIds = std::set<std::string>());
    Envelope(const std::string& counterparty, const std::map<std::string, std::string>& additionalFields);
    Envelope(const std::string& counterparty, const std::string& nettingSetId, const std::map<std::string, std::string>& additionalFields,
             const std::set<std::string>& portfolioIds = std::set<std::string>());
  Envelope(const std::string& counterparty, const ore::data::NettingSetDetails& nettingSetDetails,
             const std::map<std::string, std::string>& additionalFields, const std::set<std::string>& portfolioIds = std::set<std::string>());

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const std::string& counterparty() const;
    const std::string& nettingSetId() const;
    const ore::data::NettingSetDetails nettingSetDetails() const;
    const std::set<std::string>& portfolioIds() const;
    const std::map<std::string, std::string> additionalFields() const;
    const std::map<std::string, QuantLib::ext::any>& fullAdditionalFields() const;
    std::string additionalField(const std::string& name, const bool mandatory = true,
                  const std::string& defaultValue = std::string()) const;
    QuantLib::ext::any additionalAnyField(const std::string& name, const bool mandatory = true,
                                  const QuantLib::ext::any& defaultValue = boost::none) const;
    void setAdditionalField(const std::string& key, const QuantLib::ext::any& value);
    bool initialized() const;
    bool hasNettingSetDetails() const;
};

class InstrumentWrapper {
  private:
    InstrumentWrapper();
  public:
    QuantLib::Real NPV() const;
    QuantLib::ext::shared_ptr<QuantLib::Instrument> qlInstrument() const;
};

class Trade : public XMLSerializable {
  private:
    Trade();
  public:
    const std::string& id();
    void setId(const std::string& id);
    const std::string& tradeType();
    const ext::shared_ptr<ore::data::InstrumentWrapper>& instrument();
    std::vector<std::vector<QuantLib::ext::shared_ptr<QuantLib::CashFlow>>> legs();
    const ore::data::Envelope& envelope() const;
    const QuantLib::Date& maturity();
    QuantLib::Real notional();
};

class Portfolio : public XMLSerializable {
  public:
    Portfolio(bool buildFailedTrades = true);
    std::size_t size() const;
    std::set<std::string> ids() const;
    void add(const ext::shared_ptr<ore::data::Trade>& trade);
    bool has(const std::string& id);
    ext::shared_ptr<ore::data::Trade> get(const std::string& id) const;
    const std::map<std::string, ext::shared_ptr<ore::data::Trade>>& trades() const;
    bool remove(const std::string& tradeID);
    void fromFile(const std::string& fileName);
    void fromXMLString(const std::string& xmlString);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    std::string toXMLString();
    void build(const ext::shared_ptr<ore::data::EngineFactory>& factory,
               const std::string& context = "unspecified",
               const bool emitStructuredError = true);
 };

  } // namespace data
  } // namespace ore

#endif
