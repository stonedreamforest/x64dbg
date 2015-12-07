#include "SearchListView.h"
#include "ui_SearchListView.h"
#include "FlickerThread.h"

#include <QMessageBox>

SearchListView::SearchListView(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::SearchListView)
{
    ui->setupUi(this);

    setContextMenuPolicy(Qt::CustomContextMenu);
    mCursorPosition = 0;
    // Create the reference list
    mList = new SearchListViewTable();

    // Create the search list
    mSearchList = new SearchListViewTable();
    mSearchList->hide();

    // Set global variables
    mSearchBox = ui->searchBox;
    mCurList = mList;
    mSearchStartCol = 0;

    // Create list layout
    mListLayout = new QVBoxLayout();
    mListLayout->setContentsMargins(0, 0, 0, 0);
    mListLayout->setSpacing(0);
    mListLayout->addWidget(mList);
    mListLayout->addWidget(mSearchList);

    // Create list placeholder
    mListPlaceHolder = new QWidget();
    mListPlaceHolder->setLayout(mListLayout);

    // Insert the placeholder
    ui->mainSplitter->insertWidget(0, mListPlaceHolder);

    // Set the main layout
    mMainLayout = new QVBoxLayout();
    mMainLayout->setContentsMargins(0, 0, 0, 0);
    mMainLayout->addWidget(ui->mainSplitter);
    setLayout(mMainLayout);

    // Minimal size for the search box
    ui->mainSplitter->setStretchFactor(0, 1000);
    ui->mainSplitter->setStretchFactor(0, 1);

    // Disable main splitter
    for(int i = 0; i < ui->mainSplitter->count(); i++)
        ui->mainSplitter->handle(i)->setEnabled(false);

    // Install eventFilter
    mList->installEventFilter(this);
    mSearchList->installEventFilter(this);
    mSearchBox->installEventFilter(this);

    // Setup search menu action
    mSearchAction = new QAction("Search...", this);
    connect(mSearchAction, SIGNAL(triggered()), this, SLOT(searchSlot()));

    // Slots
//    connect(mList, SIGNAL(keyPressedSignal(QKeyEvent*)), this, SLOT(listKeyPressed(QKeyEvent*)));
    connect(mList, SIGNAL(contextMenuSignal(QPoint)), this, SLOT(listContextMenu(QPoint)));
    connect(mList, SIGNAL(doubleClickedSignal()), this, SLOT(doubleClickedSlot()));
//    connect(mSearchList, SIGNAL(keyPressedSignal(QKeyEvent*)), this, SLOT(listKeyPressed(QKeyEvent*)));
    connect(mSearchList, SIGNAL(contextMenuSignal(QPoint)), this, SLOT(listContextMenu(QPoint)));
    connect(mSearchList, SIGNAL(doubleClickedSignal()), this, SLOT(doubleClickedSlot()));
    connect(mSearchBox, SIGNAL(textChanged(QString)), this, SLOT(searchTextChanged(QString)));
}

SearchListView::~SearchListView()
{
    delete ui;
}

bool SearchListView::findTextInList(SearchListViewTable* list, QString text, int row, int startcol, bool startswith)
{
    int count = list->getColumnCount();
    if(startcol + 1 > count)
        return false;
    if(startswith)
    {
        for(int i = startcol; i < count; i++)
            if(list->getCellContent(row, i).startsWith(text, Qt::CaseInsensitive))
                return true;
    }
    else
    {
        for(int i = startcol; i < count; i++)
        {
            if(ui->checkBoxRegex->checkState() == Qt::Checked)
            {
                if(list->getCellContent(row, i).contains(QRegExp(text)))
                    return true;
            }
            else
            {
                if(list->getCellContent(row, i).contains(text, Qt::CaseInsensitive))
                    return true;
            }
        }
    }
    return false;
}

