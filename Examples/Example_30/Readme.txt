In the example, a USD-Prime Curve is built and a Prime-LIBOR basis swap is processed.

The Example confirms that the bootstrapped curves USD-FedFunds and USD-Prime follow the
3% rule observed on the market:
U.S. Prime Rate = (The Fed Funds Target Rate + 3)
http://www.fedprimerate.com/

1) Portfolio

   - US Dollar Prime Rate vs 3 Month LIBOR Basis Swap, 10m notional, 20Y maturity,
   receive USD-Prime quarterly Act/360, pay USD-LIBOR-3M + s;

   - US Dollar 3 Month LIBOR vs Fed Funds Basis Swap, 10m notional, 20Y maturity,
   receive USD-Fedfunds + 0.027, pay USD-LIBOR-3M + s.

2) Market

   Market data for basis swaps retrieved from Thomson Reuters as of 2016-02-05.
   The quotes for LIBOR-FedFunds basis swap are saved as two alternative notations.

   To avoid a conflict in the quotes, an optional identifier as a penultimate token is introduced.
   For the US Dollar Prime Rate vs 3 Month LIBOR Basis Swap quotes, used for bootstrapping of the USD-Prime index,
   is used the notation:
           BASIS_SWAP/BASIS_SPREAD/3M/1D/USD/LIBOR_PRIME/maturity,

   to distinguish from the quotes for the US Dollar 3 Month LIBOR vs Fed Funds Basis Swap,
   for which a new notation is used:
           BASIS_SWAP/BASIS_SPREAD/3M/1D/USD/LIBOR_FEDFUNDS/maturity.
   The former notation:
           BASIS_SWAP/BASIS_SPREAD/3M/1D/USD/maturity,
   is still valid and to show this, it is used, too.

3) Conventions

   The convention on the Prime leg of the Prime-LIBOR basis swap has a daily weighted reset mechanism.
   For the yield curve stripping of the USD-Prime Index are used the same conventions as for the USD-FedFunds.

4) Analytics

   NPVs equal to 0 (Libor leg spread adjustment s = 0.024246), cashflows, curves, sensitivities.

5) Run Example

   python run.py
