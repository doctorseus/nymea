/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  This file is part of guh.                                              *
 *                                                                         *
 *  Guh is free software: you can redistribute it and/or modify            *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, version 2 of the License.                *
 *                                                                         *
 *  Guh is distributed in the hope that it will be useful,                 *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with guh. If not, see <http://www.gnu.org/licenses/>.            *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*!
    \class DeviceManager
    \brief The main entry point when interacting with \l{Device}{Devices}

    \ingroup devices
    \inmodule libguh

    The DeviceManager holds all information about supported and configured Devices in the system.

    It is also responsible for loading Plugins and managing common hardware resources between
    \l{DevicePlugin}{device plugins}.

*/

/*! \enum DeviceManager::HardwareResource

    This enum type specifies hardware resources which can be requested by \l{DevicePlugin}{DevicePlugins}.

    \value HardwareResourceNone
        No Resource required.
    \value HardwareResourceRadio433
        Refers to the 433 MHz radio.
    \value HardwareResourceRadio868
        Refers to the 868 MHz radio.
    \value HardwareResourceTimer
        Refers to the global timer managed by the \l{DeviceManager}. Plugins should not create their own timers,
        but rather request the global timer using the hardware resources.
    \value HardwareResourceUpnpDisovery
        Allowes plugins to search a UPnP devices in the network.
*/

/*! \enum DeviceManager::DeviceError

    This enum type specifies the errors that can happen when working with \l{Device}{Devices}.

    \value DeviceErrorNoError
        No Error. Everything went fine.
    \value DeviceErrorPluginNotFound
        Couldn't find the Plugin for the given id.
    \value DeviceErrorDeviceNotFound
        Couldn't find a \l{Device} for the given id.
    \value DeviceErrorDeviceClassNotFound
        Couldn't find a \l{DeviceClass} for the given id.
    \value DeviceErrorActionTypeNotFound
        Couldn't find the \l{ActionType} for the given id.
    \value DeviceErrorStateTypeNotFound
        Couldn't find the \l{StateType} for the given id.
    \value DeviceErrorEventTypeNotFound
        Couldn't find the \l{EventType} for the given id.
    \value DeviceErrorDeviceDescriptorNotFound
        Couldn't find the \l{DeviceDescriptor} for the given id.
    \value DeviceErrorMissingParameter
        Parameters do not comply to the template.
    \value DeviceErrorInvalidParameter
        One of the given parameter is not valid.
    \value DeviceErrorSetupFailed
        Error setting up the \l{Device}. It will not be functional.
    \value DeviceErrorDuplicateUuid
        Error setting up the \l{Device}. The given DeviceId already exists.
    \value DeviceErrorCreationMethodNotSupported
        Error setting up the \l{Device}. This \l{DeviceClass}{CreateMethod} is not supported for this \l{Device}.
    \value DeviceErrorSetupMethodNotSupported
        Error setting up the \l{Device}. This \l{DeviceClass}{SetupMethod} is not supported for this \l{Device}.
    \value DeviceErrorHardwareNotAvailable
        The Hardware of the \l{Device} is not available.
    \value DeviceErrorHardwareFailure
        The Hardware of the \l{Device} has an error.
    \value DeviceErrorAsync
        The response of the \l{Device} will be asynchronously.
    \value DeviceErrorDeviceInUse
        The \l{Device} is currently bussy.
    \value DeviceErrorPairingTransactionIdNotFound
        Couldn't find the PairingTransactionId for the given id.
*/

/*! \enum DeviceManager::DeviceSetupStatus

    This enum type specifies the setup status of a \l{Device}.

    \value DeviceSetupStatusSuccess
        No Error. Everything went fine.
    \value DeviceSetupStatusFailure
        Something went wrong during the setup.
    \value DeviceSetupStatusAsync
        The status of the \l{Device} setup will be emitted asynchronous.
*/

/*! \fn void DeviceManager::loaded();
    The DeviceManager will emit this signal when all \l{Device}{Devices} are loaded.
*/

/*! \fn void DeviceManager::deviceSetupFinished(Device *device, DeviceError status);
    This signal is emitted when the setup of a \a device is finished. The \a status parameter describes the
    \l{DeviceManager::DeviceError}{DeviceError} that occurred.
*/

/*! \fn void DeviceManager::deviceStateChanged(Device *device, const QUuid &stateTypeId, const QVariant &value);
    This signal is emitted when the \l{State} of a \a device changed. The \a stateTypeId parameter describes the
    \l{StateType} and the \a value parameter holds the new value.
*/

/*! \fn void DeviceManager::devicesDiscovered(const DeviceClassId &deviceClassId, const QList<DeviceDescriptor> &devices);
    This signal is emitted when the discovery of a \a deviceClassId is finished. The \a devices parameter describes the
    list of \l{DeviceDescriptor}{DeviceDescriptors} of all discovered \l{Device}{Devices}.
    \sa discoverDevices()
*/

/*! \fn void DeviceManager::actionExecutionFinished(const ActionId &actionId, DeviceError status);
    The DeviceManager will emit a this signal when the \l{Action} with the given \a actionId is finished.
    The \a status of the \l{Action} execution will be described as \l{DeviceManager::DeviceError}{DeviceError}.
*/

/*! \fn void DeviceManager::pairingFinished(const PairingTransactionId &pairingTransactionId, DeviceError status, const DeviceId &deviceId = DeviceId());
    The DeviceManager will emit a this signal when the pairing of a \l{Device} with the \a deviceId and \a pairingTransactionId is finished.
    The \a status of the pairing will be described as \l{DeviceManager::DeviceError}{DeviceError}.
*/

