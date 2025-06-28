#ifndef NETWORKACCESSMANAGER_H
#define NETWORKACCESSMANAGER_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QMap>
#include <QList>
#include <QPair>

// Declare QNetworkAccessManager::Operation as a metatype so it can be stored in QVariant
Q_DECLARE_METATYPE(QNetworkAccessManager::Operation);

class NetworkAccessManager : public QNetworkAccessManager {
    Q_OBJECT

public:
    explicit NetworkAccessManager(QObject* parent = nullptr);

    // Public API for handling custom requests or configurations
    // Not explicitly defined in the provided .cpp, but typically part of the public interface
    QNetworkReply* createRequest(
        QNetworkAccessManager::Operation op, const QNetworkRequest& req, QIODevice* outgoingData = nullptr) override;

signals:
    // Existing signals based on your previous code
    void finished(QNetworkReply* reply); // Already a QNetworkAccessManager signal

private slots:
    // Corrected slot signatures to match QNetworkReply signals
    void handleEncrypted(); // Matches void QNetworkReply::encrypted()
    void handleRedirected(const QUrl& newUrl); // Matches void QNetworkReply::redirected(const QUrl&)
    void handleFinished(QNetworkReply* reply); // Existing slot for finished signal
    void handleDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void handleUploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void handleSslErrors(QNetworkReply* reply, const QList<QSslError>& errors);
    void handleAuthenticationRequired(QNetworkReply* reply, QAuthenticator* authenticator);
    void handleProxyAuthenticationRequired(const QNetworkProxy& proxy, QAuthenticator* authenticator);

private:
    // Add any private members needed for the manager's logic
};

#endif // NETWORKACCESSMANAGER_H
