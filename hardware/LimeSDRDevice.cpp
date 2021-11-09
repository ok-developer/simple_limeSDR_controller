#include <QDir>
#include <QFile>
#include <QDateTime>

#include <cstring>

#include "types/RxMissionConfig.hpp"
#include "types/TxMissionConfig.hpp"
#include "LimeSDRDevice.hpp"

inline const quint16 SampleSize = sizeof(quint16) * 2;
inline const quint16 ErrorMaxCount = 5;

inline void SameLinePrint(const QString& data)
{
    fprintf(stderr, "%c[2K", 27);
    fprintf(stderr, "\r%s", qPrintable(data));
}

QList<LimeSDRDevice*> LimeSDRDevice::availableDevicesList()
{
    lms_info_str_t* devices = nullptr;
    QList<LimeSDRDevice*> result;

    const auto count = LMS_GetDeviceList(devices);
    if (count <= 0)
    {
        qWarning("No limeSDR devices found! Exiting...");
        return result;
    }

    for (int i = 0; i < count; ++i)
    {
        auto device = new LimeSDRDevice;
        if (not device->init(&devices[i]))
        {
            qWarning("Device '%s' init fail!", devices[i]);
            delete device;
        }
        else result.append(device);
    }

    return result;
}

LimeSDRDevice::LimeSDRDevice(QObject* parent)
    : QObject(parent)
{

}

LimeSDRDevice::~LimeSDRDevice()
{
    mRxThreadFlag.store(false);
    mTxThreadFlag.store(false);

    if (mRxThread and mRxThread->joinable())
    {
        mRxThread->join();
    }

    if (mTxThread and mTxThread->joinable())
    {
        mTxThread->join();
    }

    for (auto stream : qAsConst(mRxStreams))
    {
        if (stream) deinitStream(&stream);
    }
    for (auto stream : qAsConst(mTxStreams))
    {
        if (stream) deinitStream(&stream);
    }

    if (mDevice)
    {
        switchChannel(RX, 0, false);
        switchChannel(RX, 1, false);
        switchChannel(TX, 0, false);
        switchChannel(TX, 1, false);
        LMS_Close(mDevice);
        mDevice = nullptr;
    }
}

bool LimeSDRDevice::init(lms_info_str_t* initStr)
{
    const lms_dev_info_t* deviceInformation = nullptr;

    if (mDevice not_eq nullptr)
    {
        qWarning("[LimeSDRDevice][%llu] Device already inited!",
                 mDeviceIdentificator);
        return false;
    }

    if (LMS_Open(&mDevice, *initStr, NULL) not_eq 0)
    {
        qWarning("[LimeSDRDevice] Device open error.");
        return false;
    }

    deviceInformation = LMS_GetDeviceInfo(mDevice);
    qInfo("Name: %s", deviceInformation->deviceName);
    qInfo("Expansion name: %s", deviceInformation->expansionName);
    qInfo("Firmware version: %s", deviceInformation->firmwareVersion);
    qInfo("Hardware version: %s", deviceInformation->hardwareVersion);
    qInfo("Protocol version: %s", deviceInformation->protocolVersion);
    qInfo("Board serial number: %llu", deviceInformation->boardSerialNumber);
    qInfo("Gateware version: %s", deviceInformation->gatewareVersion);
    qInfo("Gateware target board: %s", deviceInformation->gatewareTargetBoard);

    mDeviceIdentificator = deviceInformation->boardSerialNumber;
    mRxStreams.resize(LMS_GetNumChannels(mDevice, RX));
    mTxStreams.resize(LMS_GetNumChannels(mDevice, TX));

    if (LMS_Init(mDevice) not_eq 0)
    {
        qWarning("[LimeSDRDevice] Failed to init device.");
        LMS_Close(mDevice);
        mDevice = nullptr;
        return false;
    }

    return true;
}

quint64 LimeSDRDevice::deviceIdentificator() const
{
    return mDeviceIdentificator;
}

