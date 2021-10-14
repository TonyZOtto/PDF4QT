//    Copyright (C) 2021 Jakub Melka
//
//    This file is part of PDF4QT.
//
//    PDF4QT is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Lesser General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    with the written consent of the copyright owner, any later version.
//
//    PDF4QT is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public License
//    along with PDF4QT.  If not, see <https://www.gnu.org/licenses/>.

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "aboutdialog.h"
#include "differencesdockwidget.h"
#include "settingsdockwidget.h"

#include "pdfwidgetutils.h"
#include "pdfdocumentreader.h"
#include "pdfdrawspacecontroller.h"
#include "pdfdocumentmanipulator.h"

#include <QToolBar>
#include <QDesktopWidget>
#include <QDesktopServices>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QVBoxLayout>

namespace pdfdocdiff
{

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_progress(new pdf::PDFProgress(this)),
    m_taskbarButton(new QWinTaskbarButton(this)),
    m_progressTaskbarIndicator(nullptr),
    m_cmsManager(nullptr),
    m_pdfWidget(nullptr),
    m_settingsDockWidget(nullptr),
    m_differencesDockWidget(nullptr),
    m_optionalContentActivity(nullptr),
    m_diff(nullptr),
    m_isChangingProgressStep(false),
    m_dontDisplayErrorMessage(false),
    m_diffNavigator(nullptr)
{
    ui->setupUi(this);

    setMinimumSize(pdf::PDFWidgetUtils::scaleDPI(this, QSize(800, 600)));

    // Initialize task bar progress
    m_progressTaskbarIndicator = m_taskbarButton->progress();

    m_settingsDockWidget = new SettingsDockWidget(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_settingsDockWidget);;

    m_differencesDockWidget = new DifferencesDockWidget(this, &m_diffResult, &m_filteredDiffResult, &m_diffNavigator, &m_settings);
    addDockWidget(Qt::LeftDockWidgetArea, m_differencesDockWidget);

    ui->documentFrame->setLayout(new QVBoxLayout);

    m_cmsManager = new pdf::PDFCMSManager(this);
    m_pdfWidget = new pdf::PDFWidget(m_cmsManager, pdf::RendererEngine::Software, 1, ui->documentFrame);
    m_pdfWidget->getDrawWidgetProxy()->setProgress(m_progress);
    ui->documentFrame->layout()->addWidget(m_pdfWidget);

    ui->menuView->addSeparator();
    ui->menuView->addAction(m_settingsDockWidget->toggleViewAction());
    ui->menuView->addAction(m_differencesDockWidget->toggleViewAction());

    ui->actionGet_Source->setData(int(Operation::GetSource));
    ui->actionAbout->setData(int(Operation::About));
    ui->actionOpen_Left->setData(int(Operation::OpenLeft));
    ui->actionOpen_Right->setData(int(Operation::OpenRight));
    ui->actionCompare->setData(int(Operation::Compare));
    ui->actionClose->setData(int(Operation::Close));
    ui->actionPrevious_Difference->setData(int(Operation::PreviousDifference));
    ui->actionNext_Difference->setData(int(Operation::NextDifference));
    ui->actionCreate_Compare_Report->setData(int(Operation::CreateCompareReport));
    ui->actionFilter_Text->setData(int(Operation::FilterText));
    ui->actionFilter_Vector_Graphics->setData(int(Operation::FilterVectorGraphics));
    ui->actionFilter_Images->setData(int(Operation::FilterImages));
    ui->actionFilter_Shading->setData(int(Operation::FilterShading));
    ui->actionFilter_Page_Movement->setData(int(Operation::FilterPageMovement));
    ui->actionView_Differences->setData(int(Operation::ViewDifferences));
    ui->actionView_Left->setData(int(Operation::ViewLeft));
    ui->actionView_Right->setData(int(Operation::ViewRight));
    ui->actionView_Overlay->setData(int(Operation::ViewOverlay));
    ui->actionShow_Pages_with_Differences->setData(int(Operation::ShowPageswithDifferences));
    ui->actionSave_Differences_to_XML->setData(int(Operation::SaveDifferencesToXML));

    ui->actionSynchronize_View_with_Differences->setChecked(true);

    QActionGroup* actionGroup = new QActionGroup(this);
    actionGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::Exclusive);
    actionGroup->addAction(ui->actionView_Differences);
    actionGroup->addAction(ui->actionView_Left);
    actionGroup->addAction(ui->actionView_Right);
    actionGroup->addAction(ui->actionView_Overlay);
    ui->actionView_Differences->setChecked(true);

