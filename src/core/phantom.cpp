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

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QNetworkProxy> // For QNetworkProxy
#include <QTimer> // For exit() delay, if needed for graceful shutdown
#include <QDateTime> // For timing in main, if needed

// Static global instance (if Phantom is a singleton, otherwise not needed)
// Phantom is usually not a singleton, but rather instantiated once in main.
// So, removing this if it was there and not truly a singleton.

Phantom::Phantom(QCoreApplication* app)
    : QObject(app) // Phantom is the top-level QObject, parented by QCoreApplication
    , m_app(app)
    , m_page(nullptr) // Will be created in init or createWebPage
    , m_config(Config::instance()) // Get singleton Config instance
    , m_terminal(Terminal::instance()) // Get singleton Terminal instance
    , m_cookieJar(new CookieJar(this)) // Global CookieJar
    , m_repl(nullptr)
    , m_fs(nullptr)
    , m_childProcess(nullptr)
    , m_system(nullptr)
    , m_webserver(nullptr)
    , m_cmdLineParser(new QCommandLine(this)) // Instantiate your custom parser
    , m_remoteDebugPort(-1) // Default debug port
    , m_printStackTrace(false)
    , m_isInteractive(false)
    , m_helpRequested(false)
    , m_versionRequested(false) {
    // Connect to application's aboutToQuit for cleanup
    connect(m_app, &QCoreApplication::aboutToQuit, this, &Phantom::onExit);

    // Initial setup for the global cookie jar
    m_cookieJar->setParent(this); // Make Phantom the parent
    // The default WebPage will pick this up when created.
}

Phantom::~Phantom() {
    // Clean up dynamically allocated objects that are not parented
    // (QObjects parented by 'this' will be deleted automatically)
    if (m_repl) {
        delete m_repl;
        m_repl = nullptr;
    }
    // Filesystem, ChildProcess, System, WebServer are usually not newed/deleted by Phantom
    // unless explicitly created/managed. If they are QObject singletons or similar, let them be.
}

