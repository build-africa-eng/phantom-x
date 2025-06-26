#ifndef PLAYWRIGHTENGINEBACKEND_H
#define PLAYWRIGHTENGINEBACKEND_H

#include "ienginebackend.h"
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QPair> // For QPair
#include <QNetworkRequest> // For QNetworkRequest, QNetworkAccessManager::Operation
#include <QNetworkProxy> // For QNetworkProxy

class CookieJar; // Forward declare

class PlaywrightEngineBackend : public IEngineBackend
{
    Q_OBJECT

public:
    explicit PlaywrightEngineBackend(QObject* parent = nullptr);
    ~PlaywrightEngineBackend() override;

    // IEngineBackend overrides
    QUrl url() const override;
    QString title() const override;
    QString toHtml() const override;
    QString toPlainText() const override;
    QString windowName() const override;

    void load(const QNetworkRequest& request, QNetworkAccessManager::Operation operation, const QByteArray& body) override;
    void setHtml(const QString& html, const QUrl& baseUrl) override;
    void reload() override;
    void stop() override;
    bool canGoBack() const override;
    bool goBack() override;
    bool canGoForward() const override;
    bool goForward() override;
    bool goToHistoryItem(int relativeIndex) override;

    void setViewportSize(const QSize& size) override;
    QSize viewportSize() const override;
    void setClipRect(const QRect& rect) override;
    QRect clipRect() const override;
    void setScrollPosition(const QPoint& pos) override;
    QPoint scrollPosition() const override;
    QByteArray renderPdf(const QVariantMap& paperSize, const QRect& clipRect) override;
    QByteArray renderImage(const QRect& clipRect, bool onlyViewport, const QPoint& scrollPosition) override;
    qreal zoomFactor() const override;
    void setZoomFactor(qreal zoom) override;

    QVariant evaluateJavaScript(const QString& code) override;
    bool injectJavaScriptFile(const QString& jsFilePath, const QString& encoding, const QString& libraryPath, bool forEachFrame) override;
    void exposeQObject(const QString& name, QObject* object) override;
    void appendScriptElement(const QString& scriptUrl) override;

    QString userAgent() const override;
    void setUserAgent(const QString& ua) override;
    void setNavigationLocked(bool lock) override;
    bool navigationLocked() const override;
    QVariantMap customHeaders() const override;
    void setCustomHeaders(const QVariantMap& headers) override;
    void applySettings(const QVariantMap& settings) override;

    void setNetworkProxy(const QNetworkProxy& proxy) override;
    void setDiskCacheEnabled(bool enabled) override;
    void setMaxDiskCacheSize(int size) override;
    void setDiskCachePath(const QString& path) override;
    void setIgnoreSslErrors(bool ignore) override;
    void setSslProtocol(const QString& protocol) override;
    void setSslCiphers(const QString& ciphers) override;
    void setSslCertificatesPath(const QString& path) override;
    void setSslClientCertificateFile(const QString& file) override;
    void setSslClientKeyFile(const QString& file) override;
    void setSslClientKeyPassphrase(const QByteArray& passphrase) override;
    void setResourceTimeout(int timeout) override;
    void setMaxAuthAttempts(int attempts) override;
    void setLocalStoragePath(const QString& path) override;
    int localStorageQuota() const override;
    void setOfflineStoragePath(const QString& path) override;
    int offlineStorageQuota() const override;
    void clearMemoryCache() override;

    void setCookieJar(CookieJar* cookieJar) override;
    bool setCookies(const QVariantList& cookies) override;
    QVariantList cookies() const override;
    bool addCookie(const QVariantMap& cookie) override;
    bool deleteCookie(const QString& cookieName) override;
    void clearCookies() override; // Updated to void

    int framesCount() const override;
    QStringList framesName() const override;
    bool switchToFrame(const QString& frameName) override;
    bool switchToFrame(int framePosition) override;
    void switchToMainFrame() override;
    bool switchToParentFrame() override;
    bool switchToFocusedFrame() override; // Now matches base class
    QString frameName() const override;
    QString focusedFrameName() const override;

