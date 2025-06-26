/*
  This file is part of the PhantomJS project from Ofi Labs.

  Copyright (C) 2011 Ariya Hidayat <ariya.hidayat@gmail.com>
  Copyright (C) 2011 Ivan De Marino <ivan.de.marino@gmail.com>

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

class NetworkAccessManager : public QNetworkAccessManager
{
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
    QNetworkReply* createRequest(QNetworkAccessManager::Operation op, const QNetworkRequest& request, QIODevice* outgoingData = nullptr) override;

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

