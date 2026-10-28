// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "DialogOverlay.h"
#include "ToggleSwitch.h"
#include "updater/ReleaseUpdateChecker.h"
#include <QCheckBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace Pxl {
class ReleaseNotesDocument final : public QTextDocument {
public:
    using QTextDocument::QTextDocument;
protected:
    QVariant loadResource(int, const QUrl&) override { return {}; }
};

class ReleaseNotesBrowser final : public QTextBrowser {
public:
    explicit ReleaseNotesBrowser(QWidget* parent = nullptr) : QTextBrowser(parent)
    {
        // QTextDocument's default loader can fall back to local files after the
        // browser refuses a resource. Stop resolution at the document too.
        setDocument(new ReleaseNotesDocument(this));
    }
protected:
    // Release text is remote input. Never resolve images/local file resources.
    QVariant loadResource(int, const QUrl&) override { return {}; }
};

class LauncherUpdateDialog final : public QDialog {
public:
    explicit LauncherUpdateDialog(ReleaseUpdateChecker* checker, QWidget* parent = nullptr) : QDialog(parent)
    {
        setObjectName("pxlLauncherUpdates");
        resize(640, 560);
        setProperty("pxlPreferredSize", QSize(640,560));
        setStyleSheet("QDialog { background:#23262b; color:#edf0f4; border-radius:16px; }"
                      "QLabel { background:transparent; color:#edf0f4; }"
                      "QTextBrowser { background:rgba(16,20,24,120); color:#edf0f4; border:1px solid #393e46; border-radius:12px; padding:16px; }"
                      "QPushButton { background:rgba(255,255,255,16); color:#edf0f4; border:1px solid #41464d; border-radius:10px; padding:9px 16px; font-weight:600; }"
                      "QPushButton:hover { background:rgba(242,163,101,40); border-color:#b17b50; }"
                      "QPushButton#releaseDownload { background:#f2a365; color:#201a15; border:none; }"
                      "QPushButton:disabled { color:#9199a5; background:rgba(255,255,255,8); }");
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(24,24,24,24);
        layout->setSpacing(16);
        auto* header = new QHBoxLayout;
        auto* title = new QLabel(ReleaseUpdateChecker::tr("Launcher updates"), this);
        title->setStyleSheet("font-size:22px; font-weight:700;");
        header->addWidget(title,1);
        auto* close = new QPushButton(ReleaseUpdateChecker::tr("Close"),this);
        connect(close,&QPushButton::clicked,this,&QDialog::reject);
        header->addWidget(close);
        layout->addLayout(header);
        auto* status = new QLabel(this);
        status->setTextFormat(Qt::PlainText);
        status->setWordWrap(true);
        layout->addWidget(status);
        auto* notes = new ReleaseNotesBrowser(this);
        notes->setObjectName("releaseNotes");
        notes->setOpenLinks(false);
        notes->setOpenExternalLinks(false);
        notes->document()->setDefaultStyleSheet("a { color:#f2a365; } pre, code { background:#191d22; } h1,h2,h3 { color:#edf0f4; }");
        connect(notes,&QTextBrowser::anchorClicked,this,[](const QUrl& url) {
            if (url.scheme() == "https" || url.scheme() == "http") QDesktopServices::openUrl(url);
        });
        layout->addWidget(notes,1);
        auto* automatic = new ToggleSwitch(ReleaseUpdateChecker::tr("Check automatically"),this);
        automatic->setChecked(checker->getAutomaticallyChecksForUpdates());
        connect(automatic,&QCheckBox::toggled,checker,&ReleaseUpdateChecker::setAutomaticallyChecksForUpdates);
        layout->addWidget(automatic);
        auto* footer = new QHBoxLayout;
        auto* check = new QPushButton(ReleaseUpdateChecker::tr("Check for updates"),this);
        connect(check,&QPushButton::clicked,checker,&ReleaseUpdateChecker::checkForUpdates);
        footer->addWidget(check);
        footer->addStretch();
        auto* download = new QPushButton(ReleaseUpdateChecker::tr("Open release downloads"),this);
        download->setObjectName("releaseDownload");
        footer->addWidget(download);
        layout->addLayout(footer);
        connect(download,&QPushButton::clicked,checker,[checker] {
            const auto url = checker->release().url;
            if (url.scheme() == "https" && url.host() == "github.com") QDesktopServices::openUrl(url);
        });
        auto refresh = [checker,status,notes,check,download] {
            const auto release = checker->release();
            QString message;
            if (release.isAvailable()) message = ReleaseUpdateChecker::tr("Update available: %1 · Installed: %2").arg(release.tag,checker->currentVersionLabel());
            else if (checker->checking()) message = ReleaseUpdateChecker::tr("Checking for updates…");
            else if (checker->error().isEmpty()) message = ReleaseUpdateChecker::tr("You have the latest version: %1").arg(checker->currentVersionLabel());
            if (!checker->error().isEmpty()) message += (message.isEmpty() ? QString() : "\n") + checker->error();
            status->setText(message);
            notes->document()->setMarkdown(release.notes.isEmpty() ? ReleaseUpdateChecker::tr("Release notes will appear here when a new version is available.") : release.notes,
                               QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub) | QTextDocument::MarkdownNoHTML);
            check->setEnabled(!checker->checking());
            download->setVisible(release.isAvailable());
        };
        connect(checker,&ReleaseUpdateChecker::stateChanged,this,refresh);
        connect(checker,&QObject::destroyed,this,&QDialog::reject);
        refresh();
    }
};
}
