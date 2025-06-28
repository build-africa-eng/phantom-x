#include "networkaccessmanager.h"
#include <QDebug>
#include <QAuthenticator>
#include <QSslError>
#include <QNetworkRequest>
#include <QNetworkProxy>

// Constructor
NetworkAccessManager::NetworkAccessManager(QObject* parent)
    : QNetworkAccessManager(parent) {
    // Connect the QNetworkAccessManager's own finished signal to our handler
    connect(this, &NetworkAccessManager::finished, this, &NetworkAccessManager::handleFinished);
    // You might also connect other global signals of QNetworkAccessManager here
    connect(
        this, &NetworkAccessManager::authenticationRequired, this, &NetworkAccessManager::handleAuthenticationRequired);
    connect(this, &NetworkAccessManager::proxyAuthenticationRequired, this,
        &NetworkAccessManager::handleProxyAuthenticationRequired);
    connect(this, &NetworkAccessManager::sslErrors, this, &NetworkAccessManager::handleSslErrors);
    // Example of a custom network request configuration or logging
    qDebug() << "NetworkAccessManager initialized.";
}

// Override createRequest to potentially intercept or modify requests
QNetworkReply* NetworkAccessManager::createRequest(
    QNetworkAccessManager::Operation op, const QNetworkRequest& req, QIODevice* outgoingData) {
    // Log the request or apply custom headers/logic
    qDebug() << "NetworkAccessManager: Creating request for URL:" << req.url().toString() << "Operation:" << op;
    // Call the base class implementation
    QNetworkReply* reply = QNetworkAccessManager::createRequest(op, req, outgoingData);
    // Connect reply-specific signals
    // IMPORTANT: Ensure the slot signatures match the signal signatures.
    // QNetworkReply::encrypted() has NO arguments.
    connect(reply, &QNetworkReply::encrypted, this, &NetworkAccessManager::handleEncrypted);
    // QNetworkReply::redirected() has ONE QUrl argument.
    connect(reply, &QNetworkReply::redirected, this, &NetworkAccessManager::handleRedirected);
    // Download and upload progress signals take qint64 bytesReceived/bytesTotal
    connect(reply, &QNetworkReply::downloadProgress, this, &NetworkAccessManager::handleDownloadProgress);
    connect(reply, &QNetworkReply::uploadProgress, this, &NetworkAccessManager::handleUploadProgress);
    return reply;
}

// --- Slot Implementations ---
void NetworkAccessManager::handleEncrypted() {
    qDebug() << "NetworkAccessManager: QNetworkReply encrypted signal received.";
    // This slot is called when a TLS/SSL handshake has successfully completed.
    // No reply object is passed directly with this signal.
}

void NetworkAccessManager::handleRedirected(const QUrl& newUrl) {
    qDebug() << "NetworkAccessManager: QNetworkReply redirected to:" << newUrl.toString();
    // This slot is called when a redirect occurs. The newUrl is provided.
    // No reply object is passed directly with this signal.
}

void NetworkAccessManager::handleFinished(QNetworkReply* reply) {
    qDebug() << "NetworkAccessManager: Request finished for URL:" << reply->url().toString();
    // Check for errors
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "NetworkAccessManager Error:" << reply->errorString();
    }
    // Read all data from the reply
    QByteArray data = reply->readAll();
    qDebug() << "Received data size:" << data.size();
    // IMPORTANT: reply->deleteLater() is crucial to prevent memory leaks
    // QNetworkReply objects are created by QNetworkAccessManager::get(), post(), etc.
    // and must be deleted. deleteLater() is safe to call even if the reply is still
    // being used in other slots connected to it, as deletion happens after event processing.
    reply->deleteLater();
}

void NetworkAccessManager::handleDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    // qInfo() << "Download Progress:" << bytesReceived << "/" << bytesTotal;
}

void NetworkAccessManager::handleUploadProgress(qint64 bytesSent, qint64 bytesTotal) {
    // qInfo() << "Upload Progress:" << bytesSent << "/" << bytesTotal;
}

void NetworkAccessManager::handleSslErrors(QNetworkReply *reply, const QList<QSslError>& errors)
{
    qWarning() << "NetworkAccessManager: SSL Errors occurred for URL:" << reply->url().toString();
    for (const QSslError& error : errors) {
        qWarning() << " - " << error.errorString();
    }
}
    // You might want to automatically ignore some errors or prompt the user.
    // For now, we just log them.
    // reply->ignoreSslErrors(); // This would ignore the errors for the specific reply
}

void NetworkAccess