/*! \fn void DeviceManager::eventTriggered(const Event &event)
    The DeviceManager will emit a \l{Event} described in \a event whenever a Device
    creates one. Normally only \l{GuhCore} should connect to this and execute actions
    after checking back with the \{RulesEngine}. Exceptions might be monitoring interfaces
    or similar, but you should never directly react to this in a \l{DevicePlugin}.
*/

#include "devicemanager.h"

#include "hardware/radio433/radio433.h"

#include "plugin/device.h"
#include "plugin/deviceclass.h"
#include "plugin/deviceplugin.h"

#include <QPluginLoader>
#include <QStaticPlugin>
#include <QtPlugin>
#include <QDebug>
#include <QSettings>
#include <QStringList>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>

/*! Constructs the DeviceManager with the given \a parent. There should only be one DeviceManager in the system created by \l{GuhCore}.
 *  Use \c GuhCore::instance()->deviceManager() instead to access the DeviceManager. */
DeviceManager::DeviceManager(QObject *parent) :
    QObject(parent),
    m_radio433(0)
{
    qRegisterMetaType<DeviceClassId>();
    qRegisterMetaType<DeviceDescriptor>();

    m_pluginTimer.setInterval(15000);
    connect(&m_pluginTimer, &QTimer::timeout, this, &DeviceManager::timerEvent);

    m_settingsFile = QCoreApplication::instance()->organizationName() + "/devices";

    // Give hardware a chance to start up before loading plugins etc.
    QMetaObject::invokeMethod(this, "loadPlugins", Qt::QueuedConnection);
    QMetaObject::invokeMethod(this, "loadConfiguredDevices", Qt::QueuedConnection);
    QMetaObject::invokeMethod(this, "startMonitoringAutoDevices", Qt::QueuedConnection);
    // Make sure this is always emitted after plugins and devices are loaded
    QMetaObject::invokeMethod(this, "loaded", Qt::QueuedConnection);

    m_radio433 = new Radio433(this);
    connect(m_radio433, &Radio433::dataReceived, this, &DeviceManager::radio433SignalReceived);
    m_radio433->enable();
    // TODO: error handling if no Radio433 detected (GPIO or network), disable radio433 plugins or something...

    m_upnpDiscovery = new UpnpDiscovery(this);
    connect(m_upnpDiscovery, &UpnpDiscovery::discoveryFinished, this, &DeviceManager::upnpDiscoveryFinished);
    connect(m_upnpDiscovery, &UpnpDiscovery::upnpNotify, this, &DeviceManager::upnpNotifyReceived);
}

/*! Destructor of the DeviceManager. Each loaded \l{DevicePlugin} will be deleted. */
DeviceManager::~DeviceManager()
{
    qDebug() << "Shutting down DeviceManager";
    foreach (DevicePlugin *plugin, m_devicePlugins) {
        delete plugin;
    }
}

/*! Returns all the \l{DevicePlugin}{DevicePlugins} loaded in the system. */
QList<DevicePlugin *> DeviceManager::plugins() const
{
    return m_devicePlugins.values();
}

/*! Returns the \l{DevicePlugin} with the given \a id. Null if the id couldn't be found. */
DevicePlugin *DeviceManager::plugin(const PluginId &id) const
{
    return m_devicePlugins.value(id);
}

/*! Returns a certain \l{DeviceError} and sets the configuration of the plugin with the given \a pluginId
 *  and the given \a pluginConfig. */
DeviceManager::DeviceError DeviceManager::setPluginConfig(const PluginId &pluginId, const ParamList &pluginConfig)
{
    DevicePlugin *plugin = m_devicePlugins.value(pluginId);
    if (!plugin) {
        return DeviceErrorPluginNotFound;
    }
    DeviceError result = plugin->setConfiguration(pluginConfig);
    if (result != DeviceErrorNoError) {
        return result;
    }
    QSettings settings;
    settings.beginGroup("PluginConfig");
    settings.beginGroup(plugin->pluginId().toString());
    foreach (const Param &param, pluginConfig) {
        settings.setValue(param.name(), param.value());
    }
    settings.endGroup();
    settings.endGroup();
    return result;
}

/*! Returns all the \l{Vendor}s loaded in the system. */
QList<Vendor> DeviceManager::supportedVendors() const
{
    return m_supportedVendors.values();
}

/*! Returns all the supported \l{DeviceClass}{DeviceClasses} by all \l{DevicePlugin}{DevicePlugins} loaded in the system.
 *  Optionally filtered by \a vendorId. */
QList<DeviceClass> DeviceManager::supportedDevices(const VendorId &vendorId) const
{
    QList<DeviceClass> ret;
    if (vendorId.isNull()) {
        ret = m_supportedDevices.values();
    } else {
        foreach (const DeviceClassId &deviceClassId, m_vendorDeviceMap.value(vendorId)) {
            ret.append(m_supportedDevices.value(deviceClassId));
        }
    }
    return ret;
}
/*! Returns a certain \l{DeviceError} and starts the discovering process of the \l{Device} with the given \a deviceClassId
 *  and the given \a params.*/
DeviceManager::DeviceError DeviceManager::discoverDevices(const DeviceClassId &deviceClassId, const ParamList &params)
{
    qDebug() << "DeviceManager discoverdevices" << params;
    // Create a copy of the parameter list because we might modify it (fillig in default values etc)
    ParamList effectiveParams = params;
    DeviceClass deviceClass = findDeviceClass(deviceClassId);
    if (!deviceClass.isValid()) {
        return DeviceErrorDeviceClassNotFound;
    }
    if (!deviceClass.createMethods().testFlag(DeviceClass::CreateMethodDiscovery)) {
        return  DeviceErrorCreationMethodNotSupported;
    }
    DeviceError result = verifyParams(deviceClass.discoveryParamTypes(), effectiveParams);
    if (result != DeviceErrorNoError) {
        return result;
    }
    DevicePlugin *plugin = m_devicePlugins.value(deviceClass.pluginId());
    if (!plugin) {
        return DeviceErrorPluginNotFound;
    }
    m_discoveringPlugins.append(plugin);
    DeviceError ret = plugin->discoverDevices(deviceClassId, effectiveParams);
    if (ret != DeviceErrorAsync) {
        m_discoveringPlugins.removeOne(plugin);
    }
    return ret;
}

