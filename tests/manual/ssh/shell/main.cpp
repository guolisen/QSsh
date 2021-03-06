/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: http://www.qt-project.org/
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**************************************************************************/
#include "../remoteprocess/argumentscollector.h"
#include "shell.h"

#include <ssh/sshconnection.h>

#include <QCoreApplication>
#include <QObject>
#include <QStringList>

#include <cstdlib>
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    bool parseSuccess;
    //const QSsh::SshConnectionParameters &parameters
    //    = ArgumentsCollector(app.arguments()).collect(parseSuccess);
    //if (!parseSuccess)
     //   return EXIT_FAILURE;

    QSsh::SshConnectionParameters sshParams;
    sshParams.host = "192.168.0.101";
    sshParams.userName = "guolisen";
    sshParams.authenticationType = QSsh::SshConnectionParameters::AuthenticationByPassword;
    //sshParams.privateKeyFile = "C:/Users/qq/.ssh/id_rsa";

    sshParams.password = "12345678";
    sshParams.port = 22;
    sshParams.timeout = 1000;
    Shell shell(sshParams);
    shell.run();
    return app.exec();
}
