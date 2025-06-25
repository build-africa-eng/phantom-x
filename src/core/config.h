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

#include <QVariantMap>
#include <QVariantList>
#include <QString>
#include <QByteArray>
#include <QSettings> // For saving/loading simple settings

// QCommandLine includes are only needed in config.cpp for the flags array
// and in phantom.cpp for parsing. Not strictly needed here unless Config class
// directly uses QCommandLine types (which it shouldn't for properties).
// #include "qcommandline/qcommandline.h" // REMOVED THIS INCLUDE FROM HERE.

#include "terminal.h" // For setDebugMode etc.
#include "utils.h"    // For potential utility functions if Config needs them

// Forward declarations if any needed

// Global configuration management class
class Config : public QObject
{
    Q_OBJECT

public:
    explicit Config(QObject* parent = nullptr);
    static Config* instance(); // Singleton access

    // Configuration properties
    Q_PROPERTY(bool debug READ debug WRITE setDebug NOTIFY debugChanged)
    Q_PROPERTY(QString logLevel READ logLevel WRITE setLogLevel NOTIFY logLevelChanged)
    Q_PROPERTY(QString cookiesFile READ cookiesFile WRITE setCookiesFile NOTIFY cookiesFileChanged)
    Q_PROPERTY(bool cookiesEnabled READ cookiesEnabled WRITE setCookiesEnabled NOTIFY cookiesEnabledChanged)
    Q_PROPERTY(bool diskCacheEnabled READ diskCacheEnabled WRITE setDiskCacheEnabled NOTIFY diskCacheEnabledChanged)
    Q_PROPERTY(int maxDiskCacheSize READ maxDiskCacheSize WRITE setMaxDiskCacheSize NOTIFY maxDiskCacheSizeChanged)
    Q_PROPERTY(QString diskCachePath READ diskCachePath WRITE setDiskCachePath NOTIFY diskCachePathChanged)
    Q_PROPERTY(bool ignoreSslErrors READ ignoreSslErrors WRITE setIgnoreSslErrors NOTIFY ignoreSslErrorsChanged)
    Q_PROPERTY(QString sslProtocol READ sslProtocol WRITE setSslProtocol NOTIFY sslProtocolChanged)
    Q_PROPERTY(QString sslCiphers READ sslCiphers WRITE setSslCiphers NOTIFY sslCiphersChanged)
    Q_PROPERTY(QString sslCertificatesPath READ sslCertificatesPath WRITE setSslCertificatesPath NOTIFY sslCertificatesPathChanged)
    Q_PROPERTY(QString sslClientCertificateFile READ sslClientCertificateFile WRITE setSslClientCertificateFile NOTIFY sslClientCertificateFileChanged)
    Q_PROPERTY(QString sslClientKeyFile READ sslClientKeyFile WRITE setSslClientKeyFile NOTIFY sslClientKeyFileChanged)
    Q_PROPERTY(QByteArray sslClientKeyPassphrase READ sslClientKeyPassphrase WRITE setSslClientKeyPassphrase NOTIFY sslClientKeyPassphraseChanged)
    Q_PROPERTY(int resourceTimeout READ resourceTimeout WRITE setResourceTimeout NOTIFY resourceTimeoutChanged)
    Q_PROPERTY(int maxAuthAttempts READ maxAuthAttempts WRITE setMaxAuthAttempts NOTIFY maxAuthAttemptsChanged)
    Q_PROPERTY(QString outputEncoding READ outputEncoding WRITE setOutputEncoding NOTIFY outputEncodingChanged)
    Q_PROPERTY(bool autoLoadImages READ autoLoadImages WRITE setAutoLoadImages NOTIFY autoLoadImagesChanged)
    Q_PROPERTY(bool javascriptEnabled READ javascriptEnabled WRITE setJavascriptEnabled NOTIFY javascriptEnabledChanged)
    Q_PROPERTY(bool webSecurityEnabled READ webSecurityEnabled WRITE setWebSecurityEnabled NOTIFY webSecurityEnabledChanged)
    Q_PROPERTY(bool localToRemoteUrlAccessEnabled READ localToRemoteUrlAccessEnabled WRITE setLocalToRemoteUrlAccessEnabled NOTIFY localToRemoteUrlAccessEnabledChanged)
    Q_PROPERTY(bool offlineStorageEnabled READ offlineStorageEnabled WRITE setOfflineStorageEnabled NOTIFY offlineStorageEnabledChanged)
    Q_PROPERTY(QString offlineStoragePath READ offlineStoragePath WRITE setOfflineStoragePath NOTIFY offlineStoragePathChanged)
    Q_PROPERTY(int offlineStorageQuota READ offlineStorageQuota WRITE setOfflineStorageQuota NOTIFY offlineStorageQuotaChanged)
    Q_PROPERTY(bool localStorageEnabled READ localStorageEnabled WRITE setLocalStorageEnabled NOTIFY localStorageEnabledChanged)
    Q_PROPERTY(QString localStoragePath READ localStoragePath WRITE setLocalStoragePath NOTIFY localStoragePathChanged)
    Q_PROPERTY(int localStorageQuota READ localStorageQuota WRITE setLocalStorageQuota NOTIFY localStorageQuotaChanged)
    Q_PROPERTY(bool printFooter READ printFooter WRITE setPrintFooter NOTIFY printFooterChanged)
    Q_PROPERTY(bool printHeader READ printHeader WRITE setPrintHeader NOTIFY printHeaderChanged)
    Q_PROPERTY(QString scriptEncoding READ scriptEncoding WRITE setScriptEncoding NOTIFY scriptEncodingChanged)
    Q_PROPERTY(QString scriptLanguage READ scriptLanguage WRITE setScriptLanguage NOTIFY scriptLanguageChanged)
    Q_PROPERTY(bool webGLEnabled READ webGLEnabled WRITE setWebGLEnabled NOTIFY webGLEnabledChanged)
    Q_PROPERTY(bool javascriptCanOpenWindows READ javascriptCanOpenWindows WRITE setJavascriptCanOpenWindows NOTIFY javascriptCanOpenWindowsChanged)
    Q_PROPERTY(bool javascriptCanCloseWindows READ javascriptCanCloseWindows WRITE setJavascriptCanCloseWindows NOTIFY javascriptCanCloseWindowsChanged)

