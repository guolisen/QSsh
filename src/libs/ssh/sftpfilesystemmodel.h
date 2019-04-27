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
#ifndef SFTPFILESYSTEMMODEL_H
#define SFTPFILESYSTEMMODEL_H

#include "sftpdefs.h"

#include "ssh_global.h"

#include <QAbstractItemModel>
#include <QMutex>
#include <QDebug>
namespace QSsh {
class SshConnectionParameters;

namespace Internal { class SftpFileSystemModelPrivate; }

class SftpDirNode;
class SftpFileNode
{
public:
    SftpFileNode() : parent(0) { }
    virtual ~SftpFileNode() { }


    QString path;
    SftpFileInfo fileInfo;
    SftpDirNode *parent;
};

class SftpDirNode : public SftpFileNode
{
public:
    SftpDirNode() : lsState(LsNotYetCalled) { }
    ~SftpDirNode() { qDeleteAll(children); }

    void insertChild(SftpFileNode* newFileNode)
    {
        if (children.contains(newFileNode))
            return;
        if (children.isEmpty())
        {
            children << newFileNode;
            return;
        }

        int i = 0;
        auto iter = children.begin();
        for (; iter != children.end(); ++iter, ++i)
        {
            if (newFileNode->fileInfo.name.compare((*iter)->fileInfo.name, Qt::CaseInsensitive) > 0)
            {
                continue;
            }
            else
            {
                children.insert(i, newFileNode);
                return;
            }
        }
        if (iter == children.end())
            children << newFileNode;
    }
    enum { LsNotYetCalled, LsRunning, LsFinished } lsState;
    QList<SftpFileNode *> children;
};

// Very simple read-only model. Symbolic links are not followed.
class QSSH_EXPORT SftpFileSystemModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit SftpFileSystemModel(QObject *parent = 0);
    ~SftpFileSystemModel();

    /*
     * Once this is called, an SFTP connection is established and the model is populated.
     * The effect of additional calls is undefined.
     */
    void setSshConnection(const SshConnectionParameters &sshParams);

    void setRootDirectory(const QString &path); // Default is "/".
    QString rootDirectory() const;

    SftpJobId downloadFile(const QModelIndex &index, const QString &targetFilePath);

    // Use this to get the full path of a file or directory.
    static const int PathRole = Qt::UserRole;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    SftpJobId downloadFile(const QModelIndex &index, QSharedPointer<QIODevice> localFile, quint32 size);
    void shutDown();
    void update(const QModelIndex &index);
    void setNameFilters(const QStringList &filters);
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
signals:
     /*
      * E.g. "Permission denied". Note that this can happen without direct user intervention,
      * due to e.g. the view calling rowCount() on a non-readable directory. This signal should
      * therefore not result in a message box or similar, since it might occur very often.
      */
    void sftpOperationFailed(const QString &errorMessage);

    /*
     * This error is not recoverable. The model will not have any content after
     * the signal has been emitted.
     */
    void connectionError(const QString &errorMessage);
    void connectionSuccess();
    void downloadPrograss(quint64 currentSize, quint64 totleSize);
    // Success <=> error.isEmpty().
    void sftpOperationFinished(QSsh::SftpJobId, const QString &error);

private slots:
    void handleSshConnectionEstablished();
    void handleSshConnectionFailure();
    void handleSftpChannelInitialized();
    void handleSftpChannelInitializationFailed(const QString &reason);
    void handleFileInfo(QSsh::SftpJobId jobId, const QList<QSsh::SftpFileInfo> &fileInfoList);
    void handleSftpJobFinished(QSsh::SftpJobId jobId, const QString &errorMessage);

protected:
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    QModelIndex parent(const QModelIndex &child) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    bool passNameFilters(const SftpFileNode *node) const;
    void statRootDirectory();
    QMutex downloadMutex_;
    Internal::SftpFileSystemModelPrivate * const d;
    QStringList nameFilters_;
};

} // namespace QSsh;

#endif // SFTPFILESYSTEMMODEL_H
