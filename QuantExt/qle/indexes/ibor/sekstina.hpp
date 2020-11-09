#ifndef quantext_sekstina_hpp
#define quantext_sekstina_hpp

#include <ql/currencies/europe.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/sweden.hpp>
#include <ql/time/daycounters/actual360.hpp>

namespace QuantExt {
using namespace QuantLib;

//! %SEK STINA
/*! %SEK T/N rate

*/
class SEKStina : public OvernightIndex {
public:
    SEKStina(const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : OvernightIndex("SEK-STINA", 1, SEKCurrency(), Sweden(), Actual360(), h) {}
};
} // namespace QuantExt

#endif
