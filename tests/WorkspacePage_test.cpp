// SPDX-License-Identifier: GPL-3.0-only

#include <QSignalSpy>
#include <QTextBrowser>
#include <QTemporaryDir>
#include <QTest>
#include <QLineEdit>
#include <QPushButton>
#include <QDir>
#include <QApplication>
#include <QFontDatabase>
#include <QRawFont>
#include <QStyleFactory>
#include <QListView>
#include <QListWidget>
#include <QFile>
#include <QDateTime>
#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QStringListModel>
#include <QWheelEvent>
#include <QVBoxLayout>
#include <QSlider>
#include <QMessageBox>
#include <ui/pagedialog/PageDialog.h>
#include <ui/widgets/PageContainer.h>
#include <ui/pxl/TransferDialog.h>
#include <ui/pxl/ContentBrowser.h>
#include <ui/dialogs/ResourceDownloadDialog.h>
#include <BaseVersionList.h>
#include <QStandardItemModel>
#include <ui/pxl/FrostedSurface.h>
#include <ui/pxl/CatalogCardDelegate.h>
#include <ui/pxl/InstanceCovers.h>
#include <ui/pxl/CoverPicker.h>
#include <ui/pxl/CheckListDelegate.h>
#include <ui/pxl/InstanceLibraryDelegate.h>
#include <ui/pxl/LibraryControls.h>
#include <ui/pxl/ActionMenu.h>
#include <ui/pxl/AccountsPage.h>
#include <ui/pages/modplatform/ResourcePage.h>

class CatalogTestVersion final : public BaseVersion {
public:
    explicit CatalogTestVersion(QString id) : m_id(std::move(id)) {}
    QString descriptor() const override { return m_id; }
    QString name() const override { return m_id; }
    QString typeString() const override { return "release"; }
private:
    QString m_id;
};

class ResettableVersionList final : public BaseVersionList {
public:
    Task::Ptr getLoadTask(bool = false) override { return nullptr; }
    bool isLoaded() override { return true; }
    const BaseVersion::Ptr at(int index) const override { return m_versions.value(index); }
    int count() const override { return m_versions.size(); }
    void sortVersions() override {}
    void replace(const QStringList& versions)
    {
        QList<BaseVersion::Ptr> entries;
        for (const auto& version : versions) entries.append(std::make_shared<CatalogTestVersion>(version));
        updateListData(entries);
    }
protected:
    void updateListData(QList<BaseVersion::Ptr> versions) override
    {
        beginResetModel();
        m_versions = std::move(versions);
        endResetModel();
    }
private:
    QList<BaseVersion::Ptr> m_versions;
};

class EmptyCatalogDialog final : public ResourceDownload::ResourceDownloadDialog {
public:
    explicit EmptyCatalogDialog(QWidget* parent) : ResourceDownloadDialog(parent, nullptr)
    {
        initializeContainer();
        connectButtons();
    }
    QList<BasePage*> getPages() override { return {}; }
};

// The real page/container layout with deterministic data and no network requests.
class LayoutCatalogPage final : public ResourceDownload::ResourcePage {
public:
    LayoutCatalogPage(ResourceDownload::ResourceDownloadDialog* parent, BaseInstance& instance)
        : ResourcePage(parent, instance)
    {
        auto* model = new QStandardItemModel(this);
        for (int i = 0; i < 8; ++i) {
            auto pack = std::make_shared<ModPlatform::IndexedPack>();
            pack->name = QString("Resource pack %1").arg(i + 1);
            pack->authors.append(ModPlatform::ModpackAuthor{"Example creator", {}});
            auto* item = new QStandardItem;
            item->setData(QVariant::fromValue(pack), Qt::UserRole);
            item->setData("A resource pack description showing how the full catalog uses its available space.", UserDataTypes::DESCRIPTION);
            item->setData(QIcon(":/pxl/icons/squares-four.svg"), Qt::DecorationRole);
            model->appendRow(item);
        }
        findChild<QListView*>("packView")->setModel(model);
        auto* sorting = findChild<QComboBox*>("sortByBox");
        sorting->addItems({"Relevance", "Downloads"});
        connect(sorting, &QComboBox::currentIndexChanged, this, [this] { ++sortChanges; });
    }
    QString id() const override { return "layout-catalog"; }
    QString displayName() const override { return "Example provider"; }
    QIcon icon() const override { return QIcon(":/pxl/icons/squares-four.svg"); }
    QString helpPage() const override { return {}; }
    bool shouldDisplay() const override { return true; }
    QString metaEntryBase() const override { return {}; }
    QString debugName() const override { return "layout fixture"; }
    bool supportsFiltering() const override { return false; }
    void openedImpl() override
    {
        findChild<QWidget*>("filterWidget")->hide();
        findChild<QPushButton*>("resourceFilterButton")->hide();
        findChild<QLineEdit*>("searchEdit")->setPlaceholderText("Search resource packs");
    }
    void previewFirst() { showProjectDetails(findChild<QListView*>("packView")->model()->index(0,0)); }
    int sortChanges = 0;
protected:
    void triggerSearch() override {}
    QMap<QString, QString> urlHandlers() const override { return {}; }
};

class LayoutCatalogDialog final : public ResourceDownload::ResourceDownloadDialog {
public:
    LayoutCatalogDialog(QWidget* parent, BaseInstance& instance) : ResourceDownloadDialog(parent, nullptr), m_instance(instance)
    {
        initializeContainer();
        connectButtons();
    }
    QList<BasePage*> getPages() override { return {new LayoutCatalogPage(this, m_instance)}; }
private:
    BaseInstance& m_instance;
};

class PreviewSettingsPage final : public QWidget, public BasePage {
public:
    PreviewSettingsPage()
    {
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel("Memory allocation", this));
        edit = new QLineEdit("original", this);
        layout->addWidget(edit);
        slider = new QSlider(Qt::Horizontal, this);
        layout->addWidget(slider);
    }
    QString id() const override { return "memory"; }
    QString displayName() const override { return "Performance"; }
    QIcon icon() const override { return {}; }
    bool apply() override { ++applies; return valid; }
    QLineEdit* edit = nullptr;
    QSlider* slider = nullptr;
    bool valid = true;
    int applies = 0;
};

#include <memory>
#include <vector>
#include <cmath>

#include <InstanceList.h>
#include <settings/INISettingsObject.h>
#include <ui/pxl/WorkspacePage.h>
#include <ui/themes/MinecraftTheme.h>
#include <ui/themes/ConeTheme.h>
#include <ui/dialogs/ProgressDialog.h>
#include <ui/pxl/TaskHistory.h>
#include <ui/pxl/DialogOverlay.h>
#include <ui/pxl/Backdrop.h>
#include <ui/widgets/VersionSelectWidget.h>
#include <translations/POTranslator.h>
#include <minecraft/MinecraftInstance.h>
#include <minecraft/PackProfile.h>

class PreviewInstallTask final : public Task {
   public:
    PreviewInstallTask() { setAbortable(true); }
    bool isMultiStep() const override { return true; }
   protected:
    void executeTask() override
    {
        setStatus("Downloading game files");
        setDetails("Libraries and assets");
        setProgress(45, 100);
        TaskStepProgress step;
        step.update(35, 100);
        step.status = "Downloading assets";
        emit stepProgress(step);
        QTimer::singleShot(150, this, [this] { if (isRunning()) emitSucceeded(); });
    }
};

class ManualHistoryTask final : public Task {
   public:
    void complete() { emitSucceeded(); }
   protected:
    void executeTask() override {}
};

class WorkspacePageTest : public QObject {
    Q_OBJECT

   private:
    static std::unique_ptr<INISettingsObject> settings(const QTemporaryDir& temporary)
    {
        auto result = std::make_unique<INISettingsObject>(temporary.filePath("global.cfg"));
        for (const auto* key : {"ShowGameTime", "ShowGlobalGameTime", "RecordGameTime", "PreLaunchCommand", "WrapperCommand", "PostExitCommand",
                                "ShowConsole", "AutoCloseConsole", "ShowConsoleOnError", "LogPrePostOutput", "ConsoleMaxLines",
                                "ConsoleOverflowStop"})
            result->registerSetting(key, QVariant());
        result->registerSetting("WorkspaceLibraryView", "cards");
        return result;
    }

   private slots:
    void initTestCase()
    {
        ConeTheme cone;
        qApp->setStyle(QStyleFactory::create(cone.qtTheme()));
        qApp->setPalette(cone.colorScheme());
        qApp->setStyleSheet(cone.appStyleSheet());
        const auto inter = QRawFont::fromFont(QFont("Inter", 12));
        QVERIFY(inter.supportsCharacter(QChar(u'И')));
        QVERIFY(inter.supportsCharacter(QChar(u'ё')));
        if (qEnvironmentVariable("PXL_UI_THEME") == "minecraft") {
            MinecraftTheme theme;
            qApp->setStyle(QStyleFactory::create(theme.qtTheme()));
            qApp->setPalette(theme.colorScheme());
            qApp->setStyleSheet(theme.appStyleSheet());
            const int fontId = QFontDatabase::addApplicationFont(":/fonts/Monocraft.ttf");
            QVERIFY(fontId >= 0);
            const auto families = QFontDatabase::applicationFontFamilies(fontId);
            QVERIFY(!families.isEmpty());
            const auto font = QRawFont::fromFont(QFont(families.first(), 12));
            QVERIFY(font.supportsCharacter(QChar(u'И')));
            QVERIFY(font.supportsCharacter(QChar(u'ё')));
        }
    }

