#ifndef PHANTOM_H
#define PHANTOM_H

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVariantMap>
#include <QVariantList>
#include <QNetworkProxy>

// Forward declarations
class QCoreApplication;
class WebPage;
class Config;
class Terminal;
class CookieJar;
class Repl;
class FileSystem;
class ChildProcess;
class System;
class WebServer;
class QCommandLine;
class IEngineBackend;

class Phantom : public QObject
{
    Q_OBJECT

public:
    explicit Phantom(QCoreApplication* app);
    ~Phantom();

    // --- Core Lifecycle ---
    bool init(int argc, char** argv);
    int executeScript(const QString& scriptPath, const QStringList& scriptArgs);
    void startInteractive();

    // --- Properties exposed to JavaScript (Q_PROPERTY) ---
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString libraryPath READ libraryPath CONSTANT)
    Q_PROPERTY(QString scriptName READ scriptName CONSTANT)
    Q_PROPERTY(QStringList args READ args CONSTANT)
    Q_PROPERTY(QStringList casperPaths READ casperPaths WRITE setCasperPaths NOTIFY casperPathsChanged)
    Q_PROPERTY(QStringList env READ env CONSTANT)
    Q_PROPERTY(QVariantMap defaultPageSettings READ defaultPageSettings WRITE setDefaultPageSettings NOTIFY defaultPageSettingsChanged)
    Q_PROPERTY(bool cookiesEnabled READ cookiesEnabled WRITE setCookiesEnabled NOTIFY cookiesEnabledChanged)
    Q_PROPERTY(QString cookiesFile READ cookiesFile WRITE setCookiesFile NOTIFY cookiesFileChanged)
    Q_PROPERTY(int remoteDebugPort READ remoteDebugPort WRITE setRemoteDebugPort NOTIFY remoteDebugPortChanged)
    Q_PROPERTY(bool printStackTrace READ printStackTrace WRITE setPrintStackTrace NOTIFY printStackTraceChanged)
    Q_PROPERTY(QString outputEncoding READ outputEncoding WRITE setOutputEncoding NOTIFY outputEncodingChanged)
    Q_PROPERTY(QString scriptEncoding READ scriptEncoding WRITE setScriptEncoding NOTIFY scriptEncodingChanged)
    Q_PROPERTY(QString scriptLanguage READ scriptLanguage WRITE setScriptLanguage NOTIFY scriptLanguageChanged)

    // --- Methods exposed to JavaScript (Q_INVOKABLE) ---
    Q_INVOKABLE QObject* createWebPage();
    Q_INVOKABLE void exit(int code = 0);
    Q_INVOKABLE void addCookie(const QVariantMap& cookie);
    Q_INVOKABLE void deleteCookie(const QString& name);
    Q_INVOKABLE void clearCookies();
    Q_INVOKABLE QVariantList cookies();
    Q_INVOKABLE void injectJs(const QString& jsFilePath);
    Q_INVOKABLE void setProxy(const QString& ip, const qint64& port = 0, const QString& proxyType = QString(), const QString& user = QString(), const QString& password = QString());
    Q_INVOKABLE void setProxyAuth(const QString& user, const QString& password);
    Q_INVOKABLE void debugExit(int code = 0);
    Q_INVOKABLE void addEventListener(const QString& name, QObject* callback);
    Q_INVOKABLE void removeEventListener(const QString& name, QObject* callback);
    Q_INVOKABLE QVariant evaluate(const QString& func, const QVariantList& args);

    // --- Getters for Q_PROPERTY ---
    QString version() const;
    QString libraryPath() const;
    QString scriptName() const;
    QStringList args() const;
    QStringList casperPaths() const;
    QStringList env() const;
    QVariantMap defaultPageSettings() const;
    bool cookiesEnabled() const;
    QString cookiesFile() const;
    int remoteDebugPort() const;
    bool printStackTrace() const;
    QString outputEncoding() const;
    QString scriptEncoding() const;
    QString scriptLanguage() const;

    // --- Setters for Q_PROPERTY ---
    void setCasperPaths(const QStringList& paths);
    void setDefaultPageSettings(const QVariantMap& settings);
    void setCookiesEnabled(bool enabled);
    void setCookiesFile(const QString& path);
    void setRemoteDebugPort(int port);
    void setPrintStackTrace(bool enable);
    void setOutputEncoding(const QString& encoding);
    void setScriptEncoding(const QString& encoding);
    void setScriptLanguage(const QString& language);

    // --- Internal Getters ---
    bool isInteractive() const;
    QString scriptPath() const;
    QStringList scriptArgs() const;
    bool helpRequested() const;
    bool versionRequested() const;
    void showHelp();
    void showVersion();

signals:
    // Global signals
    void libraryPathChanged(const QString& libraryPath);
    void casperPathsChanged(const QStringList& paths);
    void defaultPageSettingsChanged(const QVariantMap& settings);
    void cookiesEnabledChanged(bool enabled);
    void cookiesFileChanged(const QString& path);
    void remoteDebugPortChanged(int port);
    void printStackTraceChanged(bool enable);
    void outputEncodingChanged(const QString& encoding);
    void scriptEncodingChanged(const QString& encoding);
    void scriptLanguageChanged(const QString& language);

private slots:
    void onPageCreated(WebPage* newPage);
    void onInitialized();
    void onExit();
    void onRemoteDebugPortChanged();

private:
    QCoreApplication* m_app;
    QPointer<WebPage> m_page;
    Config* m_config;
    Terminal* m_terminal;
    CookieJar* m_cookieJar;

    Repl* m_repl;
    FileSystem* m_fs;
    ChildProcess* m_childProcess;
    System* m_system;
    WebServer* m_webserver;

    QCommandLine* m_cmdLineParser;

    QString m_scriptPath;
    QStringList m_scriptArgs;
    QStringList m_appArgs;

    QStringList m_casperPaths;
    QVariantMap m_defaultPageSettings;
    int m_remoteDebugPort;
    bool m_printStackTrace;

    bool m_isInteractive;
    bool m_helpRequested;
    bool m_versionRequested;

    void setupGlobalObjects();
    void cleanupGlobalObjects();
    void parseCommandLine(int argc, char** argv);
    void exposeGlobalObjectsToJs();
};

#endif // PHANTOM_H
