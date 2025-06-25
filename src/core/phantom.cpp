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

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QMetaProperty>
#include <QScreen>
#include <QStandardPaths>
#include <QNetworkProxyFactory> // Standard Qt class, compatible with Qt5/Qt6
#include <QNetworkProxy>       // Standard Qt class, compatible with Qt5/Qt6

// NO LONGER NEEDED: QtWebKit includes are removed
// #if QT_VERSION_MAJOR == 5
// #include <QtWebKitVersion>
// #include <QtWebKitWidgets/QWebPage>
// #include <QtWebKitWidgets/QWebSettings>
// #elif QT_VERSION_MAJOR == 6
// #warning "Qt6 compatibility requires migrating from QtWebKit to QtWebEngine. QWebPage, QWebFrame, and QWebSettings are not available in Qt6. Core web functionality will be broken until WebPage is re-implemented for QtWebEngine."
// #endif

#include "callback.h"
#include "childprocess.h"
#include "consts.h"
#include "cookiejar.h"
#include "repl.h" // Will need updates to use WebPage directly
#include "system.h"
#include "terminal.h"
#include "utils.h" // Will need updates to accept WebPage* instead of QWebFrame*
#include "webpage.h" // This class now uses IEngineBackend
#include "webserver.h"

// Removed the QTWEBKIT_VERSION check as QtWebKit is no longer a direct dependency.

static Phantom* phantomInstance = Q_NULLPTR;

// private:
Phantom::Phantom(QObject* parent)
    : QObject(parent)
    , m_terminated(false)
    , m_returnValue(0)
    , m_filesystem(0)
    , m_system(0)
    , m_childprocess(0)
{
    QStringList args = QApplication::arguments();

    // Prepare the configuration object based on the command line arguments.
    // Because this object will be used by other classes, it needs to be ready
    // ASAP.
    m_config.init(&args);
    // Apply debug configuration as early as possible
    Utils::printDebugMessages = m_config.printDebugMessages();
}

