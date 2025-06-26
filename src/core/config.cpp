/*
  This file is part of the PhantomJS project from Ofi Labs.

  Copyright (C) 2012 Ariya Hidayat <ariya.hidayat@gmail.com>
  Copyright (C) 2011 Ariya Hidayat <ariya.hidayat@gmail.com>
  Copyright (C) 2011 execjosh, http://execjosh.blogspot.com
  Copyright (C) 2013 James M. Greene <james.m.greene@gmail.com>

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

#include "config.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QMetaProperty>
#include <QMetaObject>
#include "consts.h"

const struct QCommandLineConfigEntry flags[] = { { "version", QCommandLine::Switch, QCommandLine::Default,
                                                     "Show program's version number and exit" },
    { "help", QCommandLine::Switch, QCommandLine::Default, "Show this help message and exit" },

    { "script", QCommandLine::Param, QCommandLine::Optional | QCommandLine::Positional,
        "Path to the PhantomJS script file to execute", "script", "script.js" },
    { "args", QCommandLine::Param, QCommandLine::Multiple | QCommandLine::Positional, "Arguments to pass to the script",
        "arg", "[script arguments...]" },
    { "config", QCommandLine::Param, QCommandLine::Optional, "Path to a JSON configuration file", "config",
        "config.json" },

    { "debug", QCommandLine::Switch, QCommandLine::Optional, "Prints additional warnings and debug messages" },
    { "console-level", QCommandLine::Param, QCommandLine::Optional,
        "Sets the level of messages printed to console (debug, info, warning, error, none)", "level", "info" },
    { "output-encoding", QCommandLine::Param, QCommandLine::Optional,
        "Sets the encoding for the console output (default: system encoding)", "encoding", "" },
    { "script-encoding", QCommandLine::Param, QCommandLine::Optional,
        "Sets the encoding for the script file (default: system encoding)", "encoding", "" },
    { "remote-debugger-port", QCommandLine::Param, QCommandLine::Optional,
        "Starts the script in a debug mode and listens on the specified port", "port", "" },
    { "remote-debugger-autorun", QCommandLine::Switch, QCommandLine::Optional, "Runs the script in a debug mode", "" },
    { "webdriver", QCommandLine::Param, QCommandLine::Optional, "Starts in WebDriver mode (e.g., --webdriver=8910)",
        "port", "" },
    { "webdriver-logfile", QCommandLine::Param, QCommandLine::Optional, "Path to the log file for WebDriver messages",
        "path", "" },
    { "webdriver-loglevel", QCommandLine::Param, QCommandLine::Optional,
        "Sets the level of messages printed to WebDriver log (debug, info, warning, error, none)", "level", "" },
    { "webdriver-selenium-grid-hub", QCommandLine::Param, QCommandLine::Optional,
        "URL of the Selenium Grid Hub (e.g., http://localhost:4444)", "url", "" },
    { "ignore-ssl-errors", QCommandLine::Switch, QCommandLine::Optional, "Ignores SSL errors", "" },
    { "ssl-protocol", QCommandLine::Param, QCommandLine::Optional,
        "Sets the SSL protocol (SSLv3, SSLv2, TLSv1, TLSv1.1, TLSv1.2, ANY)", "protocol", "" },
    { "ssl-ciphers", QCommandLine::Param, QCommandLine::Optional, "Sets the SSL ciphers (OpenSSL format)", "ciphers",
        "" },
    { "ssl-certificates-path", QCommandLine::Param, QCommandLine::Optional, "Sets the path for custom CA certificates",
        "path", "" },
    { "ssl-client-certificate-file", QCommandLine::Param, QCommandLine::Optional,
        "Sets the client certificate file for SSL", "file", "" },
    { "ssl-client-key-file", QCommandLine::Param, QCommandLine::Optional, "Sets the client private key file for SSL",
        "file", "" },
    { "ssl-client-key-passphrase", QCommandLine::Param, QCommandLine::Optional,
        "Sets the passphrase for the client private key", "passphrase", "" },
    { "proxy", QCommandLine::Param, QCommandLine::Optional,
        "Sets the proxy server (e.g., --proxy=user:password@host:port)", "proxy", "" },
    { "proxy-type", QCommandLine::Param, QCommandLine::Optional, "Sets the proxy type (http, socks5, none)", "type",
        "http" },
    { "proxy-auth", QCommandLine::Param, QCommandLine::Optional, "Sets the proxy authentication (user:password)",
        "auth", "" },
    { "cookies-file", QCommandLine::Param, QCommandLine::Optional, "Path to a file for persistent cookie storage",
        "file", "" },
    { "cookies-enabled", QCommandLine::Switch, QCommandLine::Optional,
        "Enables or disables persistent cookies (default: enabled)", "" },

    { "disk-cache", QCommandLine::Switch, QCommandLine::Optional, "Enables or disables disk cache (default: disabled)",
        "" },
    { "max-disk-cache-size", QCommandLine::Param, QCommandLine::Optional,
        "Sets the maximum size of the disk cache in MB", "size", "" },
    { "disk-cache-path", QCommandLine::Param, QCommandLine::Optional, "Sets the path for the disk cache", "path", "" },

    { "load-images", QCommandLine::Switch, QCommandLine::Optional,
        "Enables or disables image loading (default: enabled)", "" },
    { "local-to-remote-url-access", QCommandLine::Switch, QCommandLine::Optional,
        "Allows or disallows local content to access remote URLs (default: disabled)", "" },
    { "offline-storage-path", QCommandLine::Param, QCommandLine::Optional,
        "Sets the path for offline web application storage", "path", "" },
    { "offline-storage-quota", QCommandLine::Param, QCommandLine::Optional,
        "Sets the maximum size of the offline web application storage in MB", "size", "" },
    { "local-storage-path", QCommandLine::Param, QCommandLine::Optional, "Sets the path for HTML5 local storage",
        "path", "" },
    { "local-storage-quota", QCommandLine::Param, QCommandLine::Optional,
        "Sets the maximum size of HTML5 local storage in MB", "size", "" },
    { "resource-timeout", QCommandLine::Param, QCommandLine::Optional, "Sets the resource timeout in milliseconds",
        "timeout", "" },
    { "max-auth-attempts", QCommandLine::Param, QCommandLine::Optional,
        "Sets the maximum authentication attempts for network requests", "attempts", "" },
    { "javascript-enabled", QCommandLine::Switch, QCommandLine::Optional,
        "Enables or disables JavaScript (default: enabled)", "" },
    { "web-security", QCommandLine::Switch, QCommandLine::Optional,
        "Enables or disables web security (default: enabled)", "" },
    { "webgl-enabled", QCommandLine::Switch, QCommandLine::Optional, "Enables or disables WebGL (default: disabled)",
        "" },
    { "javascript-can-open-windows", QCommandLine::Switch, QCommandLine::Optional,
        "Allows or disallows JavaScript to open new windows (default: disabled)", "" },
    { "javascript-can-close-windows", QCommandLine::Switch, QCommandLine::Optional,
        "Allows or disallows JavaScript to close windows (default: disabled)", "" },
    { "print-header", QCommandLine::Switch, QCommandLine::Optional,
        "Enables or disables header in PDF rendering (default: disabled)", "" },
    { "print-footer", QCommandLine::Switch, QCommandLine::Optional,
        "Enables or disables footer in PDF rendering (default: disabled)", "" },

    QCOMMANDLINE_CONFIG_ENTRY_END };

Config* Config::m_instance = 0;

Config* Config::instance() {
    if (!m_instance) {
        m_instance = new Config();
    }
    return m_instance;
}

Config::Config(QObject* parent)
    : QObject(parent) {
    m_settings["debug"] = false;
    m_settings["console-level"] = "info";
    m_settings["output-encoding"] = "";
    m_settings["script-encoding"] = "";
    m_settings["script-language"] = "javascript";

    m_settings["cookies-enabled"] = true;
    m_settings["cookies-file"] = "";
    m_settings["disk-cache-enabled"] = false;
    m_settings["max-disk-cache-size"] = 0;
    m_settings["disk-cache-path"] = "";
    m_settings["ignore-ssl-errors"] = false;
    m_settings["ssl-protocol"] = "ANY";
    m_settings["ssl-ciphers"] = "";
    m_settings["ssl-certificates-path"] = "";
    m_settings["ssl-client-certificate-file"] = "";
    m_settings["ssl-client-key-file"] = "";
    m_settings["ssl-client-key-passphrase"] = QByteArray();
    m_settings["resource-timeout"] = 0;
    m_settings["max-auth-attempts"] = 3;

    m_settings["javascript-enabled"] = true;
    m_settings["web-security"] = true;
    m_settings["webgl-enabled"] = false;
    m_settings["javascript-can-open-windows"] = false;
    m_settings["javascript-can-close-windows"] = false;
    m_settings["local-to-remote-url-access-enabled"] = false;
    m_settings["auto-load-images"] = true;

    m_settings["local-storage-path"] = "";
    m_settings["local-storage-quota"] = 0;
    m_settings["offline-storage-path"] = "";
    m_settings["offline-storage-quota"] = 0;

    m_settings["print-header"] = false;
    m_settings["print-footer"] = false;

    QVariantMap defaultPageSettingsMap;
    defaultPageSettingsMap[PAGE_SETTINGS_USER_AGENT] = "";
    defaultPageSettingsMap[PAGE_SETTINGS_VIEWPORT_SIZE] = QVariantMap { { "width", 1024 }, { "height", 768 } };
    defaultPageSettingsMap[PAGE_SETTINGS_CLIP_RECT]
        = QVariantMap { { "left", 0 }, { "top", 0 }, { "width", 0 }, { "height", 0 } };
    defaultPageSettingsMap[PAGE_SETTINGS_SCROLL_POSITION] = QVariantMap { { "left", 0 }, { "top", 0 } };
    defaultPageSettingsMap[PAGE_SETTINGS_ZOOM_FACTOR] = 1.0;
    defaultPageSettingsMap[PAGE_SETTINGS_CUSTOM_HEADERS] = QVariantMap();
    defaultPageSettingsMap[PAGE_SETTINGS_NAVIGATION_LOCKED] = false;
    defaultPageSettingsMap[PAGE_SETTINGS_PAPER_SIZE] = QVariantMap();

    defaultPageSettingsMap[PAGE_SETTINGS_LOAD_IMAGES] = m_settings["auto-load-images"];
    defaultPageSettingsMap[PAGE_SETTINGS_JAVASCRIPT_ENABLED] = m_settings["javascript-enabled"];
    defaultPageSettingsMap[PAGE_SETTINGS_WEB_SECURITY] = m_settings["web-security"];
    defaultPageSettingsMap[PAGE_SETTINGS_WEBG_ENABLED] = m_settings["webgl-enabled"];
    defaultPageSettingsMap[PAGE_SETTINGS_JAVASCRIPT_CAN_OPEN_WINDOWS] = m_settings["javascript-can-open-windows"];
    defaultPageSettingsMap[PAGE_SETTINGS_JAVASCRIPT_CAN_CLOSE_WINDOWS] = m_settings["javascript-can-close-windows"];
    defaultPageSettingsMap[PAGE_SETTINGS_LOCAL_TO_REMOTE_URL_ACCESS_ENABLED]
        = m_settings["local-to-remote-url-access-enabled"];
    defaultPageSettingsMap[PAGE_SETTINGS_OFFLINE_STORAGE_PATH] = m_settings["offline-storage-path"];
    defaultPageSettingsMap[PAGE_SETTINGS_OFFLINE_STORAGE_QUOTA] = m_settings["offline-storage-quota"];
    defaultPageSettingsMap[PAGE_SETTINGS_LOCAL_STORAGE_PATH] = m_settings["local-storage-path"];
    defaultPageSettingsMap[PAGE_SETTINGS_LOCAL_STORAGE_QUOTA] = m_settings["local-storage-quota"];
    defaultPageSettingsMap[PAGE_SETTINGS_RESOURCE_TIMEOUT] = m_settings["resource-timeout"];
    defaultPageSettingsMap[PAGE_SETTINGS_MAX_AUTH_ATTEMPTS] = m_settings["max-auth-attempts"];

    m_settings["defaultPageSettings"] = defaultPageSettingsMap;

    connect(this, &Config::autoLoadImagesChanged, this, [this](bool value) {
        QVariantMap currentSettings = defaultPageSettings();
        currentSettings[PAGE_SETTINGS_LOAD_IMAGES] = value;
        setDefaultPageSettings(currentSettings);
    });
    connect(this, &Config::javascriptEnabledChanged, this, [this](bool value) {
        QVariantMap currentSettings = defaultPageSettings();
        currentSettings[PAGE_SETTINGS_JAVASCRIPT_ENABLED] = value;
        setDefaultPageSettings(currentSettings);
    });
    connect(this, &Config::webSecurityEnabledChanged, this, [this](bool value) {
        QVariantMap currentSettings = defaultPageSettings();
        currentSettings[PAGE_SETTINGS_WEB_SECURITY] = value;
        setDefaultPageSettings(currentSettings);
    });
    connect(this, &Config::webGLEnabledChanged, this, [this](bool value) {
        QVariantMap currentSettings = defaultPageSettings();
        currentSettings[PAGE_SETTINGS_WEBG_ENABLED] = value;
        setDefaultPageSettings(currentSettings);
    });
    connect(this, &Config::javascriptCanOpenWindowsChanged, this, [this](bool value) {
        QVariantMap currentSettings = defaultPageSettings();
        currentSettings[PAGE_SETTINGS_JAVASCRIPT_CAN_OPEN_WINDOWS] = value;
        setDefaultPageSettings(currentSettings);
    });
    connect(this, &Config::javascriptCanCloseWindowsChanged, this, [this](bool value) {
        QVariantMap currentSettings = defaultPageSettings();
        currentSettings[PAGE_SETTINGS_JAVASCRIPT_CAN_CLOSE_WINDOWS] = value;
        setDefaultPageSettings(currentSettings);
    });
    connect(this, &Config::localToRemoteUrlAccessEnabledChanged, this, [this](bool value) {
        QVariantMap currentSettings = defaultPageSettings();
        currentSettings[PAGE_SETTINGS_LOCAL_TO_REMOTE_URL_ACCESS_ENABLED] = value;
        setDefaultPageSettings(currentSettings);
    });
    connect(this, &Config::offlineStoragePathChanged, this, [this](const QString& value) {
        QVariantMap currentSettings = defaultPageSettings();
        currentSettings[PAGE_SETTINGS_OFFLINE_STORAGE_PATH] = value;
        setDefaultPageSettings(currentSettings);
    });
    connect(this, &Config::offlineStorageQuotaChanged, this, [this](int value) {
        QVariantMap currentSettings = defaultPageSettings();
        currentSettings[PAGE_SETTINGS_OFFLINE_STORAGE_QUOTA] = value;
        setDefaultPageSettings(currentSettings);
    });
    connect(this, &Config::localStoragePathChanged, this, [this](const QString& value) {
        QVariantMap currentSettings = defaultPageSettings();
        currentSettings[PAGE_SETTINGS_LOCAL_STORAGE_PATH] = value;
        setDefaultPageSettings(currentSettings);
    });
    connect(this, &Config::localStorageQuotaChanged, this, [this](int value) {
        QVariantMap currentSettings = defaultPageSettings();
        currentSettings[PAGE_SETTINGS_LOCAL_STORAGE_QUOTA] = value;
        setDefaultPageSettings(currentSettings);
    });
    connect(this, &Config::resourceTimeoutChanged, this, [this](int value) {
        QVariantMap currentSettings = defaultPageSettings();
        currentSettings[PAGE_SETTINGS_RESOURCE_TIMEOUT] = value;
        setDefaultPageSettings(currentSettings);
    });
    connect(this, &Config::maxAuthAttemptsChanged, this, [this](int value) {
        QVariantMap currentSettings = defaultPageSettings();
        currentSettings[PAGE_SETTINGS_MAX_AUTH_ATTEMPTS] = value;
        setDefaultPageSettings(currentSettings);
    });
}

bool Config::loadJsonFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Config: Could not open config file:" << filePath;
        return false;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Config: Failed to parse JSON config file:" << parseError.errorString();
        return false;
    }

    if (!jsonDoc.isObject()) {
        qWarning() << "Config: JSON config file is not a root object.";
        return false;
    }

    QJsonObject jsonObject = jsonDoc.object();
    QMetaObject metaObject = this->staticMetaObject;

    for (int i = metaObject.propertyOffset(); i < metaObject.propertyCount(); ++i) {
        QMetaProperty property = metaObject.property(i);
        QString propertyName = property.name();

        if (jsonObject.contains(propertyName)) {
            QVariant jsonValue = jsonObject[propertyName].toVariant();

            if (property.type() == QVariant::ByteArray && jsonValue.type() == QVariant::String) {
                jsonValue = jsonValue.toString().toUtf8();
            }

            if (property.write(this, jsonValue)) {
                qDebug() << "Config: Set property from JSON:" << propertyName << "=" << jsonValue;
            } else {
                qWarning() << "Config: Failed to set property" << propertyName << "with value" << jsonValue
                           << "from JSON. Type mismatch or invalid value?";
            }
        }
    }
    return true;
}

QVariant Config::get(const QString& key) const { return m_settings.value(key); }

#define IMPLEMENT_CONFIG_GETTER(TYPE, NAME, VARNAME)                                                                   \
    TYPE Config::NAME() const { return m_settings.value(VARNAME).value<TYPE>(); }

#define IMPLEMENT_CONFIG_SETTER(TYPE, NAME, VARNAME, SIGNAL)                                                           \
    void Config::set##NAME(const TYPE& value) {                                                                        \
        if (m_settings.value(VARNAME).value<TYPE>() != value) {                                                        \
            m_settings[VARNAME] = QVariant::fromValue(value);                                                          \
            emit SIGNAL(value);                                                                                        \
        }                                                                                                              \
    }

IMPLEMENT_CONFIG_GETTER(bool, debug, "debug")
IMPLEMENT_CONFIG_SETTER(bool, Debug, "debug", debugChanged)

IMPLEMENT_CONFIG_GETTER(QString, logLevel, "console-level")
IMPLEMENT_CONFIG_SETTER(QString, LogLevel, "console-level", logLevelChanged)

IMPLEMENT_CONFIG_GETTER(QString, outputEncoding, "output-encoding")
IMPLEMENT_CONFIG_SETTER(QString, OutputEncoding, "output-encoding", outputEncodingChanged)

IMPLEMENT_CONFIG_GETTER(QString, scriptEncoding, "script-encoding")
IMPLEMENT_CONFIG_SETTER(QString, ScriptEncoding, "script-encoding", scriptEncodingChanged)

IMPLEMENT_CONFIG_GETTER(QString, scriptLanguage, "script-language")
IMPLEMENT_CONFIG_SETTER(QString, ScriptLanguage, "script-language", scriptLanguageChanged)

IMPLEMENT_CONFIG_GETTER(bool, cookiesEnabled, "cookies-enabled")
IMPLEMENT_CONFIG_SETTER(bool, CookiesEnabled, "cookies-enabled", cookiesEnabledChanged)

IMPLEMENT_CONFIG_GETTER(QString, cookiesFile, "cookies-file")
IMPLEMENT_CONFIG_SETTER(QString, CookiesFile, "cookies-file", cookiesFileChanged)

IMPLEMENT_CONFIG_GETTER(bool, diskCacheEnabled, "disk-cache-enabled")
IMPLEMENT_CONFIG_SETTER(bool, DiskCacheEnabled, "disk-cache-enabled", diskCacheEnabledChanged)

IMPLEMENT_CONFIG_GETTER(int, maxDiskCacheSize, "max-disk-cache-size")
IMPLEMENT_CONFIG_SETTER(int, MaxDiskCacheSize, "max-disk-cache-size", maxDiskCacheSizeChanged)

IMPLEMENT_CONFIG_GETTER(QString, diskCachePath, "disk-cache-path")
IMPLEMENT_CONFIG_SETTER(QString, DiskCachePath, "disk-cache-path", diskCachePathChanged)

IMPLEMENT_CONFIG_GETTER(bool, ignoreSslErrors, "ignore-ssl-errors")
IMPLEMENT_CONFIG_SETTER(bool, IgnoreSslErrors, "ignore-ssl-errors", ignoreSslErrorsChanged)

IMPLEMENT_CONFIG_GETTER(QString, sslProtocol, "ssl-protocol")
IMPLEMENT_CONFIG_SETTER(QString, SslProtocol, "ssl-protocol", sslProtocolChanged)

IMPLEMENT_CONFIG_GETTER(QString, sslCiphers, "ssl-ciphers")
IMPLEMENT_CONFIG_SETTER(QString, SslCiphers, "ssl-ciphers", sslCiphersChanged)

IMPLEMENT_CONFIG_GETTER(QString, sslCertificatesPath, "ssl-certificates-path")
IMPLEMENT_CONFIG_SETTER(QString, SslCertificatesPath, "ssl-certificates-path", sslCertificatesPathChanged)

IMPLEMENT_CONFIG_GETTER(QString, sslClientCertificateFile, "ssl-client-certificate-file")
IMPLEMENT_CONFIG_SETTER(
    QString, SslClientCertificateFile, "ssl-client-certificate-file", sslClientCertificateFileChanged)

IMPLEMENT_CONFIG_GETTER(QString, sslClientKeyFile, "ssl-client-key-file")
IMPLEMENT_CONFIG_SETTER(QString, SslClientKeyFile, "ssl-client-key-file", sslClientKeyFileChanged)

QByteArray Config::sslClientKeyPassphrase() const {
    return m_settings.value("ssl-client-key-passphrase").toByteArray();
}
void Config::setSslClientKeyPassphrase(QByteArray value) {
    if (m_settings.value("ssl-client-key-passphrase").toByteArray() != value) {
        m_settings["ssl-client-key-passphrase"] = value;
        emit sslClientKeyPassphraseChanged(value);
    }
}

IMPLEMENT_CONFIG_GETTER(int, resourceTimeout, "resource-timeout")
IMPLEMENT_CONFIG_SETTER(int, ResourceTimeout, "resource-timeout", resourceTimeoutChanged)

IMPLEMENT_CONFIG_GETTER(int, maxAuthAttempts, "max-auth-attempts")
IMPLEMENT_CONFIG_SETTER(int, MaxAuthAttempts, "max-auth-attempts", maxAuthAttemptsChanged)

IMPLEMENT_CONFIG_GETTER(bool, javascriptEnabled, "javascript-enabled")
IMPLEMENT_CONFIG_SETTER(bool, JavascriptEnabled, "javascript-enabled", javascriptEnabledChanged)

IMPLEMENT_CONFIG_GETTER(bool, webSecurityEnabled, "web-security")
IMPLEMENT_CONFIG_SETTER(bool, WebSecurityEnabled, "web-security", webSecurityEnabledChanged)

IMPLEMENT_CONFIG_GETTER(bool, webGLEnabled, "webgl-enabled")
IMPLEMENT_CONFIG_SETTER(bool, WebGLEnabled, "webgl-enabled", webGLEnabledChanged)

IMPLEMENT_CONFIG_GETTER(bool, javascriptCanOpenWindows, "javascript-can-open-windows")
IMPLEMENT_CONFIG_SETTER(bool, JavascriptCanOpenWindows, "javascript-can-open-windows", javascriptCanOpenWindowsChanged)

IMPLEMENT_CONFIG_GETTER(bool, javascriptCanCloseWindows, "javascript-can-close-windows")
IMPLEMENT_CONFIG_SETTER(
    bool, JavascriptCanCloseWindows, "javascript-can-close-windows", javascriptCanCloseWindowsChanged)

IMPLEMENT_CONFIG_GETTER(bool, localToRemoteUrlAccessEnabled, "local-to-remote-url-access-enabled")
IMPLEMENT_CONFIG_SETTER(
    bool, LocalToRemoteUrlAccessEnabled, "local-to-remote-url-access-enabled", localToRemoteUrlAccessEnabledChanged)

IMPLEMENT_CONFIG_GETTER(bool, autoLoadImages, "auto-load-images")
IMPLEMENT_CONFIG_SETTER(bool, AutoLoadImages, "auto-load-images", autoLoadImagesChanged)

IMPLEMENT_CONFIG_GETTER(QString, localStoragePath, "local-storage-path")
IMPLEMENT_CONFIG_SETTER(QString, LocalStoragePath, "local-storage-path", localStoragePathChanged)

IMPLEMENT_CONFIG_GETTER(int, localStorageQuota, "local-storage-quota")
IMPLEMENT_CONFIG_SETTER(int, LocalStorageQuota, "local-storage-quota", localStorageQuotaChanged)

IMPLEMENT_CONFIG_GETTER(QString, offlineStoragePath, "offline-storage-path")
IMPLEMENT_CONFIG_SETTER(QString, OfflineStoragePath, "offline-storage-path", offlineStoragePathChanged)

IMPLEMENT_CONFIG_GETTER(int, offlineStorageQuota, "offline-storage-quota")
IMPLEMENT_CONFIG_SETTER(int, OfflineStorageQuota, "offline-storage-quota", offlineStorageQuotaChanged)

IMPLEMENT_CONFIG_GETTER(bool, printHeader, "print-header")
IMPLEMENT_CONFIG_SETTER(bool, PrintHeader, "print-header", printHeaderChanged)

IMPLEMENT_CONFIG_GETTER(bool, printFooter, "print-footer")
IMPLEMENT_CONFIG_SETTER(bool, PrintFooter, "print-footer", printFooterChanged)

QVariantMap Config::defaultPageSettings() const { return m_settings.value("defaultPageSettings").toMap(); }
void Config::setDefaultPageSettings(QVariantMap settings) {
    if (m_settings.value("defaultPageSettings").toMap() != settings) {
        m_settings["defaultPageSettings"] = settings;
        emit defaultPageSettingsChanged(settings);
    }
}
