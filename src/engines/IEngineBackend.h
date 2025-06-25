#ifndef IENGINEBACKEND_H
#define IENGINEBACKEND_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVariantList>
#include <QUrl>
#include <QRect> // For clipRect

// Forward declarations to avoid heavy includes in the interface header
class CookieJar;
class QNetworkProxy; // For network proxy settings

/**
 * @brief IEngineBackend is an abstract interface for different web page rendering engines.
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
     * @param url The URL to load.
     * @param operation The operation type (e.g., "GET", "POST").
     * @param content The content for POST requests.
     */
    virtual void load(const QUrl& url, const QString& operation = "GET", const QVariantMap& content = QVariantMap())
        = 0;

    /**
     * @brief Reloads the current page.
     */
    virtual void reload() = 0;

    /**
     * @brief Stops loading the current page.
     */
    virtual void stop() = 0;

    /**
     * @brief Injects HTML content into the page.
     * @param htmlContent The HTML string to set.
     * @param baseUrl The base URL for resolving relative paths in the content.
     */
    virtual void setContent(const QString& htmlContent, const QUrl& baseUrl) = 0;

    // --- Navigation History ---
    virtual bool canGoBack() const = 0;
    virtual void goBack() = 0;
    virtual bool canGoForward() const = 0;
    virtual void goForward() = 0;
    virtual bool go(int historyRelativeIndex) = 0;

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
     * @return True if successful, false otherwise.
     */
    virtual bool injectJavaScript(const QString& filePath) = 0;

    /**
     * @brief Sends a generic event to the page (e.g., mouse, keyboard).
     * @param type The type of event (e.g., "click", "keypress").
     * @param args A list of arguments for the event (e.g., coordinates, key codes).
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
     * @brief Applies a set of settings to the page.
     * @param settings A map of setting names to their values.
     */
    virtual void applySettings(const QVariantMap& settings) = 0; // This will handle many config options

    virtual QString userAgent() const = 0;
    virtual void setUserAgent(const QString& ua) = 0;

    virtual QSize viewportSize() const = 0;
    virtual void setViewportSize(const QSize& size) = 0;

    virtual QRect clipRect() const = 0;
    virtual void setClipRect(const QRect& rect) = 0;
    virtual void clearClipRect() = 0; // Added for explicit clear

    virtual QPoint scrollPosition() const = 0;
    virtual void setScrollPosition(const QPoint& pos) = 0;

    // PaperSize for PDF rendering. Pass map to renderPdf instead of direct property.
    // virtual QVariantMap paperSize() const = 0;
    // virtual void setPaperSize(const QVariantMap& size) = 0;

    virtual void setNavigationLocked(bool lock) = 0;
    virtual bool navigationLocked() const = 0;

    virtual QVariantMap customHeaders() const = 0;
    virtual void setCustomHeaders(const QVariantMap& headers) = 0;

    virtual qreal zoomFactor() const = 0;
    virtual void setZoomFactor(qreal zoom) = 0;

    virtual QString windowName() const = 0; // For window.name property

    // Offline storage paths/quotas
    virtual QString offlineStoragePath() const = 0;
    virtual int offlineStorageQuota() const = 0;

    // --- Rendering ---
    virtual bool render(const QString& filename, const QString& selector = "", int quality = -1) = 0;
    virtual QByteArray renderBase64(const QString& format = "png", const QString& selector = "", int quality = -1)
        = 0; // Returns QByteArray
    virtual bool renderPdf(const QString& filename, const QVariantMap& options = QVariantMap())
        = 0; // Incorporates paperSize, headers, footers

    // --- Cookie Management ---
    virtual void setCookieJar(CookieJar* cookieJar) = 0;
    virtual QVariantList cookies() const = 0;
    virtual bool setCookies(const QVariantList& cookies) = 0;
    virtual bool addCookie(const QVariantMap& cookie) = 0;
    virtual bool deleteCookie(const QString& cookieName) = 0;
    virtual bool clearCookies() = 0;

    // --- Network / Proxy ---
    virtual void setNetworkProxy(const QNetworkProxy& proxy) = 0;
    virtual void clearMemoryCache() = 0;

    // --- Frame Management (Crucial for Playwright) ---
    virtual int framesCount() const = 0; // Number of child frames in current frame
    virtual QStringList framesName() const = 0; // Names of child frames in current frame
    virtual QString frameName() const = 0; // Name of the current frame
    virtual QString focusedFrameName() const = 0; // Name of the currently focused frame
    virtual bool switchToFrame(const QString& frameName) = 0; // Switch to frame by name
    virtual bool switchToFrame(int framePosition) = 0; // Switch to frame by index
    virtual void switchToMainFrame() = 0; // Switch to main frame
    virtual bool switchToParentFrame() = 0; // Switch to parent frame
    virtual void switchToFocusedFrame() = 0; // Switch to currently focused frame

    // --- Callbacks/Dialogs (Require IPC for Playwright) ---
    // These methods return the result of the dialog, not the QObject for callbacks.
    virtual QString handleFilePicker(const QString& oldFile) = 0;
    virtual bool handleJavaScriptConfirm(const QString& msg) = 0;
    virtual bool handleJavaScriptPrompt(const QString& msg, const QString& defaultValue, QString* result) = 0;
    virtual void handleJavaScriptInterrupt() = 0; // To stop JS execution

    // --- Debugging / Inspector ---
    // This will likely just emit a signal for Playwright to launch external inspector
    virtual int showInspector(int remotePort = -1) = 0;

    // --- Signals (to be emitted by concrete implementations) ---
signals:
    // Core Navigation/Loading
    void loadStarted(const QUrl& url); // Added URL to signal for better context
    void loadFinished(bool ok, const QUrl& url); // Added URL for better context
    void loadingProgress(int progress);
    void urlChanged(const QUrl& url);
    void titleChanged(const QString& title);
    void contentsChanged(); // For content() and plainText()

    // JS Dialogs/Errors
    void javaScriptAlertSent(const QString& msg);
    void javaScriptConsoleMessageSent(const QString& message);
    void javaScriptErrorSent(const QString& msg, int lineNumber, const QString& sourceID, const QString& stack);
    void filePickerDialog(const QString& currentFilePath); // To request user input for file picker

    // Resource Handling (more detailed data for Playwright's power)
    void resourceRequested(const QVariantMap& requestData); // Use map for full details
    void resourceReceived(const QVariantMap& responseData); // Use map for full details
    void resourceError(const QVariantMap& errorData); // Use map for full details
    void resourceTimeout(const QVariantMap& errorData); // Use map for full details

    // Navigation and Popups
    void navigationRequested(const QUrl& url, const QString& navigationType, bool isMainFrame, bool navigationLocked);
    void webPageCreated(IEngineBackend* newBackend); // Emits a new backend for a new page
    void closing(); // Page is about to close

    // Rendering/Repaint
    void repaintRequested(const QRect& dirtyRect);

    // Initialized signal (after window object creation and initial setup)
    void initialized(); // This is typically after main frame JS context is ready

    // TODO: Add more signals as needed for future Playwright features
    // e.g., 'dialogOpened', 'requestFinished', 'requestFailed', 'response', 'websocketCreated', 'console' (for richer
    // console messages)

protected:
    QString m_libraryPath; // Stored by the backend, set by WebPage

public slots:
    void setLibraryPath(const QString& libraryPath) { m_libraryPath = libraryPath; }
};

#endif // IENGINEBACKEND_H
