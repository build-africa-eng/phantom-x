#include "webpage.h"
#include "callback.h"
#include "cookiejar.h"
#include "consts.h"
#include "terminal.h"
#include "utils.h"
#include "playwrightenginebackend.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPdfWriter>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QScreen>
#include <QDebug>
#include <QBuffer>

WebPage::WebPage(QObject* parent, const QUrl& baseUrl, IEngineBackend* backend)
    : QObject(parent)
    , m_engineBackend(backend ? backend : new PlaywrightEngineBackend(this))
    , m_currentFrameBackend(m_engineBackend)
    , m_navigationLocked(false)
    , m_mousePos(0, 0)
    , m_ownsPages(false)
    , m_loadingProgress(0)
    , m_shouldInterruptJs(false)
    , m_genericCallback(new Callback(this))
    , m_filePickerCallback(new Callback(this))
    , m_jsConfirmCallback(new Callback(this))
    , m_jsPromptCallback(new Callback(this))
    , m_jsInterruptCallback(new Callback(this))
    , m_cookieJar(nullptr)
    , m_inspector(nullptr)
    , m_dpi(96.0)
    , m_cachedTitle("")
    , m_cachedUrl(baseUrl)
    , m_cachedContent("")
    , m_cachedPlainText("")
    , m_cachedViewportSize(400, 300)
    , m_cachedClipRect(0, 0, 0, 0)
    , m_cachedScrollPosition(0, 0)
    , m_cachedUserAgent("")
    , m_cachedCustomHeaders()
    , m_cachedZoomFactor(1.0)
    , m_cachedWindowName("")
    , m_cachedOfflineStoragePath("")
    , m_cachedOfflineStorageQuota(0)
    , m_cachedLocalStoragePath("")
    , m_cachedLocalStorageQuota(0)
    , m_cachedFramesCount(0)
    , m_cachedFramesName()
    , m_cachedFrameName("")
    , m_cachedFocusedFrameName("") {
    connect(m_engineBackend, &IEngineBackend::loadStarted, this, &WebPage::handleEngineLoadStarted);
    connect(m_engineBackend, &IEngineBackend::loadFinished, this, &WebPage::handleEngineLoadFinished);
    connect(m_engineBackend, &IEngineBackend::loadingProgress, this, &WebPage::handleEngineLoadingProgress);
    connect(m_engineBackend, &IEngineBackend::urlChanged, this, &WebPage::handleEngineUrlChanged);
    connect(m_engineBackend, &IEngineBackend::titleChanged, this, &WebPage::handleEngineTitleChanged);
    connect(m_engineBackend, &IEngineBackend::contentsChanged, this, &WebPage::handleEngineContentsChanged);
    connect(m_engineBackend, &IEngineBackend::navigationRequested, this, &WebPage::handleEngineNavigationRequested);
    connect(m_engineBackend, &IEngineBackend::pageCreated, this, &WebPage::handleEnginePageCreated);
    connect(m_engineBackend, &IEngineBackend::windowCloseRequested, this, &WebPage::handleEngineWindowCloseRequested);
    connect(m_engineBackend, &IEngineBackend::javaScriptAlertSent, this, &WebPage::handleEngineJavaScriptAlertSent);
    connect(m_engineBackend, &IEngineBackend::javaScriptConsoleMessageSent, this,
        &WebPage::handleEngineJavaScriptConsoleMessageSent);
    connect(m_engineBackend, &IEngineBackend::javaScriptErrorSent, this, &WebPage::handleEngineJavaScriptErrorSent);
    connect(m_engineBackend, &IEngineBackend::resourceRequested, this, &WebPage::handleEngineResourceRequested);
    connect(m_engineBackend, &IEngineBackend::resourceReceived, this, &WebPage::handleEngineResourceReceived);
    connect(m_engineBackend, &IEngineBackend::resourceError, this, &WebPage::handleEngineResourceError);
    connect(m_engineBackend, &IEngineBackend::resourceTimeout, this, &WebPage::handleEngineResourceTimeout);
    connect(m_engineBackend, &IEngineBackend::repaintRequested, this, &WebPage::handleEngineRepaintRequested);
    connect(m_engineBackend, &IEngineBackend::initialized, this, &WebPage::handleEngineInitialized);

    connect(m_engineBackend, &IEngineBackend::javaScriptConfirmRequested, this,
        &WebPage::handleEngineJavaScriptConfirmRequested);
    connect(m_engineBackend, &IEngineBackend::javaScriptPromptRequested, this,
        &WebPage::handleEngineJavaScriptPromptRequested);
    connect(m_engineBackend, &IEngineBackend::javascriptInterruptRequested, this,
        &WebPage::handleEngineJavascriptInterruptRequested);
    connect(m_engineBackend, &IEngineBackend::filePickerRequested, this, &WebPage::handleEngineFilePickerRequested);

    if (backend == nullptr && !baseUrl.isEmpty() && baseUrl != QUrl("about:blank")) {
        qDebug() << "WebPage: Initial load of base URL:" << baseUrl;
        m_engineBackend->load(QNetworkRequest(baseUrl), QNetworkAccessManager::GetOperation, QByteArray());
    }
}

