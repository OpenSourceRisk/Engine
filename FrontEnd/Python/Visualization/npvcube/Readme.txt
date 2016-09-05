On Mac:

1) Install Annaconda (2.7 or 3?)
2) from command prompt, run
% ipython notebook
   This will start the ipyton console and open a browser window
   Ctrl-C to kill the server
3) In browser, it should open at localhost:8888

4) Open npv.ipynb


Trouble shooting:

1) ImportError: No module named 'ipywidgets'

Run
conda install -c anaconda ipywidgets=4.1.1

On Windows:
	1) Download & Install the latest Anaconda Distribution of Python 3.5 from:
		https://www.continuum.io/downloads	
	
	2) Open a command prompt and navigate to the subfolder:
		\FrontEnd\Python\Visualization\npvcube
	
	3) Launch Jupyter in that folder. If in step 1) you installed Anaconda to C:\Program Files\Anaconda the command for this is:
		C:\Program Files\Anaconda\Scripts\jupyter.exe noteboook
	In case you added C:\Program Files\Anaconda\Scripts to your PATH environment variable you can just type
		jupyter notebook
	
	4) Your standard Browser should open with a list of files in that folder. Just click on the notebook you would like to try, for instance 
		vizualization_dashboard.ipynb
		launcher_dashboard.ipynb
	
	5) The notebook should open and you can run it by clicking on Cell/Run Cells.
	
	6) Some of the cells have additional dependencies as mentioned in their description. For an optimal experience, we recommend installing
		jupyter_dashboads: https://github.com/jupyter-incubator/dashboards
		bqplot: https://github.com/bloomberg/bqplot
		pythreejs: https://github.com/jovyan/pythreejs
	Complete installation instructions can be found under the links mentioned above. Short version:
		pip install jupyter_dashboards
		jupyter dashboards quick-setup --sys-prefix
		conda install -c conda-forge bqplot
		conda install -c conda-forge pythreejs
