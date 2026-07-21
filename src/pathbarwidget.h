#ifndef PATHBARWIDGET_H
#define PATHBARWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>

class PathBarWidget : public QWidget {
    Q_OBJECT
public:
    explicit PathBarWidget(QWidget* parent = nullptr);
    ~PathBarWidget() override = default;

    void setPath(const QString& path);
    QString path() const { return m_currentPath; }

    QLineEdit* lineEdit() const { return m_editPath; }

signals:
    void pathEntered(const QString& path);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onEditReturnPressed();
    void onSegmentClicked(const QString& targetPath);
    void onDropdownClicked(const QString& parentPath, QWidget* anchorWidget);

private:
    void rebuildBreadcrumbs();
    void showEditMode();
    void showBreadcrumbMode();

    QString m_currentPath;
    QStackedWidget* m_stack = nullptr;
    QWidget* m_breadcrumbPage = nullptr;
    QHBoxLayout* m_breadcrumbLayout = nullptr;
    QLineEdit* m_editPath = nullptr;
    bool m_isEditing = false;
};

#endif // PATHBARWIDGET_H
