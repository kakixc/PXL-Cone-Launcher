// SPDX-License-Identifier: GPL-3.0-only
#include "ContentBrowser.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "Application.h"
#include "icons/IconList.h"
#include "ui/pxl/InstanceCovers.h"
#include "ResourceDownloadTask.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/ResourceFolderModel.h"
#include "tasks/ConcurrentTask.h"
#include "ui/pxl/TaskHistory.h"
#include "ui/dialogs/ResourceDownloadDialog.h"
#include "ui/pxl/DialogOverlay.h"
#include "ui/pxl/MessagePanel.h"

ContentBrowser::ContentBrowser(MinecraftInstance* instance, QWidget* parent) : QWidget(parent), m_instance(instance)
{
    setObjectName("workspaceContentBrowser");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 16, 24, 24);
    layout->setSpacing(16);
    auto* header = new QHBoxLayout;
    auto* back = new QPushButton(tr("Installed content"), this);
    back->setIcon(QIcon(":/pxl/icons/arrow-left.svg"));
    connect(back, &QPushButton::clicked, this, &ContentBrowser::installedContentRequested);
    header->addWidget(back);
    header->addStretch();
    auto* context = new QLabel("Minecraft " + instance->getPackProfile()->getComponentVersion("net.minecraft"), this);
    context->setObjectName("contentInstanceContext");
    context->setProperty("pxlFlowHint", true);
    header->addWidget(context);
    layout->addLayout(header);
    auto* categories = new QHBoxLayout;
    const QList<QPair<QString, QString>> entries{{"mods", tr("Mods")}, {"resourcepacks", tr("Resource packs")},
        {"shaderpacks", tr("Shaders")}, {"datapacks", tr("Data packs")}};
    for (const auto& entry : entries) {
        auto* button = new QPushButton(entry.second, this);
        button->setCheckable(true);
        button->setProperty("pxlMode", true);
        button->setProperty("contentCategory", entry.first);
        categories->addWidget(button);
        m_categories.append(button);
        connect(button, &QPushButton::clicked, this, [this, id = entry.first] { showCategory(id); });
    }
    categories->addStretch();
    layout->addLayout(categories);
    m_stack = new QStackedWidget(this);
    layout->addWidget(m_stack, 1);
}

bool ContentBrowser::hasModLoader() const
{
    const auto loaders = m_instance->getPackProfile()->getSupportedModLoaders();
    return loaders.has_value() && bool(*loaders);
}

bool ContentBrowser::browse(const QString& category)
{
    auto* profile = m_instance->getPackProfile();
    findChild<QLabel*>("contentInstanceContext")->setText("Minecraft " + profile->getComponentVersion("net.minecraft"));
    const QString key = profile->getComponentVersion("net.minecraft") + ':' +
        QString::number(int(profile->getSupportedModLoaders().value_or(ModPlatform::ModLoaderTypes{})));
    if (key != m_profileKey) {
        if (!prepareToLeave()) return false;
        for (const auto& dialog : m_catalogs) {
            if (dialog) {
                m_stack->removeWidget(dialog);
                dialog->deleteLater();
            }
        }
        m_catalogs.clear();
        m_shaderRequirementAcknowledged = false;
        m_profileKey = key;
    }
    showCategory(category.isEmpty() ? (hasModLoader() ? "mods" : "resourcepacks") : category);
    return true;
}

QWidget* ContentBrowser::prerequisite(const QString& title, const QString& description, const QString& action,
                                    const std::function<void()>& onAction)
{
    auto* card = new QWidget(m_stack);
    card->setObjectName("contentPrerequisite");
    card->setStyleSheet("QWidget#contentPrerequisite { background: rgba(32,35,40,230); border: 1px solid #373d45; border-radius: 16px; }");
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);
    layout->addStretch();
    auto* heading = new QLabel(title, card);
    heading->setStyleSheet("font-size: 20px; font-weight: 600;");
    heading->setWordWrap(true);
    layout->addWidget(heading);
    auto* hint = new QLabel(description, card);
    hint->setWordWrap(true);
    hint->setProperty("pxlFlowHint", true);
    layout->addWidget(hint);
    auto* button = new QPushButton(action, card);
    button->setProperty("pxlPrimary", true);
    layout->addWidget(button, 0, Qt::AlignLeft);
    connect(button, &QPushButton::clicked, this, onAction);
    layout->addStretch();
    m_stack->addWidget(card);
    // Animate only the text, not a cached image of a composite provider/scroll view.
    QTimer::singleShot(0, heading, [heading] { Pxl::animatePage(heading); });
    return card;
}