WebPage::~WebPage() {
    qDebug() << "WebPage: Destructor called.";
    emit closing(this);
}

IEngineBackend* WebPage::engineBackend() const { return m_engineBackend; }

QString WebPage::content() const {
    m_cachedContent = m_engineBackend->toHtml();
    return m_cachedContent;
}
void WebPage::setContent(const QString& content) { setContent(content, QUrl()); }
void WebPage::setContent(const QString& content, const QString& baseUrl) {
    m_engineBackend->setHtml(content, QUrl(baseUrl));
    m_cachedContent = content;
}
QString WebPage::frameContent() const {
    if (m_currentFrameBackend) {
        return m_currentFrameBackend->toHtml();
    }
    return QString();
}
void WebPage::setFrameContent(const QString& content) { setFrameContent(content, QUrl()); }
void WebPage::setFrameContent(const QString& content, const QString& baseUrl) {
    if (m_currentFrameBackend) {
        m_currentFrameBackend->setHtml(content, QUrl(baseUrl));
    }
}
QString WebPage::title() const {
    m_cachedTitle = m_engineBackend->title();
    return m_cachedTitle;
}
QString WebPage::frameTitle() const {
    if (m_currentFrameBackend) {
        return m_currentFrameBackend->title();
    }
    return QString();
}
QString WebPage::url() const {
    m_cachedUrl = m_engineBackend->url();
    return m_cachedUrl;
}
QString WebPage::frameUrl() const {
    if (m_currentFrameBackend) {
        return m_currentFrameBackend->url();
    }
    return QUrl();
}
bool WebPage::loading() const { return m_loadingProgress < 100; }
int WebPage::loadingProgress() const { return m_loadingProgress; }
QString WebPage::plainText() const {
    m_cachedPlainText = m_engineBackend->toPlainText();
    return m_cachedPlainText;
}
QString WebPage::framePlainText() const {
    if (m_currentFrameBackend) {
        return m_currentFrameBackend->toPlainText();
    }
    return QString();
}
QString WebPage::windowName() const {
    m_cachedWindowName = m_engineBackend->windowName();
    return m_cachedWindowName;
}

