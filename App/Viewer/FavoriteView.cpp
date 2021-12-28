#include "FavoriteView.h"

#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QDebug>
#include <QApplication>
#include <QHeaderView>
#include <QSettings>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QSettings>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMessageBox>

#include "FavoriteMimeData.h"
#include "RabbitCommonLog.h"
#include "RabbitCommonDir.h"

CFavoriteView::CFavoriteView(QWidget *parent) : QTreeView(),
    m_pModel(nullptr)
{
    setFocusPolicy(Qt::NoFocus);
    header()->hide();
    setEditTriggers(QTreeView::NoEditTriggers);
    setAcceptDrops(true);
    
    m_pModel = new QStandardItemModel(this);
    setModel(m_pModel);
    
    setContextMenuPolicy(Qt::CustomContextMenu);
    bool check = connect(this,
                         SIGNAL(customContextMenuRequested(const QPoint &)),
                         this, SLOT(slotCustomContextMenu(const QPoint &)));
    Q_ASSERT(check);
    check = connect(this, SIGNAL(clicked(const QModelIndex&)),
                    this, SLOT(slotFavrtieClicked(const QModelIndex&)));
    Q_ASSERT(check);
    check = connect(this, SIGNAL(doubleClicked(const QModelIndex&)),
                    this, SLOT(slotFavortiedoubleClicked(const QModelIndex&)));
    Q_ASSERT(check);
    
    m_szSaveFile = RabbitCommon::CDir::Instance()->GetDirUserData()
            + QDir::separator() + "Favorite.ini";
    QSettings set(m_szSaveFile, QSettings::IniFormat);
    int nRootCount = 0;
    nRootCount = set.value("RootCount").toInt();
    for(int i = 0; i < nRootCount; i++)
    {
        QString name = set.value("Name_" + QString::number(i)).toString();
        QString file = set.value("File_" + QString::number(i)).toString();
        QStandardItem* item = new QStandardItem(name);
        if(item)
        {
            item->setData(file);
            m_pModel->appendRow(item);
        }
    }
    int nGroupCount = set.value("GroupCount").toInt();
    for(int g = 0; g < nGroupCount; g++)
    {
        QString szGroup = set.value("Group_" + QString::number(g)).toString();
        QStandardItem* pGroup = new QStandardItem(szGroup);
        m_pModel->appendRow(pGroup);
        int n = set.value(szGroup + "/" + "count").toInt();
        for(int i = 0; i < n; i++)
        {
            QString name = set.value(szGroup + "/Name_" + QString::number(i)).toString();
            QString file = set.value(szGroup + "/File_" + QString::number(i)).toString();
            QStandardItem* item = new QStandardItem(name);
            if(item)
            {
                item->setData(file);
                pGroup->appendRow(item);
            }
        }
    }
}

CFavoriteView::~CFavoriteView()
{
    Save();
}

int CFavoriteView::Save()
{
    int nRet = 0;
    if(!m_pModel)
        return -1;
    QSettings set(m_szSaveFile, QSettings::IniFormat);
    int nRootCount = 0;
    int nGroup = 0;
    for(int rootIndex = 0; rootIndex < m_pModel->rowCount(); rootIndex++)
    {
        auto rootItem = m_pModel->item(rootIndex);
        if(!rootItem) continue;
        if(rootItem->hasChildren())
        {
            int nCount = 0;
            QString szGroup = rootItem->text();
            set.setValue("Group_" + QString::number(nGroup++), szGroup);
            for(int childIndex = 0; childIndex < rootItem->rowCount(); childIndex++)
            {
                auto childItem = rootItem->child(childIndex);
                if(!childItem) continue;
                set.setValue(szGroup + "/Name_" + QString::number(nCount), childItem->text());
                set.setValue(szGroup + "/File_" + QString::number(nCount), childItem->data());
                nCount++;
            }
            set.setValue(szGroup + "/" + "count", nCount);
        } else {
            set.setValue("Name_" + QString::number(nRootCount), rootItem->text());
            set.setValue("File_" + QString::number(nRootCount), rootItem->data());
            nRootCount++;
        }
    }
    set.setValue("RootCount", nRootCount);
    set.setValue("GroupCount", nGroup);
    return nRet;
}

int CFavoriteView::AddFavorite(const QString& szName, const QString &szFile, QString szGroup)
{
    if(!m_pModel) return -1;
    QStandardItem* pItem = nullptr;
    if(szGroup.isEmpty())
    {
        auto it = m_pModel->findItems(szName, Qt::MatchFixedString);
        if(it.isEmpty())
        {
            pItem = new QStandardItem(szName);
            m_pModel->appendRow(pItem);
        } else {
            QList<QStandardItem*>::iterator i;
            for(i = it.begin(); i != it.end(); i++)
            {
                if((*i)->data() == szFile)
                    break;
            }
            if(it.end() != i)
                return 0;
            pItem = new QStandardItem(szName);
            m_pModel->appendRow(pItem);
        }
    } else {
        auto lstGroup = m_pModel->findItems(szGroup, Qt::MatchFixedString);
        if(lstGroup.isEmpty())
        {
            QStandardItem* pGroup = new QStandardItem(szGroup);
            m_pModel->appendRow(pGroup);
            pItem = new QStandardItem(szName);
            pGroup->appendRow(pItem);
        } else {
            for(int i = 0; i < lstGroup[0]->rowCount(); i++)
            {
                auto item = lstGroup[0]->child(i);
                if(item->text() == szName && item->data() == szFile)
                    return 0;
            }
            pItem = new QStandardItem(szName);
            lstGroup[0]->appendRow(pItem);
        }
    }
    if(pItem)
        pItem->setData(szFile);
    return 0;
}

