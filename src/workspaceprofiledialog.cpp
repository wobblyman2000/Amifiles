#include "workspaceprofiledialog.h"
#include "mainwindow.h"
#include "filepanel.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QSettings>

WorkspaceProfileDialog::WorkspaceProfileDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Workspace Session Manager");
    resize(400, 220);
    setupUI();
    loadProfilesList();
}

void WorkspaceProfileDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; font-size: 13px; }"
        "QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px; }"
        "QComboBox { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 4px; }"
        "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #45475a; }"
        "QPushButton#btnLoad { background-color: #a6e3a1; color: #11111b; }"
        "QPushButton#btnLoad:hover { background-color: #b4befe; }"
        "QPushButton#btnSave { background-color: #89b4fa; color: #11111b; }"
        "QPushButton#btnSave:hover { background-color: #b4befe; }"
    );

    QFormLayout* form = new QFormLayout();
    form->setSpacing(10);

    m_profilesCombo = new QComboBox(this);
    form->addRow("Saved Profiles:", m_profilesCombo);

    m_profileNameEdit = new QLineEdit(this);
    m_profileNameEdit->setPlaceholderText("Enter profile name to save...");
    form->addRow("New Profile:", m_profileNameEdit);

    mainLayout->addLayout(form);

    // Button controls layout
    QHBoxLayout* actionsLayout = new QHBoxLayout();
    
    QPushButton* btnDelete = new QPushButton("Delete", this);
    connect(btnDelete, &QPushButton::clicked, this, &WorkspaceProfileDialog::onDeleteClicked);
    actionsLayout->addWidget(btnDelete);

    actionsLayout->addStretch();

    QPushButton* btnCancel = new QPushButton("Cancel", this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    actionsLayout->addWidget(btnCancel);

    QPushButton* btnSave = new QPushButton("Save", this);
    btnSave->setObjectName("btnSave");
    connect(btnSave, &QPushButton::clicked, this, &WorkspaceProfileDialog::onSaveClicked);
    actionsLayout->addWidget(btnSave);

    QPushButton* btnLoad = new QPushButton("Load", this);
    btnLoad->setObjectName("btnLoad");
    connect(btnLoad, &QPushButton::clicked, this, &WorkspaceProfileDialog::onLoadClicked);
    actionsLayout->addWidget(btnLoad);

    mainLayout->addLayout(actionsLayout);
}

void WorkspaceProfileDialog::loadProfilesList() {
    m_profilesCombo->clear();
    QSettings settings("Amifiles", "Amifiles");
    QStringList profiles = settings.value("workspace_profiles/profile_list").toStringList();
    m_profilesCombo->addItems(profiles);
}

void WorkspaceProfileDialog::onSaveClicked() {
    QString name = m_profileNameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, "Save Workspace", "Please enter a profile name first.");
        return;
    }

    MainWindow* mw = qobject_cast<MainWindow*>(parent());
    if (!mw) {
        reject();
        return;
    }

    QSettings settings("Amifiles", "Amifiles");
    
    // Save left pane tabs details
    QStringList leftPaths;
    for (int i = 0; i < mw->m_leftTabWidget->count(); ++i) {
        FilePanel* fp = qobject_cast<FilePanel*>(mw->m_leftTabWidget->widget(i));
        if (fp) leftPaths.append(fp->currentPath());
    }
    int leftActive = mw->m_leftTabWidget->currentIndex();
    int leftViewMode = mw->leftPanel() ? mw->leftPanel()->viewModeIndex() : 0;

    // Save right pane tabs details
    QStringList rightPaths;
    for (int i = 0; i < mw->m_rightTabWidget->count(); ++i) {
        FilePanel* fp = qobject_cast<FilePanel*>(mw->m_rightTabWidget->widget(i));
        if (fp) rightPaths.append(fp->currentPath());
    }
    int rightActive = mw->m_rightTabWidget->currentIndex();
    int rightViewMode = mw->rightPanel() ? mw->rightPanel()->viewModeIndex() : 0;

    bool rightVisible = mw->m_rightTabWidget->isVisible();
    bool sidebarVisible = mw->m_sidebarTabWidget->isVisible();
    bool consoleVisible = mw->m_bottomTabWidget->isVisible();

    // Serialize keys
    settings.setValue(QString("workspace_profiles/%1/left_paths").arg(name), leftPaths);
    settings.setValue(QString("workspace_profiles/%1/left_active").arg(name), leftActive);
    settings.setValue(QString("workspace_profiles/%1/left_view_mode").arg(name), leftViewMode);
    
    settings.setValue(QString("workspace_profiles/%1/right_paths").arg(name), rightPaths);
    settings.setValue(QString("workspace_profiles/%1/right_active").arg(name), rightActive);
    settings.setValue(QString("workspace_profiles/%1/right_view_mode").arg(name), rightViewMode);

    settings.setValue(QString("workspace_profiles/%1/right_visible").arg(name), rightVisible);
    settings.setValue(QString("workspace_profiles/%1/sidebar_visible").arg(name), sidebarVisible);
    settings.setValue(QString("workspace_profiles/%1/console_visible").arg(name), consoleVisible);

    // Save to profiles index list
    QStringList profiles = settings.value("workspace_profiles/profile_list").toStringList();
    if (!profiles.contains(name)) {
        profiles.append(name);
        settings.setValue("workspace_profiles/profile_list", profiles);
    }

    QMessageBox::information(this, "Save Workspace", QString("Workspace profile '%1' saved successfully!").arg(name));
    loadProfilesList();
    m_profileNameEdit->clear();
}

