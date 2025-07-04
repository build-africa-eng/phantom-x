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

#ifndef TERMINAL_H
#define TERMINAL_H

#include <QObject>
#include <QString>
#include <ostream> // Still needed for std::ostream in output method

#include "encoding.h" // Keep this for m_encoding

class Terminal : public QObject {
    Q_OBJECT

public:
    static Terminal* instance();

    QString getEncoding() const;
    bool setEncoding(const QString& encoding);

    void cout(const QString& string, const bool newline = true) const;
    void cerr(const QString& string, const bool newline = true) const;

    // --- NEW: For debug mode control ---
    void setDebugMode(bool debug);
    bool debugMode() const; // New getter for debug state
    // -----------------------------------

signals:
    void encodingChanged(const QString& encoding);

private:
    // Private constructor for singleton pattern
    Terminal();

    // Private helper method for output
    void output(std::ostream& out, const QString& string, const bool newline) const;

private:
    Encoding m_encoding;
    bool m_debugMode; // NEW: Member to store debug state
};

#endif // TERMINAL_H
