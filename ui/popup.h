#ifndef POPUP_H
#define POPUP_H

#include <QWidget>
#include <QPointer>
#include <QGraphicsBlurEffect>

class QGridLayout;

class QPopup : public QWidget
{
    Q_OBJECT
public:
    explicit QPopup(QWidget *parent = nullptr, int gap_x = 20, int gap_y = 25);
    ~QPopup() override;

    QWidget *panel;

    void set_gaps(int gap_x, int gap_y);
    void set_panel_width(int min_w, int max_w);
    void apply_blur(qreal radius = 5);
    void reposition();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QGridLayout *main_grid = nullptr;
    QPointer<QWidget> target_parent;
    QPointer<QGraphicsBlurEffect> blur_effect;

signals:

};

#endif // POPUP_H
