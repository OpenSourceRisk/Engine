import numpy as np

from sklearn.linear_model import LinearRegression
from sklearn.preprocessing import PolynomialFeatures

from sklearn.tree import DecisionTreeRegressor

def flatten_list(l):
    return [x for u in l for x in u]

def conditional_expectation(x,y):
    X = np.array(x)
    Y = np.array(y)

    # linear regression

    # poly = PolynomialFeatures(degree=2)
    # X_pre = poly.fit_transform(X)
    # model = LinearRegression()
    # model.fit(X_pre, Y)
    # return flatten_list(model.predict(X_pre).tolist())

    # regression tree

    model = DecisionTreeRegressor(max_depth=3)
    model.fit(X, Y)
    return  model.predict(X).tolist()
