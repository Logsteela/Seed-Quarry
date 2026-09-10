#include "terrainoracle26.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <QWaitCondition>

#include <atomic>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

struct OraclePaths
{
    QString java;
    QString helperDirectory;
    QStringList classPath;
    QString error;

    bool valid() const
    {
        return error.isEmpty() && !java.isEmpty() &&
            !helperDirectory.isEmpty() && !classPath.isEmpty();
    }
};

static QString firstExistingFile(const QStringList& candidates)
{
    for (const QString& candidate : candidates)
    {
        if (!candidate.isEmpty() && QFileInfo::exists(candidate))
            return QDir::cleanPath(candidate);
    }
    return {};
}

static OraclePaths findOraclePaths()
{
    OraclePaths result;
    const QString appDirectory = QCoreApplication::applicationDirPath();
    result.helperDirectory = QDir(appDirectory).filePath("helpers");
    const QString helper = QDir(result.helperDirectory)
        .filePath("ExactTerrainOracle262.class");
    if (!QFileInfo::exists(helper))
    {
        result.error = QCoreApplication::translate("TerrainOracle26",
            "The Java 26.2 terrain helper is missing. Run Rebuild and Run once.");
        return result;
    }

    const QString programFilesX86 =
        qEnvironmentVariable("ProgramFiles(x86)");
    const QString programFiles = qEnvironmentVariable("ProgramFiles");
    const QString runtimeSuffix = QStringLiteral(
        "Minecraft Launcher/runtime/java-runtime-epsilon/windows-x64/"
        "java-runtime-epsilon/bin/java.exe");
    result.java = firstExistingFile({
        QDir(programFilesX86).filePath(runtimeSuffix),
        QDir(programFiles).filePath(runtimeSuffix),
        QStandardPaths::findExecutable(QStringLiteral("java.exe")),
        QStandardPaths::findExecutable(QStringLiteral("java")),
    });
    if (result.java.isEmpty())
    {
        result.error = QCoreApplication::translate("TerrainOracle26",
            "Minecraft's Java 26.2 runtime was not found.");
        return result;
    }

    const QString appData = qEnvironmentVariable("APPDATA");
    const QString minecraft = QDir(appData).filePath(".minecraft");
    const QString versionDirectory =
        QDir(minecraft).filePath("versions/26.2");
    const QString clientJar = QDir(versionDirectory).filePath("26.2.jar");
    const QString versionJson = QDir(versionDirectory).filePath("26.2.json");
    if (!QFileInfo::exists(clientJar) || !QFileInfo::exists(versionJson))
    {
        result.error = QCoreApplication::translate("TerrainOracle26",
            "Minecraft Java 26.2 is not installed in the official launcher.");
        return result;
    }

    QFile metadataFile(versionJson);
    if (!metadataFile.open(QIODevice::ReadOnly))
    {
        result.error = QCoreApplication::translate("TerrainOracle26",
            "Minecraft Java 26.2 metadata could not be opened.");
        return result;
    }
    QJsonParseError parseError;
    const QJsonDocument metadata = QJsonDocument::fromJson(
        metadataFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !metadata.isObject())
    {
        result.error = QCoreApplication::translate("TerrainOracle26",
            "Minecraft Java 26.2 metadata is invalid.");
        return result;
    }

    result.classPath = {result.helperDirectory, clientJar};
    const QString libraries = QDir(minecraft).filePath("libraries");
    const QJsonArray entries = metadata.object().value("libraries").toArray();
    for (const QJsonValue& entry : entries)
    {
        const QString relative = entry.toObject()
            .value("downloads").toObject()
            .value("artifact").toObject()
            .value("path").toString();
        if (relative.isEmpty())
            continue;
        const QString path = QDir(libraries).filePath(relative);
        if (QFileInfo::exists(path))
            result.classPath.push_back(path);
    }
    return result;
}

class TerrainOracleThread final : public QThread
{
public:
    void warmUp()
    {
        QMutexLocker lock(&m_mutex);
        if (!m_startAttempted)
        {
            m_startAttempted = true;
            start();
        }
    }

