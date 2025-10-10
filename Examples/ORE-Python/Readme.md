# ORE Python Module

Since release 9 we provide easy access to ORE via a pre-compiled Python module, updated with each release.
The Python module provides the same functionality as the ORE command line executable and more.

Some example scripts using this ORE module are provided in this ORE-Python directory.

Prerequisite: Python3

You may have built ORE including the ORE Python module. In that case ensure that your PYTHONPATH contains the
build artifacts (ORE.py and _OREP.so), e.g. with "export PYTHONPATH=$HOME/ore/build/ORE-SWIG".

Otherwise install the pre-built Python module as follows:

Create a virtual environment: <code> python -m venv env1 </code>

Activate the virtual environment
- on Windows: <code> .\env1\Scripts\activate.bat </code>
- on macOS or Linux: <code> source env1/bin/activate </code>

Then insall ORE: <code> pip install open-source-risk-engine </code>

Try the Python examples
- Re-run the Swap exposure of the first Exposure example: <code> python ore.py </code>
- Show how to access and post-process ORE in-memory results without reading files: <code> python ore2.py </code>
- Demonstrate lower-level access to the QuantLib and QuantExt libraries: <code> python commodityforward.py </code>
- Then try any of the Jupyter notebooks, in directories ORE-Python/Notebooks/Example_*;
  install juypter: <code> python -m pip install jupyterlab </code>
  start juypter: <code> python -m jupyterlab & </code>
  and open any of the notebooks