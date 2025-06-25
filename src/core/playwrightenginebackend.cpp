#include "playwrightenginebackend.h"

#include "consts.h" // For PAGE_SETTINGS constants
#include "terminal.h" // For console output

#include <QCoreApplication> // For QCoreApplication::applicationDirPath()
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QProcess>
#include <QDebug>
#include <QFile> // For reading files (e.g., injectJsFile)
#include <QImage> // For loading rendered images
#include <QBuffer> // For PDF data handling
#include <QPainter> // For dummy PDF rendering (removed in final, but useful for debug)
#include <QPdfWriter> // For dummy PDF rendering (removed in final, but useful for debug)
#include <QScreen> // For screen DPI
#include <QDateTime> // For generating unique IDs for sync commands
#include <QMetaMethod> // For introspection in exposeQObject
#include <QMetaProperty> // For introspection in exposeQObject
#include <QRegularExpression> // For parsing stack traces

// Helper function to convert QVariantList to a method signature string
// Used for QMetaObject::indexOfMethod matching
static QByteArray qVariantListToSignature(const QVariantList& args) {
    QStringList types;
    for (const QVariant& arg : args) {
        types << QLatin1String(arg.typeName());
    }
    return QString("(%1)").arg(types.join(',')).toLatin1();
}


// Constructor: Launches the Node.js Playwright subprocess
PlaywrightEngineBackend::PlaywrightEngineBackend(QObject* parent)
    : IEngineBackend(parent)
    , m_playwrightProcess(new QProcess(this))
    , m_nextMessageSize(0)
    , m_navigationLocked(false)
{
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
    // Assume it's in the same directory as the executable, or a 'nodejs' subfolder.
    QString nodeScriptPath = QCoreApplication::applicationDirPath() + "/playwright_backend.js";

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

    // Initial dummy values for properties, will be updated by Playwright events
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

PlaywrightEngineBackend::~PlaywrightEngineBackend()
{
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
void PlaywrightEngineBackend::sendAsyncCommand(const QString& command, const QVariantMap& params)
{
    QJsonObject message;
    message["type"] = "command";
    message["command"] = command;
    if (!params.isEmpty()) {
        message["params"] = QJsonObject::fromVariantMap(params);
    }

    QJsonDocument doc(message);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact); // Compact for single line
    QByteArray messageWithLength = QByteArray::number(jsonData.size()) + "\n" + jsonData; // Prepend length and newline

    // qDebug() << "PlaywrightEngineBackend: Sending async command:" << messageWithLength.left(200) << "..."; // Truncate for log readability
    m_playwrightProcess->write(messageWithLength);
    m_playwrightProcess->waitForBytesWritten(); // Ensure data is sent
}

// Send a synchronous command and wait for a response
QVariant PlaywrightEngineBackend::sendSyncCommand(const QString& command, const QVariantMap& params)
{
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

    // qDebug() << "PlaywrightEngineBackend: Sending sync command:" << messageWithLength.left(200) << "...";
    m_playwrightProcess->write(messageWithLength);
    m_playwrightProcess->waitForBytesWritten();

    // Wait for the response for this specific command ID
    while (m_pendingCommands.head().first != commandId || m_pendingCommands.head().second.isNull()) {
        // Use a timeout to prevent infinite blocking if the Node.js process hangs
        if (!m_commandWaitCondition.wait(locker.mutex(), 30000)) { // 30-second timeout
            qWarning() << "PlaywrightEngineBackend: Timeout waiting for sync response for command:" << command << "ID:" << commandId;
            // Optionally, handle timeout by returning an error variant or re-throwing
            return QVariant(); // Return empty QVariant on timeout
        }
    }

    QVariant result = m_pendingCommands.dequeue().second;
    return result;
}

// Send a synchronous response back to the Node.js process (for JS-initiated C++ callbacks)
void PlaywrightEngineBackend::sendSyncResponse(qint64 id, const QVariant& result) {
    QJsonObject message;
    message["type"] = "sync_response_from_cpp_callback";
    message["id"] = id;
    message["result"] = QJsonValue::fromVariant(result);

    QJsonDocument doc(message);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
    QByteArray messageWithLength = QByteArray::number(jsonData.size()) + "\n" + jsonData;

    // qDebug() << "PlaywrightEngineBackend: Sending sync response to JS:" << messageWithLength.left(200) << "...";
    m_playwrightProcess->write(messageWithLength);
    m_playwrightProcess->waitForBytesWritten();
}


// Process incoming JSON messages from Node.js stdout
void PlaywrightEngineBackend::processIncomingMessage(const QJsonObject& message)
{
    QString type = message["type"].toString();
    QString command = message["command"].toString();
    qint64 messageId = message.value("id").toVariant().toLongLong(); // Get ID, if present

    // qDebug() << "PlaywrightEngineBackend: Processing message - Type:" << type << "Command:" << command << "ID:" << messageId;

    if (type == "response" || type == "sync_response") {
        qint64 id = message["id"].toVariant().toLongLong();
        QVariant result = message["result"].toVariant();
        // Check for error field
        if (message.contains("success") && !message["success"].toBool()) {
            QString errorString = message["error"].toString();
            qWarning() << "PlaywrightEngineBackend: Received error response for ID:" << id << "Error:" << errorString;
            result = QVariant(errorString); // Propagate error message
        }

        QMutexLocker locker(&m_commandMutex);
        // Find and update the specific pending command
        bool found = false;
        for (int i = 0; i < m_pendingCommands.size(); ++i) {
            if (m_pendingCommands[i].first == id) {
                m_pendingCommands[i].second = result;
                m_commandWaitCondition.wakeAll(); // Wake all waiting threads
                found = true;
                break;
            }
        }
        if (!found) {
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
            // Backend should proactively send content update if it's available
            // If not, we might trigger a sync call to toHtml()/toPlainText()
            // This event implies we should refresh the cached content
            // However, the current `playwright_backend.js` doesn't send html/plainText with this event.
            // So for now, we'll rely on explicit calls to toHtml()/toPlainText() from WebPage.
            qDebug() << "PlaywrightEngineBackend: Received 'contentsChanged' event. WebPage should re-fetch content.";
            emit contentsChanged();
        } else if (command == "loadingProgress") {
            emit loadingProgress(eventData.value("progress").toInt());
        } else if (command == "javaScriptConsoleMessage") {
            emit javaScriptConsoleMessageSent(eventData.value("message").toString());
        } else if (command == "javaScriptError") {
            emit javaScriptErrorSent(eventData.value("message").toString(), eventData.value("lineNumber").toInt(),
                                     eventData.value("sourceID").toString(), eventData.value("stack").toString());
        } else if (command == "pageCreated") {
            // This event notifies us that a new browser page (e.g., from window.open) has been created.
            // The Playwright backend implicitly handles the new Page object.
            // We need to tell the C++ side (WebPage) to create a new WebPage instance for it.
            // The PlaywrightEngineBackend for this new page would need its own IPC pipe or a way to target it.
            // For now, we'll assume the newPageBackend passed here is a logical identifier.
            // A more robust solution would involve passing a unique page ID from Node.js and
            // creating a *new* PlaywrightEngineBackend instance in C++ associated with that ID,
            // which then communicates through a shared IPC channel but targets commands to specific page IDs.
            qWarning() << "PlaywrightEngineBackend: Received 'pageCreated' event. Full multi-page implementation pending.";
            // If newPageBackend would be a new IEngineBackend instance for the new page:
            // emit pageCreated(new PlaywrightEngineBackend(this, "new_page_id_from_js")); // Example
        } else if (command == "initialized") {
            emit initialized(); // Signifies the backend is ready
        } else if (command == "resourceRequested") {
            emit resourceRequested(eventData, nullptr); // No QNetworkReply equivalent here for now
        } else if (command == "resourceReceived") {
            emit resourceReceived(eventData);
        } else if (command == "resourceError") {
            emit resourceError(eventData);
        } else if (command == "resourceTimeout") {
            emit resourceTimeout(eventData);
        } else if (command == "repaintRequested") {
            // Playwright repaint will typically mean a new screenshot is possible.
            // This event might carry dirty rect info, which would be in eventData.
            if (eventData.contains("x") && eventData.contains("y") && eventData.contains("width") && eventData.contains("height")) {
                emit repaintRequested(QRect(eventData["x"].toInt(), eventData["y"].toInt(), eventData["width"].toInt(), eventData["height"].toInt()));
            } else {
                // If no specific rect, assume full repaint
                emit repaintRequested(QRect(0, 0, m_viewportSize.width(), m_viewportSize.height()));
            }
        } else if (command == "windowCloseRequested") {
            emit windowCloseRequested();
        } else if (command == "frameNavigated") {
            // This event typically updates the current frame context
            m_currentFrameName = eventData.value("name").toString();
            // This signal is not defined in IEngineBackend, but could be useful
            // emit frameNavigated(eventData.value("url").toString(), eventData.value("name").toString());
        } else {
            qWarning() << "PlaywrightEngineBackend: Unhandled event type:" << command;
        }
    } else if (type == "sync_command_to_cpp") { // This is a new type: JS in Playwright backend calls C++ synchronously
        QString commandName = message["command"].toString();
        QVariantMap data = message["data"].toObject().toVariantMap();
        qint64 responseId = message["id"].toVariant().toLongLong(); // ID to send back response

        QVariant result;
        bool success = true;
        QString errorString;

        if (commandName == "callExposedQObjectMethod") {
            QString objectName = data["objectName"].toString();
            QString methodName = data["methodName"].toString();
            QVariantList args = data["args"].toList();

            QObject* targetObject = m_exposedObjects.value(objectName);
            if (targetObject) {
                const QMetaObject* meta = targetObject->metaObject();

                // 1. Try as a Q_PROPERTY getter/setter
                QMetaProperty prop = meta->property(meta->indexOfProperty(methodName.toLatin1().constData()));
                if (prop.isValid()) {
                    if (args.isEmpty()) { // It's a getter
                        result = prop.read(targetObject);
                        qDebug() << "PlaywrightEngineBackend: Invoked Q_PROPERTY getter:" << objectName << "." << methodName << "-> Result:" << result;
                    } else if (args.size() == 1) { // It's a setter
                        success = prop.write(targetObject, args.first());
                        result = success; // Return true/false for setter success
                        qDebug() << "PlaywrightEngineBackend: Invoked Q_PROPERTY setter:" << objectName << "." << methodName << "=" << args.first() << "(Success:" << success << ")";
                    } else {
                        success = false;
                        errorString = "Invalid number of arguments for property access (expected 0 for getter, 1 for setter).";
                    }
                } else {
                    // 2. Try as a Q_INVOKABLE method
                    // Prepare arguments for QMetaObject::invokeMethod.
                    // This is the most complex part as QMetaObject::invokeMethod doesn't directly take QVariantList
                    // unless the method signature explicitly defines `const QVariantList&` as an argument.
                    // We'll try to match by signature or invoke with QGenericArgument.

                    // Attempt 1: Look for exact signature (e.g., method(QString,int))
                    QByteArray normalizedSignature = QMetaObject::normalizedSignature(QString("%1%2").arg(methodName).arg(qVariantListToSignature(args)).toLatin1().constData());
                    int methodIndex = meta->indexOfMethod(normalizedSignature);

                    if (methodIndex != -1) {
                         // Found method by precise signature
                         QMetaMethod method = meta->method(methodIndex);
                         QList<QByteArray> parameterTypes = method.parameterTypes();
                         if (parameterTypes.size() != args.size()) {
                             success = false;
                             errorString = QString("Method signature mismatch for '%1'.").arg(methodName);
                         } else {
                             // This is still tricky. QGenericArgument needs exact type.
                             // QVariant::constData() is generally safe only for POD types or if the QVariant
                             // holds a registered custom type.
                             // For simplicity, we try with Q_ARG for up to 10 parameters, assuming basic conversions.
                             QVariant returnVal;
                             bool invoked = false;
                             QGenericArgument genericArgs[10];

                             for (int i = 0; i < args.size() && i < 10; ++i) {
                                 // IMPORTANT: This casting is an *assumption* and can lead to crashes if types don't match.
                                 // A robust solution needs a QVariant to target-type conversion layer.
                                 // For common types (QString, int, bool, QVariantMap, QVariantList), QVariant handles it.
                                 if (parameterTypes.at(i) == "QString") {
                                     genericArgs[i] = Q_ARG(QString, args.at(i).toString());
                                 } else if (parameterTypes.at(i) == "int") {
                                     genericArgs[i] = Q_ARG(int, args.at(i).toInt());
                                 } else if (parameterTypes.at(i) == "bool") {
                                     genericArgs[i] = Q_ARG(bool, args.at(i).toBool());
                                 } else if (parameterTypes.at(i) == "QVariantMap") {
                                     genericArgs[i] = Q_ARG(QVariantMap, args.at(i).toMap());
                                 } else if (parameterTypes.at(i) == "QVariantList") {
                                     genericArgs[i] = Q_ARG(QVariantList, args.at(i).toList());
                                 } else if (parameterTypes.at(i) == "QObject*") {
                                     // Special handling for QObject*. This would involve looking up a registered QObject.
                                     // For now, assume null or handle if a known QObject proxy is passed.
                                     genericArgs[i] = Q_ARG(QObject*, args.at(i).value<QObject*>());
                                 } else {
                                     // Fallback for other types - may be unsafe
                                     genericArgs[i] = QGenericArgument(args.at(i).typeName(), args.at(i).constData());
                                 }
                             }

                             invoked = QMetaObject::invokeMethod(targetObject, methodName.toLatin1().constData(),
                                                                 Qt::DirectConnection, // or Qt::BlockingQueuedConnection if cross-thread
                                                                 Q_RETURN_ARG(QVariant, returnVal),
                                                                 genericArgs[0], genericArgs[1], genericArgs[2], genericArgs[3],
                                                                 genericArgs[4], genericArgs[5], genericArgs[6], genericArgs[7],
                                                                 genericArgs[8], genericArgs[9]); // Max 10 arguments for Q_ARG overload

                             result = returnVal;
                             success = invoked;
                             if (!invoked) {
                                 errorString = QString("Failed to invoke method '%1' (invokeMethod returned false).").arg(methodName);
                             }
                         }
                    } else {
                        // Attempt 2.1: Look for method that takes a single QVariantList (e.g., common for 'call' on Callback)
                        normalizedSignature = QMetaObject::normalizedSignature(QString("%1(QVariantList)").arg(methodName).toLatin1().constData());
                        methodIndex = meta->indexOfMethod(normalizedSignature);
                        if (methodIndex != -1 && args.size() == 1 && args.first().type() == QVariant::List) {
                             QVariant returnVal;
                             bool invoked = QMetaObject::invokeMethod(targetObject, methodName.toLatin1().constData(),
                                                                      Qt::DirectConnection,
                                                                      Q_RETURN_ARG(QVariant, returnVal),
                                                                      Q_ARG(QVariantList, args.first().toList()));
                             result = returnVal;
                             success = invoked;
                             if (!invoked) {
                                errorString = QString("Failed to invoke method '%1(QVariantList)'.").arg(methodName);
                             }
                        } else {
                            // 3. Method or property not found / no matching signature
                            success = false;
                            errorString = QString("Method or property '%1' not found or signature mismatch on exposed object '%2'.").arg(methodName).arg(objectName);
                        }
                    }
                }
            } else {
                success = false;
                errorString = QString("Exposed object '%1' not found.").arg(objectName);
            }
        } else {
            success = false;
            errorString = QString("Unknown sync_command_to_cpp command: %1").arg(commandName);
        }
        // Send response back to the Node.js backend
        sendSyncResponse(messageId, QVariantMap { { "success", success }, { "result", result }, { "error", errorString } });
    } else { // Fallback for unhandled types
        qWarning() << "PlaywrightEngineBackend: Unhandled message type:" << type;
    }
}

// --- QProcess Signal Handlers ---

void PlaywrightEngineBackend::handleReadyReadStandardOutput()
{
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

void PlaywrightEngineBackend::handleReadyReadStandardError()
{
    QByteArray errorOutput = m_playwrightProcess->readAllStandardError();
    // Forward error output to console for debugging Node.js process
    Terminal::instance()->cerr("[Playwright-ERR]: " + QString::fromUtf8(errorOutput).trimmed());
}

void PlaywrightEngineBackend::handleProcessStarted()
{
    qDebug() << "PlaywrightEngineBackend: Node.js process started successfully.";
    // Send initial handshake or setup command to Node.js backend
    sendAsyncCommand("init"); // Tell Node.js backend to initialize browser
}

void PlaywrightEngineBackend::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qWarning() << "PlaywrightEngineBackend: Node.js process finished with exit code" << exitCode << "and status"
               << (exitStatus == QProcess::NormalExit ? "Normal" : "Crash");
    // Emit a loadFinished(false) or similar to any pending pages
    // Also, potentially recreate the process if it's an unexpected crash
}

void PlaywrightEngineBackend::handleProcessErrorOccurred(QProcess::ProcessError error)
{
    qCritical() << "PlaywrightEngineBackend: QProcess error occurred:" << error;
    // Handle specific errors (e.g., QProcess::FailedToStart)
}

// --- IEngineBackend Method Implementations ---

void PlaywrightEngineBackend::load(
    const QNetworkRequest& request, QNetworkAccessManager::Operation operation, const QByteArray& body)
{
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
}

void PlaywrightEngineBackend::setHtml(const QString& html, const QUrl& baseUrl)
{
    qDebug() << "PlaywrightEngineBackend: setHtml called. Base URL:" << baseUrl.toDisplayString();
    m_currentHtml = html; // Cache locally
    m_currentUrl = baseUrl; // Update current URL
    QVariantMap params;
    params["html"] = html;
    params["baseUrl"] = baseUrl.toString();
    sendAsyncCommand("setHtml", params);
}

void PlaywrightEngineBackend::reload()
{
    qDebug() << "PlaywrightEngineBackend: reload called.";
    sendAsyncCommand("reload");
}

void PlaywrightEngineBackend::stop()
{
    qDebug() << "PlaywrightEngineBackend: stop called.";
    sendAsyncCommand("stop");
}

bool PlaywrightEngineBackend::canGoBack() const
{
    qDebug() << "PlaywrightEngineBackend: canGoBack called.";
    return sendSyncCommand("canGoBack").toBool();
}

bool PlaywrightEngineBackend::goBack()
{
    qDebug() << "PlaywrightEngineBackend: goBack called.";
    return sendSyncCommand("goBack").toBool();
}

bool PlaywrightEngineBackend::canGoForward() const
{
    qDebug() << "PlaywrightEngineBackend: canGoForward called.";
    return sendSyncCommand("canGoForward").toBool();
}

bool PlaywrightEngineBackend::goForward()
{
    qDebug() << "PlaywrightEngineBackend: goForward called.";
    return sendSyncCommand("goForward").toBool();
}

bool PlaywrightEngineBackend::goToHistoryItem(int relativeIndex)
{
    qDebug() << "PlaywrightEngineBackend: goToHistoryItem called. Index:" << relativeIndex;
    QVariantMap params;
    params["relativeIndex"] = relativeIndex;
    return sendSyncCommand("goToHistoryItem", params).toBool();
}

QString PlaywrightEngineBackend::toHtml() const
{
    qDebug() << "PlaywrightEngineBackend: toHtml called.";
    return sendSyncCommand("getHtml").toString(); // Fetch actual HTML
}

QString PlaywrightEngineBackend::title() const
{
    qDebug() << "PlaywrightEngineBackend: title called.";
    return sendSyncCommand("getTitle").toString(); // Fetch actual title
}

QUrl PlaywrightEngineBackend::url() const
{
    qDebug() << "PlaywrightEngineBackend: url called.";
    return QUrl(sendSyncCommand("getUrl").toString());
}

QString PlaywrightEngineBackend::toPlainText() const
{
    qDebug() << "PlaywrightEngineBackend: toPlainText called.";
    return sendSyncCommand("getPlainText").toString();
}

QVariant PlaywrightEngineBackend::evaluateJavaScript(const QString& script)
{
    qDebug() << "PlaywrightEngineBackend: evaluateJavaScript called. Script length:" << script.length();
    QVariantMap params;
    params["script"] = script;
    return sendSyncCommand("evaluateJs", params);
}

bool PlaywrightEngineBackend::injectJavaScriptFile(
    const QString& filePath, const QString& encoding, const QString& libraryPath, bool inPhantomScope)
{
    qDebug() << "PlaywrightEngineBackend: injectJavaScriptFile called. Path:" << filePath;
    QVariantMap params;
    params["filePath"] = filePath;
    params["encoding"] = encoding;
    params["libraryPath"] = libraryPath;
    params["inPhantomScope"] = inPhantomScope;
    return sendSyncCommand("injectJsFile", params).toBool();
}

void PlaywrightEngineBackend::appendScriptElement(const QString& scriptUrl)
{
    qDebug() << "PlaywrightEngineBackend: appendScriptElement called. URL:" << scriptUrl;
    QVariantMap params;
    params["scriptUrl"] = scriptUrl;
    sendAsyncCommand("appendScriptElement", params);
}

void PlaywrightEngineBackend::sendEvent(const QString& type, const QVariant& arg1, const QVariant& arg2,
                                        const QString& mouseButton, const QVariant& modifierArg)
{
    qDebug() << "PlaywrightEngineBackend: sendEvent called. Type:" << type;
    QVariantMap params;
    params["type"] = type;
    params["arg1"] = arg1;
    params["arg2"] = arg2;
    params["mouseButton"] = mouseButton;
    params["modifierArg"] = modifierArg;
    sendAsyncCommand("sendEvent", params);
}

void PlaywrightEngineBackend::uploadFile(const QString& selector, const QStringList& fileNames)
{
    qDebug() << "PlaywrightEngineBackend: uploadFile called. Selector:" << selector;
    QVariantMap params;
    params["selector"] = selector;
    params["fileNames"] = fileNames;
    sendAsyncCommand("uploadFile", params);
}

void PlaywrightEngineBackend::applySettings(const QVariantMap& settings)
{
    qDebug() << "PlaywrightEngineBackend: applySettings called. Settings count:" << settings.count();
    QVariantMap params;
    for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
        params[it.key()] = it.value();
    }
    sendAsyncCommand("applySettings", params);
    // Update cached settings if applicable
    if (settings.contains(PAGE_SETTINGS_USER_AGENT))
        m_userAgent = settings[PAGE_SETTINGS_USER_AGENT].toString();
    if (settings.contains(PAGE_SETTINGS_VIEWPORT_SIZE))
        m_viewportSize = settings[PAGE_SETTINGS_VIEWPORT_SIZE].toSize();
    if (settings.contains(PAGE_SETTINGS_CLIP_RECT))
        m_clipRect = settings[PAGE_SETTINGS_CLIP_RECT].toRect();
    if (settings.contains(PAGE_SETTINGS_SCROLL_POSITION))
        m_scrollPosition = settings[PAGE_SETTINGS_SCROLL_POSITION].toPoint();
    if (settings.contains(PAGE_SETTINGS_ZOOM_FACTOR))
        m_zoomFactor = settings[PAGE_SETTINGS_ZOOM_FACTOR].toReal();
    if (settings.contains(PAGE_SETTINGS_CUSTOM_HEADERS))
        m_customHeaders = settings[PAGE_SETTINGS_CUSTOM_HEADERS].toMap();
    if (settings.contains(PAGE_SETTINGS_NAVIGATION_LOCKED))
        m_navigationLocked = settings[PAGE_SETTINGS_NAVIGATION_LOCKED].toBool();

    // These settings are typically handled at browser/context creation in Playwright,
    // and dynamic changes might not be fully supported without restarting the backend.
    // They are passed to the backend, but Playwright backend might log warnings if it can't apply them dynamically.
    if (settings.contains(PAGE_SETTINGS_DISK_CACHE_ENABLED))
        setDiskCacheEnabled(settings[PAGE_SETTINGS_DISK_CACHE_ENABLED].toBool());
    if (settings.contains(PAGE_SETTINGS_MAX_DISK_CACHE_SIZE))
        setMaxDiskCacheSize(settings[PAGE_SETTINGS_MAX_DISK_CACHE_SIZE].toInt());
    if (settings.contains(PAGE_SETTINGS_DISK_CACHE_PATH))
        setDiskCachePath(settings[PAGE_SETTINGS_DISK_CACHE_PATH].toString());

    if (settings.contains(PAGE_SETTINGS_IGNORE_SSL_ERRORS))
        setIgnoreSslErrors(settings[PAGE_SETTINGS_IGNORE_SSL_ERRORS].toBool());
    if (settings.contains(PAGE_SETTINGS_SSL_PROTOCOL))
        setSslProtocol(settings[PAGE_SETTINGS_SSL_PROTOCOL].toString());
    if (settings.contains(PAGE_SETTINGS_SSL_CIPHERS))
        setSslCiphers(settings[PAGE_SETTINGS_SSL_CIPHERS].toString());
    if (settings.contains(PAGE_SETTINGS_SSL_CERTIFICATES_PATH))
        setSslCertificatesPath(settings[PAGE_SETTINGS_SSL_CERTIFICATES_PATH].toString());
    if (settings.contains(PAGE_SETTINGS_SSL_CLIENT_CERTIFICATE_FILE))
        setSslClientCertificateFile(settings[PAGE_SETTINGS_SSL_CLIENT_CERTIFICATE_FILE].toString());
    if (settings.contains(PAGE_SETTINGS_SSL_CLIENT_KEY_FILE))
        setSslClientKeyFile(settings[PAGE_SETTINGS_SSL_CLIENT_KEY_FILE].toString());
    if (settings.contains(PAGE_SETTINGS_SSL_CLIENT_KEY_PASSPHRASE))
        setSslClientKeyPassphrase(settings[PAGE_SETTINGS_SSL_CLIENT_KEY_PASSPHRASE].toByteArray());

    if (settings.contains(PAGE_SETTINGS_RESOURCE_TIMEOUT))
        setResourceTimeout(settings[PAGE_SETTINGS_RESOURCE_TIMEOUT].toInt());
    // Max auth attempts not directly handled in Playwright's Page settings, would be for Network context

    if (settings.contains(PAGE_SETTINGS_OFFLINE_STORAGE_PATH))
        m_cachedOfflineStoragePath = settings[PAGE_SETTINGS_OFFLINE_STORAGE_PATH].toString();
    if (settings.contains(PAGE_SETTINGS_OFFLINE_STORAGE_QUOTA))
        m_cachedOfflineStorageQuota = settings[PAGE_SETTINGS_OFFLINE_STORAGE_QUOTA].toInt();
    if (settings.contains(PAGE_SETTINGS_LOCAL_STORAGE_PATH))
        m_cachedLocalStoragePath = settings[PAGE_SETTINGS_LOCAL_STORAGE_PATH].toString();
    if (settings.contains(PAGE_SETTINGS_LOCAL_STORAGE_QUOTA))
        m_cachedLocalStorageQuota = settings[PAGE_SETTINGS_LOCAL_STORAGE_QUOTA].toInt();
}

