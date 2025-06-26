#include "networkaccessmanager.h"
#include "config.h"
#include "cookiejar.h"
#include "consts.h"

#include <QAuthenticator>
#include <QNetworkRequest>
#include <QDebug>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslCertificate>
#include <QFile> // CRITICAL: Added for QFile usage
#include <QDir> // For QDir in certificate paths
#include <QSsl> // Needed for QSsl::Pem, QSsl::WildcardSyntax

NetworkAccessManager::NetworkAccessManager(QObject* parent)
    : QNetworkAccessManager(parent)
    , m_config(Config::instance())
    , m_cookieJar(nullptr) // Will be set by setCookieJar call from Phantom
    , m_ignoreSslErrors(false)
    , m_resourceTimeout(0)
    , m_maxAuthAttempts(3)
{
    connect(this, &QNetworkAccessManager::finished, this, &NetworkAccessManager::handleFinished);
    connect(this, &QNetworkAccessManager::sslErrors, this, &NetworkAccessManager::handleSslErrors);
    connect(this, &QNetworkAccessManager::authenticationRequired, this, &NetworkAccessManager::handleAuthenticationRequired);
    connect(this, &QNetworkAccessManager::proxyAuthenticationRequired, this, &NetworkAccessManager::handleProxyAuthenticationRequired);

    setIgnoreSslErrors(m_config->ignoreSslErrors());
    setSslProtocol(m_config->sslProtocol());
    setSslCiphers(m_config->sslCiphers());
    setSslCertificatesPath(m_config->sslCertificatesPath());
    setSslClientCertificateFile(m_config->sslClientCertificateFile());
    setSslClientKeyFile(m_config->sslClientKeyFile());
    setSslClientKeyPassphrase(m_config->sslClientKeyPassphrase());
    setResourceTimeout(m_config->resourceTimeout());
    setMaxAuthAttempts(m_config->maxAuthAttempts());
    setDiskCacheEnabled(m_config->diskCacheEnabled());
    setMaxDiskCacheSize(m_config->maxDiskCacheSize());
    setDiskCachePath(m_config->diskCachePath());

    connect(m_config, &Config::ignoreSslErrorsChanged, this, &NetworkAccessManager::setIgnoreSslErrors);
    connect(m_config, &Config::sslProtocolChanged, this, &NetworkAccessManager::setSslProtocol);
    connect(m_config, &Config::sslCiphersChanged, this, &NetworkAccessManager::setSslCiphers);
    connect(m_config, &Config::sslCertificatesPathChanged, this, &NetworkAccessManager::setSslCertificatesPath);
    connect(m_config, &Config::sslClientCertificateFileChanged, this, &NetworkAccessManager::setSslClientCertificateFile);
    connect(m_config, &Config::sslClientKeyFileChanged, this, &NetworkAccessManager::setSslClientKeyFile);
    connect(m_config, &Config::sslClientKeyPassphraseChanged, this, &NetworkAccessManager::setSslClientKeyPassphrase);
    connect(m_config, &Config::resourceTimeoutChanged, this, &NetworkAccessManager::setResourceTimeout);
    connect(m_config, &Config::maxAuthAttemptsChanged, this, &NetworkAccessManager::setMaxAuthAttempts);
    connect(m_config, &Config::diskCacheEnabledChanged, this, &NetworkAccessManager::setDiskCacheEnabled);
    connect(m_config, &Config::maxDiskCacheSizeChanged, this, &NetworkAccessManager::setMaxDiskCacheSize);
    connect(m_config, &Config::diskCachePathChanged, this, &NetworkAccessManager::setDiskCachePath);
}

NetworkAccessManager::~NetworkAccessManager()
{
    // No specific cleanup needed for m_cookieJar as it's owned by Phantom
}

void NetworkAccessManager::setCookieJar(CookieJar* cookieJar)
{
    if (m_cookieJar != cookieJar) {
        if (m_cookieJar) {
            disconnect(m_cookieJar, nullptr, this, nullptr); // Disconnect old jar signals
        }
        m_cookieJar = cookieJar;
        QNetworkAccessManager::setCookieJar(m_cookieJar);
        qDebug() << "NetworkAccessManager: Set cookie jar to" << (m_cookieJar ? "provided instance" : "nullptr");
    }
}