/*! Add a new configured device for the given \l{DeviceClass}, the given parameters and \a id.
 *  \a deviceClassId must refer to an existing \{DeviceClass} and \a params must match the parameter description in the \l{DeviceClass}.
 *  Optionally you can supply an id yourself if you must keep track of the added device. If you don't supply it, a new one will
 *  be generated. Only devices with \l{DeviceClass}{CreateMethodUser} can be created using this method.
 *  Returns \l{DeviceError} to inform about the result. */
DeviceManager::DeviceError DeviceManager::addConfiguredDevice(const DeviceClassId &deviceClassId, const ParamList &params, const DeviceId id)
{
    DeviceClass deviceClass = findDeviceClass(deviceClassId);
    if (!deviceClass.isValid()) {
        return DeviceErrorDeviceClassNotFound;
    }
    if (deviceClass.createMethods().testFlag(DeviceClass::CreateMethodUser)) {
        return addConfiguredDeviceInternal(deviceClassId, params, id);
    }
    return DeviceErrorCreationMethodNotSupported;
}

/*! Add a new configured device for the given \l{DeviceClass} the given DeviceDescriptorId and \a deviceId. Only devices with \l{DeviceClass}{CreateMethodDiscovery}
 *  can be created using this method. The \a deviceClassId must refer to an existing \l{DeviceClass} and the \a deviceDescriptorId must refer to an existing DeviceDescriptorId
 *  from the discovery.
 *  Returns \l{DeviceError} to inform about the result. */
DeviceManager::DeviceError DeviceManager::addConfiguredDevice(const DeviceClassId &deviceClassId, const DeviceDescriptorId &deviceDescriptorId, const DeviceId &deviceId)
{
    DeviceClass deviceClass = findDeviceClass(deviceClassId);
    if (!deviceClass.isValid()) {
        return DeviceErrorDeviceClassNotFound;
    }
    if (!deviceClass.createMethods().testFlag(DeviceClass::CreateMethodDiscovery)) {
        return DeviceErrorCreationMethodNotSupported;
    }

    DeviceDescriptor descriptor = m_discoveredDevices.take(deviceDescriptorId);
    if (!descriptor.isValid()) {
        return DeviceErrorDeviceDescriptorNotFound;
    }

    return addConfiguredDeviceInternal(deviceClassId, descriptor.params(), deviceId);
}

/*! Initiates a pairing with a \l{DeviceClass}{Device} with the given \a pairingTransactionId, \a deviceClassId and \a params.
 *  Returns \l{DeviceManager::DeviceError}{DeviceError} to inform about the result. */
DeviceManager::DeviceError DeviceManager::pairDevice(const PairingTransactionId &pairingTransactionId, const DeviceClassId &deviceClassId, const ParamList &params)
{
    DeviceClass deviceClass = findDeviceClass(deviceClassId);
    if (deviceClass.id().isNull()) {
        qWarning() << "cannot find a device class with id" << deviceClassId;
        return DeviceErrorDeviceClassNotFound;
    }

    Q_UNUSED(pairingTransactionId)
    Q_UNUSED(params)
    switch (deviceClass.setupMethod()) {
    case DeviceClass::SetupMethodJustAdd:
        qWarning() << "Cannot setup this device this way. No need to pair this device.";
        return DeviceErrorSetupMethodNotSupported;
    case DeviceClass::SetupMethodDisplayPin:
        qWarning() << "SetupMethodDisplayPin not implemented yet for this CreateMethod";
        return DeviceErrorSetupFailed;
    case DeviceClass::SetupMethodEnterPin:
        qWarning() << "SetupMethodEnterPin not implemented yet for this CreateMethod";
        return DeviceErrorSetupFailed;
    case DeviceClass::SetupMethodPushButton:
        qWarning() << "SetupMethodPushButton not implemented yet for this CreateMethod";
        return DeviceErrorSetupFailed;
    }

    return DeviceErrorNoError;
}

/*! Initiates a pairing with a \l{DeviceClass}{Device} with the given \a pairingTransactionId, \a deviceClassId and \a deviceDescriptorId.
 *  Returns \l{DeviceManager::DeviceError}{DeviceError} to inform about the result. */
DeviceManager::DeviceError DeviceManager::pairDevice(const PairingTransactionId &pairingTransactionId, const DeviceClassId &deviceClassId, const DeviceDescriptorId &deviceDescriptorId)
{
    DeviceClass deviceClass = findDeviceClass(deviceClassId);
    if (deviceClass.id().isNull()) {
        qWarning() << "cannot find a device class with id" << deviceClassId;
        return DeviceErrorDeviceClassNotFound;
    }

    if (deviceClass.setupMethod() == DeviceClass::SetupMethodJustAdd) {
        qWarning() << "Cannot setup this device this way. No need to pair this device.";
        return DeviceErrorCreationMethodNotSupported;
    }

    if (!m_discoveredDevices.contains(deviceDescriptorId)) {
        qWarning() << "Cannot find a DeviceDescriptor with ID" << deviceClassId.toString();
        return DeviceErrorDeviceDescriptorNotFound;
    }

    m_pairingsDiscovery.insert(pairingTransactionId, qMakePair<DeviceClassId, DeviceDescriptorId>(deviceClassId, deviceDescriptorId));

    if (deviceClass.setupMethod() == DeviceClass::SetupMethodDisplayPin) {
        // TODO: fetch PIN from device plugin
        qWarning() << "SetupMethodDisplayPin not implemented yet";
        return DeviceErrorSetupFailed;
    }

    return DeviceErrorNoError;
}

