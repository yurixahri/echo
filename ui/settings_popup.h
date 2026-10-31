#ifndef SETTINGS_POPUP_H
#define SETTINGS_POPUP_H

#include <QComboBox>
#include <QButtonGroup>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QFrame>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDesktopServices>
#include <QUrl>
#include "functions/voice.h"
#include "functions/sound.h"
#include "ui/popup.h"
#include "utils/theme.h"

inline QScrollArea* make_settings_scroll_page(QWidget *content_widget) {
    QScrollArea *scroll = new QScrollArea();
    scroll->setObjectName("settings_page_scroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    if (content_widget) {
        content_widget->setObjectName("settings_page_content");
    }
    scroll->setWidget(content_widget);
    return scroll;
}

inline QWidget* create_general_settings_page() {
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    QLabel *title = new QLabel("General");
    title->setObjectName("popup_section_title");
    layout->addWidget(title);

    QLabel *theme_label = new QLabel("Theme (Appearance)");
    theme_label->setObjectName("popup_muted_lbl");
    layout->addWidget(theme_label);

    QComboBox *theme_combo = new QComboBox();
    theme_combo->setFixedHeight(32);
    style_combobox(theme_combo);

    QStringList themes = get_available_themes();
    for (const QString &t : themes) {
        theme_combo->addItem(t, t);
    }
    int cur_idx = theme_combo->findData(theme_path);
    if (cur_idx >= 0) theme_combo->setCurrentIndex(cur_idx);

    QObject::connect(theme_combo, qOverload<int>(&QComboBox::currentIndexChanged), [theme_combo](int index) {
        QString sel_theme = theme_combo->itemData(index).toString();
        if (!sel_theme.isEmpty() && on_theme_changed) {
            on_theme_changed(sel_theme);
        }
    });
    layout->addWidget(theme_combo);

    layout->addSpacing(14);
    QLabel *storage_title = new QLabel("Storage & Cache");
    storage_title->setObjectName("popup_section_title");
    layout->addWidget(storage_title);

    QLabel *cache_desc = new QLabel("Temporary preview images stored in %temp% can be cleared:");
    cache_desc->setObjectName("popup_muted_lbl");
    layout->addWidget(cache_desc);

    QPushButton *purge_btn = new QPushButton("Purge Temporary Image Cache");
    purge_btn->setObjectName("popup_secondary_btn");
    purge_btn->setCursor(Qt::PointingHandCursor);
    purge_btn->setFixedHeight(30);

    QLabel *purge_status = new QLabel("");
    purge_status->setObjectName("purge_status_lbl");

    QObject::connect(purge_btn, &QPushButton::clicked, [purge_btn, purge_status]() {
        clear_temporary_image_cache();
        purge_status->setText("Temporary preview cache purged successfully!");
    });

    layout->addWidget(purge_btn);
    layout->addWidget(purge_status);

    layout->addStretch(1);
    return page;
}

