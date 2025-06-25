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
#include <QMap>
#include <QVariantMap>
#include <QVariantList>
#include <QUrl>
#include <QPointer> // For safe pointers to other WebPage instances

#include "cookiejar.h" // Still needed
#include "ienginebackend.h" // Our new engine abstraction

// Forward declarations of classes that WebPage depends on
// (No longer QtWebKit specific for CustomPage etc.)
class Config; // Still relevant for configuration
class WebpageCallbacks; // Still relevant for JS callbacks (will be re-implemented)
class NetworkAccessManager; // Replaced by IEngineBackend's network handling
class QWebInspector; // This will likely be removed or become conceptual for Playwright
class Phantom; // Phantom creates WebPage instances

// No more direct QtWebKit includes!
// #include <QPdfWriter.h> // QPdfWriter is part of Qt GUI, not WebKit specific, so can stay if WebPage handles PDF
// writing itself, or moved to backend #include <QtWebKitWidgets/QWebFrame> #include <QtWebKitWidgets/QWebPage>

class WebPage : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString frameTitle READ frameTitle NOTIFY frameTitleChanged) // Renamed for consistency
    Q_PROPERTY(QString content READ content WRITE setContent NOTIFY contentChanged)
    Q_PROPERTY(QString frameContent READ frameContent WRITE setFrameContent NOTIFY
            frameContentChanged) // Renamed for consistency
    Q_PROPERTY(QString url READ url NOTIFY urlChanged)
    Q_PROPERTY(QString frameUrl READ frameUrl NOTIFY frameUrlChanged) // Renamed for consistency
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged) // Added loadingChanged signal
    Q_PROPERTY(
        int loadingProgress READ loadingProgress NOTIFY loadingProgressChanged) // Added loadingProgressChanged signal
    Q_PROPERTY(bool canGoBack READ canGoBack)
    Q_PROPERTY(bool canGoForward READ canGoForward)
    Q_PROPERTY(QString plainText READ plainText NOTIFY plainTextChanged)
    Q_PROPERTY(QString framePlainText READ framePlainText NOTIFY framePlainTextChanged) // Renamed for consistency
    Q_PROPERTY(QString libraryPath READ libraryPath WRITE setLibraryPath)
    Q_PROPERTY(QString offlineStoragePath READ offlineStoragePath)
    Q_PROPERTY(int offlineStorageQuota READ offlineStorageQuota)
    Q_PROPERTY(QVariantMap viewportSize READ viewportSize WRITE setViewportSize)
    Q_PROPERTY(QVariantMap paperSize READ paperSize WRITE setPaperSize) // Will be translated to renderPdf options
    Q_PROPERTY(QVariantMap clipRect READ clipRect WRITE setClipRect)
    Q_PROPERTY(QVariantMap scrollPosition READ scrollPosition WRITE setScrollPosition)
    Q_PROPERTY(bool navigationLocked READ navigationLocked WRITE setNavigationLocked)
    Q_PROPERTY(QVariantMap customHeaders READ customHeaders WRITE setCustomHeaders)
    Q_PROPERTY(qreal zoomFactor READ zoomFactor WRITE setZoomFactor)
    Q_PROPERTY(QVariantList cookies READ cookies WRITE setCookies)
    Q_PROPERTY(QString windowName READ windowName)
    Q_PROPERTY(QObjectList pages READ pages)
    Q_PROPERTY(QStringList pagesWindowName READ pagesWindowName)
    Q_PROPERTY(bool ownsPages READ ownsPages WRITE setOwnsPages)
    Q_PROPERTY(QStringList framesName READ framesName)
    Q_PROPERTY(QString frameName READ frameName)
    Q_PROPERTY(int framesCount READ framesCount)
    Q_PROPERTY(QString focusedFrameName READ focusedFrameName)
    Q_PROPERTY(QObject* cookieJar READ cookieJar WRITE setCookieJarFromQObject)