QString PlaywrightEngineBackend::userAgent() const
{
    qDebug() << "PlaywrightEngineBackend: userAgent called.";
    // The cached value should be accurate after applySettings, but if not, fetch it.
    if (m_userAgent.isEmpty() || m_userAgent == "PhantomJS/3.0.0 (Custom Playwright Backend)") { // Check if default, then query
        return sendSyncCommand("getUserAgent").toString();
    }
    return m_userAgent;
}

void PlaywrightEngineBackend::setUserAgent(const QString& ua)
{
    qDebug() << "PlaywrightEngineBackend: setUserAgent called. UA:" << ua;
    m_userAgent = ua;
    QVariantMap params;
    params["userAgent"] = ua;
    sendAsyncCommand("setUserAgent", params);
}

QSize PlaywrightEngineBackend::viewportSize() const
{
    qDebug() << "PlaywrightEngineBackend: viewportSize called.";
    QVariantMap sizeMap = sendSyncCommand("getViewportSize").toMap();
    if (sizeMap.contains("width") && sizeMap.contains("height")) {
        return QSize(sizeMap["width"].toInt(), sizeMap["height"].toInt());
    }
    return m_viewportSize; // Fallback to cached if backend doesn't respond
}

void PlaywrightEngineBackend::setViewportSize(const QSize& size)
{
    qDebug() << "PlaywrightEngineBackend: setViewportSize called. Size:" << size;
    m_viewportSize = size;
    QVariantMap params;
    params["width"] = size.width();
    params["height"] = size.height();
    sendAsyncCommand("setViewportSize", params);
}