void WorkspaceProfileDialog::onLoadClicked() {
    QString name = m_profilesCombo->currentText();
    if (name.isEmpty()) {
        QMessageBox::warning(this, "Load Workspace", "No workspace profile selected.");
        return;
    }

    MainWindow* mw = qobject_cast<MainWindow*>(parent());
    if (!mw) {
        reject();
        return;
    }

    QSettings settings("Amifiles", "Amifiles");
    QStringList leftPaths = settings.value(QString("workspace_profiles/%1/left_paths").arg(name)).toStringList();
    int leftActive = settings.value(QString("workspace_profiles/%1/left_active").arg(name), 0).toInt();
    int leftViewMode = settings.value(QString("workspace_profiles/%1/left_view_mode").arg(name), 0).toInt();

    QStringList rightPaths = settings.value(QString("workspace_profiles/%1/right_paths").arg(name)).toStringList();
    int rightActive = settings.value(QString("workspace_profiles/%1/right_active").arg(name), 0).toInt();
    int rightViewMode = settings.value(QString("workspace_profiles/%1/right_view_mode").arg(name), 0).toInt();

    bool rightVisible = settings.value(QString("workspace_profiles/%1/right_visible").arg(name), true).toBool();
    bool sidebarVisible = settings.value(QString("workspace_profiles/%1/sidebar_visible").arg(name), true).toBool();
    bool consoleVisible = settings.value(QString("workspace_profiles/%1/console_visible").arg(name), true).toBool();

    // Toggle container visibilities
    mw->m_rightTabWidget->setVisible(rightVisible);
    mw->m_sidebarTabWidget->setVisible(sidebarVisible);
    mw->m_bottomTabWidget->setVisible(consoleVisible);

    // Reconnect panel tabs - Left Panel
    while (mw->m_leftTabWidget->count() > 0) {
        QWidget* w = mw->m_leftTabWidget->widget(0);
        mw->m_leftTabWidget->removeTab(0);
        delete w;
    }
    for (const QString& path : leftPaths) {
        mw->createTab(mw->m_leftTabWidget, path);
    }
    if (mw->m_leftTabWidget->count() > 0) {
        mw->m_leftTabWidget->setCurrentIndex(qBound(0, leftActive, mw->m_leftTabWidget->count() - 1));
        FilePanel* fp = qobject_cast<FilePanel*>(mw->m_leftTabWidget->currentWidget());
        if (fp) fp->setViewModeIndex(leftViewMode);
    }

    // Reconnect panel tabs - Right Panel
    while (mw->m_rightTabWidget->count() > 0) {
        QWidget* w = mw->m_rightTabWidget->widget(0);
        mw->m_rightTabWidget->removeTab(0);
        delete w;
    }
    for (const QString& path : rightPaths) {
        mw->createTab(mw->m_rightTabWidget, path);
    }
    if (mw->m_rightTabWidget->count() > 0) {
        mw->m_rightTabWidget->setCurrentIndex(qBound(0, rightActive, mw->m_rightTabWidget->count() - 1));
        FilePanel* fp = qobject_cast<FilePanel*>(mw->m_rightTabWidget->currentWidget());
        if (fp) fp->setViewModeIndex(rightViewMode);
    }

    mw->updateSiblingLinks();
    accept();
}

void WorkspaceProfileDialog::onDeleteClicked() {
    QString name = m_profilesCombo->currentText();
    if (name.isEmpty()) return;

    if (QMessageBox::question(this, "Delete Profile", QString("Are you sure you want to delete profile '%1'?").arg(name)) == QMessageBox::Yes) {
        QSettings settings("Amifiles", "Amifiles");
        settings.remove(QString("workspace_profiles/%1").arg(name));
        
        QStringList profiles = settings.value("workspace_profiles/profile_list").toStringList();
        profiles.removeAll(name);
        settings.setValue("workspace_profiles/profile_list", profiles);
        
        QMessageBox::information(this, "Delete Profile", "Workspace profile deleted.");
        loadProfilesList();
    }
}