inline QWidget* create_voice_settings_page() {
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    QLabel *title = new QLabel("Voice Settings");
    title->setObjectName("popup_section_title");
    layout->addWidget(title);

    QLabel *input_label = new QLabel("Input Device (Microphone)");
    input_label->setObjectName("popup_muted_lbl");
    layout->addWidget(input_label);

    QComboBox *input_combo = new QComboBox();
    input_combo->setFixedHeight(32);
    style_combobox(input_combo);
    input_combo->addItem("System default device", QByteArray());
    int selected_index = 0;
    for (const QAudioDevice &device : QMediaDevices::audioInputs()) {
        input_combo->addItem(device.description(), device.id());
        if (!voice_ui_state.input_device_id.isEmpty() && device.id() == voice_ui_state.input_device_id) {
            selected_index = input_combo->count() - 1;
        }
    }
    input_combo->setCurrentIndex(selected_index);
    QObject::connect(input_combo, qOverload<int>(&QComboBox::currentIndexChanged),
                     [input_combo](int index) {
        voice_set_input_device(input_combo->itemData(index).toByteArray());
    });
    layout->addWidget(input_combo);

    layout->addSpacing(8);
    QLabel *mic_vol_label = new QLabel(QString("Microphone Volume: %1%").arg(voice_ui_state.input_volume));
    mic_vol_label->setObjectName("popup_muted_lbl");

    QSlider *mic_vol_slider = new QSlider(Qt::Horizontal);
    mic_vol_slider->setRange(0, 200);
    mic_vol_slider->setValue(voice_ui_state.input_volume);

    QObject::connect(mic_vol_slider, &QSlider::valueChanged, [mic_vol_label](int val) {
        mic_vol_label->setText(QString("Microphone Volume: %1%").arg(val));
        voice_set_input_volume(val, false);
    });
    QObject::connect(mic_vol_slider, &QSlider::sliderReleased, [mic_vol_slider]() {
        voice_set_input_volume(mic_vol_slider->value(), true);
    });

    layout->addWidget(mic_vol_label);
    layout->addWidget(mic_vol_slider);

    QLabel *bitrate_label = new QLabel();
    bitrate_label->setObjectName("popup_muted_lbl");
    auto update_bitrate_label = [bitrate_label](int bitrate_kbps) {
        bitrate_label->setText(QString("Voice bitrate: %1 kbps").arg(bitrate_kbps));
    };

    QSlider *bitrate_slider = new QSlider(Qt::Horizontal);
    bitrate_slider->setRange(16, 128);
    bitrate_slider->setSingleStep(1);
    bitrate_slider->setPageStep(4);
    bitrate_slider->setValue(voice_ui_state.bitrate_kbps);
    update_bitrate_label(bitrate_slider->value());
    QObject::connect(bitrate_slider, &QSlider::valueChanged, bitrate_label, update_bitrate_label);
    QObject::connect(bitrate_slider, &QSlider::valueChanged, [](int bitrate_kbps) {
        voice_set_bitrate(bitrate_kbps, false);
    });
    QObject::connect(bitrate_slider, &QSlider::sliderReleased, [bitrate_slider]() {
        voice_set_bitrate(bitrate_slider->value(), true);
    });

    layout->addSpacing(8);
    layout->addWidget(bitrate_label);
    layout->addWidget(bitrate_slider);

    layout->addSpacing(10);
    QCheckBox *vad_checkbox = new QCheckBox("Enable Voice Activity Detection (VAD)");
    vad_checkbox->setChecked(voice_ui_state.vad_enabled);

    QLabel *vad_label = new QLabel(QString("Voice threshold: %1 dB").arg(voice_ui_state.vad_threshold_db));
    vad_label->setObjectName("popup_muted_lbl");
    vad_label->setEnabled(voice_ui_state.vad_enabled);

    QSlider *vad_slider = new QSlider(Qt::Horizontal);
    vad_slider->setRange(-70, -10);
    vad_slider->setValue(voice_ui_state.vad_threshold_db);
    vad_slider->setEnabled(voice_ui_state.vad_enabled);

    QObject::connect(vad_checkbox, &QCheckBox::toggled, [vad_slider, vad_label](bool checked) {
        vad_slider->setEnabled(checked);
        vad_label->setEnabled(checked);
        voice_set_vad_enabled(checked);
    });

    QObject::connect(vad_slider, &QSlider::valueChanged, [vad_label](int val) {
        vad_label->setText(QString("Voice threshold: %1 dB").arg(val));
        voice_set_vad_threshold(val, false);
    });
    QObject::connect(vad_slider, &QSlider::sliderReleased, [vad_slider]() {
        voice_set_vad_threshold(vad_slider->value(), true);
    });

    layout->addWidget(vad_checkbox);
    layout->addWidget(vad_label);
    layout->addWidget(vad_slider);

    layout->addSpacing(10);
    QLabel *test_section = new QLabel("Microphone Test");
    test_section->setObjectName("popup_section_title");
    layout->addWidget(test_section);

    QPushButton *test_btn = new QPushButton("Test Microphone");
    test_btn->setObjectName("popup_secondary_btn");
    test_btn->setCursor(Qt::PointingHandCursor);
    test_btn->setFixedHeight(28);

    QProgressBar *test_bar = new QProgressBar();
    test_bar->setRange(0, 100);
    test_bar->setValue(0);
    test_bar->setTextVisible(false);
    test_bar->setFixedHeight(8);

    QLabel *test_level_lbl = new QLabel("Level: 0% (-100 dB)");
    test_level_lbl->setObjectName("popup_muted_lbl");

    QObject::connect(test_btn, &QPushButton::clicked, [test_btn, test_bar, test_level_lbl]() {
        if (!voice_mic_test_active) {
            test_btn->setText("Stop Testing");
            voice_start_mic_test();
            on_voice_mic_test_level = [test_bar, test_level_lbl](int lvl, double db) {
                if (test_bar) test_bar->setValue(lvl);
                if (test_level_lbl) test_level_lbl->setText(QString("Level: %1% (%2 dB)").arg(lvl).arg((int)db));
            };
        } else {
            test_btn->setText("Test Microphone");
            voice_stop_mic_test();
            on_voice_mic_test_level = nullptr;
            if (test_bar) test_bar->setValue(0);
            if (test_level_lbl) test_level_lbl->setText("Level: 0% (-100 dB)");
        }
    });

    layout->addWidget(test_btn);
    layout->addWidget(test_bar);
    layout->addWidget(test_level_lbl);
    layout->addStretch(1);
    return page;
}

