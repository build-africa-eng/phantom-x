/*
  This file is part of the PhantomJS project from Ofi Labs.

  Copyright (C) 2011-2025 Ivan De Marino <ivan.de.marino@gmail.com>

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

#ifndef REPL_H
#define REPL_H

#include <QObject>
#include <QVariant>
#include <QStringList>
#include <QPointer> // For safe pointer to WebPage

// Forward declarations
class Phantom;
class WebPage; // Now directly referencing WebPage, not QWebFrame

// linenoise library declarations (assuming it's a C library)
extern "C" {
    struct linenoiseCompletions;
    typedef void(linenoiseCompletionCallbackFn)(const char* /*buf*/, linenoiseCompletions* /*lc*/);
    void linenoiseSetCompletionCallback(linenoiseCompletionCallbackFn*);
    void linenoiseAddCompletion(linenoiseCompletions*, const char* /*str*/);
    char* linenoise(const char* /*prompt*/);
    void linenoiseHistoryLoad(const char* /*filename*/);
    void linenoiseHistorySave(const char* /*filename*/);
    void linenoiseHistoryAdd(const char* /*line*/);
    void linenoiseFree(void* /*ptr*/);
}

/**
 * @brief REPL (Read-Eval-Print-Loop) provides an interactive console for PhantomJS.
 * It allows users to type JavaScript commands and see the results directly.
 */
class REPL : public QObject
{
    Q_OBJECT

    // Q_PROPERTY(QVariant returnValue READ returnValue WRITE setReturnValue) // Not directly used in REPL itself

public:
    // Factory method to get the singleton instance
    // Now takes WebPage* instead of QWebFrame*
    static REPL* getInstance(WebPage* webpage = nullptr, Phantom* parent = nullptr);
    static REPL* getInstance(); // Overload for internal use when singleton is guaranteed

    static bool instanceExists(); // Checks if the singleton has been initialized

private:
    // Private constructor for singleton pattern
    REPL(WebPage* webpage, Phantom* parent);
    // Helper methods for JavaScript completions, moved to private as they are internal to REPL
    QString _getClassName(QObject* obj) const;
    QStringList _enumerateCompletions(QObject* obj) const;


public slots: // Exposed to JavaScript via _repl object
    // These methods are called by the repl.js script in the browser context
    // to get class names and enumerate properties for tab completion.
    QString getClassName(QObject* obj) const;
    QStringList enumerateCompletions(QObject* obj) const;

signals:
    // No direct signals from REPL to be public for now

private slots:
    void startLoop(); // Starts the main REPL loop
    void stopLoop(int code); // Stops the REPL loop on exit signal from Phantom

private:
    static void offerCompletion(const char* buf, linenoiseCompletions* lc); // Static callback for linenoise

    bool m_looping;
    QPointer<WebPage> m_webpage; // Changed from QWebFrame* to WebPage*
    QPointer<Phantom> m_parentPhantom; // QPointer for safe access
    QString m_historyFilepath;
};

#endif // REPL_H
