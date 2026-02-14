#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>

#include "config.hpp"

namespace BitrateSwitch {

class Switcher;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    SettingsDialog(Config *config, Switcher *switcher, QWidget *parent = nullptr);
    ~SettingsDialog();

private slots:
    void onAddServer();
    void onRemoveServer();
    void onSave();
    void onTestConnection();
    void refreshStatus();

private:
    void setupUI();
    void loadSettings();
    void saveSettings();
    void populateSceneComboBox(QComboBox *combo);

    Config *config_;
    Switcher *switcher_;

    // General settings
    QCheckBox *enabledCheckbox_;
    QCheckBox *onlyWhenStreamingCheckbox_;
    QCheckBox *instantRecoverCheckbox_;
    QSpinBox *retryAttemptsSpinBox_;

    // Trigger settings
    QSpinBox *lowBitrateSpinBox_;
    QSpinBox *rttThresholdSpinBox_;
    QSpinBox *offlineBitrateSpinBox_;

    // Scene settings
    QComboBox *normalSceneCombo_;
    QComboBox *lowSceneCombo_;
    QComboBox *offlineSceneCombo_;

    // Server list
    QTableWidget *serverTable_;
    QPushButton *addServerBtn_;
    QPushButton *removeServerBtn_;
    QPushButton *testBtn_;

    // Status
    QLabel *statusLabel_;
    QLabel *bitrateLabel_;

    // Timer for status updates
    QTimer *statusTimer_;
};

} // namespace BitrateSwitch
