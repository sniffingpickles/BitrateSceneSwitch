#include "settings-dialog.hpp"
#include "switcher.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QTimer>
#include <obs-frontend-api.h>

namespace BitrateSwitch {

SettingsDialog::SettingsDialog(Config *config, Switcher *switcher, QWidget *parent)
    : QDialog(parent)
    , config_(config)
    , switcher_(switcher)
{
    setWindowTitle("Bitrate Scene Switch Settings");
    setMinimumSize(600, 500);
    
    setupUI();
    loadSettings();

    // Status refresh timer
    statusTimer_ = new QTimer(this);
    connect(statusTimer_, &QTimer::timeout, this, &SettingsDialog::refreshStatus);
    statusTimer_->start(1000);
    refreshStatus();
}

SettingsDialog::~SettingsDialog()
{
    statusTimer_->stop();
}

void SettingsDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Status group
    QGroupBox *statusGroup = new QGroupBox("Status", this);
    QHBoxLayout *statusLayout = new QHBoxLayout(statusGroup);
    statusLabel_ = new QLabel("Status: Unknown", this);
    bitrateLabel_ = new QLabel("Bitrate: --", this);
    statusLayout->addWidget(statusLabel_);
    statusLayout->addWidget(bitrateLabel_);
    statusLayout->addStretch();
    mainLayout->addWidget(statusGroup);

    // General settings group
    QGroupBox *generalGroup = new QGroupBox("General Settings", this);
    QFormLayout *generalLayout = new QFormLayout(generalGroup);
    
    enabledCheckbox_ = new QCheckBox("Enable automatic scene switching", this);
    onlyWhenStreamingCheckbox_ = new QCheckBox("Only switch when streaming", this);
    instantRecoverCheckbox_ = new QCheckBox("Instantly switch on bitrate recovery", this);
    
    retryAttemptsSpinBox_ = new QSpinBox(this);
    retryAttemptsSpinBox_->setRange(1, 30);
    retryAttemptsSpinBox_->setValue(5);
    retryAttemptsSpinBox_->setToolTip("Number of checks before switching scenes");

    generalLayout->addRow(enabledCheckbox_);
    generalLayout->addRow(onlyWhenStreamingCheckbox_);
    generalLayout->addRow(instantRecoverCheckbox_);
    generalLayout->addRow("Retry Attempts:", retryAttemptsSpinBox_);
    mainLayout->addWidget(generalGroup);

    // Trigger settings group
    QGroupBox *triggerGroup = new QGroupBox("Bitrate Triggers", this);
    QFormLayout *triggerLayout = new QFormLayout(triggerGroup);

    lowBitrateSpinBox_ = new QSpinBox(this);
    lowBitrateSpinBox_->setRange(0, 50000);
    lowBitrateSpinBox_->setValue(800);
    lowBitrateSpinBox_->setSuffix(" kbps");
    lowBitrateSpinBox_->setToolTip("Switch to Low scene when bitrate drops below this");

    rttThresholdSpinBox_ = new QSpinBox(this);
    rttThresholdSpinBox_->setRange(0, 10000);
    rttThresholdSpinBox_->setValue(2500);
    rttThresholdSpinBox_->setSuffix(" ms");
    rttThresholdSpinBox_->setToolTip("Switch to Low scene when RTT exceeds this (SRT only)");

    offlineBitrateSpinBox_ = new QSpinBox(this);
    offlineBitrateSpinBox_->setRange(0, 10000);
    offlineBitrateSpinBox_->setValue(0);
    offlineBitrateSpinBox_->setSuffix(" kbps");
    offlineBitrateSpinBox_->setToolTip("Switch to Offline scene when bitrate drops below this (0 = use server offline)");

    triggerLayout->addRow("Low Bitrate Threshold:", lowBitrateSpinBox_);
    triggerLayout->addRow("RTT Threshold:", rttThresholdSpinBox_);
    triggerLayout->addRow("Offline Threshold:", offlineBitrateSpinBox_);
    mainLayout->addWidget(triggerGroup);

    // Scene settings group
    QGroupBox *sceneGroup = new QGroupBox("Scene Assignment", this);
    QFormLayout *sceneLayout = new QFormLayout(sceneGroup);

