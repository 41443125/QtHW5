#pragma once

#include <QImage>
#include <QPromise>
#include <QString>

struct ImageFile
{
    QString path;
    QImage pixels;
    QString sha256;
};

struct ImageLoadResult
{
    ImageFile file;
    QString error;
};

namespace ImageIO {

void load(QPromise<ImageLoadResult> &result, const QString &path);

// Returns an error message on failure. The destination is replaced only after a successful write.
QString save(const QImage &image, const QString &path);

} // namespace ImageIO
