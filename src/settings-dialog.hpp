#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QTreeWidget>
#include <QStackedWidget>

#include "config.hpp"
#include "update-checker.hpp"

namespace BitrateSwitch {

class Switcher;

struct ServerPage {
    QWidget *widget = nullptr;
    QCheckBox *enabledCheck = nullptr;
    QComboBox *typeCombo = nullptr;
    QLineEdit *nameEdit = nullptr;
    QLineEdit *urlEdit = nullptr;
    QLineEdit *keyEdit = nullptr;
    QSpinBox *prioritySpin = nullptr;
    QTreeWidgetItem *treeItem = nullptr;
    int stackIndex = -1;
};

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    SettingsDialog(Config *config, Switcher *switcher, QWidget *parent = nullptr);
    ~SettingsDialog();

private slots:
    void onSave();
    void onApply();
    void refreshStatus();

private:
    void setupUI();
    QWidget *createGeneralPage();
    QWidget *createTriggersPage();
    QWidget *createScenesPage();
    QWidget *createChatPage();
    QWidget *createCommandsPage();
    QWidget *createMessagesPage();
    QWidget *createAdvancedPage();
    QWidget *createServerPage(const StreamServerConfig *initial = nullptr);
    void loadSettings();
    void saveSettings();
    void populateSceneComboBox(QComboBox *combo, bool allowEmpty = false);
    void updateStreamingFieldStates();
    void updateChatPlatformUi();
    void addServerEntry(const StreamServerConfig *initial = nullptr);
    void removeSelectedServer();
    void onNavItemClicked(QTreeWidgetItem *item, int column);
    static QWidget *wrapInScrollArea(QWidget *content);
    void populateServerTypeCombo(QComboBox *combo);

    Config *config_;
    Switcher *switcher_;

    // Navigation
    QTreeWidget *navTree_;
    QStackedWidget *contentStack_;
    QTreeWidgetItem *serversParent_;

    // Server pages
    std::vector<ServerPage> serverPages_;

    // General
    QCheckBox *enabledCheckbox_;
    QCheckBox *onlyWhenStreamingCheckbox_;
    QCheckBox *instantRecoverCheckbox_;
    QCheckBox *autoNotifyCheckbox_;
    QSpinBox *retryAttemptsSpinBox_;

    // Triggers
    QSpinBox *lowBitrateSpinBox_;
    QSpinBox *rttThresholdSpinBox_;
    QSpinBox *offlineBitrateSpinBox_;
    QSpinBox *rttOfflineSpinBox_;

    // Scenes
    QComboBox *normalSceneCombo_;
    QComboBox *lowSceneCombo_;
    QComboBox *offlineSceneCombo_;
    QComboBox *startingSceneCombo_;
    QComboBox *endingSceneCombo_;
    QComboBox *privacySceneCombo_;
    QComboBox *refreshSceneCombo_;

    // Advanced
    QSpinBox *offlineTimeoutSpinBox_;
    QCheckBox *recordWhileStreamingCheckbox_;
    QCheckBox *switchToStartingCheckbox_;
    QCheckBox *switchFromStartingCheckbox_;
    QSpinBox *ristStaleFrameFixSpinBox_;

    // Status
    QLabel *statusLabel_;
    QLabel *bitrateLabel_;
    QTimer *statusTimer_;

    // Chat
    QCheckBox *chatEnabledCheckbox_;
    QComboBox *chatPlatformCombo_;
    QLineEdit *chatChannelEdit_;
    QLineEdit *chatBotUsernameEdit_;
    QLineEdit *chatOauthEdit_;
    QLineEdit *chatAdminsEdit_;
    QCheckBox *chatAnnounceCheckbox_;
    QLabel *twitchBotLabel_;
    QLabel *twitchOauthLabel_;
    QLabel *kickChannelLabel_;
    QLabel *kickChatroomLabel_;
    QLineEdit *kickChannelIdEdit_;
    QLineEdit *kickChatroomIdEdit_;
    QCheckBox *chatAutoStopRaidCheckbox_;
    QCheckBox *chatAnnounceRaidStopCheckbox_;

    // Messages
    QLineEdit *msgSwitchedLiveEdit_;
    QLineEdit *msgSwitchedLowEdit_;
    QLineEdit *msgSwitchedOfflineEdit_;
    QLineEdit *msgStatusEdit_;
    QLineEdit *msgStatusOfflineEdit_;
    QLineEdit *msgRefreshingEdit_;
    QLineEdit *msgFixEdit_;
    QLineEdit *msgStreamStartedEdit_;
    QLineEdit *msgStreamStoppedEdit_;
    QLineEdit *msgRaidStopEdit_;
    QLineEdit *msgSceneSwitchedEdit_;

    // Commands
    QLineEdit *cmdLiveEdit_;
    QLineEdit *cmdLowEdit_;
    QLineEdit *cmdBrbEdit_;
    QLineEdit *cmdPrivacyEdit_;
    QLineEdit *cmdRefreshEdit_;
    QLineEdit *cmdStatusEdit_;
    QLineEdit *cmdTriggerEdit_;
    QLineEdit *cmdFixEdit_;
    QLineEdit *cmdSwitchSceneEdit_;
    QLineEdit *cmdStartEdit_;
    QLineEdit *cmdStopEdit_;

    // Custom commands
    QTableWidget *customCmdTable_;
    QPushButton *addCustomCmdBtn_;
    QPushButton *removeCustomCmdBtn_;

    // Update notification
    QLabel *updateLabel_;
    UpdateChecker updateChecker_;
};

} // namespace BitrateSwitch
