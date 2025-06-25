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
#include "consts.h" // For PAGE_SETTINGS constants
#include "terminal.h"
#include "utils.h" // For readResourceFileUtf8 function

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStandardPaths> // For default paths
#include <QProcess> // For checking if Node.js is available

// Include QCommandLine here, as this is where the `flags` array is defined
#include "qcommandline/qcommandline.h"

// Global static instance (singleton pattern)
Config* Config::s_instance = nullptr;

// Command line configuration entries (static global, now defined in .cpp)
// This array needs to be defined once in a .cpp file.
// If it were in the .h, every .cpp including the .h would get its own copy,
// and it can interfere with moc.
const struct QCommandLineConfigEntry flags[] = { { QCommandLine::Option, '\0', "cookies-file",
                                                     "Sets the file to store cookies", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "config", "Specifies JSON-formatted configuration file", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "debug", "Prints additional warning and debug message: 'true' or 'false' (default)",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "disk-cache", "Enables disk cache: 'true' or 'false' (default)",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "disk-cache-path", "Specifies the location of the disk cache",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "ignore-ssl-errors", "Ignores SSL errors: 'true' or 'false' (default)",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "local-storage-path", "Specifies the location of the local storage",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "local-storage-quota", "Sets the maximum size of the local storage (in bytes)",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "load-images", "Loads all images: 'true' or 'false' (default)",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "local-to-remote-url-access",
        "Allows local content to access remote URLs: 'true' or 'false' (default)", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "max-disk-cache-size", "Sets the maximum size of the disk cache (in bytes)",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "offline-storage-path", "Specifies the location of the offline storage",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "offline-storage-quota", "Sets the maximum size of the offline storage (in bytes)",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "output-encoding", "Sets the encoding for the console output",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "phantom-path", "Sets the path to PhantomJS (used internally)",
        QCommandLine::Optional }, // Not really needed for Playwright
    { QCommandLine::Option, '\0', "proxy", "Sets the network proxy: 'user:password@proxyhost:proxyport' (default)",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "proxy-auth", "Sets the network proxy authentication: 'user:password'",
        QCommandLine::Optional }, // Handled in proxy string
    { QCommandLine::Option, '\0', "proxy-type", "Sets the network proxy type: 'http', 'socks5' (default)",
        QCommandLine::Optional }, // Handled in proxy string
    { QCommandLine::Option, '\0', "resource-timeout",
        "Sets the maximum timeout for network resources (in milliseconds)", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "script-encoding", "Sets the encoding for the script (default is UTF-8)",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "ssl-certificates-path", "Sets the path to a directory containing SSL certificates",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "ssl-protocol",
        "Sets the SSL protocol to use (e.g., 'SSLv3', 'TLSv1', 'TLSv1_0', 'TLSv1_1', 'TLSv1_2', 'TLSv1_3', 'Any')",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "web-security", "Enables web security: 'true' or 'false' (default)",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "webdriver", "Starts the WebDriver service (default is null)",
        QCommandLine::Optional }, // Not for Playwright
    { QCommandLine::Option, '\0', "webdriver-loglevel", "Sets the log level for WebDriver (default is 'INFO')",
        QCommandLine::Optional }, // Not for Playwright
    { QCommandLine::Option, '\0', "webdriver-selenium-grid-hub", "Sets the Selenium Grid hub URL (default is null)",
        QCommandLine::Optional }, // Not for Playwright
    { QCommandLine::Option, '\0', "console-level",
        "Sets the logging level for console output (e.g., 'debug', 'info', 'warning', 'error')",
        QCommandLine::Optional }, // Mapped to logLevel
    { QCommandLine::Option, '\0', "remote-debugger-port", "Starts the remote debugger on the specified port",
        QCommandLine::Optional }, // Mapped to showInspector
    { QCommandLine::Option, '\0', "remote-debugger-autorun",
        "Runs the script automatically when remote debugger connects",
        QCommandLine::Optional }, // No direct Playwright equivalent
    { QCommandLine::Option, '\0', "webdriver-port", "Starts WebDriver on the specified port (default is 8910)",
        QCommandLine::Optional }, // Not for Playwright
    { QCommandLine::Option, '\0', "ignore-ssl-errors", "Ignores SSL errors (duplicate, will consolidate)",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "cookies-enabled", "Enables cookies: 'true' or 'false' (default is true)",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "javascript-enabled", "Enables JavaScript: 'true' or 'false' (default is true)",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "javascript-can-open-windows",
        "Allows JavaScript to open new windows: 'true' or 'false' (default)", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "javascript-can-close-windows",
        "Allows JavaScript to close windows: 'true' or 'false' (default)", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "webgl-enabled", "Enables WebGL: 'true' or 'false' (default)",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "ssl-ciphers", "Sets the SSL ciphers to use (duplicate, will consolidate)",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "ssl-client-certificate-file", "Sets the client certificate file for SSL (duplicate)",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "ssl-client-key-file", "Sets the client key file for SSL (duplicate)",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "ssl-client-key-passphrase", "Sets the client key passphrase for SSL (duplicate)",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "print-header", "Enables printing header in PDF output: 'true' or 'false' (default)",
        QCommandLine::Optional },
    { QCommandLine::Option, '\0', "print-footer", "Enables printing footer in PDF output: 'true' or 'false' (default)",
        QCommandLine::Optional },
    { QCommandLine::Option, 'v', "version", "Prints PhantomJS version", QCommandLine::NoValue },
    { QCommandLine::Option, 'h', "help", "Prints this help message", QCommandLine::NoValue },
    { QCommandLine::File, '\0', "", "Script file to execute", QCommandLine::Optional },
    { QCommandLine::Args, '\0', "", "Script arguments", QCommandLine::Optional } };

