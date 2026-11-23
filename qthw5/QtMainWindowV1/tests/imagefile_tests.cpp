#include "imagefile.h"

#include <QCryptographicHash>
#include <QFile>
#include <QImageReader>
#include <QTemporaryDir>
#include <QTest>
#include <QtConcurrentRun>

namespace {

ImageLoadResult readImage(const QString &path)
{
    return QtConcurrent::run(ImageIO::load, path).result();
}

bool writeBytes(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.flush();
}

} // namespace

class ImageFileTests : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip_data()
    {
        QTest::addColumn<QString>("suffix");
        QTest::addColumn<QByteArray>("codec");
        QTest::newRow("PNG") << QStringLiteral("png") << QByteArray("png");
        QTest::newRow("JPEG") << QStringLiteral("JPEG") << QByteArray("jpeg");
        QTest::newRow("BMP") << QStringLiteral("bmp") << QByteArray("bmp");
    }

    void roundTrip()
    {
        QFETCH(QString, suffix);
        QFETCH(QByteArray, codec);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("測試影像.%1").arg(suffix));
        QImage original(800, 600, QImage::Format_RGB32);
        original.fill(QColor(20, 60, 190));

        const QString error = ImageIO::save(original, path);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(QImageReader::imageFormat(path), codec);
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray bytes = file.readAll();
        if (codec == "bmp")
            QVERIFY(bytes.size() > 1024 * 1024); // Exercise more than one hash chunk.

        const auto result = readImage(path);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.file.path, path);
        QCOMPARE(result.file.pixels.size(), original.size());
        QCOMPARE(result.file.sha256, QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()));
    }

    void rejectedInputs()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString missing = dir.filePath(QStringLiteral("missing.png"));
        const QString empty = dir.filePath(QStringLiteral("empty.png"));
        const QString mislabeled = dir.filePath(QStringLiteral("text.png"));
        const QString corrupt = dir.filePath(QStringLiteral("corrupt.png"));
        QVERIFY(writeBytes(empty, {}));
        QVERIFY(writeBytes(mislabeled, "not an image"));
        QVERIFY(writeBytes(corrupt, QByteArray::fromHex("89504e470d0a1a0a0000000d4948445200")));

        for (const QString &path : {missing, dir.path(), empty, mislabeled, corrupt}) {
            const auto result = readImage(path);
            QVERIFY2(!result.error.isEmpty(), qPrintable(path));
            QVERIFY(result.file.pixels.isNull());
            QVERIFY(result.file.sha256.isEmpty());
        }
    }

    void failedWritePreservesDestination()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("keep.png"));
        const QByteArray previous("existing destination bytes");
        QVERIFY(writeBytes(path, previous));
        QVERIFY(!ImageIO::save(QImage(), path).isEmpty());
        QFile result(path);
        QVERIFY(result.open(QIODevice::ReadOnly));
        QCOMPARE(result.readAll(), previous);
    }

    void missingOutputDirectory()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("nonexistent/result.png"));
        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(!ImageIO::save(image, path).isEmpty());
        QVERIFY(!QFile::exists(path));
    }
};

QTEST_GUILESS_MAIN(ImageFileTests)
#include "imagefile_tests.moc"
