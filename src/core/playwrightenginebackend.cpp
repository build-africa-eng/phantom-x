#include "playwrightenginebackend.h"
#include "cookiejar.h" // Include if CookieJar methods are directly used or passed around
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonArray>
#include <QEventLoop> // For waitForStarted/waitForFinished/waitForBytesWritten
#include <QTimer>
#include <QNetworkProxy>
#include <QUrlQuery> // For parsing URL components if needed

// Constructor
PlaywrightEngineBackend::PlaywrightEngineBackend(QObject* parent)
    : IEngineBackend(parent)
    , m_playwrightProcess(nullptr)
    , m_nextRequestId(1) {
    // Initialize default cached values
    m_currentUrl = QUrl("about:blank");
    m_currentTitle = "";
    m_currentHtml = "";
    m_currentPlainText = "";
    m_currentWindowName = "";
    m_currentViewportSize = QSize(400, 300); // Default viewport size
    m_currentClipRect = QRect(0, 0, 0, 0); // Default clipRect
    m_currentScrollPosition = QPoint(0, 0);
    m_currentUserAgent = "";
    m_currentCustomHeaders = QVariantMap();
    m_currentZoomFactor = 1.0;
    m_currentNavigationLocked = false;
    m_currentLocalStoragePath = "";
    m_currentLocalStorageQuota = 0;
    m_currentOfflineStoragePath = "";
    m_currentOfflineStorageQuota = 0;
    m_currentFramesCount = 0;
    m_currentFramesName = QStringList();
    m_currentFrameName = "";
    m_currentFocusedFrameName = "";
    m_currentCookies = QVariantList();

    // Determine the path to the Node.js backend script
    QString appDirPath = QCoreApplication::applicationDirPath();
    m_playwrightScriptPath
        = appDirPath + "/playwright_backend.js"; // Adjust as needed, e.g., to build/playwright_backend.js

    // Check if the script exists
    if (!QFile::exists(m_playwrightScriptPath)) {
        qWarning() << "PlaywrightEngineBackend: Backend script not found at:" << m_playwrightScriptPath;
        // Consider emitting initialized(false) or a different error signal
        // For now, proceed, but process will likely fail to start
    }

    // Start the Playwright Node.js process
    m_playwrightProcess = new QProcess(this);
    m_playwrightProcess->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_playwrightProcess, &QProcess::started, this, &PlaywrightEngineBackend::handleProcessStarted);
    connect(m_playwrightProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        &PlaywrightEngineBackend::handleProcessFinished);
    connect(m_playwrightProcess, &QProcess::readyReadStandardOutput, this,
        &PlaywrightEngineBackend::handleReadyReadStandardOutput);
    connect(m_playwrightProcess, &QProcess::readyReadStandardError, this,
        &PlaywrightEngineBackend::handleReadyReadStandardError);
    connect(m_playwrightProcess, &QProcess::errorOccurred, this, &PlaywrightEngineBackend::handleProcessErrorOccurred);

    qDebug() << "PlaywrightEngineBackend: Starting Node.js process:" << m_playwrightScriptPath;
    m_playwrightProcess->start("node", QStringList() << m_playwrightScriptPath);

    if (!m_playwrightProcess->waitForStarted(5000)) { // Wait up to 5 seconds for the process to start
        qCritical() << "PlaywrightEngineBackend: Failed to start Node.js backend process.";
        emitInitialized(); // Emit initialized even on failure for now to unblock.
    } else {
        qDebug() << "PlaywrightEngineBackend: Node.js process started.";
    }
}

// Destructor
PlaywrightEngineBackend::~PlaywrightEngineBackend() {
    if (m_playwrightProcess && m_playwrightProcess->state() == QProcess::Running) {
        qDebug() << "PlaywrightEngineBackend: Terminating Playwright Node.js process.";
        m_playwrightProcess->terminate();
        if (!m_playwrightProcess->waitForFinished(3000)) {
            m_playwrightProcess->kill();
        }
    }
}

// --- IEngineBackend overrides (Implementations) ---

QUrl PlaywrightEngineBackend::url() const {
    QVariant result = const_cast<PlaywrightEngineBackend*>(this)->sendSyncCommand("getUrl");
    if (result.isValid() && result.type() == QVariant::String) {
        m_currentUrl = QUrl(result.toString());
    }
    return m_currentUrl;
}

QString PlaywrightEngineBackend::title() const {
    QVariant result = const_cast<PlaywrightEngineBackend*>(this)->sendSyncCommand("getTitle");
    if (result.isValid() && result.type() == QVariant::String) {
        m_currentTitle = result.toString();
    }
    return m_currentTitle;
}

QString PlaywrightEngineBackend::toHtml() const {
    QVariant result = const_cast<PlaywrightEngineBackend*>(this)->sendSyncCommand("getHtml");
    if (result.isValid() && result.type() == QVariant::String) {
        m_currentHtml = result.toString();
    }
    return m_currentHtml;
}

QString PlaywrightEngineBackend::toPlainText() const {
    QVariant result = const_cast<PlaywrightEngineBackend*>(this)->sendSyncCommand("getPlainText");
    if (result.isValid() && result.type() == QVariant::String) {
        m_currentPlainText = result.toString();
    }
    return m_currentPlainText;
}

QString PlaywrightEngineBackend::windowName() const {
    QVariant result = const_cast<PlaywrightEngineBackend*>(this)->sendSyncCommand("getWindowName");
    if (result.isValid() && result.type() == QVariant::String) {
        m_currentWindowName = result.toString();
    }
    return m_currentWindowName;
}

void PlaywrightEngineBackend::load(
    const QNetworkRequest& request, QNetworkAccessManager::Operation operation, const QByteArray& body) {
    qDebug() << "PlaywrightEngineBackend: Loading URL:" << request.url().toString();
    QVariantMap params;
    params["url"] = request.url().toString();
    // QNetworkAccessManager::Operation to string mapping (simplified)
    QString operationString;
    switch (operation) {
    case QNetworkAccessManager::GetOperation:
        operationString = "GET";
        break;
    case QNetworkAccessManager::PostOperation:
        operationString = "POST";
        break;
    case QNetworkAccessManager::PutOperation:
        operationString = "PUT";
        break;
    case QNetworkAccessManager::DeleteOperation:
        operationString = "DELETE";
        break;
    // Add other operations as needed
    default:
        operationString = "GET";
        break;
    }
    params["method"] = operationString;
    params["body"] = QString::fromUtf8(body.toBase64()); // Send body as base64 string

    // Convert raw headers to a QVariantMap
    QVariantMap rawHeadersMap;
    for (const QNetworkRequest::RawHeaderPair& headerPair : request.rawHeaderList()) {
        rawHeadersMap[QString::fromUtf8(headerPair.first)] = QString::fromUtf8(headerPair.second);
    }
    params["headers"] = rawHeadersMap;

    sendAsyncCommand("load", params);
}

