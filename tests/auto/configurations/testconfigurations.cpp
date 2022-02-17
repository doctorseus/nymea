/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2020, nymea GmbH
* Contact: contact@nymea.io
*
* This file is part of nymea.
* This project including source code and documentation is protected by
* copyright law, and remains the property of nymea GmbH. All rights, including
* reproduction, publication, editing and translation, are reserved. The use of
* this project is subject to the terms of a license agreement to be concluded
* with nymea GmbH in accordance with the terms of use of nymea GmbH, available
* under https://nymea.io/license
*
* GNU General Public License Usage
* Alternatively, this project may be redistributed and/or modified under the
* terms of the GNU General Public License as published by the Free Software
* Foundation, GNU version 3. This project is distributed in the hope that it
* will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
* Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* this project. If not, see <https://www.gnu.org/licenses/>.
*
* For any further details and any questions please contact us under
* contact@nymea.io or see our FAQ/Licensing Information on
* https://nymea.io/license/faq
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "nymeatestbase.h"
#include "nymeacore.h"
#include "nymeasettings.h"
#include "servers/mocktcpserver.h"

using namespace nymeaserver;

class TestConfigurations: public NymeaTestBase
{
    Q_OBJECT

private:
    inline void verifyConfigurationError(const QVariant &response, NymeaConfiguration::ConfigurationError error = NymeaConfiguration::ConfigurationErrorNoError) {
        verifyError(response, "configurationError", enumValueName(error));
    }

protected slots:
    void initTestCase();

private slots:
    void getConfigurations();

    void testServerName();
    void testLanguages();

    void testDebugServerConfiguration();

private:
    QVariantMap loadBasicConfiguration();

};

void TestConfigurations::initTestCase()
{
    QDir dir(NymeaSettings::translationsPath());
    dir.mkpath(NymeaSettings::translationsPath());
    QStringList languages = {"de", "en_US"};
    foreach (const QString &language, languages) {
        QFile f(NymeaSettings::translationsPath().append("/nymead-" + language + ".qm"));
        QVERIFY2(f.open(QFile::WriteOnly), "Could not create translation file.");
        f.write(" ");
        f.close();
    }

    NymeaTestBase::initTestCase();
}

void TestConfigurations::getConfigurations()
{
    QVariant response = injectAndWait("Configuration.GetConfigurations");
    QVariantMap configurations = response.toMap().value("params").toMap();
    qDebug() << QJsonDocument::fromVariant(configurations).toJson();

    QVERIFY(configurations.contains("basicConfiguration"));
    QVERIFY(!configurations.value("basicConfiguration").toMap().value("serverUuid").toUuid().isNull());

    QVERIFY(configurations.contains("tcpServerConfigurations"));
    QVERIFY(configurations.contains("webServerConfigurations"));
    QVERIFY(configurations.contains("webSocketServerConfigurations"));
}

void TestConfigurations::testServerName()
{
    enableNotifications({"Configuration"});

    // Get current configurations
    QVariantMap basicConfigurationMap = loadBasicConfiguration();

    QString serverName = basicConfigurationMap.value("serverName").toString();
    QString serverUuid = basicConfigurationMap.value("serverUuid").toString();
    qDebug() << "Server name" << serverName << "(" << serverUuid << ")";

    QSignalSpy notificationSpy(m_mockTcpServer, SIGNAL(outgoingData(QUuid,QByteArray)));

    // Set name unchanged
    QVariantMap params; QVariant response; QVariantList configurationChangedNotifications;
    params.insert("serverName", serverName);
    response = injectAndWait("Configuration.SetServerName", params);
    verifyConfigurationError(response);

    // Check notification not emitted
    notificationSpy.wait(500);
    configurationChangedNotifications = checkNotifications(notificationSpy, "Configuration.BasicConfigurationChanged");
    QVERIFY2(configurationChangedNotifications.count() == 0, "Got Configuration.BasicConfigurationChanged notification but should have not.");

    // Set new server name
    QString newServerName = QString("Test server %1").arg(QUuid::createUuid().toString());
    params.clear(); response.clear(); configurationChangedNotifications.clear();
    params.insert("serverName", newServerName);

    notificationSpy.clear();
    response = injectAndWait("Configuration.SetServerName", params);
    verifyConfigurationError(response);

    // Check notification not emitted
    notificationSpy.wait(500);
    configurationChangedNotifications = checkNotifications(notificationSpy, "Configuration.BasicConfigurationChanged");
    QVariantMap notificationContent = configurationChangedNotifications.first().toMap().value("params").toMap();
    QVERIFY2(notificationContent.contains("basicConfiguration"), "Notification does not contain basicConfiguration");
    QVERIFY2(configurationChangedNotifications.count() == 1, "Should get only one Configuration.BasicConfigurationChanged notification");
    QVariantMap basicConfigurationNotificationMap = notificationContent.value("basicConfiguration").toMap();
    QVERIFY2(basicConfigurationNotificationMap.contains("language"), "Notification does not contain key language");
    QVERIFY2(basicConfigurationNotificationMap.contains("serverTime"), "Notification does not contain key serverTime");
    QVERIFY2(basicConfigurationNotificationMap.contains("serverUuid"), "Notification does not contain key serverUuid");
    QVERIFY2(basicConfigurationNotificationMap.contains("timeZone"), "Notification does not contain key timeZone");
    QVERIFY2(basicConfigurationNotificationMap.contains("debugServerEnabled"), "Notification does not contain key debugServerEnabled");
    QVERIFY2(basicConfigurationNotificationMap.contains("serverName"), "Notification does not contain key serverName");
    QVERIFY2(basicConfigurationNotificationMap.value("serverName").toString() == newServerName, "Notification does not contain the new serverName");

    basicConfigurationMap = loadBasicConfiguration();
    QString loadedServerName = basicConfigurationMap.value("serverName").toString();
    QVERIFY2(loadedServerName == newServerName, "Server name not set correctly");

    restartServer();

    basicConfigurationMap = loadBasicConfiguration();
    loadedServerName = basicConfigurationMap.value("serverName").toString();
    QVERIFY2(newServerName == loadedServerName, "Server name not loaded correctly after restart");

    disableNotifications();
}

