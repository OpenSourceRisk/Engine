#include <ql/quantlib.hpp>
#include <qle/quantext.hpp>

#include <boost/make_shared.hpp>

// example / tests for multicurrency lgm model

using namespace QuantLib;
using namespace QuantExt;

void nodelete() {}

int main() {

    // try {

    Date referenceDate(30, July, 2015);

    Settings::instance().evaluationDate() = referenceDate;

    // the single currency models
    // they can be calibrated in the usual way

    Handle<YieldTermStructure> eurYts(
        boost::make_shared<FlatForward>(referenceDate, 0.02, Actual365Fixed()));

    Handle<YieldTermStructure> usdYts(boost::make_shared<FlatForward>(
        referenceDate, 0.05, Actual365Fixed()));

    std::vector<Date> volstepdates;
    std::vector<Real> volsteptimes;
    Array volsteptimes_a(0);

    std::vector<Real> eurVols(1, atof(getenv("EURVOL")));
    Array eurVols_a(eurVols.begin(), eurVols.end());
    std::vector<Real> usdVols(1, atof(getenv("USDVOL")));
    Array usdVols_a(usdVols.begin(), usdVols.end());

    std::vector<Real> eurMr(1, atof(getenv("EURMR")));
    Array eurMr_a(eurMr.begin(), eurMr.end());
    std::vector<Real> usdMr(1, atof(getenv("USDMR")));
    Array usdMr_a(usdMr.begin(), usdMr.end());

    std::vector<Real> fxSigmas(1, atof(getenv("FXVOL")));
    Array fxSigmas_a(fxSigmas.begin(), fxSigmas.end());

    boost::shared_ptr<IrLgm1fParametrization> eurLgmP =
        boost::make_shared<IrLgm1fPiecewiseConstantParametrization>(
            EURCurrency(), eurYts, volsteptimes_a, eurVols_a, volsteptimes_a,
            eurMr_a);
    boost::shared_ptr<IrLgm1fParametrization> usdLgmP =
        boost::make_shared<IrLgm1fPiecewiseConstantParametrization>(
            USDCurrency(), usdYts, volsteptimes_a, usdVols_a, volsteptimes_a,
            usdMr_a);

    boost::shared_ptr<Lgm> eurLgm = boost::make_shared<Lgm>(eurLgmP);
    boost::shared_ptr<Lgm> usdLgm = boost::make_shared<Lgm>(usdLgmP);

    Handle<Quote> fxSpot(boost::make_shared<SimpleQuote>(0.9090));

    boost::shared_ptr<Parametrization> fxP =
        boost::make_shared<FxBsPiecewiseConstantParametrization>(
            USDCurrency(), fxSpot, volsteptimes_a, fxSigmas_a);

    std::vector<boost::shared_ptr<Parametrization> > parametrizations;
    parametrizations.push_back(eurLgmP);
    parametrizations.push_back(usdLgmP);
    parametrizations.push_back(fxP);

    Matrix c(3, 3);
    //  EUR             USD         FX
    c[0][0] = 1.0; c[0][1] = 0.80; c[0][2] = 0.30; // EUR
    c[1][0] = 0.80; c[1][1] = 1.0;  c[1][2] = -0.20; // USD
    c[2][0] = 0.30;  c[2][1] = -0.20; c[2][2] = 1.0; // FX

    boost::shared_ptr<XAssetModel> model =
        boost::make_shared<XAssetModel>(parametrizations, c);

    int exact = atoi(getenv("EXACT"));

    boost::shared_ptr<StochasticProcess> process = model->stateProcess(
        exact == 1 ? XAssetStateProcess::exact : XAssetStateProcess::euler);

    // generate paths

    Size n = atoi(getenv("N")); // N paths
    Time T = atof(getenv("T")); // cashflow time
    Size steps =
        static_cast<Size>(T * atof(getenv("STEPS"))); // STEPS steps per year
    Size seed = atoi(getenv("SEED"));                 // rng seed
    TimeGrid grid(T, steps);

    PseudoRandom::rsg_type sg =
        PseudoRandom::make_sequence_generator(steps * 3, seed);
    MultiPathGenerator<PseudoRandom::rsg_type> pg(process, grid, sg, false);

    // the usd process
    PseudoRandom::rsg_type sg2 =
        PseudoRandom::make_sequence_generator(steps, seed);
    PathGenerator<PseudoRandom::rsg_type> pg2(usdLgm->stateProcess(), grid, sg2,
                                              false);

    std::vector<Sample<MultiPath> > paths;
    for (Size j = 0; j < n; ++j) {
        paths.push_back(pg.next());
    }

    std::vector<Sample<Path> > paths2;
    for (Size j = 0; j < n; ++j) {
        paths2.push_back(pg2.next());
    }

    // output paths for visual inspection in gnuplot

    if (atoi(getenv("OUTPUT"))) {
        // cc model paths
        for (Size i = 0; i < paths[0].value[0].length(); ++i) {
            std::cout << grid[i] << " ";
            for (Size j = 0; j < n; ++j) {
                std::cout << std::exp(paths[j].value[2][i]) << " "
                          << paths[j].value[0][i] << " " << paths[j].value[1][i]
                          << " " << paths2[j].value[i] << " ";
            }
            std::cout << "\n";
        }
    }

    // test: 1 USD in 1y, priced in domestic measure

    Size l = paths[0].value[0].length() - 1;
    IncrementalStatistics stat, stat2;
    for (Size j = 0; j < n; ++j) {
        Real fx = std::exp(paths[j].value[2][l]);
        Real zeur = paths[j].value[0][l];
        Real zusd = paths[j].value[1][l];
        Real zusd2 = paths2[j].value[l];
        stat.add(1.0 * fx / eurLgm->numeraire(T, zeur));
        stat2.add(1.0 / usdLgm->numeraire(T, zusd2));
    }
    std::clog << "1 USD @ 1y  = " << stat.mean() << " EUR +/- "
              << stat.errorEstimate() << std::endl;
    ;
    std::clog << "curve price = " << usdYts->discount(T) << " spot "
              << fxSpot->value() << " EUR price "
              << usdYts->discount(T) * fxSpot->value() << "\n";

    std::clog << "1 USD @ 1y = " << stat2.mean() << " USD +/-"
              << stat2.errorEstimate() << std::endl;

    return 0;

    // } catch (QuantLib::Error e) {
    //     std::clog << "ql exception : " << e.what() << "\n";
    // } catch (std::exception e) {
    //     std::clog << "std exception: " << e.what() << "\n";
    // }
}
