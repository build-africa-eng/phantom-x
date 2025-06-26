#ifndef IENGINEBACKEND_H
#define IENGINEBACKEND_H

#include <QObject>
#include <QSize>
#include <QUrl>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QRect> // For clipRect and repaintRequested

// Forward declarations to avoid heavy includes in the interface header
class CookieJar; // Used for setCookieJar, but actual management is backend's responsibility
class QNetworkProxy; // Used for setNetworkProxy

/**
 * @brief The IEngineBackend class is an abstract interface for different web page rendering engines.
 * All concrete engine implementations (e.g., QtWebKit, Playwright, etc.) must implement
 * the pure virtual functions defined here.
 */
class IEngineBackend : public QObject {
    Q_OBJECT

public:
    explicit IEngineBackend(QObject* parent = nullptr)
        : QObject(parent) { }
    virtual ~IEngineBackend() = default;

    // --- Core Page Navigation and Loading ---
    /**
     * @brief Loads a URL into the engine.
     * @param request The QNetworkRequest to load (contains URL, headers, etc.).
     * @param operation The operation type (e.g., QNetworkAccessManager::GetOperation).
     * @param body The content for POST requests.
     */
    virtual void load(
        const QNetworkRequest& request, QNetworkAccessManager::Operation operation, const QByteArray& body)
        = 0;

    /**
     * @brief Sets the HTML content of the main frame.
     * @param html The HTML string.
     * @param baseUrl The base URL for resolving relative paths in the HTML.
     */
    virtual void setHtml(const QString& html, const QUrl& baseUrl) = 0;

    /**
     * @brief Reloads the current page.
     */
    virtual void reload() = 0;

    /**
     * @brief Stops loading the current page.
     */
    virtual void stop() = 0;

    /**
     * @brief Checks if the page can navigate back in history.
     * @return True if navigation back is possible, false otherwise.
     */
    virtual bool canGoBack() const = 0;
    /**
     * @brief Navigates back in history.
     * @return True if navigation occurred, false otherwise.
     */
    virtual bool goBack() = 0;
    /**
     * @brief Checks if the page can navigate forward in history.
     * @return True if navigation forward is possible, false otherwise.
     */
    virtual bool canGoForward() const = 0;
    /**
     * @brief Navigates forward in history.
     * @return True if navigation occurred, false otherwise.
     */
    virtual bool goForward() = 0;
    /**
     * @brief Navigates to a specific item in the history relative to the current position.
     * @param relativeIndex The relative index (e.g., -1 for back, 1 for forward).
     * @return True if navigation occurred, false otherwise.
     */
    virtual bool goToHistoryItem(int relativeIndex) = 0;

    // --- Page Content Properties ---
    /**
     * @brief Returns the HTML content of the main frame.
     */
    virtual QString toHtml() const = 0;
    /**
     * @brief Returns the title of the main frame.
     */
    virtual QString title() const = 0;
    /**
     * @brief Returns the current URL of the main frame.
     */
    virtual QUrl url() const = 0;
    /**
     * @brief Returns the plain text content of the main frame.
     */
    virtual QString toPlainText() const = 0;

    // --- JavaScript Execution & Interaction ---
    /**
     * @brief Evaluates JavaScript code in the page's current frame context.
     * @param script The JavaScript code to execute.
     * @return The result of the JavaScript execution.
     */
    virtual QVariant evaluateJavaScript(const QString& script) = 0;

    /**
     * @brief Injects a JavaScript file into the page's current frame context.
     * @param filePath The path to the JavaScript file.
     * @param encoding The encoding of the script file.
     * @param libraryPath The base path for resolving relative script paths.
     * @param inPhantomScope If true, injects within Phantom's scope.
     * @return True if successful, false otherwise.
     */
    virtual bool injectJavaScriptFile(
        const QString& filePath, const QString& encoding, const QString& libraryPath, bool inPhantomScope)
        = 0;

    /**
     * @brief Appends a script element to the page's DOM.
     * @param scriptUrl The URL of the script to append.
     */
    virtual void appendScriptElement(const QString& scriptUrl) = 0;

    /**
     * @brief Sends a generic keyboard/mouse event to the page.
     * @param type The type of event (e.g., "keydown", "click", "mousemove").
     * @param arg1 Primary argument (e.g., key code, x-coordinate).
     * @param arg2 Secondary argument (e.g., text, y-coordinate).
     * @param mouseButton For mouse events (e.g., "left", "right").
     * @param modifierArg Modifier keys (e.g., "shift", "control").
     */
    virtual void sendEvent(const QString& type, const QVariant& arg1, const QVariant& arg2, const QString& mouseButton,
        const QVariant& modifierArg)
        = 0;

    /**
     * @brief Uploads files to an HTML file input element.
     * @param selector CSS selector for the file input.
     * @param fileNames List of file paths to upload.
     */
    virtual void uploadFile(const QString& selector, const QStringList& fileNames) = 0;

