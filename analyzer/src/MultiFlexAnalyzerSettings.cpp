#include "MultiFlexAnalyzerSettings.h"
#include <AnalyzerHelpers.h>

MultiFlexAnalyzerSettings::MultiFlexAnalyzerSettings()
  : mClkAChannel(UNDEFINED_CHANNEL),
    mSyncAChannel(UNDEFINED_CHANNEL),
    mTxA0Channel(UNDEFINED_CHANNEL),
    mTxA1Channel(UNDEFINED_CHANNEL),
    mClkBChannel(UNDEFINED_CHANNEL),
    mSyncBChannel(UNDEFINED_CHANNEL),
    mTxB0Channel(UNDEFINED_CHANNEL),
    mTxB1Channel(UNDEFINED_CHANNEL)
{
  mClkAInterface.SetTitleAndTooltip("CLK_A", "Side A clock -- data sampled on rising edge");
  mClkAInterface.SetChannel(mClkAChannel);

  mSyncAInterface.SetTitleAndTooltip("SYNC_A", "Side A frame sync -- data valid while high");
  mSyncAInterface.SetChannel(mSyncAChannel);
  mSyncAInterface.SetSelectionOfNoneIsAllowed(true);

  mTxA0Interface.SetTitleAndTooltip("TX_A[0]", "Side A lane 0 (LSB)");
  mTxA0Interface.SetChannel(mTxA0Channel);

  mTxA1Interface.SetTitleAndTooltip("TX_A[1]", "Side A lane 1 (MSB when using 2 lanes)");
  mTxA1Interface.SetChannel(mTxA1Channel);
  mTxA1Interface.SetSelectionOfNoneIsAllowed(true);

  mClkBInterface.SetTitleAndTooltip("CLK_B", "Side B clock -- enables B-direction decode when set");
  mClkBInterface.SetChannel(mClkBChannel);
  mClkBInterface.SetSelectionOfNoneIsAllowed(true);

  mSyncBInterface.SetTitleAndTooltip("SYNC_B", "Side B frame sync");
  mSyncBInterface.SetChannel(mSyncBChannel);
  mSyncBInterface.SetSelectionOfNoneIsAllowed(true);

  mTxB0Interface.SetTitleAndTooltip("TX_B[0]", "Side B lane 0 (LSB)");
  mTxB0Interface.SetChannel(mTxB0Channel);
  mTxB0Interface.SetSelectionOfNoneIsAllowed(true);

  mTxB1Interface.SetTitleAndTooltip("TX_B[1]", "Side B lane 1 (MSB when using 2 lanes)");
  mTxB1Interface.SetChannel(mTxB1Channel);
  mTxB1Interface.SetSelectionOfNoneIsAllowed(true);

  AddInterface(&mClkAInterface);
  AddInterface(&mSyncAInterface);
  AddInterface(&mTxA0Interface);
  AddInterface(&mTxA1Interface);
  AddInterface(&mClkBInterface);
  AddInterface(&mSyncBInterface);
  AddInterface(&mTxB0Interface);
  AddInterface(&mTxB1Interface);

  AddExportOption(0, "Export as text/csv file");
  AddExportExtension(0, "text", "txt");
  AddExportExtension(0, "csv", "csv");

  ClearChannels();
  AddChannel(mClkAChannel,  "CLK_A",   false);
  AddChannel(mSyncAChannel, "SYNC_A",  false);
  AddChannel(mTxA0Channel,  "TX_A[0]", false);
  AddChannel(mTxA1Channel,  "TX_A[1]", false);
  AddChannel(mClkBChannel,  "CLK_B",   false);
  AddChannel(mSyncBChannel, "SYNC_B",  false);
  AddChannel(mTxB0Channel,  "TX_B[0]", false);
  AddChannel(mTxB1Channel,  "TX_B[1]", false);
}

MultiFlexAnalyzerSettings::~MultiFlexAnalyzerSettings()
{
}

