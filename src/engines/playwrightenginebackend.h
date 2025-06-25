#include "playwrightenginebackend.h"

#include <QCoreApplication> // For QCoreApplication::applicationDirPath()
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QProcess>
#include <QDebug>
#include <QFile> // For reading dummy HTML
#include <QImage> // For dummy image rendering
#include <QBuffer> // For dummy PDF rendering
#include <QPainter> // For dummy PDF rendering
#include <QPdfWriter> // For dummy PDF rendering
#include <QScreen> // For screen DPI

// Constructor: Launches the Node.js Playwright subprocess
PlaywrightEngineBackend::PlaywrightEngineBackend(QObject* parent)
    : IEngineBackend(parent)
    , m_playwrightProcess(new QProcess(this))
    , m_nextMessageSize(0)
    , m_navigationLocked(false) {
    qDebug() << "PlaywrightEngineBackend: Initializing...";

    // Connect QProcess signals
    connect(m_playwrightProcess, &QProcess::readyReadStandardOutput, this,
        &PlaywrightEngineBackend::handleReadyReadStandardOutput);
    connect(m_playwrightProcess, &QProcess::readyReadStandardError, this,
        &PlaywrightEngineBackend::handleReadyReadStandardError);
    connect(m_playwrightProcess, &QProcess::started, this, &PlaywrightEngineBackend::handleProcessStarted);
    connect(m_playwrightProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        &PlaywrightEngineBackend::handleProcessFinished);
    connect(m_playwrightProcess, &QProcess::errorOccurred, this, &PlaywrightEngineBackend::handleProcessErrorOccurred);

    // Path to your Node.js script that will run Playwright
    // This script will need to be created alongside this C++ code.
    // For now, assume it's in the same directory as the executable, or a 'nodejs' subfolder.
    QString nodeScriptPath
        = QCoreApplication::applicationDirPath() + "/playwright_backend.js"; // You'll create this file

    // Check if nodejs is available
    QString nodeExecutable = "node";
#ifdef Q_OS_WIN
    nodeExecutable = "node.exe";
#endif

    // Arguments for the Node.js process
    QStringList args;
    args << nodeScriptPath;

    qDebug() << "Launching Node.js process:" << nodeExecutable << args.join(" ");

    // Start the Node.js process
    m_playwrightProcess->start(nodeExecutable, args);

    // Wait for the process to start (optional, but good for initial setup)
    if (!m_playwrightProcess->waitForStarted(5000)) { // 5-second timeout
        qCritical() << "Failed to start Node.js Playwright backend process!";
        emit loadFinished(false, QUrl("about:blank")); // Signal failure to WebPage
        return;
    }

    // Initial dummy values for properties
    m_currentUrl = QUrl("about:blank");
    m_currentTitle = "Loading...";
    m_currentHtml = "<html><body><h1>Loading...</h1></body></html>";
    m_currentPlainText = "Loading...";
    m_viewportSize = QSize(400, 300);
    m_clipRect = QRect();
    m_scrollPosition = QPoint();
    m_zoomFactor = 1.0;
    m_userAgent = "PhantomJS/3.0.0 (Custom Playwright Backend)";
    m_windowName = ""; // Not yet known
}

PlaywrightEngineBackend::~PlaywrightEngineBackend() {
    qDebug() << "PlaywrightEngineBackend: Shutting down...";
    if (m_playwrightProcess->state() != QProcess::NotRunning) {
        // Send a shutdown command to the Node.js process
        sendAsyncCommand("shutdown");
        if (!m_playwrightProcess->waitForFinished(5000)) { // Give it some time to shut down gracefully
            m_playwrightProcess->kill(); // Force kill if it doesn't shut down
            qWarning() << "Playwright backend process did not terminate gracefully, killed.";
        }
    }
    // QProcess is parented, so it will be deleted automatically
}

// --- IPC Communication Helpers ---

// Send an asynchronous command to the Node.js process
void PlaywrightEngineBackend::sendAsyncCommand(const QString& command, const QVariantMap& params) {
    QJsonObject message;
    message["type"] = "command";
    message["command"] = command;
    if (!params.isEmpty()) {
        message["params"] = QJsonObject::fromVariantMap(params);
    }

    QJsonDocument doc(message);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact); // Compact for single line
    QByteArray messageWithLength = QByteArray::number(jsonData.size()) + "\n" + jsonData; // Prepend length and newline

    qDebug() << "PlaywrightEngineBackend: Sending async command:" << messageWithLength.left(100) << "...";
    m_playwrightProcess->write(messageWithLength);
    m_playwrightProcess->waitForBytesWritten(); // Ensure data is sent
}