QRect PlaywrightEngineBackend::clipRect() const
{
    qDebug() << "PlaywrightEngineBackend: clipRect called.";
    // Playwright doesn't store a persistent clipRect on the page. It's a rendering option.
    // So, this will always return the last set value or a default.
    // The `getClipRect` command in JS might be a no-op or return 0,0,0,0 if not maintained.
    QVariantMap rectMap = sendSyncCommand("getClipRect").toMap(); // This is a stub command in Playwright backend
    if (rectMap.contains("x") && rectMap.contains("y") && rectMap.contains("width") && rectMap.contains("height")) {
        return QRect(rectMap["x"].toInt(), rectMap["y"].toInt(), rectMap["width"].toInt(), rectMap["height"].toInt());
    }
    return m_clipRect; // Return cached value
}

void PlaywrightEngineBackend::setClipRect(const QRect& rect)
{
    qDebug() << "PlaywrightEngineBackend: setClipRect called. Rect:" << rect;
    m_clipRect = rect;
    QVariantMap params;
    params["x"] = rect.x();
    params["y"] = rect.y();
    params["width"] = rect.width();
    params["height"] = rect.height();
    sendAsyncCommand("setClipRect", params); // This is a stub command in Playwright backend
}

QPoint PlaywrightEngineBackend::scrollPosition() const
{
    qDebug() << "PlaywrightEngineBackend: scrollPosition called.";
    QVariantMap posMap = sendSyncCommand("getScrollPosition").toMap();
    if (posMap.contains("x") && posMap.contains("y")) {
        return QPoint(posMap["x"].toInt(), posMap["y"].toInt());
    }
    return m_scrollPosition;
}

