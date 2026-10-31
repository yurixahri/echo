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
#include <QStackedLayout>
#include "ui/server_header.h"
#include "ui/room_view.h"
#include "ui/chat_area.h"
#include "ui/voice_panel.h"

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
    QWidget *main_panel;
    QWidget *title_bar;
    QWidget *side_bar;
    QWidget *main_area;
    server_header_t server_header;
    QWidget *server_content = nullptr;
    QWidget *room_container = nullptr;
    chat_area_t chat_area;
    voice_panel_t voice_panel;
    QPushButton *sidebar_settings_btn = nullptr;
    QLabel *sidebar_logo = nullptr;
    class QCollapseWidget *server_list_widget = nullptr;
    QList<room_t> current_rooms;

    QString get_room_name(uint64_t room_id) const;
    void create_title_bar();
    void create_side_bar();
    void create_main_area();
    void set_ui_style();
    void update_border_cursor(const QPoint &global_pos);
protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
};
#endif // MAINWINDOW_H
