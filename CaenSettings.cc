#include "CaenSettings.hh"

#include <iostream>
#include <iomanip>
#include <curses.h>

#include "TEnv.h"

ClassImp(CaenSettings)

CaenSettings::CaenSettings()
{
	printw("This constructor should only be used by ROOT!\n");
}

CaenSettings::CaenSettings(const std::string& filename, bool debug)
{
	auto settings = new TEnv(filename.c_str());
	if(settings == nullptr || settings->ReadFile(filename.c_str(), kEnvLocal) != 0) {
		printw("Error occured trying to read \"%s\"\n", filename.c_str());
		throw;
	}

	fUpdate = settings->GetValue("UpdateFrequency", 1.);

	fNumberOfBoards = settings->GetValue("NumberOfBoards", 1);
	if(fNumberOfBoards < 1) {
		printw("%d boards is not possible!\n",fNumberOfBoards);
		throw;
	}
	fNumberOfChannels = settings->GetValue("NumberOfChannels", 8);
	if(fNumberOfChannels < 1) {
		printw("%d maximum channels is not possible!\n", fNumberOfChannels);
		throw;
	}
	fBufferSize = settings->GetValue("BufferSize", 100000);

	fLinkType.resize(fNumberOfBoards);
	fVmeBaseAddress.resize(fNumberOfBoards);
	fAcquisitionMode.resize(fNumberOfBoards);
	fSaveParam.resize(fNumberOfBoards);
	fIOLevel.resize(fNumberOfBoards);
	fChannelMask.resize(fNumberOfBoards);
	fRunSync.resize(fNumberOfBoards);
	fEventAggregation.resize(fNumberOfBoards);
	fTriggerMode.resize(fNumberOfBoards);
	fRecordLength.resize(fNumberOfBoards);
	fDCOffset.resize(fNumberOfBoards);
	fPreTrigger.resize(fNumberOfBoards);
	fPulsePolarity.resize(fNumberOfBoards);
	fEnableCfd.resize(fNumberOfBoards);
	fCfdParameters.resize(fNumberOfBoards);
	fChannelParameter.resize(fNumberOfBoards, new CAEN_DGTZ_DPP_PSD_Params_t);
	for(int i = 0; i < fNumberOfBoards; ++i) {
		fLinkType[i]         = CAEN_DGTZ_USB;//0
		fVmeBaseAddress[i]   = 0;
		fAcquisitionMode[i]  = static_cast<CAEN_DGTZ_DPP_AcqMode_t>(settings->GetValue(Form("Board.%d.AcquisitionMode", i), CAEN_DGTZ_DPP_ACQ_MODE_Mixed));//2
		fSaveParam[i]        = static_cast<CAEN_DGTZ_DPP_SaveParam_t>(settings->GetValue(Form("Board.%d.SaveParam", i), CAEN_DGTZ_DPP_SAVE_PARAM_EnergyAndTime));//2
		fIOLevel[i]          = static_cast<CAEN_DGTZ_IOLevel_t>(settings->GetValue(Form("Board.%d.IOlevel", i), CAEN_DGTZ_IOLevel_NIM));//0
		fChannelMask[i]      = settings->GetValue(Form("Board.%d.ChannelMask", i), 0xff);
		fRunSync[i]          = static_cast<CAEN_DGTZ_RunSyncMode_t>(settings->GetValue(Form("Board.%d.RunSync", i), CAEN_DGTZ_RUN_SYNC_Disabled));//0
		fEventAggregation[i] = settings->GetValue(Form("Board.%d.EventAggregate", i), 0);
		fTriggerMode[i]      = static_cast<CAEN_DGTZ_TriggerMode_t>(settings->GetValue(Form("Board.%d.TriggerMode", i), CAEN_DGTZ_TRGMODE_ACQ_ONLY));//1

		fRecordLength[i].resize(fNumberOfChannels);
		fDCOffset[i].resize(fNumberOfChannels);
		fPreTrigger[i].resize(fNumberOfChannels);
		fPulsePolarity[i].resize(fNumberOfChannels);
		fEnableCfd[i].resize(fNumberOfChannels);
		fCfdParameters[i].resize(fNumberOfChannels);
		for(int ch = 0; ch < fNumberOfChannels; ++ch) {
			fRecordLength[i][ch]  = settings->GetValue(Form("Board.%d.Channel.%d.RecordLength", i, ch), 192);
			fDCOffset[i][ch]      = settings->GetValue(Form("Board.%d.Channel.%d.DcOffset", i, ch), 0x8000);
			fPreTrigger[i][ch]    = settings->GetValue(Form("Board.%d.Channel.%d.PreTrigger", i, ch), 80);
			fPulsePolarity[i][ch] = static_cast<CAEN_DGTZ_PulsePolarity_t>(settings->GetValue(Form("Board.%d.Channel.%d.PulsePolarity", i, ch), CAEN_DGTZ_PulsePolarityNegative));//1
			fEnableCfd[i][ch]     = settings->GetValue(Form("Board.%d.Channel.%d.EnableCfd", i, ch), true);
			fCfdParameters[i][ch] = (settings->GetValue(Form("Board.%d.Channel.%d.CfdDelay", i, ch), 5) & 0xff);
			fCfdParameters[i][ch] |= (settings->GetValue(Form("Board.%d.Channel.%d.CfdFraction", i, ch), 0) & 0x3) << 8;
			fCfdParameters[i][ch] |= (settings->GetValue(Form("Board.%d.Channel.%d.CfdInterpolationPoints", i, ch), 0) & 0x3) << 10;
		}

		fChannelParameter[i]->purh   = static_cast<CAEN_DGTZ_DPP_PUR_t>(settings->GetValue(Form("Board.%d.PileUpRejection", i), CAEN_DGTZ_DPP_PSD_PUR_DetectOnly));//0
		fChannelParameter[i]->purgap = settings->GetValue(Form("Board.%d.PurityGap", i), 100);
		fChannelParameter[i]->blthr  = settings->GetValue(Form("Board.%d.BaseLine.Threshold", i), 3);
		fChannelParameter[i]->bltmo  = settings->GetValue(Form("Board.%d.BaseLine.Timeout", i), 100);
		fChannelParameter[i]->trgho  = settings->GetValue(Form("Board.%d.TriggerHoldOff", i), 8);
		for(int ch = 0; ch < fNumberOfChannels; ++ch) {
			fChannelParameter[i]->thr[ch]   = settings->GetValue(Form("Board.%d.Channel.%d.Threshold", i, ch), 50);
			fChannelParameter[i]->nsbl[ch]  = settings->GetValue(Form("Board.%d.Channel.%d.BaselineSamples", i, ch), 4);
			fChannelParameter[i]->lgate[ch] = settings->GetValue(Form("Board.%d.Channel.%d.LongGate", i, ch), 32);
			fChannelParameter[i]->sgate[ch] = settings->GetValue(Form("Board.%d.Channel.%d.ShortGate", i, ch), 24);
			fChannelParameter[i]->pgate[ch] = settings->GetValue(Form("Board.%d.Channel.%d.PreGate", i, ch), 8);
			fChannelParameter[i]->selft[ch] = settings->GetValue(Form("Board.%d.Channel.%d.SelfTrigger", i, ch), 1);
			fChannelParameter[i]->trgc[ch]  = static_cast<CAEN_DGTZ_DPP_TriggerConfig_t>(settings->GetValue(Form("Board.%d.Channel.%d.TriggerConfiguration", i, ch), CAEN_DGTZ_DPP_TriggerConfig_Threshold));//1
			fChannelParameter[i]->tvaw[ch]  = settings->GetValue(Form("Board.%d.Channel.%d.TriggerValidationAcquisitionWindow", i, ch), 50);
			fChannelParameter[i]->csens[ch] = settings->GetValue(Form("Board.%d.Channel.%d.ChargeSensitivity", i, ch), 0);
		}
	}

	if(debug) Print();
}

