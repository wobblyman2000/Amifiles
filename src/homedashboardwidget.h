#ifndef HOMEDASHBOARDWIDGET_H
#define HOMEDASHBOARDWIDGET_H

#include <QWidget>
#include <QStringList>

class QGridLayout;
class QVBoxLayout;

class HomeDashboardWidget : public QWidget {
    Q_OBJECT
public:
    explicit HomeDashboardWidget(QWidget* parent = nullptr);
    ~HomeDashboardWidget() override = default;

    void refreshDashboard();

signals:
    void navigateRequested(const QString& path);
    void navigateWithLayoutRequested(const QString& path, int layoutIndex);
    void applyProfileRequested(const QString& profileName);

private slots:
    void onDriveDoubleClicked(const QString& path);
    void onQuickAccessClicked(const QString& path);
    void onPinnedFolderClicked(const QString& path, int layoutIndex);
    void onUnpinFolderClicked(const QString& path);
    void onToolButtonClicked(const QString& action);

private:
    void setupUi();
    void populateDrives();
    void populateQuickAccess();
    void populatePinnedFolders();
    void populatePinnedProfiles();

    QGridLayout* m_drivesLayout = nullptr;
    QGridLayout* m_quickAccessLayout = nullptr;
    QGridLayout* m_pinnedLayout = nullptr;
    QGridLayout* m_pinnedProfilesLayout = nullptr;
};

#endif // HOMEDASHBOARDWIDGET_H
