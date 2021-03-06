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
#include "sftpfilesystemmodel.h"

#include "sftpchannel.h"
#include "sshconnection.h"
#include "sshconnectionmanager.h"

#include <QFileInfo>
#include <QHash>
#include <QIcon>
#include <QList>
#include <QString>
#include <QRegularExpression>

namespace QSsh {
namespace Internal {
namespace {

typedef QHash<SftpJobId, SftpDirNode *> DirNodeHash;

SftpFileNode *indexToFileNode(const QModelIndex &index)
{
    return static_cast<SftpFileNode *>(index.internalPointer());
}

SftpDirNode *indexToDirNode(const QModelIndex &index)
{
    SftpFileNode * const fileNode = indexToFileNode(index);
    QSSH_ASSERT(fileNode);
    return dynamic_cast<SftpDirNode *>(fileNode);
}

} // anonymous namespace

class SftpFileSystemModelPrivate
{
public:
    SshConnection *sshConnection;
    SftpChannel::Ptr sftpChannel;
    QString rootDirectory;
    SftpFileNode *rootNode;
    SftpJobId statJobId;
    DirNodeHash lsOps;
    QList<SftpJobId> externalJobs;
};
} // namespace Internal

using namespace Internal;

SftpFileSystemModel::SftpFileSystemModel(QObject *parent)
    : QAbstractItemModel(parent), d(new SftpFileSystemModelPrivate)
{
    d->sshConnection = 0;
    d->rootDirectory = QLatin1String("/");
    d->rootNode = 0;
    d->statJobId = SftpInvalidJob;
}

SftpFileSystemModel::~SftpFileSystemModel()
{
    shutDown();
    delete d;
}

void SftpFileSystemModel::setSshConnection(const SshConnectionParameters &sshParams)
{
    QSSH_ASSERT_AND_RETURN(!d->sshConnection);
    d->sshConnection = SshConnectionManager::instance().acquireConnection(sshParams);
    connect(d->sshConnection, SIGNAL(error(QSsh::SshError)), SLOT(handleSshConnectionFailure()));
    if (d->sshConnection->state() == SshConnection::Connected) {
        handleSshConnectionEstablished();
        return;
    }
    connect(d->sshConnection, SIGNAL(connected()), SLOT(handleSshConnectionEstablished()));
    if (d->sshConnection->state() == SshConnection::Unconnected)
        d->sshConnection->connectToHost();
}

void SftpFileSystemModel::setRootDirectory(const QString &path)
{
    beginResetModel();
    d->rootDirectory = path;
    delete d->rootNode;
    d->rootNode = 0;
    d->lsOps.clear();
    d->statJobId = SftpInvalidJob;
    endResetModel();
    statRootDirectory();
}

QString SftpFileSystemModel::rootDirectory() const
{
    return d->rootDirectory;
}

SftpJobId SftpFileSystemModel::downloadFile(const QModelIndex &index, const QString &targetFilePath)
{
    QSSH_ASSERT_AND_RETURN_VALUE(d->rootNode, SftpInvalidJob);
    const SftpFileNode * const fileNode = indexToFileNode(index);
    QSSH_ASSERT_AND_RETURN_VALUE(fileNode, SftpInvalidJob);
    //QSSH_ASSERT_AND_RETURN_VALUE(fileNode->fileInfo.type == FileTypeRegular, SftpInvalidJob);
    const SftpJobId jobId = d->sftpChannel->downloadFile(fileNode->path, targetFilePath,
        SftpOverwriteExisting);
    if (jobId != SftpInvalidJob)
        d->externalJobs << jobId;
    return jobId;
}

SftpJobId SftpFileSystemModel::uploadFile(const QString &localFilePath, const QString &targetFilePath)
{
    QSSH_ASSERT_AND_RETURN_VALUE(d->rootNode, SftpInvalidJob);
    const SftpJobId jobId = d->sftpChannel->uploadFile(localFilePath, targetFilePath,
        SftpOverwriteExisting);
    if (jobId != SftpInvalidJob)
        d->externalJobs << jobId;
    return jobId;
}

SftpJobId SftpFileSystemModel::removeFile(const QModelIndex &index)
{
    QSSH_ASSERT_AND_RETURN_VALUE(d->rootNode, SftpInvalidJob);
    const SftpFileNode * const fileNode = indexToFileNode(index);
    QSSH_ASSERT_AND_RETURN_VALUE(fileNode, SftpInvalidJob);
    SftpJobId jobId = 0;
    //if (fileNode->fileInfo.type != FileTypeDirectory)
    //{
        jobId = d->sftpChannel->removeFile(fileNode->path);
    //}
    //else
    //{
        //jobId = d->sftpChannel->removeDirectory(fileNode->path);
    //}

    if (jobId != SftpInvalidJob)
        d->externalJobs << jobId;
    return jobId;
}

SftpJobId SftpFileSystemModel::downloadFile(const QModelIndex &index, QSharedPointer<QIODevice> localFile, quint32 size)
{
    QMutexLocker locker(&downloadMutex_);
    QSSH_ASSERT_AND_RETURN_VALUE(d->rootNode, SftpInvalidJob);
    const SftpFileNode * const fileNode = indexToFileNode(index);
    QSSH_ASSERT_AND_RETURN_VALUE(fileNode, SftpInvalidJob);
    //QSSH_ASSERT_AND_RETURN_VALUE(fileNode->fileInfo.type == FileTypeRegular, SftpInvalidJob);
    const SftpJobId jobId = d->sftpChannel->downloadFile(fileNode->path, localFile, size);
    if (jobId != SftpInvalidJob)
        d->externalJobs << jobId;
    return jobId;
}

int SftpFileSystemModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 2; // type + name
}