public:
    // Constructor now takes parent and optional base URL
    WebPage(QObject* parent, const QUrl& baseUrl = QUrl());
    virtual ~WebPage();

    // No more direct QWebFrame access. Operations go through IEngineBackend.
    // QWebFrame* mainFrame(); // REMOVED

    QString content() const;
    QString frameContent() const;
    void setContent(const QString& content); // This will delegate to backend->setContent
    void setContent(const QString& content, const QString& baseUrl); // Overload for setting base URL
    void setFrameContent(const QString& content); // Delegates to backend, possibly requiring frame switch first
    void setFrameContent(const QString& content, const QString& baseUrl); // Overload

    QString title() const;
    QString frameTitle() const;

    QString url() const;
    QString frameUrl() const;

    bool loading() const;
    int loadingProgress() const;

    QString plainText() const;
    QString framePlainText() const;

    QString libraryPath() const;
    void setLibraryPath(const QString& dirPath);

    QString offlineStoragePath() const;
    int offlineStorageQuota() const;

    void setViewportSize(const QVariantMap& size);
    QVariantMap viewportSize() const;

    void setClipRect(const QVariantMap& size);
    QVariantMap clipRect() const;

    void setScrollPosition(const QVariantMap& size);
    QVariantMap scrollPosition() const;

    void setPaperSize(const QVariantMap& size); // This will be used in renderPdf options
    QVariantMap paperSize() const; // Will return the cached paperSize settings

    void setNavigationLocked(bool lock);
    bool navigationLocked() const; // Changed to const

    void setCustomHeaders(const QVariantMap& headers);
    QVariantMap customHeaders() const;

    int showInspector(const int remotePort = -1); // Will call backend's showInspector

    // These methods for PDF headers/footers will be passed as part of renderPdf options.
    // They are no longer direct properties of WebPage that QPdfWriter uses.
    // You might keep helper methods to format them into the renderPdf options map.
    QString footer(int page, int numPages);
    qreal footerHeight() const;
    QString header(int page, int numPages);
    qreal headerHeight() const;

    void setZoomFactor(qreal zoom);
    qreal zoomFactor() const;

    QString windowName() const;

    QObjectList pages() const;
    QStringList pagesWindowName() const;
    bool ownsPages() const;
    void setOwnsPages(const bool owns);

    int framesCount() const;
    QStringList framesName() const;
    QString frameName() const;
    QString focusedFrameName() const;

public slots:
    void openUrl(const QString& address, const QVariant& op = QVariant(), const QVariantMap& settings = QVariantMap());
    void release(); // Will close the backend and remove from pages list
    void close(); // Delegates to backend's close equivalent

    QVariant evaluateJavaScript(const QString& code);
    bool render(const QString& fileName, const QVariantMap& map = QVariantMap()); // Options map for rendering
    QString renderBase64(const QByteArray& format = "png"); // Delegates to backend, converts QByteArray to QString
    bool injectJs(const QString& jsFilePath);
    void _appendScriptElement(const QString& scriptUrl); // Will delegate to backend's JS injection
    QObject* _getGenericCallback(); // These will be re-implemented using IPC
    QObject* _getFilePickerCallback();
    QObject* _getJsConfirmCallback();
    QObject* _getJsPromptCallback();
    QObject* _getJsInterruptCallback();
    void _uploadFile(const QString& selector, const QStringList& fileNames);
    void sendEvent(const QString& type, const QVariant& arg1 = QVariant(), const QVariant& arg2 = QVariant(),
        const QString& mouseButton = QString(), const QVariant& modifierArg = QVariant());

    QObject* getPage(const QString& windowName) const; // New way to find child pages

    // Frame switching methods - now delegate to IEngineBackend
    int childFramesCount() const; // DEPRECATED, use framesCount()
    QStringList childFramesName() const; // DEPRECATED, use framesName()
    bool switchToFrame(const QString& frameName);
    bool switchToChildFrame(const QString& frameName); // DEPRECATED, use switchToFrame
    bool switchToFrame(const int framePosition);
    bool switchToChildFrame(const int framePosition); // DEPRECATED, use switchToFrame
    void switchToMainFrame();
    bool switchToParentFrame();
    void switchToFocusedFrame();
    QString currentFrameName() const; // DEPRECATED, use frameName()

    void setCookieJar(CookieJar* cookieJar);
    void setCookieJarFromQObject(QObject* cookieJar); // Handles QObject* from JS
    CookieJar* cookieJar(); // Returns the associated CookieJar

    bool setCookies(const QVariantList& cookies);
    QVariantList cookies() const;
    bool addCookie(const QVariantMap& cookie);
    bool deleteCookie(const QString& cookieName);
    bool clearCookies();

    bool canGoBack();
    bool goBack();
    bool canGoForward();
    bool goForward();
    bool go(int historyRelativeIndex);
    void reload();
    void stop();

    void stopJavaScript(); // Delegates to backend
    void clearMemoryCache(); // Delegates to backend

    void setProxy(const QString& proxyUrl); // Delegates to backend's setNetworkProxy

    qreal stringToPointSize(const QString&) const; // Helper, not directly backend
    qreal printMargin(const QVariantMap&, const QString&); // Helper, will be used in renderPdf options
    qreal getHeight(const QVariantMap&, const QString&) const; // Helper, for content measurement