    normalSceneCombo_ = new QComboBox(this);
    lowSceneCombo_ = new QComboBox(this);
    offlineSceneCombo_ = new QComboBox(this);

    populateSceneComboBox(normalSceneCombo_);
    populateSceneComboBox(lowSceneCombo_);
    populateSceneComboBox(offlineSceneCombo_);

    sceneLayout->addRow("Normal Scene:", normalSceneCombo_);
    sceneLayout->addRow("Low Bitrate Scene:", lowSceneCombo_);
    sceneLayout->addRow("Offline Scene:", offlineSceneCombo_);
    mainLayout->addWidget(sceneGroup);

    // Stream servers group
    QGroupBox *serverGroup = new QGroupBox("Stream Servers", this);
    QVBoxLayout *serverLayout = new QVBoxLayout(serverGroup);

    serverTable_ = new QTableWidget(0, 4, this);
    serverTable_->setHorizontalHeaderLabels({"Enabled", "Type", "Name", "Stats URL"});
    serverTable_->horizontalHeader()->setStretchLastSection(true);
    serverTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    serverTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    serverLayout->addWidget(serverTable_);

    QHBoxLayout *serverBtnLayout = new QHBoxLayout();
    addServerBtn_ = new QPushButton("Add Server", this);
    removeServerBtn_ = new QPushButton("Remove Server", this);
    testBtn_ = new QPushButton("Test Connection", this);
    serverBtnLayout->addWidget(addServerBtn_);
    serverBtnLayout->addWidget(removeServerBtn_);
    serverBtnLayout->addWidget(testBtn_);
    serverBtnLayout->addStretch();
    serverLayout->addLayout(serverBtnLayout);
    mainLayout->addWidget(serverGroup);

    connect(addServerBtn_, &QPushButton::clicked, this, &SettingsDialog::onAddServer);
    connect(removeServerBtn_, &QPushButton::clicked, this, &SettingsDialog::onRemoveServer);
    connect(testBtn_, &QPushButton::clicked, this, &SettingsDialog::onTestConnection);

    // Dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::onSave);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void SettingsDialog::populateSceneComboBox(QComboBox *combo)
{
    combo->clear();
    
    obs_frontend_source_list scenes = {};
    obs_frontend_get_scenes(&scenes);
    
    for (size_t i = 0; i < scenes.sources.num; i++) {
        obs_source_t *source = scenes.sources.array[i];
        const char *name = obs_source_get_name(source);
        if (name) {
            combo->addItem(QString::fromUtf8(name));
        }
    }
    
    obs_frontend_source_list_free(&scenes);
}

void SettingsDialog::loadSettings()
{
    enabledCheckbox_->setChecked(config_->enabled);
    onlyWhenStreamingCheckbox_->setChecked(config_->onlyWhenStreaming);
    instantRecoverCheckbox_->setChecked(config_->instantRecover);
    retryAttemptsSpinBox_->setValue(config_->retryAttempts);

    lowBitrateSpinBox_->setValue(config_->triggers.low);
    rttThresholdSpinBox_->setValue(config_->triggers.rtt);
    offlineBitrateSpinBox_->setValue(config_->triggers.offline);

    // Set scene combos
    int idx = normalSceneCombo_->findText(QString::fromStdString(config_->scenes.normal));
    if (idx >= 0) normalSceneCombo_->setCurrentIndex(idx);
    
    idx = lowSceneCombo_->findText(QString::fromStdString(config_->scenes.low));
    if (idx >= 0) lowSceneCombo_->setCurrentIndex(idx);
    
    idx = offlineSceneCombo_->findText(QString::fromStdString(config_->scenes.offline));
    if (idx >= 0) offlineSceneCombo_->setCurrentIndex(idx);

    // Load servers
    serverTable_->setRowCount(0);
    for (const auto &server : config_->servers) {
        int row = serverTable_->rowCount();
        serverTable_->insertRow(row);

        QCheckBox *enabledCheck = new QCheckBox();
        enabledCheck->setChecked(server.enabled);
        serverTable_->setCellWidget(row, 0, enabledCheck);

        QComboBox *typeCombo = new QComboBox();
        typeCombo->addItems({"Belabox", "NGINX", "SRT Live Server", "MediaMTX"});
        typeCombo->setCurrentIndex(static_cast<int>(server.type));
        serverTable_->setCellWidget(row, 1, typeCombo);

        serverTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(server.name)));
        serverTable_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(server.statsUrl)));
    }
}