QVariant SftpFileSystemModel::data(const QModelIndex &index, int role) const
{
    const SftpFileNode * const node = indexToFileNode(index);

    switch (role) {
    case Qt::EditRole:
    case Qt::DisplayRole:
        if (!node)
            return "";
        switch (index.column()) {
        case 0: return node->fileInfo.name;
        case 1: return node->fileInfo.size;
        default:
            qWarning("data: invalid display value column %d", index.column());
            break;
        }
        break;
    case Qt::DecorationRole:
        if (index.column() == 0)
        {

            if (!node)
                return QVariant();
#if 0
            if (node->fileInfo.name.contains(".gz") || node->fileInfo.name.contains(".zip") ||
                    node->fileInfo.name.contains(".tar") ||
                    node->fileInfo.name.contains(".tgz"))
            {
                return QIcon(QLatin1String(":/core/compress.png"));
            }
            if (node->fileInfo.name.contains(".log"))
            {
                return QIcon(QLatin1String(":/core/common.ico"));
            }
            if (node->fileInfo.name.contains(".txt"))
            {
                return QIcon(QLatin1String(":/core/textfile.ico"));
            }
#endif
            switch (node->fileInfo.type) {
            //case FileTypeRegular:
            //case FileTypeOther:
            //    return QIcon(QLatin1String(":/core/unkown.ico"));
            case FileTypeDirectory:
                return QIcon(QLatin1String(":/core/folder.ico"));
            //case FileTypeUnknown:
            //    return QIcon(QLatin1String(":/core/unkown.ico")); // Shows a question mark.
            }
        }
        break;
    case Qt::TextAlignmentRole:
        if (index.column() == 1)
            return QVariant(Qt::AlignTrailing | Qt::AlignVCenter);
        break;
    }

    return QVariant();
}
void SftpFileSystemModel::setNameFilters(const QStringList &filters)
{
    nameFilters_ = filters;
    emit layoutChanged();
}

void SftpFileSystemModel::updateLayout()
{
    emit layoutChanged();
}

Qt::ItemFlags SftpFileSystemModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QAbstractItemModel::flags(index);
    if (!index.isValid())
        return flags;

    const SftpFileNode * const node = indexToFileNode(index);
    if (!passNameFilters(node)) {
        flags &= ~Qt::ItemIsEnabled;
        // ### TODO you shouldn't be able to set this as the current item, task 119433
        return flags;
    }

    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

bool SftpFileSystemModel::passNameFilters(const SftpFileNode* node) const
{
    if (nameFilters_.isEmpty())
        return true;

    // Check the name regularexpression filters
    if (node->fileInfo.type != FileTypeDirectory) {
        const QRegularExpression::PatternOptions options =
                QRegularExpression::CaseInsensitiveOption;

        for (const auto &nameFilter : nameFilters_) {
            QRegularExpression rx(QRegularExpression::wildcardToRegularExpression(nameFilter), options);
            QRegularExpressionMatch match = rx.match(node->fileInfo.name);
            if (match.hasMatch())
                return true;
        }
        return false;
    }

    return true;
}
QVariant SftpFileSystemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return QVariant();
    if (role != Qt::DisplayRole)
        return QVariant();
    if (section == 0)
        return tr("File Name");
    if (section == 1)
        return tr("File Size");
    return QVariant();
}

