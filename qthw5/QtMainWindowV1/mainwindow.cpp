#include "mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDragEnterEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QStatusBar>
#include <QStringList>
#include <QStyle>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrentRun>
#include <utility>

namespace {

const QString imageFilter = QStringLiteral("影像檔案 (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tif *.tiff *.ico);;所有檔案 (*)");

struct SaveFormat
{
    QString name;
    QStringList suffixes;

    QString nameFilter() const
    {
        QStringList patterns;
        for (const QString &suffix : suffixes)
            patterns.append(QStringLiteral("*.%1").arg(suffix));
        return QStringLiteral("%1 (%2)").arg(name, patterns.join(QLatin1Char(' ')));
    }
};

const QList<SaveFormat> saveFormats{
    {QStringLiteral("PNG"), {QStringLiteral("png")}},
    {QStringLiteral("JPEG"), {QStringLiteral("jpg"), QStringLiteral("jpeg")}},
    {QStringLiteral("BMP"), {QStringLiteral("bmp")}},
};

bool singleLocalFile(const QMimeData *data)
{
    const auto urls = data->urls();
    return urls.size() == 1 && urls.front().isLocalFile();
}

} // namespace

MainWindow::MainWindow()
{
    setWindowTitle(QStringLiteral("Qt Image Studio"));
    setAcceptDrops(true);
    createActions();
    createWorkspace();

    connect(&loader_, &QFutureWatcher<ImageLoadResult>::progressValueChanged,
            progress_, &QProgressBar::setValue);
    connect(&loader_, &QFutureWatcher<ImageLoadResult>::finished,
            this, &MainWindow::finishLoading);
    updateActions();
    resize(930, 650);
    statusBar()->showMessage(QStringLiteral("就緒"));
}

MainWindow::~MainWindow()
{
    loader_.cancel();
    loader_.waitForFinished();
}

void MainWindow::createActions()
{
    openAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton),
                              QStringLiteral("&Open Image…"), this);
    openAction_->setShortcut(QKeySequence::Open);
    openAction_->setObjectName(QStringLiteral("openImageAction"));
    connect(openAction_, &QAction::triggered, this, &MainWindow::openFile);

    saveAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton),
                              QStringLiteral("&Save Image…"), this);
    saveAction_->setShortcut(QKeySequence::Save);
    saveAction_->setObjectName(QStringLiteral("saveImageAction"));
    connect(saveAction_, &QAction::triggered, this, &MainWindow::saveFile);

    auto *exitAction = new QAction(style()->standardIcon(QStyle::SP_DialogCloseButton),
                                   QStringLiteral("E&xit"), this);
    exitAction->setShortcuts(QKeySequence::Quit);
    if (exitAction->shortcuts().isEmpty())
        exitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Q")));
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    verifyAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogApplyButton),
                                QStringLiteral("驗證 SHA-256"), this);
    verifyAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+H")));
    verifyAction_->setObjectName(QStringLiteral("verifyHashAction"));
    connect(verifyAction_, &QAction::triggered, this, [this] {
        expectedHash_->setFocus();
        verifyHash();
    });

    const QIcon copyIcon = QIcon::fromTheme(QStringLiteral("edit-copy"),
                                           style()->standardIcon(QStyle::SP_FileIcon));
    copyHashAction_ = new QAction(copyIcon, QStringLiteral("複製 SHA-256"), this);
    copyHashAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+C")));
    copyHashAction_->setObjectName(QStringLiteral("copyHashAction"));
    connect(copyHashAction_, &QAction::triggered, this, [this] {
        QApplication::clipboard()->setText(currentImage_.sha256);
        statusBar()->showMessage(QStringLiteral("已複製來源檔案的 SHA-256。"), 5000);
    });

    auto *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    fileMenu->addAction(openAction_);
    fileMenu->addAction(saveAction_);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);
    menuBar()->addMenu(QStringLiteral("&Image"))->addAction(verifyAction_);
    menuBar()->addMenu(QStringLiteral("&Edit"))->addAction(copyHashAction_);

    auto *toolbar = addToolBar(QStringLiteral("File"));
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->addAction(openAction_);
    toolbar->addAction(saveAction_);
}

