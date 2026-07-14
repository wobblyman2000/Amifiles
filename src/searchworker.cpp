#include "searchworker.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>

SearchWorker::SearchWorker(QObject* parent)
    : QObject(parent), m_cancel(false) {}

void SearchWorker::cancel() {
    m_cancel = true;
}

void SearchWorker::doSearch(const QString& query, const QString& path) {
    m_cancel = false;
    QStringList results;
    
    if (query.isEmpty() || path.isEmpty()) {
        emit resultsReady(results);
        emit searchFinished();
        return;
    }

    // Determine if we should use wildcard/regex matching or substring matching
    bool isWildcard = query.contains('*') || query.contains('?');
    QRegularExpression regex;
    if (isWildcard) {
        regex = QRegularExpression(QRegularExpression::wildcardToRegularExpression(query), QRegularExpression::CaseInsensitiveOption);
    }

    QDirIterator it(path, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    
    int checkCounter = 0;
    while (it.hasNext()) {
        if (m_cancel) {
            break;
        }

        it.next();
        QFileInfo info = it.fileInfo();
        QString name = info.fileName();
        
        bool match = false;
        if (isWildcard) {
            match = regex.match(name).hasMatch();
        } else {
            match = name.contains(query, Qt::CaseInsensitive);
        }

        if (match) {
            results.append(info.absoluteFilePath());
        }

        // Periodically emit results progressively so UI shows progress
        if (++checkCounter % 100 == 0) {
            if (!results.isEmpty()) {
                emit resultsReady(results);
                results.clear();
            }
        }
    }

    // Emit any remaining results
    if (!results.isEmpty() && !m_cancel) {
        emit resultsReady(results);
    }

    emit searchFinished();
}
