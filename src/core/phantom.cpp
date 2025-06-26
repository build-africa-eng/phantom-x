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

#include "phantom.h"
#include "config.h"
#include "terminal.h"
#include "webpage.h" // Include WebPage header for its API
#include "cookiejar.h"
#include "repl.h"
#include "filesystem.h"
#include "childprocess.h"
#include "system.h"
#include "webserver.h"
#include "qcommandline/qcommandline.h" // For your custom QCommandLine parser
#include "ienginebackend.h" // For IEngineBackend* in onPageCreated if needed (but now we take WebPage*)

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QNetworkProxy>
#include <QTimer>
#include <QDateTime>

// Extern declaration for flags array from config.cpp
extern const struct QCommandLineConfigEntry flags[];


Phantom::Phantom(QCoreApplication* app)
    : QObject(app)
    , m_app(app)
    , m_page(nullptr)
    , m_config(Config::instance())
    , m_terminal(Terminal::instance())
    , m_cookieJar(new CookieJar(this))
    , m_repl(nullptr)
    , m_fs(nullptr)
    , m_childProcess(nullptr)
    , m_system(nullptr)
    , m_webserver(nullptr)
    , m_cmdLineParser(new QCommandLine(this))
    , m_remoteDebugPort(-1)
    , m_printStackTrace(false)
    , m_isInteractive(false)
    , m_helpRequested(false)
    , m_versionRequested(false)
{
    // Connect QCoreApplication::aboutToQuit signal to Phantom::onExit slot
    connect(m_app, &QCoreApplication::aboutToQuit, this, &Phantom::onExit); // FIXED: No arguments for onExit

    m_cookieJar->setParent(this);
}

Phantom::~Phantom()
{
    if (m_repl) {
        delete m_repl;
        m_repl = nullptr;
    }
    // Other cleanup (fs, childProcess, system, webserver) are usually parented or singletons, not deleted here.
}