void Phantom::init()
{
    if (m_config.helpFlag()) {
        Terminal::instance()->cout(QString("%1").arg(m_config.helpText()));
        Terminal::instance()->cout("Any of the options that accept boolean values "
                                   "('true'/'false') can also accept 'yes'/'no'.");
        Terminal::instance()->cout("");
        Terminal::instance()->cout("Without any argument, PhantomJS will launch in "
                                   "interactive mode (REPL).");
        Terminal::instance()->cout("");
        Terminal::instance()->cout("Documentation can be found at the web site, http://phantomjs.org.");
        Terminal::instance()->cout("");
        m_terminated = true;
        return;
    }

    if (m_config.versionFlag()) {
        m_terminated = true;
        Terminal::instance()->cout(QString("%1").arg(PHANTOMJS_VERSION_STRING));
        return;
    }

    if (!m_config.unknownOption().isEmpty()) {
        Terminal::instance()->cerr(m_config.unknownOption());
        m_terminated = true;
        return;
    }

    // Initialize the CookieJar
    m_defaultCookieJar = new CookieJar(m_config.cookiesFile());

    // set the default DPI
    m_defaultDpi = qRound(QApplication::primaryScreen()->logicalDotsPerInch());

    // --- REMOVED QWebSettings calls. These are now applied via WebPage::applySettings() ---
    // QWebSettings::setOfflineWebApplicationCachePath(...)
    // QWebSettings::setOfflineStoragePath(...)
    // QWebSettings::setOfflineStorageDefaultQuota(...)

    m_page = new WebPage(this, QUrl::fromLocalFile(m_config.scriptFile()));
    m_page->setCookieJar(m_defaultCookieJar);
    m_pages.append(m_page);

    // Set up proxy if required
    QString proxyType = m_config.proxyType();
    if (proxyType != "none") {
        setProxy(
            m_config.proxyHost(), m_config.proxyPort(), proxyType, m_config.proxyAuthUser(), m_config.proxyAuthPass());
    }

    // Set output encoding
    Terminal::instance()->setEncoding(m_config.outputEncoding());

    // Set script file encoding
    m_scriptFileEnc.setEncoding(m_config.scriptEncoding());

    // Connect to WebPage signals (which are re-emitted from IEngineBackend)
    connect(m_page, &WebPage::javaScriptConsoleMessageSent, this, &Phantom::printConsoleMessage);
    connect(m_page, &WebPage::initialized, this, &Phantom::onInitialized);

    // Build default page settings map
    m_defaultPageSettings[PAGE_SETTINGS_LOAD_IMAGES] = QVariant::fromValue(m_config.autoLoadImages());
    m_defaultPageSettings[PAGE_SETTINGS_JS_ENABLED] = QVariant::fromValue(true); // Always true for PhantomJS
    m_defaultPageSettings[PAGE_SETTINGS_XSS_AUDITING] = QVariant::fromValue(false);
    m_defaultPageSettings[PAGE_SETTINGS_USER_AGENT] = QVariant::fromValue(m_page->userAgent()); // Get initial UA from WebPage
    m_defaultPageSettings[PAGE_SETTINGS_LOCAL_ACCESS_REMOTE]
        = QVariant::fromValue(m_config.localToRemoteUrlAccessEnabled());
    m_defaultPageSettings[PAGE_SETTINGS_WEB_SECURITY_ENABLED] = QVariant::fromValue(m_config.webSecurityEnabled());
    m_defaultPageSettings[PAGE_SETTINGS_JS_CAN_OPEN_WINDOWS] = QVariant::fromValue(m_config.javascriptCanOpenWindows());
    m_defaultPageSettings[PAGE_SETTINGS_JS_CAN_CLOSE_WINDOWS]
        = QVariant::fromValue(m_config.javascriptCanCloseWindows());
    m_defaultPageSettings[PAGE_SETTINGS_DPI] = QVariant::fromValue(m_defaultDpi);
    // Add other relevant config settings to defaultPageSettings for WebPage to apply
    m_defaultPageSettings[PAGE_SETTINGS_IGNORE_SSL_ERRORS] = QVariant::fromValue(m_config.ignoreSslErrors());
    m_defaultPageSettings[PAGE_SETTINGS_LOCAL_URL_ACCESS_ENABLED] = QVariant::fromValue(m_config.localUrlAccessEnabled());
    m_defaultPageSettings[PAGE_SETTINGS_OFFLINE_STORAGE_PATH] = QVariant::fromValue(m_config.offlineStoragePath());
    m_defaultPageSettings[PAGE_SETTINGS_OFFLINE_STORAGE_QUOTA] = QVariant::fromValue(m_config.offlineStorageDefaultQuota());
    m_defaultPageSettings[PAGE_SETTINGS_LOCAL_STORAGE_PATH] = QVariant::fromValue(m_config.localStoragePath());
    m_defaultPageSettings[PAGE_SETTINGS_LOCAL_STORAGE_QUOTA] = QVariant::fromValue(m_config.localStorageDefaultQuota());
    m_defaultPageSettings[PAGE_SETTINGS_MAX_DISK_CACHE_SIZE] = QVariant::fromValue(m_config.maxDiskCacheSize());
    m_defaultPageSettings[PAGE_SETTINGS_DISK_CACHE_ENABLED] = QVariant::fromValue(m_config.diskCacheEnabled());
    m_defaultPageSettings[PAGE_SETTINGS_DISK_CACHE_PATH] = QVariant::fromValue(m_config.diskCachePath());
    m_defaultPageSettings[PAGE_SETTINGS_SSL_PROTOCOL] = QVariant::fromValue(m_config.sslProtocol());
    m_defaultPageSettings[PAGE_SETTINGS_SSL_CIPHERS] = QVariant::fromValue(m_config.sslCiphers());
    m_defaultPageSettings[PAGE_SETTINGS_SSL_CERTIFICATES_PATH] = QVariant::fromValue(m_config.sslCertificatesPath());
    m_defaultPageSettings[PAGE_SETTINGS_SSL_CLIENT_CERTIFICATE_FILE] = QVariant::fromValue(m_config.sslClientCertificateFile());
    m_defaultPageSettings[PAGE_SETTINGS_SSL_CLIENT_KEY_FILE] = QVariant::fromValue(m_config.sslClientKeyFile());
    m_defaultPageSettings[PAGE_SETTINGS_SSL_CLIENT_KEY_PASSPHRASE] = QVariant::fromValue(m_config.sslClientKeyPassphrase());

    // Apply default settings to the initial page
    m_page->applySettings(m_defaultPageSettings);

    setLibraryPath(QFileInfo(m_config.scriptFile()).dir().absolutePath());
}

