XVA Stress Calculation with classic and AMC engine

Portfolio and Market Data from Example 39

It also contains two market data inputs with shifted par quotes to replicate the two stress scenarios

Stresstests:

- shift euribor 6m curve all par rate flat 200 bps up 
- shift eonia curve 200 all par rates flat 200 bps up

Run:

ore ./Input/ore_amc.xml for AMC 
ore ./Input/ore_classic.xml for classical XVA engine


