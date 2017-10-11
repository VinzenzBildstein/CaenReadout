#include <iostream>

#include "TFile.h"
#include "TTree.h"
#include "TH1.h"
#include "TH2.h"

#include "CaenEvent.hh"

int main(int argc, char** argv)
{
	if(argc != 2) {
		std::cerr<<"Usage: "<<argv[0]<<" <input-file>"<<std::endl;
		return 1;
	}

	TFile input(argv[1]);
	if(!input.IsOpen()) {
		std::cerr<<"Failed to open input file \""<<argv[1]<<"\""<<std::endl;
		return 1;
	}

	TTree* tree = static_cast<TTree*>(input.Get("tree"));
	CaenEvent* event = nullptr;
	tree->SetBranchAddress("event", &event);

	TFile output(Form("hist_%s", argv[1]), "recreate");
	if(!output.IsOpen()) {
		std::cerr<<"Failed to open output file \""<<Form("hist_%s", argv[1])<<"\""<<std::endl;
		return 1;
	}

	std::map<int, CaenEvent> lastEvents;

	TH1F tsDiff("tsDiff", "#DeltaTS, different channels;#DeltaTS [sample]", 100000, 0., 100000.);
	TH1F tDiff("tDiff", "#Deltat, different channels;#Deltat [#mus]", 10000, 0., 1000.);
	TH2F tDiffZoomVsCharge("tDiffZoomVsCharge", "#Deltat, different channels vs. charge;charge [channels];#Deltat [ns]", 1000, 0., 65000., 1280, 0., 10.);
	TH2F cfdDiffVsTsDiff("cfdDiffVsTsDiff", "#DeltaCFD vs. #DeltaTS;#DeltaTS [sample];#DeltaCFD [ns]", 10, -0., 9.5, 512, 0., 2.);
	TH2F tDiffSame("tDiffSame", "#Deltat, same channel;Channel Number;#Deltat [#mus]", 2, -0., 1.5, 1000, 0., 1000.);
	TH2F channelVsCharge("channelVsCharge", "Channel # vs. charge;charge [channels]", 1000, 0., 65000., 8, -0.5, 7.5);
	TH2F channelVsShortGate("channelVsShortGate", "Channel # vs. short gate;short gate [channels]", 1000, 0., 65000., 8, -0.5, 7.5);

	for(Long64_t i = 0; i < tree->GetEntries(); ++i) {
		tree->GetEntry(i);

		// loop over all previous events
		for(auto it : lastEvents) {
			if(it.first == event->Channel()) {
				// same channel
				tDiffSame.Fill(it.first, (event->GetTime() - it.second.GetTime())/1e3);
			} else {
				// different channel
				tsDiff.Fill(event->GetTimestamp() - it.second.GetTimestamp());
				tDiff.Fill((event->GetTime() - it.second.GetTime())/1e3);
				tDiffZoomVsCharge.Fill(event->Energy(), event->GetTime() - it.second.GetTime());
				cfdDiffVsTsDiff.Fill(event->GetTimestamp() - it.second.GetTimestamp(), (event->Cfd() - it.second.Cfd())/512.);
			}
		}

		channelVsCharge.Fill(event->Energy(), event->Channel());
		channelVsShortGate.Fill(event->ShortGate(), event->Channel());

		// update last event of this channel to current event
		lastEvents[event->Channel()] = *event;

		if(i%1000 == 0) {
			std::cout<<std::setw(3)<<(100*i)/tree->GetEntries()<<" % done\r"<<std::flush;
		}
	}
	std::cout<<"100 % done"<<std::endl;

	input.Close();

	tsDiff.Write();
	tDiff.Write();
	tDiffZoomVsCharge.Write();
	cfdDiffVsTsDiff.Write();
	tDiffSame.Write();
	channelVsCharge.Write();
	channelVsShortGate.Write();

	output.Close();

	return 0;
}