signals:
    // Core Navigation/Loading
    void initialized();
    void loadStarted(); // Renamed from QWebPage signal for consistency
    void loadFinished(const QString& status); // Status string ("success", "fail")
    void urlChanged(const QString& url); // String version
    void titleChanged(const QString& title);
    void contentChanged(const QString& content); // Emit when content updates
    void plainTextChanged(const QString& plainText); // Emit when plain text updates
    void loadingChanged(); // New signal for Q_PROPERTY(bool loading)
    void loadingProgressChanged(); // New signal for Q_PROPERTY(int loadingProgress)

    // JS Dialogs/Errors
    void javaScriptAlertSent(const QString& msg);
    void javaScriptConsoleMessageSent(const QString& message);
    void javaScriptErrorSent(const QString& msg, int lineNumber, const QString& sourceID, const QString& stack);
    void filePicker(const QString& oldFile); // Signal to request user input for file picker

    // Resource Handling (more detailed data from IEngineBackend)
    void resourceRequested(const QVariant& requestData, QObject* request); // QVariant should be QVariantMap now
    void resourceReceived(const QVariant& resource); // QVariant should be QVariantMap now
    void resourceError(const QVariant& errorData); // QVariant should be QVariantMap now
    void resourceTimeout(const QVariant& errorData); // QVariant should be QVariantMap now

    // Navigation and Popups
    void navigationRequested(
        const QString& url, const QString& navigationType, bool navigationLocked, bool isMainFrame);
    void rawPageCreated(QObject* page); // Will emit a new WebPage* wrapped as QObject*
    void closing(QObject* page); // Page is about to close, pass itself as QObject*
    void repaintRequested(const int x, const int y, const int width, const int height);