// public:
Phantom* Phantom::instance()
{
    if (!phantomInstance) {
        phantomInstance = new Phantom();
        phantomInstance->init();
    }
    return phantomInstance;
}

Phantom::~Phantom()
{
    // Nothing to do: cleanup is handled by QObject relationships
    // and doExit() should have been called
}

QVariantMap Phantom::defaultPageSettings() const { return m_defaultPageSettings; }

QString Phantom::outputEncoding() const { return Terminal::instance()->getEncoding(); }

void Phantom::setOutputEncoding(const QString& encoding) { Terminal::instance()->setEncoding(encoding); }

bool Phantom::execute()
{
    if (m_terminated) {
        return false;
    }

#ifndef QT_NO_DEBUG_OUTPUT
    qDebug() << "Phantom - execute: Configuration";
    const QMetaObject* configMetaObj = m_config.metaObject();
    for (int i = 0, ilen = configMetaObj->propertyCount(); i < ilen; ++i) {
        qDebug() << "    " << i << configMetaObj->property(i).name() << ":"
                 << m_config.property(configMetaObj->property(i).name()).toString();
    }

    qDebug() << "Phantom - execute: Script & Arguments";
    qDebug() << "    " << "script:" << m_config.scriptFile();
    QStringList args = m_config.scriptArgs();
    for (int i = 0, ilen = args.length(); i < ilen; ++i) {
        qDebug() << "    " << i << "arg:" << args.at(i);
    }
#endif

    if (m_config.scriptFile().isEmpty()) { // REPL mode requested
        qDebug() << "Phantom - execute: Starting REPL mode";
        // REPL class needs to be adapted to work directly with WebPage, not QWebFrame
        REPL::getInstance(m_page, this);
    } else { // Load the User Script
        qDebug() << "Phantom - execute: Starting normal mode";

        if (m_config.debug()) {
            // Debug enabled
            int originalPort = m_config.remoteDebugPort();
            // Call WebPage's showInspector, which delegates to IEngineBackend
            m_config.setRemoteDebugPort(m_page->showInspector(m_config.remoteDebugPort()));

            if (m_config.remoteDebugPort() == 0) {
                qWarning() << "Can't bind remote debugging server to the port" << originalPort;
            }
            // Utils::loadJSForDebug needs to be updated to accept WebPage*
            if (!Utils::loadJSForDebug(m_config.scriptFile(), m_scriptFileEnc, QDir::currentPath(), m_page,
                                       m_config.remoteDebugAutorun())) {
                m_returnValue = -1;
                return false;
            }
        } else {
            // Utils::injectJsInFrame needs to be updated to accept WebPage*
            if (!Utils::injectJsInFrame(
                    m_config.scriptFile(), m_scriptFileEnc, QDir::currentPath(), m_page, true)) {
                m_returnValue = -1;
                return false;
            }
        }
    }

    return !m_terminated;
}

int Phantom::returnValue() const { return m_returnValue; }

QString Phantom::libraryPath() const { return m_page->libraryPath(); }

void Phantom::setLibraryPath(const QString& libraryPath) { m_page->setLibraryPath(libraryPath); }

QVariantMap Phantom::version() const
{
    QVariantMap result;
    result["major"] = PHANTOMJS_VERSION_MAJOR;
    result["minor"] = PHANTOMJS_VERSION_MINOR;
    result["patch"] = PHANTOMJS_VERSION_PATCH;
    return result;
}

QObject* Phantom::page() const { return m_page; }

Config* Phantom::config() { return &m_config; }

bool Phantom::printDebugMessages() const { return m_config.printDebugMessages(); }

bool Phantom::areCookiesEnabled() const { return m_defaultCookieJar->isEnabled(); }

void Phantom::setCookiesEnabled(const bool value)
{
    if (value) {
        m_defaultCookieJar->enable();
    } else {
        m_defaultCookieJar->disable();
    }
}

// Added this new method for WebPage::pages()
const QList<QPointer<WebPage>>& Phantom::allPages() const {
    return m_pages;
}