void PlaywrightEngineBackend::setHtml(const QString& html, const QUrl& baseUrl) {
    qDebug() << "PlaywrightEngineBackend: Setting HTML content.";
    QVariantMap params;
    params["html"] = html;
    params["baseUrl"] = baseUrl.toString();
    sendAsyncCommand("setHtml", params);
}

void PlaywrightEngineBackend::reload() {
    qDebug() << "PlaywrightEngineBackend: Reloading page.";
    sendAsyncCommand("reload");
}

void PlaywrightEngineBackend::stop() {
    qDebug() << "PlaywrightEngineBackend: Stopping page load.";
    sendAsyncCommand("stop");
}

bool PlaywrightEngineBackend::canGoBack() const {
    QVariant result = const_cast<PlaywrightEngineBackend*>(this)->sendSyncCommand("canGoBack");
    return result.toBool();
}

bool PlaywrightEngineBackend::goBack() {
    QVariant result = sendSyncCommand("goBack");
    return result.toBool();
}

bool PlaywrightEngineBackend::canGoForward() const {
    QVariant result = const_cast<PlaywrightEngineBackend*>(this)->sendSyncCommand("canGoForward");
    return result.toBool();
}

bool PlaywrightEngineBackend::goForward() {
    QVariant result = sendSyncCommand("goForward");
    return result.toBool();
}

bool PlaywrightEngineBackend::goToHistoryItem(int relativeIndex) {
    qDebug() << "PlaywrightEngineBackend: goToHistoryItem called. Index:" << relativeIndex;
    QVariantMap params; // Create a mutable copy
    params["relativeIndex"] = relativeIndex;
    QVariant result = sendSyncCommand("goToHistoryItem", params);
    return result.toBool();
}

void PlaywrightEngineBackend::setViewportSize(const QSize& size) {
    qDebug() << "PlaywrightEngineBackend: Setting viewport size:" << size;
    m_currentViewportSize = size; // Cache locally
    QVariantMap params;
    params["width"] = size.width();
    params["height"] = size.height();
    sendAsyncCommand("setViewportSize", params);
}

QSize PlaywrightEngineBackend::viewportSize() const {
    QVariant result = const_cast<PlaywrightEngineBackend*>(this)->sendSyncCommand("getViewportSize");
    if (result.isValid() && result.type() == QVariant::Map) {
        QVariantMap sizeMap = result.toMap();
        m_currentViewportSize = QSize(sizeMap.value("width").toInt(), sizeMap.value("height").toInt());
    }
    return m_currentViewportSize;
}

void PlaywrightEngineBackend::setClipRect(const QRect& rect) {
    qDebug() << "PlaywrightEngineBackend: Setting clip rect:" << rect;
    m_currentClipRect = rect; // Cache locally
    QVariantMap params;
    params["x"] = rect.x();
    params["y"] = rect.y();
    params["width"] = rect.width();
    params["height"] = rect.height();
    sendAsyncCommand("setClipRect", params);
}

QRect PlaywrightEngineBackend::clipRect() const {
    QVariant result = const_cast<PlaywrightEngineBackend*>(this)->sendSyncCommand("getClipRect");
    if (result.isValid() && result.type() == QVariant::Map) {
        QVariantMap rectMap = result.toMap();
        m_currentClipRect = QRect(rectMap.value("x").toInt(), rectMap.value("y").toInt(),
            rectMap.value("width").toInt(), rectMap.value("height").toInt());
    }
    return m_currentClipRect;
}

void PlaywrightEngineBackend::setScrollPosition(const QPoint& pos) {
    qDebug() << "PlaywrightEngineBackend: Setting scroll position:" << pos;
    m_currentScrollPosition = pos; // Cache locally
    QVariantMap params;
    params["x"] = pos.x();
    params["y"] = pos.y();
    sendAsyncCommand("setScrollPosition", params);
}

QPoint PlaywrightEngineBackend::scrollPosition() const {
    QVariant result = const_cast<PlaywrightEngineBackend*>(this)->sendSyncCommand("getScrollPosition");
    if (result.isValid() && result.type() == QVariant::Map) {
        QVariantMap posMap = result.toMap();
        m_currentScrollPosition = QPoint(posMap.value("x").toInt(), posMap.value("y").toInt());
    }
    return m_currentScrollPosition;
}

QByteArray PlaywrightEngineBackend::renderPdf(const QVariantMap& paperSize, const QRect& clipRect) {
    qDebug() << "PlaywrightEngineBackend: Rendering PDF.";
    QVariantMap params;
    params["format"] = "pdf";
    params["paperSize"] = paperSize;
    params["clipRect"] = QVariantMap { { "x", clipRect.x() }, { "y", clipRect.y() }, { "width", clipRect.width() },
        { "height", clipRect.height() } };
    QVariant result = sendSyncCommand("render", params);
    if (result.isValid() && result.type() == QVariant::String) {
        return QByteArray::fromBase64(result.toByteArray()); // Expecting base64 encoded PDF
    }
    qWarning() << "PlaywrightEngineBackend: PDF rendering failed or returned invalid data.";
    return QByteArray();
}

QByteArray PlaywrightEngineBackend::renderImage(
    const QRect& clipRect, bool onlyViewport, const QPoint& scrollPosition) {
    qDebug() << "PlaywrightEngineBackend: Rendering Image (PNG/JPEG).";
    QVariantMap params;
    params["format"] = "png"; // Default to PNG, could be parameterized
    params["clipRect"] = QVariantMap { { "x", clipRect.x() }, { "y", clipRect.y() }, { "width", clipRect.width() },
        { "height", clipRect.height() } };
    params["onlyViewport"] = onlyViewport;
    params["scrollPosition"] = QVariantMap { { "x", scrollPosition.x() }, { "y", scrollPosition.y() } };

    QVariant result = sendSyncCommand("render", params);
    if (result.isValid() && result.type() == QVariant::String) {
        return QByteArray::fromBase64(result.toByteArray()); // Expecting base64 encoded image
    }
    qWarning() << "PlaywrightEngineBackend: Image rendering failed or returned invalid data.";
    return QByteArray();
}

