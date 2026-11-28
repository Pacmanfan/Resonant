// The application side of multiscale material modeling (REQ-CHAT-20261126-020,
// RES-MSC-014): the Material Scales panel's access to MultiscaleService through the GUI's
// own session, and the derivation jobs it started. No physics here.
#pragma once

#include "foundation/Result.h"

#include <QObject>
#include <QSet>
#include <QString>

#include <nlohmann/json.hpp>

class DocumentController;

class MultiscaleController : public QObject {
    Q_OBJECT
public:
    explicit MultiscaleController(DocumentController* document, QObject* parent = nullptr);

    nlohmann::json representations() const;
    nlohmann::json representation(const QString& id) const;
    nlohmann::json materials() const;  // [{id, name}]
    resonant::Result<QString> derive(const QString& representation, const QString& method);
    nlohmann::json result(const QString& resultId) const;
    // Assigns the result's properties to an existing material (one undoable step).
    resonant::Status assign(const QString& resultId, const QString& materialId);
    nlohmann::json lineage(const QString& materialId) const;
    bool busy() const { return !pending_.isEmpty(); }

signals:
    void modelChanged();
    void resultReady(const QString& resultId);
    void derivationFailed(const QString& message);

private:
    DocumentController* document_;
    QSet<QString> pending_;
};