inline QWidget* create_audio_settings_page() {
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    QLabel *title = new QLabel("Audio Settings");
    title->setObjectName("popup_section_title");
    layout->addWidget(title);

    QLabel *label = new QLabel("Output Device (Playback)");
    label->setObjectName("popup_muted_lbl");
    layout->addWidget(label);

    QComboBox *device_combo = new QComboBox();
    device_combo->setFixedHeight(32);
    style_combobox(device_combo);
    device_combo->addItem("System default device", QByteArray());

    int selected_index = 0;
    for (const QAudioDevice &device : QMediaDevices::audioOutputs()) {
        device_combo->addItem(device.description(), device.id());
        if (!voice_ui_state.output_device_id.isEmpty() && device.id() == voice_ui_state.output_device_id) {
            selected_index = device_combo->count() - 1;
        }
    }

    device_combo->setCurrentIndex(selected_index);
    QObject::connect(device_combo, qOverload<int>(&QComboBox::currentIndexChanged),
                     [device_combo](int index) {
        voice_set_output_device(device_combo->itemData(index).toByteArray());
    });

    layout->addWidget(device_combo);

    layout->addSpacing(10);
    QLabel *vol_label = new QLabel(QString("Playback Volume: %1%").arg(voice_ui_state.output_volume));
    vol_label->setObjectName("popup_muted_lbl");

    QSlider *vol_slider = new QSlider(Qt::Horizontal);
    vol_slider->setRange(0, 150);
    vol_slider->setValue(voice_ui_state.output_volume);

    QObject::connect(vol_slider, &QSlider::valueChanged, [vol_label](int val) {
        vol_label->setText(QString("Playback Volume: %1%").arg(val));
        voice_set_output_volume(val, false);
    });
    QObject::connect(vol_slider, &QSlider::sliderReleased, [vol_slider]() {
        voice_set_output_volume(vol_slider->value(), true);
    });

    layout->addWidget(vol_label);
    layout->addWidget(vol_slider);

    layout->addSpacing(12);
    QLabel *sfx_title = new QLabel("Sound Effects");
    sfx_title->setObjectName("popup_section_title");
    layout->addWidget(sfx_title);

    QCheckBox *sfx_cb = new QCheckBox("Enable Sound Effects");
    sfx_cb->setChecked(config.sfx_enabled);
    layout->addWidget(sfx_cb);

    QLabel *sfx_vol_label = new QLabel(QString("Sound Effects Volume: %1%").arg(config.sfx_volume));
    sfx_vol_label->setObjectName("popup_muted_lbl");
    sfx_vol_label->setEnabled(config.sfx_enabled);

    QSlider *sfx_vol_slider = new QSlider(Qt::Horizontal);
    sfx_vol_slider->setRange(0, 100);
    sfx_vol_slider->setValue(config.sfx_volume);
    sfx_vol_slider->setEnabled(config.sfx_enabled);

    QObject::connect(sfx_cb, &QCheckBox::toggled, [sfx_vol_slider, sfx_vol_label](bool checked) {
        sfx_vol_slider->setEnabled(checked);
        sfx_vol_label->setEnabled(checked);
        {
            QWriteLocker lock(&config.lock);
            config.sfx_enabled = checked;
        }
        write_config_async();
    });

    QObject::connect(sfx_vol_slider, &QSlider::valueChanged, [sfx_vol_label](int val) {
        sfx_vol_label->setText(QString("Sound Effects Volume: %1%").arg(val));
        sound_update_volume(val);
        {
            QWriteLocker lock(&config.lock);
            config.sfx_volume = val;
        }
    });

    QObject::connect(sfx_vol_slider, &QSlider::sliderReleased, []() {
        write_config_async();
    });

    layout->addWidget(sfx_vol_label);
    layout->addWidget(sfx_vol_slider);

    layout->addStretch(1);
    return page;
}

