#include "ReviewMessageBox.h"
#include "ui_ReviewMessageBox.h"

#include <QClipboard>
#include <QPushButton>
#include <QShortcut>
#include <QStyle>
#include <algorithm>
#include "ui/pxl/ResourceReview.h"

ReviewMessageBox::ReviewMessageBox(QWidget* parent, [[maybe_unused]] QString const& title, [[maybe_unused]] QString const& icon)
    : QDialog(parent), ui(new Ui::ReviewMessageBox)
{
    ui->setupUi(this);

    ui->toggleDepsButton->hide();
    ui->gridLayout->setContentsMargins(24, 24, 24, 24);
    ui->gridLayout->setVerticalSpacing(16);
    ui->gridLayout->setRowStretch(2, 1);
    ui->explainLabel->setWordWrap(true);
    ui->modTreeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->modTreeWidget->header()->setStretchLastSection(false);
    ui->modTreeWidget->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ReviewMessageBox::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &ReviewMessageBox::reject);

    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Install selected"));
    ui->buttonBox->button(QDialogButtonBox::Ok)->setProperty("pxlPrimary", true);
    ui->buttonBox->button(QDialogButtonBox::Ok)->style()->unpolish(ui->buttonBox->button(QDialogButtonBox::Ok));
    ui->buttonBox->button(QDialogButtonBox::Ok)->style()->polish(ui->buttonBox->button(QDialogButtonBox::Ok));
    ui->onlyCheckedLabel->setText(tr("Only selected add-ons will be downloaded."));
    connect(ui->modTreeWidget, &QTreeWidget::itemChanged, this, [this] { updateSelectionActions(); });
    updateSelectionActions();

    // Overwrite Ctrl+C functionality to exclude the label when copying text from tree
    auto shortcut = new QShortcut(QKeySequence::Copy, ui->modTreeWidget);
    connect(shortcut, &QShortcut::activated, [this]() {
        auto currentItem = this->ui->modTreeWidget->currentItem();
        if (!currentItem)
            return;
        auto currentColumn = this->ui->modTreeWidget->currentColumn();

        auto data = currentItem->data(currentColumn, Qt::UserRole);
        QString txt;

        if (data.isValid()) {
            txt = data.toString();
        } else {
            txt = currentItem->text(currentColumn);
        }

        QApplication::clipboard()->setText(txt);
    });
}

ReviewMessageBox::~ReviewMessageBox()
{
    delete ui;
}

auto ReviewMessageBox::create(QWidget* parent, QString&& title, QString&& icon) -> ReviewMessageBox*
{
    return new ReviewMessageBox(parent, title, icon);
}

void ReviewMessageBox::appendResource(ResourceInformation&& info)
{
    auto itemTop = new QTreeWidgetItem(ui->modTreeWidget);
    itemTop->setCheckState(0, info.enabled ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);
    itemTop->setText(0, info.name);
    itemTop->setIcon(0, info.icon);
    itemTop->setData(0, Qt::UserRole + 1, info.required_by.isEmpty() ? info.provider : tr("Required by: %1").arg(info.required_by.join(", ")));
    if (!info.enabled) {
        itemTop->setToolTip(0, tr("Mod was disabled as it may be already installed."));
    }

    auto filenameItem = new QTreeWidgetItem(itemTop);
    filenameItem->setText(0, tr("Filename: %1").arg(info.filename));
    filenameItem->setData(0, Qt::UserRole, info.filename);

    auto providerItem = new QTreeWidgetItem(itemTop);
    providerItem->setText(0, tr("Provider: %1").arg(info.provider));
    providerItem->setData(0, Qt::UserRole, info.provider);

    if (!info.required_by.isEmpty()) {
        auto requiredByItem = new QTreeWidgetItem(itemTop);
        if (info.required_by.length() == 1) {
            requiredByItem->setText(0, tr("Required by: %1").arg(info.required_by.back()));
            requiredByItem->setData(0, Qt::UserRole, info.required_by.back());
        } else {
            requiredByItem->setText(0, tr("Required by:"));
            for (auto req : info.required_by) {
                auto reqItem = new QTreeWidgetItem(requiredByItem);
                reqItem->setText(0, req);
            }
        }

        ui->toggleDepsButton->show();
        m_deps << itemTop;
    }

    auto versionTypeItem = new QTreeWidgetItem(itemTop);
    versionTypeItem->setText(0, tr("Version Type: %1").arg(info.version_type));
    versionTypeItem->setData(0, Qt::UserRole, info.version_type);

    ui->modTreeWidget->addTopLevelItem(itemTop);
    updateSelectionActions();
}

