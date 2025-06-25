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

#ifndef PHANTOM_H
#define PHANTOM_H

#include <QPointer> // Line 4: Required for QPointer<WebPage>

#include "childprocess.h" // Line 6: Existing include
#include "config.h"       // Line 7: Existing include
#include "cookiejar.h"    // Line 8: Existing include
#include "encoding.h"     // Line 9: Existing include
#include "filesystem.h"   // Line 10: Existing include
#include "system.h"       // Line 11: Existing include
#include "webpage.h"      // Line 12: Ensure WebPage is included for WebPage* declarations
// Line 13: Removed 'class CustomPage;' as it's no longer used.
class WebServer; // Line 14: Existing forward declaration

class Phantom : public QObject {
    Q_OBJECT // Line 17: Required for QObject functionality and Q_PROPERTY

    Q_PROPERTY(QVariantMap defaultPageSettings READ defaultPageSettings) // Line 19: Existing property
    Q_PROPERTY(QString libraryPath READ libraryPath WRITE setLibraryPath) // Line 20: Existing property
    Q_PROPERTY(QString outputEncoding READ outputEncoding WRITE setOutputEncoding) // Line 21: Existing property
    Q_PROPERTY(QVariantMap version READ version) // Line 22: Existing property
    Q_PROPERTY(QObject* page READ page) // Line 23: Existing property
    Q_PROPERTY(bool cookiesEnabled READ areCookiesEnabled WRITE setCookiesEnabled) // Line 24: Existing property
    Q_PROPERTY(QVariantList cookies READ cookies WRITE setCookies) // Line 25: Existing property
    Q_PROPERTY(int remoteDebugPort READ remoteDebugPort) // Line 26: Existing property

private:
    // Private constructor: the Phantom class is a singleton
    // Line 29: Existing constructor declaration
    Phantom(QObject* parent = 0);
    // Line 30: Existing init method
    void init();

public:
    // Line 33: Existing static instance method
    static Phantom* instance();
    // Line 34: Existing virtual destructor
    virtual ~Phantom();

    // Line 36: Existing defaultPageSettings getter
    QVariantMap defaultPageSettings() const;

    // Line 38: Existing outputEncoding getter
    QString outputEncoding() const;
    // Line 39: Existing setOutputEncoding setter
    void setOutputEncoding(const QString& encoding);

    // Line 41: Existing execute method
    bool execute();
    // Line 42: Existing returnValue getter
    int returnValue() const;

    // Line 44: Existing libraryPath getter
    QString libraryPath() const;
    // Line 45: Existing setLibraryPath setter
    void setLibraryPath(const QString& libraryPath);

    // Line 47: Existing version getter
    QVariantMap version() const;

    // Line 49: Existing page getter
    QObject* page() const;

    /**
     * Pointer to the Config loaded at startup.
     * The configuration is determined by the commandline parameters.
     *
     * @brief config
     * @return Pointer to the current Config(uration)
     */
    // Line 58: Existing config getter
    Config* config();

    // Line 60: Existing printDebugMessages getter
    bool printDebugMessages() const;

    // Line 62: Existing areCookiesEnabled getter
    bool areCookiesEnabled() const;
    // Line 63: Existing setCookiesEnabled setter
    void setCookiesEnabled(const bool value);

    // Line 65: Existing remoteDebugPort getter
    int remoteDebugPort() const;

    /**
     * Create `child_process` module instance
     */
    // Line 70: Existing _createChildProcess invokable method
    Q_INVOKABLE QObject* _createChildProcess();

    // Line 72: New method to retrieve all managed WebPages (for WebPage::pages())
    const QList<QPointer<WebPage>>& allPages() const;

public slots:
    // Line 75: Existing createCookieJar slot
    QObject* createCookieJar(const QString& filePath);
    // Line 76: Existing createWebPage slot
    QObject* createWebPage();
    // Line 77: Existing createWebServer slot
    QObject* createWebServer();
    // Line 78: Existing createFilesystem slot
    QObject* createFilesystem();
    // Line 79: Existing createSystem slot
    QObject* createSystem();
    // Line 80: Existing createCallback slot
    QObject* createCallback();
    // Line 81: Existing loadModule slot
    void loadModule(const QString& moduleSource, const QString& filename);
    // Line 82: Existing injectJs slot
    bool injectJs(const QString& jsFilePath);

