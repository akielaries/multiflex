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

  // mType: 0 = A->B, 1 = B->A
  const char* prefix = (frame.mType == 0) ? "A:" : "B:";

  char val_str[64];
  AnalyzerHelpers::GetNumberString(frame.mData1, display_base, 8, val_str, sizeof(val_str));

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

  file_stream << "Time [s],Direction,Value" << std::endl;

  U64 num_frames = GetNumFrames();
  for (U64 i = 0; i < num_frames; i++) {
    Frame frame = GetFrame(i);

    char time_str[128];
    AnalyzerHelpers::GetTimeString(frame.mStartingSampleInclusive, trigger_sample, sample_rate, time_str, sizeof(time_str));

    char val_str[64];
    AnalyzerHelpers::GetNumberString(frame.mData1, display_base, 8, val_str, sizeof(val_str));

    const char* dir = (frame.mType == 0) ? "A->B" : "B->A";
    file_stream << time_str << "," << dir << "," << val_str << std::endl;

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

  char val_str[64];
  AnalyzerHelpers::GetNumberString(frame.mData1, display_base, 8, val_str, sizeof(val_str));

  const char* dir = (frame.mType == 0) ? "A->B" : "B->A";
  char buf[128];
  std::snprintf(buf, sizeof(buf), "%s %s", dir, val_str);
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
