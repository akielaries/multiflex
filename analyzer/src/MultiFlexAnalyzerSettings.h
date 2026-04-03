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

  Channel mClkChannel;
  Channel mSyncChannel;
  Channel mTx2Channel;
  Channel mTx1Channel;
  Channel mTx0Channel;
  Channel mRx2Channel;
  Channel mRx1Channel;
  Channel mRx0Channel;

protected:
  AnalyzerSettingInterfaceChannel mClkInterface;
  AnalyzerSettingInterfaceChannel mSyncInterface;
  AnalyzerSettingInterfaceChannel mTx2Interface;
  AnalyzerSettingInterfaceChannel mTx1Interface;
  AnalyzerSettingInterfaceChannel mTx0Interface;
  AnalyzerSettingInterfaceChannel mRx2Interface;
  AnalyzerSettingInterfaceChannel mRx1Interface;
  AnalyzerSettingInterfaceChannel mRx0Interface;
};

#endif // MULTIFLEX_ANALYZER_SETTINGS