QModelIndex SftpFileSystemModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || row >= rowCount(parent) || column < 0 || column >= columnCount(parent))
        return QModelIndex();
    if (!d->rootNode)
        return QModelIndex();
    if (!parent.isValid())
        return createIndex(row, column, d->rootNode);
    const SftpDirNode * const parentNode = indexToDirNode(parent);
    QSSH_ASSERT_AND_RETURN_VALUE(parentNode, QModelIndex());
    QSSH_ASSERT_AND_RETURN_VALUE(row < parentNode->children.count(), QModelIndex());
    SftpFileNode * const childNode = parentNode->children.at(row);
    return createIndex(row, column, childNode);
}

QModelIndex SftpFileSystemModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) // Don't assert on this, since the model tester tries it.
        return QModelIndex();

    const SftpFileNode * const childNode = indexToFileNode(child);
    QSSH_ASSERT_AND_RETURN_VALUE(childNode, QModelIndex());
    if (childNode == d->rootNode)
        return QModelIndex();
    SftpDirNode * const parentNode = childNode->parent;
    if (parentNode == d->rootNode)
        return createIndex(0, 0, d->rootNode);
    const SftpDirNode * const grandParentNode = parentNode->parent;
    QSSH_ASSERT_AND_RETURN_VALUE(grandParentNode, QModelIndex());
    return createIndex(grandParentNode->children.indexOf(parentNode), 0, parentNode);
}

int SftpFileSystemModel::rowCount(const QModelIndex &parent) const
{
    if (!d->rootNode)
        return 0;
    if (!parent.isValid())
        return 1;
    if (parent.column() != 0)
        return 0;
    SftpDirNode * const dirNode = indexToDirNode(parent);
    if (!dirNode)
        return 0;
    if (dirNode->lsState != SftpDirNode::LsNotYetCalled)
        return dirNode->children.count();
    d->lsOps.insert(d->sftpChannel->listDirectory(dirNode->path), dirNode);
    dirNode->lsState = SftpDirNode::LsRunning;
    return 0;
}

void SftpFileSystemModel::statRootDirectory()
{
    d->statJobId = d->sftpChannel->statFile(d->rootDirectory);
}

void SftpFileSystemModel::shutDown()
{
    if (d->sftpChannel) {
        disconnect(d->sftpChannel.data(), 0, this, 0);
        d->sftpChannel->closeChannel();
        d->sftpChannel.clear();
    }
    if (d->sshConnection) {
        disconnect(d->sshConnection, 0, this, 0);
        SshConnectionManager::instance().releaseConnection(d->sshConnection);
        d->sshConnection = 0;
    }
    delete d->rootNode;
    d->rootNode = 0;
}

void SftpFileSystemModel::update(const QModelIndex &index)
{
    SftpDirNode* parent = nullptr;
    if (!index.isValid())
    {
        parent = dynamic_cast<SftpDirNode*>(d->rootNode);
    }
    else
    {
        SftpFileNode* fileNode = indexToFileNode(index);
        parent = fileNode->parent;
    }
    if (!parent)
    {
        parent = dynamic_cast<SftpDirNode*>(d->rootNode);
    }
    parent->lsState = SftpDirNode::LsNotYetCalled;
    //qDeleteAll(parent->children);
    parent->children.clear();
    d->lsOps.insert(d->sftpChannel->listDirectory(parent->path), parent);
    parent->lsState = SftpDirNode::LsRunning;
}

void SftpFileSystemModel::handleSshConnectionFailure()
{
    emit connectionError(d->sshConnection->errorString());
    beginResetModel();
    shutDown();
    endResetModel();
}

void SftpFileSystemModel::handleSftpChannelInitialized()
{
    connect(d->sftpChannel.data(),
        SIGNAL(fileInfoAvailable(QSsh::SftpJobId,QList<QSsh::SftpFileInfo>)),
        SLOT(handleFileInfo(QSsh::SftpJobId,QList<QSsh::SftpFileInfo>)));
    connect(d->sftpChannel.data(), SIGNAL(finished(QSsh::SftpJobId,QString)),
        SLOT(handleSftpJobFinished(QSsh::SftpJobId,QString)));
    statRootDirectory();
}