bool WebPage::canGoBack() { return m_engineBackend->canGoBack(); }
bool WebPage::goBack() { return m_engineBackend->goBack(); }
bool WebPage::canGoForward() { return m_engineBackend->canGoForward(); }
bool WebPage::goForward() { return m_engineBackend->goForward(); }
bool WebPage::go(int historyItemRelativeIndex) { return m_engineBackend->goToHistoryItem(historyItemRelativeIndex); }
void WebPage::reload() { m_engineBackend->reload(); }
void WebPage::stop() { m_engineBackend->stop(); }
void WebPage::openUrl(const QString& address, const QVariant& op, const QVariantMap& settings) {
    QUrl url = address.startsWith("http") || address.startsWith("file") ? QUrl(address) : QUrl::fromLocalFile(address);
    QNetworkAccessManager::Operation operation = QNetworkAccessManager::GetOperation;
    QByteArray body;

    if (op.isValid()) {
        if (op.type() == QVariant::String) {
            QString opStr = op.toString().toLower();
            if (opStr == "post") {
                operation = QNetworkAccessManager::PostOperation;
            } else if (opStr == "put") {
                operation = QNetworkAccessManager::PutOperation;
            } else if (opStr == "delete") {
                operation = QNetworkAccessManager::DeleteOperation;
            }
        } else if (op.type() == QVariant::Map) {
            QVariantMap opMap = op.toMap();
            if (opMap.contains("operation")) {
                QString opStr = opMap["operation"].toString().toLower();
                if (opStr == "post") {
                    operation = QNetworkAccessManager::PostOperation;
                } else if (opStr == "put") {
                    operation = QNetworkAccessManager::PutOperation;
                } else if (opStr == "delete") {
                    operation = QNetworkAccessManager::DeleteOperation;
                }
            }
            if (opMap.contains("data")) {
                body = opMap["data"].toByteArray();
            }
        }
    }

    QNetworkRequest request(url);
    if (settings.contains(PAGE_SETTINGS_CUSTOM_HEADERS)) {
        QVariantMap customHeadersMap = settings[PAGE_SETTINGS_CUSTOM_HEADERS].toMap();
        for (auto it = customHeadersMap.constBegin(); it != customHeadersMap.constEnd(); ++it) {
            request.setRawHeader(it.key().toUtf8(), it.value().toByteArray());
        }
    }
    m_engineBackend->load(request, operation, body);
}

bool WebPage::render(const QString& fileName, const QVariantMap& option) {
    if (fileName.isEmpty()) {
        Terminal::instance()->cerr("WebPage::render: Empty file name provided.");
        return false;
    }

    QString format = option.value("format", "png").toString().toLower();
    bool onlyViewport = option.value("onlyViewport", false).toBool();
    QPoint scrollPosition = m_engineBackend->scrollPosition();

    QRect clipRect;
    if (option.contains("clipRect")) {
        QVariantMap clipMap = option.value("clipRect").toMap();
        clipRect = QRect(clipMap.value("left").toInt(), clipMap.value("top").toInt(), clipMap.value("width").toInt(),
            clipMap.value("height").toInt());
    } else {
        clipRect = m_engineBackend->clipRect();
    }

    QByteArray renderedData;
    if (format == "pdf") {
        renderedData = m_engineBackend->renderPdf(m_paperSize, clipRect);
        if (renderedData.isEmpty()) {
            Terminal::instance()->cerr("WebPage::render: PDF rendering failed or returned empty data.");
            return false;
        }
    } else {
        renderedData = m_engineBackend->renderImage(clipRect, onlyViewport, scrollPosition);
        if (renderedData.isEmpty()) {
            Terminal::instance()->cerr("WebPage::render: Image rendering failed or returned empty data.");
            return false;
        }
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        Terminal::instance()->cerr("WebPage::render: Could not open file for writing: " + fileName);
        return false;
    }
    file.write(renderedData);
    file.close();

    qDebug() << "WebPage::render: Saved to" << fileName;
    return true;
}

QString WebPage::renderBase64(const QByteArray& format) {
    QString fmt = QString::fromUtf8(format).toLower();
    QByteArray renderedData;

    QRect clipRect = m_engineBackend->clipRect();
    QPoint scrollPosition = m_engineBackend->scrollPosition();
    bool onlyViewport = m_engineBackend->viewportSize().isValid();

    if (fmt == "pdf") {
        renderedData = m_engineBackend->renderPdf(m_paperSize, clipRect);
    } else {
        renderedData = m_engineBackend->renderImage(clipRect, onlyViewport, scrollPosition);
    }

    if (!renderedData.isEmpty()) {
        return QString::fromUtf8(renderedData.toBase64());
    }
    Terminal::instance()->cerr("WebPage::renderBase64: Rendering failed or returned empty data for format " + fmt);
    return QString();
}

