#ifndef PATHBARWIDGET_H
#define PATHBARWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>

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
    bool eventFilter(QObject* watched, QEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void onTextChanged(const QString& text);
    void onPopupItemClicked(QListWidgetItem* item);

private:
    void updatePopupList();

    QString m_currentPath;
    QLineEdit* m_editPath = nullptr;
    QListWidget* m_popupList = nullptr;
    bool m_blockPopup = false;
};

#endif // PATHBARWIDGET_H