bool Phantom::init(int argc, char** argv)
{
    QStringList appArgs;
    for (int i = 0; i < argc; ++i) {
        appArgs << QString::fromLocal8Bit(argv[i]);
    }
    m_appArgs = appArgs;

    m_cmdLineParser->setConfig(flags); // Use the globally accessible flags array

    m_cmdLineParser->setArguments(m_appArgs);

    m_cmdLineParser->enableHelp(true);
    m_cmdLineParser->enableVersion(true);

    connect(m_cmdLineParser, &QCommandLine::optionFound, this, [this](const QString& name, const QVariant& value){
        if (name == "debug") {
            m_config->setDebug(value.toBool());
        } else if (name == "console-level") {
            m_config->setLogLevel(value.toString());
        } else if (name == "cookies-file") {
            m_config->setCookiesFile(value.toString());
        } else if (name == "cookies-enabled") {
            m_config->setCookiesEnabled(value.toBool());
        } else if (name == "disk-cache") {
            m_config->setDiskCacheEnabled(value.toBool());
        } else if (name == "max-disk-cache-size") {
            m_config->setMaxDiskCacheSize(value.toInt());
        } else if (name == "disk-cache-path") {
            m_config->setDiskCachePath(value.toString());
        } else if (name == "ignore-ssl-errors") {
            m_config->setIgnoreSslErrors(value.toBool());
        } else if (name == "local-storage-path") {
            m_config->setLocalStoragePath(value.toString());
        } else if (name == "local-storage-quota") {
            m_config->setLocalStorageQuota(value.toInt());
        } else if (name == "load-images") {
            m_config->setAutoLoadImages(value.toBool());
        } else if (name == "local-to-remote-url-access") {
            m_config->setLocalToRemoteUrlAccessEnabled(value.toBool());
        } else if (name == "offline-storage-path") {
            m_config->setOfflineStoragePath(value.toString());
        } else if (name == "offline-storage-quota") {
            m_config->setOfflineStorageQuota(value.toInt());
        } else if (name == "output-encoding") {
            m_config->setOutputEncoding(value.toString());
        } else if (name == "script-encoding") {
            m_config->setScriptEncoding(value.toString());
        } else if (name == "ssl-protocol") {
            m_config->setSslProtocol(value.toString());
        } else if (name == "ssl-ciphers") {
            m_config->setSslCiphers(value.toString());
        } else if (name == "ssl-certificates-path") {
            m_config->setSslCertificatesPath(value.toString());
        } else if (name == "ssl-client-certificate-file") {
            m_config->setSslClientCertificateFile(value.toString());
        } else if (name == "ssl-client-key-file") {
            m_config->setSslClientKeyFile(value.toString());
        } else if (name == "ssl-client-key-passphrase") {
            m_config->setSslClientKeyPassphrase(value.toByteArray());
        } else if (name == "resource-timeout") {
            m_config->setResourceTimeout(value.toInt());
        } else if (name == "max-auth-attempts") {
            m_config->setMaxAuthAttempts(value.toInt());
        } else if (name == "javascript-enabled") {
            m_config->setJavascriptEnabled(value.toBool());
        } else if (name == "web-security") {
            m_config->setWebSecurityEnabled(value.toBool());
        } else if (name == "webgl-enabled") {
            m_config->setWebGLEnabled(value.toBool());
        } else if (name == "javascript-can-open-windows") {
            m_config->setJavascriptCanOpenWindows(value.toBool());
        } else if (name == "javascript-can-close-windows") {
            m_config->setJavascriptCanCloseWindows(value.toBool());
        } else if (name == "print-header") {
            m_config->setPrintHeader(value.toBool());
        } else if (name == "print-footer") {
            m_config->setPrintFooter(value.toBool());
        } else if (name == "proxy") {
            QString proxyString = value.toString();
            QString proxyUser, proxyPass;
            QString proxyHost;
            qint64 proxyPort = 0;
            QString proxyType = "http";

            QRegularExpression re(R"((?:([^:]+):([^@]+)@)?([^:]+)(?::(\d+))?)");
            QRegularExpressionMatch match = re.match(proxyString);
            if (match.hasMatch()) {
                proxyUser = match.captured(1);
                proxyPass = match.captured(2);
                proxyHost = match.captured(3);
                proxyPort = match.captured(4).toLongLong();
            }
            setProxy(proxyHost, proxyPort, proxyType, proxyUser, proxyPass);
        } else if (name == "proxy-type") {
            setProxy(m_page->proxy().hostName(), m_page->proxy().port(), value.toString()); // Pass current host/port, update type
        } else if (name == "proxy-auth") {
            QString authString = value.toString();
            QString user, pass;
            if (authString.contains(':')) {
                user = authString.section(':', 0, 0);
                pass = authString.section(':', 1);
            } else {
                user = authString;
            }
            setProxyAuth(user, pass);
        } else if (name == "config") {
            m_config->loadJsonFile(value.toString());
        }
    });

    connect(m_cmdLineParser, &QCommandLine::switchFound, this, [this](const QString& name){
        if (name == "help") {
            m_helpRequested = true;
        } else if (name == "version") {
            m_versionRequested = true;
        }
    });

    connect(m_cmdLineParser, &QCommandLine::paramFound, this, [this](const QString& name, const QVariant& value){
        if (name == "script") {
            m_scriptPath = value.toString();
        } else if (name == "args") {
            m_scriptArgs.append(value.toString());
        }
    });

    connect(m_cmdLineParser, &QCommandLine::parseError, this, [this](const QString& error){
        m_terminal->cerr("Command line parse error: " + error);
        m_helpRequested = true;
    });

    bool parseOk = m_cmdLineParser->parse();
    if (!parseOk) {
        m_terminal->cerr("Failed to parse command line arguments. See --help for usage.");
        m_helpRequested = true;
        return false;
    }

    QString configFilePath = m_config->get("config").toString();
    if (!configFilePath.isEmpty()) {
        m_config->loadJsonFile(configFilePath);
    }

    m_isInteractive = m_scriptPath.isEmpty();

    m_page = new WebPage(this);
    m_page->setCookieJar(m_cookieJar);
    connect(m_page, &WebPage::initialized, this, &Phantom::onInitialized);
    connect(m_page, &WebPage::rawPageCreated, this, &Phantom::onPageCreated); // FIXED: Argument type matches

    m_page->applySettings(m_config->defaultPageSettings());

    return true;
}

int Phantom::executeScript(const QString& scriptPath, const QStringList& scriptArgs)
{
    qDebug() << "Executing script:" << scriptPath << "with args:" << scriptArgs;

    if (!QFile::exists(scriptPath)) {
        m_terminal->cerr("Script file not found: " + scriptPath);
        return 1;
    }

    m_scriptPath = scriptPath;
    m_scriptArgs = scriptArgs;

    QString scriptContent;
    QFile scriptFile(scriptPath);
    if (scriptFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        scriptContent = QString::fromUtf8(scriptFile.readAll());
        scriptFile.close();
    } else {
        m_terminal->cerr("Could not read script file: " + scriptPath);
        return 1;
    }

    m_page->evaluateJavaScript(scriptContent);

    return 0;
}

void Phantom::startInteractive()
{
    if (m_repl) {
        delete m_repl;
    }
    m_repl = new Repl(this);
    m_repl->setWebPage(m_page);
    m_repl->start();
}