void WebPage::setViewportSize(const QVariantMap& size) {
    m_cachedViewportSize = QSize(size.value("width").toInt(), size.value("height").toInt());
    m_engineBackend->setViewportSize(m_cachedViewportSize);
}
QVariantMap WebPage::viewportSize() const {
    m_cachedViewportSize = m_engineBackend->viewportSize();
    return QVariantMap { { "width", m_cachedViewportSize.width() }, { "height", m_cachedViewportSize.height() } };
}
void WebPage::setClipRect(const QVariantMap& size) {
    m_cachedClipRect = QRect(size.value("left").toInt(), size.value("top").toInt(), size.value("width").toInt(),
        size.value("height").toInt());
    m_engineBackend->setClipRect(m_cachedClipRect);
}
QVariantMap WebPage::clipRect() const {
    m_cachedClipRect = m_engineBackend->clipRect();
    return QVariantMap { { "left", m_cachedClipRect.left() }, { "top", m_cachedClipRect.top() },
        { "width", m_cachedClipRect.width() }, { "height", m_cachedClipRect.height() } };
}
void WebPage::setScrollPosition(const QVariantMap& pos) {
    m_cachedScrollPosition = QPoint(pos.value("left").toInt(), pos.value("top").toInt());
    m_engineBackend->setScrollPosition(m_cachedScrollPosition);
}
QVariantMap WebPage::scrollPosition() const {
    m_cachedScrollPosition = m_engineBackend->scrollPosition();
    return QVariantMap { { "left", m_cachedScrollPosition.x() }, { "top", m_cachedScrollPosition.y() } };
}
void WebPage::setPaperSize(const QVariantMap& size) { m_paperSize = size; }
QVariantMap WebPage::paperSize() const { return m_paperSize; }
void WebPage::setZoomFactor(qreal zoom) {
    m_cachedZoomFactor = zoom;
    m_engineBackend->setZoomFactor(zoom);
}
qreal WebPage::zoomFactor() const {
    m_cachedZoomFactor = m_engineBackend->zoomFactor();
    return m_cachedZoomFactor;
}

QVariant WebPage::evaluateJavaScript(const QString& code) {
    if (m_currentFrameBackend) {
        return m_currentFrameBackend->evaluateJavaScript(code);
    }
    Terminal::instance()->cerr("WebPage::evaluateJavaScript: No current frame backend available.");
    return QVariant();
}
bool WebPage::injectJs(const QString& jsFilePath) {
    QString scriptContent;
    QFile scriptFile(jsFilePath);
    if (!scriptFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Terminal::instance()->cerr("WebPage::injectJs: Could not open script file: " + jsFilePath);
        return false;
    }
    scriptContent = QString::fromUtf8(scriptFile.readAll());
    scriptFile.close();

    if (m_currentFrameBackend) {
        return m_currentFrameBackend->injectJavaScriptFile(jsFilePath, "UTF-8", m_libraryPath, false);
    }
    Terminal::instance()->cerr("WebPage::injectJs: No current frame backend available.");
    return false;
}

