#include <iostream>
#include <iomanip>

#include "CAENDigitizer.h"

#include "TXMLNode.h"
#include "TFile.h"
#include "TTree.h"
#include "TH1.h"
#include "TH2.h"

#include "TXMLOdb.h"
#include "TMidasFile.h"
#include "TMidasEvent.h"

#include "CaenEvent.hh"

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

std::vector<CaenEvent*> ParseData(char* bank, int bankSize, int debug) {
	if(debug > 4) {
		std::cout<<"starting to read bank "<<static_cast<void*>(bank)<<" of size "<<bankSize<<std::endl;
	}
	std::vector<CaenEvent*> result;
	uint32_t* data = reinterpret_cast<uint32_t*>(bank);

	int w = 0;
	for(int board = 0; w < bankSize; ++board) {
		if(debug > 5) {
			std::cout<<"----------------------------------------"<<std::endl;
			std::cout<<w<<" - 0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<data[w]<<std::dec<<std::setfill(' ')<<std::endl;
		}
		// read board aggregate header
		if(data[w]>>28 != 0xa) {
			if(data[w] == 0x0) {
				while(w < bankSize) {
					if(data[w++] != 0x0) {
						std::cerr<<board<<". board - failed on first word, found empty word, but not all following words were empty: "<<w-1<<" 0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<data[w-1]<<std::dec<<std::setfill(' ')<<std::endl;
						return result;
					}
				}
				return result;
			}
			std::cerr<<board<<". board - failed on first word 0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<data[w]<<std::dec<<std::setfill(' ')<<", highest nibble should have been 0xa!"<<std::endl;
			return result;
		}
		int32_t numWordsBoard = data[w++]&0xfffffff; // this is the number of 32-bit words from this board
		if(w - 1 + numWordsBoard > bankSize) { 
			std::cerr<<"0 - Missing words, at word "<<w-1<<", expecting "<<numWordsBoard<<" more words for board "<<board<<" (bank size "<<bankSize<<")"<<std::endl;
			return result;
		}
		uint8_t boardId = data[w]>>27; // GEO address of board (can be set via register 0xef08 for VME)
		uint16_t pattern = (data[w]>>8) & 0x7fff; // value read from LVDS I/O (VME only)
		uint8_t channelMask = data[w++]&0xff; // which channels are in this board aggregate
		uint32_t boardCounter = data[w++]&0x7fffff; // ??? "counts the board aggregate"
		uint32_t boardTime = data[w++]; // time of creation of aggregate (does not correspond to a physical quantity)
		if(debug > 5) {
			std::cout<<"pattern 0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<pattern<<std::dec<<std::setfill(' ')<<", counter "<<boardCounter<<", time "<<boardTime<<", board ID "<<static_cast<int>(boardId)<<std::endl;
			std::cout<<"channel mask 0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<static_cast<int>(channelMask)<<std::dec<<std::setfill(' ')<<std::endl;
		}

		for(uint8_t channel = 0; channel < 16; channel += 2) {
			if(((channelMask>>(channel/2)) & 0x1) == 0x0) {
				if(debug > 5) {
					std::cout<<"skipping dual channel "<<static_cast<int>(channel)<<std::endl;
				}
				continue;
			}
			// read channel aggregate header
			if(data[w]>>31 != 0x1) {
				std::cerr<<"Failed on first word 0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<data[w]<<std::dec<<std::setfill(' ')<<", highest bit should have been set!"<<std::endl;
				return result;
			}
			int32_t numWords = data[w++]&0x3fffff;//per channel
			if(debug > 6) {
				std::cout<<w<<" - 0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<data[w]<<std::dec<<std::setfill(' ')<<std::endl;
			}
			if(w >= bankSize) {
				std::cerr<<"1 - Missing words, got only "<<w-1<<" words for channel "<<channel<<" (bank size "<<bankSize<<")"<<std::endl;
				return result;
			}
			if(((data[w]>>29) & 0x3) != 0x3) {
				std::cerr<<"Failed on second word 0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<data[w]<<std::dec<<std::setfill(' ')<<", bits 29 and 30 should have been set!"<<std::endl;
				return result;
			}
			bool dualTrace = ((data[w]>>31) == 0x1);
			bool extras    = (((data[w]>>28) & 0x1) == 0x1);
			bool waveform  = (((data[w]>>27) & 0x1) == 0x1);
			uint8_t extraFormat = ((data[w]>>24) & 0x7);
			//for now we ignore the information which traces are stored:
			//bits 22,23: if(dualTrace) 00 = "Input and baseline", 01 = "CFD and Baseline", 10 = "Input and CFD"
			//            else          00 = "Input", 01 = "CFD"
			//bits 19,20,21: 000 = "Long gate",  001 = "over thres.", 010 = "shaped TRG", 011 = "TRG Val. Accept. Win.", 100 = "Pile Up", 101 = "Coincidence", 110 = reserved, 111 = "Trigger"
			//bits 16,17,18: 000 = "Short gate", 001 = "over thres.", 010 = "TRG valid.", 011 = "TRG HoldOff",           100 = "Pile Up", 101 = "Coincidence", 110 = reserved, 111 = "Trigger"
			int numSampleWords = 4*(data[w++]&0xffff);// this is actually the number of samples divided by eight, 2 sample per word => 4*
			if(w >= bankSize) {
				std::cerr<<"2 - Missing words, got only "<<w-1<<" words for channel "<<channel<<" (bank size "<<bankSize<<")"<<std::endl;
				return result;
			}
			int eventSize = numSampleWords+2; // +2 = trigger time words and charge word
			if(extras) ++eventSize;
			if(debug > 5) {
				std::cout<<"supposed to have "<<numWords<<" words in this channel, "<<(waveform?"w/":"w/o")<<" waveform(s), "<<(dualTrace?"w/":"w/o")<<" dual trace, "<<(extras?"w/":"w/o")<<" extras in format "<<static_cast<uint16_t>(extraFormat)<<", with "<<numSampleWords<<" sample words, at word "<<w<<"/"<<bankSize<<", event size "<<eventSize<<" => "<<(numWords-2)/eventSize<<" events"<<std::endl;
			}
			if(numWords%eventSize != 2) {
				std::cerr<<numWords<<" words in channel aggregate, event size is "<<eventSize<<" => "<<static_cast<double>(numWords-2.)/static_cast<double>(eventSize)<<" events?"<<std::endl;
				return result;
			}

			// read channel data
			for(int ev = 0; ev < (numWords-2)/eventSize; ++ev) { // -2 = 2 header words for channel aggregate
				if(debug > 6) {
					std::cout<<"--------------------"<<std::endl;
					std::cout<<w<<" - 0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<data[w]<<std::dec<<std::setfill(' ')<<std::endl;
				}
				auto event = new CaenEvent;
				event->Channel(channel + (data[w]>>31)); // highest bit indicates odd channel
				event->TriggerTime(data[w++] & 0x7fffffff);
				if(waveform) {
					if(w + numSampleWords >= bankSize) { // need to read at least the sample words plus the charge/extra word
						std::cerr<<"3 - Missing "<<numSampleWords<<" waveform words, got only "<<w-1<<" words for channel "<<channel<<" (bank size "<<bankSize<<")"<<std::endl;
						return result;
					}
					for(int s = 0; s < numSampleWords && w < bankSize; ++s, ++w) {
						if(debug > 7) {
							std::cout<<w<<" - 0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<data[w]<<std::dec<<std::setfill(' ')<<std::endl;
						}
						event->AddDigitalWaveformSample(0, (data[w]>>14)&0x1);
						event->AddDigitalWaveformSample(1, (data[w]>>15)&0x1);
						if(dualTrace) {
							// all even samples are from the first trace, all odd ones from the second trace
							event->AddWaveformSample(1, data[w]&0x3fff);
							event->AddWaveformSample(0, (data[w]>>16)&0x3fff);
						} else {
							// both samples are from the first trace
							event->AddWaveformSample(0, data[w]&0x3fff);
							event->AddWaveformSample(0, (data[w]>>16)&0x3fff);
						}
						event->AddDigitalWaveformSample(0, (data[w]>>30)&0x1);
						event->AddDigitalWaveformSample(1, (data[w]>>31)&0x1);
					}
				} else {
					if(w >= bankSize) { // need to read at least the sample words plus the charge/extra word
						std::cerr<<"3 - Missing words, got only "<<w-1<<" words for channel "<<channel<<" (bank size "<<bankSize<<")"<<std::endl;
						return result;
					}
				}
				if(extras) {
					if(debug > 6) {
						std::cout<<w<<" - 0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<data[w]<<std::dec<<std::setfill(' ')<<std::endl;
					}
					switch(extraFormat) {
						case 0: // [31:16] extended time stamp, [15:0] baseline*4
							//event->Baseline(data[w]&0xffff);
							event->ExtendedTimestamp(data[w++]>>16);
							break;
						case 1: // [31:16] extended time stamp, 15 trigger lost, 14 over range, 13 1024 triggers, 12 n lost triggers
							event->NLostCount(((data[w]>>12)&0x1) == 0x1);
							event->KiloCount(((data[w]>>13)&0x1) == 0x1);
							event->OverRange(((data[w]>>14)&0x1) == 0x1);
							event->LostTrigger(((data[w]>>15)&0x1) == 0x1);
							event->ExtendedTimestamp(data[w++]>>16);
							break;
						case 2: // [31:16] extended time stamp,  15 trigger lost, 14 over range, 13 1024 triggers, 12 n lost triggers, [9:0] fine time stamp
							event->Cfd(data[w]&0x3ff);
							event->NLostCount(((data[w]>>12)&0x1) == 0x1);
							event->KiloCount(((data[w]>>13)&0x1) == 0x1);
							event->OverRange(((data[w]>>14)&0x1) == 0x1);
							event->LostTrigger(((data[w]>>15)&0x1) == 0x1);
							event->ExtendedTimestamp(data[w++]>>16);
							break;
						case 4: // [31:16] lost trigger counter, [15:0] total trigger counter
							//event->LostTriggerCount(data[w]&0xffff);
							//event->TotalTriggerCount(data[w++]>>16);
							break;
						case 5: // [31:16] CFD sample after zero cross., [15:0] CFD sample before zero cross.
							//event->CfdAfterZC(data[w]&0xffff);
							//event->CfdBeforeZC(data[w++]>>16);
							break;
						case 7: // fixed value of 0x12345678
							if(data[w++] != 0x12345678) {
								std::cerr<<"Failed to get debug data word 0x12345678, got "<<std::hex<<std::setw(8)<<std::setfill('0')<<data[w]<<std::dec<<std::setfill(' ')<<std::endl;
								break;
							}
							break;
						default:
							break;
					}
				}
				if(debug > 6) {
					std::cout<<w<<" - 0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<data[w]<<std::dec<<std::setfill(' ')<<std::endl;
				}
				//if(w >= bankSize) {
				//	std::cerr<<"4 - Missing words, got only "<<w-1<<" words for channel "<<channel<<" (bank size "<<bankSize<<")"<<std::endl;
				//}
				event->ShortGate(data[w]&0x7fff);
				event->OverRange((data[w]>>15) & 0x1);
				event->Charge(data[w++]>>16);
				result.push_back(event);
				if(debug > 5) {
					event->Print();
				}
			} // while(w < bankSize)
		} // for(uint8_t channel = 0; channel < 16; channel += 2)
	} // for(int board = 0; w < bankSize; ++board)

	return result;
}

