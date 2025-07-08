import numpy as np

from sklearn.linear_model import Ridge
from sklearn.linear_model import LinearRegression
from sklearn.preprocessing import PolynomialFeatures
from sklearn.metrics import mean_squared_error
from sklearn.preprocessing import MinMaxScaler
from sklearn.tree import DecisionTreeRegressor
from sklearn.neighbors import KNeighborsRegressor
from sklearn.neighbors import NearestNeighbors
from sklearn.tree import plot_tree

import hnswlib as hnswlib

import matplotlib.pyplot as plt

def flatten_list(l):
    return [x for u in l for x in u]

def conditional_expectation(x,y):
    X = np.array(x)
    Y = np.array(y)

    # 1a linear regression

    # poly = PolynomialFeatures(degree=2)
    # X_pre = poly.fit_transform(X)
    # model = LinearRegression()
    # model.fit(X_pre, Y)
    # return flatten_list(model.predict(X_pre).tolist())

    # 1b ridge regression

    poly = PolynomialFeatures(degree=4)
    X_pre = poly.fit_transform(X)
    model = Ridge()
    model.fit(X_pre, Y)
    return model.predict(X_pre).tolist()

    # 2 regression tree

    # model = DecisionTreeRegressor(max_depth = 5)
    # model.fit(X, Y)
    # return  model.predict(X).tolist()

    # 3 k-nearest neighbours

    # model = KNeighborsRegressor(n_neighbors=100)
    # model.fit(X, Y)
    # return  flatten_list(model.predict(X).tolist())

    # 4 Fourier

    # X = MinMaxScaler().fit_transform(np.array(x))

    # def fourier_features(X, order=40):
    #     return np.column_stack([np.sin(k * np.pi * X[:, i]) for i in range(X.shape[1]) for k in range(1, order + 1)] +
    #                            [np.cos(k * np.pi * X[:, i]) for i in range(X.shape[1]) for k in range(1, order + 1)])


    # X_fourier = fourier_features(X)
    # ridge = Ridge(alpha=0.00001).fit(X_fourier, np.array(y))

    # pred = ridge.predict(X_fourier)
    # #rmse = np.sqrt(mean_squared_error(y, pred))
    # #print(f'RMSE: {rmse}')

    # return pred.tolist()

    # 5 Fourier + Glättung

    # X = MinMaxScaler().fit_transform(np.array(x))

    # def fourier_features(X, order=20):
    #     return np.column_stack(
    #         [np.sin(k * np.pi * X[:, i]) for i in range(X.shape[1]) for k in range(1, order + 1)] +
    #         [np.cos(k * np.pi * X[:, i]) for i in range(X.shape[1]) for k in range(1, order + 1)]
    #     )

    # X_fourier = fourier_features(X)
    # ridge = Ridge(alpha=0.00001).fit(X_fourier, np.array(y))
    # pred = ridge.predict(X_fourier)

    # # Glättung im R^3 Raum mit gewichteter 5-NN (vektorisiert)
    # nbrs = NearestNeighbors(n_neighbors=6).fit(X)  # Punkt selbst + 5 Nachbarn
    # _, indices = nbrs.kneighbors(X)

    # # Gewichtsmatrix: 0.5 für den Punkt selbst, 0.1 für die 5 Nachbarn
    # weights = np.full(indices.shape, 0.1)
    # weights[:, 0] = 0.5  # erster Nachbar ist immer der Punkt selbst

    # # Wende die Gewichtung vektorisiert an
    # pred_array = np.array(pred)
    # smoothed_pred = np.sum(weights * pred_array[indices], axis=1)

    # return smoothed_pred.tolist()

    # 6 hnswlib

    # k = 5
    # ef = 100

    # """
    #   Berechnung der bedingten Erwartung ohne Gewichtung (kein smoothing),
    #   einfaches arithmetisches Mittel der k nächsten Nachbarn.

    #   Parameters:
    #   - x: 2D-Array der Eingabedaten
    #   - y: 1D-Array der Zielwerte
    #   - k: Anzahl der Nachbarn
    #   - ef: HNSW-Suchparameter

    #   Returns:
    #   - avg_preds: Array der Durchschnittswerte der k-Nachbarn
    # """
    # X = MinMaxScaler().fit_transform(np.array(x))
    # pred = np.array(y)
    # dim = X.shape[1]

    # # Initialisiere HNSW-Index
    # index = hnswlib.Index(space='l2', dim=dim)
    # index.init_index(max_elements=len(X), ef_construction=ef, M=16)
    # index.add_items(X)
    # index.set_ef(ef)

    # # KNN-Abfrage
    # labels, _ = index.knn_query(X, k=min(k, len(X)))

    # # Durchschnitt der Zielwerte der k Nachbarn (ungewichtet)
    # neighbor_targets = pred[labels]
    # avg_preds = np.mean(neighbor_targets, axis=1)

    # return flatten_list(avg_preds)