void ReviewMessageBox::usePxlPresentation(const QString& instanceName, const QIcon& instanceIcon, const QString& coverPath)
{
    if (property("pxlResourceReview").toBool()) return;
    setProperty("pxlResourceReview", true);
    setProperty("pxlFlow", true);
    setProperty("pxlPreferredSize", QSize(700, qBound(380, 280 + ui->modTreeWidget->topLevelItemCount() * 80, 660)));
    ui->gridLayout->addWidget(new Pxl::ReviewInstanceCard(instanceName, instanceIcon, coverPath, this), 0, 0);
    ui->explainLabel->setText(tr("Choose add-ons to install"));
    ui->modTreeWidget->setItemDelegate(new Pxl::ResourceReviewDelegate(ui->modTreeWidget));
    ui->modTreeWidget->setAlternatingRowColors(false);
    ui->modTreeWidget->setMouseTracking(true);
    ui->modTreeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->modTreeWidget->setFrameShape(QFrame::NoFrame);
    ui->modTreeWidget->setStyleSheet("QTreeWidget { background: transparent; border: none; } QTreeWidget::item { padding: 0; background: transparent; }");
    for (int i = 0; i < ui->modTreeWidget->topLevelItemCount(); ++i)
        ui->modTreeWidget->topLevelItem(i)->setFirstColumnSpanned(true);
}

auto ReviewMessageBox::deselectedResources() -> QStringList
{
    QStringList list;

    auto* item = ui->modTreeWidget->topLevelItem(0);

    for (int i = 1; item != nullptr; ++i) {
        if (item->checkState(0) == Qt::CheckState::Unchecked) {
            list.append(item->text(0));
        }

        item = ui->modTreeWidget->topLevelItem(i);
    }

    return list;
}

void ReviewMessageBox::retranslateUi(QString resources_name)
{
    setWindowTitle(tr("Confirm %1 selection").arg(resources_name));

    ui->explainLabel->setText(tr("You're about to download the following %1:").arg(resources_name));
    ui->onlyCheckedLabel->setText(tr("Only selected add-ons will be downloaded."));
}
void ReviewMessageBox::updateSelectionActions()
{
    const bool allDependenciesSelected = std::all_of(m_deps.cbegin(), m_deps.cend(), [](const auto* item) {
        return item->checkState(0) == Qt::Checked;
    });
    ui->toggleDepsButton->setText(allDependenciesSelected ? tr("Deselect all dependencies") : tr("Select all dependencies"));
    ui->toggleDepsButton->setVisible(!m_deps.isEmpty());
    bool selected = false;
    for (int i = 0; i < ui->modTreeWidget->topLevelItemCount(); ++i)
        selected |= ui->modTreeWidget->topLevelItem(i)->checkState(0) == Qt::Checked;
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(selected);
}

void ReviewMessageBox::on_toggleDepsButton_clicked()
{
    const auto state = std::all_of(m_deps.cbegin(), m_deps.cend(), [](const auto* item) {
        return item->checkState(0) == Qt::Checked;
    }) ? Qt::Unchecked : Qt::Checked;
    for (auto dep : m_deps)
        dep->setCheckState(0, state);
    updateSelectionActions();
}