int main(int argc, char** argv) {
	if(argc != 3 && argc != 4) {
		std::cerr<<"Usage: "<<argv[0]<<" <input midas file> <output root file> <optional debug level>"<<std::endl;
		return 1;
	}

	// open input file
	std::string fileName = argv[1];
	TMidasFile* midasFile = nullptr;
	std::ifstream dataFile;
	size_t fileSize = 0;
	char* data = nullptr;
	if(fileName.find(".mid") != std::string::npos) {
		midasFile = new TMidasFile(argv[1]);
		if(midasFile == nullptr) {
			std::cerr<<R"(Failed to open ")"<<argv[1]<<R"(" as input midas file)"<<std::endl;
			return 1;
		}
		fileSize = midasFile->GetFileSize();
	} else {
		dataFile.open(argv[1]);
		if(!dataFile.is_open()) {
			std::cerr<<R"(Failed to open ")"<<argv[1]<<R"(" as input data file)"<<std::endl;
			return 1;
		}
		dataFile.seekg(0, dataFile.end);
		fileSize = dataFile.tellg();
		dataFile.seekg(0, dataFile.beg);
		data = new char[fileSize];
		dataFile.read(data, fileSize);
		dataFile.close();
	}

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

	// read ODB from midas file
	int nofChannels = 8;
	if(midasFile != nullptr) {
		auto odb = new TXMLOdb(midasFile->GetFirstEvent()->GetData(), midasFile->GetFirstEvent()->GetDataSize());
		TXMLNode* node = odb->FindPath("/Equipment/DT5730/Common");

		if(node == nullptr || !node->HasChildren()) {
			std::cerr<<"Failed to find ODB with children in input file"<<std::endl;
			output->Close();
		return 1;
		}

		node = node->GetChildren();

		while(true) {
			std::string key = odb->GetNodeName(node);
			if(debug > 0) {
				std::cout<<"node with key '"<<key<<"'"<<std::endl;
				if(node->GetText() != nullptr) {
					std::cout<<"node text '"<<node->GetText()<<"'"<<std::endl;
				} else {
				std::cout<<"no node text"<<std::endl;
				}
			}
			if(key.compare("Channels per digitizer") == 0) {
				nofChannels = strtol(node->GetText(), nullptr, 0);
				if(debug > 0) {
					std::cout<<"updated nofChannels to "<<nofChannels<<std::endl;
				}
				//expt = node->GetText();
			}
			if(!node->HasNextNode()) {
				if(debug > 0) {
					std::cout<<"no more nodes"<<std::endl;
				}
				break;
		}
			node = node->GetNextNode();
		}
	}

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

	// read events from midas file
	auto event = std::make_shared<TMidasEvent>();
	char* bank = nullptr;
	int bankSize;
	
	if(midasFile != nullptr) {
		int i = 0;
		while(true) {
			if(debug > 3) {
				std::cout<<"trying to read "<<i<<". midas event, read "<<midasFile->GetBytesRead()<<" bytes out of "<<fileSize<<" bytes so far"<<std::endl;
			}
			if(midasFile->Read(event) <= 0) {
				break;
			}
			switch(event->GetEventId()) {
				case 1:
					event->SetBankList();
					if(debug > 6) {
						event->Print("a");
					}
					bankSize = event->LocateBank(nullptr, "CAEN", reinterpret_cast<void**>(&bank));
					if(bankSize > 0) {
						std::vector<CaenEvent*> caenEvents = ParseData(bank, bankSize, debug);
						if(debug > 3) {
							std::cout<<"got "<<caenEvents.size()<<" events from this midas event"<<std::endl;
						}
						for(auto ev : caenEvents) {
							*caenEvent = *ev;
							tree->Fill();
							channels->Fill(ev->Channel());
							charge->Fill(ev->Charge(), ev->Channel());
						}
					}
					break;
				case 0x8000:
					std::cout<<std::endl<<"begin of run event"<<std::endl;
					break;
				case 0x8001:
					std::cout<<midasFile->GetBytesRead()/1024<<" kiB/"<<fileSize/1024<<" kiB = "<<(100*midasFile->GetBytesRead())/fileSize<<" % done"<<std::endl
						<<"end of run event"<<std::endl;
					break;
				default:
					std::cout<<std::endl<<"unknown event id 0x"<<std::hex<<event->GetEventId()<<std::dec<<std::endl;
					break;
			}
			if(i%10 == 0) {
				std::cout<<midasFile->GetBytesRead()/1024<<" kiB/"<<fileSize/1024<<" kiB = "<<(100*midasFile->GetBytesRead())/fileSize<<" % done\r"<<std::flush;
			}
			++i;
		} // end of event loop
		midasFile->Close();
	} else {
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
			}
			pos += numWords;
			if(i%10 == 0) {
				// pos count in 32bit = 4 bytes words; *4/1024 = /256
				std::cout<<pos/256<<" kiB/"<<fileSize/1024<<" kiB = "<<(400*pos)/fileSize<<" % done\r"<<std::flush;
			}
			++i;
		} // end of event loop
		std::cout<<pos/256<<" kiB/"<<fileSize/1024<<" kiB = "<<(400*pos)/fileSize<<" % done"<<std::endl;
	}

	tree->Write();
	list->Write();
	output->Close();

	return 0;
}