    // --- Page Settings and Properties ---
    /**
     * @brief Applies a set of settings to the page. This method should be called by WebPage.
     * @param settings A map of setting names to their values.
     */
    virtual void applySettings(const QVariantMap& settings) = 0;

    virtual QString userAgent() const = 0;
    virtual void setUserAgent(const QString& ua) = 0;

    virtual QSize viewportSize() const = 0;
    virtual void setViewportSize(const QSize& size) = 0;

    virtual QRect clipRect() const = 0;
    virtual void setClipRect(const QRect& rect) = 0;

    virtual QPoint scrollPosition() const = 0;
    virtual void setScrollPosition(const QPoint& pos) = 0;

    virtual void setNavigationLocked(bool lock) = 0;
    virtual bool navigationLocked() const = 0;

    virtual QVariantMap customHeaders() const = 0;
    virtual void setCustomHeaders(const QVariantMap& headers) = 0;

    virtual qreal zoomFactor() const = 0;
    virtual void setZoomFactor(qreal zoom) = 0;

    virtual QString windowName() const = 0; // For window.name property

    // Offline storage paths/quotas (Managed by Playwright context/profile)
    virtual QString offlineStoragePath() const = 0; // The actual path used by the backend
    virtual int offlineStorageQuota() const = 0;
    virtual QString localStoragePath() const = 0; // The actual path used by the backend
    virtual int localStorageQuota() const = 0;

    // --- Rendering ---
    /**
     * @brief Renders the page to an image.
     * @param clipRect The clipping rectangle for the render.
     * @param onlyViewport If true, renders only the viewport, not the full page content.
     * @param scrollPosition The scroll position to apply before rendering.
     * @return The rendered image as a QImage.
     */
    virtual QImage renderImage(const QRect& clipRect, bool onlyViewport, const QPoint& scrollPosition) = 0;
    /**
     * @brief Renders the page to a PDF.
     * @param paperSize A map containing paper size options (format, width, height, margin, orientation).
     * @param clipRect The clipping rectangle for the render.
     * @return The PDF data as a QByteArray.
     */
    virtual QByteArray renderPdf(const QVariantMap& paperSize, const QRect& clipRect) = 0;

    // --- Cookie Management (Backend handles direct interaction with browser's cookie store) ---
    /**
     * @brief Informs the backend about the current CookieJar being used by the application.
     * The backend should synchronize its browser cookies with this jar.
     * @param cookieJar Pointer to the application's CookieJar.
     */
    virtual void setCookieJar(CookieJar* cookieJar) = 0;
    /**
     * @brief Returns a list of cookies currently visible to the page/backend.
     * @return QVariantList of cookie maps.
     */
    virtual QVariantList cookies() const = 0;
    /**
     * @brief Sets a list of cookies for the page/backend.
     * @param cookies QVariantList of cookie maps to set.
     * @return True if successful.
     */
    virtual bool setCookies(const QVariantList& cookies) = 0;
    /**
     * @brief Adds a single cookie to the page/backend.
     * @param cookie QVariantMap representing the cookie.
     * @return True if successful.
     */
    virtual bool addCookie(const QVariantMap& cookie) = 0;
    /**
     * @brief Deletes a cookie by name.
     * @param cookieName Name of the cookie to delete.
     * @return True if successful.
     */
    virtual bool deleteCookie(const QString& cookieName) = 0;
    /**
     * @brief Clears all cookies for the current page/backend.
     * @return True if successful.
     */
    virtual bool clearCookies() = 0;

    // --- Network / Proxy ---
    /**
     * @brief Sets the network proxy for the backend.
     * @param proxy QNetworkProxy object.
     */
    virtual void setNetworkProxy(const QNetworkProxy& proxy) = 0;
    /**
     * @brief Clears the browser's memory cache.
     */
    virtual void clearMemoryCache() = 0;
    /**
     * @brief Sets whether SSL errors should be ignored.
     * @param ignore True to ignore SSL errors.
     */
    virtual void setIgnoreSslErrors(bool ignore) = 0;
    /**
     * @brief Sets the SSL protocol to use.
     * @param protocolName The SSL protocol name (e.g., "TLSv1.2").
     */
    virtual void setSslProtocol(const QString& protocolName) = 0;
    /**
     * @brief Sets the SSL ciphers to use.
     * @param ciphers The colon-separated list of OpenSSL cipher names.
     */
    virtual void setSslCiphers(const QString& ciphers) = 0;
    /**
     * @brief Sets the path for custom CA certificates.
     * @param path The path to custom CA certificates.
     */
    virtual void setSslCertificatesPath(const QString& path) = 0;
    /**
     * @brief Sets the path to a client certificate file for SSL.
     * @param path The path to the client certificate.
     */
    virtual void setSslClientCertificateFile(const QString& path) = 0;
    /**
     * @brief Sets the path to a client private key file for SSL.
     * @param path The path to the client key.
     */
    virtual void setSslClientKeyFile(const QString& path) = 0;
    /**
     * @brief Sets the passphrase for the client private key.
     * @param passphrase The passphrase.
     */
    virtual void setSslClientKeyPassphrase(const QByteArray& passphrase) = 0;
    /**
     * @brief Sets the resource timeout in milliseconds.
     * @param timeoutMs The timeout in milliseconds.
     */
    virtual void setResourceTimeout(int timeoutMs) = 0;
    /**
     * @brief Sets the maximum authentication attempts for network requests.
     * @param attempts The maximum number of attempts.
     */
    virtual void setMaxAuthAttempts(int attempts) = 0;

