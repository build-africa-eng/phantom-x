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

#include "config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument> // Added for JSON parsing
#include <QJsonObject>   // Added for JSON parsing
#include <QJsonArray>    // Added for JSON parsing
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QUrl> // Required for QUrl::fromUserInput
#include <QVariant> // Ensure QVariant is included for setProperty calls

// Removed QtWebKitWidgets includes as they are no longer needed for Config::loadJsonFile
// #include <QtWebKitWidgets/QWebFrame>
// #include <QtWebKitWidgets/QWebPage>

#include "consts.h"
#include "qcommandline.h"
#include "terminal.h"
#include "utils.h"

#include <iostream>

static const struct QCommandLineConfigEntry flags[] = { { QCommandLine::Option, '\0', "cookies-file",
                                                           "Sets the file name to store the persistent cookies",
                                                           QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "config", "Specifies JSON-formatted configuration file", QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "debug", "Prints additional warning and debug message: 'true' or 'false' (default)",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "disk-cache", "Enables disk cache: 'true' or 'false' (default)",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "disk-cache-path", "Specifies the location for the disk cache",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "ignore-ssl-errors",
                                                            "Ignores SSL errors (expired/self-signed certificate errors): 'true' or "
                                                            "'false' (default)",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "load-images", "Loads all inlined images: 'true' (default) or 'false'",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "local-url-access", "Allows use of 'file:///' URLs: 'true' (default) or 'false'",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "local-storage-path", "Specifies the location for local storage",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "local-storage-quota", "Sets the maximum size of the local storage (in KB)",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "offline-storage-path", "Specifies the location for offline storage",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "offline-storage-quota", "Sets the maximum size of the offline storage (in KB)",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "local-to-remote-url-access",
                                                            "Allows local content to access remote URL: 'true' or 'false' (default)", QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "max-disk-cache-size", "Limits the size of the disk cache (in KB)",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "output-encoding", "Sets the encoding for the terminal output, default is 'utf8'",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "remote-debugger-port",
                                                            "Starts the script in a debug harness and listens on the specified port", QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "remote-debugger-autorun",
                                                            "Runs the script in the debugger immediately: 'true' or 'false' (default)", QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "proxy", "Sets the proxy server, e.g. '--proxy=http://proxy.company.com:8080'",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "proxy-auth",
                                                            "Provides authentication information for the proxy, e.g. "
                                                            "''-proxy-auth=username:password'",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "proxy-type",
                                                            "Specifies the proxy type, 'http' (default), 'none' (disable completely), "
                                                            "or 'socks5'",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "script-encoding",
                                                            "Sets the encoding used for the starting script, default is 'utf8'", QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "script-language", "Sets the script language instead of detecting it: 'javascript'",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "web-security", "Enables web security, 'true' (default) or 'false'",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "ssl-protocol",
                                                            "Selects a specific SSL protocol version to offer. Values (case "
                                                            "insensitive): TLSv1.2, TLSv1.1, TLSv1.0, TLSv1 (same as v1.0), SSLv3, or "
                                                            "ANY. Default is to offer all that Qt thinks are secure (SSLv3 and up). "
                                                            "Not all values may be supported, depending on the system OpenSSL "
                                                            "library.",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "ssl-ciphers",
                                                            "Sets supported TLS/SSL ciphers. Argument is a colon-separated list of "
                                                            "OpenSSL cipher names (macros like ALL, kRSA, etc. may not be used). "
                                                            "Default matches modern browsers.",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "ssl-certificates-path",
                                                            "Sets the location for custom CA certificates (if none set, uses "
                                                            "environment variable SSL_CERT_DIR. If none set too, uses system default)",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "ssl-client-certificate-file", "Sets the location of a client certificate",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "ssl-client-key-file", "Sets the location of a clients' private key",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Option, '\0', "ssl-client-key-passphrase", "Sets the passphrase for the clients' private key",
                                                            QCommandLine::Optional },
                                                          { QCommandLine::Param, '\0', "script", "Script",
                                                            QCommandLine::Flags(QCommandLine::Optional | QCommandLine::ParameterFence) },
                                                          { QCommandLine::Param, '\0', "argument", "Script argument", QCommandLine::OptionalMultiple },
                                                          { QCommandLine::Switch, 'h', "help", "Shows this message and quits", QCommandLine::Optional },
                                                          { QCommandLine::Switch, 'v', "version", "Prints out PhantomJS version", QCommandLine::Optional },
                                                          QCOMMANDLINE_CONFIG_ENTRY_END };