void PlaywrightEngineBackend::setScrollPosition(const QPoint& pos)
{
    qDebug() << "PlaywrightEngineBackend: setScrollPosition called. Pos:" << pos;
    m_scrollPosition = pos;
    QVariantMap params;
    params["x"] = pos.x();
    params["y"] = pos.y();
    sendAsyncCommand("setScrollPosition", params);
}

void PlaywrightEngineBackend::setNavigationLocked(bool lock)
{
    qDebug() << "PlaywrightEngineBackend: setNavigationLocked called. Lock:" << lock;
    m_navigationLocked = lock;
    QVariantMap params;
    params["locked"] = lock;
    sendAsyncCommand("setNavigationLocked", params);
}

bool PlaywrightEngineBackend::navigationLocked() const
{
    qDebug() << "PlaywrightEngineBackend: navigationLocked called.";
    // This value is maintained locally based on the last setNavigationLocked call
    return m_navigationLocked;
}

QVariantMap PlaywrightEngineBackend::customHeaders() const
{
    qDebug() << "PlaywrightEngineBackend: customHeaders called.";
    // This value is maintained locally based on the last setCustomHeaders call
    return m_customHeaders;
}

void PlaywrightEngineBackend::setCustomHeaders(const QVariantMap& headers)
{
    qDebug() << "PlaywrightEngineBackend: setCustomHeaders called. Headers count:" << headers.count();
    m_customHeaders = headers;
    QVariantMap params;
    params["headers"] = headers;
    sendAsyncCommand("setCustomHeaders", params);
}

