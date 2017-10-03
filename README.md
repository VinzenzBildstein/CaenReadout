# Installation

Requirements:
- installed ROOT
- installed CommandLineInterface (see https://github.com/GRIFFINCollaboration/CommandLineInterface)

You might need to update the Makefile to have COMMON_DIR point to your installation of CommandLineInterface, and LIB_DIR to point to your local directory of shared object libraries

After that just run ```make```

# Purpose 

This program can be used to read data from a CAEN DT5730 digitizer. The output is written as a root file with a tree of CaenEvents. The program Histograms can be used to create histograms from the output tree.

# Settings

A lot of the settings are based on enums defined in the CAEN libraries, here a list of some of them:

- IO level: 0 - NIM, 1 - TTL
- acquisition mode: 0 - software controlled, 1 - S_in controlled, 2 - controlled by first trigger
- pulse polarity: 0 - positive, 1 - negative
- self trigger: 0 - disabled, 1 - enabled
- charge sensitivity:
- baseline samples: 0 - fixed, 1 - 16, 2 - 64, 3 - 256, 4 - 1024
- DPP acquisition mode: 0 - oscilloscope, 1 - list, 2 - mixed

