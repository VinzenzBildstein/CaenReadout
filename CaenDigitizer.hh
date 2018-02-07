#ifndef CAENDIGITIZER_HH
#define CAENDIGITIZER_HH
#include <vector>
#include <string>
#include <functional>

#include "TFile.h"
#include "TTree.h"
#include "TStopwatch.h"

#include "CaenSettings.hh"
#include "CaenEvent.hh"

class CaenDigitizer {
public:
	CaenDigitizer(const CaenSettings& settings, bool debug);
	~CaenDigitizer();

	double Run(TFile* outputFile, uint64_t events = 0, double runTime = 0);

private:
	void ProgramDigitizer(int board);
	void CreateTree();
	void DecodeData(int b);
	bool CheckEvent(const CAEN_DGTZ_DPP_PSD_Event_t& event);
	void SortEvents();
	void WriteEvents(bool finish = false);

	const CaenSettings* fSettings;
	TFile* fOutputFile;
	TTree* fTree;

	CaenEvent* fEvent;
	std::vector<int> fHandle;
	// raw readout data
	std::vector<char*>    fBuffer; 
	std::vector<uint32_t> fBufferSize;
	// DPP events
	std::vector<CAEN_DGTZ_DPP_PSD_Event_t**> fEvents;
	std::vector<std::vector<uint32_t> >      fNofEvents;
	// waveforms
	std::vector<CAEN_DGTZ_DPP_PSD_Waveforms_t*> fWaveforms;

	// multiset to store and sort event
	std::multiset<CaenEvent*, std::function<bool(const CaenEvent*, const CaenEvent*)> > fOrdered;

	TStopwatch fStopwatch;

	uint64_t fEventsRead;
	double   fRunTime;
	uint64_t fOldEventsRead;
	double   fOldRunTime;

	bool fDebug;
};
#endif
