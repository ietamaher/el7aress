#include "custommenudialog.h"
#include "models/systemstatemodel.h"

#include <QCloseEvent>
#include <QListWidgetItem>
#include <QDebug>

CustomMenuWidget::CustomMenuWidget(const QStringList &options,
                                   SystemStateModel *stateModel,
                                   QWidget *parent)
    : QWidget(parent),
    m_stateModel(stateModel),
    m_listWidget(new QListWidget(this))
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    //setAttribute(Qt::WA_DeleteOnClose);

    // Transparent BG
    setStyleSheet("background-color: rgba(0, 0, 0, 0);");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_listWidget);

    m_listWidget->addItems(options);
    m_listWidget->setCurrentRow(0);

    // Size/position as needed
    resize(300, 400);
    move(470, 100);

    // If we want to react to m_stateModel color changes:
    if (m_stateModel) {
        connect(m_stateModel, &SystemStateModel::colorStyleChanged,
                this,          &CustomMenuWidget::onColorStyleChanged);
    }
}

void CustomMenuWidget::moveSelectionUp()
{
    int currentRow = m_listWidget->currentRow();
    if (currentRow > 0) {
        m_listWidget->setCurrentRow(currentRow - 1);
    }
    QListWidgetItem *item = m_listWidget->currentItem();
    if (item) {
        emit currentItemChanged(item->text());
    }
}

void CustomMenuWidget::moveSelectionDown()
{
    int currentRow = m_listWidget->currentRow();
    if (currentRow < m_listWidget->count() - 1) {
        m_listWidget->setCurrentRow(currentRow + 1);
    }
    QListWidgetItem *item = m_listWidget->currentItem();
    if (item) {
        emit currentItemChanged(item->text());
    }
}

void CustomMenuWidget::selectCurrentItem()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (item) {
        emit optionSelected(item->text());
        close();
        emit menuClosed();
    }
}

void CustomMenuWidget::onColorStyleChanged(const QString &style)
{
    // same logic as setColorStyleChanged(...)
    setColorStyleChanged(style);
}

// Manually set color style if m_stateModel changes after creation
void CustomMenuWidget::setColorStyleChanged(const QString &style)
{
    m_currentColorStyle = style;
    if (m_currentColorStyle == "Red") {
        m_listWidget->setStyleSheet(
            "QListWidget {background-color: rgba(0,0,0,100); color: rgba(200,0,0,255); font: 700 14pt 'Courier New';}"
            "QListWidget::item:selected {color: white; background: rgba(200,0,0,255); border:1px solid white;}"
            );
    }
    else if (m_currentColorStyle == "Green") {
        m_listWidget->setStyleSheet(
            "QListWidget {background-color: rgba(0,0,0,100); color: rgba(0,212,76,255); font:700 14pt 'Courier New';}"
            "QListWidget::item:selected {color:white; background:rgba(0,212,76,255); border:1px solid white;}"
            );
    }
    else if (m_currentColorStyle == "White") {
        m_listWidget->setStyleSheet(
            "QListWidget {background-color: rgba(0,0,0,100); color: rgba(255,255,255,255); font:700 14pt 'Courier New';}"
            "QListWidget::item:selected {color:white; background:rgba(255,255,255,255); color:rgba(0,0,0,255); border:1px solid white;}"
            );
    }
    else {
        // default
        m_listWidget->setStyleSheet(
            "QListWidget {background-color: rgba(0,0,0,100); color: rgba(0,212,76,255); font:700 14pt 'Courier New';}"
            "QListWidget::item:selected {color:white; background:rgba(0,212,76,255); border:1px solid white;}"
            );
    }
}

QString CustomMenuWidget::currentItemText() const
{
    QListWidgetItem *item = m_listWidget->currentItem();
    return item ? item->text() : QString();
}

void CustomMenuWidget::closeEvent(QCloseEvent *event)
{
    QWidget::closeEvent(event);
    emit menuClosed();
}


