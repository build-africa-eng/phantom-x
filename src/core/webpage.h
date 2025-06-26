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
#include <QPointer>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkProxy> // Ensure this is included

#include "ienginebackend.h"

#include "cookiejar.h"

// Forward declarations
class Callback;
class Phantom;
class QPdfWriter;

class WebPage : public QObject {
    Q_OBJECT

public:
    explicit WebPage(QObject* parent = nullptr, const QUrl& baseUrl = QUrl(), IEngineBackend* backend = nullptr);
    ~WebPage() override;

    // --- Core Properties ---
    QString content() const;
    void setContent(const QString& content);
    void setContent(const QString& content, const QString& baseUrl);
    QString frameContent() const;
    void setFrameContent(const QString& content);
    void setFrameContent(const QString& content, const QString& baseUrl);
    QString title() const;
    QString frameTitle() const;
    QString url() const;
    QString frameUrl() const;
    bool loading() const;
    int loadingProgress() const;
    QString plainText() const;
    QString framePlainText() const;
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
    void setScrollPosition(const QVariantMap& pos);
    QVariantMap scrollPosition() const;
    void setPaperSize(const QVariantMap& size);
    QVariantMap paperSize() const;
    void setZoomFactor(qreal zoom);
    qreal zoomFactor() const;

    // --- JavaScript Execution ---
    QVariant evaluateJavaScript(const QString& code);
    bool injectJs(const QString& jsFilePath);

    // --- Settings ---
    void applySettings(const QVariantMap& def);
    // CRITICAL FIX: Make setProxy accept QNetworkProxy and add proxy() getter
    void setProxy(const QNetworkProxy& proxy); // FIXED: Signature updated
    QNetworkProxy proxy() const; // FIXED: Added getter for proxy
    QString userAgent() const;
    void setUserAgent(const QString& ua);
    void setNavigationLocked(bool lock);
    bool navigationLocked();
    void setCustomHeaders(const QVariantMap& headers);
    QVariantMap customHeaders() const;

    // --- Cookie Management ---
    void setCookieJar(CookieJar* cookieJar);
    void setCookieJarFromQObject(QObject* cookieJar);
    CookieJar* cookieJar();
    bool setCookies(const QVariantList& cookies);
    QVariantList cookies() const;
    bool addCookie(const QVariantMap& cookie);
    bool deleteCookie(const QString& cookieName);
    void clearCookies();

    // --- Other Settings/Properties ---
    QString libraryPath() const;
    void setLibraryPath(const QString& libraryPath);
    QString offlineStoragePath() const;
    int offlineStorageQuota() const;
    QString localStoragePath() const;
    int localStorageQuota() const;

    // --- Frame and Page Management ---
    QObjectList pages() const;
    QStringList pagesWindowName() const;
    QObject* getPage(const QString& windowName) const;
    bool ownsPages() const;
    void setOwnsPages(const bool owns);
    int framesCount() const;
    int childFramesCount() const;
    QStringList framesName() const;
    QStringList childFramesName() const;
    bool switchToFrame(const QString& frameName);
    bool switchToFrame(int framePosition);
    bool switchToChildFrame(const QString& frameName);
    bool switchToChildFrame(int framePosition);
    void switchToMainFrame();
    bool switchToParentFrame();
    void switchToFocusedFrame();
    QString frameName() const;
    QString currentFrameName() const;
    QString focusedFrameName() const;

    // --- Event Handling ---
    void sendEvent(const QString& type, const QVariant& arg1, const QVariant& arg2, const QString& mouseButton,
        const QVariant& modifierArg);
    void _uploadFile(const QString& selector, const QStringList& fileNames);
    void stopJavaScript();
    void clearMemoryCache();

    // --- Callbacks ---
    QObject* _getGenericCallback();
    QObject* _getFilePickerCallback();
    QObject* _getJsConfirmCallback();
    QObject* _getJsPromptCallback();
    QObject* _getJsInterruptCallback();