void SearchListView::searchTextChanged(const QString & arg1)
{
    if(arg1.length())
    {
        mList->hide();
        mSearchList->show();
        mCurList = mSearchList;
    }
    else
    {
        mSearchList->hide();
        mList->show();
        if(ui->checkBoxRegex->checkState() != Qt::Checked)
            mList->setFocus();
        mCurList = mList;
    }
    mSearchList->setRowCount(0);
    int rows = mList->getRowCount();
    int columns = mList->getColumnCount();
    for(int i = 0, j = 0; i < rows; i++)
    {
        if(findTextInList(mList, arg1, i, mSearchStartCol, false))
        {
            mSearchList->setRowCount(j + 1);
            for(int k = 0; k < columns; k++)
                mSearchList->setCellContent(j, k, mList->getCellContent(i, k));
            j++;
        }
    }
    rows = mSearchList->getRowCount();
    mSearchList->setTableOffset(0);
    for(int i = 0; i < rows; i++)
    {
        if(findTextInList(mSearchList, arg1, i, mSearchStartCol, true))
        {
            if(rows > mSearchList->getViewableRowsCount())
            {
                int cur = i - mSearchList->getViewableRowsCount() / 2;
                if(!mSearchList->isValidIndex(cur, 0))
                    cur = i;
                mSearchList->setTableOffset(cur);
            }
            mSearchList->setSingleSelection(i);
            break;
        }
    }

    if(rows == 0)
        emit emptySearchResult();

    if(ui->checkBoxRegex->checkState() != Qt::Checked) //do not highlight with regex
        mSearchList->highlightText = arg1;
    mSearchList->reloadData();
    if(ui->checkBoxRegex->checkState() != Qt::Checked)
        mSearchList->setFocus();
}

void SearchListView::listContextMenu(const QPoint & pos)
{
    QMenu* wMenu = new QMenu(this);
    emit listContextMenuSignal(wMenu);
    wMenu->addSeparator();
    wMenu->addAction(mSearchAction);
    QMenu wCopyMenu("&Copy", this);
    mCurList->setupCopyMenu(&wCopyMenu);
    if(wCopyMenu.actions().length())
        wMenu->addMenu(&wCopyMenu);
    wMenu->exec(mCurList->mapToGlobal(pos));
}

void SearchListView::doubleClickedSlot()
{
    emit enterPressedSignal();
}

void SearchListView::on_checkBoxRegex_toggled(bool checked)
{
    Q_UNUSED(checked);
    searchTextChanged(ui->searchBox->text());
}

bool SearchListView::eventFilter(QObject *obj, QEvent *event)
{
    // Click in searchBox
    if(obj == mSearchBox && event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent* >(event);
        QLineEdit *lineEdit = static_cast<QLineEdit* >(obj);

        mCursorPosition = lineEdit->cursorPositionAt(mouseEvent->pos());
        lineEdit->setFocusPolicy(Qt::NoFocus);

        return QObject::eventFilter(obj, event);
    }
    // KeyPress in List
    else if((obj == mList || obj == mSearchList) && event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent* >(event);
        char ch = keyEvent->text().toUtf8().constData()[0];

        if(isprint(ch)) //add a char to the search box
        {
            mSearchBox->setText(mSearchBox->text().insert(mSearchBox->cursorPosition(), QString(QChar(ch))));
            mCursorPosition++;
        }
        else if(keyEvent->key() == Qt::Key_Backspace) //remove a char from the search box
        {
            QString newText;
            if(keyEvent->modifiers() == Qt::ControlModifier) //clear the search box
            {
                newText = "";
                mCursorPosition = 0;
            }
            else
            {
                newText = mSearchBox->text();

                if(mCursorPosition != 0)
                    newText.remove(mCursorPosition, 1);

                ((mCursorPosition - 1) < 0) ? mCursorPosition = 0 : mCursorPosition--;
            }
            mSearchBox->setText(newText);
        }
        else if((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)) //user pressed enter
        {
            if(mCurList->getCellContent(mCurList->getInitialSelection(), 0).length())
                emit enterPressedSignal();
        }
        else if(keyEvent->key() == Qt::Key_Escape) // Press escape, clears the search box
        {
            mSearchBox->clear();
            mCursorPosition = 0;
        }

        // Update cursorPosition to avoid a weird bug
        mSearchBox->setCursorPosition(mCursorPosition);

        return true;
    }
    else
        return QObject::eventFilter(obj, event);
}

void SearchListView::searchSlot()
{
    FlickerThread* thread = new FlickerThread(ui->searchBox, this);
    connect(thread, SIGNAL(setStyleSheet(QString)), ui->searchBox, SLOT(setStyleSheet(QString)));
    thread->start();
}