qreal PlaywrightEngineBackend::zoomFactor() const {
    QVariant result = const_cast<PlaywrightEngineBackend*>(this)->sendSyncCommand("getZoomFactor");
    if (result.isValid() && result.type() == QVariant::Double) {
        m_currentZoomFactor = result.toReal();
    }
    return m_currentZoomFactor;
}

void PlaywrightEngineBackend::setZoomFactor(qreal zoom) {
    qDebug() << "PlaywrightEngineBackend: Setting zoom factor:" << zoom;
    m_currentZoomFactor = zoom; // Cache locally
    QVariantMap params;
    params["zoom"] = zoom;
    sendAsyncCommand("setZoomFactor", params);
}

QVariant PlaywrightEngineBackend::evaluateJavaScript(const QString& code) {
    qDebug() << "PlaywrightEngineBackend: Evaluating JavaScript.";
    QVariantMap params;
    params["code"] = code;
    return sendSyncCommand("evaluateJavaScript", params);
}

bool PlaywrightEngineBackend::injectJavaScriptFile(
    const QString& jsFilePath, const QString& encoding, const QString& libraryPath, bool forEachFrame) {
    qDebug() << "PlaywrightEngineBackend: Injecting JavaScript file:" << jsFilePath;
    QVariantMap params;
    params["path"] = jsFilePath;
    params["encoding"] = encoding;
    params["libraryPath"] = libraryPath; // Might not be needed by Playwright directly
    params["forEachFrame"] = forEachFrame;
    QVariant result = sendSyncCommand("injectJavaScriptFile", params);
    return result.toBool();
}

void PlaywrightEngineBackend::exposeQObject(const QString& name, QObject* object) {
    qDebug() << "PlaywrightEngineBackend: Exposing QObject (stub):" << name;
    // Playwright doesn't directly support QObject exposure like QtWebEngine.
    // This would typically involve serializing QObject method calls/signals to Node.js
    // and back, which is complex. For now, it's a stub.
    // If specific functionality is needed, it should be mapped to Playwright actions.
    QVariantMap params;
    params["name"] = name;
    // No actual object is sent, as we can't serialize QObjects to JSON directly
    sendAsyncCommand("exposeQObject", params); // This just tells Playwright the object exists
}

void PlaywrightEngineBackend::appendScriptElement(const QString& scriptUrl) {
    qDebug() << "PlaywrightEngineBackend: Appending script element:" << scriptUrl;
    QVariantMap params;
    params["url"] = scriptUrl;
    sendAsyncCommand("appendScriptElement", params);
}

QString PlaywrightEngineBackend::userAgent() const {
    QVariant result = const_cast<PlaywrightEngineBackend*>(this)->sendSyncCommand("getUserAgent");
    if (result.isValid() && result.type() == QVariant::String) {
        m_currentUserAgent = result.toString();
    }
    return m_currentUserAgent;
}

void PlaywrightEngineBackend::setUserAgent(const QString& ua) {
    qDebug() << "PlaywrightEngineBackend: Setting user agent:" << ua;
    m_currentUserAgent = ua; // Cache locally
    QVariantMap params;
    params["userAgent"] = ua;
    sendAsyncCommand("setUserAgent", params);
}

void PlaywrightEngineBackend::setNavigationLocked(bool lock) {
    qDebug() << "PlaywrightEngineBackend: Setting navigation locked:" << lock;
    m_currentNavigationLocked = lock; // Cache locally
    QVariantMap params;
    params["locked"] = lock;
    sendAsyncCommand("setNavigationLocked", params);
}

bool PlaywrightEngineBackend::navigationLocked() const {
    QVariant result = const_cast<PlaywrightEngineBackend*>(this)->sendSyncCommand("getNavigationLocked");
    if (result.isValid() && result.type() == QVariant::Bool) {
        m_currentNavigationLocked = result.toBool();
    }
    return m_currentNavigationLocked;
}

QVariantMap PlaywrightEngineBackend::customHeaders() const {
    QVariant result = const_cast<PlaywrightEngineBackend*>(this)->sendSyncCommand("getCustomHeaders");
    if (result.isValid() && result.type() == QVariant::Map) {
        m_currentCustomHeaders = result.toMap();
    }
    return m_currentCustomHeaders;
}

void PlaywrightEngineBackend::setCustomHeaders(const QVariantMap& headers) {
    qDebug() << "PlaywrightEngineBackend: Setting custom headers.";
    m_currentCustomHeaders = headers; // Cache locally
    QVariantMap params;
    params["headers"] = headers;
    sendAsyncCommand("setCustomHeaders", params);
}