/*! Confirms the pairing of a \l{Device} with the given \a pairingTransactionId and \a secret.
 *  Returns \l{DeviceManager::DeviceError}{DeviceError} to inform about the result. */
DeviceManager::DeviceError DeviceManager::confirmPairing(const PairingTransactionId &pairingTransactionId, const QString &secret)
{
    Q_UNUSED(secret)
    if (m_pairingsJustAdd.contains(pairingTransactionId)) {
        qWarning() << "this SetupMethod is not implemented yet";
        m_pairingsJustAdd.remove(pairingTransactionId);
        return DeviceErrorSetupFailed;
    }

    if (m_pairingsDiscovery.contains(pairingTransactionId)) {
        DeviceDescriptorId deviceDescriptorId = m_pairingsDiscovery.value(pairingTransactionId).second;
        DeviceClassId deviceClassId = m_pairingsDiscovery.value(pairingTransactionId).first;

        DeviceDescriptor deviceDescriptor = m_discoveredDevices.value(deviceDescriptorId);

        DevicePlugin *plugin = m_devicePlugins.value(m_supportedDevices.value(deviceClassId).pluginId());

        if (!plugin) {
            qWarning() << "Can't find a plugin for this device class";
            return DeviceErrorPluginNotFound;
        }

        DeviceSetupStatus status = plugin->confirmPairing(pairingTransactionId, deviceClassId, deviceDescriptor.params());
        switch (status) {
        case DeviceSetupStatusSuccess:
            m_pairingsDiscovery.remove(pairingTransactionId);
            return DeviceErrorNoError;
        case DeviceSetupStatusFailure:
            m_pairingsDiscovery.remove(pairingTransactionId);
            return DeviceErrorSetupFailed;
        case DeviceSetupStatusAsync:
            return DeviceErrorAsync;
        }
    }

    return DeviceErrorPairingTransactionIdNotFound;
}

/*! This method will only be used from the DeviceManager in order to add a \l{Device} with the given \a deviceClassId, \a params and \ id.
 *  Returns \l{DeviceError} to inform about the result. */
DeviceManager::DeviceError DeviceManager::addConfiguredDeviceInternal(const DeviceClassId &deviceClassId, const ParamList &params, const DeviceId id)
{
    ParamList effectiveParams = params;
    DeviceClass deviceClass = findDeviceClass(deviceClassId);
    if (deviceClass.id().isNull()) {
        return DeviceErrorDeviceClassNotFound;
    }

    if (deviceClass.setupMethod() != DeviceClass::SetupMethodJustAdd) {
        return DeviceErrorCreationMethodNotSupported;
    }

    DeviceError result = verifyParams(deviceClass.paramTypes(), effectiveParams);
    if (result != DeviceErrorNoError) {
        return result;
    }

    foreach(Device *device, m_configuredDevices) {
        if (device->id() == id) {
            return DeviceErrorDuplicateUuid;
        }
    }

    DevicePlugin *plugin = m_devicePlugins.value(deviceClass.pluginId());
    if (!plugin) {
        return DeviceErrorPluginNotFound;
    }

    Device *device = new Device(plugin->pluginId(), id, deviceClassId, this);
    device->setName(deviceClass.name());
    device->setParams(effectiveParams);

    DeviceSetupStatus status = setupDevice(device);
    switch (status) {
    case DeviceSetupStatusFailure:
        qWarning() << "Device setup failed. Not adding device to system.";
        delete device;
        return DeviceErrorSetupFailed;
    case DeviceSetupStatusAsync:
        return DeviceErrorAsync;
    case DeviceSetupStatusSuccess:
        qDebug() << "Device setup complete.";
        break;
    }

    m_configuredDevices.append(device);
    storeConfiguredDevices();

    return DeviceErrorNoError;
}

/*! Removes a \l{Device} with the given \a deviceId from the list of configured \l{Device}{Devices}.
 *  This method also deletes all saved settings of the \l{Device}.
 *  Returns \l{DeviceError} to inform about the result. */
DeviceManager::DeviceError DeviceManager::removeConfiguredDevice(const DeviceId &deviceId)
{
    Device *device = findConfiguredDevice(deviceId);
    if (!device) {
        return DeviceErrorDeviceNotFound;
    }

    m_configuredDevices.removeAll(device);
    m_devicePlugins.value(device->pluginId())->deviceRemoved(device);

    m_pluginTimerUsers.removeAll(device);
    if (m_pluginTimerUsers.isEmpty()) {
        m_pluginTimer.stop();
    }

    device->deleteLater();

    QSettings settings(m_settingsFile);
    settings.beginGroup("DeviceConfig");
    settings.beginGroup(deviceId.toString());
    settings.remove("");
    settings.endGroup();

    return DeviceErrorNoError;
}

/*! Returns the \l{Device} with the given \a id. Null if the id couldn't be found. */
Device *DeviceManager::findConfiguredDevice(const DeviceId &id) const
{
    foreach (Device *device, m_configuredDevices) {
        if (device->id() == id) {
            return device;
        }
    }
    return 0;
}

/*! Returns all configured \{Device}{Devices} in the system. */
QList<Device *> DeviceManager::configuredDevices() const
{
    return m_configuredDevices;
}

