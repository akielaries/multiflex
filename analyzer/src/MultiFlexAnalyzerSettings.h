#ifndef MULTIFLEX_ANALYZER_SETTINGS
#define MULTIFLEX_ANALYZER_SETTINGS

#include <AnalyzerSettings.h>
#include <AnalyzerTypes.h>

class MultiFlexAnalyzerSettings : public AnalyzerSettings
{
public:
  MultiFlexAnalyzerSettings();
  virtual ~MultiFlexAnalyzerSettings();

  virtual bool SetSettingsFromInterfaces();
  void UpdateInterfacesFromSettings();
  virtual void LoadSettings(const char* settings);
  virtual const char* SaveSettings();

  // side A (transmitting from FPGA): CLK_A, SYNC_A, TX_A[0], TX_A[1]
  Channel mClkAChannel;
  Channel mSyncAChannel;   // optional
  Channel mTxA0Channel;
  Channel mTxA1Channel;    // optional

  // side B (transmitting from remote): CLK_B, SYNC_B, TX_B[0], TX_B[1]
  Channel mClkBChannel;    // optional; enables B-direction decode
  Channel mSyncBChannel;   // optional
  Channel mTxB0Channel;    // optional
  Channel mTxB1Channel;    // optional

protected:
  AnalyzerSettingInterfaceChannel mClkAInterface;
  AnalyzerSettingInterfaceChannel mSyncAInterface;
  AnalyzerSettingInterfaceChannel mTxA0Interface;
  AnalyzerSettingInterfaceChannel mTxA1Interface;
  AnalyzerSettingInterfaceChannel mClkBInterface;
  AnalyzerSettingInterfaceChannel mSyncBInterface;
  AnalyzerSettingInterfaceChannel mTxB0Interface;
  AnalyzerSettingInterfaceChannel mTxB1Interface;
};

#endif // MULTIFLEX_ANALYZER_SETTINGS
