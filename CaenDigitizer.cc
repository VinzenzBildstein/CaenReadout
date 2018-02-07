#include "CaenDigitizer.hh"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <curses.h>

CaenDigitizer::CaenDigitizer(const CaenSettings& settings, bool debug)
	: fSettings(&settings), fOutputFile(nullptr), fTree(nullptr), fEvent(new CaenEvent), fEventsRead(0), fRunTime(0.), fOldEventsRead(0), fOldRunTime(0.), fDebug(debug)
{
	if(fDebug) std::cout<<"constructing digitizer"<<std::endl;
	CAEN_DGTZ_ErrorCode errorCode;
	CAEN_DGTZ_BoardInfo_t boardInfo;
	int majorNumber;

	try {
		fHandle.resize(fSettings->NumberOfBoards(), 0);
		fBuffer.resize(fSettings->NumberOfBoards());
		fBufferSize.resize(fSettings->NumberOfBoards());
		fEvents.resize(fSettings->NumberOfBoards());
		fNofEvents.resize(fSettings->NumberOfBoards(), std::vector<uint32_t>(fSettings->NumberOfChannels(), 0));
		fWaveforms.resize(fSettings->NumberOfBoards(), nullptr);
	} catch(std::exception e) {
		std::cerr<<"Failed to resize vectors for "<<fSettings->NumberOfBoards()<<" boards, and "<<fSettings->NumberOfChannels()<<" channels: "<<e.what()<<std::endl;
		throw e;
	}
	for(int b = 0; b < fSettings->NumberOfBoards(); ++b) {
		if(fDebug) std::cout<<"setting up board "<<b<<std::endl;
		// open digitizers
		errorCode = CAEN_DGTZ_OpenDigitizer(fSettings->LinkType(b), b, 0, fSettings->VmeBaseAddress(b), &fHandle[b]);
		if(errorCode != 0) {
			throw std::runtime_error(Form("Error %d when opening digitizer", errorCode));
		}
		// get digitizer info
		errorCode = CAEN_DGTZ_GetInfo(fHandle[b], &boardInfo);
		if(errorCode != 0) {
			throw std::runtime_error(Form("Error %d when reading digitizer info", errorCode));
		}
#ifdef USE_CURSES
		printw("\nConnected to CAEN Digitizer Model %s as %d. board\n", boardInfo.ModelName, b);
		printw("\nFirmware is ROC %s, AMC %s\n", boardInfo.ROC_FirmwareRel, boardInfo.AMC_FirmwareRel);
#else
		std::cout<<std::endl<<"Connected to CAEN Digitizer Model "<<boardInfo.ModelName<<" as "<<b<<". board"<<std::endl;
		std::cout<<std::endl<<"Firmware is ROC "<<boardInfo.ROC_FirmwareRel<<", AMC "<<boardInfo.AMC_FirmwareRel<<std::endl;
#endif

		std::stringstream str(boardInfo.AMC_FirmwareRel);
		str>>majorNumber;
		if(majorNumber != 131 && majorNumber != 132 && majorNumber != 136) {
			throw std::runtime_error("This digitizer has no DPP-PSD firmware");
		}

		ProgramDigitizer(b);
		
		// we don't really need to know how many bytes have been allocated, so we use fBufferSize here
		if(fDebug) std::cout<<fHandle[b]<<"/"<<fBuffer.size()<<": trying to allocate memory for readout buffer "<<static_cast<void*>(fBuffer[b])<<std::endl;
		errorCode = CAEN_DGTZ_MallocReadoutBuffer(fHandle[b], &fBuffer[b], &fBufferSize[b]);
		if(errorCode != 0) {
			throw std::runtime_error(Form("Error %d when allocating readout buffer", errorCode));
		}
		if(fDebug) std::cout<<"allocated "<<fBufferSize[0]<<" bytes of buffer for board "<<b<<std::endl;
		// again, we don't care how many bytes have been allocated, so we use fNofEvents here
		fEvents[b] = new CAEN_DGTZ_DPP_PSD_Event_t*[fSettings->NumberOfChannels()];
		errorCode = CAEN_DGTZ_MallocDPPEvents(fHandle[b], reinterpret_cast<void**>(fEvents[b]), fNofEvents[b].data());
		if(errorCode != 0) {
			throw std::runtime_error(Form("Error %d when allocating DPP events", errorCode));
		}
#ifdef USE_WAVEFORMS
		// allocate waveforms, again not caring how many bytes have been allocated
		uint32_t size;
		errorCode = CAEN_DGTZ_MallocDPPWaveforms(fHandle[b], reinterpret_cast<void**>(&(fWaveforms[b])), &size);
		if(errorCode != 0) {
#ifdef USE_CURSES
			printw("error 2\n");
#else
			std::cout<<"error 2"<<std::endl;
#endif
			throw std::runtime_error(Form("Error %d when allocating DPP waveforms", errorCode));
		}
#endif
#ifdef USE_CURSES
		if(fDebug) printw("done with board %d\n", b);
#else
		if(fDebug) std::cout<<"done with board "<<b<<std::endl;
#endif
	}

	fOrdered = decltype(fOrdered)([](const CaenEvent* a, const CaenEvent* b) {
			return a->GetTime() < b->GetTime();
			});
}

