#pragma once

#include "imagefile.h"

#include <QFutureWatcher>
#include <QMainWindow>

class QAction;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QLineEdit;
class QProgressBar;
class QResizeEvent;

class MainWindow : public QMainWindow
{
public:
    MainWindow();
    ~MainWindow() override;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void createActions();
    void createWorkspace();
    void openFile();
    void startLoading(const QString &path);
    void finishLoading();
    void saveFile();
    void updateActions();
    void updatePreview();
    void verifyHash();

    QAction *openAction_ = nullptr;
    QAction *saveAction_ = nullptr;
    QAction *verifyAction_ = nullptr;
    QAction *copyHashAction_ = nullptr;
    QLabel *preview_ = nullptr;
    QLabel *imageInfo_ = nullptr;
    QLineEdit *sha256_ = nullptr;
    QLineEdit *expectedHash_ = nullptr;
    QLabel *verification_ = nullptr;
    QProgressBar *progress_ = nullptr;

    ImageFile currentImage_;
    bool loading_ = false;
    QFutureWatcher<ImageLoadResult> loader_;
};
