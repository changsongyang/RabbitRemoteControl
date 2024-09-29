// Author: Kang Lin <kl222@126.com>

#ifndef CCONNECTERLIBVNC_H
#define CCONNECTERLIBVNC_H

#pragma once

#include "ConnecterThread.h"
#include "ParameterLibVNCServer.h"

class CConnecterLibVNCServer : public CConnecterThread
{
    Q_OBJECT
public:
    explicit CConnecterLibVNCServer(CPluginClient *parent);
    virtual ~CConnecterLibVNCServer() override;

public:
    virtual qint16 Version() override;

protected:
    virtual QDialog *OnOpenDialogSettings(QWidget *parent) override;
    virtual CConnect *InstanceConnect() override;
    
private:
    CParameterLibVNCServer m_Para;
};

#endif // CCONNECTERLIBVNC_H
