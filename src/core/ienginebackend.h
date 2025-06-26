#ifndef IENGINEBACKEND_H
#define IENGINEBACKEND_H

#include <QObject>
#include <QUrl>
#include <QString>
#include <QSize>
#include <QRect>
#include <QPoint>
#include <QVariantMap>
#include <QVariantList>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QByteArray>

class CookieJar; // Forward declare CookieJar

class IEngineBackend : public QObject
{
    Q_OBJECT

public:
    explicit IEngineBackend(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~IEngineBackend() = default;

    // --- Core Page Properties ---
    virtual QUrl url() const = 0;
    virtual QString title() const = 0;
    virtual QString toHtml() const = 0;
    virtual QString toPlainText() const = 0;
    virtual QString windowName() const = 0;

    // --- Navigation ---
    virtual void load(const QNetworkRequest& request, QNetworkAccessManager::Operation operation, const QByteArray& body) = 0;
    virtual void setHtml(const QString& html, const QUrl& baseUrl) = 0;
    virtual void reload() = 0;
    virtual void stop() = 0;
    virtual bool canGoBack() const = 0;
    virtual bool goBack() = 0;
    virtual bool canGoForward() const = 0;
    virtual bool goForward() = 0;
    virtual bool goToHistoryItem(int relativeIndex) = 0;

    // --- Rendering / Viewport ---
    virtual void setViewportSize(const QSize& size) = 0;
    virtual QSize viewportSize() const = 0;
    virtual void setClipRect(const QRect& rect) = 0;
    virtual QRect clipRect() const = 0;
    virtual void setScrollPosition(const QPoint& pos) = 0;
    virtual QPoint scrollPosition() const = 0;
    virtual QByteArray renderPdf(const QVariantMap& paperSize, const QRect& clipRect) = 0;
    virtual QByteArray renderImage(const QRect& clipRect, bool onlyViewport, const QPoint& scrollPosition) = 0;
    virtual qreal zoomFactor() const = 0;
    virtual void setZoomFactor(qreal zoom) = 0;

    // --- JavaScript Execution ---
    virtual QVariant evaluateJavaScript(const QString& code) = 0;
    virtual bool injectJavaScriptFile(const QString& jsFilePath, const QString& encoding, const QString& libraryPath, bool forEachFrame) = 0;
    virtual void exposeQObject(const QString& name, QObject* object) = 0;
    virtual void appendScriptElement(const QString& scriptUrl) = 0;

    // --- Settings / Capabilities ---
    virtual QString userAgent() const = 0;
    virtual void setUserAgent(const QString& ua) = 0;
    virtual void setNavigationLocked(bool lock) = 0;
    virtual bool navigationLocked() const = 0;
    virtual QVariantMap customHeaders() const = 0;
    virtual void setCustomHeaders(const QVariantMap& headers) = 0;
    virtual void applySettings(const QVariantMap& settings) = 0;

    // --- Network / Caching / SSL Settings ---
    virtual void setNetworkProxy(const QNetworkProxy& proxy) = 0;
    virtual void setDiskCacheEnabled(bool enabled) = 0;
    virtual void setMaxDiskCacheSize(int size) = 0;
    virtual void setDiskCachePath(const QString& path) = 0;
    virtual void setIgnoreSslErrors(bool ignore) = 0;
    virtual void setSslProtocol(const QString& protocol) = 0;
    virtual void setSslCiphers(const QString& ciphers) = 0;
    virtual void setSslCertificatesPath(const QString& path) = 0;
    virtual void setSslClientCertificateFile(const QString& file) = 0;
    virtual void setSslClientKeyFile(const QString& file) = 0;
    virtual void setSslClientKeyPassphrase(const QByteArray& passphrase) = 0;
    virtual void setResourceTimeout(int timeout) = 0;
    virtual void setMaxAuthAttempts(int attempts) = 0;

    // --- Storage ---
    virtual void setLocalStoragePath(const QString& path) = 0;
    virtual int localStorageQuota() const = 0;
    virtual void setOfflineStoragePath(const QString& path) = 0;
    virtual int offlineStorageQuota() const = 0;
    virtual void clearMemoryCache() = 0;

    // --- Cookie Management ---
    virtual void setCookieJar(CookieJar* cookieJar) = 0; // External CookieJar
    virtual bool setCookies(const QVariantList& cookies) = 0;
    virtual QVariantList cookies() const = 0;
    virtual bool addCookie(const QVariantMap& cookie) = 0;
    virtual bool deleteCookie(const QString& cookieName) = 0;
    virtual void clearCookies() = 0; // Changed return type from bool to void

    // --- Frame and Page Management ---
    virtual int framesCount() const = 0;
    virtual QStringList framesName() const = 0;
    virtual bool switchToFrame(const QString& frameName) = 0;
    virtual bool switchToFrame(int framePosition) = 0;
    virtual void switchToMainFrame() = 0;
    virtual bool switchToParentFrame() = 0;
    virtual bool switchToFocusedFrame() = 0; // Changed return type from void to bool
    virtual QString frameName() const = 0;
    virtual QString focusedFrameName() const = 0;

    // --- Event Simulation ---
    virtual void sendEvent(const QString& type, const QVariant& arg1, const QVariant& arg2, const QString& mouseButton, const QVariant& modifierArg) = 0;
    virtual void uploadFile(const QString& selector, const QStringList& fileNames) = 0;

    // --- DevTools ---
    virtual int showInspector(int port) = 0;

signals:
    // Signals to communicate with WebPage
    void loadStarted(const QUrl& url);
    void loadFinished(bool success, const QUrl& url);
    void loadingProgress(int progress);
    void urlChanged(const QUrl& url);
    void titleChanged(const QString& title);
    void contentsChanged(); // Emitted when page content changes (e.g., via JS)
    void navigationRequested(const QUrl& url, const QString& navigationType, bool isMainFrame, bool navigationLocked);
    void pageCreated(IEngineBackend* newPageBackend); // When a new window/page is opened
    void windowCloseRequested();

    // JavaScript dialogs
    void javaScriptAlertSent(const QString& message);
    void javaScriptConsoleMessageSent(const QString& message);
    void javaScriptErrorSent(const QString& message, int lineNumber, const QString& sourceID, const QString& stack);
    void javaScriptConfirmRequested(const QString& message, bool* result); // result is an out-parameter
    void javaScriptPromptRequested(const QString& message, const QString& defaultValue, QString* result, bool* accepted); // result & accepted are out-parameters
    void javascriptInterruptRequested(bool* interrupt);
    void filePickerRequested(const QString& oldFile, QString* chosenFile, bool* handled);

    // Resource tracking
    void resourceRequested(const QVariantMap& requestData, QObject* request); // request is a QNetworkRequest-like object
    void resourceReceived(const QVariantMap& responseData);
    void resourceError(const QVariantMap& errorData);
    void resourceTimeout(const QVariantMap& errorData);

    // Rendering
    void repaintRequested(const QRect& dirtyRect);

    // Backend Initialization
    void initialized(); // Emitted when the backend is ready to accept commands

};

#endif // IENGINEBACKEND_H
