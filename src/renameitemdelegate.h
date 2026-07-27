#pragma once

#include <QStyledItemDelegate>
#include <QLineEdit>
#include <QFileInfo>
#include <QSettings>
#include <QTimer>
#include <QFont>
#include <QCursor>
#include <QFocusEvent>

class RenameLineEdit : public QLineEdit {
    Q_OBJECT
public:
    explicit RenameLineEdit(QWidget* parent = nullptr) : QLineEdit(parent) {}
protected:
    void focusInEvent(QFocusEvent* event) override {
        QLineEdit::focusInEvent(event);
        // Pre-select only the base name (up to the last dot), keeping extension unselected.
        // We use a small delay (50ms) to ensure it runs after the view's default selectAll() call.
        QTimer::singleShot(50, this, [this]() {
            QString txt = this->text();
            int dotIdx = txt.lastIndexOf('.');
            if (dotIdx > 0) {
                this->setSelection(0, dotIdx);
            } else {
                this->selectAll();
            }
        });
    }
};

class RenameItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit RenameItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}
    ~RenameItemDelegate() override = default;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        // Enforce clicking on the text area to trigger renaming (cancel if clicked on icon/thumbnail)
        if (parent) {
            QPoint pos = parent->mapFromGlobal(QCursor::pos());
            QRect r = option.rect;
            if (r.contains(pos)) { // Only restrict if mouse is actually clicking inside the item (not F2 keypress)
                if (r.height() == 68) {
                    // Card View: icon/thumbnail on the left 60px
                    if (pos.x() < r.x() + 60) {
                        return nullptr;
                    }
                } else if (r.height() > 28) {
                    // Theater / Large Views: icon/thumbnail on the left 80px
                    if (pos.x() < r.x() + 80) {
                        return nullptr;
                    }
                } else {
                    // Tree/Details View: icon/checkbox area on the left 32px
                    if (pos.x() < r.x() + 32) {
                        return nullptr;
                    }
                }
            }
        }

        // Create our custom RenameLineEdit instead of the default QLineEdit
        RenameLineEdit* lineEdit = new RenameLineEdit(parent);
        
        // Apply option font (exact rendering font size of the item)
        lineEdit->setFont(option.font);

        // Apply premium styling to the inline editor to remove the ugly large grey box
        lineEdit->setStyleSheet(
            "QLineEdit {"
            "  background-color: #313244;"
            "  color: #cdd6f4;"
            "  border: 1px solid #89b4fa;"
            "  border-radius: 4px;"
            "  padding: 1px 2px;"
            "}"
        );
        
        return lineEdit;
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override {
        QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
        if (lineEdit) {
            QString newText = lineEdit->text().trimmed();
            if (!newText.isEmpty()) {
                QString oldText = index.data(Qt::EditRole).toString();
                QFileInfo oldInfo(oldText);
                QString oldExt = oldInfo.suffix();

                QSettings settings("Amifiles", "Amifiles");
                bool keepExt = settings.value("behavior/keep_extension_on_rename", true).toBool();

                if (keepExt && !oldExt.isEmpty() && !oldInfo.isDir()) {
                    QFileInfo newInfo(newText);
                    QString newExt = newInfo.suffix();
                    if (newExt.isEmpty() && !newText.endsWith(".")) {
                        // User completely omitted the extension; restore original
                        newText += "." + oldExt;
                    }
                }
                model->setData(index, newText, Qt::EditRole);
                return;
            }
        }
        QStyledItemDelegate::setModelData(editor, model, index);
    }

    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyledItemDelegate::updateEditorGeometry(editor, option, index);
    }
};
