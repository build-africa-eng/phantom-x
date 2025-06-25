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

#ifndef UTILS_H
#define UTILS_H

#include "encoding.h"
#include <QtGlobal>
#include <QString> // Added for QString
#include <QMessageLogContext> // Added for QMessageLogContext

// Forward declaration for WebPage
class WebPage;

/**
 * @brief Aggregate common utility functions.
 */
namespace Utils {

/**
 * @brief Custom message handler for Qt messages (qDebug, qWarning, qCritical, qFatal).
 * @param type The type of message.
 * @param context The context of the log message.
 * @param msg The message string.
 */
void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg);
extern bool printDebugMessages;

/**
 * @brief Injects JavaScript code from a file into the target web page.
 * @param jsFilePath The path to the JavaScript file.
 * @param jsFileEnc The encoding of the JavaScript file.
 * @param libraryPath The base path for resolving relative script paths.
 * @param targetPage The WebPage object into which to inject the script.
 * @param startingScript True if this is the main starting script of the application.
 * @return True if successful, false otherwise.
 */
bool injectJsInFrame(const QString& jsFilePath, const Encoding& jsFileEnc, const QString& libraryPath,
                     WebPage* targetPage, const bool startingScript = false);

/**
 * @brief Loads JavaScript for debugging purposes, optionally wrapping it in a __run() function.
 * @param jsFilePath The path to the JavaScript file.
 * @param jsFileEnc The encoding of the JavaScript file.
 * @param libraryPath The base path for resolving relative script paths.
 * @param targetPage The WebPage object into which to load the script.
 * @param autorun If true, calls __run() immediately after defining it.
 * @return True if successful, false otherwise.
 */
bool loadJSForDebug(const QString& jsFilePath, const Encoding& jsFileEnc, const QString& libraryPath,
                    WebPage* targetPage, const bool autorun = false);

/**
 * @brief Reads content from a Qt resource file.
 * @param resourceFilePath The path to the resource file (e.g., ":/bootstrap.js").
 * @return The content of the resource file as a QString (UTF-8 decoded).
 */
QString readResourceFileUtf8(const QString& resourceFilePath);
}; // namespace Utils

#endif // UTILS_H