qreal PlaywrightEngineBackend::zoomFactor() const
{
    qDebug() << "PlaywrightEngineBackend: zoomFactor called.";
    // This value is maintained locally based on the last setZoomFactor call
    return m_zoomFactor;
}

void PlaywrightEngineBackend::setZoomFactor(qreal zoom)
{
    qDebug() << "PlaywrightEngineBackend: setZoomFactor called. Zoom:" << zoom;
    m_zoomFactor = zoom;
    QVariantMap params;
    params["zoom"] = zoom;
    sendAsyncCommand("setZoomFactor", params);
}

QString PlaywrightEngineBackend::windowName() const
{
    qDebug() << "PlaywrightEngineBackend: windowName called.";
    // This might require a synchronous call to Playwright to get the window.name property
    // if not updated via events. For now, we fetch it live.
    return sendSyncCommand("getWindowName").toString();
}

QString PlaywrightEngineBackend::offlineStoragePath() const
{
    qDebug() << "PlaywrightEngineBackend: offlineStoragePath called.";
    return sendSyncCommand("getOfflineStoragePath").toString();
}

int PlaywrightEngineBackend::offlineStorageQuota() const
{
    qDebug() << "PlaywrightEngineBackend: offlineStorageQuota called.";
    return sendSyncCommand("getOfflineStorageQuota").toInt();
}

