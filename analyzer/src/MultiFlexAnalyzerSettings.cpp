#include "MultiFlexAnalyzerSettings.h"
#include <AnalyzerHelpers.h>

MultiFlexAnalyzerSettings::MultiFlexAnalyzerSettings()
  : mClkChannel(UNDEFINED_CHANNEL),
    mSyncChannel(UNDEFINED_CHANNEL),
    mTx2Channel(UNDEFINED_CHANNEL),
    mTx1Channel(UNDEFINED_CHANNEL),
    mTx0Channel(UNDEFINED_CHANNEL),
    mRx2Channel(UNDEFINED_CHANNEL),
    mRx1Channel(UNDEFINED_CHANNEL),
    mRx0Channel(UNDEFINED_CHANNEL)
{
  mClkInterface.SetTitleAndTooltip("CLK", "Clock signal -- data sampled on rising edge");
  mClkInterface.SetChannel(mClkChannel);

  mSyncInterface.SetTitleAndTooltip("SYNC", "Frame sync -- data valid while high");
  mSyncInterface.SetChannel(mSyncChannel);
  mSyncInterface.SetSelectionOfNoneIsAllowed(true);

  mTx2Interface.SetTitleAndTooltip("TX2", "Transmit bit 2 (MSB)");
  mTx2Interface.SetChannel(mTx2Channel);
  mTx2Interface.SetSelectionOfNoneIsAllowed(true);

  mTx1Interface.SetTitleAndTooltip("TX1", "Transmit bit 1");
  mTx1Interface.SetChannel(mTx1Channel);
  mTx1Interface.SetSelectionOfNoneIsAllowed(true);

  mTx0Interface.SetTitleAndTooltip("TX0", "Transmit bit 0 (LSB)");
  mTx0Interface.SetChannel(mTx0Channel);

  mRx2Interface.SetTitleAndTooltip("RX2", "Receive bit 2 (MSB)");
  mRx2Interface.SetChannel(mRx2Channel);
  mRx2Interface.SetSelectionOfNoneIsAllowed(true);

  mRx1Interface.SetTitleAndTooltip("RX1", "Receive bit 1");
  mRx1Interface.SetChannel(mRx1Channel);
  mRx1Interface.SetSelectionOfNoneIsAllowed(true);

  mRx0Interface.SetTitleAndTooltip("RX0", "Receive bit 0 (LSB)");
  mRx0Interface.SetChannel(mRx0Channel);
  mRx0Interface.SetSelectionOfNoneIsAllowed(true);

  AddInterface(&mClkInterface);
  AddInterface(&mSyncInterface);
  AddInterface(&mTx2Interface);
  AddInterface(&mTx1Interface);
  AddInterface(&mTx0Interface);
  AddInterface(&mRx2Interface);
  AddInterface(&mRx1Interface);
  AddInterface(&mRx0Interface);

  AddExportOption(0, "Export as text/csv file");
  AddExportExtension(0, "text", "txt");
  AddExportExtension(0, "csv", "csv");

  ClearChannels();
  AddChannel(mClkChannel,  "CLK",  false);
  AddChannel(mSyncChannel, "SYNC", false);
  AddChannel(mTx2Channel,  "TX2",  false);
  AddChannel(mTx1Channel,  "TX1",  false);
  AddChannel(mTx0Channel,  "TX0",  false);
  AddChannel(mRx2Channel,  "RX2",  false);
  AddChannel(mRx1Channel,  "RX1",  false);
  AddChannel(mRx0Channel,  "RX0",  false);
}

MultiFlexAnalyzerSettings::~MultiFlexAnalyzerSettings()
{
}

bool MultiFlexAnalyzerSettings::SetSettingsFromInterfaces()
{
  mClkChannel  = mClkInterface.GetChannel();
  mSyncChannel = mSyncInterface.GetChannel();
  mTx2Channel  = mTx2Interface.GetChannel();
  mTx1Channel  = mTx1Interface.GetChannel();
  mTx0Channel  = mTx0Interface.GetChannel();
  mRx2Channel  = mRx2Interface.GetChannel();
  mRx1Channel  = mRx1Interface.GetChannel();
  mRx0Channel  = mRx0Interface.GetChannel();

  ClearChannels();
  AddChannel(mClkChannel,  "CLK",  true);
  AddChannel(mSyncChannel, "SYNC", mSyncChannel != UNDEFINED_CHANNEL);
  AddChannel(mTx2Channel,  "TX2",  mTx2Channel  != UNDEFINED_CHANNEL);
  AddChannel(mTx1Channel,  "TX1",  mTx1Channel  != UNDEFINED_CHANNEL);
  AddChannel(mTx0Channel,  "TX0",  true);
  AddChannel(mRx2Channel,  "RX2",  mRx2Channel  != UNDEFINED_CHANNEL);
  AddChannel(mRx1Channel,  "RX1",  mRx1Channel  != UNDEFINED_CHANNEL);
  AddChannel(mRx0Channel,  "RX0",  mRx0Channel  != UNDEFINED_CHANNEL);

  return true;
}

void MultiFlexAnalyzerSettings::UpdateInterfacesFromSettings()
{
  mClkInterface.SetChannel(mClkChannel);
  mSyncInterface.SetChannel(mSyncChannel);
  mTx2Interface.SetChannel(mTx2Channel);
  mTx1Interface.SetChannel(mTx1Channel);
  mTx0Interface.SetChannel(mTx0Channel);
  mRx2Interface.SetChannel(mRx2Channel);
  mRx1Interface.SetChannel(mRx1Channel);
  mRx0Interface.SetChannel(mRx0Channel);
}

void MultiFlexAnalyzerSettings::LoadSettings(const char* settings)
{
  SimpleArchive text_archive;
  text_archive.SetString(settings);

  text_archive >> mClkChannel;
  text_archive >> mSyncChannel;
  text_archive >> mTx2Channel;
  text_archive >> mTx1Channel;
  text_archive >> mTx0Channel;
  text_archive >> mRx2Channel;
  text_archive >> mRx1Channel;
  text_archive >> mRx0Channel;

  ClearChannels();
  AddChannel(mClkChannel,  "CLK",  true);
  AddChannel(mSyncChannel, "SYNC", mSyncChannel != UNDEFINED_CHANNEL);
  AddChannel(mTx2Channel,  "TX2",  mTx2Channel  != UNDEFINED_CHANNEL);
  AddChannel(mTx1Channel,  "TX1",  mTx1Channel  != UNDEFINED_CHANNEL);
  AddChannel(mTx0Channel,  "TX0",  true);
  AddChannel(mRx2Channel,  "RX2",  mRx2Channel  != UNDEFINED_CHANNEL);
  AddChannel(mRx1Channel,  "RX1",  mRx1Channel  != UNDEFINED_CHANNEL);
  AddChannel(mRx0Channel,  "RX0",  mRx0Channel  != UNDEFINED_CHANNEL);

  UpdateInterfacesFromSettings();
}

const char* MultiFlexAnalyzerSettings::SaveSettings()
{
  SimpleArchive text_archive;

  text_archive << mClkChannel;
  text_archive << mSyncChannel;
  text_archive << mTx2Channel;
  text_archive << mTx1Channel;
  text_archive << mTx0Channel;
  text_archive << mRx2Channel;
  text_archive << mRx1Channel;
  text_archive << mRx0Channel;

  return SetReturnString(text_archive.GetString());
}