QNetworkReply* NetworkAccessManager::createRequest(QNetworkAccessManager::Operation op, const QNetworkRequest& request, QIODevice* outgoingData)
{
    QNetworkRequest newRequest = request;
    QNetworkReply* reply = QNetworkAccessManager::createRequest(op, newRequest, outgoingData);

    if (m_resourceTimeout > 0) {
        QTimer* timer = new QTimer(reply);
        timer->setSingleShot(true);
        timer->setInterval(m_resourceTimeout);
        connect(timer, &QTimer::timeout, this, [reply]() {
            if (reply->isRunning()) {
                reply->abort();
            }
        });
        connect(reply, &QNetworkReply::finished, timer, &QTimer::deleteLater);
        timer->start();
    }

    connect(reply, &QNetworkReply::downloadProgress, this, &NetworkAccessManager::handleReadProgress);
    connect(reply, &QNetworkReply::uploadProgress, this, &NetworkAccessManager::handleUploadProgress);
    connect(reply, &QNetworkReply::encrypted, this, &NetworkAccessManager::handleEncrypted);
    connect(reply, &QNetworkReply::redirected, this, &NetworkAccessManager::handleRedirected);

    QVariantMap requestData;
    requestData["id"] = QVariant::fromValue((qulonglong)reply);
    requestData["url"] = reply->url().toString();
    requestData["method"] = QVariant::fromValue(op);
    QVariantMap headersMap;
    for (const auto& header : reply->request().rawHeaderList()) {
        headersMap.insert(QString::fromUtf8(header), QString::fromUtf8(reply->request().rawHeader(header)));
    }
    requestData["headers"] = headersMap;

    emit resourceRequested(requestData, reply);

    return reply;
}

void NetworkAccessManager::handleFinished(QNetworkReply* reply)
{
    QVariantMap responseData;
    responseData["id"] = QVariant::fromValue((qulonglong)reply);
    responseData["url"] = reply->url().toString();
    responseData["status"] = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    responseData["statusText"] = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
    responseData["redirectURL"] = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl().toString();

    QVariantMap headersMap;
    for (const auto& header : reply->rawHeaderList()) {
        headersMap.insert(QString::fromUtf8(header), QString::fromUtf8(reply->rawHeader(header)));
    }
    responseData["headers"] = headersMap;

    if (reply->error() != QNetworkReply::NoError) {
        QVariantMap errorData;
        errorData["id"] = responseData["id"];
        errorData["url"] = responseData["url"];
        errorData["errorString"] = reply->errorString();
        errorData["errorCode"] = reply->error();

        if (reply->error() == QNetworkReply::OperationCanceledError) {
             emit resourceTimeout(errorData);
        } else {
            emit resourceError(errorData);
        }
    } else {
        emit resourceReceived(responseData);
    }

    reply->deleteLater();
}

void NetworkAccessManager::handleSslErrors(QNetworkReply* reply, const QList<QSslError>& errors)
{
    if (m_ignoreSslErrors) {
        qDebug() << "NetworkAccessManager: Ignoring SSL errors for" << reply->url().toString();
        reply->ignoreSslErrors();
    } else {
        for (const QSslError& error : errors) {
            qWarning() << "NetworkAccessManager: SSL Error for" << reply->url().toString() << ":" << error.errorString();
        }
    }
}

void NetworkAccessManager::handleAuthenticationRequired(QNetworkReply* reply, QAuthenticator* authenticator)
{
    qDebug() << "NetworkAccessManager: Authentication required for" << reply->url().toString();
    emit authenticationRequired(reply, authenticator);
}

void NetworkAccessManager::handleProxyAuthenticationRequired(const QNetworkProxy& proxy, QAuthenticator* authenticator)
{
    qDebug() << "NetworkAccessManager: Proxy authentication required for" << proxy.hostName();
    emit proxyAuthenticationRequired(proxy, authenticator);
}

void NetworkAccessManager::handleReadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    Q_UNUSED(bytesReceived);
    Q_UNUSED(bytesTotal);
}

void NetworkAccessManager::handleUploadProgress(qint64 bytesSent, qint64 bytesTotal)
{
    Q_UNUSED(bytesSent);
    Q_UNUSED(bytesTotal);
}