// Send a synchronous command and wait for a response
QVariant PlaywrightEngineBackend::sendSyncCommand(const QString& command, const QVariantMap& params) {
    // This is a placeholder for a more robust synchronous IPC.
    // In a real async environment, blocking here is generally bad for UI threads.
    // For now, it demonstrates the concept of waiting for a response.
    qint64 commandId = QDateTime::currentMSecsSinceEpoch(); // Simple unique ID

    QJsonObject message;
    message["type"] = "sync_command";
    message["command"] = command;
    message["id"] = commandId;
    if (!params.isEmpty()) {
        message["params"] = QJsonObject::fromVariantMap(params);
    }

    QJsonDocument doc(message);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
    QByteArray messageWithLength = QByteArray::number(jsonData.size()) + "\n" + jsonData;

    QMutexLocker locker(&m_commandMutex); // Lock for pending commands
    m_pendingCommands.enqueue({ commandId, QVariant() }); // Enqueue a placeholder for this command

    qDebug() << "PlaywrightEngineBackend: Sending sync command:" << messageWithLength.left(100) << "...";
    m_playwrightProcess->write(messageWithLength);
    m_playwrightProcess->waitForBytesWritten();

    // Wait for the response for this specific command ID
    // This is a blocking call and should be handled with care in a GUI application.
    // In a real application, you'd use a non-blocking signal/slot or QFuture-based approach.
    while (m_pendingCommands.head().first != commandId || m_pendingCommands.head().second.isNull()) {
        m_commandWaitCondition.wait(locker.mutex());
    }

    QVariant result = m_pendingCommands.dequeue().second;
    return result;
}

// Process incoming JSON messages from Node.js stdout
void PlaywrightEngineBackend::processIncomingMessage(const QJsonObject& message) {
    QString type = message["type"].toString();
    QString command = message["command"].toString();

    if (type == "response" || type == "sync_response") {
        qint64 id = message["id"].toVariant().toLongLong();
        QVariant result = message["result"].toVariant();

        QMutexLocker locker(&m_commandMutex);
        if (!m_pendingCommands.isEmpty() && m_pendingCommands.head().first == id) {
            // Update the result for the pending command and wake up waiting thread
            m_pendingCommands.head().second = result;
            m_commandWaitCondition.wakeAll();
        } else {
            qWarning() << "PlaywrightEngineBackend: Received unexpected response for ID:" << id;
        }
    } else if (type == "event") {
        // Handle events emitted by the Playwright backend
        QVariantMap eventData = message["data"].toObject().toVariantMap();

        if (command == "loadFinished") {
            bool success = eventData.value("success").toBool();
            QUrl eventUrl = QUrl(eventData.value("url").toString());
            m_currentUrl = eventUrl;
            emit loadFinished(success, eventUrl);
        } else if (command == "urlChanged") {
            QUrl eventUrl = QUrl(eventData.value("url").toString());
            m_currentUrl = eventUrl;
            emit urlChanged(eventUrl);
        } else if (command == "titleChanged") {
            m_currentTitle = eventData.value("title").toString();
            emit titleChanged(m_currentTitle);
        } else if (command == "contentsChanged") {
            // Trigger update of HTML and PlainText
            m_currentHtml = eventData.value("html").toString(); // Backend should send updated HTML
            m_currentPlainText = eventData.value("plainText").toString(); // Backend should send updated PlainText
            emit contentsChanged();
        } else if (command == "loadingProgress") {
            emit loadingProgress(eventData.value("progress").toInt());
        } else if (command == "javaScriptConsoleMessage") {
            emit javaScriptConsoleMessageSent(eventData.value("message").toString());
        } else if (command == "javaScriptError") {
            emit javaScriptErrorSent(eventData.value("message").toString(), eventData.value("lineNumber").toInt(),
                eventData.value("sourceID").toString(), eventData.value("stack").toString());
        } else if (command == "pageCreated") {
            // This is complex: need to instantiate a new PlaywrightEngineBackend for the new page
            // and pass it to WebPage::handleEnginePageCreated.
            // For now, just log. The Node.js side would need to send some unique ID for the new page.
            qWarning() << "PlaywrightEngineBackend: Received 'pageCreated' event. Full implementation pending.";
            // Example: emit pageCreated(new PlaywrightEngineBackend(this, newPageId));
        } else if (command == "initialized") {
            emit initialized();
        } else {
            qWarning() << "PlaywrightEngineBackend: Unhandled event type:" << command;
        }
    } else {
        qWarning() << "PlaywrightEngineBackend: Unhandled message type:" << type;
    }
}

// --- QProcess Signal Handlers ---