void WebPage::applySettings(const QVariantMap& def) {
    if (def.contains(PAGE_SETTINGS_USER_AGENT))
        setUserAgent(def[PAGE_SETTINGS_USER_AGENT].toString());
    if (def.contains(PAGE_SETTINGS_VIEWPORT_SIZE))
        setViewportSize(def[PAGE_SETTINGS_VIEWPORT_SIZE].toMap());
    if (def.contains(PAGE_SETTINGS_CLIP_RECT))
        setClipRect(def[PAGE_SETTINGS_CLIP_RECT].toMap());
    if (def.contains(PAGE_SETTINGS_SCROLL_POSITION))
        setScrollPosition(def[PAGE_SETTINGS_SCROLL_POSITION].toMap());
    if (def.contains(PAGE_SETTINGS_ZOOM_FACTOR))
        setZoomFactor(def[PAGE_SETTINGS_ZOOM_FACTOR].toReal());
    if (def.contains(PAGE_SETTINGS_CUSTOM_HEADERS))
        setCustomHeaders(def[PAGE_SETTINGS_CUSTOM_HEADERS].toMap());
    if (def.contains(PAGE_SETTINGS_NAVIGATION_LOCKED))
        setNavigationLocked(def[PAGE_SETTINGS_NAVIGATION_LOCKED].toBool());

    if (def.contains(PAGE_SETTINGS_DISK_CACHE_ENABLED))
        m_engineBackend->setDiskCacheEnabled(def[PAGE_SETTINGS_DISK_CACHE_ENABLED].toBool());
    if (def.contains(PAGE_SETTINGS_MAX_DISK_CACHE_SIZE))
        m_engineBackend->setMaxDiskCacheSize(def[PAGE_SETTINGS_MAX_DISK_CACHE_SIZE].toInt());
    if (def.contains(PAGE_SETTINGS_DISK_CACHE_PATH))
        m_engineBackend->setDiskCachePath(def[PAGE_SETTINGS_DISK_CACHE_PATH].toString());
    if (def.contains(PAGE_SETTINGS_IGNORE_SSL_ERRORS))
        m_engineBackend->setIgnoreSslErrors(def[PAGE_SETTINGS_IGNORE_SSL_ERRORS].toBool());
    if (def.contains(PAGE_SETTINGS_SSL_PROTOCOL))
        m_engineBackend->setSslProtocol(def[PAGE_SETTINGS_SSL_PROTOCOL].toString());
    if (def.contains(PAGE_SETTINGS_SSL_CIPHERS))
        m_engineBackend->setSslCiphers(def[PAGE_SETTINGS_SSL_CIPHERS].toString());
    if (def.contains(PAGE_SETTINGS_SSL_CERTIFICATES_PATH))
        m_engineBackend->setSslCertificatesPath(def[PAGE_SETTINGS_SSL_CERTIFICATES_PATH].toString());
    if (def.contains(PAGE_SETTINGS_SSL_CLIENT_CERTIFICATE_FILE))
        m_engineBackend->setSslClientCertificateFile(def[PAGE_SETTINGS_SSL_CLIENT_CERTIFICATE_FILE].toString());
    if (def.contains(PAGE_SETTINGS_SSL_CLIENT_KEY_FILE))
        m_engineBackend->setSslClientKeyFile(def[PAGE_SETTINGS_SSL_CLIENT_KEY_FILE].toString());
    if (def.contains(PAGE_SETTINGS_SSL_CLIENT_KEY_PASSPHRASE))
        m_engineBackend->setSslClientKeyPassphrase(def[PAGE_SETTINGS_SSL_CLIENT_KEY_PASSPHRASE].toByteArray());
    if (def.contains(PAGE_SETTINGS_RESOURCE_TIMEOUT))
        m_engineBackend->setResourceTimeout(def[PAGE_SETTINGS_RESOURCE_TIMEOUT].toInt());
    if (def.contains(PAGE_SETTINGS_MAX_AUTH_ATTEMPTS))
        m_engineBackend->setMaxAuthAttempts(def[PAGE_SETTINGS_MAX_AUTH_ATTEMPTS].toInt());

    if (def.contains(PAGE_SETTINGS_OFFLINE_STORAGE_PATH))
        m_cachedOfflineStoragePath = def[PAGE_SETTINGS_OFFLINE_STORAGE_PATH].toString();
    if (def.contains(PAGE_SETTINGS_OFFLINE_STORAGE_QUOTA))
        m_cachedOfflineStorageQuota = def[PAGE_SETTINGS_OFFLINE_STORAGE_QUOTA].toInt();
    if (def.contains(PAGE_SETTINGS_LOCAL_STORAGE_PATH))
        m_cachedLocalStoragePath = def[PAGE_SETTINGS_LOCAL_STORAGE_PATH].toString();
    if (def.contains(PAGE_SETTINGS_LOCAL_STORAGE_QUOTA))
        m_cachedLocalStorageQuota = def[PAGE_SETTINGS_LOCAL_STORAGE_QUOTA].toInt();

    m_engineBackend->applySettings(def);
}

void WebPage::setProxy(const QNetworkProxy& proxy) {
    m_currentProxy = proxy;
    m_engineBackend->setNetworkProxy(proxy);
}
QNetworkProxy WebPage::proxy() const { return m_currentProxy; }

QString WebPage::userAgent() const {
    m_cachedUserAgent = m_engineBackend->userAgent();
    return m_cachedUserAgent;
}
void WebPage::setUserAgent(const QString& ua) {
    m_cachedUserAgent = ua;
    m_engineBackend->setUserAgent(ua);
}