    void accountHeaderAndProfilesUseExistingAccountModel()
    {
        QTemporaryDir temporary;
        auto globalSettings=settings(temporary);
        InstanceList instances(globalSettings.get(),temporary.filePath("instances"));
        WorkspacePage surface(&instances);
        surface.resize(1080,740); surface.show();
        auto* title=surface.findChild<QLabel*>("workspacePageTitle");
        QCOMPARE(title->text(),QString("Welcome!"));
        surface.setAccountSummary("Alex");
        QVERIFY(title->text().contains("Alex"));
        QSignalSpy quickSwitch(&surface,&WorkspacePage::accountMenuRequested);
        surface.findChild<QPushButton*>("workspaceProfile")->click();
        QCOMPARE(quickSwitch.count(),1);
        surface.findChild<QPushButton*>("workspaceLibrary")->click();
        surface.setAccountSummary("Sam");
        QCOMPARE(title->text(),QString("Library"));
        AccountList accounts;
        auto* page=new Pxl::AccountsPage(&accounts,false);
        QVERIFY(surface.presentInstanceView(page,"Accounts",{}));
        QVERIFY(surface.findChild<QPushButton*>("workspaceAccounts")->isChecked());
        QVERIFY(page->findChildren<QFrame*>("pxlAccountRow").isEmpty());
        auto first=MinecraftAccount::createOffline("Alex");
        auto second=MinecraftAccount::createOffline("Sam");
        accounts.addAccount(first); accounts.addAccount(second); accounts.setDefaultAccount(first);
        QTRY_COMPARE(page->findChildren<QFrame*>("pxlAccountRow").size(),2);
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        auto controls=page->findChildren<QPushButton*>("pxlUseProfile");
        QCOMPARE(controls.size(),2);
        QVERIFY(!controls[0]->isEnabled()); QVERIFY(controls[1]->isEnabled());
        controls[1]->click(); QCOMPARE(accounts.defaultAccount(),second);
        QCoreApplication::processEvents(); QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        QTest::qWait(80);
        const auto accountRows=page->findChildren<QFrame*>("pxlAccountRow");
        QCOMPARE(accountRows.size(),2);
        QVERIFY(accountRows[0]->isVisible()); QVERIFY(accountRows[0]->height()>=70);
        const auto screenshots=qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR");
        if (!screenshots.isEmpty()) QVERIFY(surface.grab().save(QDir(screenshots).filePath("accounts-page.png")));
        QVERIFY(surface.leaveInstanceView());
        surface.findChild<QPushButton*>("workspaceHome")->click();
        surface.setAccountSummary({}); QCOMPARE(title->text(),QString("Welcome!"));
    }

