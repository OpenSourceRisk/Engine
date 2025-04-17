import numpy as np

from sklearn.linear_model import LinearRegression
from sklearn.preprocessing import PolynomialFeatures  

def conditional_expectation(x,y):
    X = np.array(x)
    Y = np.array(y)
    poly = PolynomialFeatures(degree=4)  
    X_pre = poly.fit_transform(X)
    model = LinearRegression()
    model.fit(X_pre, Y)
    return model.predict(X_pre).tolist()

