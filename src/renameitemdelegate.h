#pragma once

#include <QStyledItemDelegate>
#include <QLineEdit>
#include <QFileInfo>
#include <QSettings>
#include <QTimer>
#include <QFont>

class RenameItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit RenameItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}
    ~RenameItemDelegate() override = default;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QWidget* editor = QStyledItemDelegate::createEditor(parent, option, index);
        QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
        if (lineEdit) {
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
            
            // Match the font size of the parent list/tree item
            QVariant fontVar = index.data(Qt::FontRole);
            if (fontVar.isValid()) {
                lineEdit->setFont(fontVar.value<QFont>());
            } else {
                QFont defaultFont;
                defaultFont.setPointSize(10);
                lineEdit->setFont(defaultFont);
            }

            // Pre-select only the base name, keeping extension unselected
            QString text = lineEdit->text();
            QFileInfo info(text);
            QString ext = info.suffix();
            if (!ext.isEmpty()) {
                int baseLength = text.length() - ext.length() - 1; // -1 for dot
                if (baseLength > 0) {
                    QTimer::singleShot(0, lineEdit, [lineEdit, baseLength]() {
                        lineEdit->setSelection(0, baseLength);
                    });
                }
            }
        }
        return editor;
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
                    QString dotExt = "." + oldExt;
                    if (!newText.endsWith(dotExt, Qt::CaseInsensitive)) {
                        newText += dotExt;
                    }
                }
                model->setData(index, newText, Qt::EditRole);
                return;
            }
        }
        QStyledItemDelegate::setModelData(editor, model, index);
    }

    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(index);
        QRect r = option.rect;
        if (r.height() > 28) {
            int h = 24;
            int y = r.y() + (r.height() - h) / 2;
            if (r.height() == 68) {
                // Card View custom overlay: fits exactly over the title text area (leaves space for icon/subtitles)
                editor->setGeometry(r.x() + 62, r.y() + 10, r.width() - 70, 24);
                return;
            }
            editor->setGeometry(r.x(), y, r.width(), h);
        } else {
            editor->setGeometry(option.rect);
        }
    }
};