void PlaywrightEngineBackend::applySettings(const QVariantMap& settings) {
    qDebug() << "PlaywrightEngineBackend: Applying settings.";
    // Iterate through settings and apply them to Playwright backend
    // This is a high-level function, mapping generic settings to specific backend calls
    if (settings.contains("userAgent")) {
        setUserAgent(settings["userAgent"].toString());
    }
    if (settings.contains("viewportSize")) {
        QVariantMap sizeMap = settings["viewportSize"].toMap();
        setViewportSize(QSize(sizeMap.value("width").toInt(), sizeMap.value("height").toInt()));
    }
    if (settings.contains("clipRect")) {
        QVariantMap rectMap = settings["clipRect"].toMap();
        setClipRect(QRect(rectMap.value("x").toInt(), rectMap.value("y").toInt(), rectMap.value("width").toInt(),
            rectMap.value("height").toInt()));
    }
    if (settings.contains("scrollPosition")) {
        QVariantMap posMap = settings["scrollPosition"].toMap();
        setScrollPosition(QPoint(posMap.value("x").toInt(), posMap.value("y").toInt()));
    }
    if (settings.contains("zoomFactor")) {
        setZoomFactor(settings["zoomFactor"].toReal());
    }
    if (settings.contains("customHeaders")) {
        setCustomHeaders(settings["customHeaders"].toMap());
    }
    if (settings.contains("navigationLocked")) {
        setNavigationLocked(settings["navigationLocked"].toBool());
    }
    // Handle other settings that map directly to IEngineBackend functions
    if (settings.contains("diskCacheEnabled")) {
        setDiskCacheEnabled(settings["diskCacheEnabled"].toBool());
    }
    if (settings.contains("maxDiskCacheSize")) {
        setMaxDiskCacheSize(settings["maxDiskCacheSize"].toInt());
    }
    if (settings.contains("diskCachePath")) {
        setDiskCachePath(settings["diskCachePath"].toString());
    }
    if (settings.contains("ignoreSslErrors")) {
        setIgnoreSslErrors(settings["ignoreSslErrors"].toBool());
    }
    if (settings.contains("sslProtocol")) {
        setSslProtocol(settings["sslProtocol"].toString());
    }
    if (settings.contains("sslCiphers")) {
        setSslCiphers(settings["sslCiphers"].toString());
    }
    if (settings.contains("sslCertificatesPath")) {
        setSslCertificatesPath(settings["sslCertificatesPath"].toString());
    }
    if (settings.contains("sslClientCertificateFile")) {
        setSslClientCertificateFile(settings["sslClientCertificateFile"].toString());
    }
    if (settings.contains("sslClientKeyFile")) {
        setSslClientKeyFile(settings["sslClientKeyFile"].toString());
    }
    if (settings.contains("sslClientKeyPassphrase")) {
        setSslClientKeyPassphrase(settings["sslClientKeyPassphrase"].toByteArray());
    }
    if (settings.contains("resourceTimeout")) {
        setResourceTimeout(settings["resourceTimeout"].toInt());
    }
    if (settings.contains("maxAuthAttempts")) {
        setMaxAuthAttempts(settings["maxAuthAttempts"].toInt());
    }
    if (settings.contains("localStoragePath")) {
        setLocalStoragePath(settings["localStoragePath"].toString());
    }
    if (settings.contains("localStorageQuota")) {
        // Playwright's local storage quota might not be directly controllable as an int
        // This is a stub for now.
        // setLocalStorageQuota(settings["localStorageQuota"].toInt());
        qWarning() << "PlaywrightEngineBackend: localStorageQuota setting is not directly supported.";
    }
    if (settings.contains("offlineStoragePath")) {
        setOfflineStoragePath(settings["offlineStoragePath"].toString());
    }
    if (settings.contains("offlineStorageQuota")) {
        // setOfflineStorageQuota(settings["offlineStorageQuota"].toInt());
        qWarning() << "PlaywrightEngineBackend: offlineStorageQuota setting is not directly supported.";
    }

    // JS-related settings
    if (settings.contains("javascriptEnabled")) {
        QVariantMap jsParams;
        jsParams["enabled"] = settings["javascriptEnabled"].toBool();
        sendAsyncCommand("setJavaScriptEnabled", jsParams);
    }
    if (settings.contains("webSecurityEnabled")) {
        QVariantMap wsParams;
        wsParams["enabled"] = settings["webSecurityEnabled"].toBool();
        sendAsyncCommand("setWebSecurityEnabled", wsParams);
    }
    if (settings.contains("webGLEnabled")) {
        QVariantMap webglParams;
        webglParams["enabled"] = settings["webGLEnabled"].toBool();
        sendAsyncCommand("setWebGLEnabled", webglParams);
    }
    if (settings.contains("javascriptCanOpenWindows")) {
        QVariantMap openWinParams;
        openWinParams["canOpen"] = settings["javascriptCanOpenWindows"].toBool();
        sendAsyncCommand("setJavaScriptCanOpenWindows", openWinParams);
    }
    if (settings.contains("javascriptCanCloseWindows")) {
        QVariantMap closeWinParams;
        closeWinParams["canClose"] = settings["javascriptCanCloseWindows"].toBool();
        sendAsyncCommand("setJavaScriptCanCloseWindows", closeWinParams);
    }
    if (settings.contains("localToRemoteUrlAccessEnabled")) {
        QVariantMap accessParams;
        accessParams["enabled"] = settings["localToRemoteUrlAccessEnabled"].toBool();
        sendAsyncCommand("setLocalToRemoteUrlAccessEnabled", accessParams);
    }
    if (settings.contains("autoLoadImages")) {
        QVariantMap imagesParams;
        imagesParams["autoLoad"] = settings["autoLoadImages"].toBool();
        sendAsyncCommand("setAutoLoadImages", imagesParams);
    }
}

void PlaywrightEngineBackend::setNetworkProxy(const QNetworkProxy& proxy) {
    qDebug() << "PlaywrightEngineBackend: Setting network proxy:" << proxy.hostName() << ":" << proxy.port();
    QVariantMap params;
    params["type"] = proxy.type() == QNetworkProxy::Socks5Proxy ? "socks5" : "http";
    params["host"] = proxy.hostName();
    params["port"] = proxy.port();
    params["user"] = proxy.user();
    params["password"] = proxy.password();
    sendAsyncCommand("setNetworkProxy", params);
}

void PlaywrightEngineBackend::setDiskCacheEnabled(bool enabled) {
    qDebug() << "PlaywrightEngineBackend: Setting disk cache enabled:" << enabled;
    QVariantMap params;
    params["enabled"] = enabled;
    sendAsyncCommand("setDiskCacheEnabled", params);
}

void PlaywrightEngineBackend::setMaxDiskCacheSize(int size) {
    qDebug() << "PlaywrightEngineBackend: Setting max disk cache size:" << size;
    QVariantMap params;
    params["size"] = size;
    sendAsyncCommand("setMaxDiskCacheSize", params);
}

void PlaywrightEngineBackend::setDiskCachePath(const QString& path) {
    qDebug() << "PlaywrightEngineBackend: Setting disk cache path:" << path;
    QVariantMap params;
    params["path"] = path;
    sendAsyncCommand("setDiskCachePath", params);
}

void PlaywrightEngineBackend::setIgnoreSslErrors(bool ignore) {
    qDebug() << "PlaywrightEngineBackend: Setting ignore SSL errors:" << ignore;
    QVariantMap params;
    params["ignore"] = ignore;
    sendAsyncCommand("setIgnoreSslErrors", params);
}

void PlaywrightEngineBackend::setSslProtocol(const QString& protocol) {
    qDebug() << "PlaywrightEngineBackend: Setting SSL protocol:" << protocol;
    QVariantMap params;
    params["protocol"] = protocol;
    sendAsyncCommand("setSslProtocol", params);
}

void PlaywrightEngineBackend::setSslCiphers(const QString& ciphers) {
    qDebug() << "PlaywrightEngineBackend: Setting SSL ciphers:" << ciphers;
    QVariantMap params;
    params["ciphers"] = ciphers;
    sendAsyncCommand("setSslCiphers", params);
}

void PlaywrightEngineBackend::setSslCertificatesPath(const QString& path) {
    qDebug() << "PlaywrightEngineBackend: Setting SSL certificates path:" << path;
    QVariantMap params;
    params["path"] = path;
    sendAsyncCommand("setSslCertificatesPath", params);
}