void MainWindow::createWorkspace()
{
    auto *workspace = new QWidget;
    auto *layout = new QVBoxLayout(workspace);

    preview_ = new QLabel(QStringLiteral("把影像拖到這裡，或按 Ctrl+O 開啟"), workspace);
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setMinimumSize(320, 240);
    preview_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    preview_->setFrameShape(QFrame::StyledPanel);
    preview_->setBackgroundRole(QPalette::Base);
    preview_->setAutoFillBackground(true);
    preview_->setObjectName(QStringLiteral("imagePreview"));
    layout->addWidget(preview_, 1);

    imageInfo_ = new QLabel(QStringLiteral("尚未開啟影像"), workspace);
    imageInfo_->setTextFormat(Qt::PlainText);
    imageInfo_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    imageInfo_->setWordWrap(true);
    layout->addWidget(imageInfo_);

    auto *fields = new QFormLayout;
    sha256_ = new QLineEdit(workspace);
    sha256_->setObjectName(QStringLiteral("computedHash"));
    sha256_->setReadOnly(true);
    fields->addRow(QStringLiteral("來源檔案 SHA-256"), sha256_);

    auto *comparison = new QHBoxLayout;
    expectedHash_ = new QLineEdit(workspace);
    expectedHash_->setPlaceholderText(QStringLiteral("貼上來源提供的 64 位十六進位雜湊"));
    expectedHash_->setObjectName(QStringLiteral("expectedHash"));
    comparison->addWidget(expectedHash_, 1);
    auto *verifyButton = new QPushButton(QStringLiteral("比對"), workspace);
    comparison->addWidget(verifyButton);
    fields->addRow(QStringLiteral("預期 SHA-256"), comparison);
    verification_ = new QLabel(workspace);
    verification_->setObjectName(QStringLiteral("verificationResult"));
    fields->addRow(QString(), verification_);
    layout->addLayout(fields);
    connect(verifyButton, &QPushButton::clicked, this, &MainWindow::verifyHash);
    connect(expectedHash_, &QLineEdit::returnPressed, this, &MainWindow::verifyHash);
    connect(expectedHash_, &QLineEdit::textChanged, verification_, &QLabel::clear);

    progress_ = new QProgressBar(workspace);
    progress_->hide();
    statusBar()->addPermanentWidget(progress_);
    setCentralWidget(workspace);
}

void MainWindow::openFile()
{
    const QString startDir = currentImage_.path.isEmpty() ? QString() : QFileInfo(currentImage_.path).absolutePath();
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("開啟影像"), startDir, imageFilter);
    if (!path.isEmpty())
        startLoading(path);
}

void MainWindow::startLoading(const QString &path)
{
    if (loading_)
        return;

    loading_ = true;
    updateActions();
    progress_->setValue(0);
    progress_->show();
    statusBar()->showMessage(QStringLiteral("正在讀取影像並計算 SHA-256…"));
    loader_.setFuture(QtConcurrent::run(ImageIO::load, path));
}

void MainWindow::finishLoading()
{
    loading_ = false;
    progress_->hide();
    auto result = loader_.result();
    if (!result.error.isEmpty()) {
        statusBar()->showMessage(result.error, 7000);
        updateActions();
        QMessageBox::warning(this, QStringLiteral("開啟失敗"), result.error);
        return;
    }

    currentImage_ = std::move(result.file);
    sha256_->setText(currentImage_.sha256);
    expectedHash_->clear();
    verification_->clear();
    imageInfo_->setText(QStringLiteral("%1  |  %2 × %3 px")
                            .arg(QFileInfo(currentImage_.path).fileName())
                            .arg(currentImage_.pixels.width()).arg(currentImage_.pixels.height()));
    updatePreview();
    updateActions();
    statusBar()->showMessage(QStringLiteral("影像已開啟，SHA-256 計算完成。"), 6000);
}

