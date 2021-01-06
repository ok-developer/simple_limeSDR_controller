#include <QDir>

#include <csignal>
#include <unistd.h>

#include "Application.hpp"

void systemSignalsHandler(int signalNumber);
void checkPermissions();
QString applicationPath();

int main(int argc, char** argv)
{
    qSetMessagePattern("[%{time h:mm:ss:zzz}]["
                       "%{if-debug}D%{endif}"
                       "%{if-info}I%{endif}"
                       "%{if-warning}W%{endif}"
                       "%{if-critical}C%{endif}"
                       "%{if-fatal}F%{endif}] "
                       "%{message}");

    signal(SIGTERM, systemSignalsHandler);
    signal(SIGINT , systemSignalsHandler);
    signal(SIGABRT, systemSignalsHandler);
    signal(SIGSEGV, systemSignalsHandler);

#ifndef QT_DEBUG
    checkPermissions();
#endif

    QDir::setCurrent(applicationPath());

    Application application(argc, argv);

    return application.exec();
}

void systemSignalsHandler(int signalNumber)
{
    qInfo("System signal accepted: %i", signalNumber);

    int exitCode = signalNumber;
    switch (signalNumber)
    {
    case SIGINT:
    case SIGTERM: exitCode = 0; break;
    }

    Application::exit(exitCode);
}


void checkPermissions()
{
    if (geteuid() == 0)
    {
        qDebug("Superuser permissions granted!");
    }
    else
    {
        qFatal("This program needs to be started with superuser permissions!");
    }
}

QString applicationPath()
{
    char result[PATH_MAX];
    const auto count = readlink("/proc/self/exe", result, PATH_MAX);
    const auto string = QString::fromLatin1(result, count);

    return string.left(string.lastIndexOf('/'));
}