    void quickAccountPopupKeepsAddActionOutsideScrollingProfiles()
    {
        QWidget surface; surface.resize(900,600); surface.show();
        AccountList accounts;
        for (int i=0;i<12;++i) accounts.addAccount(MinecraftAccount::createOffline(QString("Player%1").arg(i)));
        bool manage=false;
        QPointer<Pxl::AccountPopup> popup=new Pxl::AccountPopup(&accounts,&surface,surface.mapToGlobal(QPoint(890,70)),[&] { manage=true; });
        popup->show(); QCoreApplication::processEvents();
        QVERIFY(popup->height()<=360);
        QVERIFY(surface.frameGeometry().contains(popup->geometry()));
        auto* scroll=popup->findChild<QScrollArea*>("pxlAccountChoices");
        QVERIFY(scroll->verticalScrollBar()->maximum()>0);
        auto* add=popup->findChild<QPushButton*>("pxlQuickAddAccount");
        QVERIFY(add->isVisible()); QVERIFY(add->parentWidget()==popup);
        auto choices=popup->findChildren<QPushButton*>("pxlQuickProfile");
        QCOMPARE(choices.size(),12);
        const auto screenshots=qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR");
        if (!screenshots.isEmpty()) QVERIFY(popup->grab().save(QDir(screenshots).filePath("account-switcher.png")));
        choices[1]->click(); QCOMPARE(accounts.defaultAccount(),accounts.at(1));
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); QVERIFY(!popup);
        AccountList empty;
        popup=new Pxl::AccountPopup(&empty,&surface,surface.mapToGlobal(QPoint(890,70)),[&] { manage=true; });
        popup->show();
        QVERIFY(popup->findChildren<QPushButton*>("pxlQuickProfile").isEmpty());
        QVERIFY(popup->height()<100);
        popup->findChild<QPushButton*>("pxlQuickAddAccount")->click(); QVERIFY(manage);
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
    }

    void frostedSurfaceBlursTheApplicationBackdrop()
    {
        QImage pattern(4, 4, QImage::Format_RGB32);
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x) pattern.setPixelColor(x, y, x < 2 ? Qt::white : Qt::black);
        QWidget source;
        QPalette palette = source.palette();
        palette.setBrush(QPalette::Window, QBrush(QPixmap::fromImage(pattern)));
        source.setPalette(palette);
        source.setAutoFillBackground(true);
        source.resize(400, 240);
        source.show();
        QCoreApplication::processEvents();
        const QRect region(40, 40, 200, 120);
        const auto original = source.grab(region).toImage();
        const auto blurred = Pxl::frostedBackdrop(&source, QRect(source.mapToGlobal(region.topLeft()), region.size())).toImage();
        QVERIFY(!blurred.isNull());
        auto contrast = [](const QImage& image) {
            qint64 value = 0;
            for (int x = 20; x < image.width() - 20; ++x)
                value += qAbs(image.pixelColor(x, 50).lightness() - image.pixelColor(x - 1, 50).lightness());
            return value;
        };
        QVERIFY(contrast(original) > 1000);
        QVERIFY(contrast(blurred) < contrast(original) / 3);
    }

    void catalogCardsKeepSelectionAndDetailsAsDistinctActions()
    {
        QStandardItemModel model;
        auto pack = std::make_shared<ModPlatform::IndexedPack>();
        pack->name = "Example performance mod";
        pack->authors.append(ModPlatform::ModpackAuthor{"Example author", {}});
        auto* item = new QStandardItem;
        item->setData(QVariant::fromValue(pack), Qt::UserRole);
        item->setData(pack->name, UserDataTypes::TITLE);
        item->setData("A readable description of the project and what it changes in your game.", UserDataTypes::DESCRIPTION);
        item->setData(Qt::Unchecked, Qt::CheckStateRole);
        model.appendRow(item);
        QListView view;
        view.setModel(&model);
        int selections = 0, details = 0;
        auto* delegate = new Pxl::CatalogCardDelegate(&view, [&](const QModelIndex& index) {
            ++selections;
            model.setData(index, Qt::Checked, Qt::CheckStateRole);
        }, [&](const QModelIndex&) { ++details; });
        view.setItemDelegate(delegate);
        view.resize(1000, 300);
        view.show();
        QCoreApplication::processEvents();
        const auto row = view.visualRect(model.index(0, 0));
        QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, Pxl::CatalogCardDelegate::actionRect(row).center());
        QCOMPARE(selections, 1);
        QCOMPARE(details, 0);
        QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, row.center());
        QCOMPARE(selections, 1);
        QCOMPARE(details, 1);
        const auto screenshots = qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR");
        if (!screenshots.isEmpty()) QVERIFY(view.grab().save(QDir(screenshots).filePath("catalog-cards.png")));
    }

    void transferRowsToggleFromBodyCheckboxAndKeyboard()
    {
        QListWidget view;
        view.setItemDelegate(new Pxl::CheckListDelegate(&view));
        view.setSelectionMode(QAbstractItemView::NoSelection);
        auto* item=new QListWidgetItem("Survival World",&view);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        view.resize(500,180); view.show();
        QCoreApplication::processEvents();
        const auto row=view.visualItemRect(item);
        QTest::mouseClick(view.viewport(),Qt::LeftButton,Qt::NoModifier,row.center());
        QCOMPARE(item->checkState(),Qt::Checked);
        QTest::mouseClick(view.viewport(),Qt::LeftButton,Qt::NoModifier,QPoint(row.left()+22,row.center().y()));
        QCOMPARE(item->checkState(),Qt::Unchecked);
        view.setCurrentItem(item);
        QTest::keyClick(&view,Qt::Key_Space);
        QCOMPARE(item->checkState(),Qt::Checked);
    }

    void libraryReadsDeclaredVersionsBeforeProfileIsOpened()
    {
        QTemporaryDir temporary;
        auto globalSettings=settings(temporary);
        const auto root=temporary.filePath("instances");
        QVERIFY(QDir().mkpath(root+"/pack"));
        QFile config(root+"/pack/instance.cfg");
        QVERIFY(config.open(QIODevice::WriteOnly));
        config.write("[General]\nInstanceType=OneSix\nname=User name is not a version\n"); config.close();
        QFile pack(root+"/pack/mmc-pack.json");
        QVERIFY(pack.open(QIODevice::WriteOnly));
        const QByteArray declared=R"({"formatVersion":1,"components":[{"uid":"net.minecraft","version":"1.20.1"},{"uid":"net.fabricmc.fabric-loader","version":"0.16.14"}]})";
        pack.write(declared); pack.close();
        InstanceList instances(globalSettings.get(),root); instances.loadList();
        WorkspacePage page(&instances);
        page.findChild<QPushButton*>("workspaceLibrary")->click();
        const auto index=page.findChild<QListView*>("workspaceCards")->model()->index(0,0);
        QCOMPARE(index.data(Qt::UserRole+0x5a).toString(),QString("Minecraft 1.20.1 · Fabric"));
        QVERIFY(pack.open(QIODevice::ReadOnly)); QCOMPARE(pack.readAll(),declared);
    }

    void screenshotGalleryKeepsInstancesAndExportsWithoutOverwrite()
    {
        QTemporaryDir temporary;
        auto globalSettings=settings(temporary);
        const auto root=temporary.filePath("instances");
        for (int i=0;i<2;++i) {
            const auto folder=root+"/pack"+QString::number(i);
            QVERIFY(QDir().mkpath(folder));
            QFile config(folder+"/instance.cfg"); QVERIFY(config.open(QIODevice::WriteOnly));
            config.write(QString("[General]\nInstanceType=OneSix\nname=World %1\n").arg(i).toUtf8());
        }
        InstanceList instances(globalSettings.get(),root); instances.loadList();
        for (int i=0;i<2;++i) {
            const auto shots=QDir(instances.at(i)->gameRoot()).filePath("screenshots/session");
            QVERIFY(QDir().mkpath(shots));
            QImage image(320,180,QImage::Format_RGB32); image.fill(i ? QColor("#366b65") : QColor("#a67752"));
            QVERIFY(image.save(shots+"/same-name.png"));
        }
        Pxl::ScreenshotGallery gallery(&instances);
        gallery.resize(1000,660); gallery.show();
        QTRY_COMPARE(gallery.view()->model()->rowCount(),2);
        gallery.filterInstance("pack0");
        QCOMPARE(gallery.view()->model()->rowCount(),1);
        gallery.view()->setCurrentIndex(gallery.view()->model()->index(0,0));
        const auto source=gallery.selectedFile();
        QVERIFY(!source.isEmpty());
        Pxl::ScreenshotEntry entry{source,"pack0","World 0",instances.at(0)->instanceRoot(),QFileInfo(source).absolutePath(),0};
        const auto destination=temporary.filePath("export");
        QVERIFY(Pxl::exportScreenshots({entry},destination).isEmpty());
        QVERIFY(Pxl::exportScreenshots({entry},destination).isEmpty());
        QDir exported(destination+"/PXL-World 0-pack0");
        QCOMPARE(exported.entryList({"*.png"},QDir::Files).size(),2);
        QVERIFY(QFileInfo::exists(source));
        auto outside=entry; outside.instanceRoot=temporary.filePath("different-instance");
        QVERIFY(QDir().mkpath(outside.instanceRoot));
        QVERIFY(Pxl::scanScreenshots({outside}).isEmpty());
        gallery.findChild<QComboBox*>("screenshotInstances")->setCurrentIndex(0);
        const auto screenshots=qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR");
        QTest::qWait(80);
        if (!screenshots.isEmpty()) QVERIFY(gallery.grab().save(QDir(screenshots).filePath("screenshot-gallery.png")));
        QTimer::singleShot(230,&gallery,[&] {
            auto* preview=gallery.findChild<QWidget*>("pxlScreenshotPreview");
            QVERIFY(preview); QVERIFY(preview->width()>800); QVERIFY(preview->height()>500);
            if (!screenshots.isEmpty()) QVERIFY(gallery.grab().save(QDir(screenshots).filePath("screenshot-preview.png")));
            QTest::keyClick(preview,Qt::Key_Escape);
        });
        QTest::mouseClick(gallery.view()->viewport(),Qt::LeftButton,Qt::NoModifier,gallery.view()->visualRect(gallery.view()->model()->index(0,0)).center());
    }

    void screenshotLightboxAnimatesAndSupportsImmediateClose()
    {
        QTemporaryDir temporary;
        QImage image(640,360,QImage::Format_RGB32); image.fill(QColor("#566e7d"));
        const auto file=temporary.filePath("frame.png"); QVERIFY(image.save(file));
        QWidget surface; surface.resize(900,600);
        QPushButton background("Behind image",&surface); background.show(); surface.show();
        const bool previous=QApplication::isEffectEnabled(Qt::UI_AnimateMenu);
        const bool previousGeneral=QApplication::isEffectEnabled(Qt::UI_General);
        QApplication::setEffectEnabled(Qt::UI_General,true);
        for (bool animated : {true,false}) {
            QApplication::setEffectEnabled(Qt::UI_AnimateMenu,animated);
            const bool enabled=QApplication::isEffectEnabled(Qt::UI_AnimateMenu);
            bool inspected=false;
            QTimer::singleShot(animated ? 220 : 0,&surface,[&] {
                auto* layer=surface.findChild<QWidget*>("pxlScreenshotPreview");
                QVERIFY(layer); QVERIFY(!layer->isWindow()); QVERIFY(!background.isEnabled());
                QTest::keyClick(layer,Qt::Key_Escape);
                QCOMPARE(layer->isVisible(),enabled);
                inspected=true;
            });
            Pxl::ScreenshotLightbox::showImage(file,&surface);
            QVERIFY(inspected); QVERIFY(background.isEnabled());
            QVERIFY(!surface.findChild<QWidget*>("pxlScreenshotPreview"));
        }
        QApplication::setEffectEnabled(Qt::UI_AnimateMenu,previous);
        QApplication::setEffectEnabled(Qt::UI_General,previousGeneral);
    }

    void contentPreviewUsesOwnHeaderAndControls()
    {
        QTemporaryDir temporary;
        auto globalSettings=settings(temporary);
        const auto root=temporary.filePath("pack"); QVERIFY(QDir().mkpath(root));
        QFile config(root+"/instance.cfg"); QVERIFY(config.open(QIODevice::WriteOnly)); config.write("[General]\nInstanceType=OneSix\nname=Preview test\n"); config.close();
        InstanceList instances(globalSettings.get(),temporary.path()); instances.loadList();
        auto* instance=dynamic_cast<MinecraftInstance*>(instances.getInstanceById("pack")); QVERIFY(instance);
        QWidget surface; surface.resize(1080,740); surface.show();
        LayoutCatalogDialog dialog(&surface,*instance);
        dialog.useEmbeddedPresentation();
        auto* page=dynamic_cast<LayoutCatalogPage*>(dialog.findChild<PageContainer*>()->selectedPage());
        QVERIFY(page);
        page->findChild<QTextBrowser*>("packDescription")->setHtml("<h2>A detailed project description</h2><p>Project content and screenshots appear here.</p>");
        QTimer::singleShot(50,&dialog,[&] {
            auto* preview=surface.findChild<QDialog*>("pxlProjectPreview");
            if (!preview) preview=dialog.findChild<QDialog*>("pxlProjectPreview");
            QVERIFY(preview);
            QVERIFY(preview->findChild<QComboBox*>());
            QCOMPARE(preview->findChildren<QTextBrowser*>().size(),1);
            const auto screenshots=qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR");
            if (!screenshots.isEmpty()) QVERIFY(surface.grab().save(QDir(screenshots).filePath("content-preview.png")));
            preview->accept();
        });
        page->previewFirst();
    }

    void loaderVersionsBecomeSelectableAfterModelReload()
    {
        ResettableVersionList versions;
        VersionSelectWidget picker(nullptr);
        picker.useCompactPresentation();
        picker.setEmptyString("No versions");
        picker.initialize(&versions);
        auto* combo = picker.findChild<QComboBox*>("compactVersionChoice");
        QVERIFY(!combo->isEnabled());
        QVERIFY(!picker.selectedVersion());
        versions.replace({"0.30.1", "0.29.2"});
        QVERIFY(combo->isEnabled());
        QVERIFY(picker.selectedVersion());
        QCOMPARE(combo->currentText(), QString("0.30.1"));
        combo->setCurrentIndex(1);
        emit combo->activated(1);
        QCOMPARE(picker.selectedVersion()->descriptor(), QString("0.29.2"));
        versions.replace({});
        QVERIFY(!combo->isEnabled());
        QVERIFY(!picker.selectedVersion());
        versions.replace({"0.31.0", "0.29.2"});
        QVERIFY(picker.selectedVersion());
        QCOMPARE(picker.selectedVersion()->descriptor(), QString("0.29.2"));
        QCOMPARE(combo->currentText(), QString("0.29.2"));
    }

    void libraryChoiceAndForwardedCommandsKeepTheirBehavior()
    {
        QWidget surface;
        surface.resize(500, 300);
        Pxl::ChoiceChip chip(&surface);
        chip.addItems({"All groups", "Survival"});
        chip.resize(180, 34);
        surface.show();
        QSignalSpy selected(&chip, &QComboBox::currentIndexChanged);
        QTimer::singleShot(0, &chip, [&chip] {
            auto* menu = chip.findChild<QMenu*>();
            QVERIFY(menu);
            QVERIFY(menu->actions().at(0)->isChecked());
            menu->actions().at(1)->trigger();
            menu->close();
        });
        chip.showPopup();
        QCOMPARE(chip.currentText(), QString("Survival"));
        QCOMPARE(selected.count(), 1);
        QAction command("Original command", &surface);
        command.setCheckable(true);
        command.setShortcut(QKeySequence("Ctrl+D"));
        Pxl::FrostedMenu menu(&surface);
        QVERIFY(menu.windowFlags().testFlag(Qt::NoDropShadowWindowHint));
        auto* presented = Pxl::forwardMenuAction(&menu, &command, "Duplicate");
        QSignalSpy invoked(&command, &QAction::triggered);
        presented->trigger();
        QCOMPARE(invoked.count(), 1);
        QVERIFY(command.isChecked());
        QVERIFY(presented->isChecked());
        QCOMPARE(command.text(), QString("Original command"));
        command.setEnabled(false);
        QVERIFY(!presented->isEnabled());
    }

    void emptyTaskPopoverFitsItsContent()
    {
        QWidget surface;
        surface.resize(1080, 740);
        surface.show();
        Pxl::TaskHistory history;
        history.showTasks(&surface);
        QCoreApplication::processEvents();
        auto* panel = surface.findChild<QDialog*>("pxlTaskPopover");
        QVERIFY(panel);
        QVERIFY(panel->width() <= 340);
        QVERIFY(panel->height() <= 120);
        for (auto* button : panel->findChildren<QPushButton*>())
            if (button->text() == "Clear completed") QVERIFY(!button->isVisible());
        const auto screenshots = qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR");
        if (!screenshots.isEmpty()) QVERIFY(panel->grab().save(QDir(screenshots).filePath("tasks-empty.png")));
        panel->close();
    }

    void embeddedSettingsSelectsTheSettingsRailItem()
    {
        QTemporaryDir temporary;
        auto globalSettings = settings(temporary);
        InstanceList instances(globalSettings.get(), temporary.filePath("instances"));
        WorkspacePage page(&instances);
        auto* settingsView = new QWidget;
        settingsView->setProperty("pxlNavigation", "workspaceSettings");
        QVERIFY(page.presentInstanceView(settingsView, "Settings", [] { return true; }));
        QVERIFY(page.findChild<QPushButton*>("workspaceSettings")->isChecked());
        QVERIFY(!page.findChild<QPushButton*>("workspaceHome")->isChecked());
        QVERIFY(!page.findChild<QPushButton*>("workspaceLibrary")->isChecked());
        QVERIFY(page.leaveInstanceView());
        QVERIFY(page.findChild<QPushButton*>("workspaceHome")->isChecked());
        QVERIFY(!page.findChild<QPushButton*>("workspaceSettings")->isChecked());
        // Both the old deferred view and the current view must be safe at page teardown.
        auto* secondView = new QWidget;
        QVERIFY(page.presentInstanceView(secondView, "Another view", [] { return true; }));
    }

    void embeddedCatalogEscapeReturnsWithoutHidingOrDiscarding()
    {
        QWidget surface;
        EmptyCatalogDialog dialog(&surface);
        dialog.useEmbeddedPresentation();
        auto* layout = new QVBoxLayout(&surface);
        layout->addWidget(&dialog);
        surface.resize(1000, 700);
        surface.show();
        QVERIFY(!dialog.isWindow());
        QVERIFY(!dialog.hasProviders());
        QSignalSpy backs(&dialog, &ResourceDownload::ResourceDownloadDialog::embeddedCloseRequested);
        QSignalSpy rejected(&dialog, &QDialog::rejected);
        QTest::keyClick(&dialog, Qt::Key_Escape);
        QCOMPARE(backs.count(), 1);
        QCOMPARE(rejected.count(), 0);
        QVERIFY(dialog.isVisible());
        QSignalSpy accepted(&dialog, &QDialog::accepted);
        dialog.accept();
        QCOMPARE(accepted.count(), 1);
    }

    void embeddedCatalogKeepsControlsCompactAndResultsTall()
    {
        QTemporaryDir temporary;
        auto globalSettings = settings(temporary);
        auto instanceSettings = std::make_unique<INISettingsObject>(temporary.filePath("instance.cfg"));
        MinecraftInstance instance(globalSettings.get(), std::move(instanceSettings), temporary.filePath("pack"));
        InstanceList instances(globalSettings.get(), temporary.filePath("instances"));
        WorkspacePage surface(&instances);
        auto* dialog = new LayoutCatalogDialog(&surface, instance);
        dialog->useEmbeddedPresentation();
        QVERIFY(surface.presentInstanceView(dialog, "Resource packs", [] { return true; }));
        auto* source = dialog->findChild<QWidget*>("catalogSourceRow");
        auto* page = dynamic_cast<LayoutCatalogPage*>(dialog->selectedPage());
        QVERIFY(page);
        auto* search = page->findChild<QLineEdit*>("searchEdit");
        auto* sort = page->findChild<QComboBox*>("catalogSortChoice");
        auto* results = page->findChild<QListView*>("packView");
        for (const auto size : {QSize(1080, 740), QSize(1280, 900)}) {
            surface.resize(size);
            surface.show();
            QCoreApplication::processEvents();
            QVERIFY(source->height() <= 48);
            QVERIFY(search->height() <= 48);
            QVERIFY(sort->height() <= 40);
            const int searchBottom = search->mapTo(dialog, QPoint(0, search->height())).y();
            const int sourceBottom = source->mapTo(dialog, QPoint(0, source->height())).y();
            const int searchTop = search->mapTo(dialog, QPoint()).y();
            QVERIFY2(searchTop - sourceBottom < 28, "Source and search must stay together at either window height");
            const int resultTop = results->mapTo(dialog, QPoint()).y();
            QVERIFY2(resultTop - searchBottom < 100, "Catalog controls must not consume the result area with blank bands");
            QVERIFY(results->height() > dialog->height() / 2);
            QCOMPARE(results->model()->rowCount(), 8);
            const auto screenshots = qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR");
            if (!screenshots.isEmpty()) QVERIFY(surface.grab().save(QDir(screenshots).filePath(QString("catalog-full-%1.png").arg(size.width()))));
        }
        sort->setCurrentIndex(1);
        QCOMPARE(page->findChild<QComboBox*>("sortByBox")->currentIndex(), 1);
        QCOMPARE(page->sortChanges, 1);
        dialog->deleteLater();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void contentCatalogExplainsPrerequisitesInsideTheInstance()
    {
        QTemporaryDir temporary;
        auto globalSettings = settings(temporary);
        auto instanceSettings = std::make_unique<INISettingsObject>(temporary.filePath("instance.cfg"));
        MinecraftInstance instance(globalSettings.get(), std::move(instanceSettings), temporary.filePath("pack"));
        instance.getPackProfile()->setComponentVersion("net.minecraft", "1.20.1");
        QWidget surface;
        auto* layout = new QVBoxLayout(&surface);
        ContentBrowser browser(&instance);
        layout->addWidget(&browser);
        surface.resize(1000, 700);
        surface.show();
        QSignalSpy setup(&browser, &ContentBrowser::setupRequested);
        QSignalSpy worlds(&browser, &ContentBrowser::worldsRequested);
        browser.browse("mods");
        auto* notice = browser.findChild<QWidget*>("contentPrerequisite");
        QVERIFY(notice && notice->isVisible());
        QVERIFY(!browser.isWindow());
        notice->findChild<QPushButton*>()->click();
        QCOMPARE(setup.count(), 1);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        browser.browse("shaderpacks");
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        notice = browser.findChild<QWidget*>("contentPrerequisite");
        QVERIFY(notice && notice->isVisible());
        QVERIFY(notice->findChild<QPushButton*>()->text().contains("renderer"));
        browser.browse("datapacks");
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        notice = browser.findChild<QWidget*>("contentPrerequisite");
        QVERIFY(notice && notice->isVisible());
        notice->findChild<QPushButton*>()->click();
        QCOMPARE(worlds.count(), 1);
        QCOMPARE(setup.count(), 1);
        QVERIFY(browser.prepareToLeave());
        // Stack-owned browser must be detached before the surface destroys children.
        browser.setParent(nullptr);
    }

    void emptyLibraryIsNavigableAndSafe()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        auto globalSettings = settings(temporary);
        InstanceList instances(globalSettings.get(), temporary.filePath("instances"));
        WorkspacePage page(&instances);
        auto* library = page.findChild<QPushButton*>("workspaceLibrary");
        auto* search = page.findChild<QLineEdit*>("workspaceSearch");
        auto* play = page.findChild<QPushButton*>("workspaceContinue");
        QVERIFY(library);
        QVERIFY(search);
        QVERIFY(play);
        QCOMPARE(page.findChild<QListView*>("workspaceRecent")->model()->rowCount(), 0);
        QVERIFY(!play->isEnabled());
        auto* emptyCreate = page.findChild<QPushButton*>("workspaceEmptyCreate");
        auto* emptyImport = page.findChild<QPushButton*>("workspaceEmptyImport");
        QVERIFY(emptyCreate && emptyImport);
        QVERIFY(emptyCreate->minimumHeight() >= 48);
        QVERIFY(page.findChild<QPushButton*>("workspaceHomeCreate")->isHidden());
        QSignalSpy creates(&page, &WorkspacePage::createRequested);
        QSignalSpy imports(&page, &WorkspacePage::importRequested);
        emptyCreate->click();
        emptyImport->click();
        QCOMPARE(creates.count(), 1);
        QCOMPARE(imports.count(), 1);

        const QString screenshotDirectory = qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR");
        if (!screenshotDirectory.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshotDirectory));
            page.resize(1080, 680);
            page.show();
            QCoreApplication::processEvents();
            QVERIFY(page.grab().save(QDir(screenshotDirectory).filePath("home.png")));
        }

        library->click();
        if (!screenshotDirectory.isEmpty()) {
            QCoreApplication::processEvents();
            QVERIFY(page.grab().save(QDir(screenshotDirectory).filePath("library.png")));
        }
        search->setText("missing");
        QVERIFY(search->text() == "missing");
        page.setSelectedInstance("does-not-exist");
        QSignalSpy activated(&page, &WorkspacePage::activateInstance);
        play->click();
        QCOMPARE(activated.count(), 0);
    }

    void navigationAndActionButtonsEmitRequests()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        auto globalSettings = settings(temporary);
        InstanceList instances(globalSettings.get(), temporary.filePath("instances"));
        WorkspacePage page(&instances);

        QSignalSpy create(&page, &WorkspacePage::createRequested);
        QSignalSpy import(&page, &WorkspacePage::importRequested);
        QSignalSpy exportRequest(&page, &WorkspacePage::exportRequested);
        QSignalSpy accounts(&page, &WorkspacePage::accountsRequested);
        QSignalSpy settingsRequest(&page, &WorkspacePage::settingsRequested);
        QSignalSpy classic(&page, &WorkspacePage::classicRequested);

        page.setNavigationPosition(false);
        QCOMPARE(page.findChild<QWidget*>("workspaceRail")->width(), 68);
        auto* library = page.findChild<QPushButton*>("workspaceLibrary");
        QVERIFY(library->text().isEmpty());
        QVERIFY(!library->accessibleName().isEmpty());
        QVERIFY(!library->toolTip().isEmpty());
        page.resize(1080, 820);
        page.setNavigationPosition(true);
        QVERIFY(!library->text().isEmpty());

        page.findChild<QPushButton*>("workspaceHomeCreate")->click();
        page.findChild<QPushButton*>("workspaceHomeImport")->click();
        page.findChild<QPushButton*>("workspaceHomeExport")->click();
        page.findChild<QPushButton*>("workspaceAccounts")->click();
        page.findChild<QPushButton*>("workspaceSettings")->click();
        page.findChild<QPushButton*>("workspaceClassic")->click();

        QCOMPARE(create.count(), 1);
        QCOMPARE(import.count(), 1);
        QCOMPARE(exportRequest.count(), 1);
        QCOMPARE(accounts.count(), 1);
        QCOMPARE(settingsRequest.count(), 1);
        QCOMPARE(classic.count(), 1);
    }

    void libraryModesKeepTheSameDataSelectionAndPlaytime()
    {
        QTemporaryDir temporary;
        auto globalSettings = settings(temporary);
        globalSettings->set("ShowGameTime", true);
        globalSettings->set("RecordGameTime", true);
        globalSettings->set("ShowGlobalGameTime", true);
        const auto root = temporary.filePath("instances");
        for (int i = 0; i < 2; ++i) {
            QVERIFY(QDir().mkpath(root + "/pack" + QString::number(i)));
            QFile config(root + "/pack" + QString::number(i) + "/instance.cfg");
            QVERIFY(config.open(QIODevice::WriteOnly));
            config.write(QString("[General]\nInstanceType=OneSix\nname=%1\niconKey=grass\ntotalTimePlayed=%2\n")
                .arg(i == 0 ? "First world" : "Second world").arg((i + 1) * 3600).toUtf8());
        }
        InstanceList instances(globalSettings.get(), root);
        instances.loadList();
        instances.setInstanceGroup("pack0", "Survival");
        WorkspacePage page(&instances, nullptr, globalSettings.get());
        page.resize(1080, 820);
        page.show();
        const auto heroBeforeSelection = page.findChild<QLabel*>("workspaceFeatureTitle")->text();
        page.setSelectedInstance("pack1");
        page.refresh();
        QCOMPARE(page.findChild<QLabel*>("workspaceFeatureTitle")->text(),heroBeforeSelection);
        page.findChild<QPushButton*>("workspaceLibrary")->click();
        page.setSelectedInstance("pack1");
        auto* view = page.findChild<QListView*>("workspaceCards");
        auto* model = view->model();
        QCOMPARE(model->rowCount(), 2);
        QCOMPARE(model->index(0, 0).data(InstanceList::GroupRole).toString(), QString("Survival"));
        auto* toggle = page.findChild<QComboBox*>("workspaceViewMode");
        QVERIFY(toggle);
        QCOMPARE(toggle->count(), 3);
        QCOMPARE(toggle->accessibleName(), QString("Library view"));
        QSignalSpy opened(&page, &WorkspacePage::openInstanceRequested);
        QSignalSpy launched(&page, &WorkspacePage::activateInstance);
        QSignalSpy menu(&page, &WorkspacePage::contextMenuRequested);
        page.setImageBackground(true);
        for (const auto& mode : {QString("covers"), QString("hybrid"), QString("list")}) {
            toggle->setCurrentIndex(toggle->findData(mode));
            QCoreApplication::processEvents();
            QCOMPARE(view->property("pxlLibraryMode").toString(), mode);
            QCOMPARE(view->model(), model);
            QCOMPARE(view->currentIndex().data(InstanceList::InstanceIDRole).toString(), QString("pack1"));
            QCOMPARE(globalSettings->get("WorkspaceLibraryView").toString(), mode);
            QVERIFY(page.findChild<QLabel*>("workspaceTotalTime")->text().startsWith("3 h"));
            auto* delegate = static_cast<Pxl::InstanceLibraryDelegate*>(view->itemDelegate());
            const auto card = view->visualRect(model->index(0,0)).adjusted(0,2,-4,-2);
            QVERIFY(card.height() <= (mode == "covers" ? 220 : mode == "hybrid" ? 140 : 74));
            const auto play = delegate->playRect(card);
            const auto more = delegate->moreRect(card);
            QVERIFY(card.contains(play));
            QVERIFY(!play.intersects(more));
            const int menus = menu.count();
            QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,more.center());
            QCOMPARE(menu.count(),menus+1);
            const int launches = launched.count();
            QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,play.center());
            QCOMPARE(launched.count(),launches + (instances.getInstanceById("pack0")->canLaunch() ? 1 : 0));
            const int opens = opened.count();
            if (mode != "list") {
                QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,card.topLeft()+QPoint(80,95));
                QCOMPARE(opened.count(),opens+1);
            }
            page.setSelectedInstance("pack1");
            const auto screenshots = qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR");
            if (!screenshots.isEmpty()) QVERIFY(page.grab().save(QDir(screenshots).filePath("library-"+mode+".png")));
            WorkspacePage restored(&instances,nullptr,globalSettings.get());
            QCOMPARE(restored.findChild<QListView*>("workspaceCards")->property("pxlLibraryMode").toString(),mode);
        }
        page.findChild<QLineEdit*>("workspaceSearch")->setText("Second");
        QCOMPARE(model->rowCount(),1);
        page.setLibraryPresentation("hybrid");
        QCOMPARE(model->rowCount(),1);
        QCOMPARE(view->currentIndex().data(InstanceList::InstanceIDRole).toString(),QString("pack1"));
        page.setLibraryPresentation("cards");
        QCOMPARE(toggle->currentData().toString(),QString("covers"));
    }

    void coverPickerPreviewsBeforeApplying()
    {
        QTemporaryDir temporary;
        QWidget surface;
        surface.resize(1080,740);
        surface.show();
        const auto root = temporary.filePath("pack");
        QVERIFY(QDir().mkpath(root));
        QTimer::singleShot(50,&surface,[&] {
            auto* panel = surface.findChild<QDialog*>("pxlCoverPicker");
            QVERIFY(panel);
            for (auto* button : panel->findChildren<QPushButton*>())
                if (button->text()=="Warm stone") button->click();
            QVERIFY(!QFileInfo::exists(Pxl::instanceCoverPath(root)));
            const auto screenshots = qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR");
            if (!screenshots.isEmpty()) QVERIFY(surface.grab().save(QDir(screenshots).filePath("cover-picker.png")));
            for (auto* button : panel->findChildren<QPushButton*>())
                if (button->text()=="Apply") button->click();
        });
        QVERIFY(Pxl::editInstanceCover(&surface,root));
        QVERIFY(!Pxl::instanceCover(root).isNull());
    }

    void individualCoversPreserveSourceAndPreviousImageOnFailure()
    {
        QTemporaryDir temporary;
        const auto root = temporary.filePath("pack");
        QVERIFY(QDir().mkpath(root));
        const auto source = temporary.filePath("source.png");
        QImage image(600,240,QImage::Format_RGB32);
        image.fill(QColor("#416d72"));
        QVERIFY(image.save(source));
        QString error;
        QVERIFY(Pxl::saveInstanceCover(root,source,&error));
        QVERIFY(!Pxl::instanceCover(root).isNull());
        QVERIFY(Pxl::instanceCover(temporary.filePath("other-pack")).isNull());
        QFile cover(Pxl::instanceCoverPath(root));
        QVERIFY(cover.open(QIODevice::ReadOnly));
        const auto previous = cover.readAll();
        cover.close();
        QVERIFY(!Pxl::saveInstanceCover(root,temporary.filePath("missing.png"),&error));
        QVERIFY(!error.isEmpty());
        QVERIFY(cover.open(QIODevice::ReadOnly));
        QCOMPARE(cover.readAll(),previous);
        cover.close();
        QVERIFY(Pxl::setInstanceCoverColor(root,QColor("#655147"),&error));
        QVERIFY(!Pxl::instanceCover(root).isNull());
        QVERIFY(Pxl::saveInstanceCover(root,":/pxl/cone-dawn-banner.png",&error));
        QVERIFY(Pxl::resetInstanceCover(root));
        QVERIFY(Pxl::instanceCover(root).isNull());
        QVERIFY(QFileInfo::exists(source));
        QCOMPARE(QImage(source).pixelColor(0,0),QColor("#416d72"));
    }

    void populatedLibraryFiltersSelectionAndLaunches()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        auto globalSettings = settings(temporary);
        const auto root = temporary.filePath("instances");
        const QStringList names{"Create Survival", "Vanilla 1.21.1", "Fabric Performance", "Creative Lab", "Adventure"};
        const QStringList icons{"gear", "grass", "enderpearl", "magitech", "netherstar"};
        for (int i = 0; i < names.size(); ++i) {
            QVERIFY(QDir().mkpath(root + "/pack" + QString::number(i)));
            QFile config(root + "/pack" + QString::number(i) + "/instance.cfg");
            QVERIFY(config.open(QIODevice::WriteOnly));
            config.write(QString("[General]\nInstanceType=OneSix\nname=%1\niconKey=%2\nlastLaunchTime=%3\n")
                             .arg(names[i], icons[i]).arg(i < 3 ? QDateTime::currentMSecsSinceEpoch() - (i + 1) * 3600000 : 0).toUtf8());
        }
        InstanceList instances(globalSettings.get(), root);
        instances.loadList();
        QCOMPARE(instances.rowCount(), 5);
        for (int i = 0; i < 5; ++i) {
            auto* instance = dynamic_cast<MinecraftInstance*>(instances.getInstanceById("pack" + QString::number(i)));
            QVERIFY(instance);
            instance->getPackProfile()->setComponentVersion("net.minecraft", "1.21.1");
            if (i != 1) instance->getPackProfile()->setComponentVersion("net.fabricmc.fabric-loader", "0.16.14");
        }
        instances.setInstanceGroup("pack0", "Survival");
        WorkspacePage page(&instances);
        page.resize(1080, 820);
        page.show();
        QCoreApplication::processEvents();
        auto* cards = page.findChild<QListView*>("workspaceCards");
        auto* search = page.findChild<QLineEdit*>("workspaceSearch");
        auto* play = page.findChild<QPushButton*>("workspaceContinue");
        auto* groups = page.findChild<QComboBox*>("workspaceGroups");
        QCOMPARE(cards->model()->rowCount(), 4); // Includes the two never-launched instances below the hero.
        QCOMPARE(page.findChild<QListView*>("workspaceRecent")->model()->rowCount(), 3);
        page.setSelectedInstance("pack0");
        QVERIFY(play->isEnabled());
        const QString screenshots = qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR");
        if (!screenshots.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshots));
            QCoreApplication::processEvents();
            QVERIFY(page.grab().save(QDir(screenshots).filePath("home-populated.png")));
            page.setImageBackground(true);
            QCoreApplication::processEvents();
            QVERIFY(page.grab().save(QDir(screenshots).filePath("home-wallpaper.png")));
            page.setNavigationPosition(false);
            QCoreApplication::processEvents();
            QVERIFY(page.grab().save(QDir(screenshots).filePath("home-wallpaper-side.png")));
            page.setNavigationPosition(true);
            page.setImageBackground(false);
        }
        QSignalSpy activated(&page, &WorkspacePage::activateInstance);
        play->click();
        QCOMPARE(activated.count(), 1);
        QCOMPARE(activated.takeFirst().first().toString(), QString("pack0"));
        page.findChild<QPushButton*>("workspaceLibrary")->click();
        QCOMPARE(cards->model()->rowCount(), 5);
        search->setText("Vanilla");
        QCOMPARE(cards->model()->rowCount(), 1);
        QVERIFY(!play->isEnabled());
        cards->setFocus();
        QTest::keyClick(cards, Qt::Key_Return, Qt::ControlModifier);
        QCOMPARE(activated.count(), 0);
        search->clear();
        page.setSelectedInstance("pack0");
        QVERIFY(!play->isEnabled());
        groups->setCurrentIndex(groups->findText("Survival"));
        QCOMPARE(cards->model()->rowCount(), 1);
        groups->setCurrentIndex(0);
        page.findChild<QPushButton*>("workspaceLibrary")->click();
        if (!screenshots.isEmpty()) {
            QCoreApplication::processEvents();
            QVERIFY(page.grab().save(QDir(screenshots).filePath("library-populated.png")));
            page.resize(780, 640);
            QCoreApplication::processEvents();
            QVERIFY(page.grab().save(QDir(screenshots).filePath("library-compact.png")));
        }
        page.setSelectedInstance("pack0");
        cards->setFocus();
        QSignalSpy opened(&page, &WorkspacePage::openInstanceRequested);
        QTest::keyClick(cards, Qt::Key_Return);
        QCOMPARE(opened.count(), 1);
        QCOMPARE(activated.count(), 0);
        QTest::keyClick(cards, Qt::Key_Return, Qt::ControlModifier);
        QCOMPARE(activated.count(), 1);
        page.findChild<QPushButton*>("workspaceHome")->click();
        QVERIFY(play->isEnabled());
        instances.getInstanceById("pack0")->setRunning(true);
        QVERIFY(play->isEnabled());
        QCOMPARE(play->text(), QString("Stop"));
        QSignalSpy stopped(&page, &WorkspacePage::stopInstance);
        play->click();
        QCOMPARE(stopped.count(), 1);
        QCOMPARE(stopped.takeFirst().first().toString(), QString("pack0"));
        QCOMPARE(activated.count(), 1);
        instances.getInstanceById("pack0")->setRunning(false);
        QCOMPARE(play->text(), QString("Play"));
    }

    void neverPlayedInstanceHasHomeFeature()
    {
        QTemporaryDir temporary;
        auto globalSettings = settings(temporary);
        const auto root = temporary.filePath("instances");
        QVERIFY(QDir().mkpath(root + "/new-pack"));
        QFile config(root + "/new-pack/instance.cfg");
        QVERIFY(config.open(QIODevice::WriteOnly));
        config.write("[General]\nInstanceType=OneSix\nname=Fresh survival\niconKey=grass\n");
        config.close();
        InstanceList instances(globalSettings.get(), root);
        instances.loadList();
        WorkspacePage page(&instances);
        page.resize(1080, 820);
        page.show();
        auto* play = page.findChild<QPushButton*>("workspaceContinue");
        QVERIFY(play->isVisible());
        QVERIFY(play->isEnabled());
        QVERIFY(page.findChild<QLineEdit*>("workspaceSearch")->isHidden());
        QCOMPARE(page.findChild<QListView*>("workspaceCards")->model()->rowCount(), 0);
        QSignalSpy activated(&page, &WorkspacePage::activateInstance);
        play->click();
        QCOMPARE(activated.takeFirst().first().toString(), QString("new-pack"));
        const QString screenshots = qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR");
        if (!screenshots.isEmpty()) {
            page.setImageBackground(true);
            QCoreApplication::processEvents();
            QVERIFY(page.grab().save(QDir(screenshots).filePath("home-never-played.png")));
        }
        page.findChild<QPushButton*>("workspaceLibrary")->click();
        QVERIFY(page.findChild<QLineEdit*>("workspaceSearch")->isVisible());
        QCOMPARE(page.findChild<QListView*>("workspaceCards")->model()->rowCount(), 1);
    }

    void russianCatalogAndLiveLanguageSwitch()
    {
        QTemporaryDir temporary;
        auto globalSettings = settings(temporary);
        InstanceList instances(globalSettings.get(), temporary.filePath("instances"));
        WorkspacePage page(&instances);
        QTranslator upstream;
        QVERIFY(upstream.load(":/pxl/translations/ru.qm"));
        POTranslator pxl(":/pxl/translations/pxl-ru.po");
        QVERIFY(!pxl.isEmpty());
        QVERIFY(qApp->installTranslator(&upstream));
        QVERIFY(qApp->installTranslator(&pxl));
        QCoreApplication::processEvents();
        QCOMPARE(page.findChild<QLineEdit*>("workspaceSearch")->placeholderText(), QString::fromUtf8("Поиск сборок"));
        QCOMPARE(page.findChild<QPushButton*>("workspaceLibrary")->toolTip(), QString::fromUtf8("Библиотека"));
        const QString screenshots = qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR");
        if (!screenshots.isEmpty()) {
            page.resize(1080, 820);
            page.show();
            page.setImageBackground(true);
            QCoreApplication::processEvents();
            QTest::qWait(180);
            QVERIFY(page.grab().save(QDir(screenshots).filePath("home-russian-empty.png")));
        }
        qApp->removeTranslator(&pxl);
        qApp->removeTranslator(&upstream);
        QCoreApplication::processEvents();
        QCOMPARE(page.findChild<QLineEdit*>("workspaceSearch")->placeholderText(), QString("Search instances"));
    }

    void progressDetailsAndCancellationPreserveTaskBehavior()
    {
        ProgressDialog dialog;
        auto* bar = dialog.findChild<QProgressBar*>("globalProgressBar");
        QVERIFY(bar);
        dialog.changeProgress(0, 0);
        QCOMPARE(bar->maximum(), 0);
        dialog.changeProgress(500, 100);
        QCOMPARE(bar->value(), bar->maximum());
        dialog.setSkipButton(true, "Cancel");
        PreviewInstallTask task;
        QTimer::singleShot(50, &dialog, [&] {
            auto* details = dialog.findChild<QScrollArea*>("taskProgressScrollArea");
            QVERIFY(details->isHidden());
            QPushButton* toggle = nullptr;
            for (auto* button : dialog.findChildren<QPushButton*>())
                if (button->isCheckable()) toggle = button;
            QVERIFY(toggle);
            const QString screenshots = qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR");
            if (!screenshots.isEmpty()) QVERIFY(dialog.grab().save(QDir(screenshots).filePath("task-progress.png")));
            toggle->click();
            QVERIFY(details->isVisible());
            if (!screenshots.isEmpty()) QVERIFY(dialog.grab().save(QDir(screenshots).filePath("task-details.png")));
        });
        QCOMPARE(dialog.execWithTask(&task), int(QDialog::Accepted));
        QVERIFY(task.wasSuccessful());

        ProgressDialog cancelled;
        cancelled.setSkipButton(true, "Cancel");
        PreviewInstallTask cancelledTask;
        QTimer::singleShot(20, &cancelled, [&] { cancelled.findChild<QPushButton*>("skipButton")->click(); });
        QCOMPARE(cancelled.execWithTask(&cancelledTask), int(QDialog::Rejected));
        QCOMPARE(cancelledTask.getState(), Task::State::AbortedByUser);
    }

    void taskHistoryTracksCompletionCancellationAndDestroyedTasks()
    {
        Pxl::TaskHistory history;
        PreviewInstallTask task;
        task.setProperty("pxlTaskTitle", "Adventure pack");
        history.watch(&task, "Fallback");
        history.watch(&task, "Duplicate");
        QCOMPARE(history.records().size(), 1);
        QCOMPARE(history.records().first().title, QString("Adventure pack"));
        QVERIFY(!history.cancel(history.records().first().id));
        task.start();
        QCOMPARE(history.activeCount(), 1);
        QWidget owner;
        history.showTasks(&owner);
        QTest::qWait(20);
        auto* dialog = owner.findChild<QDialog*>();
        QVERIFY(dialog);
        const QString screenshots = qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR");
        if (!screenshots.isEmpty()) QVERIFY(dialog->grab().save(QDir(screenshots).filePath("task-center-active.png")));
        QTRY_VERIFY(task.wasSuccessful());
        QCOMPARE(history.activeCount(), 0);
        QCOMPARE(history.records().first().state, Task::State::Succeeded);
        QTest::qWait(120);
        if (!screenshots.isEmpty()) QVERIFY(dialog->grab().save(QDir(screenshots).filePath("task-center-complete.png")));

        PreviewInstallTask cancelled;
        history.watch(&cancelled, "Cancelled pack");
        cancelled.start();
        QVERIFY(history.cancel(history.records().last().id));
        QCOMPARE(history.records().last().state, Task::State::AbortedByUser);
        history.clearFinished();
        QCOMPARE(history.records().size(), 0);
        auto* interrupted = new PreviewInstallTask;
        history.watch(interrupted, "Interrupted");
        delete interrupted;
        QCOMPARE(history.records().first().state, Task::State::Failed);
        QVERIFY(history.records().first().task.isNull());
        QVERIFY(!history.cancel(history.records().first().id));
        dialog->close();
    }

    void popupSelectionKeepsReadableText()
    {
        QComboBox combo;
        combo.addItems({"Simple (Blue)", "Simple (Colored)", "Simple (Dark)"});
        combo.resize(320, 44);
        combo.show();
        combo.showPopup();
        auto* view = combo.view();
        view->setCurrentIndex(view->model()->index(0, 0));
        QCoreApplication::processEvents();
        auto luminance = [](const QColor& color) {
            auto linear = [](double component) { return component <= 0.04045 ? component / 12.92 : std::pow((component + 0.055) / 1.055, 2.4); };
            return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
        };
        const auto foreground = luminance(view->palette().color(QPalette::HighlightedText));
        const auto background = luminance(view->palette().color(QPalette::Highlight));
        const auto contrast = (qMax(foreground, background) + 0.05) / (qMin(foreground, background) + 0.05);
        QVERIFY2(contrast >= 4.5, qPrintable(QString("Selected popup text contrast is %1").arg(contrast)));
        const auto hovered = view->visualRect(view->model()->index(0, 0)).center();
        QTest::mouseMove(view->viewport(), hovered);
        QTest::qWait(30);
        const QString screenshots = qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR");
        if (!screenshots.isEmpty()) QVERIFY(view->window()->grab().save(QDir(screenshots).filePath("popup-hover.png")));
        combo.hidePopup();
    }

    void instanceContentStaysInShellAndRespectsSaveGuard()
    {
        QTemporaryDir temporary;
        auto globalSettings = settings(temporary);
        InstanceList instances(globalSettings.get(), temporary.filePath("instances"));
        WorkspacePage page(&instances);
        page.resize(1080, 820);
        page.show();
        auto* editor = new QWidget;
        bool canLeave = false;
        int saveAttempts = 0;
        QVERIFY(page.presentInstanceView(editor, "Adventure pack", [&] { ++saveAttempts; return canLeave; }));
        QVERIFY(editor->isVisible());
        QVERIFY(page.findChild<QWidget*>("workspaceRail")->isVisible());
        QVERIFY(page.findChild<QPushButton*>("workspaceBack")->isVisible());
        page.findChild<QPushButton*>("workspaceLibrary")->click();
        QVERIFY(editor->isVisible());
        QVERIFY(saveAttempts > 0);
        canLeave = true;
        page.findChild<QPushButton*>("workspaceLibrary")->click();
        QVERIFY(editor->isHidden());
        QCOMPARE(page.findChild<QLabel*>("workspacePageTitle")->text(), QString("Library"));
        QVERIFY(page.presentInstanceView(editor, "Adventure pack", [] { return true; }));
        delete editor;
        QCoreApplication::processEvents();
        QVERIFY(page.findChild<QPushButton*>("workspaceBack")->isHidden());
        QCOMPARE(page.findChild<QLabel*>("workspacePageTitle")->text(), QString("Library"));
    }

    void historyCapsCompletedEntriesWithoutHidingActiveTasks()
    {
        Pxl::TaskHistory history;
        std::vector<std::unique_ptr<ManualHistoryTask>> tasks;
        for (int i = 0; i < 70; ++i) {
            auto task = std::make_unique<ManualHistoryTask>();
            history.watch(task.get(), QString::number(i));
            task->start();
            tasks.push_back(std::move(task));
        }
        QCOMPARE(history.activeCount(), 70);
        QCOMPARE(history.records().size(), 70);
        for (const auto& task : tasks) task->complete();
        QCOMPARE(history.activeCount(), 0);
        QCOMPARE(history.records().size(), 50);
        ManualHistoryTask finished;
        finished.start();
        finished.complete();
        history.watch(&finished, "Already finished");
        QCOMPARE(history.activeCount(), 0);
        QMetaObject::invokeMethod(&finished, "started", Qt::QueuedConnection);
        QCoreApplication::processEvents();
        QCOMPARE(history.activeCount(), 0);
        QCOMPARE(history.records().last().state, Task::State::Succeeded);
    }
    void creationOverlayIsContainedAndRestoresBackground()
    {
        QWidget surface;
        surface.resize(900, 760);
        QPushButton background("Library", &surface);
        background.show();
        surface.show();
        QDialog dialog(&surface);
        dialog.setStyleSheet("QDialog { background: transparent; }");
        dialog.setProperty("pxlPreferredSize", QSize(560, 490));
        bool child = false, blocked = false, contained = false, painted = false;
        QTimer::singleShot(20, &dialog, [&] {
            child = !dialog.isWindow() && dialog.parentWidget()->objectName() == "pxlDialogOverlay";
            blocked = !background.isEnabled();
            surface.resize(500, 600);
            contained = dialog.parentWidget()->rect().contains(dialog.geometry());
            const auto image = dialog.parentWidget()->grab().toImage();
            const auto point = dialog.geometry().topLeft() + QPoint(24, 24);
            const auto color = image.pixelColor(point);
            painted = color.alpha() == 255 && color.lightness() >= 18 && color.lightness() <= 65;
            const auto screenshots = qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR");
            if (!screenshots.isEmpty()) image.save(QDir(screenshots).filePath("overlay-glass.png"));
            dialog.reject();
        });
        QCOMPARE(Pxl::DialogOverlay::run(&dialog, &surface), int(QDialog::Rejected));
        QVERIFY(child);
        QVERIFY(blocked);
        QVERIFY(contained);
        QVERIFY(painted);
        QVERIFY(background.isEnabled());
        QCOMPARE(dialog.parentWidget(), &surface);
        QVERIFY(dialog.isHidden());
    }

    void compactVersionPopupStaysBounded()
    {
        VersionSelectWidget picker(nullptr);
        picker.useCompactPresentation();
        auto* combo = picker.findChild<QComboBox*>("compactVersionChoice");
        QVERIFY(combo);
        QStringList versions;
        for (int index = 0; index < 200; ++index) versions.append(QString("1.%1").arg(index));
        QStringListModel model(versions);
        combo->setModel(&model);
        combo->setEnabled(true);
        combo->setCurrentIndex(3);
        QWheelEvent wheel(QPointF(10, 10), QPointF(10, 10), QPoint(), QPoint(0, 120), Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(combo, &wheel);
        QCOMPARE(combo->currentIndex(), 3);
        picker.resize(500, 50);
        picker.show();
        combo->showPopup();
        QTest::qWait(30);
        QVERIFY(combo->view()->isVisible());
        QVERIFY(combo->view()->height() <= 280);
        QVERIFY(combo->view()->window()->height() <= 320);
        combo->hidePopup();
    }

    void creationOverlaySurvivesSurfaceDestruction()
    {
        auto* surface = new QWidget;
        surface->resize(900, 760);
        surface->show();
        auto* dialog = new QDialog(surface);
        QPointer<QDialog> guarded(dialog);
        QTimer::singleShot(20, dialog, [surface] { delete surface; });
        QCOMPARE(Pxl::DialogOverlay::run(dialog, surface), int(QDialog::Rejected));
        QVERIFY(guarded);
        QVERIFY(!guarded->parentWidget());
        delete guarded.data();
    }

    void customBackgroundAndMissingFileFallback()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QImage image(320, 200, QImage::Format_RGB32);
        image.fill(QColor("#a04127"));
        const auto path = directory.filePath("wallpaper.png");
        QVERIFY(image.save(path));
        QVERIFY(!Pxl::loadBackdropImage(path).isNull());
        const auto custom = Pxl::blurredBackdrop(path);
        QVERIFY(!custom.isNull());
        QCOMPARE(Pxl::blurredBackdrop(path).cacheKey(), custom.cacheKey());
        QVERIFY(Pxl::loadBackdropImage(directory.filePath("missing.png")).isNull());
        QVERIFY(!Pxl::blurredBackdrop(directory.filePath("missing.png")).isNull());
    }

    void emptyTaskHistoryIsCompactPopup()
    {
        QWidget surface;
        surface.resize(900, 760);
        QPushButton anchor("Tasks", &surface);
        anchor.setGeometry(20, 100, 42, 42);
        surface.show();
        auto* history = Pxl::TaskHistory::instance();
        history->clearFinished();
        history->showTasks(&anchor);
        auto* popup = anchor.findChild<QDialog*>("pxlTaskPopover");
        QVERIFY(popup);
        QCOMPARE(popup->windowType(), Qt::Popup);
        QVERIFY(popup->width() <= 420);
        QVERIFY(popup->height() <= 200);
        const auto surfacePixel = popup->grab().toImage().pixelColor(popup->width() - 20, popup->height() - 20);
        QVERIFY(surfacePixel.alpha() >= 205); // A captured, blurred app backdrop may make the final image opaque.
        QVERIFY(surfacePixel.lightness() < 90);
        QVERIFY(surfacePixel.blue() >= surfacePixel.red());
        popup->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void embeddedSettingsValidateAndProtectUnsavedChanges()
    {
        QWidget surface;
        surface.resize(1000, 800);
        surface.show();
        GenericPageProvider provider("Settings");
        PreviewSettingsPage* fields = nullptr;
        provider.addPageCreator([&] { fields = new PreviewSettingsPage; return fields; });
        auto* page = new PageDialog(&provider, {}, &surface, true);
        page->resize(950, 750);
        page->show();
        QVERIFY(!page->isWindow());
        QSignalSpy applied(page, &PageDialog::applied);
        fields->valid = false;
        QVERIFY(!page->applyChanges());
        QCOMPARE(applied.count(), 0);
        fields->valid = true;
        QVERIFY(page->applyChanges());
        QCOMPARE(applied.count(), 1);
        auto* container = page->findChild<PageContainer*>();
        container->setSearchText("memory allocation");
        auto* categories = container->findChild<QListView*>("conePageNavigation");
        QCOMPARE(categories->model()->rowCount(), 1);
        container->setSearchText("no matching setting");
        QCOMPARE(categories->model()->rowCount(), 0);
        container->setSearchText({});
        fields->slider->setValue(10);
        QTimer::singleShot(20, page, [page] {
            if (auto* question = page->findChild<QMessageBox*>()) question->reject();
        });
        QVERIFY(!page->requestLeave());
        QCOMPARE(fields->slider->value(), 10);
        fields->edit->setText("changed");
        QTimer::singleShot(20, page, [page] {
            if (auto* question = page->findChild<QMessageBox*>()) question->button(QMessageBox::Discard)->click();
        });
        QVERIFY(page->requestLeave());
        QCOMPARE(applied.count(), 1);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void importPreviewUsesSourceAndRevealsResults()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto source = directory.filePath("launcher");
        const auto instance = source + "/instances/demo";
        QVERIFY(QDir().mkpath(instance + "/minecraft"));
        auto write = [](const QString& path, const QByteArray& data) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
        };
        QVERIFY(write(instance + "/instance.cfg", "[General]\nInstanceType=OneSix\nname=Preview fixture\n"));
        QVERIFY(write(instance + "/mmc-pack.json", "{\"formatVersion\":1,\"components\":[{\"uid\":\"net.minecraft\",\"version\":\"1.20.1\"}]}"));
        Pxl::TransferDialog dialog(Pxl::TransferDialog::Import);
        auto* list = dialog.findChild<QListWidget*>("transferInstances");
        auto* run = dialog.findChild<QPushButton*>("transferRun");
        QVERIFY(list && run);
        QVERIFY(list->isHidden());
        QVERIFY(!run->isEnabled());
        auto* path = dialog.findChild<QLineEdit*>();
        QVERIFY(path);
        path->setText(source);
        QTest::keyClick(path, Qt::Key_Return);
        QCOMPARE(list->count(), 1);
        QVERIFY(!list->isHidden());
        QVERIFY(run->isEnabled());
        QVERIFY(list->item(0)->text().contains("Preview fixture"));
        path->setText(directory.filePath("different"));
        QVERIFY(list->isHidden());
        QVERIFY(!run->isEnabled());
    }
};

QTEST_MAIN(WorkspacePageTest)
#include "WorkspacePage_test.moc"
