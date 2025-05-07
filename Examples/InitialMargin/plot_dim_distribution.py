import pandas as pd
import matplotlib.pyplot as plt

df_dim = pd.read_csv("Output/Dim2/AmcCg/dim_distribution.csv")
df_simm = pd.read_csv("Output/DimValidation/simm_cube.csv")

#print("DIM dates: ", df_dim["Date"].unique())
#print("SIMM dates:", df_simm["AsOfDate"].unique())

dates = ["2025-03-10", "2026-02-10", "2029-02-12", "2033-02-10"]

fig = plt.figure(figsize=plt.figaspect(0.4))

for d in dates:
    print("plot date", d);
    df_dim_2 = df_dim[ df_dim["Date"] == d ]
    counts = df_dim_2['Count'].sum()
    delta = (df_dim_2['Bound'].max() - df_dim_2['Bound'].min()) / len(df_dim_2['Bound'])
    plt.plot(df_dim_2['Bound'], df_dim_2['Count'] / (delta * counts), linewidth=3, color="black")
    
    df_simm_2 = df_simm[ df_simm["AsOfDate"] == d ]
    x = df_simm_2["InitialMargin"]
    plt.hist(x, density=True, bins=15, label=d)

plt.legend(frameon=True, shadow=True)
plt.xlabel("SIMM")
plt.ylabel("Density")
plt.title("SIMM Distribution")

#plt.show()

file = "mpl_simm_distribution.pdf"
plt.savefig(file)
plt.close()

print("plot written to file:", file)