void PlaywrightEngineBackend::setSslClientCertificateFile(const QString& file) {
    qDebug() << "PlaywrightEngineBackend: Setting SSL client cert file:" << file;
    QVariantMap params;
    params["file"] = file;
    sendAsyncCommand("setSslClientCertificateFile", params);
}

void PlaywrightEngineBackend::setSslClientKeyFile(const QString& file) {
    qDebug() << "PlaywrightEngineBackend: Setting SSL client key file:" << file;
    QVariantMap params;
    params["file"] = file;
    sendAsyncCommand("setSslClientKeyFile", params);
}

void PlaywrightEngineBackend::setSslClientKeyPassphrase(const QByteArray& passphrase) {
    qDebug() << "PlaywrightEngineBackend: Setting SSL client key passphrase (hashed/obscured).";
    QVariantMap params;
    params["passphrase"] = QString::fromUtf8(passphrase.toBase64()); // Send as base64 for safety
    sendAsyncCommand("setSslClientKeyPassphrase", params);
}

void PlaywrightEngineBackend::setResourceTimeout(int timeout) {
    qDebug() << "PlaywrightEngineBackend: Setting resource timeout:" << timeout;
    QVariantMap params;
    params["timeout"] = timeout;
    sendAsyncCommand("setResourceTimeout", params);
}

void PlaywrightEngineBackend::setMaxAuthAttempts(int attempts) {
    qDebug() << "PlaywrightEngineBackend: Setting max auth attempts:" << attempts;
    QVariantMap params;
    params["attempts"] = attempts;
    sendAsyncCommand("setMaxAuthAttempts", params);
}

void PlaywrightEngineBackend::setLocalStoragePath(const QString& path) {
    qDebug() << "PlaywrightEngineBackend: Setting local storage path (stub):" << path;
    m_currentLocalStoragePath = path; // Cache locally
    QVariantMap params;
    params["path"] = path;
    sendAsyncCommand("setLocalStoragePath", params);
}

int PlaywrightEngineBackend::localStorageQuota() const {
    qDebug() << "PlaywrightEngineBackend: localStorageQuota called (stub).";
    QVariant result = const_cast<PlaywrightEngineBackend*>(this)->sendSyncCommand("getLocalStorageQuota");
    if (result.isValid() && result.type() == QVariant::Int) {
        m_currentLocalStorageQuota = result.toInt();
    }
    return m_currentLocalStorageQuota;
}

void PlaywrightEngineBackend::setOfflineStoragePath(const QString& path) {
    qDebug() << "PlaywrightEngineBackend: Setting offline storage path (stub):" << path;
    m_currentOfflineStoragePath = path; // Cache locally
    QVariantMap params;
    params["path"] = path;
    sendAsyncCommand("setOfflineStoragePath", params);
}

int PlaywrightEngineBackend::offlineStorageQuota() const {
    qDebug() << "PlaywrightEngineBackend: offlineStorageQuota called (stub).";
    QVariant result = const_cast<PlaywrightEngineBackend*>(this)->sendSyncCommand("getOfflineStorageQuota");
    if (result.isValid() && result.type() == QVariant::Int) {
        m_currentOfflineStorageQuota = result.toInt();
    }
    return m_currentOfflineStorageQuota;
}

void PlaywrightEngineBackend::clearMemoryCache() {
    qDebug() << "PlaywrightEngineBackend: Clearing memory cache.";
    sendAsyncCommand("clearMemoryCache");
}

void PlaywrightEngineBackend::setCookieJar(CookieJar* cookieJar) {
    qDebug() << "PlaywrightEngineBackend: Setting cookie jar (stub).";
    // This is complex. The CookieJar needs to be synchronized with Playwright's cookie management.
    // For now, it's a stub. Actual implementation would involve sending/receiving cookies to/from Playwright.
    QVariantMap params;
    // You might pass a reference/ID or a path to a cookie file if Playwright supports it.
    sendAsyncCommand("setCookieJar", params);
}

bool PlaywrightEngineBackend::setCookies(const QVariantList& cookies) {
    qDebug() << "PlaywrightEngineBackend: Setting cookies.";
    QVariantMap params;
    params["cookies"] = cookies;
    QVariant result = sendSyncCommand("setCookies", params);
    return result.toBool();
}

QVariantList PlaywrightEngineBackend::cookies() const {
    qDebug() << "PlaywrightEngineBackend: Getting cookies.";
    QVariant result = const_cast<PlaywrightEngineBackend*>(this)->sendSyncCommand("getCookies");
    if (result.isValid() && result.type() == QVariant::List) {
        m_currentCookies = result.toList();
    }
    return m_currentCookies;
}

bool PlaywrightEngineBackend::addCookie(const QVariantMap& cookie) {
    qDebug() << "PlaywrightEngineBackend: Adding cookie.";
    QVariantMap params;
    params["cookie"] = cookie;
    QVariant result = sendSyncCommand("addCookie", params);
    return result.toBool();
}

bool PlaywrightEngineBackend::deleteCookie(const QString& cookieName) {
    qDebug() << "PlaywrightEngineBackend: Deleting cookie:" << cookieName;
    QVariantMap params;
    params["name"] = cookieName;
    QVariant result = sendSyncCommand("deleteCookie", params);
    return result.toBool();
}

void PlaywrightEngineBackend::clearCookies() { // Changed to void
    qDebug() << "PlaywrightEngineBackend: Clearing cookies.";
    sendAsyncCommand("clearCookies");
}

int PlaywrightEngineBackend::framesCount() const {
    qDebug() << "PlaywrightEngineBackend: framesCount called (stub).";
    QVariant result = const_cast<PlaywrightEngineBackend*>(this)->sendSyncCommand("getFramesCount");
    if (result.isValid() && result.type() == QVariant::Int) {
        m_currentFramesCount = result.toInt();
    }
    return m_currentFramesCount;
}

QStringList PlaywrightEngineBackend::framesName() const {
    qDebug() << "PlaywrightEngineBackend: framesName called (stub).";
    QVariant result = const_cast<PlaywrightEngineBackend*>(this)->sendSyncCommand("getFramesName");
    if (result.isValid() && result.type() == QVariant::List) {
        m_currentFramesName.clear();
        for (const QVariant& item : result.toList()) {
            if (item.type() == QVariant::String) {
                m_currentFramesName.append(item.toString());
            }
        }
    }
    return m_currentFramesName;
}

bool PlaywrightEngineBackend::switchToFrame(const QString& frameName) {
    qDebug() << "PlaywrightEngineBackend: switchToFrame by name called. Name:" << frameName;
    QVariantMap params;
    params["name"] = frameName;
    QVariant result = sendSyncCommand("switchToFrameByName", params);
    if (result.toBool()) {
        m_currentFrameName = frameName;
    }
    return result.toBool();
}