    void sendEvent(const QString& type, const QVariant& arg1, const QVariant& arg2, const QString& mouseButton, const QVariant& modifierArg) override;
    void uploadFile(const QString& selector, const QStringList& fileNames) override;

    int showInspector(int port) override;

    // These are *internal* methods specific to PlaywrightEngineBackend, not part of IEngineBackend
    QVariant sendSyncCommand(const QString& command, const QVariantMap& params = QVariantMap());
    void sendAsyncCommand(const QString& command, const QVariantMap& params = QVariantMap());

private slots:
    void handleReadyReadStandardOutput();
    void handleReadyReadStandardError();
    void handleProcessStarted();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleProcessErrorOccurred(QProcess::ProcessError error);
    void processIncomingMessage(const QByteArray& message);
    void processResponse(const QJsonObject& response);
    void processSignal(const QJsonObject& signal);

private:
    QProcess* m_playwrightProcess;
    QString m_playwrightScriptPath;
    QQueue<QPair<quint64, QVariant>> m_pendingSyncCommands; // Stores request ID and response
    QMutex m_mutex;
    QWaitCondition m_commandWaitCondition;
    quint64 m_nextRequestId;
    QHash<quint64, QVariant> m_syncResponses; // Map from request ID to response data

    // Cached properties (these will be updated by messages from Playwright)
    mutable QUrl m_currentUrl;
    mutable QString m_currentTitle;
    mutable QString m_currentHtml;
    mutable QString m_currentPlainText;
    mutable QString m_currentWindowName;
    mutable QSize m_currentViewportSize;
    mutable QRect m_currentClipRect;
    mutable QPoint m_currentScrollPosition;
    mutable QString m_currentUserAgent;
    mutable QVariantMap m_currentCustomHeaders;
    mutable qreal m_currentZoomFactor;
    mutable bool m_currentNavigationLocked;
    mutable QString m_currentLocalStoragePath;
    mutable int m_currentLocalStorageQuota;
    mutable QString m_currentOfflineStoragePath;
    mutable int m_currentOfflineStorageQuota;
    mutable int m_currentFramesCount;
    mutable QStringList m_currentFramesName;
    mutable QString m_currentFrameName;
    mutable QString m_currentFocusedFrameName;
    mutable QVariantList m_currentCookies;

    void sendCommand(const QString& command, const QVariantMap& params, bool isSync);
    // Helper to emit signals from Playwright backend (these are not slots, just internal helpers)
    void emitLoadStarted(const QUrl& url);
    void emitLoadFinished(bool success, const QUrl& url);
    void emitLoadingProgress(int progress);
    void emitUrlChanged(const QUrl& url);
    void emitTitleChanged(const QString& title);
    void emitContentsChanged();
    void emitNavigationRequested(const QUrl& url, const QString& navigationType, bool isMainFrame, bool navigationLocked);
    void emitPageCreated(IEngineBackend* newPageBackend);
    void emitWindowCloseRequested();
    void emitJavaScriptAlertSent(const QString& msg);
    void emitJavaScriptConsoleMessageSent(const QString& message);
    void emitJavaScriptErrorSent(const QString& message, int lineNumber, const QString& sourceID, const QString& stack);
    void emitJavaScriptConfirmRequested(const QString& message, bool* result); // result is an out-parameter
    void emitJavaScriptPromptRequested(const QString& message, const QString& defaultValue, QString* result, bool* accepted); // result & accepted are out-parameters
    void emitJavascriptInterruptRequested(bool* interrupt);
    void emitFilePickerRequested(const QString& oldFile, QString* chosenFile, bool* handled);
    void emitResourceRequested(const QVariantMap& requestData, QObject* request);
    void emitResourceReceived(const QVariantMap& responseData);
    void emitResourceError(const QVariantMap& errorData);
    void emitResourceTimeout(const QVariantMap& errorData);
    void emitRepaintRequested(const QRect& dirtyRect);
    void emitInitialized();

    // Internal helper for JSON parsing
    QJsonParseError m_jsonParseError;
    QByteArray m_readBuffer;
};

#endif // PLAYWRIGHTENGINEBACKEND_H