void PlaywrightEngineBackend::handleReadyReadStandardOutput() {
    m_outputBuffer.append(m_playwrightProcess->readAllStandardOutput());

    // Process messages from the buffer
    while (true) {
        // Look for the newline character indicating the end of the length prefix
        qint64 newlineIndex = m_outputBuffer.indexOf('\n');
        if (newlineIndex == -1) {
            break; // No full length prefix yet
        }

        // Parse the message length
        QByteArray lengthBytes = m_outputBuffer.left(newlineIndex);
        bool ok;
        qint64 messageLength = lengthBytes.toLongLong(&ok);

        if (!ok || messageLength < 0) {
            qWarning() << "PlaywrightEngineBackend: Invalid message length received. Clearing buffer.";
            m_outputBuffer.clear(); // Clear corrupted buffer
            m_nextMessageSize = 0;
            break;
        }

        // Ensure we have enough data for the full message
        if (m_outputBuffer.size() < newlineIndex + 1 + messageLength) {
            m_nextMessageSize = messageLength; // Store expected size for next read
            break; // Not enough data for the full message yet
        }

        // Extract the full JSON message
        QByteArray jsonMessage = m_outputBuffer.mid(newlineIndex + 1, messageLength);

        // Remove the processed message from the buffer
        m_outputBuffer.remove(0, newlineIndex + 1 + messageLength);
        m_nextMessageSize = 0; // Reset for next message

        // Parse and process the JSON
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(jsonMessage, &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            qWarning() << "PlaywrightEngineBackend: JSON parse error:" << parseError.errorString();
            continue; // Skip this message and try the next
        }

        if (doc.isObject()) {
            processIncomingMessage(doc.object());
        } else {
            qWarning() << "PlaywrightEngineBackend: Received non-object JSON message.";
        }
    }
}

void PlaywrightEngineBackend::handleReadyReadStandardError() {
    QByteArray errorOutput = m_playwrightProcess->readAllStandardError();
    // Forward error output to console for debugging Node.js process
    Terminal::instance()->cerr("[Playwright-ERR]: " + QString::fromUtf8(errorOutput).trimmed());
}

void PlaywrightEngineBackend::handleProcessStarted() {
    qDebug() << "PlaywrightEngineBackend: Node.js process started successfully.";
    // Send initial handshake or setup command to Node.js backend
    sendAsyncCommand("init"); // Tell Node.js backend to initialize browser
}

void PlaywrightEngineBackend::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    qWarning() << "PlaywrightEngineBackend: Node.js process finished with exit code" << exitCode << "and status"
               << (exitStatus == QProcess::NormalExit ? "Normal" : "Crash");
    // Emit a loadFinished(false) or similar to any pending pages
    // Also, potentially recreate the process if it's an unexpected crash
}

void PlaywrightEngineBackend::handleProcessErrorOccurred(QProcess::ProcessError error) {
    qCritical() << "PlaywrightEngineBackend: QProcess error occurred:" << error;
    // Handle specific errors (e.g., QProcess::FailedToStart)
}

// --- IEngineBackend Method Implementations (Minimal Stubs) ---

void PlaywrightEngineBackend::load(
    const QNetworkRequest& request, QNetworkAccessManager::Operation operation, const QByteArray& body) {
    qDebug() << "PlaywrightEngineBackend: load called for URL:" << request.url().toDisplayString()
             << "Operation:" << operation << "Body size:" << body.size();
    QVariantMap params;
    params["url"] = request.url().toString();
    params["operation"] = (int)operation; // Cast enum to int
    params["body"] = QString::fromUtf8(body.toBase64()); // Send body as base64 string
    QVariantMap headers;
    for (const QPair<QByteArray, QByteArray>& header : request.rawHeaderList()) {
        headers[QString::fromUtf8(header.first)] = QString::fromUtf8(header.second);
    }
    params["headers"] = headers;
    sendAsyncCommand("load", params);
    // Dummy load started/finished for immediate feedback
    emit loadStarted(request.url());
    emit loadFinished(true, request.url()); // Will be replaced by actual Playwright events
}

void PlaywrightEngineBackend::setHtml(const QString& html, const QUrl& baseUrl) {
    qDebug() << "PlaywrightEngineBackend: setHtml called. Base URL:" << baseUrl.toDisplayString();
    m_currentHtml = html; // Cache locally
    m_currentUrl = baseUrl; // Update current URL
    QVariantMap params;
    params["html"] = html;
    params["baseUrl"] = baseUrl.toString();
    sendAsyncCommand("setHtml", params);
    emit loadFinished(true, baseUrl); // Simulate completion
}

void PlaywrightEngineBackend::reload() {
    qDebug() << "PlaywrightEngineBackend: reload called.";
    sendAsyncCommand("reload");
    emit loadStarted(url());
    emit loadFinished(true, url());
}

void PlaywrightEngineBackend::stop() {
    qDebug() << "PlaywrightEngineBackend: stop called.";
    sendAsyncCommand("stop");
}

bool PlaywrightEngineBackend::canGoBack() const {
    qDebug() << "PlaywrightEngineBackend: canGoBack called (stub).";
    // This would require a sync command to Playwright for history state
    return sendSyncCommand("canGoBack").toBool();
}