    // Getters (all existing ones remain)
    bool debug() const;
    QString logLevel() const;
    QString cookiesFile() const;
    bool cookiesEnabled() const;
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
    QString outputEncoding() const;
    bool autoLoadImages() const;
    bool javascriptEnabled() const;
    bool webSecurityEnabled() const;
    bool localToRemoteUrlAccessEnabled() const;
    bool offlineStorageEnabled() const;
    QString offlineStoragePath() const;
    int offlineStorageQuota() const;
    bool localStorageEnabled() const;
    QString localStoragePath() const;
    int localStorageQuota() const;
    bool printFooter() const;
    bool printHeader() const;
    QString scriptEncoding() const;
    QString scriptLanguage() const;
    bool webGLEnabled() const;
    bool javascriptCanOpenWindows() const;
    bool javascriptCanCloseWindows() const;

    // Setters (all existing ones remain)
    void setDebug(bool debug);
    void setLogLevel(const QString& level);
    void setCookiesFile(const QString& path);
    void setCookiesEnabled(bool enabled);
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
    void setOutputEncoding(const QString& encoding);
    void setAutoLoadImages(bool autoLoad);
    void setJavascriptEnabled(bool enabled);
    void setWebSecurityEnabled(bool enabled);
    void setLocalToRemoteUrlAccessEnabled(bool enabled);
    void setOfflineStorageEnabled(bool enabled);
    void setOfflineStoragePath(const QString& path);
    void setOfflineStorageQuota(int quota);
    void setLocalStorageEnabled(bool enabled);
    void setLocalStoragePath(const QString& path);
    void setLocalStorageQuota(int quota);
    void setPrintFooter(bool print);
    void setPrintHeader(bool print);
    void setScriptEncoding(const QString& encoding);
    void setScriptLanguage(const QString& language);
    void setWebGLEnabled(bool enabled);
    void setJavascriptCanOpenWindows(bool enabled);
    void setJavascriptCanCloseWindows(bool enabled);

    // Methods (all existing ones remain)
    Q_INVOKABLE void loadJsonFile(const QString& filePath);
    Q_INVOKABLE void set(const QString& key, const QVariant& value);
    Q_INVOKABLE QVariant get(const QString& key) const;

signals:
    // Signals (all existing ones remain)
    void debugChanged(bool debug);
    void logLevelChanged(const QString& level);
    void cookiesFileChanged(const QString& path);
    void cookiesEnabledChanged(bool enabled);
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
    void outputEncodingChanged(const QString& encoding);
    void autoLoadImagesChanged(bool autoLoad);
    void javascriptEnabledChanged(bool enabled);
    void webSecurityEnabledChanged(bool enabled);
    void localToRemoteUrlAccessEnabledChanged(bool enabled);
    void offlineStorageEnabledChanged(bool enabled);
    void offlineStoragePathChanged(const QString& path);
    void offlineStorageQuotaChanged(int quota);
    void localStorageEnabledChanged(bool enabled);
    void localStoragePathChanged(const QString& path);
    void localStorageQuotaChanged(int quota);
    void printFooterChanged(bool print);
    void printHeaderChanged(bool print);
    void scriptEncodingChanged(const QString& encoding);
    void scriptLanguageChanged(const QString& language);
    void webGLEnabledChanged(bool enabled);
    void javascriptCanOpenWindowsChanged(bool enabled);
    void javascriptCanCloseWindowsChanged(bool enabled);

private:
    static Config* s_instance; // Singleton instance

    // Private members to hold property values (all existing ones remain)
    bool m_debug;
    QString m_logLevel;
    QString m_cookiesFile;
    bool m_cookiesEnabled;
    bool m_diskCacheEnabled;
    int m_maxDiskCacheSize;
    QString m_diskCachePath;
    bool m_ignoreSslErrors;
    QString m_sslProtocol;
    QString m_sslCiphers;
    QString m_sslCertificatesPath;
    QString m_sslClientCertificateFile;
    QString m_sslClientKeyFile;
    QByteArray m_sslClientKeyPassphrase;
    int m_resourceTimeout;
    int m_maxAuthAttempts;
    QString m_outputEncoding;
    bool m_autoLoadImages;
    bool m_javascriptEnabled;
    bool m_webSecurityEnabled;
    bool m_localToRemoteUrlAccessEnabled;
    bool m_offlineStorageEnabled;
    QString m_offlineStoragePath;
    int m_offlineStorageQuota;
    bool m_localStorageEnabled;
    QString m_localStoragePath;
    int m_localStorageQuota;
    bool m_printFooter;
    bool m_printHeader;
    QString m_scriptEncoding;
    QString m_scriptLanguage;
    bool m_webGLEnabled;
    bool m_javascriptCanOpenWindows;
    bool m_javascriptCanCloseWindows;

    // A private QSettings object for persistent storage if needed
    QSettings m_settings;
};

// IMPORTANT: QCommandLineConfigEntry flags[] definition is in config.cpp
// If any other header *requires* this definition, you can declare it here:
// extern const struct QCommandLineConfigEntry flags[];

#endif // CONFIG_H
