#include <QCommandLineParser>

#include "hardware/LimeSDRDevice.hpp"
#include "types/RxMissionConfig.hpp"
#include "types/TxMissionConfig.hpp"
#include "Application.hpp"

inline const char* StartRxMissionSlot   = "startRxMission";
inline const char* StartTxMissionSlot   = "startTxMission";

Application::Application(int& argc, char** argv, int flags)
    : QCoreApplication(argc, argv, flags)
{
    qRegisterMetaType<RxMissionConfig>("RxMissionConfig");
    qRegisterMetaType<TxMissionConfig>("TxMissionConfig");

    QMetaObject::invokeMethod(this, &Application::onEventLoopInitialization, Qt::QueuedConnection);
}

Application::~Application()
{
    for (auto device : qAsConst(mDevices))
    {
        if (device) delete device;
    }
    mDevices.clear();
}

void Application::onEventLoopInitialization()
{
    if (not processCommandLineArguments())
    {
        exit(CmdArgumentsError);
        return;
    }

    mDevices = LimeSDRDevice::availableDevicesList();

    if (mDevices.isEmpty())
    {
        qWarning("[Application] No valid devices available! Exiting...");
        exit(DevicesInitError);
        return;
    }
}

void Application::startRxMission(const RxMissionConfig& config)
{
    if (config.deviceNumber >= mDevices.count())
    {
        qWarning("[Application] No such device number!");
        exit(MissionError);
        return;
    }

    auto device = mDevices.at(config.deviceNumber);

    if (mConsoleUseCase)
    {
        connect(device, &LimeSDRDevice::rxFinished,
                this,   [&]() { exit(NormalExit); },
                Qt::QueuedConnection);
    }

    if (not device->startRxMission(config))
    {
        if (mConsoleUseCase)
        {
            exit(MissionError);
            return;
        }
    }
}

void Application::startTxMission(const TxMissionConfig& config)
{
    if (config.deviceNumber >= mDevices.count())
    {
        qWarning("[Application] No such device number!");
        exit(MissionError);
        return;
    }

    auto device = mDevices.at(config.deviceNumber);

    if (mConsoleUseCase)
    {
        connect(device, &LimeSDRDevice::txFinished,
                this,   [&]() { exit(NormalExit); },
                Qt::QueuedConnection);
    }

    if (not device->startTxMission(config))
    {
        if (mConsoleUseCase)
        {
            exit(MissionError);
            return;
        }
    }
}

bool Application::processCommandLineArguments()
{
    QCommandLineParser argsParser;
    QCommandLineOption useAsServer("s", "Work as server in client-server use-case.");
    QCommandLineOption rxMission("rx", "RX mission exec.");
    QCommandLineOption txMission("tx", "TX mission exec.");

    argsParser.addHelpOption();
    argsParser.addOption(useAsServer);
    argsParser.addOption(rxMission);
    argsParser.addOption(txMission);
    argsParser.process(arguments());

    if (argsParser.isSet(useAsServer))
    {
        qInfo("This functionary not supported yet!");
        mConsoleUseCase = false;
        return false;
    }
    else if (argsParser.isSet(rxMission))
    {
        const auto args = argsParser.positionalArguments();
        if (RxMissionConfig::argc() not_eq args.count())
        {
            qWarning("Invalid rx mission args count! Example: --rx %s",
                     RxMissionConfig::argsExample());
            return false;
        }

        RxMissionConfig config;
        if (not config.parse(args))
        {
            qWarning("Invalid rx mission config!");
            return false;
        }

        QMetaObject::invokeMethod(this, StartRxMissionSlot, Qt::QueuedConnection,
                                  Q_ARG(RxMissionConfig, config));
        return true;
    }
    else if (argsParser.isSet(txMission))
    {
        const auto args = argsParser.positionalArguments();
        if (TxMissionConfig::argc() not_eq args.count())
        {
            qWarning("Invalid tx mission args count! Example: --tx %s",
                     TxMissionConfig::argsExample());
            return false;
        }

        TxMissionConfig config;
        if (not config.parse(args))
        {
            qWarning("Invalid tx mission config!");
            return false;
        }

        QMetaObject::invokeMethod(this, StartTxMissionSlot, Qt::QueuedConnection,
                                  Q_ARG(TxMissionConfig, config));
        return true;
    }

    printf("%s", qPrintable(argsParser.helpText()));
    return false;
}