bool PlaywrightEngineBackend::goBack() {
    qDebug() << "PlaywrightEngineBackend: goBack called (stub).";
    // This would require a sync command to Playwright
    return sendSyncCommand("goBack").toBool();
}

bool PlaywrightEngineBackend::canGoForward() const {
    qDebug() << "PlaywrightEngineBackend: canGoForward called (stub).";
    return sendSyncCommand("canGoForward").toBool();
}

bool PlaywrightEngineBackend::goForward() {
    qDebug() << "PlaywrightEngineBackend: goForward called (stub).";
    return sendSyncCommand("goForward").toBool();
}

bool PlaywrightEngineBackend::goToHistoryItem(int relativeIndex) {
    qDebug() << "PlaywrightEngineBackend: goToHistoryItem called (stub).";
    QVariantMap params;
    params["relativeIndex"] = relativeIndex;
    return sendSyncCommand("goToHistoryItem", params).toBool();
}

QString PlaywrightEngineBackend::toHtml() const {
    qDebug() << "PlaywrightEngineBackend: toHtml called (stub).";
    // This should ideally fetch from Playwright, but for stub, return cached or dummy
    if (m_currentHtml.isEmpty() || m_currentHtml == "<html><body><h1>Loading...</h1></body></html>") {
        return sendSyncCommand("getHtml").toString(); // Fetch actual HTML
    }
    return m_currentHtml;
}

QString PlaywrightEngineBackend::title() const {
    qDebug() << "PlaywrightEngineBackend: title called (stub).";
    if (m_currentTitle == "Loading...") {
        return sendSyncCommand("getTitle").toString(); // Fetch actual title
    }
    return m_currentTitle;
}

QUrl PlaywrightEngineBackend::url() const {
    qDebug() << "PlaywrightEngineBackend: url called (stub).";
    // This would ideally be updated via events from Playwright.
    // For a stub, we might fetch it synchronously.
    if (m_currentUrl.isEmpty() || m_currentUrl == QUrl("about:blank")) {
        return QUrl(sendSyncCommand("getUrl").toString());
    }
    return m_currentUrl;
}

QString PlaywrightEngineBackend::toPlainText() const {
    qDebug() << "PlaywrightEngineBackend: toPlainText called (stub).";
    if (m_currentPlainText.isEmpty() || m_currentPlainText == "Loading...") {
        return sendSyncCommand("getPlainText").toString();
    }
    return m_currentPlainText;
}

QVariant PlaywrightEngineBackend::evaluateJavaScript(const QString& script) {
    qDebug() << "PlaywrightEngineBackend: evaluateJavaScript called (stub). Script length:" << script.length();
    QVariantMap params;
    params["script"] = script;
    return sendSyncCommand("evaluateJs", params);
}

bool PlaywrightEngineBackend::injectJavaScriptFile(
    const QString& filePath, const QString& encoding, const QString& libraryPath, bool inPhantomScope) {
    qDebug() << "PlaywrightEngineBackend: injectJavaScriptFile called (stub). Path:" << filePath;
    QVariantMap params;
    params["filePath"] = filePath;
    params["encoding"] = encoding;
    params["libraryPath"] = libraryPath;
    params["inPhantomScope"] = inPhantomScope;
    return sendSyncCommand("injectJsFile", params).toBool();
}

void PlaywrightEngineBackend::appendScriptElement(const QString& scriptUrl) {
    qDebug() << "PlaywrightEngineBackend: appendScriptElement called (stub). URL:" << scriptUrl;
    QVariantMap params;
    params["scriptUrl"] = scriptUrl;
    sendAsyncCommand("appendScriptElement", params);
}

void PlaywrightEngineBackend::sendEvent(const QString& type, const QVariant& arg1, const QVariant& arg2,
    const QString& mouseButton, const QVariant& modifierArg) {
    qDebug() << "PlaywrightEngineBackend: sendEvent called (stub). Type:" << type;
    QVariantMap params;
    params["type"] = type;
    params["arg1"] = arg1;
    params["arg2"] = arg2;
    params["mouseButton"] = mouseButton;
    params["modifierArg"] = modifierArg;
    sendAsyncCommand("sendEvent", params);
}

void PlaywrightEngineBackend::uploadFile(const QString& selector, const QStringList& fileNames) {
    qDebug() << "PlaywrightEngineBackend: uploadFile called (stub). Selector:" << selector;
    QVariantMap params;
    params["selector"] = selector;
    params["fileNames"] = fileNames;
    sendAsyncCommand("uploadFile", params);
}

void PlaywrightEngineBackend::applySettings(const QVariantMap& settings) {
    qDebug() << "PlaywrightEngineBackend: applySettings called (stub). Settings count:" << settings.count();
    QVariantMap params;
    for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
        params[it.key()] = it.value();
    }
    sendAsyncCommand("applySettings", params);
    // Update cached settings if applicable
    if (settings.contains("userAgent"))
        setUserAgent(settings["userAgent"].toString());
    // ... update other cached properties based on settings
}