CaenDigitizer::~CaenDigitizer()
{
	for(int b = 0; b < fSettings->NumberOfBoards(); ++b) {
		CAEN_DGTZ_FreeReadoutBuffer(&fBuffer[b]);
		CAEN_DGTZ_FreeDPPEvents(fHandle[b], reinterpret_cast<void**>(fEvents[b]));
#ifdef USE_WAVEFORMS
		CAEN_DGTZ_FreeDPPWaveforms(fHandle[b], reinterpret_cast<void*>(fWaveforms[b]));
#endif
		if(CAEN_DGTZ_CloseDigitizer(fHandle[b]) != 0) {
			std::cout<<"Failed to close "<<b<<". digitizer "<<fHandle[b]<<std::endl;
		}
	}
}

double CaenDigitizer::Run(TFile* outputFile, uint64_t events, double runTime)
{
	CAEN_DGTZ_ErrorCode errorCode;
	fOutputFile = outputFile;

	CreateTree();

	int ch = 0; //character read from input

	// start acquisition
	for(int b = 0; b < fSettings->NumberOfBoards(); ++b) {
		CAEN_DGTZ_SWStartAcquisition(fHandle[b]);
	}

#ifdef USE_CURSES
	printw("started data aquisition\n");
#else
	std::cout<<"started data aquisition"<<std::endl;
#endif

	fStopwatch.Start();

#ifdef USE_CURSES
	int x, y;
	getyx(stdscr, y, x);
#endif

	while(ch != 's') {
		// read data
		if(fDebug) {
			std::cout<<"--------------------------------------------------------------------------------"<<std::endl;
		}
		for(int b = 0; b < fSettings->NumberOfBoards(); ++b) {
			// reset fNofEvents
			for(int ch = 0; ch < fSettings->NumberOfChannels(); ++ch) {
				fNofEvents[b][ch] = 0;
			}
			errorCode = CAEN_DGTZ_ReadData(fHandle[b], CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, fBuffer[b], &fBufferSize[b]);
			if(errorCode != 0) {
				std::cerr<<"Error "<<errorCode<<" when reading data"<<std::endl;
				return -1.;
			}
			if(fDebug) {
				std::cout<<"Read "<<fBufferSize[b]<<" bytes"<<std::endl;
			}
			if(fBufferSize[b] > 0) {
				errorCode = CAEN_DGTZ_GetDPPEvents(fHandle[b], fBuffer[b], fBufferSize[b], reinterpret_cast<void**>(fEvents[b]), fNofEvents[b].data());
				if(errorCode != 0) {
					if(fDebug) std::cerr<<"Error "<<errorCode<<" when parsing events"<<std::endl;
					continue;
				}
			}
		}
		if(fDebug) {
			std::cout<<"----------------------------------------"<<std::endl;
		}
		SortEvents();
		if(fDebug) {
			std::cout<<"--------------------"<<std::endl;
		}
		WriteEvents();
		if(fDebug) {
			std::cout<<"----------------------------------------"<<std::endl;
		}
		// check if we got enough events
		if(events > 0 && fEventsRead > events) {
#ifdef USE_CURSES
			printw("Got %lu events after %.1f s, done!\n", fEventsRead, fStopwatch.RealTime());
#else
			std::cout<<"Got "<<fEventsRead<<" events after "<<fStopwatch.RealTime()<<" s, done!"<<std::endl;
#endif
			break;
		}
		// check if we've run long enough
		fRunTime =  fStopwatch.RealTime();
		fStopwatch.Continue();
		if(runTime > 0 && fRunTime > runTime) {
#ifdef USE_CURSES
			printw("Ran %.1f s, got %lu events = %.1f events/s, done!\n", fRunTime, fEventsRead, fEventsRead/fRunTime);
#else
			std::cout<<"Ran "<<fRunTime<<" s, got "<<fEventsRead<<" events = "<<fEventsRead/fRunTime<<" events/s, done!"<<std::endl;
#endif
			break;
		}
		if(fRunTime - fOldRunTime > fSettings->Update()) {
#ifdef USE_CURSES
			//printw("%.1f s, got %lu events = %.1f events/s\n", fRunTime, fEventsRead, fEventsRead/fRunTime);
			mvprintw(y, x, "%.1f s, got %lu events = %.1f events/s average, %.1f events/s in last %.1f seconds\n", fRunTime, fEventsRead, fEventsRead/fRunTime, (fEventsRead-fOldEventsRead)/(fRunTime - fOldRunTime), fRunTime - fOldRunTime);
#else
			std::cout<<fRunTime<<" s, got "<<fEventsRead<<" events = "<<fEventsRead/fRunTime<<" events/s average, "<<(fEventsRead-fOldEventsRead)/(fRunTime - fOldRunTime)<<" events/s in last "<<fRunTime-fOldRunTime<<" seconds"<<std::endl;
#endif
			fOldRunTime = fRunTime;
			fOldEventsRead = fEventsRead;
		}
#ifdef USE_CURSES
		if((ch = getch()) != ERR) {
			// nothing to be done here, the only key recognized right now is s which stopps the whole loop
			if(fDebug) {
				printw("got character %d = %c\n", ch, static_cast<char>(ch));
			}
		}
		if(fDebug) {
			printw("--------------------------------------------------------------------------------\n");
		}
#else
		if(fDebug) {
			std::cout<<"--------------------------------------------------------------------------------"<<std::endl;
		}
#endif
	}
#ifdef USE_CURSES
	printw("flushing remaining %lu events\n", fOrdered.size());
	refresh();
#else
#endif
	// write remaining events
	WriteEvents(true);
#ifdef USE_CURSES
	printw("done\n");
#else
	std::cout<<"done"<<std::endl;
#endif
	// stop acquisition
	for(int b = 0; b < fSettings->NumberOfBoards(); ++b) {
		CAEN_DGTZ_SWStopAcquisition(fHandle[b]);
	}
	// write tree
	fTree->Write("", TObject::kOverwrite);

	return fRunTime;
}

