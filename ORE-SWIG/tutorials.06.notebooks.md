# Run Jupyter Notebook examples using the ORE Python wheels

This tutorial is aimed at the user who wants to install the pre-built ORE
Python module and use it to execute example Jupyter notebooks provided in
[the notebooks directory](https://github.com/OpenSourceRisk/ORE-SWIG/tree/master/OREAnalytics-SWIG/Python/Examples/Notebooks). No compilation is necessary.

[Back to tutorials index](tutorials.00.index.md)

**Please note** that (as of October 2023) the published ORE Python wheels are
available for Python versions 3.8 up to 3.11 for Windows, Linux and macOS arm64,
and Python versions up to 3.10 for Intel Mac users.  

We assume below that the python executable points to a permissible Python 3 version. You can check your system's default python version with

    python --version

## Quick Set Up and First Run

Please note that these steps must be executed sequentially in a single command prompt/shell. The working directory, for ease, should be OREAnalytics-SWIG/Python/Examples/Notebooks.

1. Create a virtual environment:

    ``` python -m venv venv ```

2. Activate the virtual environment: 

    (Windows): ```.\venv\Scripts\activate.bat ```
    
    (Linux, Mac): ```source "$(pwd)/venv/bin/activate" ```

3. Install required Python modules. The notebook examples require Jupyter, the ORE Python module and a few additional packages for e.g. plotting. To install these in the virtual environment please make sure to execute this **after** activating the virtual environment as above.

    ``` python -m pip install open-source-risk-engine matplotlib pandas plotly jupyter_server==2.8.0 jupyter ```

4. Launch Jupyter! This opens a browser window that shows the list of Example folders on the left hand side. Double click to change into any of the Example folders and then double click the Jupyter notebook to open.

    ``` python -m jupyterlab ```

## Normal Usage

After the first time run, you simply need to activate the virtual environment and run jupyter lab.

(Windows): ```.\venv\Scripts\activate.bat ```
    
(Linux, Mac): ```source "$(pwd)/venv/bin/activate" ```
    
    python -m jupyterlab

## Exit the virtual environment

To deactivate the virtual environment you can simply close the command prompt/terminal window. Othewise you can run the relevant deactivation script (assuming that the working directory is the Notebooks directory:

(Windows): ```.\venv\Scripts\deactivate.bat ```
    
(Linux, Mac): ```source "$(pwd)/venv/bin/deactivate" ```