QString Phantom::version() const
{
    return QCoreApplication::applicationVersion();
}

QString Phantom::libraryPath() const
{
    return QCoreApplication::applicationDirPath();
}

QString Phantom::scriptName() const
{
    QFileInfo info(m_scriptPath);
    return info.fileName();
}

QStringList Phantom::args() const
{
    return m_scriptArgs;
}

QStringList Phantom::casperPaths() const
{
    return m_casperPaths;
}

void Phantom::setCasperPaths(const QStringList& paths)
{
    if (m_casperPaths != paths) {
        m_casperPaths = paths;
        emit casperPathsChanged(paths);
    }
}

QStringList Phantom::env() const
{
    return QProcess::systemEnvironment();
}

QVariantMap Phantom::defaultPageSettings() const
{
    return m_defaultPageSettings;
}

void Phantom::setDefaultPageSettings(const QVariantMap& settings)
{
    if (m_defaultPageSettings != settings) {
        m_defaultPageSettings = settings;
        emit defaultPageSettingsChanged(settings);
    }
}

bool Phantom::cookiesEnabled() const
{
    return m_config->cookiesEnabled();
}

void Phantom::setCookiesEnabled(bool enabled)
{
    m_config->setCookiesEnabled(enabled);
}

QString Phantom::cookiesFile() const
{
    return m_config->cookiesFile();
}

void Phantom::setCookiesFile(const QString& path)
{
    m_config->setCookiesFile(path);
}

int Phantom::remoteDebugPort() const
{
    return m_remoteDebugPort;
}

void Phantom::setRemoteDebugPort(int port)
{
    if (m_remoteDebugPort != port) {
        m_remoteDebugPort = port;
        emit remoteDebugPortChanged(port);
        if (m_page && m_page->engineBackend()) { // This access is now public via getter
            m_page->showInspector(m_remoteDebugPort);
        }
    }
}

bool Phantom::printStackTrace() const
{
    return m_printStackTrace;
}

void Phantom::setPrintStackTrace(bool enable)
{
    if (m_printStackTrace != enable) {
        m_printStackTrace = enable;
        emit printStackTraceChanged(enable);
    }
}

QString Phantom::outputEncoding() const
{
    return m_config->outputEncoding();
}

void Phantom::setOutputEncoding(const QString& encoding)
{
    m_config->setOutputEncoding(encoding);
}

QString Phantom::scriptEncoding() const
{
    return m_config->scriptEncoding();
}

void Phantom::setScriptEncoding(const QString& encoding)
{
    m_config->setScriptEncoding(encoding);
}

QString Phantom::scriptLanguage() const
{
    return m_config->scriptLanguage();
}

void Phantom::setScriptLanguage(const QString& language)
{
    m_config->setScriptLanguage(language);
}

bool Phantom::isInteractive() const
{
    return m_isInteractive;
}

QString Phantom::scriptPath() const
{
    return m_scriptPath;
}

QStringList Phantom::scriptArgs() const
{
    // Fix for Error 1: scriptArgs is a member of Phantom, not Config.
    return m_scriptArgs;
}

bool Phantom::helpRequested() const
{
    return m_helpRequested;
}

bool Phantom::versionRequested() const
{
    return m_versionRequested;
}

void Phantom::showHelp()
{
    m_terminal->cout(m_cmdLineParser->help(true));
    exit(0);
}

void Phantom::showVersion()
{
    m_terminal->cout(m_cmdLineParser->version());
    exit(0);
}

QObject* Phantom::createWebPage()
{
    WebPage* newPage = new WebPage(this);
    newPage->setCookieJar(m_cookieJar);
    newPage->applySettings(m_defaultPageSettings);
    return newPage;
}

void Phantom::exit(int code)
{
    qDebug() << "Phantom::exit(" << code << ") called. Shutting down application.";
    QCoreApplication::exit(code);
}

void Phantom::addCookie(const QVariantMap& cookie)
{
    m_cookieJar->addCookie(cookie);
}

void Phantom::deleteCookie(const QString& name)
{
    m_cookieJar->deleteCookie(name);
}

void Phantom::clearCookies()
{
    m_cookieJar->clearCookies();
}

QVariantList Phantom::cookies()
{
    return m_cookieJar->allCookiesToMap();
}

void Phantom::injectJs(const QString& jsFilePath)
{
    if (m_page) {
        m_page->injectJs(jsFilePath);
    } else {
        m_terminal->cerr("Cannot injectJs: No active WebPage.");
    }
}