/*! Returns all \l{Device}{Devices} matching the \l{DeviceClass} referred by \a deviceClassId. */
QList<Device *> DeviceManager::findConfiguredDevices(const DeviceClassId &deviceClassId) const
{
    QList<Device*> ret;
    foreach (Device *device, m_configuredDevices) {
        if (device->deviceClassId() == deviceClassId) {
            ret << device;
        }
    }
    return ret;
}

/*! For conveninece, this returns the \l{DeviceClass} with the id given by \a deviceClassId.
 *  Note: The returned \l{DeviceClass} may be invalid. */
DeviceClass DeviceManager::findDeviceClass(const DeviceClassId &deviceClassId) const
{
    foreach (const DeviceClass &deviceClass, m_supportedDevices) {
        if (deviceClass.id() == deviceClassId) {
            return deviceClass;
        }
    }
    return DeviceClass();
}

/*! Execute the given \l{Action}.
 *  This will find the \l{Device} \a action refers to the \l{Action}{deviceId()} and
 *  its \l{DevicePlugin}. Then will dispatch the execution to the \l{DevicePlugin}.*/
DeviceManager::DeviceError DeviceManager::executeAction(const Action &action)
{
    Action finalAction = action;
    foreach (Device *device, m_configuredDevices) {
        if (action.deviceId() == device->id()) {
            // found device

            // Make sure this device has an action type with this id
            DeviceClass deviceClass = findDeviceClass(device->deviceClassId());
            bool found = false;
            foreach (const ActionType &actionType, deviceClass.actionTypes()) {
                if (actionType.id() == action.actionTypeId()) {
                    ParamList finalParams = action.params();
                    DeviceError paramCheck = verifyParams(actionType.paramTypes(), finalParams);
                    if (paramCheck != DeviceErrorNoError) {
                        return paramCheck;
                    }
                    finalAction.setParams(finalParams);

                    found = true;
                    continue;
                }
            }
            if (!found) {
                return DeviceErrorActionTypeNotFound;
            }

            return m_devicePlugins.value(device->pluginId())->executeAction(device, finalAction);
        }
    }
    return DeviceErrorDeviceNotFound;
}

void DeviceManager::loadPlugins()
{
    QStringList searchDirs;
    searchDirs << QCoreApplication::applicationDirPath() + "/../lib/guh/plugins";
    searchDirs << QCoreApplication::applicationDirPath() + "/../plugins/";
    searchDirs << QCoreApplication::applicationDirPath() + "/../plugins/deviceplugins";
    searchDirs << QCoreApplication::applicationDirPath() + "/../../../plugins/deviceplugins";

    foreach (const QString &path, searchDirs) {
        QDir dir(path);
        qDebug() << "Loading plugins from:" << dir.absolutePath();
        foreach (const QString &entry, dir.entryList()) {
            QFileInfo fi;
            if (entry.startsWith("libguh_deviceplugin") && entry.endsWith(".so")) {
                fi.setFile(path + "/" + entry);
            } else {
                fi.setFile(path + "/" + entry + "/libguh_deviceplugin" + entry + ".so");
            }
            if (!fi.exists()) {
                continue;
            }
            QPluginLoader loader(fi.absoluteFilePath());

            DevicePlugin *pluginIface = qobject_cast<DevicePlugin*>(loader.instance());
            if (verifyPluginMetadata(loader.metaData().value("MetaData").toObject()) && pluginIface) {
                pluginIface->initPlugin(loader.metaData().value("MetaData").toObject(), this);
                qDebug() << "*** Loaded plugin" << pluginIface->pluginName();
                foreach (const Vendor &vendor, pluginIface->supportedVendors()) {
                    qDebug() << "* Loaded vendor:" << vendor.name();
                    if (m_supportedVendors.contains(vendor.id())) {
                        qWarning() << "! Duplicate vendor. Ignoring vendor" << vendor.name();
                        continue;
                    }
                    m_supportedVendors.insert(vendor.id(), vendor);
                }

                foreach (const DeviceClass &deviceClass, pluginIface->supportedDevices()) {
                    if (!m_supportedVendors.contains(deviceClass.vendorId())) {
                        qWarning() << "! Vendor not found. Ignoring device. VendorId:" << deviceClass.vendorId() << "DeviceClass:" << deviceClass.name() << deviceClass.id();
                        continue;
                    }
                    m_vendorDeviceMap[deviceClass.vendorId()].append(deviceClass.id());
                    m_supportedDevices.insert(deviceClass.id(), deviceClass);
                    qDebug() << "* Loaded device class:" << deviceClass.name();
                }
                QSettings settings(m_settingsFile);
                settings.beginGroup("PluginConfig");
                ParamList params;
                if (settings.childGroups().contains(pluginIface->pluginId().toString())) {
                    settings.beginGroup(pluginIface->pluginId().toString());
                    foreach (const QString &paramName, settings.allKeys()) {
                        Param param(paramName, settings.value(paramName));
                        params.append(param);
                    }
                    settings.endGroup();
                } else if (pluginIface->configurationDescription().count() > 0){
                    // plugin requires config but none stored. Init with defaults
                    foreach (const ParamType &paramType, pluginIface->configurationDescription()) {
                        Param param(paramType.name(), paramType.defaultValue());
                        params.append(param);
                    }
                }
                settings.endGroup();
                QPair<DeviceError, QString> status = pluginIface->setConfiguration(params);
                if (status.first != DeviceErrorNoError) {
                    qWarning() << "Error setting params to plugin. Broken configuration?" << status.second;
                }
            }
            settings.endGroup();
            DeviceError status = pluginIface->setConfiguration(params);
            if (status != DeviceErrorNoError) {
                qWarning() << "Error setting params to plugin. Broken configuration?";
            }

                m_devicePlugins.insert(pluginIface->pluginId(), pluginIface);
                connect(pluginIface, &DevicePlugin::emitEvent, this, &DeviceManager::eventTriggered);
                connect(pluginIface, &DevicePlugin::devicesDiscovered, this, &DeviceManager::slotDevicesDiscovered);
                connect(pluginIface, &DevicePlugin::deviceSetupFinished, this, &DeviceManager::slotDeviceSetupFinished);
                connect(pluginIface, &DevicePlugin::actionExecutionFinished, this, &DeviceManager::actionExecutionFinished);
                connect(pluginIface, &DevicePlugin::pairingFinished, this, &DeviceManager::slotPairingFinished);
                connect(pluginIface, &DevicePlugin::autoDevicesAppeared, this, &DeviceManager::autoDevicesAppeared);
            }
        }
    }
}