QString PlaywrightEngineBackend::localStoragePath() const
{
    qDebug() << "PlaywrightEngineBackend: localStoragePath called.";
    return sendSyncCommand("getLocalStoragePath").toString();
}

int PlaywrightEngineBackend::localStorageQuota() const
{
    qDebug() << "PlaywrightEngineBackend: localStorageQuota called.";
    return sendSyncCommand("getLocalStorageQuota").toInt();
}

QByteArray PlaywrightEngineBackend::renderImage(const QRect& clipRect, bool onlyViewport, const QPoint& scrollPosition)
{
    qDebug() << "PlaywrightEngineBackend: renderImage called. ClipRect:" << clipRect << "OnlyViewport:" << onlyViewport << "ScrollPos:" << scrollPosition;
    QVariantMap params;
    if (!clipRect.isNull() && (clipRect.width() > 0 || clipRect.height() > 0)) {
        params["clipRect"] = QVariantMap{{"x", clipRect.x()}, {"y", clipRect.y()}, {"width", clipRect.width()},
                                        {"height", clipRect.height()}};
    } else {
        params["clipRect"] = QVariantMap(); // Empty map if no valid clipRect
    }

    params["onlyViewport"] = onlyViewport;
    params["scrollPosition"] = QVariantMap{{"x", scrollPosition.x()}, {"y", scrollPosition.y()}};

    // The Node.js backend should return base64 encoded image data
    QVariant result = sendSyncCommand("renderImage", params);
    if (result.isValid() && result.canConvert<QString>()) {
        return QByteArray::fromBase64(result.toString().toUtf8());
    }
    qWarning() << "PlaywrightEngineBackend: renderImage received invalid or empty response.";
    return QByteArray(); // Return empty if failed
}