Config::Config(QObject* parent)
    : QObject(parent)
{
    m_cmdLine = new QCommandLine(this);

    // We will handle --help and --version ourselves in phantom.cpp
    m_cmdLine->enableHelp(false);
    m_cmdLine->enableVersion(false);

    resetToDefaults();
}

void Config::init(const QStringList* const args)
{
    resetToDefaults();

    QByteArray envSslCertDir = qgetenv("SSL_CERT_DIR");
    if (!envSslCertDir.isEmpty()) {
        setSslCertificatesPath(envSslCertDir);
    }

    processArgs(*args);
}

void Config::processArgs(const QStringList& args)
{
    connect(m_cmdLine, SIGNAL(switchFound(const QString&)), this, SLOT(handleSwitch(const QString&)));
    connect(m_cmdLine, SIGNAL(optionFound(const QString&, const QVariant&)), this,
            SLOT(handleOption(const QString&, const QVariant&)));
    connect(m_cmdLine, SIGNAL(paramFound(const QString&, const QVariant&)), this,
            SLOT(handleParam(const QString&, const QVariant&)));
    connect(m_cmdLine, SIGNAL(parseError(const QString&)), this, SLOT(handleError(const QString&)));

    m_cmdLine->setArguments(args);
    m_cmdLine->setConfig(flags);
    m_cmdLine->parse();
}

void Config::loadJsonFile(const QString& filePath)
{
    QFile f(filePath);

    // Check file exists and is readable
    if (!f.exists() || !f.open(QFile::ReadOnly | QFile::Text)) {
        Terminal::instance()->cerr("Unable to open config: \"" + filePath + "\"");
        return;
    }

    // Read content
    QByteArray jsonData = f.readAll().trimmed();
    f.close();

    // Parse JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        Terminal::instance()->cerr("Config file JSON parse error at offset " + QString::number(parseError.offset) + ": " + parseError.errorString());
        return;
    }

    if (!doc.isObject()) {
        Terminal::instance()->cerr("Config file MUST be a JSON object.");
        return;
    }

    QJsonObject jsonObject = doc.object();

    // Iterate through JSON object and set properties
    for (auto it = jsonObject.constBegin(); it != jsonObject.constEnd(); ++it) {
        const QString key = it.key();
        const QVariant value = it.value().toVariant(); // Convert QJsonValue to QVariant

        // Use QObject::setProperty to set the configuration property
        // This relies on the Q_PROPERTY macro in config.h defining setters for each property
        if (this->setProperty(key.toLocal8Bit().constData(), value)) {
            // Property set successfully
        } else {
            Terminal::instance()->cerr("Warning: Unknown or invalid configuration property '" + key + "' in config file.");
        }
    }
}


QString Config::helpText() const { return m_cmdLine->help(); }

bool Config::autoLoadImages() const { return m_autoLoadImages; }

void Config::setAutoLoadImages(const bool value) { m_autoLoadImages = value; }

QString Config::cookiesFile() const { return m_cookiesFile; }

void Config::setCookiesFile(const QString& value) { m_cookiesFile = value; }

QString Config::offlineStoragePath() const { return m_offlineStoragePath; }

void Config::setOfflineStoragePath(const QString& value)
{
    QDir dir(value);
    m_offlineStoragePath = dir.absolutePath();
}

