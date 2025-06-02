
# Installing ORE Python Libraries and Example Scripts on Posix Systems (e.g. MacOS and Linux)

This tutorial is aimed at users on Posix systems (e.g. MacOS and Linux) who
want to install and use the ORE Python libraries and example scripts.  No
compilation is necessary.  The commands listed here were tested on
Ubuntu and you will likely have to modify the commands slightly on other
systems.

[Back to tutorials index](../tutorials_index.md)

## Introduction

In principle, the command to install ORE is simply:

    pip install open-source-risk-engine

However, there are some additional considerations:

1) The ORE example Python scripts depend upon other Python libraries.
2) If you use `pip install` to install a load of Python libraries, there is the
potential to pollute your machine with outdated or incompatible libraries.  It
is cleaner to install Python libraries into a virtual environment.

Therefore, this tutorial will take a step back.  Starting from a clean machine,
we will install all necessary prerequisites, and then the ORE Python library
itself, along with the example Python scripts.  We will use a virtual
environment which can be modified or deleted without affecting the rest of your
computer.

## Python

You need to download and install Python.  You need at least Python version 3.8.
To verify that you have Python installed correctly, and that it is a supported
version, open a command prompt and do:

    python3 --version

## Pip

We are going to be using pip, so make sure that you have it installed and that
you have the latest version:

    python3 -m ensurepip --upgrade

## Example Scripts

You probably want a copy of the example Python scripts from the ORE project.
So point your browser at the git repo...

https://github.com/OpenSourceRisk/Engine/Examples/ORE-Python/ExampleScripts

...and click on the download button.  Download a zip file of the repo and
uncompress it somewhere on your hard drive.

Back at the command prompt, cd into the directory containing the example
scripts, e.g:

    cd /path/to/Engine/Examples/ORE-Python/ExampleScripts

## Virtual Environment

Create a virtual environment into which you will install the ORE Python library:

    python3 -m venv env1

Activate the virtual environment:

    . ./env1/bin/activate

You might get a warning message that the version of pip installed inside the
virtual environment is out of date, and you can make that go away with:

    python3 -m pip install --upgrade pip

Some of the example scripts depend on other Python libs, here is the command to
install those prerequisites:

    pip install jinja2 pandas

Now, after all that, you are finally ready to install ORE!

    pip install open-source-risk-engine

Now you can run the example scripts, e.g:

    python3 swap.py

Once you are done using your virtual environment, you can exit it using the
following command:

    deactivate

You can later re-enter the virtual environment with the same command that you
used earlier:

    . ./env1/bin/activate

To delete the virtual environment, simply deactivate it (if necessary) and then
delete the relevant directory:

    rm -rf env1

