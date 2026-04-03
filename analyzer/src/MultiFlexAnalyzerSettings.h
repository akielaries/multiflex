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
	virtual void LoadSettings( const char* settings );
	virtual const char* SaveSettings();

	
	Channel mInputChannel;
	U32 mBitRate;

protected:
	AnalyzerSettingInterfaceChannel	mInputChannelInterface;
	AnalyzerSettingInterfaceInteger	mBitRateInterface;
};

#endif //MULTIFLEX_ANALYZER_SETTINGS