    bool query(const QString& command, int *portalY, QString *error)
    {
        // A single JVM avoids multiplying Minecraft's registry/noise memory by
        // the number of native search workers.
        QMutexLocker oneCaller(&m_callMutex);
        QMutexLocker lock(&m_mutex);
        if (!m_startAttempted)
            warmUpLocked();
        while (!m_startFinished)
        {
            if (!m_startCondition.wait(&m_mutex, 60000))
            {
                if (error)
                    *error = QCoreApplication::translate(
                        "TerrainOracle26",
                        "Timed out while starting the Java terrain helper.");
                return false;
            }
        }
        if (!m_startError.isEmpty())
        {
            if (error) *error = m_startError;
            return false;
        }
        m_command = command;
        m_responseReady = false;
        m_requestCondition.wakeOne();
        while (!m_responseReady)
        {
            if (!m_responseCondition.wait(&m_mutex, 120000))
            {
                if (error)
                    *error = QCoreApplication::translate(
                        "TerrainOracle26",
                        "The Java terrain helper did not answer in time.");
                return false;
            }
        }
        if (!m_responseError.isEmpty())
        {
            if (error) *error = m_responseError;
            return false;
        }
        if (portalY) *portalY = m_responseY;
        return true;
    }

    ~TerrainOracleThread() override
    {
        {
            QMutexLocker lock(&m_mutex);
            m_stopping = true;
            m_abort.store(true, std::memory_order_relaxed);
            m_requestCondition.wakeOne();
        }
        if (isRunning())
            wait(5000);
    }

protected:
    void run() override
    {
        const OraclePaths paths = findOraclePaths();
        if (!paths.valid())
        {
            finishStart(paths.error);
            return;
        }

        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
        process.setCreateProcessArgumentsModifier(
            [](QProcess::CreateProcessArguments *arguments) {
                arguments->flags |= CREATE_NO_WINDOW;
            });
#endif
        process.start(paths.java, {
            QStringLiteral("-Xms32m"), QStringLiteral("-Xmx384m"),
            QStringLiteral("-Dlog4j.configurationFile=%1")
                .arg(QDir(paths.helperDirectory)
                    .filePath("exact-terrain-log4j2.xml")),
            QStringLiteral("-cp"),
            paths.classPath.join(QDir::listSeparator()),
            QStringLiteral("ExactTerrainOracle262"),
        });
        if (!process.waitForStarted(15000))
        {
            finishStart(QCoreApplication::translate("TerrainOracle26",
                "The Java terrain helper could not be started: %1")
                .arg(process.errorString()));
            return;
        }

        QString startupError;
        if (!readProtocolLine(&process, "SQREADY ", nullptr,
                &startupError, 60000))
        {
            finishStart(startupError);
            stopProcess(process);
            return;
        }
        finishStart({});

        for (;;)
        {
            QString command;
            {
                QMutexLocker lock(&m_mutex);
                while (m_command.isEmpty() && !m_stopping)
                    m_requestCondition.wait(&m_mutex);
                if (m_stopping)
                    break;
                command = m_command;
                m_command.clear();
            }

            process.write(command.toUtf8());
            process.write("\n");
            QString response;
            QString responseError;
            bool ok = process.waitForBytesWritten(10000);
            if (ok)
            {
                ok = readProtocolLine(&process, "SQOK ", &response,
                    &responseError, 120000);
            }
            else
            {
                responseError = QCoreApplication::translate(
                    "TerrainOracle26",
                    "Could not send a request to the Java terrain helper.");
            }
            int y = 0;
            if (ok)
            {
                bool converted = false;
                y = response.toInt(&converted);
                if (!converted)
                {
                    ok = false;
                    responseError = QCoreApplication::translate(
                        "TerrainOracle26",
                        "The Java terrain helper returned an invalid height.");
                }
            }
            {
                QMutexLocker lock(&m_mutex);
                m_responseY = y;
                m_responseError = ok ? QString() : responseError;
                m_responseReady = true;
                m_responseCondition.wakeOne();
            }
        }
        process.write("Q\n");
        process.waitForBytesWritten(1000);
        stopProcess(process);
    }

private:
    static void stopProcess(QProcess& process)
    {
        process.closeWriteChannel();
        if (!process.waitForFinished(2000))
        {
            process.terminate();
            if (!process.waitForFinished(1000))
                process.kill();
        }
    }