    QToolBar* mainToolbar = addToolBar(tr("Main"));
    mainToolbar->setObjectName("main_toolbar");
    mainToolbar->addActions({ ui->actionOpen_Left, ui->actionOpen_Right });
    mainToolbar->addSeparator();
    mainToolbar->addAction(ui->actionCompare);
    mainToolbar->addAction(ui->actionCreate_Compare_Report);
    mainToolbar->addAction(ui->actionSave_Differences_to_XML);

    QToolBar* differencesToolbar = addToolBar(tr("Differences"));
    differencesToolbar->setObjectName("differences_toolbar");
    differencesToolbar->addActions({ ui->actionPrevious_Difference, ui->actionNext_Difference });

    QToolBar* viewToolbar = addToolBar(tr("View"));
    viewToolbar->setObjectName("view_toolbar");
    viewToolbar->addActions({ ui->actionView_Differences, ui->actionView_Left, ui->actionView_Right, ui->actionView_Overlay });
    viewToolbar->addSeparator();
    viewToolbar->addActions({ ui->actionShow_Pages_with_Differences, ui->actionSynchronize_View_with_Differences });
    viewToolbar->addSeparator();
    viewToolbar->addActions({ ui->actionFilter_Text, ui->actionFilter_Vector_Graphics, ui->actionFilter_Images, ui->actionFilter_Shading, ui->actionFilter_Page_Movement });

    QSize iconSize = pdf::PDFWidgetUtils::scaleDPI(this, QSize(24, 24));
    auto toolbars = findChildren<QToolBar*>();
    for (QToolBar* toolbar : toolbars)
    {
        toolbar->setIconSize(iconSize);
        ui->menuToolbars->addAction(toolbar->toggleViewAction());
    }

    connect(&m_mapper, QOverload<int>::of(&QSignalMapper::mapped), this, &MainWindow::onMappedActionTriggered);

    QList<QAction*> actions = findChildren<QAction*>();
    for (QAction* action : actions)
    {
        QVariant actionData = action->data();
        if (actionData.isValid())
        {
            connect(action, &QAction::triggered, &m_mapper, QOverload<>::of(&QSignalMapper::map));
            m_mapper.setMapping(action, actionData.toInt());
        }
    }

    connect(m_progress, &pdf::PDFProgress::progressStarted, this, &MainWindow::onProgressStarted);
    connect(m_progress, &pdf::PDFProgress::progressStep, this, &MainWindow::onProgressStep);
    connect(m_progress, &pdf::PDFProgress::progressFinished, this, &MainWindow::onProgressFinished);

    m_diff.setProgress(m_progress);
    m_diff.setOption(pdf::PDFDiff::Asynchronous, true);
    connect(&m_diff, &pdf::PDFDiff::comparationFinished, this, &MainWindow::onComparationFinished);

    m_diff.setLeftDocument(&m_leftDocument);
    m_diff.setRightDocument(&m_rightDocument);

    m_diffNavigator.setResult(&m_filteredDiffResult);
    connect(&m_diffNavigator, &pdf::PDFDiffResultNavigator::selectionChanged, this, &MainWindow::onSelectionChanged);

    loadSettings();
    updateAll(false);
}

MainWindow::~MainWindow()
{
    saveSettings();
    delete ui;
}

void MainWindow::showEvent(QShowEvent* event)
{
    Q_UNUSED(event);
    m_taskbarButton->setWindow(windowHandle());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    BaseClass::closeEvent(event);
    m_diff.stop();
}

void MainWindow::onMappedActionTriggered(int actionId)
{
    performOperation(static_cast<Operation>(actionId));
}

