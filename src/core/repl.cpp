/*
  This file is part of the PhantomJS project from Ofi Labs.

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

#include "repl.h"

#include <QDebug>
#include <QDir>
#include <QFile> // For history file access
#include <QFileInfo>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSet>
#include <QTimer>
#include <algorithm> // For std::sort

#include "consts.h"
#include "phantom.h" // Needed for Phantom::instance() and signals
#include "terminal.h"
#include "utils.h"
#include "webpage.h" // Now directly including WebPage

#define PROMPT "phantomjs> "
#define HISTORY_FILENAME "phantom_repl_history"
#define REGEXP_NON_COMPLETABLE_CHARS "[^\\w\\s\\.]"
// JS for completions needs to interact with the _repl object on window, not internal C++ methods
#define JS_RETURN_POSSIBLE_COMPLETIONS "JSON.stringify(REPL._getCompletions(%1, \"%2\"));" // REPL.js will have _getCompletions
#define JS_EVAL_USER_INPUT                                                                  \
    "try { "                                                                                \
    "REPL._lastEval = eval(\"%1\");"                                                     \
    "console.log(JSON.stringify(REPL._lastEval, REPL._expResStringifyReplacer, '    ')); " \
    "} catch(e) { "                                                                         \
    "if (e instanceof TypeError) { "                                                    \
    "console.error(\"'%1' is a cyclic structure\"); "                               \
    "} else { "                                                                         \
    "console.error(e.message);"                                                     \
    "}"                                                                                  \
    "} "

// Static singleton instance
static REPL* s_replInstance = nullptr; // Renamed to avoid confusion with the method name

bool REPL::instanceExists() { return s_replInstance != nullptr; }

REPL* REPL::getInstance(WebPage* webpage, Phantom* parent)
{
    if (!s_replInstance && webpage && parent) {
        s_replInstance = new REPL(webpage, parent);
    }
    return s_replInstance;
}

REPL* REPL::getInstance()
{
    // This overload should only be called when the singleton is guaranteed to exist.
    // Otherwise, it would return nullptr, potentially leading to crashes.
    return s_replInstance;
}

QString REPL::_getClassName(QObject* obj) const { return QString::fromLatin1(obj->metaObject()->className()); }

QStringList REPL::_enumerateCompletions(QObject* obj) const
{
    const QMetaObject* meta = obj->metaObject();
    QSet<QString> completions;

    for (int i = meta->methodOffset(); i < meta->methodCount(); ++i) {
        QString name = QString::fromLatin1(meta->method(i).methodSignature());
        if (!name.startsWith('_')) { // Exclude private/internal methods
            int cutoff = name.indexOf('(');
            completions.insert(cutoff > 0 ? name.left(cutoff) : name);
        }
    }

    for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i) {
        QMetaProperty prop = meta->property(i);
        QString name = QString::fromLatin1(prop.name());
        if (prop.isScriptable() && !name.startsWith('_')) { // Exclude private properties
            completions.insert(name);
        }
    }

    QStringList keys = completions.values();
    std::sort(keys.begin(), keys.end());
    return keys;
}

// These are the actual slots exposed to JavaScript for _repl object
QString REPL::getClassName(QObject* obj) const {
    return _getClassName(obj);
}

QStringList REPL::enumerateCompletions(QObject* obj) const {
    return _enumerateCompletions(obj);
}

REPL::REPL(WebPage* webpage, Phantom* parent)
    : QObject(parent)
    , m_looping(true)
    , m_webpage(webpage) // Store WebPage* directly
    , m_parentPhantom(parent)
{
    m_historyFilepath = QString("%1/%2")
                            .arg(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation), HISTORY_FILENAME)
                            .toLocal8Bit();

    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));

    connect(m_parentPhantom, &Phantom::aboutToExit, this, &REPL::stopLoop);

    linenoiseSetCompletionCallback(REPL::offerCompletion);

    // This is crucial: the REPL JavaScript needs to be loaded into the page.
    // It will then be responsible for calling back to the C++ _repl object.
    m_webpage->evaluateJavaScript(Utils::readResourceFileUtf8(":/repl.js"));

    // Expose the REPL C++ object to the JavaScript context of the WebPage's backend.
    // This uses the new IEngineBackend abstraction.
    if (m_webpage && m_webpage->engineBackend()) {
        m_webpage->engineBackend()->exposeQObject("_repl", this);
    } else {
        qWarning() << "REPL: Cannot expose _repl object. WebPage or its backend is not ready.";
    }


    // Start the REPL loop after the event loop has started
    QTimer::singleShot(0, this, &REPL::startLoop);
}

void REPL::offerCompletion(const char* buf, linenoiseCompletions* lc)
{
    QString buffer(buf);
    QRegularExpression nonCompletableChars(REGEXP_NON_COMPLETABLE_CHARS);

    if (nonCompletableChars.match(buffer).hasMatch())
        return;

    int lastIndexOfDot = buffer.lastIndexOf('.');
    QString toInspect = (lastIndexOfDot > -1) ? buffer.left(lastIndexOfDot) : "window";
    QString toComplete = (lastIndexOfDot > -1) ? buffer.mid(lastIndexOfDot + 1) : buffer;

    // Call evaluateJavaScript on the current WebPage (which delegates to IEngineBackend)
    // The JS_RETURN_POSSIBLE_COMPLETIONS string now calls REPL._getCompletions in JS
    QStringList completions = REPL::getInstance()
                                  ->m_webpage->evaluateJavaScript(QString(JS_RETURN_POSSIBLE_COMPLETIONS).arg(toInspect, toComplete))
                                  .toStringList();

    for (const QString& c : completions) {
        QString suggestion = (lastIndexOfDot > -1) ? QString("%1.%2").arg(toInspect, c) : c;
        linenoiseAddCompletion(lc, suggestion.toLocal8Bit().data());
    }
}

void REPL::startLoop()
{
    char* userInput;
    linenoiseHistoryLoad(m_historyFilepath.toLocal8Bit().data()); // Use toLocal8Bit().data()
    while (m_looping && (userInput = linenoise(PROMPT)) != nullptr) {
        if (userInput[0] != '\0') {
            // Evaluate JavaScript via WebPage (which delegates to IEngineBackend)
            m_webpage->evaluateJavaScript(QString(JS_EVAL_USER_INPUT).arg(QString(userInput).replace('"', "\\\"")));
            linenoiseHistoryAdd(userInput);
            linenoiseHistorySave(m_historyFilepath.toLocal8Bit().data()); // Use toLocal8Bit().data()
        }
        free(userInput);
    }

    if (m_looping) {
        // If the loop exited but m_looping is still true, it means linenoise returned null
        // which typically implies EOF (Ctrl+D), so we exit the application.
        m_parentPhantom->exit();
    }
}

void REPL::stopLoop(const int code)
{
    Q_UNUSED(code);
    m_looping = false;
}
