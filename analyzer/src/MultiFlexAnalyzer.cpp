#include "MultiFlexAnalyzer.h"
#include "MultiFlexAnalyzerSettings.h"
#include <AnalyzerChannelData.h>

MultiFlexAnalyzer::MultiFlexAnalyzer()
  : Analyzer2(),
    mSettings(),
    mClk(nullptr),
    mSync(nullptr),
    mTx2(nullptr),
    mTx1(nullptr),
    mTx0(nullptr),
    mRx2(nullptr),
    mRx1(nullptr),
    mRx0(nullptr),
    mSimulationInitilized(false),
    mSampleRateHz(0)
{
  SetAnalyzerSettings(&mSettings);
}

MultiFlexAnalyzer::~MultiFlexAnalyzer()
{
  KillThread();
}

void MultiFlexAnalyzer::SetupResults()
{
  mResults.reset(new MultiFlexAnalyzerResults(this, &mSettings));
  SetAnalyzerResults(mResults.get());

  // bubbles appear on all active data channels, not CLK/SYNC
  mResults->AddChannelBubblesWillAppearOn(mSettings.mTx0Channel);
  if (mSettings.mTx1Channel != UNDEFINED_CHANNEL) {
    mResults->AddChannelBubblesWillAppearOn(mSettings.mTx1Channel);
  }
  if (mSettings.mTx2Channel != UNDEFINED_CHANNEL) {
    mResults->AddChannelBubblesWillAppearOn(mSettings.mTx2Channel);
  }
  if (mSettings.mRx0Channel != UNDEFINED_CHANNEL) {
    mResults->AddChannelBubblesWillAppearOn(mSettings.mRx0Channel);
  }
  if (mSettings.mRx1Channel != UNDEFINED_CHANNEL) {
    mResults->AddChannelBubblesWillAppearOn(mSettings.mRx1Channel);
  }
  if (mSettings.mRx2Channel != UNDEFINED_CHANNEL) {
    mResults->AddChannelBubblesWillAppearOn(mSettings.mRx2Channel);
  }
}