    // --- Frame Management (Proxy to Playwright's frame API) ---
    virtual int framesCount() const = 0; // Number of child frames in current frame
    virtual QStringList framesName() const = 0; // Names of child frames in current frame
    virtual QString frameName() const = 0; // Name of the current frame (the one evaluateJavaScript etc. operates on)
    virtual QString focusedFrameName() const = 0; // Name of the frame that has focus
    virtual bool switchToFrame(const QString& frameName) = 0; // Switch to frame by name
    virtual bool switchToFrame(int framePosition) = 0; // Switch to frame by index
    virtual void switchToMainFrame() = 0; // Switch to main frame
    virtual bool switchToParentFrame() = 0; // Switch to parent frame
    virtual void switchToFocusedFrame() = 0; // Switch to currently focused frame

    // --- JavaScript-to-C++ Callbacks (Exposing QObject* for JS bridge) ---
    /**
     * @brief Exposes a C++ QObject to the JavaScript context of the page/frame.
     * This method is crucial for bridging the PhantomJS API (`window.phantom`, `window.callPhantom`)
     * @param name The name of the JavaScript object.
     * @param object The QObject to expose.
     */
    virtual void exposeQObject(const QString& name, QObject* object) = 0;

    // --- HTML Element interaction ---
    /**
     * @brief Clicks an element identified by a selector.
     * @param selector CSS selector for the element.
     */
    virtual void clickElement(const QString& selector) = 0;

    // --- DevTools ---
    /**
     * @brief Shows a debugger/inspector interface, typically external for Playwright.
     * @param remotePort The port to use for remote debugging (if applicable).
     * @return The actual port opened, or 0 if not supported/opened.
     */
    virtual int showInspector(int remotePort = -1) = 0;

    // --- Signals (to be emitted by concrete implementations) ---
signals:
    // Core Navigation/Load Signals
    void loadStarted(const QUrl& url); // Added URL to signal for better context
    void loadFinished(bool ok, const QUrl& url); // Added URL for better context
    void loadingProgress(int progress);
    void urlChanged(const QUrl& url);
    void titleChanged(const QString& title);
    void contentsChanged(); // For toHtml() and toPlainText() changes

    // JS Dialogs/Errors (Backend emits, WebPage handles with Callbacks)
    void javaScriptAlertSent(const QString& msg);
    // These signals require the WebPage to provide a synchronous response back to the backend
    void javaScriptConfirmRequested(const QString& message, bool* result); // result is OUT parameter
    void javaScriptPromptRequested(const QString& message, const QString& defaultValue, QString* result,
        bool* accepted); // result is OUT parameter, accepted is OUT parameter
    void javascriptInterruptRequested(bool* interrupt); // interrupt is OUT parameter
    void filePickerRequested(
        const QString& oldFile, QString* chosenFile, bool* handled); // chosenFile is OUT, handled is OUT

    void javaScriptConsoleMessageSent(const QString& message);
    void javaScriptErrorSent(const QString& message, int lineNumber, const QString& sourceID, const QString& stack);

    // Resource Handling (more detailed data for Playwright's power)
    void resourceRequested(
        const QVariantMap& requestData, QObject* request); // QObject* is a placeholder for a request handle
    void resourceReceived(const QVariantMap& responseData);
    void resourceError(const QVariantMap& errorData);
    void resourceTimeout(const QVariantMap& errorData);

    // Navigation and Popups
    void navigationRequested(const QUrl& url, const QString& navigationType, bool isMainFrame, bool navigationLocked);
    /**
     * @brief Emitted when a new browser page/window is created by the backend (e.g., window.open).
     * The `newPageBackend` is a new IEngineBackend instance representing the new page.
     */
    void pageCreated(IEngineBackend* newPageBackend);
    void windowCloseRequested(); // When the browser window itself requests to close

    // Rendering/Repaint
    void repaintRequested(const QRect& dirtyRect);

    // Initialized signal (after window object creation and initial setup)
    void initialized(); // This is typically after main frame JS context is ready

protected:
    // Any protected members/helpers for implementations
};

#endif // IENGINEBACKEND_H