// public slots:
QObject* Phantom::createCookieJar(const QString& filePath) { return new CookieJar(filePath, this); }

QObject* Phantom::createWebPage()
{
    WebPage* page = new WebPage(this);
    page->setCookieJar(m_defaultCookieJar);

    // Store pointer to the page for later cleanup
    m_pages.append(page);
    // Apply default settings to the page
    page->applySettings(m_defaultPageSettings);

    // Show web-inspector if in debug mode
    if (m_config.debug()) {
        page->showInspector(m_config.remoteDebugPort());
    }

    return page;
}

QObject* Phantom::createWebServer()
{
    WebServer* server = new WebServer(this);
    m_servers.append(server);
    return server;
}

QObject* Phantom::createFilesystem()
{
    if (!m_filesystem) {
        m_filesystem = new FileSystem(this);
    }

    return m_filesystem;
}

QObject* Phantom::createSystem()
{
    if (!m_system) {
        m_system = new System(this);

        QStringList systemArgs;
        systemArgs += m_config.scriptFile();
        systemArgs += m_config.scriptArgs();
        m_system->setArgs(systemArgs);
    }

    return m_system;
}

QObject* Phantom::_createChildProcess()
{
    if (!m_childprocess) {
        m_childprocess = new ChildProcess(this);
    }

    return m_childprocess;
}

QObject* Phantom::createCallback() { return new Callback(this); }

void Phantom::loadModule(const QString& moduleSource, const QString& filename)
{
    if (m_terminated) {
        return;
    }

    QString scriptSource = "(function(require, exports, module) {\n" + moduleSource + "\n}.call({}," + "require.cache['"
                           + filename + "']._getRequire()," + "require.cache['" + filename + "'].exports," + "require.cache['" + filename
                           + "']" + "));";
    // Now delegates to WebPage, which uses IEngineBackend
    m_page->evaluateJavaScript(scriptSource);
}

bool Phantom::injectJs(const QString& jsFilePath)
{
    QString pre = "";
    qDebug() << "Phantom - injectJs:" << jsFilePath;

    if (m_terminated) {
        return false;
    }

    // Now delegates to WebPage, which uses IEngineBackend
    return m_page->injectJs(pre + jsFilePath);
}

void Phantom::setProxy(
    const QString& ip, const qint64& port, const QString& proxyType, const QString& user, const QString& password)
{
    qDebug() << "Set " << proxyType << " proxy to: " << ip << ":" << port;
    QNetworkProxy::ProxyType networkProxyType = QNetworkProxy::HttpProxy;
    if (proxyType == "socks5") {
        networkProxyType = QNetworkProxy::Socks5Proxy;
    } else if (proxyType == "none") {
        // Handle "none" explicitly as QNetworkProxy::NoProxy
        networkProxyType = QNetworkProxy::NoProxy;
    }

    QNetworkProxy proxy(networkProxyType, ip, port, user, password);
    QNetworkProxy::setApplicationProxy(proxy); // Still set global Qt proxy for now

    // Also inform the default page's backend about the proxy
    if (m_page) {
        m_page->setProxy(proxy.toUrl().toString()); // Use WebPage's setProxy
    }
}

QString Phantom::proxy()
{
    QNetworkProxy proxy = QNetworkProxy::applicationProxy();
    if (proxy.hostName().isEmpty()) {
        return QString();
    }
    // Return the string representation of the proxy set
    QString proxyString = proxy.hostName() + ":" + QString::number(proxy.port());
    if (!proxy.user().isEmpty()) {
        proxyString = proxy.user() + ":" + proxy.password() + "@" + proxyString;
    }
    return proxy.type() == QNetworkProxy::Socks5Proxy ? "socks5://" + proxyString : "http://" + proxyString;
}

int Phantom::remoteDebugPort() const { return m_config.remoteDebugPort(); }

void Phantom::exit(int code)
{
    if (m_config.debug()) {
        Terminal::instance()->cout("Phantom::exit() called but not quitting in debug mode.");
    } else {
        doExit(code);
    }
}

void Phantom::debugExit(int code) { doExit(code); }

QString Phantom::resolveRelativeUrl(QString url, QString base)
{
    QUrl u = QUrl::fromEncoded(url.toLatin1());
    QUrl b = QUrl::fromEncoded(base.toLatin1());

    return b.resolved(u).toEncoded();
}