QString PlaywrightEngineBackend::userAgent() const {
    qDebug() << "PlaywrightEngineBackend: userAgent called (stub).";
    if (m_userAgent.isEmpty() || m_userAgent == "PhantomJS/3.0.0 (Custom Playwright Backend)") {
        return sendSyncCommand("getUserAgent").toString();
    }
    return m_userAgent;
}

void PlaywrightEngineBackend::setUserAgent(const QString& ua) {
    qDebug() << "PlaywrightEngineBackend: setUserAgent called (stub). UA:" << ua;
    m_userAgent = ua;
    QVariantMap params;
    params["userAgent"] = ua;
    sendAsyncCommand("setUserAgent", params);
}

QSize PlaywrightEngineBackend::viewportSize() const {
    qDebug() << "PlaywrightEngineBackend: viewportSize called (stub).";
    QVariantMap sizeMap = sendSyncCommand("getViewportSize").toMap();
    if (sizeMap.contains("width") && sizeMap.contains("height")) {
        return QSize(sizeMap["width"].toInt(), sizeMap["height"].toInt());
    }
    return m_viewportSize;
}

void PlaywrightEngineBackend::setViewportSize(const QSize& size) {
    qDebug() << "PlaywrightEngineBackend: setViewportSize called (stub). Size:" << size;
    m_viewportSize = size;
    QVariantMap params;
    params["width"] = size.width();
    params["height"] = size.height();
    sendAsyncCommand("setViewportSize", params);
}

QRect PlaywrightEngineBackend::clipRect() const {
    qDebug() << "PlaywrightEngineBackend: clipRect called (stub).";
    QVariantMap rectMap = sendSyncCommand("getClipRect").toMap();
    if (rectMap.contains("x") && rectMap.contains("y") && rectMap.contains("width") && rectMap.contains("height")) {
        return QRect(rectMap["x"].toInt(), rectMap["y"].toInt(), rectMap["width"].toInt(), rectMap["height"].toInt());
    }
    return m_clipRect;
}

void PlaywrightEngineBackend::setClipRect(const QRect& rect) {
    qDebug() << "PlaywrightEngineBackend: setClipRect called (stub). Rect:" << rect;
    m_clipRect = rect;
    QVariantMap params;
    params["x"] = rect.x();
    params["y"] = rect.y();
    params["width"] = rect.width();
    params["height"] = rect.height();
    sendAsyncCommand("setClipRect", params);
}

QPoint PlaywrightEngineBackend::scrollPosition() const {
    qDebug() << "PlaywrightEngineBackend: scrollPosition called (stub).";
    QVariantMap posMap = sendSyncCommand("getScrollPosition").toMap();
    if (posMap.contains("x") && posMap.contains("y")) {
        return QPoint(posMap["x"].toInt(), posMap["y"].toInt());
    }
    return m_scrollPosition;
}

void PlaywrightEngineBackend::setScrollPosition(const QPoint& pos) {
    qDebug() << "PlaywrightEngineBackend: setScrollPosition called (stub). Pos:" << pos;
    m_scrollPosition = pos;
    QVariantMap params;
    params["x"] = pos.x();
    params["y"] = pos.y();
    sendAsyncCommand("setScrollPosition", params);
}

void PlaywrightEngineBackend::setNavigationLocked(bool lock) {
    qDebug() << "PlaywrightEngineBackend: setNavigationLocked called (stub). Lock:" << lock;
    m_navigationLocked = lock;
    QVariantMap params;
    params["locked"] = lock;
    sendAsyncCommand("setNavigationLocked", params);
}

bool PlaywrightEngineBackend::navigationLocked() const {
    qDebug() << "PlaywrightEngineBackend: navigationLocked called (stub).";
    return m_navigationLocked;
}

QVariantMap PlaywrightEngineBackend::customHeaders() const {
    qDebug() << "PlaywrightEngineBackend: customHeaders called (stub).";
    if (m_customHeaders.isEmpty()) {
        return sendSyncCommand("getCustomHeaders").toMap();
    }
    return m_customHeaders;
}

void PlaywrightEngineBackend::setCustomHeaders(const QVariantMap& headers) {
    qDebug() << "PlaywrightEngineBackend: setCustomHeaders called (stub). Headers count:" << headers.count();
    m_customHeaders = headers;
    QVariantMap params;
    params["headers"] = headers;
    sendAsyncCommand("setCustomHeaders", params);
}

qreal PlaywrightEngineBackend::zoomFactor() const {
    qDebug() << "PlaywrightEngineBackend: zoomFactor called (stub).";
    if (m_zoomFactor == 1.0) {
        return sendSyncCommand("getZoomFactor").toReal();
    }
    return m_zoomFactor;
}

