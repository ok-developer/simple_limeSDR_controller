#pragma once
#include <QCoreApplication>

class LimeSDRDevice;
struct RxMissionConfig;
struct TxMissionConfig;

class Application : public QCoreApplication
{
    Q_OBJECT
private:
    enum ExitCodes
    {
        NormalExit = 0,
        CmdArgumentsError,
        DevicesInitError,
        MissionError
    };

public:
    Application(int& argc, char** argv, int flags = ApplicationFlags);
    ~Application();

private slots:
    void onEventLoopInitialization();
    void startRxMission(const RxMissionConfig& config);
    void startTxMission(const TxMissionConfig& config);

private:
    bool processCommandLineArguments();

private:
    QList<LimeSDRDevice*> mDevices;
    bool mConsoleUseCase = true;
};