void TestConfigurations::testLanguages()
{
    enableNotifications({"Configuration"});

    // Get current configurations
    QVariantMap basicConfigurationMap = loadBasicConfiguration();

    QSignalSpy notificationSpy(m_mockTcpServer, SIGNAL(outgoingData(QUuid,QByteArray)));

    // Set language unchanged
    QVariant response; QVariantMap params;
    params.insert("language", basicConfigurationMap.value("language"));
    response = injectAndWait("Configuration.SetLanguage", params);
    verifyConfigurationError(response);

    // Check notification not emitted
    notificationSpy.wait(500);
    QVariantList languageChangedNotifications = checkNotifications(notificationSpy, "Configuration.LanguageChanged");
    QVERIFY2(languageChangedNotifications.count() == 0, "Got Configuration.LanguageChanged notification but should have not.");

    // Get available languages
    response = injectAndWait("Configuration.GetAvailableLanguages");
    QVERIFY2(response.toMap().value("params").toMap().contains("languages"), "Did not get list of languages");
    QVariantMap responseMap = response.toMap().value("params").toMap();
    QVERIFY2(responseMap.value("languages").toList().count() >= 2, "Available languages list to short: " +  responseMap.value("languages").toList().count());

    QVariantList languageVariantList = responseMap.value("languages").toList();
    foreach (const QVariant &languageVariant, languageVariantList) {
        // create a new spy for each run as we restart the server and kill the old one in this loop
         QSignalSpy notificationSpy2 (m_mockTcpServer, SIGNAL(outgoingData(QUuid,QByteArray)));

        // Get current configurations
        basicConfigurationMap = loadBasicConfiguration();

        // Set language
        params.clear(); response.clear();
        params.insert("language", languageVariant);
        QVariant response = injectAndWait("Configuration.SetLanguage", params);
        verifyConfigurationError(response);

        // Check notification
        notificationSpy2.wait(500);
        QVariantList languageChangedNotifications = checkNotifications(notificationSpy2, "Configuration.LanguageChanged");

        // If the language did not change no notification should be emitted
        if (basicConfigurationMap.value("language").toString() == languageVariant.toString()) {
            QVERIFY2(languageChangedNotifications.count() == 0, "Got Configuration.LanguageChanged notification but should have not.");
        } else {
            QVERIFY2(languageChangedNotifications.count() == 1, "Should get only one Configuration.LanguageChanged notification");
            QVariantMap notificationMap = languageChangedNotifications.first().toMap().value("params").toMap();
            QVERIFY2(notificationMap.contains("language"), "Notification does not contain language");
            QVERIFY2(notificationMap.value("language").toString() == languageVariant.toString(), "Notification does not contain the new language");

            // Restart the server and check if the language will be loaded correctly
            restartServer();
            enableNotifications({"Configuration"});


            // Get configuration
            basicConfigurationMap = loadBasicConfiguration();

            QCOMPARE(basicConfigurationMap.value("language").toString(), languageVariant.toString());
        }
    }

    // Reset the language to en_US
    params.clear(); response.clear();
    params.insert("language", "en_US");

    // Set language
    response = injectAndWait("Configuration.SetLanguage", params);
    verifyConfigurationError(response);

    disableNotifications();
}