void PlaywrightEngineBackend::setZoomFactor(qreal zoom) {
    qDebug() << "PlaywrightEngineBackend: setZoomFactor called (stub). Zoom:" << zoom;
    m_zoomFactor = zoom;
    QVariantMap params;
    params["zoom"] = zoom;
    sendAsyncCommand("setZoomFactor", params);
}

QString PlaywrightEngineBackend::windowName() const {
    qDebug() << "PlaywrightEngineBackend: windowName called (stub).";
    if (m_windowName.isEmpty()) {
        return sendSyncCommand("getWindowName").toString();
    }
    return m_windowName;
}

QString PlaywrightEngineBackend::offlineStoragePath() const {
    qDebug() << "PlaywrightEngineBackend: offlineStoragePath called (stub).";
    return sendSyncCommand("getOfflineStoragePath").toString();
}

int PlaywrightEngineBackend::offlineStorageQuota() const {
    qDebug() << "PlaywrightEngineBackend: offlineStorageQuota called (stub).";
    return sendSyncCommand("getOfflineStorageQuota").toInt();
}

QString PlaywrightEngineBackend::localStoragePath() const {
    qDebug() << "PlaywrightEngineBackend: localStoragePath called (stub).";
    return sendSyncCommand("getLocalStoragePath").toString();
}

int PlaywrightEngineBackend::localStorageQuota() const {
    qDebug() << "PlaywrightEngineBackend: localStorageQuota called (stub).";
    return sendSyncCommand("getLocalStorageQuota").toInt();
}

QImage PlaywrightEngineBackend::renderImage(const QRect& clipRect, bool onlyViewport, const QPoint& scrollPosition) {
    qDebug() << "PlaywrightEngineBackend: renderImage called (stub).";
    // For a stub, return a blank image or a dummy one
    QSize renderSize = onlyViewport ? m_viewportSize : QSize(800, 600); // Dummy size
    if (!clipRect.isNull()) {
        renderSize = clipRect.size();
    }
    QImage dummyImage(renderSize, QImage::Format_ARGB32);
    dummyImage.fill(Qt::blue); // Make it clearly a dummy
    QPainter painter(&dummyImage);
    painter.drawText(dummyImage.rect(), Qt::AlignCenter, "Render Image Stub");
    painter.end();
    return dummyImage;

    // Real implementation would send a command:
    // QVariantMap params;
    // params["clipRect"] = QVariantMap{{"x", clipRect.x()}, {"y", clipRect.y()}, {"width", clipRect.width()},
    // {"height", clipRect.height()}}; params["onlyViewport"] = onlyViewport; params["scrollPosition"] =
    // QVariantMap{{"x", scrollPosition.x()}, {"y", scrollPosition.y()}}; QByteArray base64Image =
    // sendSyncCommand("renderImage", params).toByteArray(); QImage image;
    // image.loadFromData(QByteArray::fromBase64(base64Image));
    // return image;
}

QByteArray PlaywrightEngineBackend::renderPdf(const QVariantMap& paperSize, const QRect& clipRect) {
    qDebug() << "PlaywrightEngineBackend: renderPdf called (stub).";
    // For a stub, return dummy PDF data
    QByteArray pdfData;
    QBuffer buffer(&pdfData);
    buffer.open(QIODevice::WriteOnly);
    QPdfWriter pdfWriter(&buffer);
    pdfWriter.setPageSize(QPageSize(QPageSize::A4));
    QPainter painter(&pdfWriter);
    painter.drawText(pdfWriter.pageRect(), Qt::AlignCenter, "Render PDF Stub");
    painter.end();
    return pdfData;

    // Real implementation would send a command:
    // QVariantMap params;
    // params["paperSize"] = paperSize;
    // params["clipRect"] = QVariantMap{{"x", clipRect.x()}, {"y", clipRect.y()}, {"width", clipRect.width()},
    // {"height", clipRect.height()}}; return sendSyncCommand("renderPdf", params).toByteArray();
}

void PlaywrightEngineBackend::setCookieJar(CookieJar* cookieJar) {
    qDebug() << "PlaywrightEngineBackend: setCookieJar called (stub).";
    // This method is primarily to inform the backend about the cookie jar
    // The Playwright backend will need to read cookies from this jar and synchronize.
    // For now, just send the current cookies from the jar.
    if (cookieJar) {
        setCookies(cookieJar->allCookiesToMap());
    }
}

QVariantList PlaywrightEngineBackend::cookies() const {
    qDebug() << "PlaywrightEngineBackend: cookies called (stub).";
    return sendSyncCommand("getCookies").toList();
}

bool PlaywrightEngineBackend::setCookies(const QVariantList& cookies) {
    qDebug() << "PlaywrightEngineBackend: setCookies called (stub).";
    QVariantMap params;
    params["cookies"] = cookies;
    return sendSyncCommand("setCookies", params).toBool();
}