void MultiFlexAnalyzer::WorkerThread()
{
  mSampleRateHz = GetSampleRate();

  mClk  = GetAnalyzerChannelData(mSettings.mClkChannel);
  mTx0  = GetAnalyzerChannelData(mSettings.mTx0Channel);
  mSync = (mSettings.mSyncChannel != UNDEFINED_CHANNEL) ? GetAnalyzerChannelData(mSettings.mSyncChannel) : nullptr;
  mTx1  = (mSettings.mTx1Channel  != UNDEFINED_CHANNEL) ? GetAnalyzerChannelData(mSettings.mTx1Channel)  : nullptr;
  mTx2  = (mSettings.mTx2Channel  != UNDEFINED_CHANNEL) ? GetAnalyzerChannelData(mSettings.mTx2Channel)  : nullptr;
  mRx0  = (mSettings.mRx0Channel  != UNDEFINED_CHANNEL) ? GetAnalyzerChannelData(mSettings.mRx0Channel)  : nullptr;
  mRx1  = (mSettings.mRx1Channel  != UNDEFINED_CHANNEL) ? GetAnalyzerChannelData(mSettings.mRx1Channel)  : nullptr;
  mRx2  = (mSettings.mRx2Channel  != UNDEFINED_CHANNEL) ? GetAnalyzerChannelData(mSettings.mRx2Channel)  : nullptr;

  // active TX lane count determines bits per symbol
  int num_lanes = 1;
  if (mTx1 != nullptr) num_lanes++;
  if (mTx2 != nullptr) num_lanes++;

  for (;;) {
    U8 byte_val = 0;
    int bits_collected = 0;
    U64 byte_start = 0;
    U64 byte_end = 0;

    // accumulate symbols until we have 8 bits (one byte)
    while (bits_collected < 8) {
      // advance CLK to next rising edge
      if (mClk->GetBitState() == BIT_HIGH)
        mClk->AdvanceToNextEdge(); // to falling
      mClk->AdvanceToNextEdge(); // to rising

      U64 rising = mClk->GetSampleNumber();

      // if sync is deasserted this is an inter-byte gap; reset accumulator
      if (mSync != nullptr) {
        mSync->AdvanceToAbsPosition(rising);
        if (mSync->GetBitState() == BIT_LOW) {
          // mark the reset point so it is visible on the CLK channel
          mResults->AddMarker(rising, AnalyzerResults::X, mSettings.mClkChannel);
          byte_val = 0;
          bits_collected = 0;
          byte_start = 0;
          mClk->AdvanceToNextEdge(); // to falling, keep state consistent
          continue;
        }
      }

      if (bits_collected == 0)
        byte_start = rising;

      // sample TX lanes; higher-indexed lane carries the more significant bit
      mTx0->AdvanceToAbsPosition(rising);
      if (mTx1 != nullptr) mTx1->AdvanceToAbsPosition(rising);
      if (mTx2 != nullptr) mTx2->AdvanceToAbsPosition(rising);

      U8 sym = (mTx0->GetBitState() == BIT_HIGH ? 1 : 0)
             | (mTx1 != nullptr && mTx1->GetBitState() == BIT_HIGH ? 2 : 0)
             | (mTx2 != nullptr && mTx2->GetBitState() == BIT_HIGH ? 4 : 0);

      mResults->AddMarker(rising, AnalyzerResults::Dot, mSettings.mTx0Channel);
      if (mTx1 != nullptr) mResults->AddMarker(rising, AnalyzerResults::Dot, mSettings.mTx1Channel);
      if (mTx2 != nullptr) mResults->AddMarker(rising, AnalyzerResults::Dot, mSettings.mTx2Channel);

      // upper `bits_this_sym` lanes carry data MSB-first (lower lanes are padding
      // in the last partial symbol when 8 % num_lanes != 0)
      int bits_remaining = 8 - bits_collected;
      int bits_this_sym = (bits_remaining < num_lanes) ? bits_remaining : num_lanes;
      for (int i = 0; i < bits_this_sym; i++) {
        int lane = num_lanes - 1 - i; // descending: highest lane = MSB
        byte_val = (byte_val << 1) | ((sym >> lane) & 1);
      }
      bits_collected += bits_this_sym;

      // advance to falling edge to bound the last symbol's frame end
      mClk->AdvanceToNextEdge();
      byte_end = mClk->GetSampleNumber();
    }

    Frame frame;
    frame.mData1 = byte_val;
    frame.mData2 = 0;
    frame.mType  = 0;
    frame.mFlags = 0;
    frame.mStartingSampleInclusive = byte_start;
    frame.mEndingSampleInclusive   = byte_end;
    mResults->AddFrame(frame);
    mResults->CommitResults();
    ReportProgress(byte_end);
  }
}

bool MultiFlexAnalyzer::NeedsRerun()
{
  return false;
}

U32 MultiFlexAnalyzer::GenerateSimulationData(U64 minimum_sample_index, U32 device_sample_rate, SimulationChannelDescriptor** simulation_channels)
{
  if (mSimulationInitilized == false) {
    mSimulationDataGenerator.Initialize(GetSimulationSampleRate(), &mSettings);
    mSimulationInitilized = true;
  }

  return mSimulationDataGenerator.GenerateSimulationData(minimum_sample_index, device_sample_rate, simulation_channels);
}

U32 MultiFlexAnalyzer::GetMinimumSampleRateHz()
{
  return 4000000;
}

const char* MultiFlexAnalyzer::GetAnalyzerName() const
{
  return "MultiFlex Analyzer";
}

const char* GetAnalyzerName()
{
  return "MultiFlex Analyzer";
}

Analyzer* CreateAnalyzer()
{
  return new MultiFlexAnalyzer();
}

void DestroyAnalyzer(Analyzer* analyzer)
{
  delete analyzer;
}