void ContentBrowser::showCategory(const QString& category)
{
    m_category = category;
    for (auto* button : m_categories) button->setChecked(button->property("contentCategory").toString() == category);
    if (m_notice) {
        m_stack->removeWidget(m_notice);
        m_notice->deleteLater();
        m_notice = nullptr;
    }
    if (category == "mods" && !hasModLoader()) {
        m_notice = prerequisite(tr("A mod loader is needed"),
            tr("This instance has no supported mod loader. Choose one in the version settings before adding mods. Resource packs are available without a loader."),
            tr("Set up a loader"), [this] { emit setupRequested(); });
    } else if (category == "shaderpacks" && !m_shaderRequirementAcknowledged) {
        m_notice = prerequisite(tr("Shaders need a compatible renderer"),
            tr("Downloading a shader pack does not enable shaders. Set up a renderer such as Iris or OptiFine that supports this Minecraft version. The launcher will not install or replace it automatically."),
            tr("I have a renderer — browse shaders"), [this] {
                m_shaderRequirementAcknowledged = true;
                showCategory("shaderpacks");
            });
    } else if (category == "datapacks") {
        m_notice = prerequisite(tr("Choose a world for data packs"),
            tr("Data packs are installed for a world. Open Worlds and select the world you want to manage."),
            tr("Open worlds"), [this] { emit worldsRequested(); });
    }
    if (m_notice) {
        m_stack->setCurrentWidget(m_notice);
        return;
    }
    if (auto* existing = m_catalogs.value(category).data()) {
        m_stack->setCurrentWidget(existing);
        return;
    }
    using namespace ResourceDownload;
    ResourceDownloadDialog* dialog = nullptr;
    if (category == "mods") dialog = new ModDownloadDialog(this, m_instance->loaderModList(), m_instance);
    else if (category == "resourcepacks" && m_instance->traits().contains("texturepacks"))
        dialog = new TexturePackDownloadDialog(this, m_instance->texturePackList(), m_instance);
    else if (category == "resourcepacks") dialog = new ResourcePackDownloadDialog(this, m_instance->resourcePackList(), m_instance);
    else if (category == "shaderpacks") dialog = new ShaderPackDownloadDialog(this, m_instance->shaderPackList(), m_instance);
    if (!dialog) return;
    if (!dialog->hasProviders()) {
        dialog->deleteLater();
        m_notice = prerequisite(tr("No catalog supports this setup"),
            tr("The available providers do not support this loader. You can manage local files or review the instance's version settings."),
            tr("Version settings"), [this] { emit setupRequested(); });
        m_stack->setCurrentWidget(m_notice);
        return;
    }
    dialog->useEmbeddedPresentation();
    dialog->setProperty("pxlReviewInstanceName", m_instance->name());
    dialog->setProperty("pxlReviewInstanceIcon", APPLICATION->icons()->getIcon(m_instance->iconKey()));
    dialog->setProperty("pxlReviewInstanceCover", Pxl::instanceCoverPath(m_instance->instanceRoot()));
    m_catalogs.insert(category, dialog);
    m_stack->addWidget(dialog);
    m_stack->setCurrentWidget(dialog);
    connect(dialog, &ResourceDownloadDialog::embeddedCloseRequested, this, &ContentBrowser::installedContentRequested);
    connect(dialog, &QDialog::accepted, this, [this, category, dialog] {
        // Let QDialog::done finish before showing progress or replacing the accepted page.
        QTimer::singleShot(0, this, [this, category, dialog] { installSelection(category, dialog); });
    });
}

void ContentBrowser::installSelection(const QString& category, ResourceDownload::ResourceDownloadDialog* dialog)
{
    auto* tasks = new ConcurrentTask(tr("Install content"), APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
    for (const auto& task : dialog->getTasks()) tasks->addTask(task);
    if (!m_instance->beginContentInstall(tasks)) {
        tasks->deleteLater();
        dialog->show();
        Pxl::showNotice(this, tr("Installation unavailable"),
                        tr("Stop the game and wait for this instance's current installation to finish before installing more content."));
        return;
    }
    const QPointer<ResourceFolderModel> model = dialog->getBaseModel();
    connect(tasks, &Task::finished, tasks, [model] { if (model && !model->isBeingDestroyed()) model->update(); });
    connect(m_instance, &BaseInstance::statusChanged, tasks, [tasks](auto, auto status) {
        if (status == BaseInstance::Status::Gone && tasks->isRunning()) tasks->abort();
    });
    connect(model.data(), &ResourceFolderModel::aboutToBeDestroyed, tasks, [tasks] { if (tasks->isRunning()) tasks->abort(); });
    const auto title = tr("Install content: %1").arg(m_instance->name());
    Pxl::TaskHistory::instance()->startOwned(std::unique_ptr<Task>(tasks), title,
                                            APPLICATION->icons()->getIcon(m_instance->iconKey()));
    // The task holds the selected ResourceDownloadTask pointers. The catalog
    // and its selection can now go away without cancelling the operation.
    m_stack->removeWidget(dialog);
    m_catalogs.remove(category);
    dialog->deleteLater();
    emit installedContentRequested();
}

bool ContentBrowser::prepareToLeave()
{
    bool hasSelection = false;
    for (const auto& dialog : m_catalogs) hasSelection |= dialog && !dialog->getTasks().isEmpty();
    if (!hasSelection) return true;
    if (!Pxl::messagePanel(this, tr("Leave content browser?"),
        tr("There is content selected for installation. Leave and discard the selection?"), tr("Discard selection"), true)) return false;
    for (const auto& dialog : m_catalogs) {
        if (dialog) {
            m_stack->removeWidget(dialog);
            dialog->deleteLater();
        }
    }
    m_catalogs.clear();
    return true;
}
