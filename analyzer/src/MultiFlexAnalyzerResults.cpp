#include "MultiFlexAnalyzerResults.h"
#include <AnalyzerHelpers.h>
#include "MultiFlexAnalyzer.h"
#include "MultiFlexAnalyzerSettings.h"
#include <fstream>
#include <cstdio>

MultiFlexAnalyzerResults::MultiFlexAnalyzerResults(MultiFlexAnalyzer* analyzer, MultiFlexAnalyzerSettings* settings)
  : AnalyzerResults(),
    mSettings(settings),
    mAnalyzer(analyzer)
{
}

MultiFlexAnalyzerResults::~MultiFlexAnalyzerResults()
{
}

void MultiFlexAnalyzerResults::GenerateBubbleText(U64 frame_index, Channel& channel, DisplayBase display_base)
{
  ClearResultStrings();
  Frame frame = GetFrame(frame_index);

  bool is_tx = (channel == mSettings->mTx0Channel ||
                channel == mSettings->mTx1Channel ||
                channel == mSettings->mTx2Channel);

  U64 word           = is_tx ? frame.mData1 : frame.mData2;
  const char* prefix = is_tx ? "TX:" : "RX:";

  char val_str[64];
  AnalyzerHelpers::GetNumberString(word, display_base, 3, val_str, sizeof(val_str));

  char full_str[128];
  std::snprintf(full_str, sizeof(full_str), "%s%s", prefix, val_str);

  AddResultString(full_str);
  AddResultString(val_str);
}

void MultiFlexAnalyzerResults::GenerateExportFile(const char* file, DisplayBase display_base, U32 export_type_user_id)
{
  std::ofstream file_stream(file, std::ios::out);

  U64 trigger_sample = mAnalyzer->GetTriggerSample();
  U32 sample_rate    = mAnalyzer->GetSampleRate();

  // check whether any frame has RX data to decide CSV columns
  bool has_rx = (GetNumFrames() > 0) && (GetFrame(0).mFlags & 1);
  if (has_rx) {
    file_stream << "Time [s],TX,RX" << std::endl;
  } else {
    file_stream << "Time [s],TX" << std::endl;
  }

  U64 num_frames = GetNumFrames();
  for (U64 i = 0; i < num_frames; i++) {
    Frame frame = GetFrame(i);

    char time_str[128];
    AnalyzerHelpers::GetTimeString(frame.mStartingSampleInclusive, trigger_sample, sample_rate, time_str, sizeof(time_str));

    char tx_str[64];
    AnalyzerHelpers::GetNumberString(frame.mData1, display_base, 3, tx_str, sizeof(tx_str));

    if (has_rx) {
      char rx_str[64];
      AnalyzerHelpers::GetNumberString(frame.mData2, display_base, 3, rx_str, sizeof(rx_str));
      file_stream << time_str << "," << tx_str << "," << rx_str << std::endl;
    } else {
      file_stream << time_str << "," << tx_str << std::endl;
    }

    if (UpdateExportProgressAndCheckForCancel(i, num_frames) == true) {
      file_stream.close();
      return;
    }
  }

  file_stream.close();
}

void MultiFlexAnalyzerResults::GenerateFrameTabularText(U64 frame_index, DisplayBase display_base)
{
#ifdef SUPPORTS_PROTOCOL_SEARCH
  Frame frame = GetFrame(frame_index);
  ClearTabularText();

  char tx_str[64];
  char rx_str[64];
  AnalyzerHelpers::GetNumberString(frame.mData1, display_base, 3, tx_str, sizeof(tx_str));
  AnalyzerHelpers::GetNumberString(frame.mData2, display_base, 3, rx_str, sizeof(rx_str));

  char buf[256];
  std::snprintf(buf, sizeof(buf), "TX:%s RX:%s", tx_str, rx_str);
  AddTabularText(buf);
#endif
}

void MultiFlexAnalyzerResults::GeneratePacketTabularText(U64 packet_id, DisplayBase display_base)
{
  // not used
}

void MultiFlexAnalyzerResults::GenerateTransactionTabularText(U64 transaction_id, DisplayBase display_base)
{
  // not used
}