bool PlaywrightEngineBackend::switchToFrame(int framePosition) {
    qDebug() << "PlaywrightEngineBackend: switchToFrame by position called. Position:" << framePosition;
    QVariantMap params;
    params["position"] = framePosition;
    QVariant result = sendSyncCommand("switchToFrameByPosition", params);
    if (result.toBool()) {
        // Need to fetch actual frame name after switching
        m_currentFrameName = sendSyncCommand("getFrameName").toString();
    }
    return result.toBool();
}

void PlaywrightEngineBackend::switchToMainFrame() {
    qDebug() << "PlaywrightEngineBackend: switchToMainFrame called.";
    sendAsyncCommand("switchToMainFrame");
    m_currentFrameName = ""; // Main frame has no specific name usually
}

bool PlaywrightEngineBackend::switchToParentFrame() {
    qDebug() << "PlaywrightEngineBackend: switchToParentFrame called.";
    QVariant result = sendSyncCommand("switchToParentFrame");
    if (result.toBool()) {
        m_currentFrameName = sendSyncCommand("getFrameName").toString();
    }
    return result.toBool();
}

bool PlaywrightEngineBackend::switchToFocusedFrame() { // Changed to bool
    qDebug() << "PlaywrightEngineBackend: switchToFocusedFrame called.";
    QVariant result = sendSyncCommand("switchToFocusedFrame");
    if (result.toBool()) {
        m_currentFocusedFrameName = sendSyncCommand("getFocusedFrameName").toString();
    }
    return result.toBool();
}

QString PlaywrightEngineBackend::frameName() const {
    QVariant result = const_cast<PlaywrightEngineBackend*>(this)->sendSyncCommand("getFrameName");
    if (result.isValid() && result.type() == QVariant::String) {
        m_currentFrameName = result.toString();
    }
    return m_currentFrameName;
}

QString PlaywrightEngineBackend::focusedFrameName() const {
    QVariant result = const_cast<PlaywrightEngineBackend*>(this)->sendSyncCommand("getFocusedFrameName");
    if (result.isValid() && result.type() == QVariant::String) {
        m_currentFocusedFrameName = result.toString();
    }
    return m_currentFocusedFrameName;
}

void PlaywrightEngineBackend::sendEvent(const QString& type, const QVariant& arg1, const QVariant& arg2,
    const QString& mouseButton, const QVariant& modifierArg) {
    qDebug() << "PlaywrightEngineBackend: Sending event (stub):" << type;
    QVariantMap params;
    params["type"] = type;
    params["arg1"] = arg1;
    params["arg2"] = arg2;
    params["mouseButton"] = mouseButton;
    params["modifierArg"] = modifierArg;
    sendAsyncCommand("sendEvent", params);
}

void PlaywrightEngineBackend::uploadFile(const QString& selector, const QStringList& fileNames) {
    qDebug() << "PlaywrightEngineBackend: Uploading file:" << selector << fileNames;
    QVariantMap params;
    params["selector"] = selector;
    params["fileNames"] = fileNames;
    sendAsyncCommand("uploadFile", params);
}

int PlaywrightEngineBackend::showInspector(int port) {
    qDebug() << "PlaywrightEngineBackend: Showing inspector on port (stub):" << port;
    QVariantMap params;
    params["port"] = port;
    QVariant result = sendSyncCommand("showInspector", params);
    return result.toInt();
}

// --- Internal Communication Methods ---

QVariant PlaywrightEngineBackend::sendSyncCommand(const QString& command, const QVariantMap& params) {
    QMutexLocker locker(&m_mutex);
    quint64 requestId = m_nextRequestId++;
    QVariantMap requestData;
    requestData["id"] = QString::number(requestId);
    requestData["command"] = command;
    requestData["params"] = params;

    QJsonDocument doc(QJsonObject::fromVariantMap(requestData));
    QByteArray messageJson = doc.toJson(QJsonDocument::Compact);
    QByteArray messageWithLength = QByteArray::number(messageJson.length()) + "\n" + messageJson;

    qDebug() << "PlaywrightEngineBackend: Sending sync command (ID:" << requestId << "):" << messageWithLength.left(100)
             << "...";

    if (!m_playwrightProcess || m_playwrightProcess->state() != QProcess::Running) {
        qWarning() << "PlaywrightEngineBackend: Playwright process not running. Cannot send sync command.";
        return QVariant();
    }

    m_playwrightProcess->write(messageWithLength);
    m_playwrightProcess->waitForBytesWritten(-1); // Wait indefinitely for write to complete

    // Wait for the response
    while (!m_syncResponses.contains(requestId)) {
        if (!m_commandWaitCondition.wait(&m_mutex, 5000)) { // Wait with a timeout
            qWarning() << "PlaywrightEngineBackend: Timeout waiting for sync command response for ID:" << requestId;
            return QVariant(); // Return empty if timeout
        }
    }
    return m_syncResponses.take(requestId);
}

void PlaywrightEngineBackend::sendAsyncCommand(const QString& command, const QVariantMap& params) {
    QVariantMap requestData;
    requestData["command"] = command;
    requestData["params"] = params;

    QJsonDocument doc(QJsonObject::fromVariantMap(requestData));
    QByteArray messageJson = doc.toJson(QJsonDocument::Compact);
    QByteArray messageWithLength = QByteArray::number(messageJson.length()) + "\n" + messageJson;

    qDebug() << "PlaywrightEngineBackend: Sending async command:" << messageWithLength.left(100) << "...";

    if (!m_playwrightProcess || m_playwrightProcess->state() != QProcess::Running) {
        qWarning() << "PlaywrightEngineBackend: Playwright process not running. Cannot send async command.";
        return;
    }

    m_playwrightProcess->write(messageWithLength);
    m_playwrightProcess->waitForBytesWritten(-1); // Wait indefinitely for write to complete
}

void PlaywrightEngineBackend::handleReadyReadStandardOutput() {
    m_readBuffer.append(m_playwrightProcess->readAllStandardOutput());
    // The processIncomingMessage needs to handle reading the length prefix and then the JSON payload.
    // It should recursively call itself if more complete messages are in the buffer.
    QByteArray currentBuffer = m_readBuffer;
    m_readBuffer.clear(); // Clear for the next read
    processIncomingMessage(currentBuffer); // Process the current chunk
}

