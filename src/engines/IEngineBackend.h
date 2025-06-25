#ifndef IENGINEBACKEND_H
#define IENGINEBACKEND_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVariantList>
#include <QUrl>

// Forward declarations to avoid heavy includes in the interface header
class CookieJar; // Assuming you'll pass a reference to your existing CookieJar
class QNetworkProxy; // For proxy settings

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

    // --- JavaScript Execution ---
    /**
     * @brief Evaluates JavaScript code in the page's context.
     * @param script The JavaScript code to execute.
     * @return The result of the JavaScript execution.
     */
    virtual QVariant evaluateJavaScript(const QString& script) = 0;

    /**
     * @brief Injects a JavaScript file into the page's context.
     * @param filePath The path to the JavaScript file.
     * @return True if successful, false otherwise.
     */
    virtual bool injectJavaScript(const QString& filePath) = 0;

    // --- Page Settings and Properties ---
    /**
     * @brief Applies a set of settings to the page.
     * @param settings A map of setting names to their values.
     */
    virtual void applySettings(const QVariantMap& settings) = 0;

    /**
     * @brief Gets the current user agent string.
     * @return The user agent string.
     */
    virtual QString userAgent() const = 0;

    /**
     * @brief Sets the user agent string.
     * @param ua The new user agent string.
     */
    virtual void setUserAgent(const QString& ua) = 0;

    /**
     * @brief Sets the viewport size for rendering.
     * @param width Viewport width.
     * @param height Viewport height.
     */
    virtual void setViewportSize(int width, int height) = 0;

    /**
     * @brief Sets the clip rect for rendering.
     * @param x X coordinate.
     * @param y Y coordinate.
     * @param width Width.
     * @param height Height.
     */
    virtual void setClipRect(int x, int y, int width, int height) = 0;

    /**
     * @brief Clears the clip rect, resetting to full page.
     */
    virtual void clearClipRect() = 0;

    /**
     * @brief Sets the zoom factor for the page.
     * @param zoomFactor The zoom factor (e.g., 1.0 for actual size).
     */
    virtual void setZoomFactor(qreal zoomFactor) = 0;

    /**
     * @brief Renders the current page to an image file.
     * @param filename The path to save the image.
     * @param selector CSS selector for the element to render (optional).
     * @param quality Quality for image formats like JPEG (0-100).
     */
    virtual bool render(const QString& filename, const QString& selector = "", int quality = -1) = 0;

    /**
     * @brief Renders the current page as a PDF.
     * @param filename The path to save the PDF.
     * @param format PDF format (e.g., "A4", "Letter").
     * @param orientation PDF orientation ("portrait" or "landscape").
     * @param zoomFactor Zoom factor for PDF.
     */
    virtual bool renderPdf(const QString& filename, const QString& format = "A4",
        const QString& orientation = "portrait", qreal zoomFactor = 1.0)
        = 0;

    // --- Cookie Management ---
    /**
     * @brief Sets the cookie jar for the engine.
     * @param cookieJar Pointer to the CookieJar instance.
     */
    virtual void setCookieJar(CookieJar* cookieJar) = 0;

    // --- Network and Proxy ---
    /**
     * @brief Sets the network proxy for the page.
     * @param proxy The QNetworkProxy object.
     */
    virtual void setNetworkProxy(const QNetworkProxy& proxy) = 0;

    // --- Signals (to be emitted by concrete implementations) ---
signals:
    /**
     * @brief Emitted when a page starts loading.
     * @param url The URL being loaded.
     */
    void loadStarted(const QUrl& url);

    /**
     * @brief Emitted when a page finishes loading.
     * @param ok True if loading was successful, false otherwise.
     */
    void loadFinished(bool ok);

    /**
     * @brief Emitted when the JavaScript console outputs a message.
     * @param message The console message.
     */
    void javaScriptConsoleMessageSent(const QString& message);

    /**
     * @brief Emitted when a new window/page is created (e.g., by window.open).
     * @param newPage A pointer to the newly created IEngineBackend for the new page.
     */
    void windowObjectCreated(IEngineBackend* newPage);

    /**
     * @brief Emitted when an error occurs during page loading or rendering.
     * @param errorDescription A description of the error.
     */
    void error(const QString& errorDescription);

    // TODO: Add more signals as needed, e.g., for resource requests/responses, alerts, confirms etc.

protected:
    // This allows derived classes to set the library path if needed internally
    // For Playwright, this might be less relevant for the browser itself,
    // but useful for injectJs to resolve paths for example.
    QString m_libraryPath;

public slots:
    virtual void setLibraryPath(const QString& libraryPath) { m_libraryPath = libraryPath; }
};

#endif // IENGINEBACKEND_H