void MainWindow::onComparationFinished()
{
    clear(false, false);

    m_diffResult = m_diff.getResult();

    if (!m_dontDisplayErrorMessage)
    {
        if (!m_diffResult.getResult())
        {
            QMessageBox::critical(this, tr("Error"), m_diffResult.getResult().getErrorMessage());
        }
        if (m_diffResult.isSame())
        {
            QMessageBox::information(this, tr("Info"), tr("No differences found between the compared documents."), QMessageBox::Ok);
        }
    }

    // Create merged document
    pdf::PDFDocumentManipulator manipulator;
    manipulator.setOutlineMode(pdf::PDFDocumentManipulator::OutlineMode::NoOutline);
    manipulator.addDocument(1, &m_leftDocument);
    manipulator.addDocument(2, &m_rightDocument);

    pdf::PDFDocumentManipulator::AssembledPages assembledPages1 = pdf::PDFDocumentManipulator::createAllDocumentPages(1, &m_leftDocument);
    pdf::PDFDocumentManipulator::AssembledPages assembledPages2 = pdf::PDFDocumentManipulator::createAllDocumentPages(2, &m_rightDocument);
    assembledPages1.insert(assembledPages1.end(), std::make_move_iterator(assembledPages2.begin()), std::make_move_iterator(assembledPages2.end()));

    if (manipulator.assemble(assembledPages1))
    {
        m_combinedDocument = manipulator.takeAssembledDocument();
    }
    else
    {
        m_combinedDocument = pdf::PDFDocument();
    }

    updateAll(true);
}

void MainWindow::updateActions()
{
    QList<QAction*> actions = findChildren<QAction*>();
    for (QAction* action : actions)
    {
        QVariant actionData = action->data();
        if (actionData.isValid())
        {
            bool canPerformAction = canPerformOperation(static_cast<Operation>(actionData.toInt()));
            action->setEnabled(canPerformAction);

            if (!canPerformAction && action->isCheckable())
            {
                action->setChecked(false);
            }
        }
    }
}

void MainWindow::onSelectionChanged(size_t currentIndex)
{
    if (ui->actionSynchronize_View_with_Differences->isChecked())
    {
        pdf::PDFInteger destinationPage = -1;

        if (destinationPage == -1)
        {
            pdf::PDFInteger leftPageIndex = m_filteredDiffResult.getLeftPage(currentIndex);
            if (leftPageIndex != -1)
            {
                destinationPage = m_documentMapper.getPageIndexFromLeftPageIndex(leftPageIndex);
            }
        }

        if (destinationPage == -1)
        {
            pdf::PDFInteger rightPageIndex = m_filteredDiffResult.getRightPage(currentIndex);
            if (rightPageIndex != -1)
            {
                destinationPage = m_documentMapper.getPageIndexFromRightPageIndex(rightPageIndex);
            }
        }

        if (destinationPage != -1)
        {
            m_pdfWidget->getDrawWidgetProxy()->goToPage(destinationPage);
        }
    }

    updateActions();
}

void MainWindow::loadSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.beginGroup("MainWindow");
    QByteArray geometry = settings.value("geometry", QByteArray()).toByteArray();
    if (geometry.isEmpty())
    {
        QRect availableGeometry = QApplication::desktop()->availableGeometry(this);
        QRect windowRect(0, 0, availableGeometry.width() / 2, availableGeometry.height() / 2);
        windowRect = windowRect.translated(availableGeometry.center() - windowRect.center());
        setGeometry(windowRect);
    }
    else
    {
        restoreGeometry(geometry);
    }

    QByteArray state = settings.value("windowState", QByteArray()).toByteArray();
    if (!state.isEmpty())
    {
        restoreState(state);
    }
    settings.endGroup();

    settings.beginGroup("Settings");
    m_settings.directory = settings.value("directory").toString();
    m_settings.colorPageMove = settings.value("colorPageMove", m_settings.colorPageMove).value<QColor>();
    m_settings.colorAdded = settings.value("colorAdded", m_settings.colorAdded).value<QColor>();
    m_settings.colorRemoved = settings.value("colorRemoved", m_settings.colorRemoved).value<QColor>();
    m_settings.colorReplaced = settings.value("colorReplaced", m_settings.colorReplaced).value<QColor>();
    settings.endGroup();
}

