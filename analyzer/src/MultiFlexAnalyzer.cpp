#include "MultiFlexAnalyzer.h"
#include "MultiFlexAnalyzerSettings.h"
#include <AnalyzerChannelData.h>

MultiFlexAnalyzer::MultiFlexAnalyzer()
  : Analyzer2(),
    mSettings(),
    mClkA(nullptr),
    mSyncA(nullptr),
    mTxA0(nullptr),
    mTxA1(nullptr),
    mClkB(nullptr),
    mSyncB(nullptr),
    mTxB0(nullptr),
    mTxB1(nullptr),
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

  mResults->AddChannelBubblesWillAppearOn(mSettings.mTxA0Channel);
  if (mSettings.mTxA1Channel != UNDEFINED_CHANNEL) {
    mResults->AddChannelBubblesWillAppearOn(mSettings.mTxA1Channel);
  }
  if (mSettings.mTxB0Channel != UNDEFINED_CHANNEL) {
    mResults->AddChannelBubblesWillAppearOn(mSettings.mTxB0Channel);
  }
  if (mSettings.mTxB1Channel != UNDEFINED_CHANNEL) {
    mResults->AddChannelBubblesWillAppearOn(mSettings.mTxB1Channel);
  }
}

void MultiFlexAnalyzer::WorkerThread()
{
  mSampleRateHz = GetSampleRate();

  mClkA  = GetAnalyzerChannelData(mSettings.mClkAChannel);
  mSyncA = (mSettings.mSyncAChannel != UNDEFINED_CHANNEL) ? GetAnalyzerChannelData(mSettings.mSyncAChannel) : nullptr;
  mTxA0  = GetAnalyzerChannelData(mSettings.mTxA0Channel);
  mTxA1  = (mSettings.mTxA1Channel  != UNDEFINED_CHANNEL) ? GetAnalyzerChannelData(mSettings.mTxA1Channel)  : nullptr;

  mClkB  = (mSettings.mClkBChannel  != UNDEFINED_CHANNEL) ? GetAnalyzerChannelData(mSettings.mClkBChannel)  : nullptr;
  mSyncB = (mSettings.mSyncBChannel != UNDEFINED_CHANNEL) ? GetAnalyzerChannelData(mSettings.mSyncBChannel) : nullptr;
  mTxB0  = (mSettings.mTxB0Channel  != UNDEFINED_CHANNEL) ? GetAnalyzerChannelData(mSettings.mTxB0Channel)  : nullptr;
  mTxB1  = (mSettings.mTxB1Channel  != UNDEFINED_CHANNEL) ? GetAnalyzerChannelData(mSettings.mTxB1Channel)  : nullptr;

  int num_lanes_a = 1 + (mTxA1 != nullptr ? 1 : 0);
  int num_lanes_b = (mTxB0 != nullptr) ? (1 + (mTxB1 != nullptr ? 1 : 0)) : 0;

  // advance a clock to its first rising edge
  auto seek_rising = [](AnalyzerChannelData* clk) {
    if (clk->GetBitState() == BIT_HIGH) clk->AdvanceToNextEdge();
    clk->AdvanceToNextEdge();
  };

  seek_rising(mClkA);
  if (mClkB) seek_rising(mClkB);

  // per-direction byte accumulator state
  U8  val_a = 0,  val_b = 0;
  int bits_a = 0, bits_b = 0;
  U64 start_a = 0, start_b = 0;

  for (;;) {
    U64 pos_a = mClkA->GetSampleNumber();
    U64 pos_b = mClkB ? mClkB->GetSampleNumber() : ~(U64)0;

    // choose whichever direction has the earlier clock edge
    bool do_a = (pos_a <= pos_b);
    U64 rising = do_a ? pos_a : pos_b;

    AnalyzerChannelData* clk   = do_a ? mClkA  : mClkB;
    AnalyzerChannelData* sync  = do_a ? mSyncA : mSyncB;
    AnalyzerChannelData* lane0 = do_a ? mTxA0  : mTxB0;
    AnalyzerChannelData* lane1 = do_a ? mTxA1  : mTxB1;
    int num_lanes  = do_a ? num_lanes_a  : num_lanes_b;
    Channel ch_l0  = do_a ? mSettings.mTxA0Channel : mSettings.mTxB0Channel;
    Channel ch_l1  = do_a ? mSettings.mTxA1Channel : mSettings.mTxB1Channel;
    Channel ch_clk = do_a ? mSettings.mClkAChannel : mSettings.mClkBChannel;
    U8&  val   = do_a ? val_a   : val_b;
    int& bits  = do_a ? bits_a  : bits_b;
    U64& start = do_a ? start_a : start_b;

    // advance to falling edge (bounds this symbol's frame), then to next rising
    clk->AdvanceToNextEdge();
    U64 falling = clk->GetSampleNumber();
    clk->AdvanceToNextEdge();

    // check sync; if deasserted, reset accumulator
    bool sync_active = true;
    if (sync) {
      sync->AdvanceToAbsPosition(rising);
      sync_active = (sync->GetBitState() == BIT_HIGH);
    }

    if (!sync_active) {
      if (bits != 0) {
        mResults->AddMarker(rising, AnalyzerResults::X, ch_clk);
      }
      val = 0; bits = 0; start = 0;
      continue;
    }

    if (bits == 0) start = rising;

    // sample data lanes at the rising edge
    lane0->AdvanceToAbsPosition(rising);
    U8 sym = (lane0->GetBitState() == BIT_HIGH) ? 1 : 0;
    if (lane1) {
      lane1->AdvanceToAbsPosition(rising);
      sym |= (lane1->GetBitState() == BIT_HIGH) ? 2 : 0;
    }

    mResults->AddMarker(rising, AnalyzerResults::Dot, ch_l0);
    if (lane1) mResults->AddMarker(rising, AnalyzerResults::Dot, ch_l1);

    // pack bits MSB-first; highest-indexed active lane carries the MSB
    int bits_remaining = 8 - bits;
    int bits_this_sym  = (bits_remaining < num_lanes) ? bits_remaining : num_lanes;
    for (int i = 0; i < bits_this_sym; i++) {
      int lane = num_lanes - 1 - i;
      val = (U8)((val << 1) | ((sym >> lane) & 1));
    }
    bits += bits_this_sym;

    if (bits >= 8) {
      Frame frame;
      frame.mData1 = val;
      frame.mData2 = 0;
      frame.mType  = do_a ? 0 : 1;  // 0 = A->B, 1 = B->A
      frame.mFlags = 0;
      frame.mStartingSampleInclusive = start;
      frame.mEndingSampleInclusive   = falling;
      mResults->AddFrame(frame);
      mResults->CommitResults();
      ReportProgress(falling);
      val = 0; bits = 0; start = 0;
    }
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
