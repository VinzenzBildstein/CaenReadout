#include "CaenEvent.hh"

#include <iostream>

ClassImp(CaenEvent)

CaenEvent::CaenEvent(int channel, const CAEN_DGTZ_DPP_PSD_Event_t& event, const CAEN_DGTZ_DPP_PSD_Waveforms_t* waveforms)
{
	Read(channel, event, waveforms);
}

void CaenEvent::Read(int channel, const CAEN_DGTZ_DPP_PSD_Event_t& event, const CAEN_DGTZ_DPP_PSD_Waveforms_t* waveforms)
{
	fChannel = channel;
	fTriggerTime = event.TimeTag;
	fEnergy = event.ChargeLong;
	fExtendedTimestamp = (event.Extras>>16);
	fCfd = (event.Extras & 0x3ff);
	fLostTrigger = ((event.Extras & 0x8000) == 0x8000);
	fOverRange = ((event.Extras & 0x4000) == 0x4000);
	fKiloCount = ((event.Extras & 0x2000) == 0x2000);
	fNLostCount = ((event.Extras & 0x1000) == 0x1000);
	fShortGate = event.ChargeShort;
	fFormat = event.Format;
	fFormat2 = event.Format2;
	fBaseline = event.Baseline;
	fPur = event.Pur;
	fWaveforms.resize(2);
	fDigitalWaveforms.resize(4);
	if(waveforms != nullptr) {
		fWaveforms[0].assign(waveforms->Trace1, waveforms->Trace1 + waveforms->Ns);
		fWaveforms[1].assign(waveforms->Trace2, waveforms->Trace2 + waveforms->Ns);
		fDigitalWaveforms[0].assign(waveforms->DTrace1, waveforms->DTrace1 + waveforms->Ns);
		fDigitalWaveforms[1].assign(waveforms->DTrace2, waveforms->DTrace2 + waveforms->Ns);
		fDigitalWaveforms[2].assign(waveforms->DTrace3, waveforms->DTrace3 + waveforms->Ns);
		fDigitalWaveforms[3].assign(waveforms->DTrace4, waveforms->DTrace4 + waveforms->Ns);
	}
}

uint64_t CaenEvent::GetTimestamp() const {
	uint64_t timestamp = fExtendedTimestamp;
	timestamp = (timestamp<<31) | fTriggerTime;
	return timestamp;
}

double CaenEvent::GetTime() const
{
	// CFD is 10 bits for 2 ns (500 MHz sampling)
	return GetTimestamp()*2. + (fCfd/512.);
}

void CaenEvent::Print(Option_t*) const
{
	std::cout<<"event "<<this<<std::endl;
	std::cout<<"trigger time = "<<fTriggerTime<<" = 0x"<<std::hex<<fTriggerTime<<std::dec<<std::endl;
	std::cout<<"energy = "<<fEnergy<<" = 0x"<<std::hex<<fEnergy<<std::dec<<std::endl;
	std::cout<<"extended TS = "<<fExtendedTimestamp<<" = 0x"<<std::hex<<fExtendedTimestamp<<std::dec<<std::endl;
	std::cout<<"fine TS = "<<fCfd<<" = 0x"<<std::hex<<fCfd<<std::dec<<std::endl;
	std::cout<<"lost trigger "<<(fLostTrigger?"true":"false")<<std::endl;
	std::cout<<"over range "<<(fOverRange?"true":"false")<<std::endl;
	std::cout<<"kilo count "<<(fKiloCount?"true":"false")<<std::endl;
	std::cout<<"nth lost count "<<(fNLostCount?"true":"false")<<std::endl;
	std::cout<<"short gate = "<<fShortGate<<" = 0x"<<std::hex<<fShortGate<<std::dec<<std::endl;
	std::cout<<"format = "<<fFormat<<" = 0x"<<std::hex<<fFormat<<std::dec<<std::endl;
	std::cout<<"format2 = "<<fFormat2<<" = 0x"<<std::hex<<fFormat2<<std::dec<<std::endl;
	std::cout<<"baseline = "<<fBaseline<<" = 0x"<<std::hex<<fBaseline<<std::dec<<std::endl;
	std::cout<<"pur = "<<fPur<<" = 0x"<<std::hex<<fPur<<std::dec<<std::endl;
}
