1) Portfolio

   - fixed rate bond
   - floating rate bond
   - bond switching from fixed to floating
   - bond with 'fixed amount' amortisation
   - bond with percentage amortisation relative to the initial notional
   - bond with percentage amortisation relative to the previous notional
   - bond with fixed annuity amortisation
   - bond with floating annuity amortisation
     (needs QuantLib 1.10, virtual amount() method the Coupon class)
   - bond with fixed amount amortisation followed by percentage
     amortisation relative to previous notional

2) Market

   Flat 6m Euribor Swap curve at 2%

3) Pricing

   Discounted cash flows with discounting risky bond engine taking
   a benchmark yield curve, a default term structure and an individual
   security spread into account

4) Analytics

   NPVs and cash flows

5) Run Example

   python run.py