    bool readProtocolLine(
        QProcess *process, const QByteArray& wantedPrefix,
        QString *value, QString *error, int timeout)
    {
        QElapsedTimer timer;
        timer.start();
        QByteArray buffered;
        for (;;)
        {
            while (process->canReadLine())
            {
                const QByteArray line = process->readLine().trimmed();
                if (line.startsWith(wantedPrefix))
                {
                    if (value)
                        *value = QString::fromUtf8(
                            line.mid(wantedPrefix.size()));
                    return true;
                }
                if (line.startsWith("SQERR "))
                {
                    if (error)
                        *error = QCoreApplication::translate(
                            "TerrainOracle26",
                            "Java terrain calculation failed: %1")
                            .arg(QString::fromUtf8(line.mid(6)));
                    return false;
                }
                if (!line.isEmpty())
                {
                    buffered += line;
                    buffered += '\n';
                }
            }
            if (m_abort.load(std::memory_order_relaxed))
            {
                if (error)
                    *error = QCoreApplication::translate(
                        "TerrainOracle26", "The Java terrain helper was stopped.");
                return false;
            }
            const int remaining = timeout - int(timer.elapsed());
            if (remaining <= 0 ||
                !process->waitForReadyRead(qMin(remaining, 1000)))
            {
                if (process->state() == QProcess::NotRunning || remaining <= 0)
                {
                    if (error)
                    {
                        const QString detail = QString::fromUtf8(
                            buffered.trimmed());
                        *error = detail.isEmpty()
                            ? QCoreApplication::translate(
                                "TerrainOracle26",
                                "The Java terrain helper stopped unexpectedly.")
                            : QCoreApplication::translate(
                                "TerrainOracle26",
                                "The Java terrain helper stopped unexpectedly: %1")
                                .arg(detail);
                    }
                    return false;
                }
            }
        }
    }

    void finishStart(const QString& error)
    {
        QMutexLocker lock(&m_mutex);
        m_startError = error;
        m_startFinished = true;
        m_startCondition.wakeAll();
    }

    void warmUpLocked()
    {
        m_startAttempted = true;
        start();
    }

    QMutex m_callMutex;
    QMutex m_mutex;
    QWaitCondition m_startCondition;
    QWaitCondition m_requestCondition;
    QWaitCondition m_responseCondition;
    bool m_startAttempted = false;
    bool m_startFinished = false;
    bool m_stopping = false;
    bool m_responseReady = false;
    std::atomic_bool m_abort{false};
    QString m_startError;
    QString m_command;
    QString m_responseError;
    int m_responseY = 0;
};

static TerrainOracleThread& oracleThread()
{
    static TerrainOracleThread oracle;
    return oracle;
}

} // namespace

bool isExactTerrainOracle26Available(QString *error)
{
    const OraclePaths paths = findOraclePaths();
    if (error) *error = paths.error;
    return paths.valid();
}

void warmUpExactTerrainOracle26()
{
    oracleThread().warmUp();
}

bool exactRuinedPortalY26(
    uint64_t worldSeed, uint64_t structureRandom,
    int location, int ySpan,
    int minX, int minZ, int maxX, int maxZ,
    bool largeBiomes, int *portalY, QString *error)
{
    const QString command = QStringLiteral(
        "P %1 %2 %3 %4 %5 %6 %7 %8 %9 262")
        .arg(qulonglong(worldSeed))
        .arg(qulonglong(structureRandom))
        .arg(location).arg(ySpan)
        .arg(minX).arg(minZ).arg(maxX).arg(maxZ)
        .arg(largeBiomes ? 1 : 0);
    return oracleThread().query(command, portalY, error);
}
