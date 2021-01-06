#include <QStringList>

#include "TxMissionConfig.hpp"

unsigned short TxMissionConfig::argc()
{
    return AbstractMissionConfig::argc() + 1;
}

const char* TxMissionConfig::argsExample()
{
    return "<device_number> <rx_channel_number> <antena_number> "
           "<try_count_or_0> <sample_rate> <frequency> <gain> <file_name_in_TX_folder>";
}

bool TxMissionConfig::valid() const
{
    return not fileName.isEmpty()
       and AbstractMissionConfig::valid();
}

bool TxMissionConfig::parse(const QStringList& args)
{
    deviceNumber = args.at(0).toUShort();
    channelNumber = args.at(1).toUShort();
    antenaNumber = args.at(2).toUShort();
    tryCount = args.at(3).toUInt();
    sampleRate = args.at(4).toDouble();
    frequency = args.at(5).toDouble();
    gain = args.at(6).toUShort();
    fileName = args.at(7);

    return valid();
}
