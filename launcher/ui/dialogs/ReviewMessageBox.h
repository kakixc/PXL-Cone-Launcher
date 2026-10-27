#pragma once

#include <QDialog>
#include <QTreeWidgetItem>
#include <QIcon>

namespace Ui {
class ReviewMessageBox;
}

class ReviewMessageBox : public QDialog {
    Q_OBJECT

   public:
    static auto create(QWidget* parent, QString&& title, QString&& icon = "") -> ReviewMessageBox*;

    using ResourceInformation = struct res_info {
        QString name;
        QString filename;
        QString provider;
        QStringList required_by;
        QString version_type;
        bool enabled = true;
        QIcon icon;
    };

    void appendResource(ResourceInformation&& info);
    auto deselectedResources() -> QStringList;

    void retranslateUi(QString resources_name);
    void usePxlPresentation(const QString& instanceName, const QIcon& instanceIcon, const QString& coverPath);

    ~ReviewMessageBox() override;

   protected slots:
    void on_toggleDepsButton_clicked();

   protected:
    ReviewMessageBox(QWidget* parent, const QString& title, const QString& icon);

    Ui::ReviewMessageBox* ui;

    QList<QTreeWidgetItem*> m_deps;
    void updateSelectionActions();
};
