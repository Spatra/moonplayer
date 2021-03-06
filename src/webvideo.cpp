#include "webvideo.h"
#include <QMessageBox>
#include <QDir>
#include <QGridLayout>
#include <QPushButton>
#include <QListWidget>
#include <QSpacerItem>
#include <QComboBox>
#include <QInputDialog>
#include <QShowEvent>
#include <QDesktopWidget>
#include <QApplication>
#include <QLabel>
#include <QUrl>
#include "plugins.h"
#include "pyapi.h"
#include "utils.h"
#include <iostream>

WebVideo *webvideo = NULL;

WebVideo::WebVideo(QWidget *parent) :
    QTabWidget(parent)
{
    std::cout << "Initialize webview..." << std::endl;
    webvideo = this;
    setObjectName("WebVideo");

    initPlugins();
    if (n_plugins == 0)
    {
        QLabel *label = new QLabel(tr("You have not install any plugins yet ~_~"));
        addTab(label, tr("Web videos"));
        return;
    }

    QPushButton *downButton = new QPushButton(tr("Down"));
    QPushButton *albumButton = new QPushButton(tr("Album"));
    lineEdit = new QLineEdit;
    prevButton = new QPushButton(tr("Prev"));
    nextButton = new QPushButton(tr("Next"));
    backButton = new QPushButton(tr("Back"));
    backButton->setEnabled(false);
    listWidget = new QListWidget;
    comboBox = new QComboBox;
    QWidget *page = new QWidget;
    QGridLayout *grid = new QGridLayout(page);
    addTab(page, tr("Web videos"));
    grid->addWidget(comboBox, 0, 0, 1, 1);
    grid->addWidget(lineEdit, 0, 1, 1, 3);
    grid->addWidget(albumButton, 0, 4, 1, 1);
    grid->addWidget(listWidget, 1, 0, 1, 5);
    grid->addWidget(downButton, 2, 0, 1, 1);
    grid->addWidget(prevButton, 2, 2, 1, 1);
    grid->addWidget(nextButton, 2, 3, 1, 1);
    grid->addWidget(backButton, 2, 4, 1, 1);
    grid->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding), 2, 1);
    setMinimumSize(950, 500);

    //down search page and parse
    connect(nextButton, SIGNAL(clicked()), this, SLOT(nextSearchPage()));
    connect(prevButton, SIGNAL(clicked()), this, SLOT(prevSearchPage()));
    connect(backButton, SIGNAL(clicked()), this, SLOT(backSearchPage()));
    connect(lineEdit, SIGNAL(returnPressed()), this, SLOT(searchVideo()));
    connect(albumButton, SIGNAL(clicked()), this, SLOT(searchAlbum()));

    //Download video file
    connect(downButton, SIGNAL(clicked()), this, SLOT(onDownButton()));
    connect(listWidget, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(onDoubleClicked(QListWidgetItem*)));

    //plugins support
    provider = 0;
    for (int i = 0; i < n_plugins; i++)
        comboBox->addItem(plugins[i]->getName());
}

void WebVideo::showEvent(QShowEvent *event)
{
    QDesktopWidget *desktop = qApp->desktop();
    move((desktop->width() - width())/2,
         (desktop->height() - height())/2);
    event->accept();
}

void WebVideo::warnHavingTask()
{
    QMessageBox::warning(this, "warning", tr("Another file is parsing. Please wait."));
}


//Search videos
void WebVideo::searchVideo()
{
    if (geturl_obj->hasTask())
    {
        warnHavingTask();
        return;
    }
    QString key = lineEdit->text().simplified();
    if (key.isEmpty())
        return;
    provider = comboBox->currentIndex();
    type = TYPE_VIDEO;
    keyword = key;
    downSearchPage();
}

