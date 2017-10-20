#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

#include <curses.h>

#include "TFile.h"
#include "TTree.h"

#include "CommandLineInterface.hh"
#include "CaenSettings.hh"
#include "CaenDigitizer.hh"

int main(int argc, char** argv) {
	CommandLineInterface interface;
	std::string settingsFilename;
	interface.Add("-s", "settings file (required)", &settingsFilename);
	std::string outputFilename;
	interface.Add("-o", "output file (required, together with -r this is just the base name)", &outputFilename);
	uint64_t numberOfTriggers = 0;
	interface.Add("-n", "number of triggers to record (either this, -r, or -t required)", &numberOfTriggers);
	uint32_t secondsToRun = 0;
	interface.Add("-t", "second to run (either this, -n, or -r required)", &secondsToRun);
	uint32_t runNumber = 0;
	interface.Add("-r", "run number (either this, -n, or -t required)", &runNumber);
	bool debug = false;
	interface.Add("-d", "turn debugging messages on", &debug);

	interface.CheckFlags(argc, argv);

	if(settingsFilename.empty() || outputFilename.empty()) {
		std::cerr<<"You need to provide a settings file (-s flag), and an output file (-o flag)"<<std::endl;
		return 1;
	}

	if(numberOfTriggers == 0 && secondsToRun == 0 && runNumber == 0) {
		std::cerr<<"No triggers to record, zero seconds to run, and no run number? Done!"<<std::endl;
		return 1;
	}

	//initscr(); //initialize curses
	WINDOW* w = initscr();
	cbreak(); //disable buffering and get characters one at a time
	noecho(); //suppress echoing of typed characters
	nodelay(stdscr, true); //make getch() work in non-blocking manner
	keypad(stdscr, true); //get F1, F2, arrow keys, etc.
	int row, col;
	getmaxyx(w, row, col);

	CaenSettings settings(settingsFilename, debug);

	CaenDigitizer* digitizer;
	try{
		digitizer = new CaenDigitizer(settings, debug);
	} catch(const std::runtime_error& e) {
		std::cout<<e.what()<<std::endl;
		return 1;
	}

	int ch = 0; //character read from input

	if(runNumber == 0) {
		TFile* output = new TFile(outputFilename.c_str(), "recreate");
		try {
			settings.RunLength(digitizer->Run(output, numberOfTriggers, secondsToRun));
		} catch(const std::runtime_error& e) {
			std::cout<<e.what()<<std::endl;
			return 1;
		}
		settings.Write();
		output->Close();
	} else {
		printw("use 's' to start/stop a run, and 'q' to quit the program\n");
		while(ch != 'q') {
			if((ch = getch()) != ERR) {
				switch(ch) {
					case 's':
						{
							printw("starting run %03d\n", runNumber);
							TFile* output = new TFile(Form("%s_%03d.root", outputFilename.c_str(), runNumber++), "recreate");
							try {
								settings.RunLength(digitizer->Run(output));
							} catch(const std::runtime_error& e) {
								std::cout<<e.what()<<std::endl;
								return 1;
							}
							settings.Write();
							output->Close();
							break;
						}
					default:
						break;
				}
			} else {
				// haven't had any input, so lets sleep for a while
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	}

	std::vector<std::string> line(row);
	for(int i = 0; i < row; ++i) {
		line[i].resize(col+1);
		mvinnstr(i, 0, const_cast<char*>(line[i].data()), col);
		if(line[i].find_first_not_of(' ') == std::string::npos || static_cast<int>(line[i].find_first_not_of(' ')) == col) {
			line[i].clear();
		}
	}

	endwin(); //restore terminal settings

	for(int i = 0; i < row-1; ++i) {
		// skip empty line if the next one is empty as well
		if(!line[i].empty() || !line[i+1].empty()) std::cout<<line[i]<<std::endl;
	}
	std::cout<<line[row-1]<<std::endl;

	return 0;
}