bool Phantom::init(int argc, char** argv) {
    // 1. Parse command line arguments using your custom QCommandLine
    QStringList appArgs;
    for (int i = 0; i < argc; ++i) {
        appArgs << QString::fromLocal8Bit(argv[i]);
    }
    m_appArgs = appArgs; // Store application arguments

    // Pass the global flags array (from config.cpp) to your custom QCommandLine parser
    // Ensure the 'flags' array is visible or passed correctly.
    // If 'flags' is defined as `extern const struct QCommandLineConfigEntry flags[];` in a common header
    // and defined in config.cpp, then it's accessible.
    // Or, if it's a member of Config, you'd do m_config->getCommandLineFlags().
    // Assuming 'flags' is globally accessible, or QCommandLine handles it:
    m_cmdLineParser->setConfig(flags); // Assuming 'flags' is visible here

    m_cmdLineParser->setArguments(m_appArgs);

    // Explicitly enable help/version entries (if not handled by setConfig via flags)
    m_cmdLineParser->enableHelp(true);
    m_cmdLineParser->enableVersion(true);

    // Connect signals from the parser to Phantom's slots for handling
    connect(m_cmdLineParser, &QCommandLine::optionFound, this, [this](const QString& name, const QVariant& value) {
        // Handle options found by the parser
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
            // Proxy parsing is complex, handle it here or in a helper
            // Expected format: user:password@host:port or host:port
            QString proxyString = value.toString();
            QString proxyUser, proxyPass;
            QString proxyHost;
            qint64 proxyPort = 0;
            QString proxyType = "http"; // Default

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
            // Already handled by proxy parsing, or if separate option
            // This would override the type set by the 'proxy' option
            setProxy(
                m_config->get("proxy").toString(), 0, value.toString()); // Placeholder, requires actual proxy state
        } else if (name == "proxy-auth") {
            // Already handled by proxy parsing, or if separate option
            // This would set credentials for an already configured proxy
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

    connect(m_cmdLineParser, &QCommandLine::switchFound, this, [this](const QString& name) {
        if (name == "help") {
            m_helpRequested = true;
        } else if (name == "version") {
            m_versionRequested = true;
        }
    });

    connect(m_cmdLineParser, &QCommandLine::paramFound, this, [this](const QString& name, const QVariant& value) {
        if (name == "script") { // This should be the first positional argument
            m_scriptPath = value.toString();
        } else if (name == "args") { // These are subsequent positional arguments
            m_scriptArgs.append(value.toString());
        }
    });

    // Handle parse errors from QCommandLine
    connect(m_cmdLineParser, &QCommandLine::parseError, this, [this](const QString& error) {
        m_terminal->cerr("Command line parse error: " + error);
        m_helpRequested = true; // Show help on parse error
    });

    bool parseOk = m_cmdLineParser->parse();
    if (!parseOk) {
        // If parsing failed, and help wasn't explicitly requested, then there's a problem.
        m_terminal->cerr("Failed to parse command line arguments. See --help for usage.");
        m_helpRequested = true; // Force help display
        return false;
    }

    // After parsing, process any config file specified by --config
    QString configFilePath = m_config->get("config").toString(); // Retrieve 'config' option from Config
    if (!configFilePath.isEmpty()) {
        m_config->loadJsonFile(configFilePath);
    }

    // Set interactive mode if no script is provided
    m_isInteractive = m_scriptPath.isEmpty();

    // Create the default WebPage (main page)
    m_page = new WebPage(this); // Parent WebPage to Phantom
    m_page->setCookieJar(m_cookieJar); // Set global cookie jar
    // Connect WebPage signals to Phantom slots
    connect(m_page, &WebPage::initialized, this, &Phantom::onInitialized);
    connect(m_page, &WebPage::rawPageCreated, this, &Phantom::onPageCreated);
    connect(m_page, &WebPage::closing, this, &Phantom::onPageCreated); // This will emit after page has been created
    // ... other signals from WebPage to handle globally ...

    // Apply default settings to the page initially
    m_page->applySettings(m_config->defaultPageSettings());

    return true;
}

int Phantom::executeScript(const QString& scriptPath, const QStringList& scriptArgs) {
    qDebug() << "Executing script:" << scriptPath << "with args:" << scriptArgs;

    // Check if script exists
    if (!QFile::exists(scriptPath)) {
        m_terminal->cerr("Script file not found: " + scriptPath);
        return 1;
    }

    // Set script-specific arguments (used by system.args and page.args)
    m_scriptPath = scriptPath;
    m_scriptArgs = scriptArgs;

    // Simulate script execution by evaluating the script content
    // This is a placeholder. In a real scenario, you would pass the script content
    // to the JS engine (Playwright backend) for execution.
    // Playwright backend would need a way to run a full script environment.
    QString scriptContent;
    QFile scriptFile(scriptPath);
    if (scriptFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        scriptContent = QString::fromUtf8(scriptFile.readAll());
        scriptFile.close();
    } else {
        m_terminal->cerr("Could not read script file: " + scriptPath);
        return 1;
    }

    // The bootstrap.js (or similar) would usually handle loading the script
    // and setting up the global scope (phantom, WebPage etc.).
    // For now, we'll just evaluate the content directly.
    m_page->evaluateJavaScript(scriptContent);

    // In a real PhantomJS, the script execution would block until page.onLoadFinished or phantom.exit() is called.
    // For Playwright, the JS process runs asynchronously.
    // We need to keep the QCoreApplication event loop running until explicit exit.
    // This is typically done by running app.exec() in main.
    // If you return from executeScript, main() will continue and app.exec() will be called.
    // For headless execution, you usually wait for an explicit `phantom.exit()` call from JS.

    return 0; // Success (application will exit later via phantom.exit())
}

void Phantom::startInteractive() {
    if (m_repl) {
        delete m_repl;
    }
    m_repl = new Repl(this); // Repl is parented by Phantom
    m_repl->setWebPage(m_page); // Give REPL access to the current page
    m_repl->start(); // Start the REPL loop (this is typically blocking)
}

// --- Properties and their Getters/Setters ---

QString Phantom::version() const { return QCoreApplication::applicationVersion(); }

QString Phantom::libraryPath() const {
    // Return the directory where the PhantomJS executable resides
    return QCoreApplication::applicationDirPath();
}

QString Phantom::scriptName() const {
    QFileInfo info(m_scriptPath);
    return info.fileName();
}

QStringList Phantom::args() const {
    return m_scriptArgs; // Arguments passed *to the script*
}

QStringList Phantom::casperPaths() const { return m_casperPaths; }

void Phantom::setCasperPaths(const QStringList& paths) {
    if (m_casperPaths != paths) {
        m_casperPaths = paths;
        emit casperPathsChanged(paths);
    }
}

QStringList Phantom::env() const {
    // Return environment variables as a list of "KEY=VALUE" strings
    return QProcess::systemEnvironment();
}

QVariantMap Phantom::defaultPageSettings() const { return m_defaultPageSettings; }

void Phantom::setDefaultPageSettings(const QVariantMap& settings) {
    if (m_defaultPageSettings != settings) {
        m_defaultPageSettings = settings;
        emit defaultPageSettingsChanged(settings);
    }
}

bool Phantom::cookiesEnabled() const { return m_config->cookiesEnabled(); }

void Phantom::setCookiesEnabled(bool enabled) { m_config->setCookiesEnabled(enabled); }

QString Phantom::cookiesFile() const { return m_config->cookiesFile(); }

void Phantom::setCookiesFile(const QString& path) { m_config->setCookiesFile(path); }

int Phantom::remoteDebugPort() const {
    return m_remoteDebugPort; // Return the member variable
}

void Phantom::setRemoteDebugPort(int port) {
    if (m_remoteDebugPort != port) {
        m_remoteDebugPort = port;
        emit remoteDebugPortChanged(port);
        // You might want to trigger the inspector in the backend here
        // if the port changes after initialization.
        if (m_page && m_page->engineBackend()) { // Ensure backend is available
            m_page->showInspector(m_remoteDebugPort);
        }
    }
}

bool Phantom::printStackTrace() const { return m_printStackTrace; }

void Phantom::setPrintStackTrace(bool enable) {
    if (m_printStackTrace != enable) {
        m_printStackTrace = enable;
        emit printStackTraceChanged(enable);
        // Maybe also update terminal's debug/error verbosity based on this.
    }
}

QString Phantom::outputEncoding() const { return m_config->outputEncoding(); }

void Phantom::setOutputEncoding(const QString& encoding) { m_config->setOutputEncoding(encoding); }

QString Phantom::scriptEncoding() const { return m_config->scriptEncoding(); }

void Phantom::setScriptEncoding(const QString& encoding) { m_config->setScriptEncoding(encoding); }

QString Phantom::scriptLanguage() const { return m_config->scriptLanguage(); }

void Phantom::setScriptLanguage(const QString& language) { m_config->setScriptLanguage(language); }

bool Phantom::isInteractive() const { return m_isInteractive; }

QString Phantom::scriptPath() const { return m_scriptPath; }

QStringList Phantom::scriptArgs() const { return m_scriptArgs; }

bool Phantom::helpRequested() const { return m_helpRequested; }

bool Phantom::versionRequested() const { return m_versionRequested; }

void Phantom::showHelp() {
    m_terminal->cout(m_cmdLineParser->help(true)); // Show help with logo
    exit(0);
}

void Phantom::showVersion() {
    m_terminal->cout(m_cmdLineParser->version());
    exit(0);
}

// --- Q_INVOKABLE Methods ---

QObject* Phantom::createWebPage() {
    // Create a new WebPage instance, parent it to this Phantom object
    // Connect its rawPageCreated signal (from engine backend) to a slot in Phantom
    WebPage* newPage = new WebPage(this);
    newPage->setCookieJar(m_cookieJar); // Ensure new pages also use the global cookie jar

    // Apply default page settings to new page
    newPage->applySettings(m_defaultPageSettings);

    // No need to add to an explicit list here unless Phantom itself needs to manage a list of all pages.
    // WebPage::handleEnginePageCreated already re-emits rawPageCreated.

    return newPage;
}

void Phantom::exit(int code) {
    qDebug() << "Phantom::exit(" << code << ") called. Shutting down application.";
    // Perform any necessary cleanup before quitting.
    // QCoreApplication::quit() will stop the event loop.
    QCoreApplication::exit(code);
}

void Phantom::addCookie(const QVariantMap& cookie) { m_cookieJar->addCookie(cookie); }

void Phantom::deleteCookie(const QString& name) { m_cookieJar->deleteCookie(name); }

void Phantom::clearCookies() { m_cookieJar->clearCookies(); }

QVariantList Phantom::cookies() { return m_cookieJar->allCookiesToMap(); }

void Phantom::injectJs(const QString& jsFilePath) {
    if (m_page) {
        m_page->injectJs(jsFilePath);
    } else {
        m_terminal->cerr("Cannot injectJs: No active WebPage.");
    }
}

void Phantom::setProxy(
    const QString& ip, const qint64& port, const QString& proxyType, const QString& user, const QString& password) {
    QNetworkProxy proxy;
    proxy.setHostName(ip);
    proxy.setPort(port);
    if (proxyType.compare("socks5", Qt::CaseInsensitive) == 0) {
        proxy.setType(QNetworkProxy::Socks5Proxy);
    } else { // Default to HTTP Proxy
        proxy.setType(QNetworkProxy::HttpProxy);
    }
    proxy.setUser(user);
    proxy.setPassword(password);

    if (m_page) {
        m_page->setProxy(proxy); // Use WebPage's setProxy, which takes QNetworkProxy
    } else {
        m_terminal->cerr("Cannot set proxy: No active WebPage.");
    }
}

void Phantom::setProxyAuth(const QString& user, const QString& password) {
    // This assumes the proxy host/port are already set.
    // QNetworkProxy does not have a separate 'setAuth' method,
    // auth is part of the proxy object itself.
    // You would typically re-apply the proxy with updated auth.
    // This method implies changing the auth on the *current* proxy.
    if (m_page && m_page->proxy().type() != QNetworkProxy::NoProxy) {
        QNetworkProxy currentProxy = m_page->proxy(); // Get current proxy
        currentProxy.setUser(user);
        currentProxy.setPassword(password);
        m_page->setProxy(currentProxy); // Re-apply with updated auth
    } else {
        m_terminal->cerr("Cannot set proxy authentication: No active WebPage or proxy not set.");
    }
}

void Phantom::debugExit(int code) {
    qDebug() << "Phantom::debugExit(" << code << ") called.";
    exit(code);
}

void Phantom::addEventListener(const QString& name, QObject* callback) {
    // This would typically register a global event listener, e.g., for 'page.onLoadFinished'
    // but on all new pages created. Complex to implement without a global event bus.
    qWarning() << "Phantom::addEventListener is not fully implemented.";
}

void Phantom::removeEventListener(const QString& name, QObject* callback) {
    qWarning() << "Phantom::removeEventListener is not fully implemented.";
}

QObject* Phantom::evaluate(const QString& func, const QVariantList& args) {
    if (m_page) {
        return m_page->evaluateJavaScript(
            func + "(" + args.join(",") + ");"); // Simple eval, assumes func is a JS string
    }
    m_terminal->cerr("Cannot evaluate: No active WebPage.");
    return QVariant();
}

// --- Private Slots ---
void Phantom::onPageCreated(IEngineBackend* newPageBackend) {
    // This slot is called by WebPage::handleEnginePageCreated
    // It indicates that the underlying engine has created a new page (e.g., from window.open)
    // We need to create a new C++ WebPage wrapper for it.
    WebPage* newWebPage = new WebPage(this, QUrl(), newPageBackend); // Create new WebPage with existing backend
    newWebPage->setCookieJar(m_cookieJar); // Set global cookie jar
    newWebPage->applySettings(m_defaultPageSettings); // Apply default settings

    // Now, emit a signal so JS can get a handle to this new page
    // (e.g., phantom.onPageCreated or phantom.pages[].onInitialized)
    // For now, rawPageCreated is emitted by WebPage, this allows Phantom to manage it.
    // You might want to maintain a list of all pages in Phantom for `phantom.pages` property.
    qDebug() << "Phantom: A new WebPage was created by the backend.";
    // No direct signal like phantom.onPageCreated, the JS side would usually listen to page.onInitialized or similar.
    // However, if there's a phantom.pages array, you'd add it there.
}

void Phantom::onInitialized() {
    qDebug() << "Phantom: Initial WebPage initialized.";
    if (m_page && m_page->engineBackend()) { // Ensure backend is available
        // Expose global 'phantom' object to the main page's JavaScript context
        m_page->engineBackend()->exposeQObject("phantom", this);
        // Expose 'webpage' (the current page) itself
        m_page->engineBackend()->exposeQObject("page", m_page);

        // Expose the global objects (fs, child_process, system, webserver)
        m_fs = new FileSystem(this); // Parent to Phantom
        m_page->engineBackend()->exposeQObject("fs", m_fs);

        m_childProcess = new ChildProcess(this);
        m_page->engineBackend()->exposeQObject("child_process", m_childProcess);

        m_system = new System(this);
        m_page->engineBackend()->exposeQObject("system", m_system);

        m_webserver = new WebServer(this);
        m_page->engineBackend()->exposeQObject("webserver", m_webserver);

        // Expose the 'console' object for direct JS console.log calls to C++ Terminal
        m_page->engineBackend()->exposeQObject("console", m_terminal); // Expose Terminal as 'console'
    } else {
        qWarning() << "Phantom::onInitialized: WebPage or its backend not available.";
    }
}

void Phantom::onExit(int exitCode) {
    qDebug() << "Phantom::onExit called with code:" << exitCode;
    // Perform any last-minute cleanup here.
    // The QCoreApplication is already quitting.
    // Objects parented by m_app or this Phantom instance will be deleted.
}

void Phantom::onRemoteDebugPortChanged() {
    // This slot can be used to re-trigger the inspector setup if the port changes dynamically.
    if (m_page && m_page->engineBackend() && m_remoteDebugPort > 0) {
        m_page->showInspector(m_remoteDebugPort);
    }
}

// --- Private Helper Methods ---

void Phantom::parseCommandLine(int argc, char** argv) {
    // This method is now integrated into init()
    Q_UNUSED(argc);
    Q_UNUSED(argv);
}

void Phantom::setupGlobalObjects() {
    // This is now largely handled in onInitialized()
}

void Phantom::cleanupGlobalObjects() {
    // This is largely handled by QObject parenting.
    // If you had non-QObject resources or unparented QObjects, delete them here.
}

void Phantom::exposeGlobalObjectsToJs() {
    // This is now handled in onInitialized() for the main page.
    // New pages would need to re-expose objects as well.
}
