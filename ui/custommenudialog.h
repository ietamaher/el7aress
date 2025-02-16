#ifndef CUSTOMMENUWIDGET_H
#define CUSTOMMENUWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QStringList>

class SystemStateModel;

class CustomMenuWidget : public QWidget {
    Q_OBJECT
public:
    explicit CustomMenuWidget(const QStringList &options,
                              SystemStateModel *stateModel,
                              QWidget *parent = nullptr);

    void moveSelectionUp();
    void moveSelectionDown();
    void selectCurrentItem();
    void setColorStyleChanged(const QString &style); // manually update color

signals:
    void optionSelected(const QString &option);
    void menuClosed();
    void currentItemChanged(const QString &currentItem);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onColorStyleChanged(const QString &style);

private:
    QString currentItemText() const;

private:
    SystemStateModel *m_stateModel = nullptr;
    QString m_currentColorStyle;
    QListWidget *m_listWidget = nullptr;
};

#endif // CUSTOMMENUWIDGET_H


