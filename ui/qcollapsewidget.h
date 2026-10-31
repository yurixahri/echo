#ifndef QCOLLAPSEWIDGET_H
#define QCOLLAPSEWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QApplication>

class QCollapseWidget : public QWidget
{
    Q_OBJECT
public:
    explicit QCollapseWidget(QString text, QString icon_name, QWidget *parent = nullptr);
    QWidget *header;
    QWidget *content;
    QLabel *icon = nullptr;
    QLabel *title;
    QPushButton *collapse;
    QString current_icon_name;

    bool expanded = true;

    void expand();
    void update_theme_icons();
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
signals:

};

#endif // QCOLLAPSEWIDGET_H
