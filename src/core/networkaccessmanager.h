#ifndef NETWORKACCESSMANAGER_H
#define NETWORKACCESSMANAGER_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QList>
#include <QSslError>
#include <QNetworkProxy>
#include <QSslConfiguration>
#include <QVariantMap>

// Forward declarations
class CookieJar;
class Config;

class NetworkAccessManager : public QNetworkAccessManager {
    Q_OBJECT

public:
    explicit NetworkAccessManager(QObject* parent = nullptr);
    ~NetworkAccessManager();

    void setCookieJar(CookieJar* cookieJar);

    void setIgnoreSslErrors(bool ignore);
    void setSslProtocol(const QString& protocolName);
    void setSslCiphers(const QString& ciphers);
    void setSslCertificatesPath(const QString& path);
    void setSslClientCertificateFile(const QString& path);
    void setSslClientKeyFile(const QString& path);
    void setSslClientKeyPassphrase(const QByteArray& passphrase);
    void setResourceTimeout(int timeoutMs);
    void setMaxAuthAttempts(int attempts);
    void setDiskCacheEnabled(bool enabled);
    void setMaxDiskCacheSize(int size);
    void setDiskCachePath(const QString& path);

signals:
    void resourceRequested(QVariant requestData, QObject* reply);
    void resourceReceived(QVariant responseData);
    void resourceError(QVariant errorData);
    void resourceTimeout(QVariant timeoutData);
    void authenticationRequired(QNetworkReply* reply, QAuthenticator* authenticator);
    void proxyAuthenticationRequired(const QNetworkProxy& proxy, QAuthenticator* authenticator);

protected:
    QNetworkReply* createRequest(QNetworkAccessManager::Operation op, const QNetworkRequest& request,
        QIODevice* outgoingData = nullptr) override;

private slots:
    void handleFinished(QNetworkReply* reply);
    void handleSslErrors(QNetworkReply* reply, const QList<QSslError>& errors);
    void handleAuthenticationRequired(QNetworkReply* reply, QAuthenticator* authenticator);
    void handleProxyAuthenticationRequired(const QNetworkProxy& proxy, QAuthenticator* authenticator);
    void handleReadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void handleUploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void handleEncrypted(QNetworkReply* reply);
    void handleRedirected(QNetworkReply* reply, const QUrl& url);

private:
    Config* m_config;
    CookieJar* m_cookieJar;

    bool m_ignoreSslErrors;
    int m_resourceTimeout;
    int m_maxAuthAttempts;
};

#endif // NETWORKACCESSMANAGER_H