bool PlaywrightEngineBackend::addCookie(const QVariantMap& cookie) {
    qDebug() << "PlaywrightEngineBackend: addCookie called (stub).";
    QVariantMap params;
    params["cookie"] = cookie;
    return sendSyncCommand("addCookie", params).toBool();
}

bool PlaywrightEngineBackend::deleteCookie(const QString& cookieName) {
    qDebug() << "PlaywrightEngineBackend: deleteCookie called (stub).";
    QVariantMap params;
    params["cookieName"] = cookieName;
    return sendSyncCommand("deleteCookie", params).toBool();
}

bool PlaywrightEngineBackend::clearCookies() {
    qDebug() << "PlaywrightEngineBackend: clearCookies called (stub).";
    return sendSyncCommand("clearCookies").toBool();
}

void PlaywrightEngineBackend::setNetworkProxy(const QNetworkProxy& proxy) {
    qDebug() << "PlaywrightEngineBackend: setNetworkProxy called (stub). Host:" << proxy.hostName();
    QVariantMap params;
    params["host"] = proxy.hostName();
    params["port"] = proxy.port();
    params["type"] = proxy.type() == QNetworkProxy::HttpProxy
        ? "http"
        : (proxy.type() == QNetworkProxy::Socks5Proxy ? "socks5" : "none");
    params["user"] = proxy.user();
    params["password"] = proxy.password();
    sendAsyncCommand("setNetworkProxy", params);
}

void PlaywrightEngineBackend::clearMemoryCache() {
    qDebug() << "PlaywrightEngineBackend: clearMemoryCache called (stub).";
    sendAsyncCommand("clearMemoryCache");
}

void PlaywrightEngineBackend::setIgnoreSslErrors(bool ignore) {
    qDebug() << "PlaywrightEngineBackend: setIgnoreSslErrors called (stub). Ignore:" << ignore;
    QVariantMap params;
    params["ignore"] = ignore;
    sendAsyncCommand("setIgnoreSslErrors", params);
}

void PlaywrightEngineBackend::setSslProtocol(const QString& protocolName) {
    qDebug() << "PlaywrightEngineBackend: setSslProtocol called (stub). Protocol:" << protocolName;
    QVariantMap params;
    params["protocolName"] = protocolName;
    sendAsyncCommand("setSslProtocol", params);
}

void PlaywrightEngineBackend::setSslCiphers(const QString& ciphers) {
    qDebug() << "PlaywrightEngineBackend: setSslCiphers called (stub). Ciphers:" << ciphers;
    QVariantMap params;
    params["ciphers"] = ciphers;
    sendAsyncCommand("setSslCiphers", params);
}

void PlaywrightEngineBackend::setSslCertificatesPath(const QString& path) {
    qDebug() << "PlaywrightEngineBackend: setSslCertificatesPath called (stub). Path:" << path;
    QVariantMap params;
    params["path"] = path;
    sendAsyncCommand("setSslCertificatesPath", params);
}

void PlaywrightEngineBackend::setSslClientCertificateFile(const QString& path) {
    qDebug() << "PlaywrightEngineBackend: setSslClientCertificateFile called (stub). Path:" << path;
    QVariantMap params;
    params["path"] = path;
    sendAsyncCommand("setSslClientCertificateFile", params);
}

void PlaywrightEngineBackend::setSslClientKeyFile(const QString& path) {
    qDebug() << "PlaywrightEngineBackend: setSslClientKeyFile called (stub). Path:" << path;
    QVariantMap params;
    params["path"] = path;
    sendAsyncCommand("setSslClientKeyFile", params);
}

void PlaywrightEngineBackend::setSslClientKeyPassphrase(const QByteArray& passphrase) {
    qDebug() << "PlaywrightEngineBackend: setSslClientKeyPassphrase called (stub). Passphrase length:"
             << passphrase.length();
    QVariantMap params;
    params["passphrase"] = QString::fromUtf8(passphrase.toBase64());
    sendAsyncCommand("setSslClientKeyPassphrase", params);
}

void PlaywrightEngineBackend::setResourceTimeout(int timeoutMs) {
    qDebug() << "PlaywrightEngineBackend: setResourceTimeout called (stub). Timeout:" << timeoutMs;
    QVariantMap params;
    params["timeoutMs"] = timeoutMs;
    sendAsyncCommand("setResourceTimeout", params);
}

void PlaywrightEngineBackend::setMaxAuthAttempts(int attempts) {
    qDebug() << "PlaywrightEngineBackend: setMaxAuthAttempts called (stub). Attempts:" << attempts;
    QVariantMap params;
    params["attempts"] = attempts;
    sendAsyncCommand("setMaxAuthAttempts", params);
}

