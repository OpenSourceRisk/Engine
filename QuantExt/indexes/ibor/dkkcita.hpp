/*! \file dkkcita.hpp
    \brief DKK T/N rate
    \ingroup indexes
*/

#ifndef quantext_dkkcita_hpp
#define quantext_dkkcita_hpp

#include <ql/currencies/europe.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/denmark.hpp>
#include <ql/time/daycounters/actual360.hpp>

namespace QuantExt {
using namespace QuantLib;

//! %DKK CITA
/*! %DKK T/N rate

\remark Using Denmark calendar.

\ingroup indexes
*/
class DKKCita : public OvernightIndex {
public:
    DKKCita(const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : OvernightIndex("DKK-CITA", 1, DKKCurrency(), Denmark(), Actual360(), h) {}
};
} // namespace QuantExt

#endif
