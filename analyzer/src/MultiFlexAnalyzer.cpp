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

  for (;;) {
    // advance CLK to the next rising edge
    if (mClk->GetBitState() == BIT_HIGH) {
      mClk->AdvanceToNextEdge(); // falling
    }
    mClk->AdvanceToNextEdge(); // rising

    U64 rising_sample = mClk->GetSampleNumber();

    // if SYNC is configured, skip clock edges where it is not asserted
    if (mSync != nullptr) {
      mSync->AdvanceToAbsPosition(rising_sample);
      if (mSync->GetBitState() == BIT_LOW) {
        continue;
      }
    }

    // sample TX -- missing pins contribute 0
    mTx0->AdvanceToAbsPosition(rising_sample);
    if (mTx1 != nullptr) { mTx1->AdvanceToAbsPosition(rising_sample); }
    if (mTx2 != nullptr) { mTx2->AdvanceToAbsPosition(rising_sample); }

    U8 tx = ((mTx2 != nullptr && mTx2->GetBitState() == BIT_HIGH) ? 4 : 0) |
            ((mTx1 != nullptr && mTx1->GetBitState() == BIT_HIGH) ? 2 : 0) |
            ((mTx0->GetBitState() == BIT_HIGH) ? 1 : 0);

    // sample RX -- only if at least one RX pin is configured
    U8 rx = 0;
    bool has_rx = (mRx0 != nullptr || mRx1 != nullptr || mRx2 != nullptr);
    if (has_rx) {
      if (mRx0 != nullptr) { mRx0->AdvanceToAbsPosition(rising_sample); }
      if (mRx1 != nullptr) { mRx1->AdvanceToAbsPosition(rising_sample); }
      if (mRx2 != nullptr) { mRx2->AdvanceToAbsPosition(rising_sample); }

      rx = ((mRx2 != nullptr && mRx2->GetBitState() == BIT_HIGH) ? 4 : 0) |
           ((mRx1 != nullptr && mRx1->GetBitState() == BIT_HIGH) ? 2 : 0) |
           ((mRx0 != nullptr && mRx0->GetBitState() == BIT_HIGH) ? 1 : 0);
    }

    // mark sample point on each active data line
    mResults->AddMarker(rising_sample, AnalyzerResults::Dot, mSettings.mTx0Channel);
    if (mTx1 != nullptr) { mResults->AddMarker(rising_sample, AnalyzerResults::Dot, mSettings.mTx1Channel); }
    if (mTx2 != nullptr) { mResults->AddMarker(rising_sample, AnalyzerResults::Dot, mSettings.mTx2Channel); }
    if (mRx0 != nullptr) { mResults->AddMarker(rising_sample, AnalyzerResults::Dot, mSettings.mRx0Channel); }
    if (mRx1 != nullptr) { mResults->AddMarker(rising_sample, AnalyzerResults::Dot, mSettings.mRx1Channel); }
    if (mRx2 != nullptr) { mResults->AddMarker(rising_sample, AnalyzerResults::Dot, mSettings.mRx2Channel); }

    // span frame to CLK falling edge so bubbles are visible
    mClk->AdvanceToNextEdge(); // falling
    U64 falling_sample = mClk->GetSampleNumber();

    Frame frame;
    frame.mData1 = tx;
    frame.mData2 = rx;
    frame.mType  = 0;
    frame.mFlags = has_rx ? 1 : 0;
    frame.mStartingSampleInclusive = rising_sample;
    frame.mEndingSampleInclusive   = falling_sample;

    mResults->AddFrame(frame);
    mResults->CommitResults();
    ReportProgress(frame.mEndingSampleInclusive);
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