void SftpFileSystemModel::handleSshConnectionEstablished()
{
    d->sftpChannel = d->sshConnection->createSftpChannel();
    connect(d->sftpChannel.data(), SIGNAL(initialized()), SLOT(handleSftpChannelInitialized()));
    connect(d->sftpChannel.data(), SIGNAL(initializationFailed(QString)),
        SLOT(handleSftpChannelInitializationFailed(QString)));
    d->sftpChannel->initialize();
    connect(d->sftpChannel.data(), &SftpChannel::transferPrograss, this,
            [this](quint64 current, quint64 total){emit transferPrograss(current, total);});
    emit connectionSuccess();
}

void SftpFileSystemModel::handleSftpChannelInitializationFailed(const QString &reason)
{
    emit connectionError(reason);
    beginResetModel();
    shutDown();
    endResetModel();
}

void SftpFileSystemModel::handleFileInfo(SftpJobId jobId, const QList<SftpFileInfo> &fileInfoList)
{
    if (jobId == d->statJobId) {
        //QSSH_ASSERT_AND_RETURN(!d->rootNode);
        beginInsertRows(QModelIndex(), 0, 0);
        d->rootNode = new SftpDirNode;
        d->rootNode->path = d->rootDirectory;
        d->rootNode->fileInfo = fileInfoList.first();
        d->rootNode->fileInfo.name = d->rootDirectory == QLatin1String("/")
            ? d->rootDirectory : QFileInfo(d->rootDirectory).fileName();
        endInsertRows();
        return;
    }
    SftpDirNode * const parentNode = d->lsOps.value(jobId);
    //QSSH_ASSERT_AND_RETURN(parentNode);
    QList<SftpFileInfo> filteredList;
    foreach (const SftpFileInfo &fi, fileInfoList) {
        if (fi.name != QLatin1String(".") && fi.name != QLatin1String(".."))
            filteredList << fi;
    }
    if (filteredList.isEmpty())
        return;

    // In theory beginInsertRows() should suffice, but that fails to have an effect
    // if rowCount() returned 0 earlier.
    emit layoutAboutToBeChanged();

    foreach (const SftpFileInfo &fileInfo, filteredList) {
        SftpFileNode *childNode;
        if (fileInfo.type == FileTypeDirectory)
            childNode = new SftpDirNode;
        else
            childNode = new SftpFileNode;
        childNode->path = parentNode->path;
        if (!childNode->path.endsWith(QLatin1Char('/')))
            childNode->path += QLatin1Char('/');
        childNode->path += fileInfo.name;
        childNode->fileInfo = fileInfo;
        childNode->parent = parentNode;
        //parentNode->children << childNode;
        parentNode->insertChild(childNode);
    }
    //qSort(parentNode->children.begin(), parentNode->children.end(),[](SftpFileNode* lh, SftpFileNode* rh){
    //    return lh->fileInfo.name.toLower() < rh->fileInfo.name.toLower();});
    emit layoutChanged(); // Should be endInsertRows(), see above.
}

void SftpFileSystemModel::handleSftpJobFinished(SftpJobId jobId, const QString &errorMessage)
{
    if (jobId == d->statJobId) {
        d->statJobId = SftpInvalidJob;
        if (!errorMessage.isEmpty())
            emit sftpOperationFailed(tr("Error getting 'stat' info about '%1': %2")
                .arg(rootDirectory(), errorMessage));
        emit sftpOperationFinished(jobId, errorMessage);
        return;
    }

    DirNodeHash::Iterator it = d->lsOps.find(jobId);
    if (it != d->lsOps.end()) {
        QSSH_ASSERT(it.value()->lsState == SftpDirNode::LsRunning);
        it.value()->lsState = SftpDirNode::LsFinished;
        if (!errorMessage.isEmpty())
            emit sftpOperationFailed(tr("Error listing contents of directory '%1': %2")
                .arg(it.value()->path, errorMessage));
        d->lsOps.erase(it);
        emit sftpOperationFinished(jobId, errorMessage);
        return;
    }

    const int jobIndex = d->externalJobs.indexOf(jobId);
    QSSH_ASSERT_AND_RETURN(jobIndex != -1);
    d->externalJobs.removeAt(jobIndex);
    emit sftpOperationFinished(jobId, errorMessage);
}

} // namespace QSsh
