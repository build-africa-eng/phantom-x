/*
  This file is part of the PhantomJS project from Ofi Labs.

  Copyright (C) 2011=2025 Ariya Hidayat <ariya.hidayat@gmail.com>
  Copyright (C) 2011-2025 Ivan De Marino <ivan.de.marino@gmail.com>

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

#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <QObject>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QPointer> // For safe pointers to other QObjects like WebPage itself
#include <QNetworkRequest> // Needed for openUrl, load function
#include <QNetworkAccessManager> // Needed for QNetworkAccessManager::Operation

#include "ienginebackend.h" // The new abstraction
#include "core/cookiejar.h" // Still needed for public API
// NetworkAccessManager is now largely handled internally by IEngineBackend
// #include "networkaccessmanager.h" // Removed as direct usage moves to backend

// Forward declarations
class Callback;
class Phantom; // Assuming Phantom is still a singleton controlling the lifecycle
class QPdfWriter; // Used in renderPdf, which is now delegated to IEngineBackend

// Removed CustomPage class entirely. Its responsibilities are split between
// IEngineBackend implementation and WebPage's handling of IEngineBackend signals.
// class CustomPage : public QWebPage { ... };

class WebPage : public QObject {
    Q_OBJECT

public:
    explicit WebPage(QObject* parent = nullptr, const QUrl& baseUrl = QUrl());
    ~WebPage() override;

    // --- Core Properties (now cached from IEngineBackend) ---
    QString content() const;
    void setContent(const QString& content);
    void setContent(const QString& content, const QString& baseUrl);
    QString frameContent() const; // Will now operate on the 'current frame' set via backend
    void setFrameContent(const QString& content);
    void setFrameContent(const QString& content, const QString& baseUrl);
    QString title() const;
    QString frameTitle() const; // Will now operate on the 'current frame' set via backend
    QString url() const;
    QString frameUrl() const; // Will now operate on the 'current frame' set via backend
    bool loading() const;
    int loadingProgress() const;
    QString plainText() const;
    QString framePlainText() const; // Will now operate on the 'current frame' set via backend
    QString windowName() const;

    // --- Navigation ---
    bool canGoBack();
    bool goBack();
    bool canGoForward();
    bool goForward();
    bool go(int historyItemRelativeIndex);
    void reload();
    void stop();
    void openUrl(const QString& address, const QVariant& op, const QVariantMap& settings);

    // --- Rendering ---
    bool render(const QString& fileName, const QVariantMap& option);
    QString renderBase64(const QByteArray& format);
    void setViewportSize(const QVariantMap& size);
    QVariantMap viewportSize() const;
    void setClipRect(const QVariantMap& size);
    QVariantMap clipRect() const;
    void setScrollPosition(const QVariantMap& size);
    QVariantMap scrollPosition() const;
    void setPaperSize(const QVariantMap& size);
    QVariantMap paperSize() const;
    void setZoomFactor(qreal zoom);
    qreal zoomFactor() const;

    // --- JavaScript Execution ---
    QVariant evaluateJavaScript(const QString& code);
    bool injectJs(const QString& jsFilePath); // Delegates to IEngineBackend::injectJavaScriptFile

    // --- Settings ---
    void applySettings(const QVariantMap& def); // Now delegates to IEngineBackend::applySettings
    void setProxy(const QString& proxyUrl); // Delegates to IEngineBackend::setNetworkProxy
    QString userAgent() const; // Delegates to IEngineBackend::userAgent
    void setNavigationLocked(bool lock); // Delegates to IEngineBackend::setNavigationLocked
    bool navigationLocked(); // Delegates to IEngineBackend::navigationLocked
    void setCustomHeaders(const QVariantMap& headers); // Delegates to IEngineBackend::setCustomHeaders
    QVariantMap customHeaders() const; // Delegates to IEngineBackend::customHeaders

    // --- Cookie Management ---
    void setCookieJar(CookieJar* cookieJar); // Delegates to IEngineBackend::setCookieJar
    void setCookieJarFromQObject(QObject* cookieJar);
    CookieJar* cookieJar(); // Returns the locally held CookieJar
    bool setCookies(const QVariantList& cookies); // Delegates to IEngineBackend::setCookies
    QVariantList cookies() const; // Delegates to IEngineBackend::cookies
    bool addCookie(const QVariantMap& cookie); // Delegates to IEngineBackend::addCookie
    bool deleteCookie(const QString& cookieName); // Delegates to IEngineBackend::deleteCookie
    void clearCookies(); // Delegates to IEngineBackend::clearCookies

    // --- Other Settings/Properties ---
    QString libraryPath() const; // This is a WebPage property, not backend specific
    void setLibraryPath(const QString& libraryPath); // WebPage property
    QString offlineStoragePath() const; // Delegates to IEngineBackend
    int offlineStorageQuota() const; // Delegates to IEngineBackend

    // --- Frame and Page Management ---
    QObjectList pages() const; // Returns other WebPage instances
    QStringList pagesWindowName() const;
    QObject* getPage(const QString& windowName) const;
    bool ownsPages() const;
    void setOwnsPages(const bool owns);
    int framesCount() const;
    int childFramesCount() const; // deprecated, calls framesCount()
    QStringList framesName() const;
    QStringList childFramesName() const; // deprecated, calls framesName()
    bool switchToFrame(const QString& frameName);
    bool switchToChildFrame(const QString& frameName); // deprecated
    bool switchToFrame(int framePosition);
    bool switchToChildFrame(int framePosition); // deprecated
    void switchToMainFrame();
    bool switchToParentFrame();
    void switchToFocusedFrame();
    QString frameName() const; // Delegates to IEngineBackend
    QString currentFrameName() const; // deprecated, calls frameName()
    QString focusedFrameName() const; // Delegates to IEngineBackend

    // --- Event Handling ---
    void sendEvent(const QString& type, const QVariant& arg1, const QVariant& arg2, const QString& mouseButton,
        const QVariant& modifierArg); // Delegates to IEngineBackend
    void _uploadFile(const QString& selector, const QStringList& fileNames); // Delegates to IEngineBackend
    void stopJavaScript(); // Delegates to IEngineBackend (via m_shouldInterruptJs)
    void clearMemoryCache(); // Delegates to IEngineBackend

    // --- Callbacks (exposed to JS, managed by WebPage, triggered by IEngineBackend) ---
    // These will return the local Callback objects to be exposed to JS by Phantom's bootstrap
    QObject* _getGenericCallback();
    QObject* _getFilePickerCallback();
    QObject* _getJsConfirmCallback();
    QObject* _getJsPromptCallback();
    QObject* _getJsInterruptCallback();

    // --- DevTools ---
    int showInspector(const int port); // Delegates to IEngineBackend

    // --- Internal Callback Handlers (Called by WebPage's handleEngine... methods) ---
    // These methods will contain the logic previously in CustomPage's overrides
    // They are public here because CustomPage accessed them directly, but now
    // they will be called by handleEngine... methods.
    QString filePicker(const QString& oldFile); // Will use m_filePickerCallback
    bool javaScriptConfirm(const QString& msg); // Will use m_jsConfirmCallback
    bool javaScriptPrompt(
        const QString& msg, const QString& defaultValue, QString* result); // Will use m_jsPromptCallback
    void javascriptInterrupt(); // Will use m_jsInterruptCallback

signals:
    // --- Core WebPage Signals (re-emitted from IEngineBackend signals) ---
    void loadStarted();
    void loadFinished(const QString& status); // "success" or "fail"
    void initialized(); // After JS window object is ready
    void urlChanged(const QString& url);
    void navigationRequested(const QVariant& url, const QVariant& navigationType, bool willNavigate, bool isMainFrame);
    void rawPageCreated(WebPage* newPage); // Emitted when a new child page is created by backend

    // --- JavaScript Interaction Signals (re-emitted from IEngineBackend signals) ---
    void javaScriptAlertSent(const QString& message);
    void javaScriptConsoleMessageSent(const QString& message);
    void javaScriptErrorSent(const QString& message, int lineNumber, const QString& sourceID, const QString& stack);

    // --- Resource Loading Signals (re-emitted from IEngineBackend signals) ---
    void resourceRequested(QVariant requestData, QObject* reply); // reply placeholder for request handle
    void resourceReceived(QVariant responseData);
    void resourceError(QVariant errorData);
    void resourceTimeout(QVariant timeoutData);

    // --- Rendering Signals (re-emitted from IEngineBackend signals) ---
    void repaintRequested(int x, int y, int width, int height);

    // --- Lifecycle ---
    void closing(WebPage* page); // When this WebPage instance is about to be deleted

private slots:
    // --- Internal Slots to handle signals from IEngineBackend ---
    void handleEngineLoadStarted(const QUrl& url);
    void handleEngineLoadFinished(bool success, const QUrl& url);
    void handleEngineLoadingProgress(int progress);
    void handleEngineUrlChanged(const QUrl& url);
    void handleEngineTitleChanged(const QString& title);
    void handleEngineContentsChanged();
    void handleEngineNavigationRequested(
        const QUrl& url, const QString& navigationType, bool isMainFrame, bool navigationLocked);
    void handleEnginePageCreated(IEngineBackend* newPageBackend); // New backend for the new page
    void handleEngineWindowCloseRequested();

    // --- Internal Slots for JS Dialogs (triggered by IEngineBackend) ---
    void handleEngineJavaScriptAlertSent(const QString& msg);
    void handleEngineJavaScriptConfirmRequested(const QString& message, bool* result);
    void handleEngineJavaScriptPromptRequested(
        const QString& message, const QString& defaultValue, QString* result, bool* accepted);
    void handleEngineJavascriptInterruptRequested(bool* interrupt);
    void handleEngineFilePickerRequested(const QString& oldFile, QString* chosenFile, bool* handled);

    // --- Internal Slots for Resource Events (triggered by IEngineBackend) ---
    void handleEngineResourceRequested(const QVariantMap& requestData, QObject* request);
    void handleEngineResourceReceived(const QVariantMap& responseData);
    void handleEngineResourceError(const QVariantMap& errorData);
    void handleEngineResourceTimeout(const QVariantMap& errorData);

    // --- Internal Slots for Rendering Events (triggered by IEngineBackend) ---
    void handleEngineRepaintRequested(const QRect& dirtyRect);

    // --- Other internal slots ---
    void finish(bool ok); // Original finish slot, now called by handleEngineLoadFinished
    // This slot is for the *logical* current frame, not a QWebFrame
    // It's still useful if WebPage wants to track its "current" frame context
    // even if the underlying engine handles the actual switching.
    void changeCurrentFrame(IEngineBackend* frameBackend); // Renamed from QWebFrame to IEngineBackend*
    void handleCurrentFrameDestroyed(); // This might become less relevant if backend manages frames directly

private:
    // Member variables
    IEngineBackend* m_engineBackend; // The actual browser engine implementation
    QPointer<IEngineBackend> m_currentFrameBackend; // Tracks the currently "active" frame for evaluation/manipulation

    bool m_navigationLocked;
    QPoint m_mousePos; // Still needed for mouse event simulation
    bool m_ownsPages;
    int m_loadingProgress;
    bool m_shouldInterruptJs; // Used internally for stopJavaScript()

    // Callbacks. These are the QObject instances passed to JS (via Phantom)
    // and invoked from JS. Their `called` signal will be connected to logic
    // that informs the IEngineBackend to trigger a sync dialog on its side.
    Callback* m_genericCallback;
    Callback* m_filePickerCallback;
    Callback* m_jsConfirmCallback;
    Callback* m_jsPromptCallback;
    Callback* m_jsInterruptCallback;

    CookieJar* m_cookieJar; // Local cookie jar, sync with backend
    // NetworkAccessManager* m_networkAccessManager; // This will be absorbed into IEngineBackend
    QObject* m_inspector; // Renamed from QWebInspector*
    qreal m_dpi;

    // Cached properties updated by IEngineBackend signals
    QString m_cachedTitle;
    QUrl m_cachedUrl;
    QString m_cachedContent;
    QString m_cachedPlainText;
    QSize m_cachedViewportSize;
    QRect m_cachedClipRect;
    QPoint m_cachedScrollPosition;
    QString m_cachedUserAgent;
    QVariantMap m_cachedCustomHeaders;
    qreal m_cachedZoomFactor;
    QString m_cachedWindowName;
    QString m_cachedOfflineStoragePath;
    int m_cachedOfflineStorageQuota;
    QString m_cachedLocalStoragePath;
    int m_cachedLocalStorageQuota;
    int m_cachedFramesCount;
    QStringList m_cachedFramesName;
    QString m_cachedFrameName;
    QString m_cachedFocusedFrameName;

    QVariantMap m_paperSize; // Still stored locally for PDF rendering options
    QString m_libraryPath; // Still stored locally for script injection paths

    // Private helper methods
    qreal stringToPointSize(const QString& string) const;
    qreal printMargin(const QVariantMap& map, const QString& key);
    qreal getHeight(const QVariantMap& map, const QString& key) const;
    QString header(int page, int numPages); // These will depend on a C++ callback model
    QString footer(int page, int numPages); // These will depend on a C++ callback model
    void _appendScriptElement(const QString& scriptUrl); // Helper for JS injection (delegates to backend)
    // No longer CustomPage member, so defined here or made static/free func
    // bool renderPdf(QPdfWriter& pdfWriter); // This logic moves to IEngineBackend or is part of a general image
    // processing utility
};

#endif // WEBPAGE_H