void WebVideo::searchAlbum()
{
    if (geturl_obj->hasTask())
    {
        warnHavingTask();
        return;
    }
    int pv = comboBox->currentIndex();
    if (!plugins[pv]->supportAlbum())
    {
        QMessageBox::warning(this, "Warning", tr("Album is not supported."));
        return;
    }
    QString key = lineEdit->text().simplified();
    if (key.isEmpty())
        return;
    provider = pv;
    type = TYPE_ALBUM;
    keyword = key;
    downSearchPage();
}

void WebVideo::downSearchPage()
{
    // no plugins, return
    if (geturl_obj->hasTask() || keyword.isEmpty() || !n_plugins)
        return;
    // set ui
    page_n = 1;
    prevButton->setEnabled(false);
    backButton->setEnabled(false);
    nextButton->setEnabled(true);
    // call plugin
    if (type == TYPE_ALBUM)
        plugins[provider]->searchAlbum(keyword, 1);
    else
        plugins[provider]->search(keyword, 1);
}

void WebVideo::nextSearchPage()
{
    if (geturl_obj->hasTask())
    {
        warnHavingTask();
        return;
    }
    if (keyword.isEmpty())
        return;
    // set ui
    page_n++;
    prevButton->setEnabled(true);
    backButton->setEnabled(false);
    nextButton->setEnabled(true);
    //call plugin
    if (type == TYPE_ALBUM)
        plugins[provider]->searchAlbum(keyword, page_n);
    else
        plugins[provider]->search(keyword, page_n);
}

void WebVideo::prevSearchPage()
{
    if (geturl_obj->hasTask())
    {
        warnHavingTask();
        return;
    }
    if (keyword.isEmpty())
        return;
    //set ui
    page_n--;
    if (page_n == 1)
        prevButton->setEnabled(false);
    else
        prevButton->setEnabled(true);
    backButton->setEnabled(false);
    nextButton->setEnabled(true);
    //call plugin
    if (type == TYPE_ALBUM)
        plugins[provider]->searchAlbum(keyword, page_n);
    else
        plugins[provider]->search(keyword, page_n);
}

void WebVideo::backSearchPage()
{
    page_n++;
    prevSearchPage();
}

PyObject* WebVideo::showList(PyObject *list)
{
    listWidget->clear();
    result.clear();
    int size = PyList_Size(list);
    if (size < 0)
        return NULL;
    PyObject *item;
    const char *str;
    for (int i = 0; i < size; i+=2)
    {
         if ((item = PyList_GetItem(list, i)) == NULL)
             return NULL;
        listWidget->addItem(PyString_AsQString(item));
        if ((item = PyList_GetItem(list, i+1)) == NULL)
            return NULL;
        if ((str = PyString_AsString(item)) == NULL)
            return NULL;
        result.append(str);
    }
    show();
    activateWindow();
    setCurrentIndex(1);
    Py_IncRef(Py_None);
    return Py_None;
}

PyObject* WebVideo::showAlbum(PyObject *list)
{
    nextButton->setEnabled(false);
    prevButton->setEnabled(false);
    backButton->setEnabled(true);
    return showList(list);
}

void WebVideo::setListItemColor(int n, const QColor &color)
{
    QListWidgetItem *item = listWidget->item(n);
    if (item)
        item->setForeground(QBrush(color));
}

// Parse video url and play or download
void WebVideo::onDoubleClicked(QListWidgetItem *item)
{
    if (geturl_obj->hasTask())
    {
        warnHavingTask();
        return;
    }
    int i = listWidget->row(item);
    QByteArray url = result[i];
    Plugin *plugin = getPluginByHost(QUrl(QString::fromUtf8(url)).host());
    if (plugin == 0)
        plugin = plugins[provider];
    plugin->parse(url.constData(), false);
}


void WebVideo::onDownButton()
{
    if (geturl_obj->hasTask())
    {
        warnHavingTask();
        return;
    }
    int i = listWidget->currentRow();
    if (i == -1)
        return;
    QByteArray url = result[i];
    Plugin *plugin = getPluginByHost(QUrl(QString::fromUtf8(url)).host());
    if (plugin == 0)
        plugin = plugins[provider];
    plugin->parse(result[i], true);
}
