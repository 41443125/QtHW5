#include "imagefile.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QSaveFile>

void ImageIO::load(QPromise<ImageLoadResult> &result, const QString &path)
{
    const auto fail = [&](const QString &message) {
        result.addResult(ImageLoadResult{{}, message});
    };

    QFileInfo before(path);
    if (!before.isFile()) {
        fail(QStringLiteral("請選擇有效的本機影像檔案。"));
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        fail(QStringLiteral("無法讀取檔案：%1").arg(file.errorString()));
        return;
    }

    QImageReader reader(&file);
    reader.setDecideFormatFromContent(true);
    reader.setAutoTransform(true);
    if (!reader.canRead() || !file.seek(0)) {
        fail(QStringLiteral("無法辨識或讀取這個影像檔案。"));
        return;
    }

    const qint64 size = before.size();
    const QDateTime modified = before.lastModified();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    QByteArray block(1024 * 1024, '\0');
    qint64 total = 0;
    result.setProgressRange(0, 100);
    while (!file.atEnd()) {
        if (result.isCanceled())
            return;

        const qint64 count = file.read(block.data(), block.size());
        if (count <= 0) {
            fail(QStringLiteral("讀取檔案失敗：%1").arg(file.errorString()));
            return;
        }
        hash.addData(QByteArrayView(block.constData(), count));
        total += count;
        if (size > 0)
            result.setProgressValue(qMin(95, int(95.0 * total / size)));
    }

    if (result.isCanceled())
        return;
    if (!file.seek(0)) {
        fail(QStringLiteral("無法重新讀取影像。"));
        return;
    }

    // Use a fresh reader after hashing so its decoder starts at byte zero.
    QImageReader decoder(&file);
    decoder.setDecideFormatFromContent(true);
    decoder.setAutoTransform(true);
    const QImage image = decoder.read();
    if (image.isNull()) {
        fail(QStringLiteral("影像解碼失敗：%1").arg(decoder.errorString()));
        return;
    }

    const QFileInfo after(path);
    if (total != size || after.size() != size || after.lastModified() != modified) {
        fail(QStringLiteral("檔案在讀取時有變更，請重新開啟。"));
        return;
    }

    result.addResult(ImageLoadResult{{path, image, QString::fromLatin1(hash.result().toHex())}, {}});
    result.setProgressValue(100);
}

QString ImageIO::save(const QImage &image, const QString &path)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return QStringLiteral("無法開啟輸出檔案：%1").arg(file.errorString());

    const QByteArray format = QFileInfo(path).suffix().toLower().toLatin1();
    QImageWriter writer(&file, format);
    if (!writer.write(image)) {
        file.cancelWriting();
        return QStringLiteral("無法編碼影像：%1").arg(writer.errorString());
    }

    if (!file.commit())
        return QStringLiteral("無法寫入輸出檔案：%1").arg(file.errorString());

    return {};
}
