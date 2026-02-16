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
#include "update-checker.hpp"

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
    void setupChatTab(QWidget *tab);
    void setupMessagesTab(QWidget *tab);
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

    // Chat settings
    QCheckBox *chatEnabledCheckbox_;
    QComboBox *chatPlatformCombo_;
    QLineEdit *chatChannelEdit_;
    QLineEdit *chatBotUsernameEdit_;
    QLineEdit *chatOauthEdit_;
    QLineEdit *chatAdminsEdit_;
    QCheckBox *chatAnnounceCheckbox_;
    QPushButton *chatConnectBtn_;

    // Message templates
    QLineEdit *msgSwitchedLiveEdit_;
    QLineEdit *msgSwitchedLowEdit_;
    QLineEdit *msgSwitchedOfflineEdit_;
    QLineEdit *msgStatusEdit_;
    QLineEdit *msgStatusOfflineEdit_;
    QLineEdit *msgRefreshingEdit_;
    QLineEdit *msgFixEdit_;
    QLineEdit *msgStreamStartedEdit_;
    QLineEdit *msgStreamStoppedEdit_;
    QLineEdit *msgSceneSwitchedEdit_;

    // Custom commands table
    QTableWidget *customCmdTable_;
    QPushButton *addCustomCmdBtn_;
    QPushButton *removeCustomCmdBtn_;
    
    // Update notification
    QLabel *updateLabel_;
    UpdateChecker updateChecker_;
};

} // namespace BitrateSwitch