void WebPage::setNavigationLocked(bool lock) {
    m_navigationLocked = lock;
    m_engineBackend->setNavigationLocked(lock);
}
bool WebPage::navigationLocked() {
    m_navigationLocked = m_engineBackend->navigationLocked();
    return m_navigationLocked;
}
void WebPage::setCustomHeaders(const QVariantMap& headers) {
    m_cachedCustomHeaders = headers;
    m_engineBackend->setCustomHeaders(headers);
}
QVariantMap WebPage::customHeaders() const {
    m_cachedCustomHeaders = m_engineBackend->customHeaders();
    return m_cachedCustomHeaders;
}

void WebPage::setCookieJar(CookieJar* cookieJar) {
    m_cookieJar = cookieJar;
    m_engineBackend->setCookieJar(cookieJar);
}
void WebPage::setCookieJarFromQObject(QObject* cookieJar) {
    CookieJar* cj = qobject_cast<CookieJar*>(cookieJar);
    if (cj) {
        setCookieJar(cj);
    } else {
        Terminal::instance()->cerr("WebPage::setCookieJarFromQObject: Provided QObject* is not a CookieJar*");
    }
}
CookieJar* WebPage::cookieJar() { return m_cookieJar; }
bool WebPage::setCookies(const QVariantList& cookies) { return m_engineBackend->setCookies(cookies); }
QVariantList WebPage::cookies() const { return m_engineBackend->cookies(); }
bool WebPage::addCookie(const QVariantMap& cookie) { return m_engineBackend->addCookie(cookie); }
bool WebPage::deleteCookie(const QString& cookieName) { return m_engineBackend->deleteCookie(cookieName); }
void WebPage::clearCookies() { m_engineBackend->clearCookies(); }

QString WebPage::libraryPath() const { return m_libraryPath; }
void WebPage::setLibraryPath(const QString& libraryPath) { m_libraryPath = libraryPath; }
QString WebPage::offlineStoragePath() const {
    m_cachedOfflineStoragePath = m_engineBackend->offlineStoragePath();
    return m_cachedOfflineStoragePath;
}
int WebPage::offlineStorageQuota() const {
    m_cachedOfflineStorageQuota = m_engineBackend->offlineStorageQuota();
    return m_cachedOfflineStorageQuota;
}
QString WebPage::localStoragePath() const {
    m_cachedLocalStoragePath = m_engineBackend->localStoragePath();
    return m_cachedLocalStoragePath;
}
int WebPage::localStorageQuota() const {
    m_cachedLocalStorageQuota = m_engineBackend->localStorageQuota();
    return m_cachedLocalStorageQuota;
}

QObjectList WebPage::pages() const { return QObjectList(); }
QStringList WebPage::pagesWindowName() const { return QStringList(); }
QObject* WebPage::getPage(const QString& windowName) const { return nullptr; }
bool WebPage::ownsPages() const { return m_ownsPages; }
void WebPage::setOwnsPages(const bool owns) { m_ownsPages = owns; }
int WebPage::framesCount() const {
    m_cachedFramesCount = m_engineBackend->framesCount();
    return m_cachedFramesCount;
}
int WebPage::childFramesCount() const { return framesCount(); }
QStringList WebPage::framesName() const {
    m_cachedFramesName = m_engineBackend->framesName();
    return m_cachedFramesName;
}
QStringList WebPage::childFramesName() const { return framesName(); }
bool WebPage::switchToFrame(const QString& frameName) {
    bool ok = m_engineBackend->switchToFrame(frameName);
    if (ok) {
        m_cachedFrameName = frameName;
    }
    return ok;
}
bool WebPage::switchToFrame(int framePosition) {
    bool ok = m_engineBackend->switchToFrame(framePosition);
    if (ok) {
        m_cachedFrameName = m_engineBackend->frameName();
    }
    return ok;
}
bool WebPage::switchToChildFrame(const QString& frameName) { return switchToFrame(frameName); }
bool WebPage::switchToChildFrame(int framePosition) { return switchToFrame(framePosition); }

void WebPage::switchToMainFrame() {
    m_engineBackend->switchToMainFrame();
    m_cachedFrameName = "";
    m_currentFrameBackend = m_engineBackend;
}
bool WebPage::switchToParentFrame() {
    bool ok = m_engineBackend->switchToParentFrame();
    if (ok) {
        m_cachedFrameName = m_engineBackend->frameName();
    }
    return ok;
}
void WebPage::switchToFocusedFrame() {
    m_engineBackend->switchToFocusedFrame();
    m_cachedFocusedFrameName = m_engineBackend->focusedFrameName();
}
QString WebPage::frameName() const {
    m_cachedFrameName = m_engineBackend->frameName();
    return m_cachedFrameName;
}
QString WebPage::currentFrameName() const { return frameName(); }
QString WebPage::focusedFrameName() const {
    m_cachedFocusedFrameName = m_engineBackend->focusedFrameName();
    return m_cachedFocusedFrameName;
}