    /**
     * Allows to set cookies into the CookieJar.
     * Pages will be able to access only the cookies they are supposed to see
     * given their URL.
     *
     * Cookies are expected in the format:
     * <pre>
     * {
     * "name"      : "cookie name (string)",
     * "value"     : "cookie value (string)",
     * "domain"    : "cookie domain (string)",
     * "path"      : "cookie path (string, optional)",
     * "httponly" : "http only cookie (boolean, optional)",
     * "secure"    : "secure cookie (boolean, optional)",
     * "expires"   : "expiration date (string, GMT format, optional)"
     * }
     * </pre>
     * @brief setCookies
     * @param cookies Expects a QList of QVariantMaps
     * @return Boolean "true" if at least 1 cookie was set
     */
    // Line 102: Existing setCookies slot
    bool setCookies(const QVariantList& cookies);
    /**
     * All the Cookies in the CookieJar
     *
     * @see WebPage::setCookies for details on the format
     * @brief cookies
     * @return QList of QVariantMap cookies visible to this Page, at the current
     * URL.
     */
    // Line 111: Existing cookies getter
    QVariantList cookies() const;
    /**
     * Add a Cookie (in QVariantMap format) into the CookieJar
     * @see WebPage::setCookies for details on the format
     * @brief addCookie
     * @param cookie Cookie in QVariantMap format
     * @return Boolean "true" if cookie was added
     */
    // Line 120: Existing addCookie slot
    bool addCookie(const QVariantMap& cookie);
    /**
     * Delete cookie by name from the CookieJar
     * @brief deleteCookie
     * @param cookieName Name of the Cookie to delete
     * @return Boolean "true" if cookie was deleted
     */
    // Line 127: Existing deleteCookie slot
    bool deleteCookie(const QString& cookieName);
    /**
     * Delete All Cookies from the CookieJar
     * @brief clearCookies
     */
    // Line 132: Existing clearCookies slot
    void clearCookies();

    /**
     * Set the application proxy
     * @brief setProxy
     * @param ip The proxy ip
     * @param port The proxy port
     * @param proxyType The type of this proxy
     */
    // Line 140: Existing setProxy slot
    void setProxy(const QString& ip, const qint64& port = 80, const QString& proxyType = "http",
                  const QString& user = QString(), const QString& password = QString());

    // Line 142: Existing proxy getter
    QString proxy();

    // exit() will not exit in debug mode. debugExit() will always exit.
    // Line 145: Existing exit slot
    void exit(int code = 0);
    // Line 146: Existing debugExit slot
    void debugExit(int code = 0);

    // URL utilities

    /**
     * Resolve a URL relative to a base.
     */
    // Line 152: Existing resolveRelativeUrl method
    QString resolveRelativeUrl(QString url, QString base);

    /**
     * Decode a URL to human-readable form.
     * @param url The URL to be decoded.
     *
     * This operation potentially destroys information.  It should only be
     * used when displaying URLs to the user, not when recording URLs for
     * later processing.  Quoting http://qt-project.org/doc/qt-5/qurl.html:
     *
     * _Full decoding_
     *
     * This [operation] should be used with care, since there are
     * two conditions that cannot be reliably represented in the
     * returned QString. They are:
     *
     * + Non-UTF-8 sequences: URLs may contain sequences of
     * percent-encoded characters that do not form valid UTF-8
     * sequences. Since URLs need to be decoded using UTF-8, any
     * decoder failure will result in the QString containing one or
     * more replacement characters where the sequence existed.
     *
     * + Encoded delimiters: URLs are also allowed to make a
     * distinction between a delimiter found in its literal form and
     * its equivalent in percent-encoded form. This is most commonly
     * found in the query, but is permitted in most parts of the URL.
     */
    // Line 180: Existing fullyDecodeUrl method
    QString fullyDecodeUrl(QString url);

signals:
    // Line 183: Existing aboutToExit signal
    void aboutToExit(int code);

private slots:
    // Line 186: Existing printConsoleMessage slot
    void printConsoleMessage(const QString& msg);

    // Line 188: Existing onInitialized slot
    void onInitialized();

private:
    // Line 191: Existing doExit method
    void doExit(int code);

    // Line 193: Existing m_scriptFileEnc member
    Encoding m_scriptFileEnc;
    // Line 194: Existing m_page member
    WebPage* m_page;
    // Line 195: Existing m_terminated member
    bool m_terminated;
    // Line 196: Existing m_returnValue member
    int m_returnValue;
    // Line 197: Existing m_script member
    QString m_script;
    // Line 198: Existing m_defaultPageSettings member
    QVariantMap m_defaultPageSettings;
    // Line 199: Existing m_filesystem member
    FileSystem* m_filesystem;
    // Line 200: Existing m_system member
    System* m_system;
    // Line 201: Existing m_childprocess member
    ChildProcess* m_childprocess;
    // Line 202: Existing m_pages member
    QList<QPointer<WebPage>> m_pages;
    // Line 203: Existing m_servers member
    QList<QPointer<WebServer>> m_servers;
    // Line 204: Existing m_config member
    Config m_config;
    // Line 205: Existing m_defaultCookieJar member
    CookieJar* m_defaultCookieJar;
    // Line 206: Existing m_defaultDpi member
    qreal m_defaultDpi;

    // Line 208: Removed 'friend class CustomPage;' as CustomPage is no longer used.
};

#endif // PHANTOM_H