int PlaywrightEngineBackend::framesCount() const {
    qDebug() << "PlaywrightEngineBackend: framesCount called (stub).";
    return sendSyncCommand("getFramesCount").toInt();
}

QStringList PlaywrightEngineBackend::framesName() const {
    qDebug() << "PlaywrightEngineBackend: framesName called (stub).";
    return sendSyncCommand("getFramesName").toStringList();
}

QString PlaywrightEngineBackend::frameName() const {
    qDebug() << "PlaywrightEngineBackend: frameName called (stub).";
    return sendSyncCommand("getFrameName").toString();
}

QString PlaywrightEngineBackend::focusedFrameName() const {
    qDebug() << "PlaywrightEngineBackend: focusedFrameName called (stub).";
    return sendSyncCommand("getFocusedFrameName").toString();
}

bool PlaywrightEngineBackend::switchToFrame(const QString& frameName) {
    qDebug() << "PlaywrightEngineBackend: switchToFrame by name called (stub). Name:" << frameName;
    QVariantMap params;
    params["name"] = frameName;
    return sendSyncCommand("switchToFrameByName", params).toBool();
}

bool PlaywrightEngineBackend::switchToFrame(int framePosition) {
    qDebug() << "PlaywrightEngineBackend: switchToFrame by position called (stub). Position:" << framePosition;
    QVariantMap params;
    params["position"] = framePosition;
    return sendSyncCommand("switchToFrameByPosition", params).toBool();
}

void PlaywrightEngineBackend::switchToMainFrame() {
    qDebug() << "PlaywrightEngineBackend: switchToMainFrame called (stub).";
    sendAsyncCommand("switchToMainFrame");
}

bool PlaywrightEngineBackend::switchToParentFrame() {
    qDebug() << "PlaywrightEngineBackend: switchToParentFrame called (stub).";
    return sendSyncCommand("switchToParentFrame").toBool();
}

void PlaywrightEngineBackend::switchToFocusedFrame() {
    qDebug() << "PlaywrightEngineBackend: switchToFocusedFrame called (stub).";
    sendAsyncCommand("switchToFocusedFrame");
}

void PlaywrightEngineBackend::exposeQObject(const QString& name, QObject* object) {
    qDebug() << "PlaywrightEngineBackend: exposeQObject called. Name:" << name << "Object:" << object->objectName();

    QVariantList invokableMethodNames;
    const QMetaObject* meta = object->metaObject();
    for (int i = meta->methodOffset(); i < meta->methodCount(); ++i) {
        QMetaMethod method = meta->method(i);
        // Only expose invokable slots
        if (method.methodType() == QMetaMethod::Slot && method.accessSpecifier() == QMetaMethod::Public
            && method.tag() != QMetaMethod::Signal) {
            // Get method name without arguments
            QString methodName = QString::fromLatin1(method.methodSignature()).split('(').first();
            // Exclude internal PhantomJS methods (starting with '_') if not meant for user JS
            if (!methodName.startsWith('_')) {
                invokableMethodNames.append(methodName);
            }
        }
    }

    QVariantMap exposedProperties;
    for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i) {
        QMetaProperty prop = meta->property(i);
        // Only expose scriptable properties
        if (prop.isScriptable() && prop.isValid()) {
            QString propertyName = QString::fromLatin1(prop.name());
            // Exclude internal properties (starting with '_')
            if (!propertyName.startsWith('_')) {
                // For now, we'll just send the name, not the value. The JS side will then 'get' the value.
                exposedProperties[propertyName] = prop.typeName(); // Send type name for introspection
            }
        }
    }

    QVariantMap params;
    params["objectName"] = name;
    params["methods"] = invokableMethodNames;
    params["properties"] = exposedProperties; // List properties that can be gotten/set

    // Store a QPointer to the object internally if we need to call its methods from IPC responses
    // For now, we'll assume the Playwright backend will directly handle callPhantom / _repl calls
    // and route them back to the C++ 'Phantom' or 'REPL' instance.
    // A map like QMap<QString, QObject*> m_exposedObjects; would store them.

    sendAsyncCommand("exposeObject", params);
}

void PlaywrightEngineBackend::clickElement(const QString& selector) {
    qDebug() << "PlaywrightEngineBackend: clickElement called (stub). Selector:" << selector;
    QVariantMap params;
    params["selector"] = selector;
    sendAsyncCommand("clickElement", params);
}

int PlaywrightEngineBackend::showInspector(int remotePort) {
    qDebug() << "PlaywrightEngineBackend: showInspector called (stub). Port:" << remotePort;
    // Playwright does not have an embedded inspector in the same way QtWebKit did.
    // This would typically involve launching Playwright in non-headless mode or
    // printing instructions for connecting Chrome DevTools to the Playwright browser.
    // For now, return a dummy port.
    QVariantMap params;
    params["remotePort"] = remotePort;
    return sendSyncCommand("showInspector", params).toInt();
}
