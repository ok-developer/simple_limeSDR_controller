#include <QStringList>

#include "RxMissionConfig.hpp"

unsigned short RxMissionConfig::argc()
{
    return AbstractMissionConfig::argc() + 1;
}

const char* RxMissionConfig::argsExample()
{
    return "<device_number> <rx_channel_number> <antena_number> "
           "<try_count_or_0> <samples_count> <sample_rate> <frequency> <bandwidth> <gain>";
}

bool RxMissionConfig::valid() const
{
    return samplesCount not_eq 0
       and AbstractMissionConfig::valid();
}

bool RxMissionConfig::parse(const QStringList& args)
{
    deviceNumber = args.at(0).toUShort();
    channelNumber = args.at(1).toUShort();
    antenaNumber = args.at(2).toUShort();
    tryCount = args.at(3).toUInt();
    samplesCount = args.at(4).toUInt();
    sampleRate = args.at(5).toDouble();
    frequency = args.at(6).toDouble();
    bandwidth = args.at(7).toDouble();
    gain = args.at(8).toUShort();

    return valid();
}
