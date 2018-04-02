#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>

#include "CAENDigitizer.h"

#include "TFile.h"
#include "TTree.h"
#include "TH1.h"
#include "TH2.h"

#include "CaenEvent.hh"
#include "CaenParser.hh"

std::string format(const std::string& format, ...)
{
	va_list args;
	va_start (args, format);
	size_t len = std::vsnprintf(NULL, 0, format.c_str(), args);
	va_end (args);
	std::vector<char> vec(len + 1);
	va_start (args, format);
	std::vsnprintf(&vec[0], len + 1, format.c_str(), args);
	va_end (args);
	return &vec[0];
}

int main(int argc, char** argv) {
	if(argc != 3 && argc != 4) {
		std::cerr<<"Usage: "<<argv[0]<<" <input midas file> <output root file> <optional debug level>"<<std::endl;
		return 1;
	}

	// open input file
	std::ifstream dataFile(argv[1]);
	if(!dataFile.is_open()) {
		std::cerr<<R"(Failed to open ")"<<argv[1]<<R"(" as input data file)"<<std::endl;
		return 1;
	}
	dataFile.seekg(0, dataFile.end);
	size_t fileSize = dataFile.tellg();
	dataFile.seekg(0, dataFile.beg);
	char* data = new char[fileSize];
	dataFile.read(data, fileSize);
	dataFile.close();

	// open root file
	auto output = new TFile(argv[2], "recreate");
	if(output == nullptr || !output->IsOpen()) {
		std::cerr<<R"(Failed to open ")"<<argv[2]<<R"(" as output root file)"<<std::endl;
		return 1;
	}

	int debug = 0;
	if(argc == 4) {
		debug = strtol(argv[3], nullptr, 0);
	}

	int nofChannels = 8;

	// create tree and histograms
	TTree* tree = new TTree("tree", "tree");
	auto caenEvent = new CaenEvent;
	tree->Branch("event", &caenEvent);

	auto list = new TList;
	auto channels = new TH1F("channels", "channel number", nofChannels+1, 0, nofChannels+1); list->Add(channels);
	auto charge = new TH2F("channelVsCharge", "channelVsCharge", 5000, 0, 50000, nofChannels+1, 0, nofChannels+1); list->Add(charge);

	if(debug > 0) {
		std::cout<<std::endl<<"created histograms:"<<std::endl;
		list->Print();
	}

	// read from data buffer
	uint32_t* word = reinterpret_cast<uint32_t*>(data);
	size_t pos = 0;
	int i = 0;
	while(pos < fileSize/4) {
		// check that we have the next header, otherwise advance until we find it
		while((word[pos]>>28) != 0xa) {
			std::cout<<"0x"<<std::hex<<word[pos]<<std::dec<<": not a header, skipping"<<std::endl;
			++pos;
		}
		// read data size (in 32-bit words) from header
		int32_t numWords = word[pos]&0xfffffff;
		std::vector<CaenEvent*> caenEvents = ParseData(reinterpret_cast<char*>(word + pos), numWords, debug);
		if(debug > 3) {
			std::cout<<"got "<<caenEvents.size()<<" events from this midas event"<<std::endl;
		}
		for(auto ev : caenEvents) {
			*caenEvent = *ev;
			tree->Fill();
			channels->Fill(ev->Channel());
			charge->Fill(ev->Charge(), ev->Channel());
			if(debug > 4) {
				std::cout<<"Charge "<<caenEvent->Charge()<<std::endl;
			}
			delete ev;
		}
		if(debug > 3) {
			std::cout<<"have "<<tree->GetEntries()<<" entries total"<<std::endl;
		}
		pos += numWords;
		if(i%10 == 0) {
			// pos count in 32bit = 4 bytes words; *4/1024 = /256
			std::cout<<pos/256<<" kiB/"<<fileSize/1024<<" kiB = "<<(400*pos)/fileSize<<" % done\r"<<std::flush;
		}
		++i;
	} // end of event loop
	std::cout<<pos/256<<" kiB/"<<fileSize/1024<<" kiB = "<<(400*pos)/fileSize<<" % done"<<std::endl;

	tree->Write();
	list->Write();
	output->Close();

	return 0;
}
