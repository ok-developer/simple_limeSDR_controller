#pragma once

#include <QString>

#include "AbstractMissionConfig.hpp"

struct TxMissionConfig : public AbstractMissionConfig
{
    static unsigned short argc();
    static const char* argsExample();

    virtual bool valid() const override;
    virtual bool parse(const QStringList& args) override;

public:
    QString fileName;

};
