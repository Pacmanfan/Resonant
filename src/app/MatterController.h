// The application side of functionally architected matter (REQ-CHAT-20261127-001, RES-FAM-019):
// the Matter panel's access to MatterService through the GUI's own session. No physics here.
#pragma once

#include "foundation/Result.h"

#include <QObject>
#include <QString>

#include <nlohmann/json.hpp>

class DocumentController;

class MatterController : public QObject {
    Q_OBJECT
public:
    explicit MatterController(DocumentController* document, QObject* parent = nullptr);

    nlohmann::json architectures() const;
    nlohmann::json architecture(const QString& id) const;
    resonant::Result<nlohmann::json> analyze(const QString& architecture, const QString& stack, const QString& analysis, const nlohmann::json& options) const;
    resonant::Result<nlohmann::json> manufacturability(const QString& architecture) const;
    resonant::Result<nlohmann::json> explore(const QString& architecture) const;
    resonant::Result<nlohmann::json> mechanisms(const QString& from, const QString& to, const QString& architecture) const;
    resonant::Result<nlohmann::json> compare(const nlohmann::json& architectures) const;
    resonant::Result<nlohmann::json> duplicate(const QString& architecture, const QString& newId);
    resonant::Result<nlohmann::json> createPhysicalSystem(const QString& architecture, const QString& stack);
    resonant::Result<QString> report(const QString& architecture, const QString& format) const;

signals:
    void modelChanged();

private:
    DocumentController* document_;
};