void WebPage::sendEvent(const QString& type, const QVariant& arg1, const QVariant& arg2, const QString& mouseButton,
    const QVariant& modifierArg) {
    if (m_currentFrameBackend) {
        m_currentFrameBackend->sendEvent(type, arg1, arg2, mouseButton, modifierArg);
    } else {
        Terminal::instance()->cerr("WebPage::sendEvent: No current frame backend available.");
    }
}
void WebPage::_uploadFile(const QString& selector, const QStringList& fileNames) {
    if (m_currentFrameBackend) {
        m_currentFrameBackend->uploadFile(selector, fileNames);
    } else {
        Terminal::instance()->cerr("WebPage::_uploadFile: No current frame backend available.");
    }
}
void WebPage::stopJavaScript() {
    m_shouldInterruptJs = true;
    m_engineBackend->stop();
}
void WebPage::clearMemoryCache() { m_engineBackend->clearMemoryCache(); }

QObject* WebPage::_getGenericCallback() { return m_genericCallback; }
QObject* WebPage::_getFilePickerCallback() { return m_filePickerCallback; }
QObject* WebPage::_getJsConfirmCallback() { return m_jsConfirmCallback; }
QObject* WebPage::_getJsPromptCallback() { return m_jsPromptCallback; }
QObject* WebPage::_getJsInterruptCallback() { return m_jsInterruptCallback; }

int WebPage::showInspector(const int port) { return m_engineBackend->showInspector(port); }

QString WebPage::filePicker(const QString& oldFile) {
    qDebug() << "WebPage::filePicker requested for old file:" << oldFile;
    return oldFile;
}
bool WebPage::javaScriptConfirm(const QString& msg) {
    qDebug() << "WebPage::javaScriptConfirm: " << msg;
    return true;
}
bool WebPage::javaScriptPrompt(const QString& msg, const QString& defaultValue, QString* result) {
    qDebug() << "WebPage::javaScriptPrompt: " << msg << "Default:" << defaultValue;
    if (result) {
        *result = defaultValue;
    }
    return true;
}
void WebPage::javascriptInterrupt() { qDebug() << "WebPage::javascriptInterrupt: JS interruption requested."; }

void WebPage::handleEngineLoadStarted(const QUrl& url) {
    qDebug() << "WebPage: Load started for URL:" << url;
    m_cachedUrl = url;
    m_loadingProgress = 0;
    emit loadStarted();
}
void WebPage::handleEngineLoadFinished(bool success, const QUrl& url) {
    qDebug() << "WebPage: Load finished for URL:" << url << "Success:" << success;
    m_cachedUrl = url;
    m_loadingProgress = 100;
    emit loadFinished(success ? "success" : "fail");
}
void WebPage::handleEngineLoadingProgress(int progress) { m_loadingProgress = progress; }
void WebPage::handleEngineUrlChanged(const QUrl& url) {
    qDebug() << "WebPage: URL changed to:" << url;
    m_cachedUrl = url;
    emit urlChanged(url.toString());
}
void WebPage::handleEngineTitleChanged(const QString& title) {
    qDebug() << "WebPage: Title changed to:" << title;
    m_cachedTitle = title;
}
void WebPage::handleEngineContentsChanged() {
    qDebug() << "WebPage: Contents changed (will trigger re-fetch on toHtml/toPlainText calls).";
}
void WebPage::handleEngineNavigationRequested(
    const QUrl& url, const QString& navigationType, bool isMainFrame, bool navigationLocked) {
    qDebug() << "WebPage: Navigation requested to:" << url << "Type:" << navigationType << "MainFrame:" << isMainFrame
             << "Locked:" << navigationLocked;
    emit navigationRequested(url.toString(), navigationType, !navigationLocked, isMainFrame);
}
void WebPage::handleEnginePageCreated(IEngineBackend* newPageBackend) {
    qDebug() << "WebPage: Engine created new page backend. Delegating to Phantom.";
    emit rawPageCreated(new WebPage(this, QUrl(), newPageBackend));
}
void WebPage::handleEngineWindowCloseRequested() {
    qDebug() << "WebPage: Engine requested window close.";
    emit windowCloseRequested();
}