void MainWindow::saveSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.beginGroup("MainWindow");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.endGroup();

    settings.beginGroup("Settings");
    settings.setValue("directory", m_settings.directory);
    settings.setValue("colorPageMove", m_settings.colorPageMove);
    settings.setValue("colorAdded", m_settings.colorAdded);
    settings.setValue("colorRemoved", m_settings.colorRemoved);
    settings.setValue("colorReplaced", m_settings.colorReplaced);
    settings.endGroup();
}

bool MainWindow::canPerformOperation(Operation operation) const
{
    switch (operation)
    {
        case Operation::OpenLeft:
        case Operation::OpenRight:
        case Operation::Compare:
        case Operation::Close:
        case Operation::GetSource:
        case Operation::About:
            return true;

        case Operation::ViewDifferences:
        case Operation::ViewLeft:
        case Operation::ViewRight:
        case Operation::ViewOverlay:
            return true; // Allow always to change a view

        case Operation::FilterText:
            return m_diffResult.hasTextDifferences();

        case Operation::FilterVectorGraphics:
            return m_diffResult.hasVectorGraphicsDifferences();

        case Operation::FilterImages:
            return m_diffResult.hasImageDifferences();

        case Operation::FilterShading:
            return m_diffResult.hasShadingDifferences();

        case Operation::FilterPageMovement:
            return m_diffResult.hasPageMoveDifferences();

        case Operation::PreviousDifference:
            return m_diffNavigator.canGoPrevious();

        case Operation::NextDifference:
            return m_diffNavigator.canGoNext();

        case Operation::CreateCompareReport:
        case Operation::ShowPageswithDifferences:
        case Operation::SaveDifferencesToXML:
            return m_diffResult.isChanged();

        default:
            Q_ASSERT(false);
            break;
    }

    return false;
}