// ... (Rest of your config.cpp content follows) ...
// The existing content for Config::Config, Config::instance, getters, setters,
// loadJsonFile, set, get, etc., remains here.

// Constructor implementation
Config::Config(QObject* parent)
    : QObject(parent)
    // Initialize default values based on PhantomJS original behavior or reasonable defaults
    , m_debug(false)
    , m_logLevel("info") // Default log level
    , m_cookiesFile("")
    , m_cookiesEnabled(true)
    , m_diskCacheEnabled(false)
    , m_maxDiskCacheSize(0)
    , m_diskCachePath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/phantomjs-disk-cache")
    , m_ignoreSslErrors(false)
    , m_sslProtocol("Any")
    , m_sslCiphers("")
    , m_sslCertificatesPath("")
    , m_sslClientCertificateFile("")
    , m_sslClientKeyFile("")
    , m_sslClientKeyPassphrase("")
    , m_resourceTimeout(0) // No timeout by default
    , m_maxAuthAttempts(3)
    , m_outputEncoding("UTF-8")
    , m_autoLoadImages(true)
    , m_javascriptEnabled(true)
    , m_webSecurityEnabled(true)
    , m_localToRemoteUrlAccessEnabled(false) // Default to false for security
    , m_offlineStorageEnabled(false)
    , m_offlineStoragePath(
          QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/phantomjs-offline-storage")
    , m_offlineStorageQuota(0) // Default to 0 (unlimited)
    , m_localStorageEnabled(true)
    , m_localStoragePath(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/phantomjs-local-storage")
    , m_localStorageQuota(0) // Default to 0 (unlimited)
    , m_printFooter(false)
    , m_printHeader(false)
    , m_scriptEncoding("UTF-8")
    , m_scriptLanguage("javascript")
    , m_webGLEnabled(true)
    , m_javascriptCanOpenWindows(false) // Default to false for security
    , m_javascriptCanCloseWindows(false) // Default to false for security
    // Initialize QSettings with appropriate application name/organization
    , m_settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(),
          QCoreApplication::applicationName() + "_config") {
    // Load persisted settings (if any) or apply defaults
    setDebug(m_settings.value("debug", m_debug).toBool());
    setLogLevel(m_settings.value("logLevel", m_logLevel).toString());
    setCookiesFile(m_settings.value("cookiesFile", m_cookiesFile).toString());
    setCookiesEnabled(m_settings.value("cookiesEnabled", m_cookiesEnabled).toBool());
    setDiskCacheEnabled(m_settings.value("diskCacheEnabled", m_diskCacheEnabled).toBool());
    setMaxDiskCacheSize(m_settings.value("maxDiskCacheSize", m_maxDiskCacheSize).toInt());
    setDiskCachePath(m_settings.value("diskCachePath", m_diskCachePath).toString());
    setIgnoreSslErrors(m_settings.value("ignoreSslErrors", m_ignoreSslErrors).toBool());
    setSslProtocol(m_settings.value("sslProtocol", m_sslProtocol).toString());
    setSslCiphers(m_settings.value("sslCiphers", m_sslCiphers).toString());
    setSslCertificatesPath(m_settings.value("sslCertificatesPath", m_sslCertificatesPath).toString());
    setSslClientCertificateFile(m_settings.value("sslClientCertificateFile", m_sslClientCertificateFile).toString());
    setSslClientKeyFile(m_settings.value("sslClientKeyFile", m_sslClientKeyFile).toString());
    setSslClientKeyPassphrase(m_settings.value("sslClientKeyPassphrase", m_sslClientKeyPassphrase).toByteArray());
    setResourceTimeout(m_settings.value("resourceTimeout", m_resourceTimeout).toInt());
    setMaxAuthAttempts(m_settings.value("maxAuthAttempts", m_maxAuthAttempts).toInt());
    setOutputEncoding(m_settings.value("outputEncoding", m_outputEncoding).toString());
    setAutoLoadImages(m_settings.value("autoLoadImages", m_autoLoadImages).toBool());
    setJavascriptEnabled(m_settings.value("javascriptEnabled", m_javascriptEnabled).toBool());
    setWebSecurityEnabled(m_settings.value("webSecurityEnabled", m_webSecurityEnabled).toBool());
    setLocalToRemoteUrlAccessEnabled(
        m_settings.value("localToRemoteUrlAccessEnabled", m_localToRemoteUrlAccessEnabled).toBool());
    setOfflineStorageEnabled(m_settings.value("offlineStorageEnabled", m_offlineStorageEnabled).toBool());
    setOfflineStoragePath(m_settings.value("offlineStoragePath", m_offlineStoragePath).toString());
    setOfflineStorageQuota(m_settings.value("offlineStorageQuota", m_offlineStorageQuota).toInt());
    setLocalStorageEnabled(m_settings.value("localStorageEnabled", m_localStorageEnabled).toBool());
    setLocalStoragePath(m_settings.value("localStoragePath", m_localStoragePath).toString());
    setLocalStorageQuota(m_settings.value("localStorageQuota", m_localStorageQuota).toInt());
    setPrintFooter(m_settings.value("printFooter", m_printFooter).toBool());
    setPrintHeader(m_settings.value("printHeader", m_printHeader).toBool());
    setScriptEncoding(m_settings.value("scriptEncoding", m_scriptEncoding).toString());
    setScriptLanguage(m_settings.value("scriptLanguage", m_scriptLanguage).toString());
    setWebGLEnabled(m_settings.value("webGLEnabled", m_webGLEnabled).toBool());
    setJavascriptCanOpenWindows(m_settings.value("javascriptCanOpenWindows", m_javascriptCanOpenWindows).toBool());
    setJavascriptCanCloseWindows(m_settings.value("javascriptCanCloseWindows", m_javascriptCanCloseWindows).toBool());

    qDebug() << "Config: Initialized. Debug:" << m_debug << "Log Level:" << m_logLevel;
}

Config* Config::instance() {
    if (!s_instance) {
        s_instance = new Config(QCoreApplication::instance());
    }
    return s_instance;
}

// Getters
bool Config::debug() const { return m_debug; }
QString Config::logLevel() const { return m_logLevel; }
QString Config::cookiesFile() const { return m_cookiesFile; }
bool Config::cookiesEnabled() const { return m_cookiesEnabled; }
bool Config::diskCacheEnabled() const { return m_diskCacheEnabled; }
int Config::maxDiskCacheSize() const { return m_maxDiskCacheSize; }
QString Config::diskCachePath() const { return m_diskCachePath; }
bool Config::ignoreSslErrors() const { return m_ignoreSslErrors; }
QString Config::sslProtocol() const { return m_sslProtocol; }
QString Config::sslCiphers() const { return m_sslCiphers; }
QString Config::sslCertificatesPath() const { return m_sslCertificatesPath; }
QString Config::sslClientCertificateFile() const { return m_sslClientCertificateFile; }
QString Config::sslClientKeyFile() const { return m_sslClientKeyFile; }
QByteArray Config::sslClientKeyPassphrase() const { return m_sslClientKeyPassphrase; }
int Config::resourceTimeout() const { return m_resourceTimeout; }
int Config::maxAuthAttempts() const { return m_maxAuthAttempts; }
QString Config::outputEncoding() const { return m_outputEncoding; }
bool Config::autoLoadImages() const { return m_autoLoadImages; }
bool Config::javascriptEnabled() const { return m_javascriptEnabled; }
bool Config::webSecurityEnabled() const { return m_webSecurityEnabled; }
bool Config::localToRemoteUrlAccessEnabled() const { return m_localToRemoteUrlAccessEnabled; }
bool Config::offlineStorageEnabled() const { return m_offlineStorageEnabled; }
QString Config::offlineStoragePath() const { return m_offlineStoragePath; }
int Config::offlineStorageQuota() const { return m_offlineStorageQuota; }
bool Config::localStorageEnabled() const { return m_localStorageEnabled; }
QString Config::localStoragePath() const { return m_localStoragePath; }
int Config::localStorageQuota() const { return m_localStorageQuota; }
bool Config::printFooter() const { return m_printFooter; }
bool Config::printHeader() const { return m_printHeader; }
QString Config::scriptEncoding() const { return m_scriptEncoding; }
QString Config::scriptLanguage() const { return m_scriptLanguage; }
bool Config::webGLEnabled() const { return m_webGLEnabled; }
bool Config::javascriptCanOpenWindows() const { return m_javascriptCanOpenWindows; }
bool Config::javascriptCanCloseWindows() const { return m_javascriptCanCloseWindows; }

// Setters
void Config::setDebug(bool debug) {
    if (m_debug != debug) {
        m_debug = debug;
        m_settings.setValue("debug", debug);
        emit debugChanged(debug);
        Terminal::instance()->setDebugMode(debug); // Update Terminal directly
        qDebug() << "Config: Debug mode set to" << debug;
    }
}

void Config::setLogLevel(const QString& level) {
    if (m_logLevel != level) {
        m_logLevel = level;
        m_settings.setValue("logLevel", level);
        emit logLevelChanged(level);
        // You might want to update Terminal's log level filtering here as well
        qDebug() << "Config: Log level set to" << level;
    }
}

void Config::setCookiesFile(const QString& path) {
    if (m_cookiesFile != path) {
        m_cookiesFile = path;
        m_settings.setValue("cookiesFile", path);
        emit cookiesFileChanged(path);
    }
}

void Config::setCookiesEnabled(bool enabled) {
    if (m_cookiesEnabled != enabled) {
        m_cookiesEnabled = enabled;
        m_settings.setValue("cookiesEnabled", enabled);
        emit cookiesEnabledChanged(enabled);
    }
}

void Config::setDiskCacheEnabled(bool enabled) {
    if (m_diskCacheEnabled != enabled) {
        m_diskCacheEnabled = enabled;
        m_settings.setValue("diskCacheEnabled", enabled);
        emit diskCacheEnabledChanged(enabled);
    }
}

void Config::setMaxDiskCacheSize(int size) {
    if (m_maxDiskCacheSize != size) {
        m_maxDiskCacheSize = size;
        m_settings.setValue("maxDiskCacheSize", size);
        emit maxDiskCacheSizeChanged(size);
    }
}

void Config::setDiskCachePath(const QString& path) {
    if (m_diskCachePath != path) {
        m_diskCachePath = path;
        m_settings.setValue("diskCachePath", path);
        emit diskCachePathChanged(path);
    }
}

void Config::setIgnoreSslErrors(bool ignore) {
    if (m_ignoreSslErrors != ignore) {
        m_ignoreSslErrors = ignore;
        m_settings.setValue("ignoreSslErrors", ignore);
        emit ignoreSslErrorsChanged(ignore);
    }
}

void Config::setSslProtocol(const QString& protocol) {
    if (m_sslProtocol != protocol) {
        m_sslProtocol = protocol;
        m_settings.setValue("sslProtocol", protocol);
        emit sslProtocolChanged(protocol);
    }
}

void Config::setSslCiphers(const QString& ciphers) {
    if (m_sslCiphers != ciphers) {
        m_sslCiphers = ciphers;
        m_settings.setValue("sslCiphers", ciphers);
        emit sslCiphersChanged(ciphers);
    }
}

void Config::setSslCertificatesPath(const QString& path) {
    if (m_sslCertificatesPath != path) {
        m_sslCertificatesPath = path;
        m_settings.setValue("sslCertificatesPath", path);
        emit sslCertificatesPathChanged(path);
    }
}

void Config::setSslClientCertificateFile(const QString& path) {
    if (m_sslClientCertificateFile != path) {
        m_sslClientCertificateFile = path;
        m_settings.setValue("sslClientCertificateFile", path);
        emit sslClientCertificateFileChanged(path);
    }
}

void Config::setSslClientKeyFile(const QString& path) {
    if (m_sslClientKeyFile != path) {
        m_sslClientKeyFile = path;
        m_settings.setValue("sslClientKeyFile", path);
        emit sslClientKeyFileChanged(path);
    }
}

void Config::setSslClientKeyPassphrase(const QByteArray& passphrase) {
    if (m_sslClientKeyPassphrase != passphrase) {
        m_sslClientKeyPassphrase = passphrase;
        m_settings.setValue("sslClientKeyPassphrase", passphrase);
        emit sslClientKeyPassphraseChanged(passphrase);
    }
}

void Config::setResourceTimeout(int timeout) {
    if (m_resourceTimeout != timeout) {
        m_resourceTimeout = timeout;
        m_settings.setValue("resourceTimeout", timeout);
        emit resourceTimeoutChanged(timeout);
    }
}

void Config::setMaxAuthAttempts(int attempts) {
    if (m_maxAuthAttempts != attempts) {
        m_maxAuthAttempts = attempts;
        m_settings.setValue("maxAuthAttempts", attempts);
        emit maxAuthAttemptsChanged(attempts);
    }
}

void Config::setOutputEncoding(const QString& encoding) {
    if (m_outputEncoding != encoding) {
        m_outputEncoding = encoding;
        m_settings.setValue("outputEncoding", encoding);
        emit outputEncodingChanged(encoding);
    }
}

void Config::setAutoLoadImages(bool autoLoad) {
    if (m_autoLoadImages != autoLoad) {
        m_autoLoadImages = autoLoad;
        m_settings.setValue("autoLoadImages", autoLoad);
        emit autoLoadImagesChanged(autoLoad);
    }
}

void Config::setJavascriptEnabled(bool enabled) {
    if (m_javascriptEnabled != enabled) {
        m_javascriptEnabled = enabled;
        m_settings.setValue("javascriptEnabled", enabled);
        emit javascriptEnabledChanged(enabled);
    }
}

void Config::setWebSecurityEnabled(bool enabled) {
    if (m_webSecurityEnabled != enabled) {
        m_webSecurityEnabled = enabled;
        m_settings.setValue("webSecurityEnabled", enabled);
        emit webSecurityEnabledChanged(enabled);
    }
}

void Config::setLocalToRemoteUrlAccessEnabled(bool enabled) {
    if (m_localToRemoteUrlAccessEnabled != enabled) {
        m_localToRemoteUrlAccessEnabled = enabled;
        m_settings.setValue("localToRemoteUrlAccessEnabled", enabled);
        emit localToRemoteUrlAccessEnabledChanged(enabled);
    }
}

void Config::setOfflineStorageEnabled(bool enabled) {
    if (m_offlineStorageEnabled != enabled) {
        m_offlineStorageEnabled = enabled;
        m_settings.setValue("offlineStorageEnabled", enabled);
        emit offlineStorageEnabledChanged(enabled);
    }
}

void Config::setOfflineStoragePath(const QString& path) {
    if (m_offlineStoragePath != path) {
        m_offlineStoragePath = path;
        m_settings.setValue("offlineStoragePath", path);
        emit offlineStoragePathChanged(path);
    }
}

void Config::setOfflineStorageQuota(int quota) {
    if (m_offlineStorageQuota != quota) {
        m_offlineStorageQuota = quota;
        m_settings.setValue("offlineStorageQuota", quota);
        emit offlineStorageQuotaChanged(quota);
    }
}

void Config::setLocalStorageEnabled(bool enabled) {
    if (m_localStorageEnabled != enabled) {
        m_localStorageEnabled = enabled;
        m_settings.setValue("localStorageEnabled", enabled);
        emit localStorageEnabledChanged(enabled);
    }
}

void Config::setLocalStoragePath(const QString& path) {
    if (m_localStoragePath != path) {
        m_localStoragePath = path;
        m_settings.setValue("localStoragePath", path);
        emit localStoragePathChanged(path);
    }
}

void Config::setLocalStorageQuota(int quota) {
    if (m_localStorageQuota != quota) {
        m_localStorageQuota = quota;
        m_settings.setValue("localStorageQuota", quota);
        emit localStorageQuotaChanged(quota);
    }
}

void Config::setPrintFooter(bool print) {
    if (m_printFooter != print) {
        m_printFooter = print;
        m_settings.setValue("printFooter", print);
        emit printFooterChanged(print);
    }
}

void Config::setPrintHeader(bool print) {
    if (m_printHeader != print) {
        m_printHeader = print;
        m_settings.setValue("printHeader", print);
        emit printHeaderChanged(print);
    }
}

void Config::setScriptEncoding(const QString& encoding) {
    if (m_scriptEncoding != encoding) {
        m_scriptEncoding = encoding;
        m_settings.setValue("scriptEncoding", encoding);
        emit scriptEncodingChanged(encoding);
    }
}

void Config::setScriptLanguage(const QString& language) {
    if (m_scriptLanguage != language) {
        m_scriptLanguage = language;
        m_settings.setValue("scriptLanguage", language);
        emit scriptLanguageChanged(language);
    }
}

void Config::setWebGLEnabled(bool enabled) {
    if (m_webGLEnabled != enabled) {
        m_webGLEnabled = enabled;
        m_settings.setValue("webGLEnabled", enabled);
        emit webGLEnabledChanged(enabled);
    }
}

void Config::setJavascriptCanOpenWindows(bool enabled) {
    if (m_javascriptCanOpenWindows != enabled) {
        m_javascriptCanOpenWindows = enabled;
        m_settings.setValue("javascriptCanOpenWindows", enabled);
        emit javascriptCanOpenWindowsChanged(enabled);
    }
}

void Config::setJavascriptCanCloseWindows(bool enabled) {
    if (m_javascriptCanCloseWindows != enabled) {
        m_javascriptCanCloseWindows = enabled;
        m_settings.setValue("javascriptCanCloseWindows", enabled);
        emit javascriptCanCloseWindowsChanged(enabled);
    }
}

// Methods
void Config::loadJsonFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Config: Could not open JSON config file:" << filePath;
        return;
    }

    QByteArray jsonData = file.readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Config: JSON parse error in config file" << filePath << ":" << parseError.errorString();
        return;
    }

    if (!doc.isObject()) {
        qWarning() << "Config: JSON config file" << filePath << "is not a valid JSON object.";
        return;
    }

    QJsonObject obj = doc.object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        set(it.key(), it.value().toVariant());
    }
    qDebug() << "Config: Loaded JSON configuration from" << filePath;
}

void Config::set(const QString& key, const QVariant& value) {
    const QMetaObject* meta = this->metaObject();
    int propertyIndex = meta->indexOfProperty(key.toLatin1().constData());
    if (propertyIndex != -1) {
        QMetaProperty property = meta->property(propertyIndex);
        if (property.isWritable()) {
            property.write(this, value);
            // This will internally call the setter and emit the signal if value changed.
            qDebug() << "Config: Set property" << key << "=" << value;
        } else {
            qWarning() << "Config: Property" << key << "is not writable.";
        }
    } else {
        qWarning() << "Config: Property" << key << "not found.";
    }
}

QVariant Config::get(const QString& key) const {
    const QMetaObject* meta = this->metaObject();
    int propertyIndex = meta->indexOfProperty(key.toLatin1().constData());
    if (propertyIndex != -1) {
        QMetaProperty property = meta->property(propertyIndex);
        if (property.isReadable()) {
            return property.read(this);
        } else {
            qWarning() << "Config: Property" << key << "is not readable.";
        }
    } else {
        qWarning() << "Config: Property" << key << "not found.";
    }
    return QVariant(); // Return invalid QVariant if property not found or not readable
}
