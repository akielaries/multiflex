#include "MultiFlexSimulationDataGenerator.h"
#include "MultiFlexAnalyzerSettings.h"
#include <AnalyzerHelpers.h>

MultiFlexSimulationDataGenerator::MultiFlexSimulationDataGenerator()
  : mSettings(nullptr),
    mSimulationSampleRateHz(0),
    mSamplesPerHalfClock(0),
    mClkSim(nullptr),
    mSyncSim(nullptr),
    mTx2Sim(nullptr),
    mTx1Sim(nullptr),
    mTx0Sim(nullptr),
    mRx2Sim(nullptr),
    mRx1Sim(nullptr),
    mRx0Sim(nullptr)
{
}

MultiFlexSimulationDataGenerator::~MultiFlexSimulationDataGenerator()
{
}

void MultiFlexSimulationDataGenerator::Initialize(U32 simulation_sample_rate, MultiFlexAnalyzerSettings* settings)
{
  mSimulationSampleRateHz = simulation_sample_rate;
  mSettings = settings;

  // simulate at 1 MHz clock
  mSamplesPerHalfClock = simulation_sample_rate / (2 * 1000000);
  if (mSamplesPerHalfClock < 1) {
    mSamplesPerHalfClock = 1;
  }

  mClkSim  = mSimChannels.Add(mSettings->mClkChannel, simulation_sample_rate, BIT_LOW);
  mTx0Sim  = mSimChannels.Add(mSettings->mTx0Channel, simulation_sample_rate, BIT_LOW);
  mSyncSim = (mSettings->mSyncChannel != UNDEFINED_CHANNEL) ? mSimChannels.Add(mSettings->mSyncChannel, simulation_sample_rate, BIT_LOW) : nullptr;
  mTx1Sim  = (mSettings->mTx1Channel  != UNDEFINED_CHANNEL) ? mSimChannels.Add(mSettings->mTx1Channel,  simulation_sample_rate, BIT_LOW) : nullptr;
  mTx2Sim  = (mSettings->mTx2Channel  != UNDEFINED_CHANNEL) ? mSimChannels.Add(mSettings->mTx2Channel,  simulation_sample_rate, BIT_LOW) : nullptr;
  mRx0Sim  = (mSettings->mRx0Channel  != UNDEFINED_CHANNEL) ? mSimChannels.Add(mSettings->mRx0Channel,  simulation_sample_rate, BIT_LOW) : nullptr;
  mRx1Sim  = (mSettings->mRx1Channel  != UNDEFINED_CHANNEL) ? mSimChannels.Add(mSettings->mRx1Channel,  simulation_sample_rate, BIT_LOW) : nullptr;
  mRx2Sim  = (mSettings->mRx2Channel  != UNDEFINED_CHANNEL) ? mSimChannels.Add(mSettings->mRx2Channel,  simulation_sample_rate, BIT_LOW) : nullptr;
}

U32 MultiFlexSimulationDataGenerator::GenerateSimulationData(U64 largest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channels)
{
  U64 adjusted = AnalyzerHelpers::AdjustSimulationTargetSample(largest_sample_requested, sample_rate, mSimulationSampleRateHz);

  // example transaction matching the timing diagram:
  //   TX words: 5(101), 5(101), 2(010), 6(110)
  //   RX words: 3(011), 4(100), 2(010), 5(101)
  const U8 tx_words[] = { 0x5, 0x5, 0x2, 0x6 };
  const U8 rx_words[] = { 0x3, 0x4, 0x2, 0x5 };

  while (mClkSim->GetCurrentSampleNumber() < adjusted) {
    // idle gap between transactions
    AdvanceAll(mSamplesPerHalfClock * 20);
    CreateTransaction(tx_words, rx_words, 4);
  }

  *simulation_channels = mSimChannels.GetArray();
  return mSimChannels.GetCount();
}

void MultiFlexSimulationDataGenerator::CreateTransaction(const U8* tx_words, const U8* rx_words, U32 num_words)
{
  if (mSyncSim != nullptr) { mSyncSim->TransitionIfNeeded(BIT_HIGH); }

  for (U32 i = 0; i < num_words; i++) {
    if (mTx2Sim != nullptr) { mTx2Sim->TransitionIfNeeded((tx_words[i] & 4) ? BIT_HIGH : BIT_LOW); }
    if (mTx1Sim != nullptr) { mTx1Sim->TransitionIfNeeded((tx_words[i] & 2) ? BIT_HIGH : BIT_LOW); }
    mTx0Sim->TransitionIfNeeded((tx_words[i] & 1) ? BIT_HIGH : BIT_LOW);
    if (mRx2Sim != nullptr) { mRx2Sim->TransitionIfNeeded((rx_words[i] & 4) ? BIT_HIGH : BIT_LOW); }
    if (mRx1Sim != nullptr) { mRx1Sim->TransitionIfNeeded((rx_words[i] & 2) ? BIT_HIGH : BIT_LOW); }
    if (mRx0Sim != nullptr) { mRx0Sim->TransitionIfNeeded((rx_words[i] & 1) ? BIT_HIGH : BIT_LOW); }

    AdvanceAll(mSamplesPerHalfClock);

    mClkSim->TransitionIfNeeded(BIT_HIGH); // rising edge -- sample point

    AdvanceAll(mSamplesPerHalfClock);

    mClkSim->TransitionIfNeeded(BIT_LOW); // falling edge
  }

  if (mSyncSim != nullptr) { mSyncSim->TransitionIfNeeded(BIT_LOW); }
  if (mTx2Sim  != nullptr) { mTx2Sim->TransitionIfNeeded(BIT_LOW); }
  if (mTx1Sim  != nullptr) { mTx1Sim->TransitionIfNeeded(BIT_LOW); }
  mTx0Sim->TransitionIfNeeded(BIT_LOW);
  if (mRx2Sim  != nullptr) { mRx2Sim->TransitionIfNeeded(BIT_LOW); }
  if (mRx1Sim  != nullptr) { mRx1Sim->TransitionIfNeeded(BIT_LOW); }
  if (mRx0Sim  != nullptr) { mRx0Sim->TransitionIfNeeded(BIT_LOW); }

  AdvanceAll(mSamplesPerHalfClock);
}

void MultiFlexSimulationDataGenerator::AdvanceAll(U32 samples)
{
  mClkSim->Advance(samples);
  mTx0Sim->Advance(samples);
  if (mSyncSim != nullptr) { mSyncSim->Advance(samples); }
  if (mTx1Sim  != nullptr) { mTx1Sim->Advance(samples); }
  if (mTx2Sim  != nullptr) { mTx2Sim->Advance(samples); }
  if (mRx0Sim  != nullptr) { mRx0Sim->Advance(samples); }
  if (mRx1Sim  != nullptr) { mRx1Sim->Advance(samples); }
  if (mRx2Sim  != nullptr) { mRx2Sim->Advance(samples); }
}