void MainWindow::performOperation(Operation operation)
{
    switch (operation)
    {
        case Operation::OpenLeft:
        {
            pdf::PDFTemporaryValueChange guard(&m_dontDisplayErrorMessage, true);
            m_diff.stop();

            std::optional<pdf::PDFDocument> document = openDocument();
            if (document)
            {
                clear(true, false);
                m_leftDocument = std::move(*document);

                const size_t pageCount = m_leftDocument.getCatalog()->getPageCount();
                if (pageCount > 1)
                {
                    ui->leftPageSelectionEdit->setText(QString("1-%2").arg(pageCount));
                }
                else if (pageCount == 1)
                {
                    ui->leftPageSelectionEdit->setText("1");
                }
                else
                {
                    ui->leftPageSelectionEdit->clear();
                }

                updateViewDocument();
            }

            break;
        }

        case Operation::OpenRight:
        {
            pdf::PDFTemporaryValueChange guard(&m_dontDisplayErrorMessage, true);
            m_diff.stop();

            std::optional<pdf::PDFDocument> document = openDocument();
            if (document)
            {
                clear(false, true);
                m_rightDocument = std::move(*document);

                const size_t pageCount = m_rightDocument.getCatalog()->getPageCount();
                if (pageCount > 1)
                {
                    ui->rightPageSelectionEdit->setText(QString("1-%2").arg(pageCount));
                }
                else if (pageCount == 1)
                {
                    ui->rightPageSelectionEdit->setText("1");
                }
                else
                {
                    ui->rightPageSelectionEdit->clear();
                }

                updateViewDocument();
            }

            break;
        }

        case Operation::Compare:
        {
            pdf::PDFTemporaryValueChange guard(&m_dontDisplayErrorMessage, true);
            m_diff.stop();

            QString errorMessage;

            pdf::PDFClosedIntervalSet rightPageIndices;
            pdf::PDFClosedIntervalSet leftPageIndices = pdf::PDFClosedIntervalSet::parse(1, qMax<pdf::PDFInteger>(1, m_leftDocument.getCatalog()->getPageCount()), ui->leftPageSelectionEdit->text(), &errorMessage);

            if (errorMessage.isEmpty())
            {
                rightPageIndices = pdf::PDFClosedIntervalSet::parse(1, qMax<pdf::PDFInteger>(1, m_rightDocument.getCatalog()->getPageCount()), ui->rightPageSelectionEdit->text(), &errorMessage);
            }

            // Check if pages are succesfully parsed
            if (!errorMessage.isEmpty())
            {
                QMessageBox::critical(this, tr("Error"), errorMessage);
                break;
            }

            leftPageIndices.translate(-1);
            rightPageIndices.translate(-1);

            m_diff.setPagesForLeftDocument(std::move(leftPageIndices));
            m_diff.setPagesForRightDocument(std::move(rightPageIndices));

            m_diff.start();
            break;
        }

        case Operation::Close:
        {
            close();
            break;
        }

        case Operation::GetSource:
        {
            QDesktopServices::openUrl(QUrl("https://github.com/JakubMelka/PDF4QT"));
            break;
        }

        case Operation::About:
        {
            PDFAboutDialog aboutDialog(this);
            aboutDialog.exec();
            break;
        }

        case Operation::PreviousDifference:
            m_diffNavigator.goPrevious();
            break;

        case Operation::NextDifference:
            m_diffNavigator.goNext();
            break;

        case Operation::FilterText:
        case Operation::FilterVectorGraphics:
        case Operation::FilterImages:
        case Operation::FilterShading:
        case Operation::FilterPageMovement:
        {
            updateFilteredResult();

            if (ui->actionShow_Pages_with_Differences->isChecked())
            {
                updateCustomPageLayout();
            }

            break;
        }

        case Operation::ViewDifferences:
        case Operation::ViewLeft:
        case Operation::ViewRight:
        case Operation::ViewOverlay:
            updateViewDocument();
            break;

        case Operation::ShowPageswithDifferences:
            updateCustomPageLayout();
            break;

        case Operation::SaveDifferencesToXML:
        case Operation::CreateCompareReport:
            Q_ASSERT(false);
            break;

        default:
        {
            Q_ASSERT(false);
            break;
        }
    }

    updateActions();
}

void MainWindow::setViewDocument(pdf::PDFDocument* document, bool updateCustomPageLayout)
{
    if (document != m_pdfWidget->getDrawWidgetProxy()->getDocument())
    {
        m_optionalContentActivity->deleteLater();
        m_optionalContentActivity = nullptr;

        if (document)
        {
            m_optionalContentActivity = new pdf::PDFOptionalContentActivity(document, pdf::OCUsage::View, this);
        }

        if (document)
        {
            pdf::PDFModifiedDocument modifiedDocument(document, m_optionalContentActivity);
            m_pdfWidget->setDocument(modifiedDocument);
        }
        else
        {
            m_pdfWidget->setDocument(pdf::PDFModifiedDocument());
        }
    }

    if (updateCustomPageLayout)
    {
        this->updateCustomPageLayout();
    }
}

ComparedDocumentMapper::Mode MainWindow::getDocumentViewMode() const
{
    if (ui->actionView_Left->isChecked())
    {
        return ComparedDocumentMapper::Mode::Left;
    }

    if (ui->actionView_Right->isChecked())
    {
        return ComparedDocumentMapper::Mode::Right;
    }

    if (ui->actionView_Overlay->isChecked())
    {
        return ComparedDocumentMapper::Mode::Overlay;
    }

    return ComparedDocumentMapper::Mode::Combined;
}

void MainWindow::clear(bool clearLeftDocument, bool clearRightDocument)
{
    setViewDocument(nullptr, true);

    if (clearLeftDocument)
    {
        m_leftDocument = pdf::PDFDocument();
        ui->leftPageSelectionEdit->clear();
    }

    if (clearRightDocument)
    {
        m_rightDocument = pdf::PDFDocument();
        ui->rightPageSelectionEdit->clear();
    }

    m_diffResult = pdf::PDFDiffResult();
    m_filteredDiffResult = pdf::PDFDiffResult();
    m_diffNavigator.update();

    updateAll(false);
}