inline QWidget* create_about_settings_page() {
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    QLabel *title = new QLabel("About Echo");
    title->setObjectName("popup_section_title");
    layout->addWidget(title);

    QLabel *version_lbl = new QLabel("Version: 1.0.0");
    version_lbl->setObjectName("popup_muted_lbl");
    layout->addWidget(version_lbl);

    QLabel *desc_lbl = new QLabel("Private, server-authoritative voice call & chat application.");
    desc_lbl->setObjectName("popup_muted_lbl");
    desc_lbl->setWordWrap(true);
    layout->addWidget(desc_lbl);

    layout->addSpacing(10);
    QLabel *repo_title = new QLabel("Repository");
    repo_title->setObjectName("popup_section_title");
    layout->addWidget(repo_title);

    QLabel *repo_lbl = new QLabel("<a href=\"https://github.com/yurixahri/echo\" style=\"color: " + global_theme.primary + "; text-decoration: underline;\">https://github.com/yurixahri/echo</a>");
    repo_lbl->setOpenExternalLinks(true);
    layout->addWidget(repo_lbl);

    layout->addSpacing(10);
    QLabel *update_title = new QLabel("Updates");
    update_title->setObjectName("popup_section_title");
    layout->addWidget(update_title);

    QPushButton *check_update_btn = new QPushButton("Check for Updates");
    check_update_btn->setObjectName("popup_secondary_btn");
    check_update_btn->setCursor(Qt::PointingHandCursor);
    check_update_btn->setFixedHeight(30);

    QLabel *update_status = new QLabel("");
    update_status->setObjectName("popup_muted_lbl");
    update_status->setWordWrap(true);

    QPushButton *download_btn = new QPushButton("Download Latest Release");
    download_btn->setObjectName("popup_primary_btn");
    download_btn->setCursor(Qt::PointingHandCursor);
    download_btn->setFixedHeight(30);
    download_btn->hide();

    QObject::connect(check_update_btn, &QPushButton::clicked, [page, check_update_btn, update_status, download_btn]() {
        check_update_btn->setEnabled(false);
        check_update_btn->setText("Checking...");
        update_status->setText("Connecting to GitHub...");
        download_btn->hide();

        QNetworkAccessManager *nam = new QNetworkAccessManager(page);
        QNetworkRequest req(QUrl("https://api.github.com/repos/yurixahri/echo/releases/latest"));
        req.setHeader(QNetworkRequest::UserAgentHeader, "Echo-Client");
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

        QNetworkReply *reply = nam->get(req);
        QObject::connect(reply, &QNetworkReply::finished, [check_update_btn, update_status, download_btn, reply, nam]() {
            check_update_btn->setEnabled(true);
            check_update_btn->setText("Check for Updates");

            if (reply->error() != QNetworkReply::NoError) {
                update_status->setText("No releases found or network error. You can visit the releases page:");
                download_btn->setText("Open GitHub Releases Page");
                download_btn->show();
                download_btn->disconnect();
                QObject::connect(download_btn, &QPushButton::clicked, []() {
                    QDesktopServices::openUrl(QUrl("https://github.com/yurixahri/echo/releases"));
                });
            } else {
                QByteArray data = reply->readAll();
                QJsonDocument doc = QJsonDocument::fromJson(data);
                QJsonObject obj = doc.object();
                QString tag = obj.value("tag_name").toString().trimmed();
                QString html_url = obj.value("html_url").toString();
                if (html_url.isEmpty()) html_url = "https://github.com/yurixahri/echo/releases";

                QString current_version = "v1.0.0";
                QString clean_tag = tag.startsWith("v") ? tag : ("v" + tag);

                if (!tag.isEmpty() && clean_tag != current_version && clean_tag > current_version) {
                    update_status->setText("New version available: " + tag);
                    update_status->setStyleSheet("color: " + global_theme.primary + "; font-size: 11px;");
                    download_btn->setText("Download " + tag);
                    download_btn->show();
                    download_btn->disconnect();
                    QObject::connect(download_btn, &QPushButton::clicked, [html_url]() {
                        QDesktopServices::openUrl(QUrl(html_url));
                    });
                } else {
                    update_status->setText("You are using the latest version (v1.0.0).");
                    update_status->setStyleSheet("color: " + global_theme.muted_foreground + "; font-size: 11px;");
                    download_btn->hide();
                }
            }
            reply->deleteLater();
            nam->deleteLater();
        });
    });

    layout->addWidget(check_update_btn);
    layout->addWidget(update_status);
    layout->addWidget(download_btn);

    layout->addStretch(1);
    return page;
}