void TestConfigurations::testDebugServerConfiguration()
{
    enableNotifications({"Configuration"});

    // Get current configurations
    QVariantMap basicConfigurationMap = loadBasicConfiguration();

    bool debugServerEnabled = basicConfigurationMap.value("debugServerEnabled").toBool();
    qCDebug(dcTests) << "Debug server enabled" << debugServerEnabled;

    QSignalSpy notificationSpy(m_mockTcpServer, SIGNAL(outgoingData(QUuid,QByteArray)));

    // Unchanged debug server
    QVariantMap params; QVariant response; QVariantList configurationChangedNotifications;
    params.insert("enabled", debugServerEnabled);
    response = injectAndWait("Configuration.SetDebugServerEnabled", params);
    verifyConfigurationError(response);

    // Check notification not emitted
    notificationSpy.wait(500);
    configurationChangedNotifications = checkNotifications(notificationSpy, "Configuration.BasicConfigurationChanged");
    QVERIFY2(configurationChangedNotifications.count() == 0, "Got Configuration.BasicConfigurationChanged notification but should have not.");

    // Enable debug server
    bool newValue = true;
    params.clear(); response.clear(); configurationChangedNotifications.clear();
    params.insert("enabled", newValue);

    qCDebug(dcTests()) << "Enabling debug server";

    notificationSpy.clear();
    response = injectAndWait("Configuration.SetDebugServerEnabled", params);
    verifyConfigurationError(response);

    // Check notification not emitted
    notificationSpy.wait();
    configurationChangedNotifications = checkNotifications(notificationSpy, "Configuration.BasicConfigurationChanged");
    QVariantMap notificationContent = configurationChangedNotifications.first().toMap().value("params").toMap();
    QVERIFY2(notificationContent.contains("basicConfiguration"), "Notification does not contain basicConfiguration");
    QVERIFY2(configurationChangedNotifications.count() == 1, "Should get only one Configuration.BasicConfigurationChanged notification");
    QVariantMap basicConfigurationNotificationMap = notificationContent.value("basicConfiguration").toMap();

    QVERIFY2(basicConfigurationNotificationMap.contains("language"), "Notification does not contain key language");
    QVERIFY2(basicConfigurationNotificationMap.contains("serverTime"), "Notification does not contain key serverTime");
    QVERIFY2(basicConfigurationNotificationMap.contains("serverUuid"), "Notification does not contain key serverUuid");
    QVERIFY2(basicConfigurationNotificationMap.contains("timeZone"), "Notification does not contain key timeZone");
    QVERIFY2(basicConfigurationNotificationMap.contains("serverName"), "Notification does not contain key serverName");
    QVERIFY2(basicConfigurationNotificationMap.contains("debugServerEnabled"), "Notification does not contain key debugServerEnabled");
    QVERIFY2(basicConfigurationNotificationMap.value("debugServerEnabled").toBool() == newValue, "Notification does not contain the new debugServerEnabled");

    qCDebug(dcTests()) << "TestWebserver starting";
    foreach (const WebServerConfiguration &config, NymeaCore::instance()->configuration()->webServerConfigurations()) {
        if (config.port == 3333 && (QHostAddress(config.address) == QHostAddress("127.0.0.1") || QHostAddress(config.address) == QHostAddress("0.0.0.0"))) {
            qDebug() << "Already have a webserver listening on 127.0.0.1:3333";
            return;
        }
    }

    qCDebug(dcTests) << "Creating new webserver instance on 127.0.0.1:3333";
    WebServerConfiguration config;
    config.id = "Testwebserver for debug server interface";
    config.address = "127.0.0.1";
    config.port = 3333;
    config.sslEnabled = true;
    NymeaCore::instance()->configuration()->setWebServerConfiguration(config);

    // Webserver request
    QNetworkAccessManager nam;
    connect(&nam, &QNetworkAccessManager::sslErrors, [this, &nam](QNetworkReply* reply, const QList<QSslError> &) {
        reply->ignoreSslErrors();
    });
    QSignalSpy namSpy(&nam, SIGNAL(finished(QNetworkReply*)));

    // Check if debug interface is reachable
    QNetworkRequest request;
    request.setUrl(QUrl("https://localhost:3333/debug/"));
    QNetworkReply *reply = nam.get(request);

    namSpy.wait();
    QVERIFY2(namSpy.count() > 0, "expected response from webserver");

    bool ok = false;
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(&ok);
    QVERIFY2(ok, "Could not convert statuscode from response to int");
    QCOMPARE(statusCode, 200);
    reply->deleteLater();

    // Disable debug server
    params.clear(); response.clear();
    params.insert("enabled", false);
    response = injectAndWait("Configuration.SetDebugServerEnabled", params);
    verifyConfigurationError(response);

    // Check if debug interface is not reachable any more
    namSpy.clear();
    reply = nam.get(request);

    namSpy.wait();
    QVERIFY2(namSpy.count() > 0, "expected response from webserver");

    ok = false;
    statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(&ok);
    QVERIFY2(ok, "Could not convert statuscode from response to int");
    QCOMPARE(statusCode, 404);
    reply->deleteLater();

    NymeaCore::instance()->configuration()->removeWebServerConfiguration(config.id);

    disableNotifications();
}

QVariantMap TestConfigurations::loadBasicConfiguration()
{
    QVariant response = injectAndWait("Configuration.GetConfigurations");
    QVariantMap configurationMap = response.toMap().value("params").toMap();
    return configurationMap.value("basicConfiguration").toMap();
}

#include "testconfigurations.moc"
QTEST_MAIN(TestConfigurations)
