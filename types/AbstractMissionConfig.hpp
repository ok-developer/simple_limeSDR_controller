#pragma once

#define UNLIMITED 0

class QStringList;

struct AbstractMissionConfig
{
    ~AbstractMissionConfig() = default;

    virtual bool valid() const
    {
        return sampleRate not_eq 0
           and frequency not_eq 0;
    };
    virtual bool parse(const QStringList& args) = 0;

protected:
    static unsigned short argc() { return 7; }

public:
    unsigned short antenaNumber = 1;
    unsigned short gain = 0;
    unsigned long long sampleRate = 0;
    unsigned long long frequency = 0;
    unsigned short deviceNumber = 0;
    unsigned short channelNumber = 0;
    unsigned tryCount = UNLIMITED;
};