QByteArray PlaywrightEngineBackend::renderPdf(const QVariantMap& paperSize, const QRect& clipRect)
{
    qDebug() << "PlaywrightEngineBackend: renderPdf called. PaperSize:" << paperSize << "ClipRect:" << clipRect;
    QVariantMap params;
    params["paperSize"] = paperSize;
    if (!clipRect.isNull() && (clipRect.width() > 0 || clipRect.height() > 0)) {
        params["clipRect"] = QVariantMap{{"x", clipRect.x()}, {"y", clipRect.y()}, {"width", clipRect.width()},
                                        {"height", clipRect.height()}};
    } else {
        params["clipRect"] = QVariantMap(); // Empty map if no valid clipRect
    }

    // The Node.js backend should return base64 encoded PDF data
    QVariant result = sendSyncCommand("renderPdf", params);
    if (result.isValid() && result.canConvert<QString>()) {
        return QByteArray::fromBase64(result.toString().toUtf8());
    }
    qWarning() << "PlaywrightEngineBackend: renderPdf received invalid or empty response.";
    return QByteArray(); // Return empty if failed
}


void PlaywrightEngineBackend::setCookieJar(CookieJar* cookieJar)
{
    qDebug() << "PlaywrightEngineBackend: setCookieJar called.";
    // This method is primarily to inform the backend about the cookie jar
    // The Playwright backend will need to read cookies from this jar and synchronize.
    // For now, just send the current cookies from the jar.
    if (cookieJar) {
        setCookies(cookieJar->allCookiesToMap());
    }
}

QVariantList PlaywrightEngineBackend::cookies() const
{
    qDebug() << "PlaywrightEngineBackend: cookies called.";
    return sendSyncCommand("getCookies").toList();
}

bool PlaywrightEngineBackend::setCookies(const QVariantList& cookies)
{
    qDebug() << "PlaywrightEngineBackend: setCookies called.";
    QVariantMap params;
    params["cookies"] = cookies;
    return sendSyncCommand("setCookies", params).toBool();
}

bool PlaywrightEngineBackend::addCookie(const QVariantMap& cookie)
{
    qDebug() << "PlaywrightEngineBackend: addCookie called.";
    QVariantMap params;
    params["cookie"] = cookie;
    return sendSyncCommand("addCookie", params).toBool();
}

bool PlaywrightEngineBackend::deleteCookie(const QString& cookieName)
{
    qDebug() << "PlaywrightEngineBackend: deleteCookie called.";
    QVariantMap params;
    params["cookieName"] = cookieName;
    return sendSyncCommand("deleteCookie", params).toBool();
}

bool PlaywrightEngineBackend::clearCookies()
{
    qDebug() << "PlaywrightEngineBackend: clearCookies called.";
    return sendSyncCommand("clearCookies").toBool();
}

