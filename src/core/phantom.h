/*
  This file is part of the PhantomJS project from Ofi Labs.

  Copyright (C) 2011 Ariya Hidayat <ariya.hidayat@gmail.com>
  Copyright (C) 2011 Ivan De Marino <ivan.de.marino@gmail.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef PHANTOM_H
#define PHANTOM_H

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVariantMap>
#include <QVariantList>
#include <QNetworkProxy> // For setProxy method

// Forward declarations
class QCoreApplication;
class WebPage;
class Config; // Still need to reference the singleton config
class Terminal;
class CookieJar; // For global cookie jar
class Repl; // For interactive mode
class FileSystem; // For fs module
class ChildProcess; // For child_process module
class System; // For system module
class WebServer; // For webserver module
class QCommandLine; // Your custom command line parser

class Phantom : public QObject {
    Q_OBJECT

public:
    explicit Phantom(QCoreApplication* app);
    ~Phantom();

    // --- Core Lifecycle ---
    bool init(int argc, char** argv); // Handles command-line parsing and initial setup
    int executeScript(const QString& scriptPath, const QStringList& scriptArgs);
    void startInteractive(); // For REPL mode

    // --- Properties exposed to JavaScript (Q_PROPERTY) ---
    Q_PROPERTY(QString version READ version CONSTANT) // This should be dynamic now
    Q_PROPERTY(QString libraryPath READ libraryPath CONSTANT)
    Q_PROPERTY(QString scriptName READ scriptName CONSTANT)
    Q_PROPERTY(QStringList args READ args CONSTANT) // Command-line arguments to the script
    Q_PROPERTY(QStringList casperPaths READ casperPaths WRITE setCasperPaths NOTIFY casperPathsChanged)
    Q_PROPERTY(QStringList env READ env CONSTANT) // Environment variables
    Q_PROPERTY(QVariantMap defaultPageSettings READ defaultPageSettings WRITE setDefaultPageSettings NOTIFY
            defaultPageSettingsChanged)
    Q_PROPERTY(bool cookiesEnabled READ cookiesEnabled WRITE setCookiesEnabled NOTIFY cookiesEnabledChanged)
    Q_PROPERTY(QString cookiesFile READ cookiesFile WRITE setCookiesFile NOTIFY cookiesFileChanged)
    Q_PROPERTY(
        int remoteDebugPort READ remoteDebugPort WRITE setRemoteDebugPort NOTIFY remoteDebugPortChanged) // NEW PROPERTY
    Q_PROPERTY(bool printStackTrace READ printStackTrace WRITE setPrintStackTrace NOTIFY printStackTraceChanged)
    Q_PROPERTY(QString outputEncoding READ outputEncoding WRITE setOutputEncoding NOTIFY outputEncodingChanged)
    Q_PROPERTY(QString scriptEncoding READ scriptEncoding WRITE setScriptEncoding NOTIFY scriptEncodingChanged)
    Q_PROPERTY(QString scriptLanguage READ scriptLanguage WRITE
            setScriptEncoding) // Typo: Should be setScriptLanguage, not setScriptEncoding

    // --- Methods exposed to JavaScript (Q_INVOKABLE) ---
    Q_INVOKABLE QObject* createWebPage(); // Creates a new WebPage instance
    Q_INVOKABLE void exit(int code = 0); // Exits the application
    Q_INVOKABLE void addCookie(const QVariantMap& cookie); // Adds a cookie globally
    Q_INVOKABLE void deleteCookie(const QString& name); // Deletes a cookie globally
    Q_INVOKABLE void clearCookies(); // Clears all cookies globally
    Q_INVOKABLE QVariantList cookies(); // Gets all cookies globally
    Q_INVOKABLE void injectJs(const QString& jsFilePath); // Injects JS globally
    Q_INVOKABLE void setProxy(const QString& ip, const qint64& port = 0, const QString& proxyType = QString(),
        const QString& user = QString(), const QString& password = QString()); // Set proxy globally
    Q_INVOKABLE void setProxyAuth(const QString& user, const QString& password); // Set proxy authentication
    Q_INVOKABLE void debugExit(int code = 0); // For debugging purposes, exits
    Q_INVOKABLE void addEventListener(const QString& name, QObject* callback); // For adding global event listeners
    Q_INVOKABLE void removeEventListener(const QString& name, QObject* callback); // For removing global event listeners
    Q_INVOKABLE QObject* evaluate(const QString& func, const QVariantList& args); // Global evaluate in the current page

    // --- Getters for Q_PROPERTY ---
    QString version() const;
    QString libraryPath() const;
    QString scriptName() const;
    QStringList args() const; // Script arguments passed to Phantom
    QStringList casperPaths() const;
    QStringList env() const;
    QVariantMap defaultPageSettings() const;
    bool cookiesEnabled() const;
    QString cookiesFile() const;
    int remoteDebugPort() const; // NEW GETTER
    bool printStackTrace() const;
    QString outputEncoding() const;
    QString scriptEncoding() const;
    QString scriptLanguage() const; // Fixed getter name

    // --- Setters for Q_PROPERTY ---
    void setCasperPaths(const QStringList& paths);
    void setDefaultPageSettings(const QVariantMap& settings);
    void setCookiesEnabled(bool enabled);
    void setCookiesFile(const QString& path);
    void setRemoteDebugPort(int port); // NEW SETTER
    void setPrintStackTrace(bool enable);
    void setOutputEncoding(const QString& encoding);
    void setScriptEncoding(const QString& encoding);
    void setScriptLanguage(const QString& language);

    // --- Internal Getters ---
    bool isInteractive() const; // Is PhantomJS running in interactive mode?
    QString scriptPath() const; // The full path to the script being run
    QStringList scriptArgs() const; // Arguments *to the script* (different from application arguments)
    bool helpRequested() const; // Indicates if --help was requested
    bool versionRequested() const; // Indicates if --version was requested
    void showHelp(); // Prints help and exits
    void showVersion(); // Prints version and exits

signals:
    // Global signals
    void libraryPathChanged(const QString& libraryPath);
    void casperPathsChanged(const QStringList& paths);
    void defaultPageSettingsChanged(const QVariantMap& settings);
    void cookiesEnabledChanged(bool enabled);
    void cookiesFileChanged(const QString& path);
    void remoteDebugPortChanged(int port); // NEW SIGNAL
    void printStackTraceChanged(bool enable);
    void outputEncodingChanged(const QString& encoding);
    void scriptEncodingChanged(const QString& encoding);
    void scriptLanguageChanged(const QString& language);

private slots:
    void onPageCreated(IEngineBackend* newPageBackend); // Handles new pages from backend
    void onInitialized(); // Called when the initial page's JS context is ready
    void onExit(int exitCode); // Handles application exit
    void onRemoteDebugPortChanged(); // For internal use after setting port

private:
    QCoreApplication* m_app;
    QPointer<WebPage> m_page; // The default WebPage instance
    Config* m_config; // Singleton instance of Config
    Terminal* m_terminal; // Singleton instance of Terminal
    CookieJar* m_cookieJar; // Global cookie jar

    Repl* m_repl; // For interactive mode
    FileSystem* m_fs; // fs module
    ChildProcess* m_childProcess; // child_process module
    System* m_system; // system module
    WebServer* m_webserver; // webserver module

    QCommandLine* m_cmdLineParser; // Our custom command line parser instance

    QString m_scriptPath; // Full path to the script being executed
    QStringList m_scriptArgs; // Arguments passed *to the script*
    QStringList m_appArgs; // Arguments passed *to the phantomjs executable*

    QStringList m_casperPaths;
    QVariantMap m_defaultPageSettings;
    int m_remoteDebugPort; // NEW MEMBER
    bool m_printStackTrace;

    bool m_isInteractive; // Flag for interactive mode
    bool m_helpRequested;
    bool m_versionRequested;

    // Private helper methods
    void setupGlobalObjects();
    void cleanupGlobalObjects();
    void parseCommandLine(int argc, char** argv);
    void exposeGlobalObjectsToJs();
};

#endif // PHANTOM_H
