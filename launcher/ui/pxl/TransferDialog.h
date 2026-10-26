// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QDialog>
#include "pxl/WorkspaceTransfer.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace Pxl {
class TransferDialog : public QDialog {
    Q_OBJECT
   public:
    enum Mode { Import, Export };
    TransferDialog(Mode mode, QWidget* parent = nullptr, const QString& selectedId = {}, const QString& archive = {});

   private:
    void loadSource();
    void chooseSource(bool folder);
    void updateSummary();
    void runOperation();
    void populate();

    Mode m_mode;
    QString m_archive;
    QString m_previewSource;
    QVector<Workspace> m_workspaces;
    QLineEdit* m_source = nullptr;
    QListWidget* m_list;
    QComboBox* m_format = nullptr;
    QComboBox* m_storage = nullptr;
    QCheckBox* m_worlds = nullptr;
    QCheckBox* m_screenshots = nullptr;
    QLabel* m_summary;
    QPushButton* m_run;
    QPushButton* m_details = nullptr;
    QLabel* m_privacy = nullptr;
    QWidget* m_sourceChoices = nullptr;
    QPushButton* m_changeSource = nullptr;
};
}  // namespace Pxl