void DeviceManager::loadConfiguredDevices()
{
    QSettings settings(m_settingsFile);
    settings.beginGroup("DeviceConfig");
    qDebug() << "loading devices from" << settings.fileName();
    foreach (const QString &idString, settings.childGroups()) {
        settings.beginGroup(idString);
        Device *device = new Device(PluginId(settings.value("pluginid").toString()), DeviceId(idString), DeviceClassId(settings.value("deviceClassId").toString()), this);
        device->setName(settings.value("devicename").toString());

        ParamList params;
        settings.beginGroup("Params");
        foreach (QString paramNameString, settings.allKeys()) {
            Param param(paramNameString);
            param.setValue(settings.value(paramNameString));
            params.append(param);
        }
        device->setParams(params);
        settings.endGroup();
        settings.endGroup();

        // We always add the device to the list in this case. If its in the storedDevices
        // it means that it was working at some point so lets still add it as there might
        // be rules associated with this device. Device::setupCompleted() will be false.
        setupDevice(device);

        m_configuredDevices.append(device);
    }
    settings.endGroup();
}

void DeviceManager::storeConfiguredDevices()
{
    QSettings settings(m_settingsFile);
    settings.beginGroup("DeviceConfig");
    foreach (Device *device, m_configuredDevices) {
        settings.beginGroup(device->id().toString());
        settings.setValue("devicename", device->name());
        settings.setValue("deviceClassId", device->deviceClassId().toString());
        settings.setValue("pluginid", device->pluginId().toString());
        settings.beginGroup("Params");
        foreach (const Param &param, device->params()) {
            settings.setValue(param.name(), param.value());
        }
        settings.endGroup();
        settings.endGroup();
    }
    settings.endGroup();
}

void DeviceManager::startMonitoringAutoDevices()
{
    foreach (DevicePlugin *plugin, m_devicePlugins) {
        plugin->startMonitoringAutoDevices();
    }
}

void DeviceManager::slotDevicesDiscovered(const DeviceClassId &deviceClassId, const QList<DeviceDescriptor> deviceDescriptors)
{
    DevicePlugin *plugin = static_cast<DevicePlugin*>(sender());
    m_discoveringPlugins.removeOne(plugin);

    foreach (const DeviceDescriptor &descriptor, deviceDescriptors) {
        m_discoveredDevices.insert(descriptor.id(), descriptor);
    }
    emit devicesDiscovered(deviceClassId, deviceDescriptors);
}

void DeviceManager::slotDeviceSetupFinished(Device *device, DeviceManager::DeviceSetupStatus status)
{
    Q_ASSERT_X(device, "DeviceManager", "Device must be a valid pointer.");
    if (!device) {
        qWarning() << "Received deviceSetupFinished for an invalid device... ignoring...";
        return;
    }

    if (device->setupComplete()) {
        qWarning() << "Received a deviceSetupFinished event, but this Device has been set up before... ignoring...";
        return;
    }

    Q_ASSERT_X(status != DeviceSetupStatusAsync, "DeviceManager", "Bad plugin implementation. You should not emit deviceSetupFinished with status DeviceSetupStatusAsync.");
    if (status == DeviceSetupStatusAsync) {
        qWarning() << "Bad plugin implementation. Received a deviceSetupFinished event with status Async... ignoring...";
        return;
    }

    if (status == DeviceSetupStatusFailure) {
        if (m_configuredDevices.contains(device)) {
            qWarning() << QString("Error in device setup. Device %1 (%2) will not be functional.").arg(device->name()).arg(device->id().toString());
            emit deviceSetupFinished(device, DeviceError::DeviceErrorSetupFailed);
            return;
        } else {
            qWarning() << QString("Error in device setup. Device %1 (%2) will not be added to the configured devices.").arg(device->name()).arg(device->id().toString());
            emit deviceSetupFinished(device, DeviceError::DeviceErrorSetupFailed);
            return;
        }
    }

    // A device might be in here already if loaded from storedDevices. If it's not in the configuredDevices,
    // lets add it now.
    if (!m_configuredDevices.contains(device)) {
        m_configuredDevices.append(device);
        storeConfiguredDevices();
    }

    DevicePlugin *plugin = m_devicePlugins.value(device->pluginId());
    if (plugin->requiredHardware().testFlag(HardwareResourceTimer)) {
        if (!m_pluginTimer.isActive()) {
            m_pluginTimer.start();
            // Additionally fire off one event to initialize stuff
            QTimer::singleShot(0, this, SLOT(timerEvent()));
        }
        m_pluginTimerUsers.append(device);
    }

    connect(device, SIGNAL(stateValueChanged(QUuid,QVariant)), this, SLOT(slotDeviceStateValueChanged(QUuid,QVariant)));

    device->setupCompleted();
    emit deviceSetupFinished(device, DeviceManager::DeviceErrorNoError);
}

