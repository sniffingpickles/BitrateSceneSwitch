#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>

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
    void setupGeneralTab(QWidget *tab);
    void setupTriggersTab(QWidget *tab);
    void setupScenesTab(QWidget *tab);
    void setupServersTab(QWidget *tab);
    void setupAdvancedTab(QWidget *tab);
    void loadSettings();
    void saveSettings();
    void populateSceneComboBox(QComboBox *combo, bool allowEmpty = false);

    Config *config_;
    Switcher *switcher_;

    QTabWidget *tabWidget_;

    // General settings
    QCheckBox *enabledCheckbox_;
    QCheckBox *onlyWhenStreamingCheckbox_;
    QCheckBox *instantRecoverCheckbox_;
    QCheckBox *autoNotifyCheckbox_;
    QSpinBox *retryAttemptsSpinBox_;

    // Trigger settings
    QSpinBox *lowBitrateSpinBox_;
    QSpinBox *rttThresholdSpinBox_;
    QSpinBox *offlineBitrateSpinBox_;
    QSpinBox *rttOfflineSpinBox_;

    // Main scene settings
    QComboBox *normalSceneCombo_;
    QComboBox *lowSceneCombo_;
    QComboBox *offlineSceneCombo_;

    // Optional scenes
    QComboBox *startingSceneCombo_;
    QComboBox *endingSceneCombo_;
    QComboBox *privacySceneCombo_;
    QComboBox *refreshSceneCombo_;

    // Optional options
    QSpinBox *offlineTimeoutSpinBox_;
    QCheckBox *recordWhileStreamingCheckbox_;
    QCheckBox *switchToStartingCheckbox_;
    QCheckBox *switchFromStartingCheckbox_;

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
