#include "MultiFlexSimulationDataGenerator.h"
#include "MultiFlexAnalyzerSettings.h"
#include <AnalyzerHelpers.h>

MultiFlexSimulationDataGenerator::MultiFlexSimulationDataGenerator()
  : mSettings(nullptr),
    mSimulationSampleRateHz(0),
    mSamplesPerHalfClock(0),
    mClkASim(nullptr),
    mSyncASim(nullptr),
    mTxA0Sim(nullptr),
    mTxA1Sim(nullptr)
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

  mClkASim  = mSimChannels.Add(mSettings->mClkAChannel,  simulation_sample_rate, BIT_LOW);
  mTxA0Sim  = mSimChannels.Add(mSettings->mTxA0Channel,  simulation_sample_rate, BIT_LOW);
  mSyncASim = (mSettings->mSyncAChannel != UNDEFINED_CHANNEL) ? mSimChannels.Add(mSettings->mSyncAChannel, simulation_sample_rate, BIT_LOW) : nullptr;
  mTxA1Sim  = (mSettings->mTxA1Channel  != UNDEFINED_CHANNEL) ? mSimChannels.Add(mSettings->mTxA1Channel,  simulation_sample_rate, BIT_LOW) : nullptr;
}

U32 MultiFlexSimulationDataGenerator::GenerateSimulationData(U64 largest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channels)
{
  U64 adjusted = AnalyzerHelpers::AdjustSimulationTargetSample(largest_sample_requested, sample_rate, mSimulationSampleRateHz);

  static const U8 tx_bytes[] = { 0xDE, 0xAD, 0xBE, 0xEF };

  while (mClkASim->GetCurrentSampleNumber() < adjusted) {
    AdvanceAll(mSamplesPerHalfClock * 20);
    CreateTransaction(tx_bytes, 4);
  }

  *simulation_channels = mSimChannels.GetArray();
  return mSimChannels.GetCount();
}

void MultiFlexSimulationDataGenerator::CreateTransaction(const U8* bytes, U32 num_bytes)
{
  int num_lanes = 1;
  if (mTxA1Sim != nullptr) num_lanes++;

  // one rising edge with SYNC=0 so the decoder resets its accumulator
  if (mSyncASim != nullptr) { mSyncASim->TransitionIfNeeded(BIT_LOW); }
  if (mTxA1Sim  != nullptr) { mTxA1Sim->TransitionIfNeeded(BIT_LOW); }
  mTxA0Sim->TransitionIfNeeded(BIT_LOW);

  AdvanceAll(mSamplesPerHalfClock);
  mClkASim->TransitionIfNeeded(BIT_HIGH); // SYNC=0 rising -- decoder resets
  AdvanceAll(mSamplesPerHalfClock);
  mClkASim->TransitionIfNeeded(BIT_LOW);

  // data: drive each symbol MSB-first across active lanes, SYNC=1
  for (U32 i = 0; i < num_bytes; i++) {
    int bits_sent = 0;
    while (bits_sent < 8) {
      int bits_remaining = 8 - bits_sent;
      int bits_this_sym = (bits_remaining < num_lanes) ? bits_remaining : num_lanes;

      if (mSyncASim != nullptr) { mSyncASim->TransitionIfNeeded(BIT_HIGH); }
      for (int lane = 0; lane < num_lanes; lane++) {
        int offset = num_lanes - 1 - lane;
        BitState b = BIT_LOW;
        if (offset < bits_this_sym) {
          b = ((bytes[i] >> (7 - bits_sent - offset)) & 1) ? BIT_HIGH : BIT_LOW;
        }
        if      (lane == 0)                     { mTxA0Sim->TransitionIfNeeded(b); }
        else if (lane == 1 && mTxA1Sim != nullptr) { mTxA1Sim->TransitionIfNeeded(b); }
      }

      AdvanceAll(mSamplesPerHalfClock);
      mClkASim->TransitionIfNeeded(BIT_HIGH); // decoder samples here
      AdvanceAll(mSamplesPerHalfClock);
      mClkASim->TransitionIfNeeded(BIT_LOW);

      bits_sent += bits_this_sym;
    }
  }

  // end of burst: deassert SYNC and TX lanes
  if (mSyncASim != nullptr) { mSyncASim->TransitionIfNeeded(BIT_LOW); }
  if (mTxA1Sim  != nullptr) { mTxA1Sim->TransitionIfNeeded(BIT_LOW); }
  mTxA0Sim->TransitionIfNeeded(BIT_LOW);
}

void MultiFlexSimulationDataGenerator::AdvanceAll(U32 samples)
{
  mClkASim->Advance(samples);
  mTxA0Sim->Advance(samples);
  if (mSyncASim != nullptr) { mSyncASim->Advance(samples); }
  if (mTxA1Sim  != nullptr) { mTxA1Sim->Advance(samples); }
}
