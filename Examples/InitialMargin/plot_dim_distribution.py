import matplotlib.pyplot as plt
import pandas as pd

df = pd.read_csv("Output/Dim2/AmcCg/dim_distribution.csv")

#timeSteps = df["TimeStep"].unique()
#print(timeSteps)

steps = [0, 2, 10, 20, 50, 100, 118]

for s in steps:
    df2 = df[ df["TimeStep"] == s ]
    plt.plot(df2['Bound'], df2['Count'], label="time step " + str(s), linewidth=2)

plt.legend(frameon=False)
plt.xlabel("SIMM")
plt.ylabel("Count")
plt.title("SIMM Distribution")
plt.show()