void DeviceManager::slotPairingFinished(const PairingTransactionId &pairingTransactionId, DeviceManager::DeviceSetupStatus status)
{
    if (!m_pairingsJustAdd.contains(pairingTransactionId) && !m_pairingsDiscovery.contains(pairingTransactionId)) {
        DevicePlugin *plugin = dynamic_cast<DevicePlugin*>(sender());
        if (plugin) {
            qWarning() << "Received a pairing finished without waiting for it from plugin:" << plugin->metaObject()->className();
        } else {
            qWarning() << "Received a pairing finished without waiting for it.";
        }
        return;
    }

    DeviceClassId deviceClassId;
    ParamList params;

    // Do this before checking status to make sure we clean up our hashes properly
    if (m_pairingsJustAdd.contains(pairingTransactionId)) {
        QPair<DeviceClassId, ParamList> pair = m_pairingsJustAdd.take(pairingTransactionId);
        deviceClassId = pair.first;
        params = pair.second;
    }

    if (m_pairingsDiscovery.contains(pairingTransactionId)) {
        QPair<DeviceClassId, DeviceDescriptorId> pair = m_pairingsDiscovery.take(pairingTransactionId);

        DeviceDescriptorId deviceDescriptorId = pair.second;
        DeviceDescriptor descriptor = m_discoveredDevices.take(deviceDescriptorId);

        deviceClassId = pair.first;
        params = descriptor.params();
    }

    if (status != DeviceSetupStatusSuccess) {
        emit pairingFinished(pairingTransactionId, DeviceErrorSetupFailed);
        return;
    }

    DeviceClass deviceClass = findDeviceClass(deviceClassId);
    DevicePlugin *plugin = m_devicePlugins.value(deviceClass.pluginId());
    if (!plugin) {
        qWarning() << "Cannot find a plugin for this device class!";
        emit pairingFinished(pairingTransactionId, DeviceErrorPluginNotFound, deviceClass.pluginId().toString());
        return;
    }

    // Ok... pairing went fine... Let consumers know about it and inform them about the ongoing setup with a deviceId.
    DeviceId id = DeviceId::createDeviceId();
    emit pairingFinished(pairingTransactionId, DeviceErrorNoError, id);

    QList<DeviceId> newDevices;
    Device *device = new Device(plugin->pluginId(), id, deviceClassId, this);
    device->setName(deviceClass.name());
    device->setParams(params);

    DeviceSetupStatus setupStatus = setupDevice(device);
    switch (setupStatus) {
    case DeviceSetupStatusFailure:
        qWarning() << "Device setup failed. Not adding device to system.";
        emit deviceSetupFinished(device, DeviceError::DeviceErrorSetupFailed);
        delete device;
        break;
    case DeviceSetupStatusAsync:
        return;
    case DeviceSetupStatusSuccess:
        qDebug() << "Device setup complete.";
        newDevices.append(id);
        break;
    }

    m_configuredDevices.append(device);
    storeConfiguredDevices();

    emit deviceSetupFinished(device, DeviceError::DeviceErrorNoError);
}

void DeviceManager::autoDevicesAppeared(const DeviceClassId &deviceClassId, const QList<DeviceDescriptor> &deviceDescriptors)
{
    DeviceClass deviceClass = findDeviceClass(deviceClassId);
    if (!deviceClass.isValid()) {
        return;
    }
    DevicePlugin *plugin = m_devicePlugins.value(deviceClass.pluginId());
    if (!plugin) {
        return;
    }

    foreach (const DeviceDescriptor &deviceDescriptor, deviceDescriptors) {
        Device *device = new Device(plugin->pluginId(), deviceClassId, this);
        device->setName(deviceClass.name());
        device->setParams(deviceDescriptor.params());

        DeviceSetupStatus setupStatus = setupDevice(device);
        switch (setupStatus) {
        case DeviceSetupStatusFailure:
            qWarning() << "Device setup failed. Not adding device to system.";
            emit deviceSetupFinished(device, DeviceError::DeviceErrorSetupFailed);
            delete device;
            break;
        case DeviceSetupStatusAsync:
            break;
        case DeviceSetupStatusSuccess:
            qDebug() << "Device setup complete.";
            emit deviceSetupFinished(device, DeviceError::DeviceErrorNoError);
            m_configuredDevices.append(device);
            storeConfiguredDevices();
            break;
        }
    }
}

void DeviceManager::slotDeviceStateValueChanged(const QUuid &stateTypeId, const QVariant &value)
{
    Device *device = qobject_cast<Device*>(sender());
    if (!device) {
        return;
    }
    emit deviceStateChanged(device, stateTypeId, value);

    Param valueParam("value", value);
    Event event(EventTypeId(stateTypeId.toString()), device->id(), ParamList() << valueParam, true);
    emit eventTriggered(event);
}

void DeviceManager::radio433SignalReceived(QList<int> rawData)
{
    QList<DevicePlugin*> targetPlugins;

    foreach (Device *device, m_configuredDevices) {
        DeviceClass deviceClass = m_supportedDevices.value(device->deviceClassId());
        DevicePlugin *plugin = m_devicePlugins.value(deviceClass.pluginId());
        if (plugin->requiredHardware().testFlag(HardwareResourceRadio433) && !targetPlugins.contains(plugin)) {
            targetPlugins.append(plugin);
        }
    }
    foreach (DevicePlugin *plugin, m_discoveringPlugins) {
        if (plugin->requiredHardware().testFlag(HardwareResourceRadio433) && !targetPlugins.contains(plugin)) {
            targetPlugins.append(plugin);
        }
    }

    foreach (DevicePlugin *plugin, targetPlugins) {
        plugin->radioData(rawData);
    }
}