void MainWindow::updateAll(bool resetFilters)
{
    if (resetFilters)
    {
        ui->actionFilter_Page_Movement->setChecked(m_diffResult.hasPageMoveDifferences());
        ui->actionFilter_Text->setChecked(m_diffResult.hasTextDifferences());
        ui->actionFilter_Vector_Graphics->setChecked(m_diffResult.hasVectorGraphicsDifferences());
        ui->actionFilter_Images->setChecked(m_diffResult.hasImageDifferences());
        ui->actionFilter_Shading->setChecked(m_diffResult.hasShadingDifferences());
    }

    updateFilteredResult();
    updateViewDocument();
}

void MainWindow::updateFilteredResult()
{
    m_filteredDiffResult = m_diffResult.filter(ui->actionFilter_Page_Movement->isChecked(),
                                               ui->actionFilter_Text->isChecked(),
                                               ui->actionFilter_Vector_Graphics->isChecked(),
                                               ui->actionFilter_Images->isChecked(),
                                               ui->actionFilter_Shading->isChecked());
    m_diffNavigator.update();

    if (m_differencesDockWidget)
    {
        m_differencesDockWidget->update();
    }

    updateActions();
}

void MainWindow::updateViewDocument()
{
    pdf::PDFDocument* document = nullptr;

    switch (getDocumentViewMode())
    {
        case ComparedDocumentMapper::Mode::Left:
            document = &m_leftDocument;
            break;

        case ComparedDocumentMapper::Mode::Right:
            document = &m_rightDocument;
            break;

        case ComparedDocumentMapper::Mode::Combined:
        case ComparedDocumentMapper::Mode::Overlay:
            document = &m_combinedDocument;
            break;
    }

    setViewDocument(document, true);
}

void MainWindow::updateCustomPageLayout()
{
    m_documentMapper.update(getDocumentViewMode(),
                            ui->actionShow_Pages_with_Differences->isChecked(),
                            m_filteredDiffResult,
                            &m_leftDocument,
                            &m_rightDocument,
                            m_pdfWidget->getDrawWidgetProxy()->getDocument());


    m_pdfWidget->getDrawWidgetProxy()->setCustomPageLayout(m_documentMapper.getLayout());
    m_pdfWidget->getDrawWidgetProxy()->setPageLayout(pdf::PageLayout::Custom);
}

std::optional<pdf::PDFDocument> MainWindow::openDocument()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select PDF document"), m_settings.directory, tr("PDF document (*.pdf)"));
    if (fileName.isEmpty())
    {
        return std::nullopt;
    }

    auto queryPassword = [this](bool* ok)
    {
        *ok = false;
        return QInputDialog::getText(this, tr("Encrypted document"), tr("Enter password to access document content"), QLineEdit::Password, QString(), ok);
    };

    // Mark current directory as this
    QFileInfo fileInfo(fileName);
    m_settings.directory = fileInfo.dir().absolutePath();

    // Try to open a new document
    pdf::PDFDocumentReader reader(nullptr, qMove(queryPassword), true, false);
    pdf::PDFDocument document = reader.readFromFile(fileName);

    QString errorMessage = reader.getErrorMessage();
    pdf::PDFDocumentReader::Result result = reader.getReadingResult();
    if (result == pdf::PDFDocumentReader::Result::OK)
    {
        return document;
    }
    else if (result == pdf::PDFDocumentReader::Result::Failed)
    {
        QMessageBox::critical(this, tr("Error"), errorMessage);
    }

    return pdf::PDFDocument();
}

void MainWindow::onProgressStarted(pdf::ProgressStartupInfo info)
{
    m_progressTaskbarIndicator->setRange(0, 100);
    m_progressTaskbarIndicator->reset();
    m_progressTaskbarIndicator->show();
}