void MainWindow::saveFile()
{
    const QFileInfo source(currentImage_.path);
    QStringList filters;
    for (const SaveFormat &format : saveFormats)
        filters.append(format.nameFilter());

    QString selectedFilter = filters.front();
    const QString suggestedName = source.completeBaseName().replace(QLatin1Char('.'), QLatin1Char('_'))
                                  + QStringLiteral("_copy");
    QString destination = QFileDialog::getSaveFileName(
        this, QStringLiteral("另存影像"),
        QDir(source.absolutePath()).filePath(suggestedName),
        filters.join(QStringLiteral(";;")), &selectedFilter, QFileDialog::DontConfirmOverwrite);
    if (destination.isEmpty())
        return;

    const qsizetype formatIndex = filters.indexOf(selectedFilter);
    if (formatIndex < 0) {
        QMessageBox::warning(this, QStringLiteral("儲存失敗"), QStringLiteral("無法辨識所選格式，請重新選擇。"));
        return;
    }

    const SaveFormat &format = saveFormats[formatIndex];
    const QString suffix = QFileInfo(destination).suffix().toLower();
    if (suffix.isEmpty()) {
        if (!destination.endsWith(QLatin1Char('.')))
            destination += QLatin1Char('.');
        destination += format.suffixes.front();
    } else if (!format.suffixes.contains(suffix)) {
        QStringList extensions;
        for (const QString &allowedSuffix : format.suffixes)
            extensions.append(QLatin1Char('.') + allowedSuffix);
        QMessageBox::warning(this, QStringLiteral("儲存失敗"),
                             QStringLiteral("副檔名 .%1 與所選的 %2 格式不符。請使用 %3 或改選格式。")
                                 .arg(suffix, format.name, extensions.join(QStringLiteral("、"))));
        return;
    }

    if (QFile::exists(destination) &&
        QMessageBox::question(this, QStringLiteral("另存影像"),
                              QStringLiteral("%1 已存在。要覆寫嗎？").arg(QFileInfo(destination).fileName()),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    const QString error = ImageIO::save(currentImage_.pixels, destination);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("儲存失敗"), error);
        return;
    }
    statusBar()->showMessage(QStringLiteral("已儲存 %1").arg(QFileInfo(destination).fileName()), 6000);
}

void MainWindow::updateActions()
{
    openAction_->setEnabled(!loading_);
    const bool imageAvailable = !loading_ && !currentImage_.pixels.isNull();
    saveAction_->setEnabled(imageAvailable);
    verifyAction_->setEnabled(imageAvailable);
    copyHashAction_->setEnabled(imageAvailable);
}

void MainWindow::updatePreview()
{
    const QImage &image = currentImage_.pixels;
    if (image.isNull())
        return;

    QSize size = image.size();
    const QSize available = preview_->contentsRect().size();
    if (size.width() > available.width() || size.height() > available.height())
        size.scale(available, Qt::KeepAspectRatio);
    preview_->setPixmap(QPixmap::fromImage(image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
}

void MainWindow::verifyHash()
{
    if (loading_ || currentImage_.sha256.isEmpty()) {
        verification_->setText(QStringLiteral("請先開啟影像並等待計算完成。"));
        return;
    }

    static const QRegularExpression pattern(QStringLiteral("^[0-9A-Fa-f]{64}$"));
    const QString expected = expectedHash_->text().trimmed();
    if (!pattern.match(expected).hasMatch()) {
        verification_->setText(QStringLiteral("請輸入 64 位十六進位 SHA-256。"));
        return;
    }

    verification_->setText(expected.compare(currentImage_.sha256, Qt::CaseInsensitive) == 0
                               ? QStringLiteral("相符：來源檔案的 SHA-256 一致。")
                               : QStringLiteral("不相符：請確認檔案與預期 SHA-256。"));
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!loading_ && singleLocalFile(event->mimeData()))
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (loading_ || !singleLocalFile(event->mimeData()))
        return;

    event->acceptProposedAction();
    startLoading(event->mimeData()->urls().front().toLocalFile());
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updatePreview();
}