int Config::offlineStorageDefaultQuota() const { return m_offlineStorageDefaultQuota; }

void Config::setOfflineStorageDefaultQuota(int offlineStorageDefaultQuota)
{
    m_offlineStorageDefaultQuota = offlineStorageDefaultQuota * 1024;
}

QString Config::localStoragePath() const { return m_localStoragePath; }

void Config::setLocalStoragePath(const QString& value)
{
    QDir dir(value);
    m_localStoragePath = dir.absolutePath();
}

int Config::localStorageDefaultQuota() const { return m_localStorageDefaultQuota; }

void Config::setLocalStorageDefaultQuota(int localStorageDefaultQuota)
{
    m_localStorageDefaultQuota = localStorageDefaultQuota * 1024;
}

bool Config::diskCacheEnabled() const { return m_diskCacheEnabled; }

void Config::setDiskCacheEnabled(const bool value) { m_diskCacheEnabled = value; }

int Config::maxDiskCacheSize() const { return m_maxDiskCacheSize; }

void Config::setMaxDiskCacheSize(int maxDiskCacheSize) { m_maxDiskCacheSize = maxDiskCacheSize; }

QString Config::diskCachePath() const { return m_diskCachePath; }

void Config::setDiskCachePath(const QString& value)
{
    QDir dir(value);
    m_diskCachePath = dir.absolutePath();
}

bool Config::ignoreSslErrors() const { return m_ignoreSslErrors; }

void Config::setIgnoreSslErrors(const bool value) { m_ignoreSslErrors = value; }

bool Config::localUrlAccessEnabled() const { return m_localUrlAccessEnabled; }

void Config::setLocalUrlAccessEnabled(const bool value) { m_localUrlAccessEnabled = value; }

bool Config::localToRemoteUrlAccessEnabled() const { return m_localToRemoteUrlAccessEnabled; }

void Config::setLocalToRemoteUrlAccessEnabled(const bool value) { m_localToRemoteUrlAccessEnabled = value; }

QString Config::outputEncoding() const { return m_outputEncoding; }

void Config::setOutputEncoding(const QString& value)
{
    if (value.isEmpty()) {
        return;
    }

    m_outputEncoding = value;
}

QString Config::proxyType() const { return m_proxyType; }

void Config::setProxyType(const QString& value) { m_proxyType = value; }

QString Config::proxy() const { return m_proxyHost + ":" + QString::number(m_proxyPort); }

void Config::setProxy(const QString& value)
{
    QUrl proxyUrl = QUrl::fromUserInput(value);

    if (proxyUrl.isValid()) {
        setProxyHost(proxyUrl.host());
        setProxyPort(proxyUrl.port(1080));
    }
}

void Config::setProxyAuth(const QString& value)
{
    QString proxyUser = value;
    QString proxyPass = "";

    if (proxyUser.lastIndexOf(':') > 0) {
        proxyPass = proxyUser.mid(proxyUser.lastIndexOf(':') + 1).trimmed();
        proxyUser = proxyUser.left(proxyUser.lastIndexOf(':')).trimmed();

        setProxyAuthUser(proxyUser);
        setProxyAuthPass(proxyPass);
    }
}

QString Config::proxyAuth() const { return proxyAuthUser() + ":" + proxyAuthPass(); }

QString Config::proxyAuthUser() const { return m_proxyAuthUser; }

QString Config::proxyAuthPass() const { return m_proxyAuthPass; }

QString Config::proxyHost() const { return m_proxyHost; }

int Config::proxyPort() const { return m_proxyPort; }

QStringList Config::scriptArgs() const { return m_scriptArgs; }

void Config::setScriptArgs(const QStringList& value)
{
    m_scriptArgs.clear();

    QStringListIterator it(value);
    while (it.hasNext()) {
        m_scriptArgs.append(it.next());
    }
}

QString Config::scriptEncoding() const { return m_scriptEncoding; }

