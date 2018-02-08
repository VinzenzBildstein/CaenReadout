#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>

#include <curses.h>
#include <signal.h>

#include "TFile.h"
#include "TTree.h"

#include "CommandLineInterface.hh"
#include "CaenSettings.hh"
#include "CaenDigitizer.hh"

bool controlC = false;
int  nRows, nCols;

void AtExitHandler()
{
   // this function is called on normal exits (via std::atexit) or
   // if the programm is killed with ctrl-c (via sigaction and HandleSignal)
   if(controlC) exit(0);
   controlC = true;

#ifdef USE_CURSES
	std::vector<std::string> line(nRows);
	for(int i = 0; i < nRows; ++i) {
		line[i].resize(nCols+1);
		mvinnstr(i, 0, const_cast<char*>(line[i].data()), nCols);
		if(line[i].find_first_not_of(' ') == std::string::npos || static_cast<int>(line[i].find_first_not_of(' ')) == nCols) {
			line[i].clear();
		}
	}

	endwin(); //restore terminal settings

	for(int i = 0; i < nRows-1; ++i) {
		// skip empty line if the next one is empty as well
		if(!line[i].empty() || !line[i+1].empty()) std::cout<<line[i]<<std::endl;
	}
	std::cout<<line[nRows-1]<<std::endl;
#endif
}

void HandleSignal(int)
{
   // sigaction requires a function that takes an integer as argument
   // since we don't care what the signal was, we just call AtExitHandler
   AtExitHandler();
}

static void CatchSignals()
{
   struct sigaction action;
   action.sa_handler = HandleSignal;
   action.sa_flags = 0;
   sigemptyset(&action.sa_mask);
   sigaction(SIGINT, &action, NULL);
   sigaction(SIGTERM, &action, NULL);
   sigaction(SIGSEGV, &action, NULL);
}

int main(int argc, char** argv) {
   std::atexit(AtExitHandler);
   CatchSignals();

	CommandLineInterface interface;
	std::string settingsFilename;
	interface.Add("-s", "settings file (required)", &settingsFilename);
	std::string outputFilename;
	interface.Add("-o", "output file (required, together with -r this is just the base name)", &outputFilename);
	std::string dataOutputFilename;
	interface.Add("-df", "data output file (optional, writes out the binary data)", &dataOutputFilename);
	uint64_t numberOfTriggers = 0;
	interface.Add("-n", "number of triggers to record (either this, -r, or -t required)", &numberOfTriggers);
	uint32_t secondsToRun = 0;
	interface.Add("-t", "second to run (either this, -n, or -r required)", &secondsToRun);
#ifdef USE_CURSES
	uint32_t runNumber = 0;
	interface.Add("-r", "run number (either this, -n, or -t required)", &runNumber);
#endif
	bool debug = false;
	interface.Add("-d", "turn debugging messages on", &debug);

	interface.CheckFlags(argc, argv);

	if(settingsFilename.empty()) {
		std::cerr<<"You need to provide a settings file (-s flag)"<<std::endl;
		return 1;
	}
	if(outputFilename.empty() && dataOutputFilename.empty()) {
		std::cerr<<"Warning, neither root output file (-o flag), nor data output file (-df flag) provided."<<std::endl;
		char c = '\0';
		do {
			std::cout<<"Do you want to proceed without writing any output file? [y/n]"<<std::endl;
			std::cin>>c;
			if(c == 'n') {
				return 0;
			}
		} while (!std::cin.fail() && c != 'y');
	}

#ifdef USE_CURSES
	if(numberOfTriggers == 0 && secondsToRun == 0 && runNumber == 0) {
#else
	if(numberOfTriggers == 0 && secondsToRun == 0) {
#endif
		std::cerr<<"No triggers to record, zero seconds to run, and no run number? Done!"<<std::endl;
		return 1;
	}

#ifdef USE_CURSES
	//initscr(); //initialize curses
	WINDOW* w = initscr();
	cbreak(); //disable buffering and get characters one at a time
	noecho(); //suppress echoing of typed characters
	nodelay(stdscr, true); //make getch() work in non-blocking manner
	keypad(stdscr, true); //get F1, F2, arrow keys, etc.
	getmaxyx(w, nRows, nCols);
#endif

	CaenSettings settings(settingsFilename, debug);

	CaenDigitizer* digitizer;
	try{
		digitizer = new CaenDigitizer(settings, debug);
	} catch(const std::runtime_error& e) {
#ifdef USE_CURSES
		printw("Error in CaenDigitizer: %s\n", e.what());
#else
		std::cout<<"Error in CaenDigitizer: "<<e.what()<<std::endl;
#endif
		return 1;
	}

#ifdef USE_CURSES
	int ch = 0; //character read from input

	if(runNumber == 0) {
		std::ofstream dataFile;
		if(!dataOutputFilename.empty()) {
			dataFile.open(dataOutputFilename, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
		}
		TFile* output = nullptr;
		if(!outputFilename.empty()) {
			output = new TFile(outputFilename.c_str(), "recreate");
		}
		try {
			settings.RunLength(digitizer->Run(output, dataFile, numberOfTriggers, secondsToRun));
		} catch(const std::runtime_error& e) {
			printw("%s\n", e.what());
			return 1;
		}
		settings.Write();
		output->Close();
		dataFile.close();
	} else {
		printw("use 's' to start/stop a run, and 'q' to quit the program\n");
		while(ch != 'q') {
			if((ch = getch()) != ERR) {
				switch(ch) {
					case 's':
						{
							printw("starting run %03d\n", runNumber);
							std::ofstream dataFile;
							if(!dataOutputFilename.empty()) {
								dataFile.open(Form("%s_%03d.dat", dataOutputFilename.c_str(), runNumber), std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
							}
							TFile* output = nullptr;
							if(!outputFilename.empty()) {
								output = new TFile(Form("%s_%03d.root", outputFilename.c_str(), runNumber++), "recreate");
							}
							try {
								settings.RunLength(digitizer->Run(output, dataFile));
							} catch(const std::runtime_error& e) {
								std::cout<<e.what()<<std::endl;
								return 1;
							}
							settings.Write();
							output->Close();
							dataFile.close();
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
#else
	std::cout<<"Opening file"<<std::endl;
	std::ofstream dataFile;
	if(!dataOutputFilename.empty()) {
		dataFile.open(dataOutputFilename, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
	}
   TFile* output = nullptr;
	if(!outputFilename.empty()) {
		output = new TFile(outputFilename.c_str(), "recreate");
	}

   try {
      settings.RunLength(digitizer->Run(output, dataFile, numberOfTriggers, secondsToRun));
   } catch(const std::runtime_error& e) {
      std::cout<<e.what()<<std::endl;
      return 1;
   }
   settings.Write();

   output->Close();
#endif

	return 0;
}