void Phantom::setProxy(const QString& ip, const qint64& port, const QString& proxyType, const QString& user, const QString& password)
{
    QNetworkProxy proxy;
    proxy.setHostName(ip);
    proxy.setPort(port);
    if (proxyType.compare("socks5", Qt::CaseInsensitive) == 0) {
        proxy.setType(QNetworkProxy::Socks5Proxy);
    } else {
        proxy.setType(QNetworkProxy::HttpProxy);
    }
    proxy.setUser(user);
    proxy.setPassword(password);

    if (m_page) {
        m_page->setProxy(proxy); // FIXED: Pass QNetworkProxy object directly
    } else {
        m_terminal->cerr("Cannot set proxy: No active WebPage.");
    }
}

void Phantom::setProxyAuth(const QString& user, const QString& password)
{
    if (m_page && m_page->proxy().type() != QNetworkProxy::NoProxy) {
        QNetworkProxy currentProxy = m_page->proxy();
        currentProxy.setUser(user);
        currentProxy.setPassword(password);
        m_page->setProxy(currentProxy);
    } else {
        m_terminal->cerr("Cannot set proxy authentication: No active WebPage or proxy not set.");
    }
}

void Phantom::debugExit(int code)
{
    qDebug() << "Phantom::debugExit(" << code << ") called.";
    exit(code);
}

void Phantom::addEventListener(const QString& name, QObject* callback)
{
    qWarning() << "Phantom::addEventListener is not fully implemented.";
}

void Phantom::removeEventListener(const QString& name, QObject* callback)
{
    qWarning() << "Phantom::removeEventListener is not fully implemented.";
}

QObject* Phantom::evaluate(const QString& func, const QVariantList& args)
{
    if (m_page) {
        // Simple eval, assumes func is a JS string and args are JS-compatible.
        // For complex cases, consider a JSON.stringify for args.
        QString scriptToEval = func;
        if (!args.isEmpty()) {
            QStringList argStrings;
            for (const QVariant& arg : args) {
                // This is a very basic conversion. Real solution needs more robust variant to JS literal conversion.
                if (arg.type() == QVariant::String) {
                    argStrings << QString("'%1'").arg(arg.toString().replace("'", "\\'"));
                } else {
                    argStrings << arg.toString();
                }
            }
            scriptToEval += "(" + argStrings.join(", ") + ")";
        } else {
            scriptToEval += "()"; // Call as a function even if no args
        }

        return m_page->evaluateJavaScript(scriptToEval);
    }
    m_terminal->cerr("Cannot evaluate: No active WebPage.");
    return QVariant();
}

void Phantom::onPageCreated(WebPage* newPage) // FIXED: Argument type
{
    qDebug() << "Phantom: A new WebPage was created by the backend.";
    if (newPage) {
        // You might want to add newPage to a list of managed pages here
        // if Phantom needs to keep track of all pages for `phantom.pages`
        // newPage->setParent(this); // Already parented by the signal emitting WebPage usually.
    } else {
        qWarning() << "Phantom::onPageCreated: Received null newPage.";
    }
}

void Phantom::onInitialized()
{
    qDebug() << "Phantom: Initial WebPage initialized.";
    if (m_page && m_page->engineBackend()) {
        m_page->engineBackend()->exposeQObject("phantom", this);
        m_page->engineBackend()->exposeQObject("page", m_page);

        m_fs = new FileSystem(this);
        m_page->engineBackend()->exposeQObject("fs", m_fs);

        m_childProcess = new ChildProcess(this);
        m_page->engineBackend()->exposeQObject("child_process", m_childProcess);

        m_system = new System(this);
        m_page->engineBackend()->exposeQObject("system", m_system);

        m_webserver = new WebServer(this);
        m_page->engineBackend()->exposeQObject("webserver", m_webserver);

        m_page->engineBackend()->exposeQObject("console", m_terminal);
    } else {
        qWarning() << "Phantom::onInitialized: WebPage or its backend not available.";
    }
}

void Phantom::onExit() // FIXED: No arguments
{
    qDebug() << "Phantom::onExit called.";
}

void Phantom::onRemoteDebugPortChanged()
{
    if (m_page && m_page->engineBackend() && m_remoteDebugPort > 0) {
        m_page->showInspector(m_remoteDebugPort);
    }
}

void Phantom::parseCommandLine(int argc, char** argv) { Q_UNUSED(argc); Q_UNUSED(argv); /* Integrated into init() */ }
void Phantom::setupGlobalObjects() { /* Handled in onInitialized() */ }
void Phantom::cleanupGlobalObjects() { /* Handled by QObject parenting */ }
void Phantom::exposeGlobalObjectsToJs() { /* Handled in onInitialized() */ }

