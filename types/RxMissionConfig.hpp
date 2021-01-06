#pragma once

#include "AbstractMissionConfig.hpp"

struct RxMissionConfig : public AbstractMissionConfig
{
    static unsigned short argc();
    static const char* argsExample();

    virtual bool valid() const override;
    virtual bool parse(const QStringList& args) override;

public:
    unsigned samplesCount = 0;
};
