#ifndef MULTIFLEX_SIMULATION_DATA_GENERATOR
#define MULTIFLEX_SIMULATION_DATA_GENERATOR

#include <SimulationChannelDescriptor.h>
#include <string>
class MultiFlexAnalyzerSettings;

class MultiFlexSimulationDataGenerator
{
public:
	MultiFlexSimulationDataGenerator();
	~MultiFlexSimulationDataGenerator();

	void Initialize( U32 simulation_sample_rate, MultiFlexAnalyzerSettings* settings );
	U32 GenerateSimulationData( U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channel );

protected:
	MultiFlexAnalyzerSettings* mSettings;
	U32 mSimulationSampleRateHz;

protected:
	void CreateSerialByte();
	std::string mSerialText;
	U32 mStringIndex;

	SimulationChannelDescriptor mSerialSimulationData;

};
#endif //MULTIFLEX_SIMULATION_DATA_GENERATOR