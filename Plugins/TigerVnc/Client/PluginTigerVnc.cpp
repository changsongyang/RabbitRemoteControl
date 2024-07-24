// Author: Kang Lin <kl222@126.com>

#include "PluginTigerVnc.h"
#include "ConnecterVnc.h"
#include <QLoggingCategory>
#include "RabbitCommonDir.h"
#include <QScopedPointer>

static Q_LOGGING_CATEGORY(log, "VNC.Plugin.Tiger")

CPluginTigerVnc::CPluginTigerVnc(QObject *parent)
    : CPluginClient(parent)
{
    qDebug(log) << "CPluginTigerVnc::CPluginTigerVnc()";
}

CPluginTigerVnc::~CPluginTigerVnc()
{
    qDebug(log) << "CPluginTigerVnc::~CPluginTigerVnc()";
}

const QString CPluginTigerVnc::Name() const
{
    return "TigerVNC";
}

const QString CPluginTigerVnc::DisplayName() const
{
    return tr("TigerVNC");
}

const QString CPluginTigerVnc::Description() const
{
    return tr("VNC(Virtual Network Console): Access remote desktops such as unix/linux, windows, etc.") + "\n"
            + tr("VNC is a set of programs using the RFB (Remote Frame Buffer) protocol.");
}

const QString CPluginTigerVnc::Protocol() const
{
    return "RFB";
}

CConnecter *CPluginTigerVnc::CreateConnecter(const QString &szID)
{
    if(Id() == szID)
    {
        return new CConnecterVnc(this);
    }
    return nullptr;
}

const QIcon CPluginTigerVnc::Icon() const
{
    return QIcon::fromTheme("network-wired");
}
