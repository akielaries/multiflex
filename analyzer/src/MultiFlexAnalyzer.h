#ifndef MULTIFLEX_ANALYZER_H
#define MULTIFLEX_ANALYZER_H

#include <Analyzer.h>
#include "MultiFlexAnalyzerSettings.h"
#include "MultiFlexAnalyzerResults.h"
#include "MultiFlexSimulationDataGenerator.h"
#include <memory>

class ANALYZER_EXPORT MultiFlexAnalyzer : public Analyzer2
{
public:
	MultiFlexAnalyzer();
	virtual ~MultiFlexAnalyzer();

	virtual void SetupResults();
	virtual void WorkerThread();

	virtual U32 GenerateSimulationData( U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channels );
	virtual U32 GetMinimumSampleRateHz();

	virtual const char* GetAnalyzerName() const;
	virtual bool NeedsRerun();

protected: //vars
	MultiFlexAnalyzerSettings mSettings;
	std::unique_ptr<MultiFlexAnalyzerResults> mResults;
	AnalyzerChannelData* mSerial;

	MultiFlexSimulationDataGenerator mSimulationDataGenerator;
	bool mSimulationInitilized;

	//Serial analysis vars:
	U32 mSampleRateHz;
	U32 mStartOfStopBitOffset;
	U32 mEndOfStopBitOffset;
};

extern "C" ANALYZER_EXPORT const char* __cdecl GetAnalyzerName();
extern "C" ANALYZER_EXPORT Analyzer* __cdecl CreateAnalyzer( );
extern "C" ANALYZER_EXPORT void __cdecl DestroyAnalyzer( Analyzer* analyzer );

#endif //MULTIFLEX_ANALYZER_H