void SettingsDialog::saveSettings()
{
    config_->enabled = enabledCheckbox_->isChecked();
    config_->onlyWhenStreaming = onlyWhenStreamingCheckbox_->isChecked();
    config_->instantRecover = instantRecoverCheckbox_->isChecked();
    config_->retryAttempts = retryAttemptsSpinBox_->value();

    config_->triggers.low = lowBitrateSpinBox_->value();
    config_->triggers.rtt = rttThresholdSpinBox_->value();
    config_->triggers.offline = offlineBitrateSpinBox_->value();

    config_->scenes.normal = normalSceneCombo_->currentText().toStdString();
    config_->scenes.low = lowSceneCombo_->currentText().toStdString();
    config_->scenes.offline = offlineSceneCombo_->currentText().toStdString();

    // Save servers
    config_->servers.clear();
    for (int row = 0; row < serverTable_->rowCount(); row++) {
        StreamServerConfig server;
        
        QCheckBox *enabledCheck = qobject_cast<QCheckBox*>(serverTable_->cellWidget(row, 0));
        server.enabled = enabledCheck ? enabledCheck->isChecked() : true;

        QComboBox *typeCombo = qobject_cast<QComboBox*>(serverTable_->cellWidget(row, 1));
        server.type = typeCombo ? static_cast<ServerType>(typeCombo->currentIndex()) : ServerType::Belabox;

        QTableWidgetItem *nameItem = serverTable_->item(row, 2);
        server.name = nameItem ? nameItem->text().toStdString() : "";

        QTableWidgetItem *urlItem = serverTable_->item(row, 3);
        server.statsUrl = urlItem ? urlItem->text().toStdString() : "";

        config_->servers.push_back(server);
    }
}

void SettingsDialog::onAddServer()
{
    int row = serverTable_->rowCount();
    serverTable_->insertRow(row);

    QCheckBox *enabledCheck = new QCheckBox();
    enabledCheck->setChecked(true);
    serverTable_->setCellWidget(row, 0, enabledCheck);

    QComboBox *typeCombo = new QComboBox();
    typeCombo->addItems({"Belabox", "NGINX", "SRT Live Server", "MediaMTX"});
    serverTable_->setCellWidget(row, 1, typeCombo);

    serverTable_->setItem(row, 2, new QTableWidgetItem("New Server"));
    serverTable_->setItem(row, 3, new QTableWidgetItem("http://"));
}

void SettingsDialog::onRemoveServer()
{
    int row = serverTable_->currentRow();
    if (row >= 0) {
        serverTable_->removeRow(row);
    }
}

void SettingsDialog::onTestConnection()
{
    int row = serverTable_->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "Test Connection", "Please select a server to test.");
        return;
    }

    QTableWidgetItem *urlItem = serverTable_->item(row, 3);
    if (!urlItem) return;

    QString url = urlItem->text();
    QMessageBox::information(this, "Test Connection", 
        QString("Testing connection to:\n%1\n\n(Full test requires plugin restart)").arg(url));
}

void SettingsDialog::onSave()
{
    saveSettings();
    
    if (switcher_) {
        switcher_->reloadServers();
    }

    accept();
}

void SettingsDialog::refreshStatus()
{
    if (!switcher_) {
        statusLabel_->setText("Status: Not initialized");
        bitrateLabel_->setText("Bitrate: --");
        return;
    }

    QString status = QString::fromStdString(switcher_->getStatusString());
    statusLabel_->setText("Status: " + status);

    auto info = switcher_->getCurrentBitrate();
    if (info.isOnline) {
        bitrateLabel_->setText(QString("Bitrate: %1 kbps").arg(info.bitrateKbps));
    } else {
        bitrateLabel_->setText("Bitrate: Offline");
    }
}

} // namespace BitrateSwitch
