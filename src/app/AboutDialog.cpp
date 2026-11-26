#include "AboutDialog.h"

#include "foundation/Version.h"
#include "ui_AboutDialog.h"

AboutDialog::AboutDialog(const QString& graphicsDescription, QWidget* parent)
    : QDialog(parent), ui_(std::make_unique<Ui::AboutDialog>())
{
    ui_->setupUi(this);
    QFont titleFont = ui_->titleLabel->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.8);
    titleFont.setBold(true);
    ui_->titleLabel->setFont(titleFont);
    ui_->titleLabel->setText(QStringLiteral("%1 %2").arg(resonant::kApplicationName, resonant::kApplicationVersion));
    ui_->detailsText->setPlainText(
        QStringLiteral("Design. Simulate. Focus. Measure.\n\n"
                       "This version provides the application foundation: scene, geometry, materials, "
                       "project files and the job system. See IMPLEMENTATION_STATUS.md for what is "
                       "implemented and what is not.\n\n"
                       "Graphics: %1\n\n"
                       "Third-party software:\n"
                       "  Qt %2 (LGPL-3.0)\n"
                       "  Eigen (MPL-2.0)\n"
                       "  JSON for Modern C++ by Niels Lohmann (MIT)\n"
                       "  GoogleTest (BSD-3-Clause), tests only")
            .arg(graphicsDescription, QString::fromLatin1(qVersion())));
}

AboutDialog::~AboutDialog() = default;