    // --- DevTools ---
    int showInspector(const int port);

    // --- Internal Callback Handlers ---
    QString filePicker(const QString& oldFile);
    bool javaScriptConfirm(const QString& msg);
    bool javaScriptPrompt(const QString& msg, const QString& defaultValue, QString* result);
    void javascriptInterrupt();

    IEngineBackend* engineBackend() const; // Ensure this is PUBLIC

signals:
    void loadStarted();
    void loadFinished(const QString& status);
    void initialized();
    void urlChanged(const QString& url);
    void navigationRequested(const QVariant& url, const QVariant& navigationType, bool willNavigate, bool isMainFrame);
    void rawPageCreated(WebPage* newPage);

    void javaScriptAlertSent(const QString& message);
    void javaScriptConsoleMessageSent(const QString& message);
    void javaScriptErrorSent(const QString& message, int lineNumber, const QString& sourceID, const QString& stack);

    void resourceRequested(QVariant requestData, QObject* reply);
    void resourceReceived(QVariant responseData);
    void resourceError(QVariant errorData);
    void resourceTimeout(QVariant errorData);

    void repaintRequested(int x, int y, int width, int height);

    void closing(WebPage* page);

private slots:
    void handleEngineLoadStarted(const QUrl& url);
    void handleEngineLoadFinished(bool success, const QUrl& url);
    void handleEngineLoadingProgress(int progress);
    void handleEngineUrlChanged(const QUrl& url);
    void handleEngineTitleChanged(const QString& title);
    void handleEngineContentsChanged();
    void handleEngineNavigationRequested(
        const QUrl& url, const QString& navigationType, bool isMainFrame, bool navigationLocked);
    void handleEnginePageCreated(IEngineBackend* newPageBackend);
    void handleEngineWindowCloseRequested();
    void handleEngineJavaScriptAlertSent(const QString& msg);
    void handleEngineJavaScriptConfirmRequested(const QString& message, bool* result);
    void handleEngineJavaScriptPromptRequested(
        const QString& message, const QString& defaultValue, QString* result, bool* accepted);
    void handleEngineJavascriptInterruptRequested(bool* interrupt);
    void handleEngineFilePickerRequested(const QString& oldFile, QString* chosenFile, bool* handled);
    void handleEngineResourceRequested(const QVariantMap& requestData, QObject* request);
    void handleEngineResourceReceived(const QVariantMap& responseData);
    void handleEngineResourceError(const QVariantMap& errorData);
    void handleEngineResourceTimeout(const QVariantMap& errorData);
    void handleEngineRepaintRequested(const QRect& dirtyRect);
    void handleEngineInitialized();

    void finish(bool ok);
    void changeCurrentFrame(IEngineBackend* frameBackend);
    void handleCurrentFrameDestroyed();

private:
    IEngineBackend* m_engineBackend;
    QPointer<IEngineBackend> m_currentFrameBackend;

    bool m_navigationLocked;
    QPoint m_mousePos;
    bool m_ownsPages;
    int m_loadingProgress;
    bool m_shouldInterruptJs;

    Callback* m_genericCallback;
    Callback* m_filePickerCallback;
    Callback* m_jsConfirmCallback;
    Callback* m_jsPromptCallback;
    Callback* m_jsInterruptCallback;

    CookieJar* m_cookieJar;
    QNetworkProxy m_currentProxy;
    QObject* m_inspector;
    qreal m_dpi;

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

    QVariantMap m_paperSize;
    QString m_libraryPath;

    qreal stringToPointSize(const QString& string) const;
    qreal printMargin(const QVariantMap& map, const QString& key);
    qreal getHeight(const QVariantMap& map, const QString& key) const;
    QString header(int page, int numPages);
    QString footer(int page, int numPages);
    void _appendScriptElement(const QString& scriptUrl);
};

#endif // WEBPAGE_H