CaenSettings::~CaenSettings()
{
}

void CaenSettings::Print()
{
	std::cout<<fNumberOfBoards<<" boards with "<<fNumberOfChannels<<" channels:"<<std::endl;
	for(int i = 0; i < fNumberOfBoards; ++i) {
		std::cout<<"Board #"<<i<<":"<<std::endl;
		std::cout<<"  link type ";
		switch(fLinkType[i]) {
			case CAEN_DGTZ_USB:
				std::cout<<"USB"<<std::endl;
				break;
			case CAEN_DGTZ_OpticalLink:
				std::cout<<"Optical Link"<<std::endl;
				break;
			default:
				std::cout<<"unknown"<<std::endl;
				break;
		}
		std::cout<<"   VME base address 0x"<<std::hex<<fVmeBaseAddress[i]<<std::dec<<std::endl;
		std::cout<<"   acquisition mode ";
		switch(fAcquisitionMode[i]) {
			case CAEN_DGTZ_DPP_ACQ_MODE_Oscilloscope:
				std::cout<<"oscilloscope"<<std::endl;
				break;
			case CAEN_DGTZ_DPP_ACQ_MODE_List:
				std::cout<<"list mode"<<std::endl;
				break;
			case CAEN_DGTZ_DPP_ACQ_MODE_Mixed:
				std::cout<<"mixed"<<std::endl;
				break;
			//case CAEN_DGTZ_SW_CONTROLLED:
			//	std::cout<<"software controlled"<<std::endl;
			//	break;
			//case CAEN_DGTZ_S_IN_CONTROLLED:
			//	std::cout<<"external signal controlled"<<std::endl;
			//	break;
			//case CAEN_DGTZ_FIRST_TRG_CONTROLLED:
			//	std::cout<<"first trigger controlled"<<std::endl;
			//	break;
			default:
				std::cout<<"unknown"<<std::endl;
				break;
		}
		std::cout<<"   IO level ";
		switch(fIOLevel[i]) {
			case CAEN_DGTZ_IOLevel_NIM:
				std::cout<<"NIM"<<std::endl;
				break;
			case CAEN_DGTZ_IOLevel_TTL:
				std::cout<<"TTL"<<std::endl;
				break;
			default:
				std::cout<<"unknown"<<std::endl;
				break;
		}
		std::cout<<"   channel mask 0x"<<std::hex<<fChannelMask[i]<<std::dec<<std::endl;
		std::cout<<"   run sync ";
		switch(fRunSync[i]) {
			case CAEN_DGTZ_RUN_SYNC_Disabled:
				std::cout<<"disabled"<<std::endl;
				break;
			case CAEN_DGTZ_RUN_SYNC_TrgOutTrgInDaisyChain:
				std::cout<<"trigger out/trigger in chain"<<std::endl;
				break;
			case CAEN_DGTZ_RUN_SYNC_TrgOutSinDaisyChain:
				std::cout<<"trigger out/s in chain"<<std::endl;
				break;
			case CAEN_DGTZ_RUN_SYNC_SinFanout:
				std::cout<<"s in fanout"<<std::endl;
				break;
			case CAEN_DGTZ_RUN_SYNC_GpioGpioDaisyChain:
				std::cout<<"gpio chain"<<std::endl;
				break;
			default:
				std::cout<<"unknown"<<std::endl;
				break;
		}
		std::cout<<"   event aggregation "<<fEventAggregation[i]<<std::endl;
		std::cout<<"   trigger mode "<<fTriggerMode[i]<<std::endl;
		for(int ch = 0; ch < fNumberOfChannels; ++ch) {
			std::cout<<"   Channel #"<<ch<<":"<<std::endl;
			std::cout<<"      record length "<<fRecordLength[i][ch]<<std::endl;
			std::cout<<"      DC offset 0x"<<std::hex<<fDCOffset[i][ch]<<std::dec<<std::endl;
			std::cout<<"      pre trigger "<<fPreTrigger[i][ch]<<std::endl;
			std::cout<<"      pulse polarity ";
			switch(fPulsePolarity[i][ch]) {
				case CAEN_DGTZ_PulsePolarityPositive:
					std::cout<<"positive"<<std::endl;
					break;
				case CAEN_DGTZ_PulsePolarityNegative:
					std::cout<<"negative"<<std::endl;
					break;
				default:
					std::cout<<"unknown"<<std::endl;
					break;
			}
			if(fEnableCfd[i][ch]) {
				std::cout<<"      cfd enabled"<<std::endl;
				std::cout<<"      cfd parameters 0x"<<std::hex<<fCfdParameters[i][ch]<<std::dec<<std::endl;
			} else {
				std::cout<<"      cfd disabled"<<std::endl;
			}
		}
		std::cout<<"   pile-up rejection mode ";
		switch(fChannelParameter[i]->purh) {
			case CAEN_DGTZ_DPP_PSD_PUR_DetectOnly:
				std::cout<<"detection only"<<std::endl;
				break;
			case CAEN_DGTZ_DPP_PSD_PUR_Enabled:
				std::cout<<"enabled"<<std::endl;
				break;
			default:
				std::cout<<"unknown"<<std::endl;
				break;
		}
		std::cout<<"   pile-up gap "<<fChannelParameter[i]->purgap<<std::endl;
		std::cout<<"   baseline threshold "<<fChannelParameter[i]->blthr<<std::endl;
		std::cout<<"   baseline timeout "<<fChannelParameter[i]->bltmo<<std::endl;
		std::cout<<"   trigger holdoff "<<fChannelParameter[i]->trgho<<std::endl;
		for(int ch = 0; ch < fNumberOfChannels; ++ch) {
			std::cout<<"   Channel #"<<ch<<":"<<std::endl;
			std::cout<<"      threshold "<<fChannelParameter[i]->thr[ch]<<std::endl;
			std::cout<<"      baseline samples "<<fChannelParameter[i]->nsbl[ch]<<std::endl;
			std::cout<<"      long gate "<<fChannelParameter[i]->lgate[ch]<<std::endl;
			std::cout<<"      short gate "<<fChannelParameter[i]->sgate[ch]<<std::endl;
			std::cout<<"      pre-gate "<<fChannelParameter[i]->pgate[ch]<<std::endl;
			std::cout<<"      self trigger "<<fChannelParameter[i]->selft[ch]<<std::endl;
			std::cout<<"      trigger conf. ";
			switch(fChannelParameter[i]->trgc[ch]) {
				case CAEN_DGTZ_DPP_TriggerConfig_Peak:
					std::cout<<" peak"<<std::endl;
					break;
				case CAEN_DGTZ_DPP_TriggerConfig_Threshold:
					std::cout<<" threshold"<<std::endl;
					break;
				default:
					std::cout<<"unknown"<<std::endl;
					break;
			}
			std::cout<<"      trigger val. window "<<fChannelParameter[i]->tvaw[ch]<<std::endl;
			std::cout<<"      charge sensitivity "<<fChannelParameter[i]->csens[ch]<<std::endl;
		}
	}
	
	//std::vector<CAEN_DGTZ_DPP_PSD_Params_t*> fChannelParameter;
}

