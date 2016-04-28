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

