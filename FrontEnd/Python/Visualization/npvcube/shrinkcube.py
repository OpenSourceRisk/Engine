import pandas as pd
import numpy as np

csv_file = 'cube_full.csv'
df = pd.read_csv(csv_file)
df.columns = ['TradeID', 'DateIndex', 'Date', 'Sample', 'Value']

values = df.loc[np.in1d(df['TradeID'], ['722963AI', '833397AI']) & (df['Sample'] <= 30)]

print(values)

df2 = pd.DataFrame(values, index=None)
df2.columns = ['TradeID', 'DateIndex', 'Date', 'Sample', 'Value']
print(df2.head())
df2.to_csv('cube.csv', index=None)