bool MultiFlexAnalyzerSettings::SetSettingsFromInterfaces()
{
  mClkAChannel  = mClkAInterface.GetChannel();
  mSyncAChannel = mSyncAInterface.GetChannel();
  mTxA0Channel  = mTxA0Interface.GetChannel();
  mTxA1Channel  = mTxA1Interface.GetChannel();
  mClkBChannel  = mClkBInterface.GetChannel();
  mSyncBChannel = mSyncBInterface.GetChannel();
  mTxB0Channel  = mTxB0Interface.GetChannel();
  mTxB1Channel  = mTxB1Interface.GetChannel();

  ClearChannels();
  AddChannel(mClkAChannel,  "CLK_A",   true);
  AddChannel(mSyncAChannel, "SYNC_A",  mSyncAChannel != UNDEFINED_CHANNEL);
  AddChannel(mTxA0Channel,  "TX_A[0]", true);
  AddChannel(mTxA1Channel,  "TX_A[1]", mTxA1Channel  != UNDEFINED_CHANNEL);
  AddChannel(mClkBChannel,  "CLK_B",   mClkBChannel  != UNDEFINED_CHANNEL);
  AddChannel(mSyncBChannel, "SYNC_B",  mSyncBChannel != UNDEFINED_CHANNEL);
  AddChannel(mTxB0Channel,  "TX_B[0]", mTxB0Channel  != UNDEFINED_CHANNEL);
  AddChannel(mTxB1Channel,  "TX_B[1]", mTxB1Channel  != UNDEFINED_CHANNEL);

  return true;
}

void MultiFlexAnalyzerSettings::UpdateInterfacesFromSettings()
{
  mClkAInterface.SetChannel(mClkAChannel);
  mSyncAInterface.SetChannel(mSyncAChannel);
  mTxA0Interface.SetChannel(mTxA0Channel);
  mTxA1Interface.SetChannel(mTxA1Channel);
  mClkBInterface.SetChannel(mClkBChannel);
  mSyncBInterface.SetChannel(mSyncBChannel);
  mTxB0Interface.SetChannel(mTxB0Channel);
  mTxB1Interface.SetChannel(mTxB1Channel);
}

void MultiFlexAnalyzerSettings::LoadSettings(const char* settings)
{
  SimpleArchive text_archive;
  text_archive.SetString(settings);

  text_archive >> mClkAChannel;
  text_archive >> mSyncAChannel;
  text_archive >> mTxA0Channel;
  text_archive >> mTxA1Channel;
  text_archive >> mClkBChannel;
  text_archive >> mSyncBChannel;
  text_archive >> mTxB0Channel;
  text_archive >> mTxB1Channel;

  ClearChannels();
  AddChannel(mClkAChannel,  "CLK_A",   true);
  AddChannel(mSyncAChannel, "SYNC_A",  mSyncAChannel != UNDEFINED_CHANNEL);
  AddChannel(mTxA0Channel,  "TX_A[0]", true);
  AddChannel(mTxA1Channel,  "TX_A[1]", mTxA1Channel  != UNDEFINED_CHANNEL);
  AddChannel(mClkBChannel,  "CLK_B",   mClkBChannel  != UNDEFINED_CHANNEL);
  AddChannel(mSyncBChannel, "SYNC_B",  mSyncBChannel != UNDEFINED_CHANNEL);
  AddChannel(mTxB0Channel,  "TX_B[0]", mTxB0Channel  != UNDEFINED_CHANNEL);
  AddChannel(mTxB1Channel,  "TX_B[1]", mTxB1Channel  != UNDEFINED_CHANNEL);

  UpdateInterfacesFromSettings();
}

const char* MultiFlexAnalyzerSettings::SaveSettings()
{
  SimpleArchive text_archive;

  text_archive << mClkAChannel;
  text_archive << mSyncAChannel;
  text_archive << mTxA0Channel;
  text_archive << mTxA1Channel;
  text_archive << mClkBChannel;
  text_archive << mSyncBChannel;
  text_archive << mTxB0Channel;
  text_archive << mTxB1Channel;

  return SetReturnString(text_archive.GetString());
}