void CaenDigitizer::ProgramDigitizer(int b)
{
	if(fDebug) std::cout<<"programming digitizer "<<b<<std::endl;
	CAEN_DGTZ_ErrorCode errorCode;

	errorCode = CAEN_DGTZ_Reset(fHandle[b]);

	if(errorCode != 0) {
		throw std::runtime_error(Form("Error %d when resetting digitizer", errorCode));
	}

	errorCode = CAEN_DGTZ_SetDPPAcquisitionMode(fHandle[b], fSettings->AcquisitionMode(b), CAEN_DGTZ_DPP_SAVE_PARAM_EnergyAndTime);

	if(errorCode != 0) {
		throw std::runtime_error(Form("Error %d when setting DPP acquisition mode", errorCode));
	}

	errorCode = CAEN_DGTZ_SetAcquisitionMode(fHandle[b], CAEN_DGTZ_SW_CONTROLLED);

	if(errorCode != 0) {
		throw std::runtime_error(Form("Error %d when setting acquisition mode", errorCode));
	}

	errorCode = CAEN_DGTZ_SetIOLevel(fHandle[b], fSettings->IOLevel(b));

	if(errorCode != 0) {
		throw std::runtime_error(Form("Error %d when setting IO level", errorCode));
	}

	errorCode = CAEN_DGTZ_SetExtTriggerInputMode(fHandle[b], fSettings->TriggerMode(b));

	if(errorCode != 0) {
		throw std::runtime_error(Form("Error %d when setting external trigger DPP events", errorCode));
	}

	errorCode = CAEN_DGTZ_SetChannelEnableMask(fHandle[b], fSettings->ChannelMask(b));

	if(errorCode != 0) {
		throw std::runtime_error(Form("Error %d when setting channel mask", errorCode));
	}

	errorCode = CAEN_DGTZ_SetRunSynchronizationMode(fHandle[b], CAEN_DGTZ_RUN_SYNC_Disabled); // change to settings

	if(errorCode != 0) {
		throw std::runtime_error(Form("Error %d when setting run sychronization", errorCode));
	}

	errorCode = CAEN_DGTZ_SetDPPParameters(fHandle[b], fSettings->ChannelMask(b), static_cast<void*>(fSettings->ChannelParameter(b)));

	if(errorCode != 0) {
		throw std::runtime_error(Form("Error %d when setting dpp parameters", errorCode));
	}

	// write some special registers directly
	uint32_t address;
	uint32_t data;
	// enable EXTRA word
	address = 0x8000;
	CAEN_DGTZ_ReadRegister(fHandle[b], address, &data);
	data |= 0x10000; // no mask necessary, we just set one bit
	CAEN_DGTZ_WriteRegister(fHandle[b], address, data);

	for(int ch = 0; ch < fSettings->NumberOfChannels(); ++ch) {
		if((fSettings->ChannelMask(b) & (1<<ch)) != 0) {
			if(fDebug) std::cout<<"programming channel "<<ch<<std::endl;
			if(ch%2 == 0) {
				errorCode = CAEN_DGTZ_SetRecordLength(fHandle[b], fSettings->RecordLength(b, ch), ch);
			}

			errorCode = CAEN_DGTZ_SetChannelDCOffset(fHandle[b], ch, fSettings->DCOffset(b, ch));

			errorCode = CAEN_DGTZ_SetDPPPreTriggerSize(fHandle[b], ch, fSettings->PreTrigger(b, ch));

			errorCode = CAEN_DGTZ_SetChannelPulsePolarity(fHandle[b], ch, fSettings->PulsePolarity(b, ch));

			if(fSettings->EnableCfd(b, ch)) {
				if(fDebug) std::cout<<"enabling CFD on channel "<<ch<<std::endl;
				// enable CFD mode
				address = 0x1080 + ch*0x100;
				CAEN_DGTZ_ReadRegister(fHandle[b], address, &data);
				data |= 0x40; // no mask necessary, we just set one bit
				CAEN_DGTZ_WriteRegister(fHandle[b], address, data);

				// set CFD parameters
				address = 0x103c + ch*0x100;
				CAEN_DGTZ_ReadRegister(fHandle[b], address, &data);
				data = (data & ~0xfff) | fSettings->CfdParameters(b, ch);
				CAEN_DGTZ_WriteRegister(fHandle[b], address, data);
			}
			// write extended TS, flags, and fine TS (from CFD) to extra word
			address = 0x1084 + ch*0x100;
			CAEN_DGTZ_ReadRegister(fHandle[b], address, &data);
			data = (data & ~0x700) | 0x200;
			CAEN_DGTZ_WriteRegister(fHandle[b], address, data);
		}
	}

	errorCode = CAEN_DGTZ_SetDPPEventAggregation(fHandle[b], fSettings->EventAggregation(b), 0);

	errorCode = CAEN_DGTZ_SetDPP_VirtualProbe(fHandle[b], ANALOG_TRACE_2,  CAEN_DGTZ_DPP_VIRTUALPROBE_CFD);

	errorCode = CAEN_DGTZ_SetDPP_VirtualProbe(fHandle[b], DIGITAL_TRACE_1, CAEN_DGTZ_DPP_DIGITALPROBE_Gate);

	errorCode = CAEN_DGTZ_SetDPP_VirtualProbe(fHandle[b], DIGITAL_TRACE_2, CAEN_DGTZ_DPP_DIGITALPROBE_GateShort);
	if(fDebug) std::cout<<"done with digitizer "<<b<<std::endl;
}

