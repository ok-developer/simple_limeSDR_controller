#pragma once

#include <QList>
#include <QObject>

#include <thread>
#include <atomic>

#include "lime/LimeSuite.h"

struct RxMissionConfig;
struct TxMissionConfig;

class LimeSDRDevice : public QObject
{
    Q_OBJECT
public:
    enum ChannelType
    {
        RX = LMS_CH_RX,
        TX = LMS_CH_TX
    };

public:
    static QList<LimeSDRDevice*> availableDevicesList();

    explicit LimeSDRDevice(QObject* parent = nullptr);
    ~LimeSDRDevice();

    bool init(lms_info_str_t* initStr);
    quint64 deviceIdentificator() const;

    bool startRxMission(const RxMissionConfig& config);
    void stopRxMission(quint16 rxNumber);

    bool startTxMission(const TxMissionConfig& config);
    void stopTxMission(quint16 txNumber);

signals:
    void rxStarted();
    void rxAvailable(const QByteArray& data);
    void rxFinished();

    void txStarted();
    void txFinished();

private:
    bool switchChannel(ChannelType type, quint16 channel, bool state);
    void deinitStream(lms_stream_t** stream);
    void deinitRxStream(int channel);
    void deinitTxStream(int channel);

    void rxRoutine(int streamId, quint32 samplesCount, int recordsCount);
    void txRoutine(int streamId, int transmissionsCount, const QString& fileName);

    const char* channelToString(ChannelType type) const;
    const char* stateToString(bool state) const;

private:
    lms_device_t* mDevice = nullptr;
    QVector<lms_stream_t*> mRxStreams;
    QVector<lms_stream_t*> mTxStreams;

    // TODO: more then one rx/tx thread
    std::unique_ptr<std::thread> mRxThread = nullptr;
    std::unique_ptr<std::thread> mTxThread = nullptr;

    // TODO: more then one rx/tx thread flag
    std::atomic_bool mRxThreadFlag = false;
    std::atomic_bool mTxThreadFlag = false;

    quint64 mDeviceIdentificator;
};