bool LimeSDRDevice::startRxMission(const RxMissionConfig& config)
{
    if (mRxStreams.at(config.channelNumber)) return false;

    if (not switchChannel(RX, config.channelNumber, true)) return false;

    if (LMS_SetSampleRate(mDevice, config.sampleRate, 0) not_eq 0)
    {
        switchChannel(RX, config.channelNumber, false);
        qWarning("[LimeSDRDevice][%llu] Error while setting samplerate to %llu: %s!",
                 mDeviceIdentificator, config.sampleRate, LMS_GetLastErrorMessage());
        return false;
    }

    if (LMS_SetLOFrequency(mDevice, RX, config.channelNumber, config.frequency) not_eq 0)
    {
        switchChannel(RX, config.channelNumber, false);
        qWarning("[LimeSDRDevice][%llu] Error while setting frequency to %llu: %s!",
                 mDeviceIdentificator, config.frequency, LMS_GetLastErrorMessage());
        return false;
    }

    if (LMS_SetAntenna(mDevice, RX, config.channelNumber, config.antenaNumber) not_eq 0)
    {
        switchChannel(RX, config.channelNumber, false);
        qWarning("[LimeSDRDevice][%llu] Error while setting antena: %s!",
                 mDeviceIdentificator, LMS_GetLastErrorMessage());
        return false;
    }

    if (LMS_SetGaindB(mDevice, RX, config.channelNumber, config.gain) not_eq 0)
    {
        switchChannel(RX, config.channelNumber, false);
        qWarning("[LimeSDRDevice][%llu] Error while setting gain to %u: %s!",
                 mDeviceIdentificator, config.gain, LMS_GetLastErrorMessage());
        return false;
    }

    if (LMS_Calibrate(mDevice, RX, config.channelNumber, config.bandwidth, 0) not_eq 0)
    {
        switchChannel(RX, config.channelNumber, false);
        qWarning("[LimeSDRDevice][%llu] Error while calibrating: %s!",
                 mDeviceIdentificator, LMS_GetLastErrorMessage());
        return false;
    }

    auto stream = new lms_stream_t;
    stream->dataFmt = lms_stream_t::LMS_FMT_I16;
    stream->channel = config.channelNumber;
    stream->fifoSize = config.samplesCount * 2;
    stream->isTx = false;
    stream->throughputVsLatency = 1.0;

    if (LMS_SetupStream(mDevice, stream) not_eq 0)
    {
        delete stream;
        switchChannel(RX, config.channelNumber, false);
        qWarning("[LimeSDRDevice][%llu] Error while stream setup: %s!",
                 mDeviceIdentificator, LMS_GetLastErrorMessage());
        return false;
    }

    mRxStreams[config.channelNumber] = stream;

    mRxThread.reset(new std::thread(&LimeSDRDevice::rxRoutine, this,
                                    config.channelNumber, config.samplesCount, config.tryCount));

    qDebug("[LimeSDRDevice][%llu] Rx mission created!", mDeviceIdentificator);
    return true;
}

void LimeSDRDevice::stopRxMission(quint16 )
{
    mRxThreadFlag.store(false);
}