void CaenDigitizer::CreateTree()
{
	fTree = new TTree("tree", "tree");
	fTree->Branch("event",&fEvent);
}

void CaenDigitizer::DecodeData(int b)
{
	// is there a header on this data???
	//

	uint32_t i;
	uint16_t tmpInt16;
	//for(int b = 0; b < fSettings->NumberOfBoards(); ++b) {
	if(fDebug) {
		std::cout<<"sorting board "<<b<<" with buffer "<<static_cast<void*>(fBuffer[b])<<std::endl;
		for(i = 0; i < fBufferSize[b]; i += 4) {
			if(i%16 == 0) {
				std::cout<<std::endl
					<<std::setw(4)<<i/4<<" "<<std::flush;
			}
			std::cout<<std::hex<<"0x"<<std::setw(8)<<std::setfill('0')<<*reinterpret_cast<uint32_t*>(fBuffer[b]+i)<<std::dec<<" ";
		}
		std::cout<<std::endl;
	}
	i = 0;
	while(i+12 < fBufferSize[b]) {
		//first: 32 bit trigger time
		fEvent->TriggerTime(*reinterpret_cast<uint32_t*>(fBuffer[b]+i));
		i += 4;
		//second: 16 bit energy
		fEvent->Energy(*reinterpret_cast<uint16_t*>(fBuffer[b]+i));
		i += 2;
		//third: 32 bit "extra" = 16 bit extended time stamp, 6 bits flags, and 10 bits fine time stamp
		fEvent->ExtendedTimestamp(*reinterpret_cast<uint16_t*>(fBuffer[b]+i));
		i += 2;
		tmpInt16 = *reinterpret_cast<uint16_t*>(fBuffer[b]+i);
		i += 2;
		fEvent->Cfd(tmpInt16 & 0x3ff);
		fEvent->LostTrigger((tmpInt16 & 0x8000) == 0x8000);
		fEvent->OverRange((tmpInt16 & 0x4000) == 0x4000);
		fEvent->KiloCount((tmpInt16 & 0x2000) == 0x2000);
		fEvent->NLostCount((tmpInt16 & 0x1000) == 0x1000);
		//fourth: 16 bit short Gate
		fEvent->ShortGate(*reinterpret_cast<uint16_t*>(fBuffer[b]+i));
		i += 2;
		fTree->Fill();
		++fEventsRead;
	}
	//}
}

