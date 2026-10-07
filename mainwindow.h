#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMouseEvent>
#include <QLabel>
#include <QFile>
#include <QWindow>
#include <QSplitter>
#include <QLineEdit>
#include <QCheckBox>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    Ui::MainWindow *ui;
    QVBoxLayout *parent_layout;
    QSplitter *main_splitter;
    QWidget *title_bar;
    QWidget *side_bar;
    QWidget *main_area;

    void create_title_bar();
    void create_side_bar();
    void create_main_area();
    void set_ui_style();
protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
};
#endif // MAINWINDOW_H
