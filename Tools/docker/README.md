
The Dockerfile in this folder can be used to build a Docker image with
ORE compiled and hopefully ready to try - with some caveats.


Installation
------------

You don't need to build the whole thing on your machine, nor to
install its prerequisites.  If you have Docker, you can execute

    docker build -t ore .

in this directory to build the image, or you can save some time and run

    docker pull lballabio/ore
    docker tag lballabio/ore ore

to download and re-tag a pre-built one.  Once you have the image, running

    docker run --rm -ti ore bash

will drop you into a shell inside the container you started.


Running examples
----------------

The containers you'll start from the image don't have a graphical
interface, so you'll have to work around the problem.

For instance, if you run one of the examples from the container shell
as follows:

    root@bb2897dfe99a:/ORE-1.8# cd Examples/Example_1
    root@bb2897dfe99a:/ORE-1.8/Examples/Example_1# python run.py
    
    1) Run ORE to produce NPV cube and exposures
    ORE starting
    Market data loader...                   OK
    
    [...more output...]
    
    3) Plot results: Simulated exposures vs analytical swaption prices
    Saving plot....Output/mpl_swaptions.pdf

The file `mpl_swaption.pdf` is created inside the container and can't
be viewed directly.  Therefore, you'll have to copy it from the
container to your computer.  While the container is still running,
open another terminal and run

    docker ps

to get the name of the container; for instance, `sleepy_dijkstra`.
Then, use

    docker cp sleepy_dijkstra:/ORE-1.8/Examples/Example_1/Output/mpl_swaptions.pdf .

to copy the PDF to your computer, from which you can open it with
whatever program you have available.


Running Jupyter notebook
------------------------

From this image, you can also try out the provided Jupyter notebook.
You'll have to pass a few more options while starting the container,
because Docker needs to map on your machine the port used by the
Jupyter server.  Start the container with

    docker run --rm -ti -p 8888:8888 ore bash

Now, move to the folder where the notebook is stored and start the
server as follows:

    root@4eda0fdf9610:/ORE-1.8# cd FrontEnd/Python/Visualization/npvcube/
    root@4eda0fdf9610:/ORE-1.8/FrontEnd/Python/Visualization/npvcube# jupyter notebook --no-browser --ip=0.0.0.0

Then, open your browser, point it to `http://localhost:8888`, and
click on the notebook name `ore_jupyter_dashboard.ipynb` to start it.
You'll have to set the paths in the first cell as:

    ore_exe_path = '/ORE-1.8/App/ore'
    ore_xml = '/ORE-1.8/Examples/Example_2/Input/ore.xml'

At this time, the matplotlib-based examples work but those based on
pythreejs and bqplot don't; this must be investigated.