bool CaenDigitizer::CheckEvent(const CAEN_DGTZ_DPP_PSD_Event_t& event)
{
	if(event.TimeTag == 0 && (event.Extras>>16) == 0 && (event.Extras & 0x3ff) == 0) {
		if(fDebug) {
			std::cout<<"empty time"<<std::endl;
		}
		return false;
	}
	if(fDebug) { 
		std::cout<<"times: "<<(event.Extras>>16)<<", "<<event.TimeTag<<", "<<(event.Extras & 0x3ff)<<std::endl;
	}
	return true;
}

void CaenDigitizer::SortEvents()
{
#ifdef USE_WAVEFORMS
	CAEN_DGTZ_ErrorCode errorCode;
#endif
	for(int b = 0; b < fSettings->NumberOfBoards(); ++b) {
		for(int ch = 0; ch < fSettings->NumberOfChannels(); ++ch) {
			for(unsigned int ev = 0; ev < fNofEvents[b][ch]; ++ev) {
				if(!CheckEvent(fEvents[b][ch][ev])) {
					if(fDebug) {
						std::cout<<"Skipping event "<<fEventsRead<<", board "<<b<<", channel "<<ch<<", event "<<ev<<" with all times zero!"<<std::endl;
					}
					continue;
				}
#ifdef USE_WAVEFORMS
				errorCode = CAEN_DGTZ_DecodeDPPWaveforms(fHandle[b], reinterpret_cast<void*>(fEvents[b][ch]+ev), reinterpret_cast<void*>(fWaveforms[b]));
				if(errorCode != 0) {
					if(fDebug) {
						std::cout<<"failed to decode waveform for board "<<b<<", channel "<<ch<<", event "<<ev<<": "<<fEvents[b][ch][ev].Waveforms<<std::endl;
					}
					fWaveforms[b] = nullptr;
				}
				auto tmpEvent = new CaenEvent(ch, fEvents[b][ch][ev], fWaveforms[b]);
#else
				auto tmpEvent = new CaenEvent(ch, fEvents[b][ch][ev], nullptr);
#endif
				fOrdered.insert(tmpEvent);
				++fEventsRead;
				if(fDebug) {
					std::cout<<"board "<<b<<", channel "<<ch<<", event "<<ev<<":"<<std::endl;
					tmpEvent->Print();
					std::cout<<"total events read "<<fEventsRead<<std::endl;
				}
				//fTree->Fill();
			}
		}
	}
}

void CaenDigitizer::WriteEvents(bool finish)
{
	if(fOrdered.empty()) {
		return;
	}
#ifdef USE_CURSES
	int x, y;
	getyx(stdscr, y, x);
#endif
	while(finish || fOrdered.size() > fSettings->BufferSize()) {
		fEvent = *fOrdered.begin();
		if(fDebug) {
			std::cout<<"Writing event "<<fTree->GetEntries()<<std::endl;
			fEvent->Print();
		}
		fTree->Fill();
		//delete fOrdered.begin();
		fOrdered.erase(fOrdered.begin());
		if(finish) {
			if(true || fOrdered.size()%1000 == 0) {
#ifdef USE_CURSES
				mvprintw(y, x, "%8lu events remaining\n", fOrdered.size());
				refresh();
#else
				std::cout<<std::setw(8)<<fOrdered.size()<<" events remaining\r"<<std::flush;
#endif
			}
			if(fOrdered.size() == 0) {
				std::cout<<std::endl;
				break;
			}
		}
	} 
}