inline QPopup* open_settings_popup(QWidget *parent, int gap_x = 20, int gap_y = 25) {
    if (!parent) return nullptr;

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(12);
    layout->setContentsMargins(15, 15, 15, 15);

    QLabel *title_label = new QLabel("Settings");
    title_label->setObjectName("popup_title");
    title_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(title_label);

    QWidget *content = new QWidget();
    QHBoxLayout *content_layout = new QHBoxLayout(content);
    content_layout->setContentsMargins(0, 0, 0, 0);
    content_layout->setSpacing(10);

    QStackedWidget *pages = new QStackedWidget();
    pages->setMinimumHeight(240);
    pages->addWidget(make_settings_scroll_page(create_general_settings_page()));
    pages->addWidget(make_settings_scroll_page(create_voice_settings_page()));
    pages->addWidget(make_settings_scroll_page(create_audio_settings_page()));
    pages->addWidget(make_settings_scroll_page(create_about_settings_page()));

    QVBoxLayout *navigation_layout = new QVBoxLayout();
    navigation_layout->setSpacing(6);
    navigation_layout->setContentsMargins(0, 0, 0, 0);
    QButtonGroup *section_group = new QButtonGroup(content);
    section_group->setExclusive(true);

    const QStringList section_names = {"General", "Voice", "Audio", "About"};
    for (int i = 0; i < section_names.size(); ++i) {
        QPushButton *section_button = new QPushButton(section_names[i]);
        section_button->setObjectName("settings_nav_btn");
        section_button->setCheckable(true);
        section_button->setFixedWidth(90);
        section_button->setFixedHeight(32);
        section_button->setCursor(Qt::PointingHandCursor);
        section_group->addButton(section_button, i);
        navigation_layout->addWidget(section_button);
        QObject::connect(section_button, &QPushButton::clicked, pages, [pages, i]() {
            pages->setCurrentIndex(i);
        });
        if (i == 0) section_button->setChecked(true);
    }
    navigation_layout->addStretch(1);

    QFrame *v_sep = new QFrame();
    v_sep->setObjectName("settings_vsep");
    v_sep->setFrameShape(QFrame::VLine);

    // Navigation on left, content pages on right
    content_layout->addLayout(navigation_layout);
    content_layout->addWidget(v_sep);
    content_layout->addWidget(pages, 1);
    layout->addWidget(content);

    QPushButton *close_button = new QPushButton("Close");
    close_button->setObjectName("popup_close_btn");
    close_button->setCursor(Qt::PointingHandCursor);
    close_button->setFixedHeight(30);
    layout->addWidget(close_button);

    QPopup *popup = new QPopup(parent, gap_x, gap_y);
    popup->set_panel_width(460, 600);
    popup->panel->setLayout(layout);
    popup->setStyleSheet(get_roles_popup_qss(theme_path));
    QObject::connect(close_button, &QPushButton::clicked, popup, &QObject::deleteLater);
    QObject::connect(popup, &QObject::destroyed, []() {
        voice_stop_mic_test();
        on_voice_mic_test_level = nullptr;
    });

    popup->raise();
    popup->show();
    return popup;
}

#endif // SETTINGS_POPUP_H
