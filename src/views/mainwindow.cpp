#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QKeyEvent>
#include <QDebug>
#include <QMimeData>
#include <QApplication>
#include "model/modellocator.h"
#include "commands/appfrontcontroller.h"
#include "bitstreamversionselector.h"
#include "model/common/comrom.h"
#include "gitlivkcmdevt.h"
#include "preferencedialog.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_cPreferenceDialog(this),
    m_cBusyDialog(this),
    m_cThemeGroup(this),
    ui(new Ui::MainWindow)
{

    setModualName("main_window");
    ui->setupUi(this);

    /// set listeners
    listenToParams("picture", MAKE_CALLBACK(MainWindow::onFrameArrived));
    listenToParams(QStringList() <<"total_frame_num"<<"current_frame_poc",
                   MAKE_CALLBACK(MainWindow::onPOCInfoChanged));
    listenToParams("theme_stylesheet", MAKE_CALLBACK(MainWindow::onStylesheetChanged));
    listenToParams("snapshot", MAKE_CALLBACK(MainWindow::onSnapshot));


    /// layout hacks
    ui->msgDockWidget->widget()->layout()->setContentsMargins(0,0,0,0);
    ui->pluginFilterListDockWidget->widget()->layout()->setContentsMargins(0,0,0,0);
    ui->sequencesListDockWidget->widget()->layout()->setContentsMargins(0,0,0,0);

    /// exclusive theme action btns
    ui->defaultThemeAction->setActionGroup(&m_cThemeGroup);
    ui->darkThemeAction->setActionGroup(&m_cThemeGroup);

    /// bitstream drop open accepted
    setAcceptDrops(true);

    /// model init, including filter loading, etc..
    ModelLocator::getInstance();

    /// load theme from preferences
    GitlIvkCmdEvt cEvt("switch_theme");
    cEvt.setParameter("load_theme_from_pref", true);
    cEvt.dispatch();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onFrameArrived(GitlUpdateUIEvt& rcEvt)
{
    QVariant vValue = rcEvt.getParameter("picture");
    QPixmap* pcPixMap = (QPixmap*)(vValue.value<void *>());
    ui->imageView->setDisplayImage(pcPixMap);
}

void MainWindow::onPOCInfoChanged(GitlUpdateUIEvt& rcEvt)
{
    static int iTotalFrameNum = -1;
    static int iCurrentFrameNum = -1;
    QVariant vValue;
    /// frame number
    if( rcEvt.hasParameter("total_frame_num") )
    {
        vValue = rcEvt.getParameter("total_frame_num");
        iTotalFrameNum = vValue.toInt();
        ui->totalFrameNum->setText(QString("%1").arg(iTotalFrameNum));
    }

    /// total frame number
    if( rcEvt.hasParameter("current_frame_poc") )
    {
        vValue = rcEvt.getParameter("current_frame_poc");
        iCurrentFrameNum = vValue.toInt() + 1;
        ui->currentFrameNum->setText(QString("%1").arg(iCurrentFrameNum));
    }

    /// slide bar position
    int iMin = ui->progressBar->minimum();
    int iMax = ui->progressBar->maximum();
    int iPos = 0;
    if(iTotalFrameNum != 1)
    {
        iPos = (iCurrentFrameNum-1)*(iMax-iMin)/(iTotalFrameNum-1);
    }
    ui->progressBar->setValue(iPos);
}

void MainWindow::onStylesheetChanged(GitlUpdateUIEvt& rcEvt)
{
    qApp->setStyleSheet(rcEvt.getParameter("theme_stylesheet").toString());
    QString strThemeName = rcEvt.getParameter("theme_name").toString();
    if(strThemeName == "default")
        ui->defaultThemeAction->setChecked(true);
    else if(strThemeName == "dark")
        ui->darkThemeAction->setChecked(true);
}

void MainWindow::onSnapshot(GitlUpdateUIEvt& rcEvt)
{
    /// TODO refactor
    QVariant vValue = rcEvt.getParameter("snapshot");
    QPixmap* pcPixMap = (QPixmap*)(vValue.value<void *>());
    xSaveSnapshot(pcPixMap);
}

/// Keyboard Event
void MainWindow::keyPressEvent ( QKeyEvent * event )
{
    if(event->key() == Qt::Key_Comma || event->key() == Qt::LeftArrow )
    {
        /// Next Frame
        on_previousFrame_clicked();
    }
    else if(event->key() == Qt::Key_Period || event->key() == Qt::RightArrow )
    {
        /// Previous Frame
        on_nextFrame_clicked();
    }

}


void MainWindow::on_previousFrame_clicked()
{
    /// invoke command
    GitlIvkCmdEvt cEvt("prev_frame");
    cEvt.dispatch();
}

void MainWindow::on_nextFrame_clicked()
{
    /// invoke command
    GitlIvkCmdEvt cEvt("next_frame");
    cEvt.dispatch();
}

void MainWindow::on_progressBar_actionTriggered(int action)
{
    int iBarPercent = int(100*double(ui->progressBar->sliderPosition()-ui->progressBar->minimum()) /
                                       (ui->progressBar->maximum()-ui->progressBar->minimum()));

    /// invoke command
    GitlIvkCmdEvt cEvt("jumpto_percent");
    cEvt.setParameter("percent", iBarPercent);
    cEvt.dispatch();
}


void MainWindow::xSaveSnapshot(QPixmap *pcPixmap)
{
    ///
    QString strFilename;
    QString strLastPath = g_cAppSetting.value("snapshot_saving_path",".").toString();
    strFilename=QFileDialog::getSaveFileName(this,
                                          tr("Save Snapshot"),
                                          strLastPath,
                                          tr("Images (*.png)"));
    if(!strFilename.isEmpty())
    {
        g_cAppSetting.setValue("snapshot_saving_path",strFilename);
        if( pcPixmap->save(strFilename) )
            qDebug() << QString("Snapshot Has Been Saved to %1 !").arg(strFilename);
        else
            qWarning() <<"Snapshot Saving Failed!";
    }


}


void MainWindow::on_actionOpen_bitstream_triggered()
{
    /// select file path
    QString strFilename;
    QString strLastPath = g_cAppSetting.value("open_bitstream_path",".").toString();
    strFilename=QFileDialog::getOpenFileName(this,
                                          tr("Open Bitstream File"),
                                          strLastPath,
                                          tr("All Files (*.*)"));

    if(!strFilename.isEmpty())
        g_cAppSetting.setValue("open_bitstream_path",strFilename);

    if(strFilename.isEmpty() || !QFileInfo(strFilename).exists() )
    {
        qWarning() << "File not found.";
        return;
    }

    /// select HM version
    BitstreamVersionSelector cBitstreamDig(this);
    if( cBitstreamDig.exec() == QDialog::Rejected )
        return;

    /// prepare & sent event to bus
    GitlIvkCmdEvt cEvt("decode_bitstream");
    cEvt.setParameter("filename", strFilename);
    cEvt.setParameter("skip_decode", false);
    cEvt.setParameter("version", cBitstreamDig.getBitstreamVersion());
    cEvt.dispatch();

}


void MainWindow::on_printScreenBtn_clicked()
{
    /// invoke command
    GitlIvkCmdEvt cEvt("print_screen");
    cEvt.dispatch();
}



void MainWindow::on_actionAbout_triggered()
{
    m_cAboutDialog.show();
}




void MainWindow::on_openBitstreamBtn_clicked()
{
    on_actionOpen_bitstream_triggered();
}


void MainWindow::on_actionExit_triggered()
{
    exit(0);
}

void MainWindow::on_actionCheckUpdate_triggered()
{
    /// invoke command
    GitlIvkCmdEvt cEvt("check_update");
    cEvt.dispatch();
}

void MainWindow::on_actionReloadPluginsFilters_triggered()
{
    /// invoke command
    GitlIvkCmdEvt cEvt("reload_filter");
    cEvt.dispatch();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
 {
     event->acceptProposedAction();
 }

void MainWindow::dropEvent(QDropEvent *event)
{

    event->acceptProposedAction();

    if(event->mimeData()->urls().empty())
        return;

    QString strFilename = event->mimeData()->urls().at(0).toLocalFile();

    if(!strFilename.isEmpty())
        g_cAppSetting.setValue("open_bitstream_path",strFilename);

    if(strFilename.isEmpty() || !QFileInfo(strFilename).exists() )
    {
        qWarning() << "File not found.";
        return;
    }

    /// select HM version
    BitstreamVersionSelector cBitstreamDig(this);
    if( cBitstreamDig.exec() == QDialog::Rejected )
        return;

    /// prepare & sent event to bus
    /// invoke command
    GitlIvkCmdEvt cEvt("reload_filter");
    cEvt.setParameter("command_name", "decode_bitstream");
    cEvt.setParameter("filename", strFilename);
    cEvt.setParameter("skip_decode", false);
    cEvt.setParameter("version", cBitstreamDig.getBitstreamVersion());
    cEvt.dispatch();
}

void MainWindow::on_actionPreferences_triggered()
{
    m_cPreferenceDialog.show();
}

void MainWindow::on_defaultThemeAction_triggered()
{
    GitlIvkCmdEvt cEvt("switch_theme");
    cEvt.setParameter("theme_name", "default");
    cEvt.dispatch();
}

void MainWindow::on_darkThemeAction_triggered()
{
    GitlIvkCmdEvt cEvt("switch_theme");
    cEvt.setParameter("theme_name", "dark");
    cEvt.dispatch();
}

