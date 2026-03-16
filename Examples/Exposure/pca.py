import pandas as pd
import numpy as np
import csv

def run_pca(subdir=''):

        # these must match the yield curve tenors in simulation.xml
        curve_times = [1, 2, 3, 5, 10, 15, 20, 30]

        # load the scenariodump.csv into a dataframe
        data = pd.read_csv('Output/' + subdir + '/scenariodump.csv')
        
        # just keep the discount factors
        data = data.iloc[:, 3:]

        # convert to zero rates
        data = -np.log(data).div(curve_times)

        # take differences
        data = data.diff().iloc[1:,:]

        # subtract the means
        data = data - data.mean()

        # build the covariance matrix
        cov = 252.0 / len(data) * np.dot(data.T, data)

        # get the eigne vectors and values
        eigenvalues, eigenvectors = np.linalg.eig(cov)

        # output results
        with open('Output/' + subdir + '/eigenvalues.csv', 'w', newline='') as file1:
                writer = csv.writer(file1, delimiter=',')
                writer.writerow(['number', 'value'])
                no = 1
                for v in eigenvalues:
                        writer.writerow([no, v])
                        no = no + 1

        with open('Output/' + subdir + '/eigenvectors.csv', 'w', newline='') as file2:
                writer = csv.writer(file2, delimiter=',')
                header = ['tenor']
                no = 1
                for t in eigenvectors:
                        header.append('eigenvector_' + str(no))
                        no = no +1
                writer.writerow(header)
                index = 0
                for v in eigenvectors:
                        data = [curve_times[index]] + list(v)
                        writer.writerow(data)
                        index = index + 1
