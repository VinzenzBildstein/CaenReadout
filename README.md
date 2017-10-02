# Installation

Requirements:
- installed ROOT
- installed CommandLineInterface (see https://github.com/GRIFFINCollaboration/CommandLineInterface)

You might need to update the Makefile to have COMMON_DIR point to your installation of CommandLineInterface, and LIB_DIR to point to your local directory of shared object libraries

After that just run ```make```

# Purpose 

This program can be used to read data from a CAEN DT5730 digitizer. The output is written as a root file with a tree of CaenEvents. The program Histograms can be used to create histograms from the output tree.