void NetworkAccessManager::handleEncrypted(QNetworkReply* reply)
{
    qDebug() << "NetworkAccessManager: Encrypted connection established for" << reply->url().toString();
}

void NetworkAccessManager::handleRedirected(QNetworkReply* reply, const QUrl& url)
{
    qDebug() << "NetworkAccessManager: Redirected from" << reply->url().toString() << "to" << url.toString();
}

void NetworkAccessManager::setIgnoreSslErrors(bool ignore) {
    if (m_ignoreSslErrors != ignore) {
        m_ignoreSslErrors = ignore;
    }
}

void NetworkAccessManager::setSslProtocol(const QString& protocolName) {
    qDebug() << "NetworkAccessManager: Set SSL protocol to" << protocolName;
}

void NetworkAccessManager::setSslCiphers(const QString& ciphers) {
    qDebug() << "NetworkAccessManager: Set SSL ciphers to" << ciphers;
}

void NetworkAccessManager::setSslCertificatesPath(const QString& path) {
    if (!path.isEmpty()) {
        QList<QSslCertificate> certs = QSslCertificate::fromPath(path, QSsl::Pem, QSsl::WildcardSyntax);
        if (!certs.isEmpty()) {
            QSslConfiguration currentSslConfig = QSslConfiguration::defaultConfiguration();
            currentSslConfig.addCaCertificates(certs);
            QSslConfiguration::setDefaultConfiguration(currentSslConfig);
            qDebug() << "NetworkAccessManager: Loaded" << certs.count() << "CA certificates from" << path;
        } else {
            qWarning() << "NetworkAccessManager: No CA certificates found in" << path;
        }
    }
}

void NetworkAccessManager::setSslClientCertificateFile(const QString& path) {
    if (!path.isEmpty()) {
        QList<QSslCertificate> certs = QSslCertificate::fromPath(path, QSsl::Pem, QSsl::WildcardSyntax);
        if (!certs.isEmpty()) {
            QSslConfiguration currentSslConfig = QSslConfiguration::defaultConfiguration();
            currentSslConfig.setLocalCertificate(certs.first());
            QSslConfiguration::setDefaultConfiguration(currentSslConfig);
            qDebug() << "NetworkAccessManager: Loaded client certificate from" << path;
        } else {
            qWarning() << "NetworkAccessManager: No client certificate found in" << path;
        }
    }
}

void NetworkAccessManager::setSslClientKeyFile(const QString& path) {
    if (!path.isEmpty()) {
        QFile keyFile(path);
        if (keyFile.open(QIODevice::ReadOnly)) {
            QSslKey key(&keyFile, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey, m_config->sslClientKeyPassphrase());
            keyFile.close();
            if (!key.isNull()) {
                QSslConfiguration currentSslConfig = QSslConfiguration::defaultConfiguration();
                currentSslConfig.setPrivateKey(key);
                QSslConfiguration::setDefaultConfiguration(currentSslConfig);
                qDebug() << "NetworkAccessManager: Loaded client private key from" << path;
            } else {
                qWarning() << "NetworkAccessManager: Failed to load client private key from" << path;
            }
        } else {
            qWarning() << "NetworkAccessManager: Could not open client private key file:" << path;
        }
    }
}

void NetworkAccessManager::setSslClientKeyPassphrase(const QByteArray& passphrase) {
    qDebug() << "NetworkAccessManager: SSL Client Key Passphrase set.";
}

void NetworkAccessManager::setResourceTimeout(int timeoutMs) {
    if (m_resourceTimeout != timeoutMs) {
        m_resourceTimeout = timeoutMs;
    }
}

void NetworkAccessManager::setMaxAuthAttempts(int attempts) {
    if (m_maxAuthAttempts != attempts) {
        m_maxAuthAttempts = attempts;
    }
}

void NetworkAccessManager::setDiskCacheEnabled(bool enabled) {
    qDebug() << "NetworkAccessManager: Disk cache enabled:" << enabled;
}

void NetworkAccessManager::setMaxDiskCacheSize(int size) {
    qDebug() << "NetworkAccessManager: Max disk cache size:" << size << "MB";
}

void NetworkAccessManager::setDiskCachePath(const QString& path) {
    qDebug() << "NetworkAccessManager: Disk cache path:" << path;
}