bool LimeSDRDevice::startTxMission(const TxMissionConfig& config)
{
    if (mTxStreams.at(config.channelNumber)) return false;

    const QFileInfo file(QDir::current().absoluteFilePath("TX") + "/" + config.fileName);

    if (not file.exists())
    {
        qWarning("[LimeSDRDevice][%llu] Selected file not exists!", mDeviceIdentificator);
        return false;
    }

    if (not switchChannel(TX, config.channelNumber, true)) return false;

    if (LMS_SetSampleRate(mDevice, config.sampleRate, 0) not_eq 0)
    {
        switchChannel(TX, config.channelNumber, false);
        qWarning("[LimeSDRDevice][%llu] Error while setting samplerate to %llu: %s!",
                 mDeviceIdentificator, config.sampleRate, LMS_GetLastErrorMessage());
        return false;
    }

    if (LMS_SetLOFrequency(mDevice, TX, config.channelNumber, config.frequency) not_eq 0)
    {
        switchChannel(TX, config.channelNumber, false);
        qWarning("[LimeSDRDevice][%llu] Error while setting frequency to %llu: %s!",
                 mDeviceIdentificator, config.frequency, LMS_GetLastErrorMessage());
        return false;
    }

    if (LMS_SetAntenna(mDevice, TX, config.channelNumber, config.antenaNumber) not_eq 0)
    {
        switchChannel(TX, config.channelNumber, false);
        qWarning("[LimeSDRDevice][%llu] Error while setting antena: %s!",
                 mDeviceIdentificator, LMS_GetLastErrorMessage());
        return false;
    }

    if (LMS_SetGaindB(mDevice, TX, config.channelNumber, config.gain) not_eq 0)
    {
        switchChannel(TX, config.channelNumber, false);
        qWarning("[LimeSDRDevice][%llu] Error while setting gain to %u: %s!",
                 mDeviceIdentificator, config.gain, LMS_GetLastErrorMessage());
        return false;
    }

    if (LMS_Calibrate(mDevice, TX, config.channelNumber, config.bandwidth, 0) not_eq 0)
    {
        switchChannel(TX, config.channelNumber, false);
        qWarning("[LimeSDRDevice][%llu] Error while calibrating: %s!",
                 mDeviceIdentificator, LMS_GetLastErrorMessage());
        return false;
    }

    auto stream = new lms_stream_t;
    stream->dataFmt = lms_stream_t::LMS_FMT_I16;
    stream->channel = config.channelNumber;
    stream->fifoSize = file.size() / SampleSize;
    stream->isTx = true;
    stream->throughputVsLatency = 0.5;

    if (LMS_SetupStream(mDevice, stream) not_eq 0)
    {
        delete stream;
        switchChannel(TX, config.channelNumber, false);
        qWarning("[LimeSDRDevice][%llu] Error while stream setup: %s!",
                 mDeviceIdentificator, LMS_GetLastErrorMessage());
        return false;
    }

    mTxStreams[config.channelNumber] = stream;

    mTxThread.reset(new std::thread(&LimeSDRDevice::txRoutine, this,
                                    config.channelNumber, config.tryCount, config.fileName));

    qDebug("[LimeSDRDevice][%llu] Tx mission created!", mDeviceIdentificator);
    return true;
}

void LimeSDRDevice::stopTxMission(quint16 )
{
    mTxThreadFlag.store(false);
}

bool LimeSDRDevice::switchChannel(ChannelType type, quint16 channel, bool state)
{
    if (not mDevice) return false;
    else if (LMS_EnableChannel(mDevice, type, channel, state) not_eq 0)
    {
        qWarning("[LimeSDRDevice][%llu] Error while setting %s%i channel to state '%s': %s!",
                 mDeviceIdentificator,
                 channelToString(type),
                 channel + 1,
                 stateToString(state),
                 LMS_GetLastErrorMessage());
        return false;
    }
    else return true;
}

void LimeSDRDevice::deinitStream(lms_stream_t** stream)
{
    if (*stream)
    {
        LMS_StopStream(*stream);
        LMS_DestroyStream(mDevice, *stream);
        *stream = nullptr;
    }
}

void LimeSDRDevice::deinitRxStream(int channel)
{
    if (channel >= mRxStreams.count()) return;
    deinitStream(&mRxStreams[channel]);
}

void LimeSDRDevice::deinitTxStream(int channel)
{
    if (channel >= mTxStreams.count()) return;
    deinitStream(&mTxStreams[channel]);
}

