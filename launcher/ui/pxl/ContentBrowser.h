// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QHash>
#include <QPointer>
#include <QWidget>
#include <functional>
#include <memory>

class MinecraftInstance;
class QPushButton;
class QStackedWidget;
class DataPackFolderModel;
namespace ResourceDownload { class ResourceDownloadDialog; }

// A single in-instance destination; provider queries and install review remain upstream-owned.
class ContentBrowser : public QWidget {
    Q_OBJECT
public:
    explicit ContentBrowser(MinecraftInstance* instance, QWidget* parent = nullptr);
    bool browse(const QString& category = {});
    bool prepareToLeave();
    void setWorldDataPacks(std::shared_ptr<DataPackFolderModel> model, const QString& worldName);

signals:
    void installedContentRequested();
    void setupRequested();
    void worldsRequested();

private:
    bool hasModLoader() const;
    void showCategory(const QString& category);
    void installSelection(const QString& category, ResourceDownload::ResourceDownloadDialog* dialog);
    QWidget* prerequisite(const QString& title, const QString& description, const QString& action,
                          const std::function<void()>& onAction);

    MinecraftInstance* m_instance;
    QStackedWidget* m_stack;
    QList<QPushButton*> m_categories;
    QHash<QString, QPointer<ResourceDownload::ResourceDownloadDialog>> m_catalogs;
    QWidget* m_notice = nullptr;
    QString m_category;
    QString m_profileKey;
    bool m_shaderRequirementAcknowledged = false;
    std::shared_ptr<DataPackFolderModel> m_worldDataPacks;
    QString m_worldName;
};