void MainWindow::onProgressStep(int percentage)
{
    if (m_isChangingProgressStep)
    {
        return;
    }

    pdf::PDFTemporaryValueChange guard(&m_isChangingProgressStep, true);
    m_progressTaskbarIndicator->setValue(percentage);
}

void MainWindow::onProgressFinished()
{
    m_progressTaskbarIndicator->hide();
}

void ComparedDocumentMapper::update(ComparedDocumentMapper::Mode mode,
                                    bool filterDifferences,
                                    const pdf::PDFDiffResult& diff,
                                    const pdf::PDFDocument* leftDocument,
                                    const pdf::PDFDocument* rightDocument,
                                    const pdf::PDFDocument* currentDocument)
{
    m_layout.clear();

    m_leftPageIndices.clear();
    m_rightPageIndices.clear();

    m_allLeft = false;
    m_allRight = false;

    if (!leftDocument || !rightDocument || !currentDocument)
    {
        return;
    }

    // Jakub Melka
    pdf::PDFDiffResult::PageSequence pageSequence = diff.getPageSequence();
    const bool isEmpty = pageSequence.empty();

    if (filterDifferences)
    {
        pdf::PDFDiffResult::PageSequence filteredPageSequence;

        std::vector<pdf::PDFInteger> leftPageIndices = diff.getChangedLeftPageIndices();
        std::vector<pdf::PDFInteger> rightPageIndices = diff.getChangedRightPageIndices();

        for (const pdf::PDFDiffResult::PageSequenceItem& item : pageSequence)
        {
            const bool isLeftModified = std::binary_search(leftPageIndices.cbegin(), leftPageIndices.cend(), item.leftPage);
            const bool isRightModified = std::binary_search(rightPageIndices.cbegin(), rightPageIndices.cend(), item.rightPage);

            if (isLeftModified || isRightModified)
            {
                filteredPageSequence.push_back(item);
            }
        }

        pageSequence = std::move(filteredPageSequence);
    }

    switch (mode)
    {
        case ComparedDocumentMapper::Mode::Left:
        {
            Q_ASSERT(leftDocument == currentDocument);

            m_allLeft = true;
            double yPos = 0.0;
            const pdf::PDFCatalog* catalog = leftDocument->getCatalog();

            if (isEmpty)
            {
                // Just copy all pages
                const size_t pageCount = catalog->getPageCount();
                for (size_t i = 0; i < pageCount; ++i)
                {
                    QSizeF pageSize = catalog->getPage(i)->getRotatedMediaBoxMM().size();
                    QRectF rect(-pageSize.width() * 0.5, yPos, pageSize.width(), pageSize.height());
                    m_layout.emplace_back(0, i, rect);
                    yPos += pageSize.height() + 5;
                }
            }
            else
            {
                for (const pdf::PDFDiffResult::PageSequenceItem& item : pageSequence)
                {
                    if (item.leftPage == -1)
                    {
                        continue;
                    }

                    QSizeF pageSize = catalog->getPage(item.leftPage)->getRotatedMediaBoxMM().size();
                    QRectF rect(-pageSize.width() * 0.5, yPos, pageSize.width(), pageSize.height());
                    m_layout.emplace_back(0, item.leftPage, rect);
                    yPos += pageSize.height() + 5;
                }
            }

            break;
        }

        case ComparedDocumentMapper::Mode::Right:
        {
            Q_ASSERT(rightDocument == currentDocument);

            m_allRight = true;
            double yPos = 0.0;
            const pdf::PDFCatalog* catalog = rightDocument->getCatalog();

            if (isEmpty)
            {
                // Just copy all pages
                const size_t pageCount = catalog->getPageCount();
                for (size_t i = 0; i < pageCount; ++i)
                {
                    QSizeF pageSize = catalog->getPage(i)->getRotatedMediaBoxMM().size();
                    QRectF rect(-pageSize.width() * 0.5, yPos, pageSize.width(), pageSize.height());
                    m_layout.emplace_back(0, i, rect);
                    yPos += pageSize.height() + 5;
                }
            }
            else
            {
                for (const pdf::PDFDiffResult::PageSequenceItem& item : pageSequence)
                {
                    if (item.rightPage == -1)
                    {
                        continue;
                    }

                    QSizeF pageSize = catalog->getPage(item.rightPage)->getRotatedMediaBoxMM().size();
                    QRectF rect(-pageSize.width() * 0.5, yPos, pageSize.width(), pageSize.height());
                    m_layout.emplace_back(0, item.rightPage, rect);
                    yPos += pageSize.height() + 5;
                }
            }

            break;
        }

        case ComparedDocumentMapper::Mode::Combined:
        case ComparedDocumentMapper::Mode::Overlay:
        {
            double yPos = 0.0;
            const pdf::PDFCatalog* catalog = currentDocument->getCatalog();
            pdf::PDFInteger offset = leftDocument->getCatalog()->getPageCount();

            for (const pdf::PDFDiffResult::PageSequenceItem& item : pageSequence)
            {
                double yAdvance = 0.0;

                if (item.leftPage != -1)
                {
                    QSizeF pageSize = catalog->getPage(item.leftPage)->getRotatedMediaBoxMM().size();
                    QRectF rect;
                    if (mode == ComparedDocumentMapper::Mode::Combined)
                    {
                        rect = QRectF(-pageSize.width() - 5, yPos, pageSize.width(), pageSize.height());
                    }
                    else
                    {
                        rect = QRectF(-pageSize.width() * 0.5, yPos, pageSize.width(), pageSize.height());
                    }
                    m_layout.emplace_back(0, item.leftPage, rect);
                    yAdvance = pageSize.height() + 5;
                    m_leftPageIndices[item.leftPage] = item.leftPage;
                }

                if (item.rightPage != -1)
                {
                    pdf::PDFInteger rightPageIndex = item.rightPage + offset;
                    QSizeF pageSize = catalog->getPage(rightPageIndex)->getRotatedMediaBoxMM().size();
                    QRectF rect;
                    if (mode == ComparedDocumentMapper::Mode::Combined)
                    {
                        rect = QRectF(5, yPos, pageSize.width(), pageSize.height());
                    }
                    else
                    {
                        rect = QRectF(-pageSize.width() * 0.5, yPos, pageSize.width(), pageSize.height());
                    }
                    m_layout.emplace_back(0, rightPageIndex, rect);
                    yAdvance = qMax(yAdvance, pageSize.height() + 5);
                    m_rightPageIndices[rightPageIndex] = item.rightPage;
                }

                yPos += yAdvance;
            }

            break;
        }

        default:
            Q_ASSERT(false);
            break;
    }
}

