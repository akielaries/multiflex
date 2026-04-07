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

  static const U8 tx_bytes[] = { 0xDE, 0xAD, 0xBE, 0xEF };

  while (mClkSim->GetCurrentSampleNumber() < adjusted) {
    AdvanceAll(mSamplesPerHalfClock * 20);
    CreateTransaction(tx_bytes, 4);
  }

  *simulation_channels = mSimChannels.GetArray();
  return mSimChannels.GetCount();
}

void MultiFlexSimulationDataGenerator::CreateTransaction(const U8* bytes, U32 num_bytes)
{
  int num_lanes = 1;
  if (mTx1Sim != nullptr) num_lanes++;
  if (mTx2Sim != nullptr) num_lanes++;

  // LOAD: one rising edge with SYNC=0 so the decoder resets its accumulator
  if (mSyncSim != nullptr) { mSyncSim->TransitionIfNeeded(BIT_LOW); }
  if (mTx2Sim  != nullptr) { mTx2Sim->TransitionIfNeeded(BIT_LOW); }
  if (mTx1Sim  != nullptr) { mTx1Sim->TransitionIfNeeded(BIT_LOW); }
  mTx0Sim->TransitionIfNeeded(BIT_LOW);

  AdvanceAll(mSamplesPerHalfClock);
  mClkSim->TransitionIfNeeded(BIT_HIGH); // LOAD rising -- SYNC=0, decoder resets
  AdvanceAll(mSamplesPerHalfClock);
  mClkSim->TransitionIfNeeded(BIT_LOW);  // falling before first data symbol

  // data: drive each symbol MSB-first across active lanes, SYNC=1
  for (U32 i = 0; i < num_bytes; i++) {
    int bits_sent = 0;
    while (bits_sent < 8) {
      int bits_remaining = 8 - bits_sent;
      int bits_this_sym = (bits_remaining < num_lanes) ? bits_remaining : num_lanes;

      if (mSyncSim != nullptr) { mSyncSim->TransitionIfNeeded(BIT_HIGH); }
      for (int lane = 0; lane < num_lanes; lane++) {
        // lane (num_lanes-1) carries MSB of symbol; lower lanes carry less significant bits
        int offset = num_lanes - 1 - lane;
        BitState b = BIT_LOW;
        if (offset < bits_this_sym) {
          b = ((bytes[i] >> (7 - bits_sent - offset)) & 1) ? BIT_HIGH : BIT_LOW;
        }
        if      (lane == 0)                    { mTx0Sim->TransitionIfNeeded(b); }
        else if (lane == 1 && mTx1Sim != nullptr) { mTx1Sim->TransitionIfNeeded(b); }
        else if (lane == 2 && mTx2Sim != nullptr) { mTx2Sim->TransitionIfNeeded(b); }
      }

      AdvanceAll(mSamplesPerHalfClock);
      mClkSim->TransitionIfNeeded(BIT_HIGH); // data rising -- decoder samples here
      AdvanceAll(mSamplesPerHalfClock);
      mClkSim->TransitionIfNeeded(BIT_LOW);  // falling (used as frame end for last bit)

      bits_sent += bits_this_sym;
    }
  }

  // end of burst: deassert SYNC and TX
  if (mSyncSim != nullptr) { mSyncSim->TransitionIfNeeded(BIT_LOW); }
  if (mTx2Sim  != nullptr) { mTx2Sim->TransitionIfNeeded(BIT_LOW); }
  if (mTx1Sim  != nullptr) { mTx1Sim->TransitionIfNeeded(BIT_LOW); }
  mTx0Sim->TransitionIfNeeded(BIT_LOW);
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
