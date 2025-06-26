/*
  This file is part of the PhantomJS project from Ofi Labs.

  Copyright (C) 2011 Klarälvdalens Datakonsult AB, a KDAB Group company,
  info@kdab.com Author: Milian Wolff <milian.wolff@kdab.com>

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

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <QObject>
#include <QHostAddress>
#include <QPointer> // For safe pointers to other QObjects like WebPage

// Forward declarations to avoid MOC redefinition issues
class WebPage; // <-- NEW: Forward declare WebPage instead of including its header
class WebServerPrivate; // Assuming a pimpl pattern

class WebServer : public QObject {
    Q_OBJECT

public:
    explicit WebServer(QObject* parent = nullptr);
    ~WebServer();

    Q_PROPERTY(QHostAddress address READ address WRITE setAddress NOTIFY addressChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(bool listening READ listening NOTIFY listeningChanged)

    QHostAddress address() const;
    void setAddress(const QHostAddress& address);

    int port() const;
    void setPort(int port);

    bool listening() const;

    Q_INVOKABLE bool listen(int port = 8080, const QHostAddress& address = QHostAddress::Any);
    Q_INVOKABLE void close();

    Q_INVOKABLE void newHandler(const QString& url, QObject* callback);
    Q_INVOKABLE void removeHandler(const QString& url);
    Q_INVOKABLE void clearHandlers();

signals:
    void addressChanged(const QHostAddress& address);
    void portChanged(int port);
    void listeningChanged(bool listening);
    void requestReceived(QObject* request, QObject* response); // Might emit QObjects for JS interaction

private:
    WebServerPrivate* d_ptr;
    Q_DECLARE_PRIVATE(WebServer);
};

#endif // WEBSERVER_H
