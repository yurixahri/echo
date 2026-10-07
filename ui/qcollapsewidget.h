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
    QLabel *icon;
    QLabel *title;
    QPushButton *collapse;

    bool expanded = true;

    void expand();
signals:

};

#endif // QCOLLAPSEWIDGET_H
