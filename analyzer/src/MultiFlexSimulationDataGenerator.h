#ifndef MULTIFLEX_SIMULATION_DATA_GENERATOR
#define MULTIFLEX_SIMULATION_DATA_GENERATOR

#include <SimulationChannelDescriptor.h>

class MultiFlexAnalyzerSettings;

class MultiFlexSimulationDataGenerator
{
public:
  MultiFlexSimulationDataGenerator();
  ~MultiFlexSimulationDataGenerator();

  void Initialize(U32 simulation_sample_rate, MultiFlexAnalyzerSettings* settings);
  U32 GenerateSimulationData(U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channels);

protected:
  void CreateTransaction(const U8* tx_words, const U8* rx_words, U32 num_words);
  void AdvanceAll(U32 samples);

  MultiFlexAnalyzerSettings* mSettings;
  U32 mSimulationSampleRateHz;
  U32 mSamplesPerHalfClock;

  SimulationChannelDescriptorGroup mSimChannels;
  SimulationChannelDescriptor* mClkSim;
  SimulationChannelDescriptor* mSyncSim;
  SimulationChannelDescriptor* mTx2Sim;
  SimulationChannelDescriptor* mTx1Sim;
  SimulationChannelDescriptor* mTx0Sim;
  SimulationChannelDescriptor* mRx2Sim;
  SimulationChannelDescriptor* mRx1Sim;
  SimulationChannelDescriptor* mRx0Sim;
};

#endif // MULTIFLEX_SIMULATION_DATA_GENERATOR