pdf::PDFInteger ComparedDocumentMapper::getLeftPageIndex(pdf::PDFInteger pageIndex) const
{
    if (m_allLeft)
    {
        return pageIndex;
    }

    auto it = m_leftPageIndices.find(pageIndex);
    if (it != m_leftPageIndices.cend())
    {
        return it->second;
    }

    return -1;
}

pdf::PDFInteger ComparedDocumentMapper::getRightPageIndex(pdf::PDFInteger pageIndex) const
{
    if (m_allRight)
    {
        return pageIndex;
    }

    auto it = m_rightPageIndices.find(pageIndex);
    if (it != m_rightPageIndices.cend())
    {
        return it->second;
    }

    return -1;
}

pdf::PDFInteger ComparedDocumentMapper::getPageIndexFromLeftPageIndex(pdf::PDFInteger leftPageIndex) const
{
    if (m_allLeft)
    {
        return leftPageIndex;
    }

    for (const auto& indexItem : m_leftPageIndices)
    {
        if (indexItem.second == leftPageIndex)
        {
            return indexItem.first;
        }
    }

    return -1;
}

pdf::PDFInteger ComparedDocumentMapper::getPageIndexFromRightPageIndex(pdf::PDFInteger rightPageIndex) const
{
    if (m_allRight)
    {
        return rightPageIndex;
    }

    for (const auto& indexItem : m_rightPageIndices)
    {
        if (indexItem.second == rightPageIndex)
        {
            return indexItem.first;
        }
    }

    return -1;
}

}   // namespace pdfdocdiff