void LimeSDRDevice::rxRoutine(int streamId, quint32 samplesCount, int recordsCount)
{
    const auto currentFolderName = QDateTime::currentDateTime().toString("dd.MM.yyyy_hh.mm.ss");
    const QString rxLabel = channelToString(RX);
    const QString fileTemplate = "%1.bin";
    QByteArray buffer(samplesCount * SampleSize, Qt::Uninitialized);
    QDir dir(QDir::current());
    auto stream = mRxStreams.at(streamId);
    int errorsCounter = 0;
    int currentTry = 0;

    recordsCount = (recordsCount == 0) ? -1 : recordsCount;

    if (not dir.exists(rxLabel)) dir.mkdir(rxLabel);
    dir.cd(rxLabel);
    dir.mkdir(currentFolderName);
    dir.cd(currentFolderName);

    mRxThreadFlag.store(true);
    LMS_StartStream(stream);

    qDebug("[LimeSDRDevice][%llu] Rx mission started!", mDeviceIdentificator);
    emit rxStarted();

    while (mRxThreadFlag.load()
      and  recordsCount not_eq currentTry)
    {
        const int captured = LMS_RecvStream(stream, buffer.data(), samplesCount, NULL, 1000);
        if (captured < 0)
        {
            qWarning("[LimeSDRDevice][%llu] Rx stream receive error: %s!",
                     mDeviceIdentificator, LMS_GetLastErrorMessage());

            ++errorsCounter;
            if (errorsCounter not_eq ErrorMaxCount) continue;
            else break;
        }

        QFile output(dir.absoluteFilePath(fileTemplate.arg(currentTry)));

        if (not output.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            qWarning("[LimeSDRDevice][%llu] Rx output write error: %s!",
                     mDeviceIdentificator, qPrintable(output.errorString()));

            ++errorsCounter;
            if (errorsCounter not_eq ErrorMaxCount) continue;
            else break;
        }
        else
        {
            output.write(buffer);
            emit rxAvailable(buffer);

            qDebug("[LimeSDRDevice][%llu] Rx mission %i try.",
                   mDeviceIdentificator, currentTry);

            if (currentTry == INT32_MAX) currentTry = 0;
            else ++currentTry;
        }
    }

    deinitRxStream(streamId);

    qDebug("[LimeSDRDevice][%llu] Rx mission finished.", mDeviceIdentificator);
    emit rxFinished();
}

void LimeSDRDevice::txRoutine(int streamId, int transmissionsCount, const QString& fileName)
{
    QFile input(QDir::current().absoluteFilePath("TX") + "/" + fileName);
    auto stream = mTxStreams.at(streamId);
    int errorsCounter = 0;
    int currentTry = 0;
    QByteArray buffer;

    transmissionsCount = (transmissionsCount == 0) ? -1 : transmissionsCount;

    if (not input.open(QIODevice::ReadOnly))
    {
        qWarning("[LimeSDRDevice][%llu] Tx file read error: %s!",
                 mDeviceIdentificator, qPrintable(input.errorString()));
        deinitTxStream(streamId);
        return;
    }
    else
    {
        buffer = input.readAll();
        input.close();
    }

    const auto samplesCount = buffer.size() / SampleSize;
    const auto oneTransmissionSize = samplesCount;

    mTxThreadFlag.store(true);
    LMS_StartStream(stream);

    qDebug("[LimeSDRDevice][%llu] Tx mission started!", mDeviceIdentificator);
    emit txStarted();

    while (mTxThreadFlag.load()
      and  transmissionsCount not_eq currentTry)
    {
        const auto sent = LMS_SendStream(stream, buffer.data(), oneTransmissionSize, NULL, INT_MAX);
        if (sent not_eq oneTransmissionSize)
        {
            qWarning("[LimeSDRDevice][%llu] Tx error: %s!",
                     mDeviceIdentificator, LMS_GetLastErrorMessage());

            ++errorsCounter;
            if (errorsCounter not_eq ErrorMaxCount) continue;
            else break;
        }

        //qDebug("[LimeSDRDevice][%llu] Tx mission %i try.",
        //       mDeviceIdentificator, currentTry);

        lms_stream_status_t status;
        LMS_GetStreamStatus(stream, &status);
        SameLinePrint(QString("fifo %1 | "
               "fifoSize %2 | "
               "underrun %3 | "
               "overrun %4 | "
               "dropped %5 | "
               "smplRate %6 | "
               "%7 mb/s")
               .arg(status.fifoFilledCount)
               .arg(status.fifoSize)
               .arg(status.underrun)
               .arg(status.overrun)
               .arg(status.droppedPackets)
               .arg(status.sampleRate / 1e6)
               .arg(status.linkRate / 1e6));

        if (currentTry == INT32_MAX) currentTry = 0;
        else ++currentTry;

        //std::this_thread::sleep_for(std::chrono::microseconds(50));
    }

    deinitTxStream(streamId);

    qDebug("\n[LimeSDRDevice][%llu] Tx mission finished.", mDeviceIdentificator);
    emit txFinished();
}

const char* LimeSDRDevice::channelToString(LimeSDRDevice::ChannelType type) const
{
    switch (type)
    {
    case RX: return "RX";
    case TX: return "TX";
    default: return "unknown";
    }
}

const char* LimeSDRDevice::stateToString(bool state) const
{
    return state ? "true" : "false";
}