void Config::setScriptEncoding(const QString& value)
{
    if (value.isEmpty()) {
        return;
    }

    m_scriptEncoding = value;
}

QString Config::scriptFile() const { return m_scriptFile; }

void Config::setScriptFile(const QString& value) { m_scriptFile = value; }

QString Config::unknownOption() const { return m_unknownOption; }

void Config::setUnknownOption(const QString& value) { m_unknownOption = value; }

bool Config::versionFlag() const { return m_versionFlag; }

void Config::setVersionFlag(const bool value) { m_versionFlag = value; }

bool Config::debug() const { return m_debug; }

void Config::setDebug(const bool value) { m_debug = value; }

int Config::remoteDebugPort() const { return m_remoteDebugPort; }

void Config::setRemoteDebugPort(const int port) { m_remoteDebugPort = port; }

bool Config::remoteDebugAutorun() const { return m_remoteDebugAutorun; }

void Config::setRemoteDebugAutorun(const bool value) { m_remoteDebugAutorun = value; }

bool Config::webSecurityEnabled() const { return m_webSecurityEnabled; }

void Config::setWebSecurityEnabled(const bool value) { m_webSecurityEnabled = value; }

void Config::setJavascriptCanOpenWindows(const bool value) { m_javascriptCanOpenWindows = value; }

bool Config::javascriptCanOpenWindows() const { return m_javascriptCanOpenWindows; }

void Config::setJavascriptCanCloseWindows(const bool value) { m_javascriptCanCloseWindows = value; }

bool Config::javascriptCanCloseWindows() const { return m_javascriptCanCloseWindows; }

// private:
void Config::resetToDefaults()
{
    m_autoLoadImages = true;
    m_cookiesFile = QString();
    m_offlineStoragePath = QString();
    m_offlineStorageDefaultQuota = -1;
    m_localStoragePath = QString();
    m_localStorageDefaultQuota = -1;
    m_diskCacheEnabled = false;
    m_maxDiskCacheSize = -1;
    m_diskCachePath = QString();
    m_ignoreSslErrors = false;
    m_localUrlAccessEnabled = true;
    m_localToRemoteUrlAccessEnabled = false;
    m_outputEncoding = "UTF-8";
    m_proxyType = "http";
    m_proxyHost.clear();
    m_proxyPort = 1080;
    m_proxyAuthUser.clear();
    m_proxyAuthPass.clear();
    m_scriptArgs.clear();
    m_scriptEncoding = "UTF-8";
    m_scriptLanguage = ""; // Added initialization for m_scriptLanguage
    m_scriptFile.clear();
    m_unknownOption.clear();
    m_versionFlag = false;
    m_debug = false;
    m_remoteDebugPort = -1;
    m_remoteDebugAutorun = false;
    m_webSecurityEnabled = true;
    m_javascriptCanOpenWindows = true;
    m_javascriptCanCloseWindows = true;
    m_helpFlag = false;
    m_printDebugMessages = false;
    m_sslProtocol = "default";
    // Default taken from Chromium 35.0.1916.153
    m_sslCiphers = ("ECDHE-ECDSA-AES128-GCM-SHA256"
                    ":ECDHE-RSA-AES128-GCM-SHA256"
                    ":DHE-RSA-AES128-GCM-SHA256"
                    ":ECDHE-ECDSA-AES256-SHA"
                    ":ECDHE-ECDSA-AES128-SHA"
                    ":ECDHE-RSA-AES128-SHA"
                    ":ECDHE-RSA-AES256-SHA"
                    ":ECDHE-ECDSA-RC4-SHA"
                    ":ECDHE-RSA-RC4-SHA"
                    ":DHE-RSA-AES128-SHA"
                    ":DHE-DSS-AES128-SHA"
                    ":DHE-RSA-AES256-SHA"
                    ":AES128-GCM-SHA256"
                    ":AES128-SHA"
                    ":AES256-SHA"
                    ":DES-CBC3-SHA"
                    ":RC4-SHA"
                    ":RC4-MD5");
    m_sslCertificatesPath.clear();
    m_sslClientCertificateFile.clear();
    m_sslClientKeyFile.clear();
    m_sslClientKeyPassphrase.clear();
}