void WebPage::handleEngineJavaScriptAlertSent(const QString& msg) {
    qDebug() << "WebPage: JS Alert:" << msg;
    emit javaScriptAlertSent(msg);
}
void WebPage::handleEngineJavaScriptConfirmRequested(const QString& message, bool* result) {
    qDebug() << "WebPage: JS Confirm requested: " << message;
    *result = javaScriptConfirm(message);
}
void WebPage::handleEngineJavaScriptPromptRequested(
    const QString& message, const QString& defaultValue, QString* result, bool* accepted) {
    qDebug() << "WebPage: JS Prompt requested: " << message << "Default:" << defaultValue;
    QString promptResult;
    bool acceptedByHandler = javaScriptPrompt(message, defaultValue, &promptResult);
    if (result) {
        *result = promptResult;
    }
    if (accepted) {
        *accepted = acceptedByHandler;
    }
}
void WebPage::handleEngineJavascriptInterruptRequested(bool* interrupt) {
    qDebug() << "WebPage::javascriptInterrupt: JS interruption requested.";
    *interrupt = m_shouldInterruptJs;
    if (m_shouldInterruptJs) {
        m_shouldInterruptJs = false;
        javascriptInterrupt();
    }
}
void WebPage::handleEngineFilePickerRequested(const QString& oldFile, QString* chosenFile, bool* handled) {
    qDebug() << "WebPage: File picker requested. Old file:" << oldFile;
    QString pickedFile = filePicker(oldFile);
    if (chosenFile) {
        *chosenFile = pickedFile;
    }
    if (handled) {
        *handled = !pickedFile.isEmpty();
    }
}

void WebPage::handleEngineResourceRequested(const QVariantMap& requestData, QObject* request) {
    emit resourceRequested(requestData, request);
}
void WebPage::handleEngineResourceReceived(const QVariantMap& responseData) { emit resourceReceived(responseData); }
void WebPage::handleEngineResourceError(const QVariantMap& errorData) { emit resourceError(errorData); }
void WebPage::handleEngineResourceTimeout(const QVariantMap& errorData) { emit resourceTimeout(errorData); }

void WebPage::handleEngineRepaintRequested(const QRect& dirtyRect) {
    emit repaintRequested(dirtyRect.x(), dirtyRect.y(), dirtyRect.width(), dirtyRect.height());
}

void WebPage::handleEngineInitialized() {
    qDebug() << "WebPage: Engine reports initialization complete.";
    emit initialized();
}

void WebPage::finish(bool ok) { Q_UNUSED(ok); }
void WebPage::changeCurrentFrame(IEngineBackend* frameBackend) {
    m_currentFrameBackend = frameBackend;
    qDebug() << "WebPage: Switched current frame backend.";
}
void WebPage::handleCurrentFrameDestroyed() {
    m_currentFrameBackend = nullptr;
    qWarning() << "WebPage: Current frame backend was destroyed.";
}

qreal WebPage::stringToPointSize(const QString& string) const {
    bool ok;
    qreal value = string.toFloat(&ok);
    if (ok)
        return value;
    return 0;
}

qreal WebPage::printMargin(const QVariantMap& map, const QString& key) { return map.value(key).toReal(); }

qreal WebPage::getHeight(const QVariantMap& map, const QString& key) const { return map.value(key).toReal(); }

QString WebPage::header(int page, int numPages) {
    Q_UNUSED(page);
    Q_UNUSED(numPages);
    return "HEADER";
}

QString WebPage::footer(int page, int numPages) {
    Q_UNUSED(page);
    Q_UNUSED(numPages);
    return "FOOTER";
}

void WebPage::_appendScriptElement(const QString& scriptUrl) {
    if (m_engineBackend) {
        m_engineBackend->appendScriptElement(scriptUrl);
    }
}

#include "moc_webpage.cpp"