void PlaywrightEngineBackend::handleReadyReadStandardError() {
    QByteArray errorData = m_playwrightProcess->readAllStandardError();
    if (!errorData.isEmpty()) {
        qWarning() << "Playwright Node.js Backend Error:" << QString::fromUtf8(errorData).trimmed();
    }
}

void PlaywrightEngineBackend::handleProcessStarted() {
    qDebug() << "PlaywrightEngineBackend: Node.js process has started.";
    // Send an initialization command to the Playwright backend
    sendAsyncCommand("initialize"); // Assuming Playwright backend expects an 'initialize' command
    emitInitialized(); // Signal that the backend is ready
}

void PlaywrightEngineBackend::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    qDebug() << "PlaywrightEngineBackend: Node.js process finished with exit code:" << exitCode
             << "status:" << exitStatus;
    if (exitStatus == QProcess::CrashExit) {
        qCritical() << "PlaywrightEngineBackend: Node.js process crashed!";
    }
}

void PlaywrightEngineBackend::handleProcessErrorOccurred(QProcess::ProcessError error) {
    qCritical() << "PlaywrightEngineBackend: QProcess error:" << error << m_playwrightProcess->errorString();
}

void PlaywrightEngineBackend::processIncomingMessage(const QByteArray& data) {
    QByteArray buffer = data; // Use a local copy for processing
    while (true) {
        int newlineIndex = buffer.indexOf('\n');
        if (newlineIndex == -1) {
            m_readBuffer.append(buffer); // Append remaining incomplete message to class buffer
            return;
        }

        QByteArray lengthBytes = buffer.left(newlineIndex).trimmed();
        bool ok;
        int messageLength = lengthBytes.toInt(&ok);

        if (!ok || messageLength < 0) {
            qWarning() << "PlaywrightEngineBackend: Invalid message length header:" << lengthBytes;
            m_readBuffer.clear(); // Clear buffer to avoid parsing issues with corrupted stream
            return;
        }

        // Check if we have the full message body
        if (buffer.length() < newlineIndex + 1 + messageLength) {
            m_readBuffer.append(buffer); // Not enough data, append to class buffer and wait for more
            return;
        }

        QByteArray messageBody = buffer.mid(newlineIndex + 1, messageLength);
        QJsonDocument doc = QJsonDocument::fromJson(messageBody, &m_jsonParseError);

        if (m_jsonParseError.error != QJsonParseError::NoError) {
            qWarning() << "PlaywrightEngineBackend: JSON parse error:" << m_jsonParseError.errorString()
                       << "in message:" << messageBody;
            m_readBuffer.clear(); // Clear buffer to avoid parsing issues with corrupted stream
            return;
        }

        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("type")) {
                QString type = obj["type"].toString();
                if (type == "response") {
                    processResponse(obj);
                } else if (type == "signal") {
                    processSignal(obj);
                } else {
                    qWarning() << "PlaywrightEngineBackend: Unknown message type:" << type;
                }
            } else {
                qWarning() << "PlaywrightEngineBackend: Message object has no 'type' field.";
            }
        } else {
            qWarning() << "PlaywrightEngineBackend: Incoming message is not a JSON object.";
        }

        // Remove processed message from buffer and continue loop
        buffer = buffer.mid(newlineIndex + 1 + messageLength);
    }
}

void PlaywrightEngineBackend::processResponse(const QJsonObject& response) {
    QMutexLocker locker(&m_mutex);
    if (response.contains("id") && response.contains("result")) {
        quint64 requestId = response["id"].toString().toULongLong();
        m_syncResponses[requestId] = response["result"].toVariant();
        m_commandWaitCondition.wakeAll(); // Wake up any waiting sync commands
        qDebug() << "PlaywrightEngineBackend: Received sync response for ID:" << requestId;
    } else if (response.contains("id") && response.contains("error")) {
        quint64 requestId = response["id"].toString().toULongLong();
        QVariantMap errorMap = response["error"].toObject().toVariantMap();
        qWarning() << "PlaywrightEngineBackend: Received error response for ID:" << requestId << ":"
                   << errorMap["message"].toString();
        m_syncResponses[requestId] = QVariant(); // Store an invalid variant to signal error/completion
        m_commandWaitCondition.wakeAll();
    } else {
        qWarning() << "PlaywrightEngineBackend: Invalid response format:" << response;
    }
}

