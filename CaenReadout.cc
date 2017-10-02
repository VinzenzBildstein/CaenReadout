#include <iostream>
#include <vector>
#include <string>

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
	interface.Add("-o", "output file (required)", &outputFilename);
	uint64_t numberOfTriggers = 0;
	interface.Add("-n", "number of triggers to record (either this or -t required)", &numberOfTriggers);
	uint32_t secondsToRun = 0;
	interface.Add("-t", "second to run (either this or -n required)", &secondsToRun);
	bool debug = false;
	interface.Add("-d", "turn debugging messages on", &debug);

	interface.CheckFlags(argc, argv);

	if(settingsFilename.empty() || outputFilename.empty()) {
		std::cerr<<"You need to provide a settings file (-s flag), and an output file (-o flag)"<<std::endl;
		return 1;
	}

	if(numberOfTriggers == 0 && secondsToRun == 0) {
		std::cerr<<"No triggers to record and zero second to run? Done!"<<std::endl;
		return 1;
	}

	CaenSettings settings(settingsFilename, debug);

	CaenDigitizer* digitizer;
	try{
		digitizer = new CaenDigitizer(settings, debug);
	} catch(const std::runtime_error& e) {
		std::cout<<e.what()<<std::endl;
		return 1;
	}

	TFile* output = new TFile(outputFilename.c_str(), "recreate");

	try {
		settings.RunLength(digitizer->Run(output, numberOfTriggers, secondsToRun));
	} catch(const std::runtime_error& e) {
		std::cout<<e.what()<<std::endl;
		return 1;
	}
	settings.Write();

	output->Close();

	return 0;
}
