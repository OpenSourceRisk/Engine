#include <ql/math/interpolation.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <qle/math/flatextrapolation.hpp>

#include <iostream>

using namespace QuantLib;
using namespace QuantExt;

int main() {

    std::vector<Real> x, y;
    x.push_back(1.0);
    x.push_back(2.0);
    x.push_back(3.0);
    y.push_back(1.0);
    y.push_back(2.0);
    y.push_back(4.0);

    boost::shared_ptr<Interpolation> lin =
        boost::make_shared<LinearInterpolation>(x.begin(), x.end(), y.begin());

    boost::shared_ptr<Interpolation> flin =
        boost::make_shared<FlatExtrapolation>(lin);

    flin->enableExtrapolation();

    try {

        Real tmp = 0.0;
        while (tmp < 4.0) {
            std::cout << tmp << " " << (*flin)(tmp) << " "
                      << flin->primitive(tmp) << " " << flin->derivative(tmp)
                      << " " << flin->secondDerivative(tmp) << std::endl;
            tmp += 0.1;
        }

    } catch (QuantLib::Error e) {
        std::cout << "ql exception: " << e.what() << std::endl;
    }

    return 0;
}