QString Phantom::fullyDecodeUrl(QString url) { return QUrl::fromEncoded(url.toLatin1()).toDisplayString(); }

// private slots:
void Phantom::printConsoleMessage(const QString& message) { Terminal::instance()->cout(message); }

void Phantom::onInitialized()
{
    // This slot is triggered when the IEngineBackend (via WebPage) is ready
    // to have JS objects exposed.
    qDebug() << "Phantom - onInitialized: Exposing 'phantom' object and loading bootstrap.js";

    // Expose the 'phantom' QObject to the JavaScript context via the IEngineBackend
    // This will instruct the Playwright backend to set up `window.phantom`
    // and map its methods/properties.
    if (m_page && m_page->engineBackend()) { // Ensure backend is available
        m_page->engineBackend()->exposeQObject("phantom", this);
        // Then, load the bootstrap.js. This script will now expect 'window.phantom'
        // to be available through the Playwright exposure mechanism.
        // It will call evaluateJavaScript on the page which delegates to the backend.
        m_page->evaluateJavaScript(Utils::readResourceFileUtf8(":/bootstrap.js"));
    } else {
        qWarning() << "Phantom - onInitialized: WebPage or its backend not ready for JavaScript object exposure.";
    }
}

bool Phantom::setCookies(const QVariantList& cookies)
{
    // Delete all the cookies from the CookieJar
    m_defaultCookieJar->clearCookies();
    // Add a new set of cookies
    bool success = m_defaultCookieJar->addCookiesFromMap(cookies);
    // Also instruct the default page's backend to set these cookies globally
    if (success && m_page) {
        m_page->setCookies(cookies);
    }
    return success;
}

QVariantList Phantom::cookies() const
{
    // Return all the Cookies in the CookieJar, as a list of Maps
    // For consistency, consider getting this from the default page's backend.
    // For now, returning from local CookieJar.
    // return m_page->cookies(); // This would delegate to backend
    return m_defaultCookieJar->cookiesToMap();
}

bool Phantom::addCookie(const QVariantMap& cookie)
{
    bool success = m_defaultCookieJar->addCookieFromMap(cookie);
    if (success && m_page) {
        m_page->addCookie(cookie);
    }
    return success;
}

bool Phantom::deleteCookie(const QString& cookieName)
{
    if (!cookieName.isEmpty()) {
        bool success = m_defaultCookieJar->deleteCookie(cookieName);
        if (success && m_page) {
            m_page->deleteCookie(cookieName);
        }
        return success;
    }
    return false;
}

void Phantom::clearCookies()
{
    m_defaultCookieJar->clearCookies();
    if (m_page) {
        m_page->clearCookies();
    }
}

// private:
void Phantom::doExit(int code)
{
    emit aboutToExit(code);
    m_terminated = true;
    m_returnValue = code;

    // Iterate in reverse order so the first page is the last one scheduled for
    // deletion. This should help prevent issues with root page invalidation.
    QListIterator<QPointer<WebPage>> i(m_pages);
    i.toBack();
    while (i.hasPrevious()) {
        const QPointer<WebPage> page = i.previous();

        if (!page) {
            continue;
        }

        // stop processing of JavaScript code by loading a blank page
        // Delegating to WebPage's setContent which uses IEngineBackend
        page->setContent(QStringLiteral("about:blank"));
        // delay deletion into the event loop, direct deletion can trigger crashes
        page->deleteLater();
    }
    m_pages.clear();
    m_page = 0; // Clear default page pointer

    // --- Critical: Ensure Playwright backend processes are killed ---
    // This is handled by WebPage's destructor (due to parentage)
    // and PlaywrightEngineBackend's destructor which sends a shutdown command.
    // However, if the QApplication is exiting, make sure all child processes are terminated.
    // QProcess objects (like in PlaywrightEngineBackend) are usually stopped gracefully on QObject parent destruction.
    // Explicitly calling close() on all WebPage objects would trigger their destructors.
    QApplication::instance()->exit(code);
}

// Private helper to get IEngineBackend from WebPage
// This is added to phantom.h as a friend function or similar if needed externally.
// It is useful during development to confirm type.
IEngineBackend* WebPage::engineBackend() const {
    return m_engineBackend;
}