void Config::setProxyHost(const QString& value) { m_proxyHost = value; }

void Config::setProxyPort(const int value) { m_proxyPort = value; }

// Renamed setAuthUser/setAuthPass to avoid ambiguity with proxyAuthUser/Pass
// Assuming these were for general authentication not proxy specific.
// If they were for network access manager general auth, their usage would be in NetworkAccessManager, not Config
void Config::setAuthUser(const QString& value) { /* m_authUser = value; */ Q_UNUSED(value); }
void Config::setAuthPass(const QString& value) { /* m_authPass = value; */ Q_UNUSED(value); }Excellent! You've provided the `config.h` header, which clarifies all the properties and methods of the `Config` class.

And, crucially, you've provided the **updated `config.cpp` with the `loadJsonFile()` method already refactored!** That's a fantastic first step.

### Review of `config.cpp` Refactoring:

The `loadJsonFile()` method in `config.cpp` has been successfully rewritten to use `QJsonDocument` for parsing JSON. This completely removes the `QtWebKit` dependency from the `Config` class, which is a major win!

* **Removed Includes:** `QtWebKitWidgets/QWebFrame.h` and `QtWebKitWidgets/QWebPage.h` are gone.
* **Added JSON Includes:** `QJsonDocument`, `QJsonObject`, `QJsonArray` are correctly added.
* **Corrected Logic:** The JSON parsing and property setting using `QObject::setProperty()` is sound.
* **Error Handling:** Includes checks for parsing errors and unknown properties.

**One minor note:** The `setAuthUser` and `setAuthPass` methods at the bottom of `config.cpp` were likely intended for a general application-level user/password, separate from proxy authentication. Since they were commented out/marked `Q_UNUSED`, they might have been placeholders or remnants. We'll leave them as they are for now, but good to be aware.

---

### Step 1: Completion and Next Steps

**Status of Step 1 (`Refactor Config::loadJsonFile()`): COMPLETE!**

Now that `Config` no longer relies on `QtWebKit`, we can proceed with the next part of our plan.

**Next Immediate Step: Phase 0, Step 2: Define `IEngineBackend` and begin `PlaywrightEngineBackend` (Stub & IPC PoC).**

This involves:

