#ifndef DISKSPACEANALYZERDIALOG_H
#define DISKSPACEANALYZERDIALOG_H

#include <QDialog>
#include <QWidget>
#include <QThread>
#include <QList>
#include <QColor>
#include <QRectF>

class QProgressBar;
class QLabel;

class TreeMapWidget : public QWidget {
    Q_OBJECT
public:
    struct Node {
        QString name;
        QString path;
        qint64 size = 0;
        bool isDir = false;
        QList<Node*> children;

        ~Node() {
            qDeleteAll(children);
        }
    };

    struct RectItem {
        QRectF rect;
        QString label;
        QString path;
        qint64 size;
        bool isDir = false;
        QColor color;
    };

    explicit TreeMapWidget(QWidget* parent = nullptr);
    ~TreeMapWidget() override;

    void setRootNode(Node* root);
    void clear();

signals:
    void fileDoubleClicked(const QString& path);
    void itemHovered(const QString& path, qint64 size);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void computeLayout(Node* parentNode, const QRectF& rect);
    void sliceAndDice(const QList<Node*>& nodes, const QRectF& rect);
    QColor colorForNode(const QString& name, bool isDir);
    QString formatSize(qint64 bytes) const;

    Node* m_rootNode = nullptr;
    QList<RectItem> m_items;
    int m_hoveredIndex = -1;
};

class ScanWorker : public QThread {
    Q_OBJECT
public:
    explicit ScanWorker(const QString& path, QObject* parent = nullptr);
    ~ScanWorker() override = default;

signals:
    void progressUpdate(const QString& currentFolder);
    void scanFinished(TreeMapWidget::Node* rootNode);

protected:
    void run() override;

private:
    TreeMapWidget::Node* scanDir(const QString& path);
    QString m_path;
};

class DiskSpaceAnalyzerDialog : public QDialog {
    Q_OBJECT
public:
    explicit DiskSpaceAnalyzerDialog(const QString& path, QWidget* parent = nullptr);
    ~DiskSpaceAnalyzerDialog() override;

signals:
    void locateFileRequested(const QString& path);

private slots:
    void onScanFinished(TreeMapWidget::Node* rootNode);
    void onProgressUpdate(const QString& folderName);
    void onFileDoubleClicked(const QString& path);
    void onItemHovered(const QString& path, qint64 size);

private:
    void setupUI();
    QString formatSize(qint64 bytes) const;

    QString m_path;
    ScanWorker* m_worker = nullptr;
    TreeMapWidget* m_treeMap = nullptr;
    QLabel* m_lblStatus = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_lblHoverInfo = nullptr;
};

#endif // DISKSPACEANALYZERDIALOG_H
