#ifndef MULTIFLEX_ANALYZER_RESULTS
#define MULTIFLEX_ANALYZER_RESULTS

#include <AnalyzerResults.h>

class MultiFlexAnalyzer;
class MultiFlexAnalyzerSettings;

class MultiFlexAnalyzerResults : public AnalyzerResults
{
public:
  MultiFlexAnalyzerResults(MultiFlexAnalyzer* analyzer, MultiFlexAnalyzerSettings* settings);
  virtual ~MultiFlexAnalyzerResults();

  virtual void GenerateBubbleText(U64 frame_index, Channel& channel, DisplayBase display_base);
  virtual void GenerateExportFile(const char* file, DisplayBase display_base, U32 export_type_user_id);

  virtual void GenerateFrameTabularText(U64 frame_index, DisplayBase display_base);
  virtual void GeneratePacketTabularText(U64 packet_id, DisplayBase display_base);
  virtual void GenerateTransactionTabularText(U64 transaction_id, DisplayBase display_base);

protected:
  MultiFlexAnalyzerSettings* mSettings;
  MultiFlexAnalyzer* mAnalyzer;
};

#endif // MULTIFLEX_ANALYZER_RESULTS