void DeviceManager::upnpDiscoveryFinished(const QList<UpnpDeviceDescriptor> &deviceDescriptorList, const PluginId &pluginId)
{
    foreach (DevicePlugin *devicePlugin, m_devicePlugins) {
        if (devicePlugin->requiredHardware().testFlag(HardwareResourceUpnpDisovery) && devicePlugin->pluginId() == pluginId) {
            devicePlugin->upnpDiscoveryFinished(deviceDescriptorList);
        }
    }
}

void DeviceManager::upnpNotifyReceived(const QByteArray &notifyData)
{
    foreach (DevicePlugin *devicePlugin, m_devicePlugins) {
        if (devicePlugin->requiredHardware().testFlag(HardwareResourceUpnpDisovery)) {
            devicePlugin->upnpNotifyReceived(notifyData);
        }
    }
}

void DeviceManager::timerEvent()
{
    foreach (Device *device, m_configuredDevices) {
        DeviceClass deviceClass = m_supportedDevices.value(device->deviceClassId());
        DevicePlugin *plugin = m_devicePlugins.value(deviceClass.pluginId());
        if (plugin && plugin->requiredHardware().testFlag(HardwareResourceTimer)) {
            plugin->guhTimer();
        }
    }
}

bool DeviceManager::verifyPluginMetadata(const QJsonObject &data)
{
    QStringList requiredFields;
    requiredFields << "name" << "id" << "vendors";

    foreach (const QString &field, requiredFields) {
        if (!data.contains("name")) {
            qWarning() << "Error loading plugin. Incomplete metadata. Missing field:" << field;
            return false;
        }
    }
    return true;
}

DeviceManager::DeviceSetupStatus DeviceManager::setupDevice(Device *device)
{
    DeviceClass deviceClass = findDeviceClass(device->deviceClassId());
    DevicePlugin *plugin = m_devicePlugins.value(deviceClass.pluginId());

    if (!plugin) {
        qWarning() << "Can't find a plugin for this device" << device->id();
        return DeviceSetupStatusFailure;
    }

    QList<State> states;
    foreach (const StateType &stateType, deviceClass.stateTypes()) {
        State state(stateType.id(), device->id());
        state.setValue(stateType.defaultValue());
        states.append(state);
    }
    device->setStates(states);

    DeviceSetupStatus status = plugin->setupDevice(device);
    if (status != DeviceSetupStatusSuccess) {
        return status;
    }

    if (plugin->requiredHardware().testFlag(HardwareResourceTimer)) {
        if (!m_pluginTimer.isActive()) {
            m_pluginTimer.start();
            // Additionally fire off one event to initialize stuff
            QTimer::singleShot(0, this, SLOT(timerEvent()));
        }
        m_pluginTimerUsers.append(device);
    }

    connect(device, SIGNAL(stateValueChanged(QUuid,QVariant)), this, SLOT(slotDeviceStateValueChanged(QUuid,QVariant)));

    device->setupCompleted();
    return status;
}

DeviceManager::DeviceError DeviceManager::verifyParams(const QList<ParamType> paramTypes, ParamList &params, bool requireAll)
{
    foreach (const Param &param, params) {
        DeviceManager::DeviceError result = verifyParam(paramTypes, param);
        if (result != DeviceErrorNoError) {
            return result;
        }
    }
    if (!requireAll) {
        return DeviceErrorNoError;
    }
    foreach (const ParamType &paramType, paramTypes) {
        bool found = false;
        foreach (const Param &param, params) {
            if (paramType.name() == param.name()) {
                found = true;
            }
        }

        // This paramType has a default value... lets fill in that one.
        if (!paramType.defaultValue().isNull()) {
            found = true;
            params.append(Param(paramType.name(), paramType.defaultValue()));
        }

        if (!found) {
            qWarning() << "Missing parameter:" << paramType.name();
            return DeviceErrorMissingParameter;
        }
    }
    return DeviceErrorNoError;
}

DeviceManager::DeviceError DeviceManager::verifyParam(const QList<ParamType> paramTypes, const Param &param)
{
    foreach (const ParamType &paramType, paramTypes) {
        if (paramType.name() == param.name()) {
            return verifyParam(paramType, param);
        }
    }
    qWarning() << "Invalid parameter" << param.name() << "in parameter list";
    return DeviceErrorInvalidParameter;
}

DeviceManager::DeviceError DeviceManager::verifyParam(const ParamType &paramType, const Param &param)
{
    if (paramType.name() == param.name()) {
        if (!param.value().canConvert(paramType.type())) {
            qWarning() << "Wrong parameter type for param" << param.name() << " Got:" << param.value() << " Expected:" << QVariant::typeToName(paramType.type());
            return DeviceErrorInvalidParameter;
        }

        if (paramType.maxValue().isValid() && param.value() > paramType.maxValue()) {
            qWarning() << "Value out of range for param" << param.name() << " Got:" << param.value() << " Max:" << paramType.maxValue();
            return DeviceErrorInvalidParameter;
        }
        if (paramType.minValue().isValid() && param.value() < paramType.minValue()) {
            qWarning() << "Value out of range for param" << param.name() << " Got:" << param.value() << " Min:" << paramType.minValue();
            return DeviceErrorInvalidParameter;
        }
        if (!paramType.allowedValues().isEmpty() && !paramType.allowedValues().contains(param.value())) {
            QStringList allowedValues;
            foreach (const QVariant &value, paramType.allowedValues()) {
                allowedValues.append(value.toString());
            }

            qWarning() << "Value not in allowed values for param" << param.name() << " Got:" << param.value() << " Allowed:" << allowedValues.join(",");
            return DeviceErrorInvalidParameter;
        }
        return DeviceErrorNoError;
    }
    qWarning() << "Parameter name" << param.name() << "does not match with ParamType name" << paramType.name();
    return DeviceErrorInvalidParameter;
}