private slots:
    // Internal slots to connect to IEngineBackend signals and manage cached properties
    void handleEngineLoadStarted(const QUrl& url);
    void handleEngineLoadFinished(bool ok, const QUrl& url);
    void handleEngineLoadingProgress(int progress);
    void handleEngineUrlChanged(const QUrl& url);
    void handleEngineTitleChanged(const QString& title);
    void handleEngineContentsChanged(); // Generic signal from backend when HTML/text updates
    void handleEngineJavaScriptAlert(const QString& msg);
    void handleEngineJavaScriptConsoleMessage(const QString& message);
    void handleEngineJavaScriptError(const QString& msg, int lineNumber, const QString& sourceID, const QString& stack);
    void handleEngineFilePickerDialog(const QString& currentFilePath); // Signal from backend for file pickers
    void handleEngineResourceRequested(const QVariantMap& requestData);
    void handleEngineResourceReceived(const QVariantMap& responseData);
    void handleEngineResourceError(const QVariantMap& errorData);
    void handleEngineResourceTimeout(const QVariantMap& errorData);
    void handleEngineNavigationRequested(
        const QUrl& url, const QString& navigationType, bool isMainFrame, bool navigationLocked);
    void handleEngineWebPageCreated(IEngineBackend* newBackend); // Creates a new WebPage
    void handleEngineClosing(); // Backend closing, emit closing() for this page
    void handleEngineRepaintRequested(const QRect& dirtyRect);
    void handleEngineInitialized(); // From backend, re-emit WebPage::initialized

    // For internal state management
    void handleChildPageClosing(QObject* page); // Slot to track child pages closing themselves

private:
    // Our abstracted engine backend
    IEngineBackend* m_engineBackend;

    // Cached properties that are updated via signals from IEngineBackend
    QString m_currentTitle;
    QString m_currentFrameTitle; // Will need a mechanism to get focused frame title from backend
    QString m_currentContent;
    QString m_currentFrameContent;
    QUrl m_currentUrl;
    QUrl m_currentFrameUrl;
    bool m_isLoading;
    int m_currentLoadingProgress;
    QString m_currentPlainText;
    QString m_currentFramePlainText;

    // Other internal state variables
    QVariantMap m_cachedSettings; // To store settings applied via applySettings or explicit setters
    QVariantMap m_cachedViewportSize;
    QRect m_cachedClipRect;
    QPoint m_cachedScrollPosition;
    QVariantMap m_cachedPaperSize;
    QVariantMap m_cachedCustomHeaders;
    qreal m_cachedZoomFactor;
    QString m_cachedLibraryPath;
    bool m_cachedNavigationLocked;
    CookieJar* m_cookieJar; // Directly managed by WebPage
    qreal m_dpi; // Still relevant for internal scaling logic if needed

    // List of owned child pages (QPointer for safety)
    QList<QPointer<WebPage>> m_childPages;
    bool m_ownsPages;

    // For JS callbacks, these will likely be implemented differently with WebChannel/IPC
    WebpageCallbacks* m_callbacks; // Retaining for now, but its implementation will change

    friend class Phantom; // Phantom creates WebPage instances and might manage their lifecycle.
    // The following private members from original WebPage.h are removed or replaced:
    // CustomPage* m_customWebPage; // Replaced by IEngineBackend
    // NetworkAccessManager* m_networkAccessManager; // Replaced by IEngineBackend
    // QWebFrame* m_mainFrame; // Replaced by IEngineBackend
    // QWebFrame* m_currentFrame; // Replaced by IEngineBackend
    // QWebInspector* m_inspector; // Replaced by IEngineBackend, or removed
    // QPoint m_mousePos; // Mouse events go through sendEvent to backend
    // bool m_shouldInterruptJs; // Handled by backend's interrupt method

    // Private helper methods (might still be needed or become internal to backend)
    // QImage renderImage(const RenderMode mode = Content); // Delegates to backend->render
    // bool renderPdf(QPdfWriter& pdfWriter); // Delegates to backend->renderPdf
    // void applySettings(const QVariantMap& defaultSettings); // Now a public method
    // QString userAgent() const; // Now a public method/property
    // void changeCurrentFrame(QWebFrame* const frame); // Replaced by switchToFrame methods
    // QString filePicker(const QString& oldFile); // Replaced by handleFilePicker signal/method
    // bool javaScriptConfirm(const QString& msg); // Replaced by handleJavaScriptConfirm
    // bool javaScriptPrompt(const QString& msg, const QString& defaultValue, QString* result); // Replaced by
    // handleJavaScriptPrompt void javascriptInterrupt(); // Replaced by handleJavaScriptInterrupt
};

#endif // WEBPAGE_H