void PlaywrightEngineBackend::setNetworkProxy(const QNetworkProxy& proxy)
{
    qDebug() << "PlaywrightEngineBackend: setNetworkProxy called. Host:" << proxy.hostName();
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

void PlaywrightEngineBackend::clearMemoryCache()
{
    qDebug() << "PlaywrightEngineBackend: clearMemoryCache called.";
    sendAsyncCommand("clearMemoryCache");
}

void PlaywrightEngineBackend::setIgnoreSslErrors(bool ignore)
{
    qDebug() << "PlaywrightEngineBackend: setIgnoreSslErrors called. Ignore:" << ignore;
    QVariantMap params;
    params["ignore"] = ignore;
    sendAsyncCommand("setIgnoreSslErrors", params);
}

void PlaywrightEngineBackend::setSslProtocol(const QString& protocolName)
{
    qDebug() << "PlaywrightEngineBackend: setSslProtocol called. Protocol:" << protocolName;
    QVariantMap params;
    params["protocolName"] = protocolName;
    sendAsyncCommand("setSslProtocol", params);
}

void PlaywrightEngineBackend::setSslCiphers(const QString& ciphers)
{
    qDebug() << "PlaywrightEngineBackend: setSslCiphers called. Ciphers:" << ciphers;
    QVariantMap params;
    params["ciphers"] = ciphers;
    sendAsyncCommand("setSslCiphers", params);
}

void PlaywrightEngineBackend::setSslCertificatesPath(const QString& path)
{
    qDebug() << "PlaywrightEngineBackend: setSslCertificatesPath called. Path:" << path;
    QVariantMap params;
    params["path"] = path;
    sendAsyncCommand("setSslCertificatesPath", params);
}

void PlaywrightEngineBackend::setSslClientCertificateFile(const QString& path)
{
    qDebug() << "PlaywrightEngineBackend: setSslClientCertificateFile called. Path:" << path;
    QVariantMap params;
    params["path"] = path;
    sendAsyncCommand("setSslClientCertificateFile", params);
}

void PlaywrightEngineBackend::setSslClientKeyFile(const QString& path)
{
    qDebug() << "PlaywrightEngineBackend: setSslClientKeyFile called. Path:" << path;
    QVariantMap params;
    params["path"] = path;
    sendAsyncCommand("setSslClientKeyFile", params);
}

void PlaywrightEngineBackend::setSslClientKeyPassphrase(const QByteArray& passphrase)
{
    qDebug() << "PlaywrightEngineBackend: setSslClientKeyPassphrase called. Passphrase length:"
             << passphrase.length();
    QVariantMap params;
    params["passphrase"] = QString::fromUtf8(passphrase.toBase64());
    sendAsyncCommand("setSslClientKeyPassphrase", params);
}

void PlaywrightEngineBackend::setResourceTimeout(int timeoutMs)
{
    qDebug() << "PlaywrightEngineBackend: setResourceTimeout called. Timeout:" << timeoutMs;
    QVariantMap params;
    params["timeoutMs"] = timeoutMs;
    sendAsyncCommand("setResourceTimeout", params);
}

void PlaywrightEngineBackend::setMaxAuthAttempts(int attempts)
{
    qDebug() << "PlaywrightEngineBackend: setMaxAuthAttempts called. Attempts:" << attempts;
    QVariantMap params;
    params["attempts"] = attempts;
    sendAsyncCommand("setMaxAuthAttempts", params);
}

int PlaywrightEngineBackend::framesCount() const
{
    qDebug() << "PlaywrightEngineBackend: framesCount called.";
    return sendSyncCommand("getFramesCount").toInt();
}

QStringList PlaywrightEngineBackend::framesName() const
{
    qDebug() << "PlaywrightEngineBackend: framesName called.";
    return sendSyncCommand("getFramesName").toStringList();
}

QString PlaywrightEngineBackend::frameName() const
{
    qDebug() << "PlaywrightEngineBackend: frameName called.";
    return sendSyncCommand("getFrameName").toString();
}

QString PlaywrightEngineBackend::focusedFrameName() const
{
    qDebug() << "PlaywrightEngineBackend: focusedFrameName called.";
    return sendSyncCommand("getFocusedFrameName").toString();
}

bool PlaywrightEngineBackend::switchToFrame(const QString& frameName)
{
    qDebug() << "PlaywrightEngineBackend: switchToFrame by name called. Name:" << frameName;
    QVariantMap params;
    params["name"] = frameName;
    return sendSyncCommand("switchToFrameByName", params).toBool();
}

bool PlaywrightEngineBackend::switchToFrame(int framePosition)
{
    qDebug() << "PlaywrightEngineBackend: switchToFrame by position called. Position:" << framePosition;
    QVariantMap params;
    params["position"] = framePosition;
    return sendSyncCommand("switchToFrameByPosition", params).toBool();
}

void PlaywrightEngineBackend::switchToMainFrame()
{
    qDebug() << "PlaywrightEngineBackend: switchToMainFrame called.";
    sendAsyncCommand("switchToMainFrame");
}

bool PlaywrightEngineBackend::switchToParentFrame()
{
    qDebug() << "PlaywrightEngineBackend: switchToParentFrame called.";
    return sendSyncCommand("switchToParentFrame").toBool();
}

void PlaywrightEngineBackend::switchToFocusedFrame()
{
    qDebug() << "PlaywrightEngineBackend: switchToFocusedFrame called.";
    sendAsyncCommand("switchToFocusedFrame");
}

void PlaywrightEngineBackend::exposeQObject(const QString& name, QObject* object)
{
    qDebug() << "PlaywrightEngineBackend: exposeQObject called. Name:" << name << "Object:" << object->objectName();

    QVariantList invokableMethodNames;
    const QMetaObject* meta = object->metaObject();
    for (int i = meta->methodOffset(); i < meta->methodCount(); ++i) {
        QMetaMethod method = meta->method(i);
        // Only expose invokable methods that are not signals or slots (handled separately)
        // PhantomJS exposes Q_INVOKABLE methods and some slots (e.g. debugExit)
        if ((method.methodType() == QMetaMethod::Method || method.methodType() == QMetaMethod::Slot) && method.accessSpecifier() == QMetaMethod::Public) {
            QString methodName = QString::fromLatin1(method.name()); // Get method name without arguments
            // Exclude signals, properties' read/write methods if you don't want them as direct methods
            // Also exclude internal methods starting with '_'
            if (method.methodType() != QMetaMethod::Signal && !methodName.startsWith('_')) {
                 invokableMethodNames.append(methodName);
            }
        }
    }

    QVariantMap exposedProperties;
    for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i) {
        QMetaProperty prop = meta->property(i);
        // Only expose scriptable properties that have read/write access
        if (prop.isScriptable() && prop.isValid()) {
            QString propertyName = QString::fromLatin1(prop.name());
            if (!propertyName.startsWith('_')) { // Exclude internal properties
                exposedProperties[propertyName] = prop.typeName(); // Send type name for introspection (optional)
            }
        }
    }

    QVariantMap params;
    params["objectName"] = name;
    params["methods"] = invokableMethodNames;
    params["properties"] = exposedProperties; // List properties that can be gotten/set

    m_exposedObjects[name] = object; // Store the QObject pointer for later invocation

    sendAsyncCommand("exposeObject", params);
}

void PlaywrightEngineBackend::clickElement(const QString& selector)
{
    qDebug() << "PlaywrightEngineBackend: clickElement called. Selector:" << selector;
    QVariantMap params;
    params["selector"] = selector;
    sendAsyncCommand("clickElement", params);
}

int PlaywrightEngineBackend::showInspector(int remotePort)
{
    qDebug() << "PlaywrightEngineBackend: showInspector called. Port:" << remotePort;
    QVariantMap params;
    params["remotePort"] = remotePort;
    // Playwright backend will respond with 0 as it doesn't offer a direct embedded inspector port
    return sendSyncCommand("showInspector", params).toInt();
}