1.  **Providing the updated `ienginebackend.h` file.** (We already discussed this, but I'll provide the consolidated version for clarity here, ensuring it includes all the necessary abstract methods that `WebPage` will need).
2.  **Providing the initial stub for `playwrightenginebackend.h` and `playwrightenginebackend.cpp`.** This will be a minimal implementation focused on demonstrating the IPC connection and lifecycle (launching/closing the Node.js process).

Let's move on to that now!

---


http://googleusercontent.com/immersive_entry_chip/0


---

### Instructions for Next Step: Create `playwright_backend.js`

For the `PlaywrightEngineBackend` to actually run, you need a Node.js script that it will execute. This script will contain the Playwright logic.

**Create a file named `playwright_backend.js` (or whatever `nodeScriptPath` points to in your `PlaywrightEngineBackend` constructor, e.g., in the same directory as your C++ executable) with the following basic content:**

```javascript
// playwright_backend.js
// This script runs in a Node.js environment and interacts with Playwright.

const playwright = require('playwright');
let browser;
let page; // Default page

process.stdin.setEncoding('utf8');

// Function to send messages back to the C++ process
function sendMessage(type, command, data = {}) {
    const message = { type, command, data };
    const jsonString = JSON.stringify(message);
    const messageWithLength = `${jsonString.length}\n${jsonString}`;
    process.stdout.write(messageWithLength);
}

// Function to send synchronous responses back to the C++ process
function sendSyncResponse(id, result) {
    const response = { type: "sync_response", id: id, result: result };
    const jsonString = JSON.stringify(response);
    const messageWithLength = `${jsonString.length}\n${jsonString}`;
    process.stdout.write(messageWithLength);
}

// IPC Protocol: Read length-prefixed JSON messages from stdin
let buffer = "";
process.stdin.on('data', (chunk) => {
    buffer += chunk;
    while (true) {
        const newlineIndex = buffer.indexOf('\n');
        if (newlineIndex === -1) {
            break; // No full length prefix yet
        }

        const lengthStr = buffer.substring(0, newlineIndex);
        const messageLength = parseInt(lengthStr, 10);

        if (isNaN(messageLength) || messageLength < 0) {
            console.error('PLAYWRIGHT_BACKEND_JS: Invalid message length received. Clearing buffer.');
            buffer = ""; // Clear corrupted buffer
            break;
        }

        if (buffer.length < newlineIndex + 1 + messageLength) {
            break; // Not enough data for the full message yet
        }

        const jsonMessage = buffer.substring(newlineIndex + 1, newlineIndex + 1 + messageLength);
        buffer = buffer.substring(newlineIndex + 1 + messageLength); // Remove processed message

        try {
            const parsedMessage = JSON.parse(jsonMessage);
            handleCommand(parsedMessage);
        } catch (e) {
            console.error('PLAYWRIGHT_BACKEND_JS: JSON parse error:', e.message, 'Message:', jsonMessage);
        }
    }
});

// Command handler
async function handleCommand(message) {
    const { type, command, id, params } = message;

    console.log(`PLAYWRIGHT_BACKEND_JS: Received ${type} command: ${command} (ID: ${id || 'N/A'})`);

    let result;
    let error;

    try {
        switch (command) {
            case "init":
                // Launch browser, create default page
                browser = await playwright.chromium.launch(); // Or .firefox.launch(), .webkit.launch()
                page = await browser.newPage();
                console.log('PLAYWRIGHT_BACKEND_JS: Browser and page initialized.');
                sendMessage('event', 'initialized'); // Inform C++ that page is ready
                break;

            case "shutdown":
                if (browser) {
                    await browser.close();
                    browser = null;
                    page = null;
                    console.log('PLAYWRIGHT_BACKEND_JS: Browser closed.');
                }
                process.exit(0);
                break;

            case "load":
                // params: { url, operation, body, headers }
                if (page) {
                    const response = await page.goto(params.url, { waitUntil: 'load' }); // 'load' waits for initial HTML
                    sendMessage('event', 'loadStarted', { url: params.url });
                    sendMessage('event', 'loadFinished', { success: !!response, url: params.url });
                    if (response) {
                        sendMessage('event', 'urlChanged', { url: page.url() });
                        sendMessage('event', 'titleChanged', { title: await page.title() });
                        sendMessage('event', 'contentsChanged', { html: await page.content(), plainText: await page.textContent('body') });
                    }
                }
                break;

            case "evaluateJs":
                // params: { script }
                if (page) {
                    result = await page.evaluate(params.script);
                }
                break;

            case "getHtml":
                if (page) {
                    result = await page.content();
                }
                break;

            case "getTitle":
                if (page) {
                    result = await page.title();
                }
                break;

            case "getUrl":
                if (page) {
                    result = page.url();
                }
                break;

            case "getPlainText":
                if (page) {
                    result = await page.textContent('body');
                }
                break;

            // --- Add more Playwright API mappings here as you implement IEngineBackend methods ---

            default:
                console.warn(`PLAYWRIGHT_BACKEND_JS: Unknown command: ${command}`);
                error = `Unknown command: ${command}`;
        }
    } catch (e) {
        console.error(`PLAYWRIGHT_BACKEND_JS: Error executing command ${command}:`, e);
        error = e.message;
    }

    if (type === "sync_command") {
        sendSyncResponse(id, { success: !error, result: result, error: error });
    }
}