void CFavoriteView::slotFavrtieClicked(const QModelIndex &index)
{
}

void CFavoriteView::slotFavortiedoubleClicked(const QModelIndex &index)
{
    auto item = m_pModel->itemFromIndex(index);
    if(!item) return;
    QString szFile = item->data().toString();
    if(!szFile.isEmpty())
        emit sigConnect(szFile, false);
}

void CFavoriteView::slotCustomContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    menu.addAction(tr("Connect"), this, SLOT(slotConnect()));
    menu.addAction(tr("Open settings and connect"), this, SLOT(slotOpenConnect()));
    menu.addSeparator();
    menu.addAction(tr("New group"), this, SLOT(slotNewGroup()));
    menu.addAction(tr("Delete"), this, SLOT(slotDelete()));
    menu.exec(mapToGlobal(pos));
}

void CFavoriteView::slotConnect()
{
    auto lstIndex = selectionModel()->selectedIndexes();
    foreach(auto index, lstIndex)
    {
        slotFavortiedoubleClicked(index);
    }
}

void CFavoriteView::slotOpenConnect()
{
    auto lstIndex = selectionModel()->selectedIndexes();
    foreach(auto index, lstIndex)
    {
        auto item = m_pModel->itemFromIndex(index);
        QString szFile = item->data().toString();
        if(!szFile.isEmpty())
            emit sigConnect(szFile, true);
    }
}

void CFavoriteView::slotDelete()
{
    auto lstIndex = selectionModel()->selectedIndexes();
    foreach(auto index, lstIndex)
        m_pModel->removeRow(index.row());
}

void CFavoriteView::slotNewGroup()
{
    QString szGroup = QInputDialog::getText(this, tr("Input"), tr("Input group name"));
    if(szGroup.isEmpty()) return;
    auto lstItem = m_pModel->findItems(szGroup);
    if(!lstItem.isEmpty())
    {
        QMessageBox::critical(this, tr("Error"), tr("The group [%1] is existed").arg(szGroup));
        return;
    }

    QStandardItem* pItem = new QStandardItem(szGroup);
    m_pModel->appendRow(pItem);
}

void CFavoriteView::dragEnterEvent(QDragEnterEvent *event)
{
    LOG_MODEL_DEBUG("CFavoriteView", "dragEnterEvent");
    const CFavoriteMimeData* pData = qobject_cast<const CFavoriteMimeData*>(event->mimeData());
    if (pData)
    {
        LOG_MODEL_DEBUG("CFavoriteView", "dragEnterEvent acceptProposedAction");
        event->acceptProposedAction();
    }
}

void CFavoriteView::dragMoveEvent(QDragMoveEvent *event)
{
}

void CFavoriteView::dropEvent(QDropEvent *event)
{
    LOG_MODEL_DEBUG("CFavoriteView", "dropEvent");
    const CFavoriteMimeData *pData = qobject_cast<const CFavoriteMimeData*>(event->mimeData());
    if(!pData) return;
    QStandardItemModel* pModel = dynamic_cast<QStandardItemModel*>(model());
    if(!pModel) return;
    auto index = indexAt(event->pos());
    if(index.isValid())
    {
        auto item = pModel->itemFromIndex(index);
        foreach(auto index, pData->m_Items)
        {
            LOG_MODEL_DEBUG("CFavoriteView", "dropEvent: %s",
                            item->text().toStdString().c_str());
            if(item->data().isNull())
            {
                auto newItem = NewItem(index);
                item->appendRow(newItem);
                if(event->dropAction() == Qt::MoveAction)
                    pModel->removeRow(index.row(), index.parent());
            } else
                LOG_MODEL_WARNING("CFavoriteView", "Don't group node. the data:%s",
                                  item->data().toString().toStdString().c_str());
        }
    }else{
        foreach(auto index, pData->m_Items)
        {
            pModel->appendRow(NewItem(index));
            if(event->dropAction() == Qt::MoveAction)
                pModel->removeRow(index.row(), index.parent());
        }
    }

    event->accept();
}

QStandardItem* CFavoriteView::NewItem(const QModelIndex &index)
{
    QStandardItemModel* pModel = dynamic_cast<QStandardItemModel*>(model());
    if(!pModel) return nullptr;
    auto item = pModel->itemFromIndex(index);
    if(!item) return nullptr;
    auto ri = new QStandardItem(item->text());
    ri->setData(item->data());
    return ri;
}

void CFavoriteView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_DragStartPosition = event->pos();
    QTreeView::mousePressEvent(event);
}

void CFavoriteView::mouseMoveEvent(QMouseEvent *event)
{
    LOG_MODEL_DEBUG("CFavoriteView", "mouseMoveEvent");
    do{
        if (!(event->buttons() & Qt::LeftButton))
            break;
        if ((event->pos() - m_DragStartPosition).manhattanLength()
                < QApplication::startDragDistance())
            break;
        LOG_MODEL_DEBUG("CFavoriteView", "mouseMoveEvent drag");
        QDrag *drag = new QDrag(this);
        CFavoriteMimeData *pData = new CFavoriteMimeData();
        pData->m_Items = this->selectionModel()->selectedIndexes();
        drag->setMimeData(pData);
        
        Qt::DropAction dropAction = Qt::MoveAction;
        if(event->modifiers() & Qt::ControlModifier)
            dropAction = Qt::CopyAction;
        drag->exec(dropAction);
    } while (false);
    
    QTreeView::mouseMoveEvent(event);
}
