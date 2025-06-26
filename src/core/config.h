/*
  This file is part of the PhantomJS project from Ofi Labs.

  Copyright (C) 2011 Ariya Hidayat <ariya.hidayat@gmail.com>
  Copyright (C) 2011 execjosh, http://execjosh.blogspot.com

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

#ifndef CONFIG_H
#define CONFIG_H

#include <QObject>
#include <QVariantMap>
#include <QByteArray>
#include <QSettings>
#include "qcommandline/qcommandline.h" // Assuming this is your custom QCommandLine

struct QCommandLineConfigEntry; // Forward declare the struct

class Config : public QObject {
    Q_OBJECT

public:
    static Config* instance();

    bool loadJsonFile(const QString& filePath);
    QVariant get(const QString& key) const;

    // --- Core Settings ---
    Q_PROPERTY(bool debug READ debug WRITE setDebug NOTIFY debugChanged)
    Q_PROPERTY(QString logLevel READ logLevel WRITE setLogLevel NOTIFY logLevelChanged)
    Q_PROPERTY(QString outputEncoding READ outputEncoding WRITE setOutputEncoding NOTIFY outputEncodingChanged)
    Q_PROPERTY(QString scriptEncoding READ scriptEncoding WRITE setScriptEncoding NOTIFY scriptEncodingChanged)
    Q_PROPERTY(QString scriptLanguage READ scriptLanguage WRITE setScriptLanguage NOTIFY scriptLanguageChanged)

    // --- Network / Caching / SSL Settings ---
    Q_PROPERTY(bool cookiesEnabled READ cookiesEnabled WRITE setCookiesEnabled NOTIFY cookiesEnabledChanged)
    Q_PROPERTY(QString cookiesFile READ cookiesFile WRITE setCookiesFile NOTIFY cookiesFileChanged)
    Q_PROPERTY(bool diskCacheEnabled READ diskCacheEnabled WRITE setDiskCacheEnabled NOTIFY diskCacheEnabledChanged)
    Q_PROPERTY(int maxDiskCacheSize READ maxDiskCacheSize WRITE setMaxDiskCacheSize NOTIFY maxDiskCacheSizeChanged)
    Q_PROPERTY(QString diskCachePath READ diskCachePath WRITE setDiskCachePath NOTIFY diskCachePathChanged)
    Q_PROPERTY(bool ignoreSslErrors READ ignoreSslErrors WRITE setIgnoreSslErrors NOTIFY ignoreSslErrorsChanged)
    Q_PROPERTY(QString sslProtocol READ sslProtocol WRITE setSslProtocol NOTIFY sslProtocolChanged)
    Q_PROPERTY(QString sslCiphers READ sslCiphers WRITE setSslCiphers NOTIFY sslCiphersChanged)
    Q_PROPERTY(QString sslCertificatesPath READ sslCertificatesPath WRITE setSslCertificatesPath NOTIFY
            sslCertificatesPathChanged)
    Q_PROPERTY(QString sslClientCertificateFile READ sslClientCertificateFile WRITE setSslClientCertificateFile NOTIFY
            sslClientCertificateFileChanged)
    Q_PROPERTY(QString sslClientKeyFile READ sslClientKeyFile WRITE setSslClientKeyFile NOTIFY sslClientKeyFileChanged)
    Q_PROPERTY(QByteArray sslClientKeyPassphrase READ sslClientKeyPassphrase WRITE setSslClientKeyPassphrase NOTIFY
            sslClientKeyPassphraseChanged)
    Q_PROPERTY(int resourceTimeout READ resourceTimeout WRITE setResourceTimeout NOTIFY resourceTimeoutChanged)
    Q_PROPERTY(int maxAuthAttempts READ maxAuthAttempts WRITE setMaxAuthAttempts NOTIFY maxAuthAttemptsChanged)

    // --- JavaScript / Page Security Settings ---
    Q_PROPERTY(bool javascriptEnabled READ javascriptEnabled WRITE setJavascriptEnabled NOTIFY javascriptEnabledChanged)
    Q_PROPERTY(
        bool webSecurityEnabled READ webSecurityEnabled WRITE setWebSecurityEnabled NOTIFY webSecurityEnabledChanged)
    Q_PROPERTY(bool webGLEnabled READ webGLEnabled WRITE setWebGLEnabled NOTIFY webGLEnabledChanged)
    Q_PROPERTY(bool javascriptCanOpenWindows READ javascriptCanOpenWindows WRITE setJavascriptCanOpenWindows NOTIFY
            javascriptCanOpenWindowsChanged)
    Q_PROPERTY(bool javascriptCanCloseWindows READ javascriptCanCloseWindows WRITE setJavascriptCanCloseWindows NOTIFY
            javascriptCanCloseWindowsChanged)
    Q_PROPERTY(bool localToRemoteUrlAccessEnabled READ localToRemoteUrlAccessEnabled WRITE
            setLocalToRemoteUrlAccessEnabled NOTIFY localToRemoteUrlAccessEnabledChanged)
    Q_PROPERTY(bool autoLoadImages READ autoLoadImages WRITE setAutoLoadImages NOTIFY autoLoadImagesChanged)

    // --- Local/Offline Storage Settings ---
    Q_PROPERTY(QString localStoragePath READ localStoragePath WRITE setLocalStoragePath NOTIFY localStoragePathChanged)
    Q_PROPERTY(int localStorageQuota READ localStorageQuota WRITE setLocalStorageQuota NOTIFY localStorageQuotaChanged)
    Q_PROPERTY(
        QString offlineStoragePath READ offlineStoragePath WRITE setOfflineStoragePath NOTIFY offlineStoragePathChanged)
    Q_PROPERTY(
        int offlineStorageQuota READ offlineStorageQuota WRITE setOfflineStorageQuota NOTIFY offlineStorageQuotaChanged)

    // --- Printing Settings ---
    Q_PROPERTY(bool printHeader READ printHeader WRITE setPrintHeader NOTIFY printHeaderChanged)
    Q_PROPERTY(bool printFooter READ printFooter WRITE setPrintFooter NOTIFY printFooterChanged)

    // --- Page Settings (as a map, directly used by WebPage) ---
    Q_PROPERTY(QVariantMap defaultPageSettings READ defaultPageSettings WRITE setDefaultPageSettings NOTIFY
            defaultPageSettingsChanged)

    // Getters
    bool debug() const;
    QString logLevel() const;
    QString outputEncoding() const;
    QString scriptEncoding() const;
    QString scriptLanguage() const;
    bool cookiesEnabled() const;
    QString cookiesFile() const;
    bool diskCacheEnabled() const;
    int maxDiskCacheSize() const;
    QString diskCachePath() const;
    bool ignoreSslErrors() const;
    QString sslProtocol() const;
    QString sslCiphers() const;
    QString sslCertificatesPath() const;
    QString sslClientCertificateFile() const;
    QString sslClientKeyFile() const;
    QByteArray sslClientKeyPassphrase() const;
    int resourceTimeout() const;
    int maxAuthAttempts() const;
    bool javascriptEnabled() const;
    bool webSecurityEnabled() const;
    bool webGLEnabled() const;
    bool javascriptCanOpenWindows() const;
    bool javascriptCanCloseWindows() const;
    bool localToRemoteUrlAccessEnabled() const;
    bool autoLoadImages() const;
    QString localStoragePath() const;
    int localStorageQuota() const;
    QString offlineStoragePath() const;
    int offlineStorageQuota() const;
    bool printHeader() const;
    bool printFooter() const;
    QVariantMap defaultPageSettings() const;

    // Setters
    void setDebug(bool debug);
    void setLogLevel(const QString& level);
    void setOutputEncoding(const QString& encoding);
    void setScriptEncoding(const QString& encoding);
    void setScriptLanguage(const QString& language);
    void setCookiesEnabled(bool enabled);
    void setCookiesFile(const QString& path);
    void setDiskCacheEnabled(bool enabled);
    void setMaxDiskCacheSize(int size);
    void setDiskCachePath(const QString& path);
    void setIgnoreSslErrors(bool ignore);
    void setSslProtocol(const QString& protocol);
    void setSslCiphers(const QString& ciphers);
    void setSslCertificatesPath(const QString& path);
    void setSslClientCertificateFile(const QString& path);
    void setSslClientKeyFile(const QString& path);
    void setSslClientKeyPassphrase(const QByteArray& passphrase);
    void setResourceTimeout(int timeout);
    void setMaxAuthAttempts(int attempts);
    void setJavascriptEnabled(bool enabled);
    void setWebSecurityEnabled(bool enabled);
    void setWebGLEnabled(bool enabled);
    void setJavascriptCanOpenWindows(bool enabled);
    void setJavascriptCanCloseWindows(bool enabled);
    void setLocalToRemoteUrlAccessEnabled(bool enabled);
    void setAutoLoadImages(bool enabled);
    void setLocalStoragePath(const QString& path);
    void setLocalStorageQuota(int quota);
    void setOfflineStoragePath(const QString& path);
    void setOfflineStorageQuota(int quota);
    void setPrintHeader(bool enable);
    void setPrintFooter(bool enable);
    void setDefaultPageSettings(const QVariantMap& settings);

signals:
    void debugChanged(bool debug);
    void logLevelChanged(const QString& level);
    void outputEncodingChanged(const QString& encoding);
    void scriptEncodingChanged(const QString& encoding);
    void scriptLanguageChanged(const QString& language);
    void cookiesEnabledChanged(bool enabled);
    void cookiesFileChanged(const QString& path);
    void diskCacheEnabledChanged(bool enabled);
    void maxDiskCacheSizeChanged(int size);
    void diskCachePathChanged(const QString& path);
    void ignoreSslErrorsChanged(bool ignore);
    void sslProtocolChanged(const QString& protocol);
    void sslCiphersChanged(const QString& ciphers);
    void sslCertificatesPathChanged(const QString& path);
    void sslClientCertificateFileChanged(const QString& path);
    void sslClientKeyFileChanged(const QString& path);
    void sslClientKeyPassphraseChanged(const QByteArray& passphrase);
    void resourceTimeoutChanged(int timeout);
    void maxAuthAttemptsChanged(int attempts);
    void javascriptEnabledChanged(bool enabled);
    void webSecurityEnabledChanged(bool enabled);
    void webGLEnabledChanged(bool enabled);
    void javascriptCanOpenWindowsChanged(bool enabled);
    void javascriptCanCloseWindowsChanged(bool enabled);
    void localToRemoteUrlAccessEnabledChanged(bool enabled);
    void autoLoadImagesChanged(bool enabled);
    void localStoragePathChanged(const QString& path);
    void localStorageQuotaChanged(int quota);
    void offlineStoragePathChanged(const QString& path);
    void offlineStorageQuotaChanged(int quota);
    void printHeaderChanged(bool enable);
    void printFooterChanged(bool enable);
    void defaultPageSettingsChanged(QVariantMap settings);

private:
    Config(QObject* parent = nullptr); // Private constructor for singleton
    QVariantMap m_settings; // Store all settings as a map
    static Config* m_instance; // Singleton instance
};

// Global flags array for QCommandLine (if not directly part of Config class)
extern const struct QCommandLineConfigEntry flags[];

#endif // CONFIG_H
