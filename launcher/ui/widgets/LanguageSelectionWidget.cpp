#include "LanguageSelectionWidget.h"

#include <QCheckBox>
#include <QHeaderView>
#include <QLabel>
#include <QTreeView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include "ui/pxl/SettingsCards.h"
#include "Application.h"
#include "settings/SettingsObject.h"
#include "BuildConfig.h"
#include "settings/Setting.h"
#include "translations/TranslationsModel.h"

LanguageSelectionWidget::LanguageSelectionWidget(QWidget* parent) : QWidget(parent)
{
    verticalLayout = new QVBoxLayout(this);
    verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
    languageView = new QTreeView(this);
    languageView->setObjectName(QStringLiteral("languageView"));
    languageView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    languageView->setAlternatingRowColors(true);
    languageView->setRootIsDecorated(false);
    languageView->setItemsExpandable(false);
    languageView->setWordWrap(true);
    languageView->header()->setCascadingSectionResizes(true);
    languageView->header()->setStretchLastSection(false);
    verticalLayout->addWidget(languageView);
    helpUsLabel = new QLabel(this);
    helpUsLabel->setObjectName(QStringLiteral("helpUsLabel"));
    helpUsLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    helpUsLabel->setOpenExternalLinks(true);
    helpUsLabel->setWordWrap(true);
    verticalLayout->addWidget(helpUsLabel);

    formatCheckbox = new QCheckBox(this);
    formatCheckbox->setObjectName(QStringLiteral("formatCheckbox"));
    formatCheckbox->setCheckState(APPLICATION->settings()->get("UseSystemLocale").toBool() ? Qt::Checked : Qt::Unchecked);
    connect(formatCheckbox, &QCheckBox::stateChanged,
            [this]() { APPLICATION->translations()->setUseSystemLocale(formatCheckbox->isChecked()); });
    verticalLayout->addWidget(formatCheckbox);

    auto translations = APPLICATION->translations();
    auto index = translations->selectedIndex();
    languageView->setModel(translations);
    languageView->setCurrentIndex(index);
    languageView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    languageView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    connect(languageView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &LanguageSelectionWidget::languageRowChanged);
    verticalLayout->setContentsMargins(0, 0, 0, 0);

    auto language_setting = APPLICATION->settings()->getSetting("Language");
    connect(language_setting.get(), &Setting::SettingChanged, this, &LanguageSelectionWidget::languageSettingChanged);
    languageView->hide();
    helpUsLabel->hide();
    verticalLayout->removeWidget(formatCheckbox);
    auto* card = Pxl::settingsCard(this, tr("Interface language"), tr("Choose the language used by PXL Cone."));
    auto* choices = static_cast<QVBoxLayout*>(card->layout());
    auto* languages = new QHBoxLayout;
    choices->addLayout(languages);
    for (const auto& entry : QList<QPair<QString, QString>>{{"en_US", "English"}, {"ru", QString::fromUtf8("Русский")}}) {
        auto* button = new QPushButton(entry.second, card);
        button->setCheckable(true);
        button->setAutoExclusive(true);
        button->setProperty("pxlMode", true);
        button->setMinimumHeight(40);
        languages->addWidget(button);
        auto sync = [this, button, key = entry.first] {
            button->setChecked(getSelectedLanguageKey() == key);
        };
        connect(button, &QPushButton::clicked, this, [this, translations, key = entry.first] {
            for (int row = 0; row < translations->rowCount(); ++row) {
                const auto choice = translations->index(row, 0);
                if (translations->data(choice, Qt::UserRole).toString() == key) {
                    languageView->setCurrentIndex(choice);
                    break;
                }
            }
        });
        connect(languageView->selectionModel(), &QItemSelectionModel::currentRowChanged, button, [sync] { sync(); });
        sync();
    }
    choices->addWidget(formatCheckbox);
    formatCheckbox->show();
    verticalLayout->addWidget(card);
    verticalLayout->addStretch();
}

QString LanguageSelectionWidget::getSelectedLanguageKey() const
{
    auto translations = APPLICATION->translations();
    return translations->data(languageView->currentIndex(), Qt::UserRole).toString();
}

void LanguageSelectionWidget::retranslate()
{
    QString text = tr("Don't see your language or the quality is poor?<br/><a href=\"%1\">Help us with translations!</a>")
                       .arg(BuildConfig.TRANSLATIONS_URL);
    helpUsLabel->setText(text);
    formatCheckbox->setText(tr("Use system regional standards"));
}

void LanguageSelectionWidget::languageRowChanged(const QModelIndex& current, const QModelIndex& previous)
{
    if (current == previous) {
        return;
    }
    auto translations = APPLICATION->translations();
    QString key = translations->data(current, Qt::UserRole).toString();
    translations->selectLanguage(key);
    translations->updateLanguage(key);
}

void LanguageSelectionWidget::languageSettingChanged(const Setting&, const QVariant&)
{
    auto translations = APPLICATION->translations();
    auto index = translations->selectedIndex();
    languageView->setCurrentIndex(index);
}