void PlaywrightEngineBackend::processSignal(const QJsonObject& signal) {
    QString signalName = signal["name"].toString();
    QVariantMap data = signal["data"].toObject().toVariantMap();

    qDebug() << "PlaywrightEngineBackend: Received signal:" << signalName;

    if (signalName == "loadStarted") {
        emitLoadStarted(QUrl(data.value("url").toString()));
    } else if (signalName == "loadFinished") {
        emitLoadFinished(data.value("success").toBool(), QUrl(data.value("url").toString()));
    } else if (signalName == "loadingProgress") {
        emitLoadingProgress(data.value("progress").toInt());
    } else if (signalName == "urlChanged") {
        emitUrlChanged(QUrl(data.value("url").toString()));
    } else if (signalName == "titleChanged") {
        emitTitleChanged(data.value("title").toString());
    } else if (signalName == "contentsChanged") {
        emitContentsChanged();
    } else if (signalName == "navigationRequested") {
        emitNavigationRequested(QUrl(data.value("url").toString()), data.value("navigationType").toString(),
            data.value("isMainFrame").toBool(), data.value("navigationLocked").toBool());
    } else if (signalName == "pageCreated") {
        // This is complex: need to instantiate a new WebPage with a new PlaywrightEngineBackend
        // For now, this is a stub or would require a more sophisticated process management.
        // If Playwright backend supports creating new "contexts" or "pages", this would map to it.
        // emitPageCreated(new PlaywrightEngineBackend(this)); // This creates a new backend, which might spawn another
        // Node.js process if not handled carefully
        qWarning() << "PlaywrightEngineBackend: 'pageCreated' signal received, but new page creation not fully "
                      "implemented yet.";
    } else if (signalName == "windowCloseRequested") {
        emitWindowCloseRequested();
    } else if (signalName == "javaScriptAlertSent") {
        emitJavaScriptAlertSent(data.value("message").toString());
    } else if (signalName == "javaScriptConsoleMessageSent") {
        emitJavaScriptConsoleMessageSent(data.value("message").toString());
    } else if (signalName == "javaScriptErrorSent") {
        emitJavaScriptErrorSent(data.value("message").toString(), data.value("lineNumber").toInt(),
            data.value("sourceID").toString(), data.value("stack").toString());
    } else if (signalName == "javaScriptConfirmRequested") {
        bool result = false; // Default response
        // IMPORTANT: The actual logic for confirm() should be handled in WebPage, which then calls this.
        // This 'emitJavaScriptConfirmRequested' will trigger WebPage::javaScriptConfirm.
        emitJavaScriptConfirmRequested(data.value("message").toString(), &result);
        QVariantMap responseParams;
        responseParams["responseId"] = data.value("responseId").toString();
        responseParams["result"] = result;
        sendAsyncCommand("handleConfirmResponse", responseParams);
    } else if (signalName == "javaScriptPromptRequested") {
        QString resultString = "";
        bool accepted = false;
        // IMPORTANT: The actual logic for prompt() should be handled in WebPage.
        emitJavaScriptPromptRequested(
            data.value("message").toString(), data.value("defaultValue").toString(), &resultString, &accepted);
        QVariantMap responseParams;
        responseParams["responseId"] = data.value("responseId").toString();
        responseParams["result"] = resultString;
        responseParams["accepted"] = accepted; // Pass the accepted status back
        sendAsyncCommand("handlePromptResponse", responseParams);
    } else if (signalName == "javascriptInterruptRequested") {
        bool interrupt = false;
        emitJavascriptInterruptRequested(&interrupt);
        QVariantMap responseParams;
        responseParams["responseId"] = data.value("responseId").toString();
        responseParams["interrupt"] = interrupt;
        sendAsyncCommand("handleInterruptResponse", responseParams);
    } else if (signalName == "filePickerRequested") {
        QString chosenFile = "";
        bool handled = false;
        emitFilePickerRequested(data.value("oldFile").toString(), &chosenFile, &handled);
        QVariantMap responseParams;
        responseParams["responseId"] = data.value("responseId").toString();
        responseParams["chosenFile"] = chosenFile;
        responseParams["handled"] = handled;
        sendAsyncCommand("handleFilePickerResponse", responseParams);
    } else if (signalName == "resourceRequested") {
        emitResourceRequested(data.value("requestData").toMap(), nullptr); // QObject* request can be modeled later
    } else if (signalName == "resourceReceived") {
        emitResourceReceived(data.value("responseData").toMap());
    } else if (signalName == "resourceError") {
        emitResourceError(data.value("errorData").toMap());
    } else if (signalName == "resourceTimeout") {
        emitResourceTimeout(data.value("errorData").toMap());
    } else if (signalName == "repaintRequested") {
        QVariantMap rectMap = data.value("rect").toMap();
        emitRepaintRequested(QRect(rectMap.value("x").toInt(), rectMap.value("y").toInt(),
            rectMap.value("width").toInt(), rectMap.value("height").toInt()));
    } else if (signalName == "initialized") {
        // This initial 'initialized' signal might be from the backend itself confirming startup
        // The one in constructor is for the C++ side process initiation.
        emitInitialized();
    } else {
        qWarning() << "PlaywrightEngineBackend: Unhandled signal from backend:" << signalName << data;
    }
}

// Helper methods to emit signals (to simplify code in processSignal)
// Using Q_EMIT explicitly for clarity, though 'emit' macro usually suffices within QObject context
void PlaywrightEngineBackend::emitLoadStarted(const QUrl& url) { Q_EMIT loadStarted(url); }
void PlaywrightEngineBackend::emitLoadFinished(bool success, const QUrl& url) { Q_EMIT loadFinished(success, url); }
void PlaywrightEngineBackend::emitLoadingProgress(int progress) { Q_EMIT loadingProgress(progress); }
void PlaywrightEngineBackend::emitUrlChanged(const QUrl& url) {
    m_currentUrl = url;
    Q_EMIT urlChanged(url);
}
void PlaywrightEngineBackend::emitTitleChanged(const QString& title) {
    m_currentTitle = title;
    Q_EMIT titleChanged(title);
}
void PlaywrightEngineBackend::emitContentsChanged() { Q_EMIT contentsChanged(); }
void PlaywrightEngineBackend::emitNavigationRequested(
    const QUrl& url, const QString& navigationType, bool isMainFrame, bool navigationLocked) {
    Q_EMIT navigationRequested(url, navigationType, isMainFrame, navigationLocked);
}
void PlaywrightEngineBackend::emitPageCreated(IEngineBackend* newPageBackend) { Q_EMIT pageCreated(newPageBackend); }
void PlaywrightEngineBackend::emitWindowCloseRequested() { Q_EMIT windowCloseRequested(); }
void PlaywrightEngineBackend::emitJavaScriptAlertSent(const QString& msg) { Q_EMIT javaScriptAlertSent(msg); }
void PlaywrightEngineBackend::emitJavaScriptConsoleMessageSent(const QString& message) {
    Q_EMIT javaScriptConsoleMessageSent(message);
}
void PlaywrightEngineBackend::emitJavaScriptErrorSent(
    const QString& message, int lineNumber, const QString& sourceID, const QString& stack) {
    Q_EMIT javaScriptErrorSent(message, lineNumber, sourceID, stack);
}
void PlaywrightEngineBackend::emitJavaScriptConfirmRequested(const QString& message, bool* result) {
    Q_EMIT javaScriptConfirmRequested(message, result);
}
void PlaywrightEngineBackend::emitJavaScriptPromptRequested(
    const QString& message, const QString& defaultValue, QString* result, bool* accepted) {
    Q_EMIT javaScriptPromptRequested(message, defaultValue, result, accepted);
}
void PlaywrightEngineBackend::emitJavascriptInterruptRequested(bool* interrupt) {
    Q_EMIT javascriptInterruptRequested(interrupt);
}
void PlaywrightEngineBackend::emitFilePickerRequested(const QString& oldFile, QString* chosenFile, bool* handled) {
    Q_EMIT filePickerRequested(oldFile, chosenFile, handled);
}
void PlaywrightEngineBackend::emitResourceRequested(const QVariantMap& requestData, QObject* request) {
    Q_EMIT resourceRequested(requestData, request);
}
void PlaywrightEngineBackend::emitResourceReceived(const QVariantMap& responseData) {
    Q_EMIT resourceReceived(responseData);
}
void PlaywrightEngineBackend::emitResourceError(const QVariantMap& errorData) { Q_EMIT resourceError(errorData); }
void PlaywrightEngineBackend::emitResourceTimeout(const QVariantMap& errorData) { Q_EMIT resourceTimeout(errorData); }
void PlaywrightEngineBackend::emitRepaintRequested(const QRect& dirtyRect) { Q_EMIT repaintRequested(dirtyRect); }
void PlaywrightEngineBackend::emitInitialized() { Q_EMIT initialized(); }
